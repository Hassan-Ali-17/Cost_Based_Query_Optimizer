#include "executor.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace {
double asNumber(const std::string& s, bool& ok) {
    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    ok = end != nullptr && *end == '\0';
    return v;
}

std::string valueOf(const ExecRow& row, const std::string& expr) {
    auto it = row.find(expr);
    if (it != row.end()) {
        return it->second;
    }
    return expr;
}

bool evalPred(const Predicate& p, const ExecRow& row) {
    const std::string lv = valueOf(row, p.left);
    const std::string rv = valueOf(row, p.right);

    bool lNum = false;
    bool rNum = false;
    const double l = asNumber(lv, lNum);
    const double r = asNumber(rv, rNum);

    if (lNum && rNum) {
        switch (p.op) {
            case PredOp::EQ: return l == r;
            case PredOp::NE: return l != r;
            case PredOp::LT: return l < r;
            case PredOp::LE: return l <= r;
            case PredOp::GT: return l > r;
            case PredOp::GE: return l >= r;
        }
    }

    switch (p.op) {
        case PredOp::EQ: return lv == rv;
        case PredOp::NE: return lv != rv;
        case PredOp::LT: return lv < rv;
        case PredOp::LE: return lv <= rv;
        case PredOp::GT: return lv > rv;
        case PredOp::GE: return lv >= rv;
    }
    return false;
}

std::vector<std::string> splitCsv(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    int depth = 0;
    for (char ch : s) {
        if (ch == '(') {
            depth++;
        } else if (ch == ')') {
            depth = std::max(0, depth - 1);
        }

        if (ch == ',' && depth == 0) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(ch);
        }
    }
    if (!cur.empty()) {
        out.push_back(cur);
    }

    for (auto& x : out) {
        size_t i = 0;
        while (i < x.size() && std::isspace(static_cast<unsigned char>(x[i])) != 0) {
            ++i;
        }
        size_t j = x.size();
        while (j > i && std::isspace(static_cast<unsigned char>(x[j - 1])) != 0) {
            --j;
        }
        x = x.substr(i, j - i);
    }

    return out;
}

std::string upper(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}

bool isAggregateExpr(const std::string& expr) {
    const std::string u = upper(expr);
    return u.rfind("SUM(", 0) == 0 || u.rfind("COUNT(", 0) == 0 || u.rfind("AVG(", 0) == 0 || u.rfind("MIN(", 0) == 0 || u.rfind("MAX(", 0) == 0;
}

bool findGroupKey(const std::shared_ptr<PlanNode>& n, std::string& out) {
    if (!n) {
        return false;
    }
    if (n->type == PlanType::GroupBy) {
        out = n->label;
        return true;
    }
    return findGroupKey(n->left, out) || findGroupKey(n->right, out);
}

double evalExprNumeric(const ExecRow& row, const std::string& expr) {
    const size_t mul = expr.find('*');
    if (mul != std::string::npos) {
        const std::string a = expr.substr(0, mul);
        const std::string b = expr.substr(mul + 1);
        bool okA = false;
        bool okB = false;
        const double va = asNumber(valueOf(row, a), okA);
        const double vb = asNumber(valueOf(row, b), okB);
        return (okA && okB) ? (va * vb) : 0.0;
    }

    bool ok = false;
    const double v = asNumber(valueOf(row, expr), ok);
    return ok ? v : 0.0;
}

