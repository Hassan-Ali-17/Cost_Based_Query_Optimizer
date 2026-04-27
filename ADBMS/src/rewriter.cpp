#include "rewriter.hpp"

#include <algorithm>
#include <cctype>
#include <set>

namespace {
bool hasDot(const std::string& s) {
    return s.find('.') != std::string::npos;
}

std::string tableOf(const std::string& expr) {
    const size_t dot = expr.find('.');
    if (dot == std::string::npos) {
        return "";
    }
    return expr.substr(0, dot);
}

void collectTables(const std::shared_ptr<PlanNode>& node, std::set<std::string>& out) {
    if (!node) {
        return;
    }
    if (node->type == PlanType::Scan) {
        out.insert(node->label);
    }
    collectTables(node->left, out);
    collectTables(node->right, out);
}

bool isNumericLiteral(const std::string& s) {
    if (s.empty()) {
        return false;
    }
    size_t i = 0;
    if (s[0] == '+' || s[0] == '-') {
        i = 1;
    }
    bool seenDigit = false;
    for (; i < s.size(); ++i) {
        const char ch = s[i];
        if (std::isdigit(static_cast<unsigned char>(ch)) != 0) {
            seenDigit = true;
            continue;
        }
        if (ch == '.') {
            continue;
        }
        return false;
    }
    return seenDigit;
}

bool evalConstantPredicate(const Predicate& p, bool& result) {
    if (hasDot(p.left) || hasDot(p.right) || p.isJoin) {
        return false;
    }

    if (isNumericLiteral(p.left) && isNumericLiteral(p.right)) {
        const double l = std::stod(p.left);
        const double r = std::stod(p.right);
        switch (p.op) {
            case PredOp::EQ: result = (l == r); return true;
            case PredOp::NE: result = (l != r); return true;
            case PredOp::LT: result = (l < r); return true;
            case PredOp::LE: result = (l <= r); return true;
            case PredOp::GT: result = (l > r); return true;
            case PredOp::GE: result = (l >= r); return true;
        }
    }

    switch (p.op) {
        case PredOp::EQ: result = (p.left == p.right); return true;
        case PredOp::NE: result = (p.left != p.right); return true;
        case PredOp::LT: result = (p.left < p.right); return true;
        case PredOp::LE: result = (p.left <= p.right); return true;
        case PredOp::GT: result = (p.left > p.right); return true;
        case PredOp::GE: result = (p.left >= p.right); return true;
    }

    return false;
}

std::set<std::string> predicateTables(const Predicate& p) {
    std::set<std::string> out;
    const std::string l = tableOf(p.left);
    const std::string r = tableOf(p.right);
    if (!l.empty()) {
        out.insert(l);
    }
    if (!r.empty()) {
        out.insert(r);
    }
    return out;
}

bool allIn(const std::set<std::string>& need, const std::set<std::string>& have) {
    for (const auto& t : need) {
        if (have.find(t) == have.end()) {
            return false;
        }
    }
    return true;
}

std::shared_ptr<PlanNode> rewriteNode(const std::shared_ptr<PlanNode>& node) {
    if (!node) {
        return nullptr;
    }

    node->left = rewriteNode(node->left);
    node->right = rewriteNode(node->right);

    if (node->type == PlanType::Project && node->left && node->left->type == PlanType::Project) {
        node->left = node->left->left;
        return rewriteNode(node);
    }

    if (node->type != PlanType::Filter || !node->left) {
        return node;
    }

    bool constResult = false;
    if (evalConstantPredicate(node->pred, constResult)) {
        if (constResult) {
            return node->left;
        }
        return makeLimit(0, node->left);
    }

    if (node->left->type != PlanType::HashJoin && node->left->type != PlanType::CrossProduct) {
        return node;
    }

    auto joinNode = node->left;
    std::set<std::string> leftTables;
    std::set<std::string> rightTables;
    collectTables(joinNode->left, leftTables);
    collectTables(joinNode->right, rightTables);

    const std::set<std::string> usedTables = predicateTables(node->pred);
    if (usedTables.empty()) {
        return node;
    }

    if (allIn(usedTables, leftTables)) {
        joinNode->left = rewriteNode(makeFilter(node->pred, joinNode->left));
        return joinNode;
    }

    if (allIn(usedTables, rightTables)) {
        joinNode->right = rewriteNode(makeFilter(node->pred, joinNode->right));
        return joinNode;
    }

    return node;
}
}

std::shared_ptr<PlanNode> rewritePlan(const std::shared_ptr<PlanNode>& root) {
    std::shared_ptr<PlanNode> current = root;
    for (int i = 0; i < 4; ++i) {
        current = rewriteNode(current);
    }
    return current;
}
