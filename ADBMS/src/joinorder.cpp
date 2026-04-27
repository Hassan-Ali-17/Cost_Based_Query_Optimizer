#include "joinorder.hpp"

#include "cost.hpp"

#include <limits>
#include <set>
#include <string>
#include <vector>

namespace {
std::string tableOf(const std::string& expr) {
    const size_t dot = expr.find('.');
    if (dot == std::string::npos) {
        return "";
    }
    return expr.substr(0, dot);
}

struct Step {
    bool valid = false;
    double cost = std::numeric_limits<double>::infinity();
    double card = 0.0;
    int prevMask = 0;
    int tableIdx = -1;
    bool usedJoin = false;
    Predicate joinPred;
};

double baseCardinality(const Catalog& catalog, const std::string& table) {
    const auto* t = findTable(catalog, table);
    return t ? t->rowCount : 1000.0;
}

bool tableInMask(const std::vector<std::string>& tables, int mask, const std::string& table) {
    for (size_t i = 0; i < tables.size(); ++i) {
        if ((mask & (1 << static_cast<int>(i))) != 0 && tables[i] == table) {
            return true;
        }
    }
    return false;
}

bool touchesMask(const Predicate& p, const std::vector<std::string>& tables, int leftMask, int rightIdx) {
    if (!p.isJoin) {
        return false;
    }
    const std::string lt = tableOf(p.left);
    const std::string rt = tableOf(p.right);
    if (lt.empty() || rt.empty()) {
        return false;
    }

    const std::string rightTable = tables[static_cast<size_t>(rightIdx)];
    const bool leftHasLt = tableInMask(tables, leftMask, lt);
    const bool leftHasRt = tableInMask(tables, leftMask, rt);
    return (leftHasLt && rt == rightTable) || (leftHasRt && lt == rightTable);
}

Step bestTransition(
    const Query& q,
    const Catalog& catalog,
    const std::vector<std::string>& tables,
    const std::vector<Step>& dp,
    int mask,
    int rightIdx) {
    Step out;
    out.valid = false;

    const int rightBit = (1 << rightIdx);
    if ((mask & rightBit) == 0) {
        return out;
    }

    const int leftMask = mask ^ rightBit;
    if (leftMask == 0 || !dp[static_cast<size_t>(leftMask)].valid) {
        return out;
    }

    const Step& left = dp[static_cast<size_t>(leftMask)];
    const double rightCard = baseCardinality(catalog, tables[static_cast<size_t>(rightIdx)]);
    const double rightCost = rightCard;

    double bestCost = std::numeric_limits<double>::infinity();
    double bestCard = 0.0;
    bool joinUsed = false;
    Predicate chosen;

    for (const auto& p : q.predicates) {
        if (!touchesMask(p, tables, leftMask, rightIdx)) {
            continue;
        }
        const double sel = estimateSelectivity(p, catalog);
        const double outCard = left.card * rightCard * sel;
        const double outCost = left.cost + rightCost + 2.0 * left.card + rightCard + outCard;
        if (outCost < bestCost) {
            bestCost = outCost;
            bestCard = outCard;
            chosen = p;
            joinUsed = true;
        }
    }

    if (!joinUsed) {
        bestCard = left.card * rightCard;
        bestCost = left.cost + rightCost + bestCard;
    }

    out.valid = true;
    out.cost = bestCost;
    out.card = bestCard;
    out.prevMask = leftMask;
    out.tableIdx = rightIdx;
    out.usedJoin = joinUsed;
    out.joinPred = chosen;
    return out;
}

std::shared_ptr<PlanNode> buildFromDp(
    int mask,
    const std::vector<std::string>& tables,
    const std::vector<Step>& dp) {
    const Step& s = dp[static_cast<size_t>(mask)];
    if ((mask & (mask - 1)) == 0) {
        int idx = 0;
        while ((mask & (1 << idx)) == 0) {
            ++idx;
        }
        return makeScan(tables[static_cast<size_t>(idx)]);
    }

    auto left = buildFromDp(s.prevMask, tables, dp);
    auto right = makeScan(tables[static_cast<size_t>(s.tableIdx)]);
    if (s.usedJoin) {
        Predicate p = s.joinPred;
        p.isJoin = true;
        return makeJoin(p, left, right);
    }
    return makeCross(left, right);
}
}

std::shared_ptr<PlanNode> buildDpJoinPlan(const Query& q, const Catalog& catalog) {
    if (q.tables.empty()) {
        return nullptr;
    }

    if (q.tables.size() > 20) {
        return buildNaivePlan(q);
    }

    const int n = static_cast<int>(q.tables.size());
    const int fullMask = (1 << n) - 1;
    std::vector<Step> dp(static_cast<size_t>(fullMask + 1));

    for (int i = 0; i < n; ++i) {
        const int mask = (1 << i);
        dp[static_cast<size_t>(mask)].valid = true;
        dp[static_cast<size_t>(mask)].card = baseCardinality(catalog, q.tables[static_cast<size_t>(i)]);
        dp[static_cast<size_t>(mask)].cost = dp[static_cast<size_t>(mask)].card;
        dp[static_cast<size_t>(mask)].tableIdx = i;
    }

    for (int mask = 1; mask <= fullMask; ++mask) {
        if ((mask & (mask - 1)) == 0) {
            continue;
        }

        Step best;
        for (int t = 0; t < n; ++t) {
            if ((mask & (1 << t)) == 0) {
                continue;
            }
            Step cand = bestTransition(q, catalog, q.tables, dp, mask, t);
            if (!cand.valid) {
                continue;
            }
            if (!best.valid || cand.cost < best.cost) {
                best = cand;
            }
        }
        dp[static_cast<size_t>(mask)] = best;
    }

    if (!dp[static_cast<size_t>(fullMask)].valid) {
        return buildNaivePlan(q);
    }

    std::shared_ptr<PlanNode> root = buildFromDp(fullMask, q.tables, dp);

    for (const auto& pred : q.predicates) {
        if (!pred.isJoin) {
            root = makeFilter(pred, root);
        }
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