std::vector<ExecRow> projectRows(const std::vector<ExecRow>& input, const std::string& selectRaw, const std::string& groupKey) {
    const auto items = splitCsv(selectRaw);
    const bool hasAgg = std::any_of(items.begin(), items.end(), [](const std::string& x) { return isAggregateExpr(x); });

    if (selectRaw == "*" && !hasAgg) {
        return input;
    }

    if (!hasAgg) {
        std::vector<ExecRow> out;
        out.reserve(input.size());
        for (const auto& in : input) {
            ExecRow r;
            for (const auto& it : items) {
                r[it] = valueOf(in, it);
            }
            out.push_back(std::move(r));
        }
        return out;
    }

    std::unordered_map<std::string, std::vector<const ExecRow*>> groups;
    if (groupKey.empty()) {
        groups["__all__"] = {};
        groups["__all__"].reserve(input.size());
        for (const auto& r : input) {
            groups["__all__"].push_back(&r);
        }
    } else {
        for (const auto& r : input) {
            groups[valueOf(r, groupKey)].push_back(&r);
        }
    }

    std::vector<ExecRow> out;
    out.reserve(groups.size());
    for (const auto& [gk, rows] : groups) {
        ExecRow r;
        for (const auto& expr : items) {
            const std::string u = upper(expr);
            if (u.rfind("COUNT(", 0) == 0) {
                r[expr] = std::to_string(rows.size());
                continue;
            }
            if (u.rfind("SUM(", 0) == 0) {
                const std::string inner = expr.substr(4, expr.size() - 5);
                double sum = 0.0;
                for (const auto* pr : rows) {
                    sum += evalExprNumeric(*pr, inner);
                }
                r[expr] = std::to_string(sum);
                continue;
            }
            if (u.rfind("AVG(", 0) == 0) {
                const std::string inner = expr.substr(4, expr.size() - 5);
                double sum = 0.0;
                for (const auto* pr : rows) {
                    sum += evalExprNumeric(*pr, inner);
                }
                const double avg = rows.empty() ? 0.0 : sum / static_cast<double>(rows.size());
                r[expr] = std::to_string(avg);
                continue;
            }
            if (u.rfind("MIN(", 0) == 0) {
                const std::string inner = expr.substr(4, expr.size() - 5);
                double mn = 0.0;
                bool init = false;
                for (const auto* pr : rows) {
                    const double v = evalExprNumeric(*pr, inner);
                    if (!init || v < mn) {
                        mn = v;
                        init = true;
                    }
                }
                r[expr] = std::to_string(init ? mn : 0.0);
                continue;
            }
            if (u.rfind("MAX(", 0) == 0) {
                const std::string inner = expr.substr(4, expr.size() - 5);
                double mx = 0.0;
                bool init = false;
                for (const auto* pr : rows) {
                    const double v = evalExprNumeric(*pr, inner);
                    if (!init || v > mx) {
                        mx = v;
                        init = true;
                    }
                }
                r[expr] = std::to_string(init ? mx : 0.0);
                continue;
            }

            if (!rows.empty()) {
                r[expr] = valueOf(*rows.front(), expr);
            } else {
                r[expr] = "";
            }
        }
        out.push_back(std::move(r));
    }

    return out;
}

std::vector<ExecRow> filterOverCross(const std::vector<ExecRow>& leftRows, const std::vector<ExecRow>& rightRows, const Predicate& pred) {
    std::vector<ExecRow> out;

    const bool eq = pred.op == PredOp::EQ;
    const bool leftIsCol = pred.left.find('.') != std::string::npos;
    const bool rightIsCol = pred.right.find('.') != std::string::npos;

    auto hasKey = [](const std::vector<ExecRow>& rows, const std::string& key) {
        if (rows.empty()) {
            return false;
        }
        return rows.front().find(key) != rows.front().end();
    };

    if (eq && leftIsCol && rightIsCol) {
        std::string leftKey;
        std::string rightKey;
        if (hasKey(leftRows, pred.left) && hasKey(rightRows, pred.right)) {
            leftKey = pred.left;
            rightKey = pred.right;
        } else if (hasKey(leftRows, pred.right) && hasKey(rightRows, pred.left)) {
            leftKey = pred.right;
            rightKey = pred.left;
        }

        if (!leftKey.empty() && !rightKey.empty()) {
            std::unordered_multimap<std::string, const ExecRow*> rightBuckets;
            rightBuckets.reserve(rightRows.size() * 2 + 1);
            for (const auto& rr : rightRows) {
                auto it = rr.find(rightKey);
                if (it != rr.end()) {
                    rightBuckets.emplace(it->second, &rr);
                }
            }

            for (const auto& lr : leftRows) {
                auto il = lr.find(leftKey);
                if (il == lr.end()) {
                    continue;
                }
                auto range = rightBuckets.equal_range(il->second);
                for (auto it = range.first; it != range.second; ++it) {
                    ExecRow c = lr;
                    c.insert(it->second->begin(), it->second->end());
                    out.push_back(std::move(c));
                }
            }
            return out;
        }
    }

    out.reserve(std::min(leftRows.size() * rightRows.size(), static_cast<size_t>(500000)));

    for (const auto& a : leftRows) {
        for (const auto& b : rightRows) {
            bool pass = false;
            if (eq && leftIsCol && rightIsCol) {
                const auto ilA = a.find(pred.left);
                const auto irB = b.find(pred.right);
                const auto ilB = b.find(pred.left);
                const auto irA = a.find(pred.right);
                pass = (ilA != a.end() && irB != b.end() && ilA->second == irB->second) ||
                       (ilB != b.end() && irA != a.end() && ilB->second == irA->second);
            } else {
                ExecRow c = a;
                c.insert(b.begin(), b.end());
                pass = evalPred(pred, c);
                if (pass) {
                    out.push_back(std::move(c));
                }
            }

            if (pass && !(eq && leftIsCol && rightIsCol)) {
                continue;
            }
            if (pass && (eq && leftIsCol && rightIsCol)) {
                ExecRow c = a;
                c.insert(b.begin(), b.end());
                out.push_back(std::move(c));
            }
        }
    }

    return out;
}

