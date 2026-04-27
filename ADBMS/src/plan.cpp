#include "plan.hpp"

#include <iostream>

namespace {
std::shared_ptr<PlanNode> node(PlanType t, const std::string& label) {
    auto n = std::make_shared<PlanNode>();
    n->type = t;
    n->label = label;
    return n;
}

std::string planTypeName(PlanType type) {
    switch (type) {
        case PlanType::Scan: return "Scan";
        case PlanType::Filter: return "Filter";
        case PlanType::Project: return "Project";
        case PlanType::HashJoin: return "HashJoin";
        case PlanType::CrossProduct: return "CrossProduct";
        case PlanType::GroupBy: return "GroupBy";
        case PlanType::Limit: return "Limit";
    }
    return "Unknown";
}
}

std::shared_ptr<PlanNode> makeScan(const std::string& table) {
    return node(PlanType::Scan, table);
}

std::shared_ptr<PlanNode> makeFilter(const Predicate& pred, std::shared_ptr<PlanNode> child) {
    auto n = node(PlanType::Filter, "filter");
    n->pred = pred;
    n->left = std::move(child);
    return n;
}

std::shared_ptr<PlanNode> makeJoin(const Predicate& pred, std::shared_ptr<PlanNode> left, std::shared_ptr<PlanNode> right) {
    auto n = node(PlanType::HashJoin, "hashjoin");
    n->pred = pred;
    n->left = std::move(left);
    n->right = std::move(right);
    return n;
}

std::shared_ptr<PlanNode> makeCross(std::shared_ptr<PlanNode> left, std::shared_ptr<PlanNode> right) {
    auto n = node(PlanType::CrossProduct, "cross");
    n->left = std::move(left);
    n->right = std::move(right);
    return n;
}

std::shared_ptr<PlanNode> makeProject(const std::string& selectList, std::shared_ptr<PlanNode> child) {
    auto n = node(PlanType::Project, selectList);
    n->left = std::move(child);
    return n;
}

std::shared_ptr<PlanNode> makeGroupBy(const std::string& col, std::shared_ptr<PlanNode> child) {
    auto n = node(PlanType::GroupBy, col);
    n->left = std::move(child);
    return n;
}

std::shared_ptr<PlanNode> makeLimit(int limit, std::shared_ptr<PlanNode> child) {
    auto n = node(PlanType::Limit, std::to_string(limit));
    n->left = std::move(child);
    return n;
}

std::shared_ptr<PlanNode> buildNaivePlan(const Query& q) {
    if (q.tables.empty()) {
        return nullptr;
    }

    std::shared_ptr<PlanNode> root = makeScan(q.tables.front());
    for (size_t i = 1; i < q.tables.size(); ++i) {
        root = makeCross(root, makeScan(q.tables[i]));
    }

    for (const auto& pred : q.predicates) {
        root = makeFilter(pred, root);
    }

    if (q.hasGroupBy) {
        root = makeGroupBy(q.groupBy, root);
    }

    root = makeProject(q.selectRaw, root);

    if (q.hasLimit) {
        root = makeLimit(q.limit, root);
    }

    return root;
}

void printPlan(const std::shared_ptr<PlanNode>& nodePtr, int depth) {
    if (!nodePtr) {
        return;
    }
    for (int i = 0; i < depth; ++i) {
        std::cout << "  ";
    }
    std::cout << planTypeName(nodePtr->type)
              << " [" << nodePtr->label << "]"
              << " card=" << nodePtr->estCardinality
              << " cost=" << nodePtr->estCost
              << "\n";

    printPlan(nodePtr->left, depth + 1);
    printPlan(nodePtr->right, depth + 1);
}
