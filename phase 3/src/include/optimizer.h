#pragma once

#include "plan.h"
#include "catalog.h"
#include <map>
#include <set>
#include <vector>

struct PlanStats {
    double cardinality;
    double cost;
};

class Optimizer {
    Catalog* catalog;
    std::map<const LogicalPlan*, PlanStats> stats_cache;

public:
    Optimizer(Catalog* cat) : catalog(cat) {}

    // --- Rule Rewriter ---
    shared_ptr<LogicalPlan> optimize(shared_ptr<LogicalPlan> plan);
    
    shared_ptr<LogicalPlan> apply_rules(shared_ptr<LogicalPlan> plan);
    shared_ptr<LogicalPlan> rule_constant_folding(shared_ptr<LogicalPlan> plan, bool& changed);
    shared_ptr<LogicalPlan> rule_predicate_pushdown(shared_ptr<LogicalPlan> plan, bool& changed);
    shared_ptr<LogicalPlan> rule_projection_pushdown(shared_ptr<LogicalPlan> plan, std::set<ColumnRef>& needed_cols);
    shared_ptr<LogicalPlan> rule_join_input_swap(shared_ptr<LogicalPlan> plan);

    // --- Join-Order Search (Selinger DP) ---
    shared_ptr<LogicalPlan> optimize_joins(shared_ptr<LogicalPlan> plan);

    // --- Cost Model ---
    PlanStats estimate(const LogicalPlan* plan);
    double get_selectivity(const Predicate& pred, const LogicalPlan* child);
    
    void clear_stats() { stats_cache.clear(); }
    PlanStats get_stats(const LogicalPlan* plan) { return stats_cache[plan]; }
};