ExecResult executeNode(const std::shared_ptr<PlanNode>& node, const Database& db) {
    ExecResult out;
    if (!node) {
        return out;
    }

    if (node->type == PlanType::Scan) {
        auto it = db.tables.find(node->label);
        if (it == db.tables.end()) {
            return out;
        }
        const TableData& t = it->second;
        out.rows.reserve(t.rows.size());
        for (const auto& rr : t.rows) {
            ExecRow r;
            for (size_t i = 0; i < t.columns.size() && i < rr.size(); ++i) {
                r[t.name + "." + t.columns[i]] = rr[i];
            }
            out.rows.push_back(std::move(r));
        }
        out.resultRows = static_cast<long long>(out.rows.size());
        return out;
    }

    if (node->type == PlanType::Filter) {
        if (node->left && node->left->type == PlanType::CrossProduct) {
            ExecResult l = executeNode(node->left->left, db);
            ExecResult r = executeNode(node->left->right, db);
            out.rows = filterOverCross(l.rows, r.rows, node->pred);
        } else {
            ExecResult child = executeNode(node->left, db);
            out.rows.reserve(child.rows.size());
            for (const auto& r : child.rows) {
                if (evalPred(node->pred, r)) {
                    out.rows.push_back(r);
                }
            }
        }
        out.resultRows = static_cast<long long>(out.rows.size());
        return out;
    }

    if (node->type == PlanType::CrossProduct) {
        ExecResult l = executeNode(node->left, db);
        ExecResult r = executeNode(node->right, db);
        out.rows.reserve(l.rows.size() * r.rows.size());
        for (const auto& a : l.rows) {
            for (const auto& b : r.rows) {
                ExecRow c = a;
                c.insert(b.begin(), b.end());
                out.rows.push_back(std::move(c));
            }
        }
        out.resultRows = static_cast<long long>(out.rows.size());
        return out;
    }

    if (node->type == PlanType::HashJoin) {
        ExecResult l = executeNode(node->left, db);
        ExecResult r = executeNode(node->right, db);
        out.rows.reserve(std::min(l.rows.size() * r.rows.size(), static_cast<size_t>(1000000)));
        for (const auto& a : l.rows) {
            for (const auto& b : r.rows) {
                ExecRow c = a;
                c.insert(b.begin(), b.end());
                if (evalPred(node->pred, c)) {
                    out.rows.push_back(std::move(c));
                }
            }
        }
        out.resultRows = static_cast<long long>(out.rows.size());
        return out;
    }

    if (node->type == PlanType::GroupBy) {
        return executeNode(node->left, db);
    }

    if (node->type == PlanType::Project) {
        ExecResult child = executeNode(node->left, db);
        std::string groupKey;
        findGroupKey(node->left, groupKey);
        out.rows = projectRows(child.rows, node->label, groupKey);
        out.resultRows = static_cast<long long>(out.rows.size());
        return out;
    }

    if (node->type == PlanType::Limit) {
        ExecResult child = executeNode(node->left, db);
        int lim = 0;
        try {
            lim = std::stoi(node->label);
        } catch (...) {
            lim = 0;
        }
        lim = std::max(0, lim);
        if (static_cast<int>(child.rows.size()) > lim) {
            child.rows.resize(static_cast<size_t>(lim));
        }
        child.resultRows = static_cast<long long>(child.rows.size());
        return child;
    }

    return out;
}
}

ExecResult executePlan(const std::shared_ptr<PlanNode>& plan, const Database& db) {
    const auto start = std::chrono::steady_clock::now();
    ExecResult out = executeNode(plan, db);
    const auto stop = std::chrono::steady_clock::now();
    out.runtimeMs = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count()) / 1000.0;
    return out;
}
