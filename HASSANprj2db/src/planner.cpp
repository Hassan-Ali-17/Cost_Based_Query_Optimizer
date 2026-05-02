#include "planner.h"
#include <iostream>

shared_ptr<LogicalPlan> Planner::build_naive_plan(const Query& query) {
    if (query.from_tables.empty()) return nullptr;

    // 1. FROM clause (Cross products)
    shared_ptr<LogicalPlan> plan = make_shared<Scan>(query.from_tables[0]);
    for (size_t i = 1; i < query.from_tables.size(); ++i) {
        plan = make_shared<CrossProduct>(plan, make_shared<Scan>(query.from_tables[i]));
    }

    // 2. WHERE clause
    for (const auto& pred : query.where_preds) {
        plan = make_shared<Filter>(pred, plan);
    }

    // 3. GROUP BY clause
    if (query.has_group_by) {
        // Find aggregate in select list to know what to compute
        AggType agg = AggType::NONE;
        for (const auto& expr : query.select_list) {
            if (expr.type == ExprType::AGGREGATE) {
                agg = expr.agg;
                break;
            }
        }
        plan = make_shared<GroupBy>(query.group_by_col, agg, plan);
    }

    // 4. SELECT clause (Projection)
    if (!query.select_star) {
        plan = make_shared<Project>(query.select_list, plan);
    }

    // 5. LIMIT clause
    if (query.has_limit) {
        plan = make_shared<Limit>(query.limit_count, plan);
    }

    return plan;
}
