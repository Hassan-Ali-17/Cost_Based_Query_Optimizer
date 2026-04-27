#include "cost.hpp"

#include <algorithm>
#include <cmath>

namespace {
double clamp01(double x) {
    return std::max(0.0, std::min(1.0, x));
}

double nodeCard(const std::shared_ptr<PlanNode>& n) {
    return n ? n->estCardinality : 1000.0;
}

double nodeCost(const std::shared_ptr<PlanNode>& n) {
    return n ? n->estCost : 1000.0;
}
}

double estimateSelectivity(const Predicate& pred, const Catalog& catalog) {
    if (pred.isJoin) {
        const auto* l = findColumn(catalog, pred.left);
        const auto* r = findColumn(catalog, pred.right);
        if (!l || !r || l->distinctCount <= 0 || r->distinctCount <= 0) {
            return 0.1;
        }
        return 1.0 / std::max(l->distinctCount, r->distinctCount);
    }

    const auto* l = findColumn(catalog, pred.left);
    if (!l) {
        return 0.1;
    }

    switch (pred.op) {
        case PredOp::EQ:
            return (l->distinctCount > 0) ? clamp01(1.0 / l->distinctCount) : 0.1;
        case PredOp::NE:
            return (l->distinctCount > 0) ? clamp01(1.0 - (1.0 / l->distinctCount)) : 0.9;
        case PredOp::LT:
        case PredOp::LE:
        case PredOp::GT:
        case PredOp::GE: {
            try {
                const double lit = std::stod(pred.right);
                if (l->maxValue <= l->minValue) {
                    return 0.3;
                }
                double frac = (lit - l->minValue) / (l->maxValue - l->minValue);
                frac = clamp01(frac);
                if (pred.op == PredOp::GT || pred.op == PredOp::GE) {
                    frac = 1.0 - frac;
                }
                return frac;
            } catch (...) {
                return 0.3;
            }
        }
    }
    return 0.1;
}

void estimatePlan(const std::shared_ptr<PlanNode>& node, const Catalog& catalog) {
    if (!node) {
        return;
    }

    estimatePlan(node->left, catalog);
    estimatePlan(node->right, catalog);

    if (node->type == PlanType::Scan) {
        const auto* t = findTable(catalog, node->label);
        node->estCardinality = t ? t->rowCount : 1000.0;
        node->estCost = node->estCardinality;
        return;
    }

    if (node->type == PlanType::Filter) {
        const double childCard = nodeCard(node->left);
        const double childCost = nodeCost(node->left);
        const double sel = estimateSelectivity(node->pred, catalog);
        node->estCardinality = childCard * sel;
        node->estCost = childCost + childCard;
        return;
    }

    if (node->type == PlanType::HashJoin) {
        const double lCard = nodeCard(node->left);
        const double rCard = nodeCard(node->right);
        const double lCost = nodeCost(node->left);
        const double rCost = nodeCost(node->right);
        const double sel = estimateSelectivity(node->pred, catalog);
        node->estCardinality = lCard * rCard * sel;
        node->estCost = lCost + rCost + 2.0 * lCard + rCard + node->estCardinality;
        return;
    }

    if (node->type == PlanType::CrossProduct) {
        const double lCard = nodeCard(node->left);
        const double rCard = nodeCard(node->right);
        const double lCost = nodeCost(node->left);
        const double rCost = nodeCost(node->right);
        node->estCardinality = lCard * rCard;
        node->estCost = lCost + rCost + (lCard * rCard);
        return;
    }

    if (node->type == PlanType::GroupBy) {
        const double childCard = nodeCard(node->left);
        const double childCost = nodeCost(node->left);
        node->estCardinality = std::max(1.0, childCard * 0.1);
        node->estCost = childCost + childCard;
        return;
    }

    if (node->type == PlanType::Project) {
        const double childCard = nodeCard(node->left);
        const double childCost = nodeCost(node->left);
        node->estCardinality = childCard;
        node->estCost = childCost + childCard * 0.1;
        return;
    }

    if (node->type == PlanType::Limit) {
        const double childCard = nodeCard(node->left);
        const double childCost = nodeCost(node->left);
        const double lim = std::max(0, std::stoi(node->label));
        node->estCardinality = std::min(childCard, lim);
        node->estCost = childCost;
        return;
    }
}
