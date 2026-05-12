#include "optimizer.h"
#include <algorithm>

shared_ptr<LogicalPlan> Optimizer::optimize(shared_ptr<LogicalPlan> plan) {
    // 1. Apply heuristic rules (Fixed-point)
    plan = apply_rules(plan);
    
    // 2. Cost estimation
    estimate(plan.get());
    
    // 3. Physical reordering (Join Input Swap)
    plan = rule_join_input_swap(plan);
    
    return plan;
}

shared_ptr<LogicalPlan> Optimizer::apply_rules(shared_ptr<LogicalPlan> plan) {
    bool changed = true;
    int iterations = 0;
    while (changed && iterations < 10) {
        changed = false;
        plan = rule_constant_folding(plan, changed);
        plan = rule_predicate_pushdown(plan, changed);
        iterations++;
    }
    
    // Final pass: Projection Pushdown
    std::set<ColumnRef> needed;
    plan = rule_projection_pushdown(plan, needed);
    
    return plan;
}

shared_ptr<LogicalPlan> Optimizer::rule_constant_folding(shared_ptr<LogicalPlan> plan, bool& changed) {
    if (!plan) return nullptr;

    // Recurse first
    if (auto filter = dynamic_pointer_cast<Filter>(plan)) {
        filter->child = rule_constant_folding(filter->child, changed);
        
        // Simple Constant Folding: if filter is on a literal? 
        // (Our parser currently mostly produces Col Op Literal, 
        // but if it produced Literal Op Literal we'd handle it here)
        // For now, let's handle "WHERE 1=1" which might be parsed as a ColumnRef 
        // that doesn't exist or a specialized literal predicate.
    } else if (auto cp = dynamic_pointer_cast<CrossProduct>(plan)) {
        cp->left = rule_constant_folding(cp->left, changed);
        cp->right = rule_constant_folding(cp->right, changed);
    } else if (auto hj = dynamic_pointer_cast<HashJoin>(plan)) {
        hj->left = rule_constant_folding(hj->left, changed);
        hj->right = rule_constant_folding(hj->right, changed);
    } else if (auto proj = dynamic_pointer_cast<Project>(plan)) {
        proj->child = rule_constant_folding(proj->child, changed);
    } else if (auto gb = dynamic_pointer_cast<GroupBy>(plan)) {
        gb->child = rule_constant_folding(gb->child, changed);
    } else if (auto lim = dynamic_pointer_cast<Limit>(plan)) {
        lim->child = rule_constant_folding(lim->child, changed);
    }

    return plan;
}

// Placeholder implementations for the rest to make it compile
// Helper to get all tables referenced in a subtree
std::set<std::string> get_referenced_tables(const LogicalPlan* plan) {
    std::set<std::string> tables;
    if (auto scan = dynamic_cast<const Scan*>(plan)) {
        tables.insert(scan->table_name);
    } else if (auto filter = dynamic_cast<const Filter*>(plan)) {
        return get_referenced_tables(filter->child.get());
    } else if (auto cp = dynamic_cast<const CrossProduct*>(plan)) {
        auto l = get_referenced_tables(cp->left.get());
        auto r = get_referenced_tables(cp->right.get());
        tables.insert(l.begin(), l.end());
        tables.insert(r.begin(), r.end());
    } else if (auto hj = dynamic_cast<const HashJoin*>(plan)) {
        auto l = get_referenced_tables(hj->left.get());
        auto r = get_referenced_tables(hj->right.get());
        tables.insert(l.begin(), l.end());
        tables.insert(r.begin(), r.end());
    } else if (auto proj = dynamic_cast<const Project*>(plan)) {
        return get_referenced_tables(proj->child.get());
    } else if (auto gb = dynamic_cast<const GroupBy*>(plan)) {
        return get_referenced_tables(gb->child.get());
    } else if (auto lim = dynamic_cast<const Limit*>(plan)) {
        return get_referenced_tables(lim->child.get());
    }
    return tables;
}

shared_ptr<LogicalPlan> Optimizer::rule_predicate_pushdown(shared_ptr<LogicalPlan> plan, bool& changed) {
    if (!plan) return nullptr;

    if (auto filter = dynamic_pointer_cast<Filter>(plan)) {
        auto child = filter->child;
        
        // Push through CrossProduct
        if (auto cp = dynamic_pointer_cast<CrossProduct>(child)) {
            auto left_tables = get_referenced_tables(cp->left.get());
            auto right_tables = get_referenced_tables(cp->right.get());
            
            bool uses_left = left_tables.count(filter->pred.col1.table);
            bool uses_right = right_tables.count(filter->pred.col1.table);
            if (filter->pred.is_col2) {
                uses_left = uses_left || left_tables.count(filter->pred.col2.table);
                uses_right = uses_right || right_tables.count(filter->pred.col2.table);
            }
            
            if (uses_left && !uses_right) {
                cp->left = make_shared<Filter>(filter->pred, cp->left);
                changed = true;
                return rule_predicate_pushdown(cp, changed); 
            } else if (uses_right && !uses_left) {
                cp->right = make_shared<Filter>(filter->pred, cp->right);
                changed = true;
                return rule_predicate_pushdown(cp, changed); 
            } else if (uses_left && uses_right && filter->pred.is_col2 && filter->pred.op == OpType::EQ) {
                // Convert CrossProduct to HashJoin!
                auto hj = make_shared<HashJoin>(filter->pred, cp->left, cp->right);
                changed = true;
                return rule_predicate_pushdown(hj, changed);
            }
        } else if (auto hj = dynamic_pointer_cast<HashJoin>(child)) {
            // Same logic for HashJoin
            auto left_tables = get_referenced_tables(hj->left.get());
            auto right_tables = get_referenced_tables(hj->right.get());
            
            bool uses_left = left_tables.count(filter->pred.col1.table);
            bool uses_right = right_tables.count(filter->pred.col1.table);
            if (filter->pred.is_col2) {
                uses_left = uses_left || left_tables.count(filter->pred.col2.table);
                uses_right = uses_right || right_tables.count(filter->pred.col2.table);
            }
            
            if (uses_left && !uses_right) {
                hj->left = make_shared<Filter>(filter->pred, hj->left);
                changed = true;
                return rule_predicate_pushdown(hj, changed);
            } else if (uses_right && !uses_left) {
                hj->right = make_shared<Filter>(filter->pred, hj->right);
                changed = true;
                return rule_predicate_pushdown(hj, changed);
            }
        }
        
        filter->child = rule_predicate_pushdown(filter->child, changed);
        return filter;
    }

    // Recurse for other nodes
    if (auto cp = dynamic_pointer_cast<CrossProduct>(plan)) {
        cp->left = rule_predicate_pushdown(cp->left, changed);
        cp->right = rule_predicate_pushdown(cp->right, changed);
    } else if (auto hj = dynamic_pointer_cast<HashJoin>(plan)) {
        hj->left = rule_predicate_pushdown(hj->left, changed);
        hj->right = rule_predicate_pushdown(hj->right, changed);
    } else if (auto proj = dynamic_pointer_cast<Project>(plan)) {
        proj->child = rule_predicate_pushdown(proj->child, changed);
    } else if (auto gb = dynamic_pointer_cast<GroupBy>(plan)) {
        gb->child = rule_predicate_pushdown(gb->child, changed);
    } else if (auto lim = dynamic_pointer_cast<Limit>(plan)) {
        lim->child = rule_predicate_pushdown(lim->child, changed);
    }

    return plan;
}
// Helper to collect all column references in a predicate
void collect_columns(const Predicate& p, std::set<ColumnRef>& cols) {
    cols.insert(p.col1);
    if (p.is_col2) cols.insert(p.col2);
}

// Helper to collect all column references in an expression
void collect_columns(const Expr& e, std::set<ColumnRef>& cols) {
    if (e.type == ExprType::COLUMN || e.type == ExprType::AGGREGATE) {
        cols.insert(e.col1);
    }
}

shared_ptr<LogicalPlan> Optimizer::rule_projection_pushdown(shared_ptr<LogicalPlan> plan, std::set<ColumnRef>& needed) {
    if (!plan) return nullptr;

    if (auto proj = dynamic_pointer_cast<Project>(plan)) {
        std::set<ColumnRef> next_needed;
        for (const auto& e : proj->exprs) collect_columns(e, next_needed);
        proj->child = rule_projection_pushdown(proj->child, next_needed);
        return proj;
    } else if (auto filter = dynamic_pointer_cast<Filter>(plan)) {
        collect_columns(filter->pred, needed);
        filter->child = rule_projection_pushdown(filter->child, needed);
        return filter;
    } else if (auto cp = dynamic_pointer_cast<CrossProduct>(plan)) {
        auto left_needed = needed; // Simplified: just pass all needed to both
        auto right_needed = needed; 
        cp->left = rule_projection_pushdown(cp->left, left_needed);
        cp->right = rule_projection_pushdown(cp->right, right_needed);
        return cp;
    } else if (auto hj = dynamic_pointer_cast<HashJoin>(plan)) {
        collect_columns(hj->condition, needed);
        auto left_needed = needed;
        auto right_needed = needed;
        hj->left = rule_projection_pushdown(hj->left, left_needed);
        hj->right = rule_projection_pushdown(hj->right, right_needed);
        return hj;
    } else if (auto gb = dynamic_pointer_cast<GroupBy>(plan)) {
        needed.insert(gb->col);
        needed.insert(gb->agg_col);
        gb->child = rule_projection_pushdown(gb->child, needed);
        return gb;
    } else if (auto lim = dynamic_pointer_cast<Limit>(plan)) {
        lim->child = rule_projection_pushdown(lim->child, needed);
        return lim;
    } else if (auto scan = dynamic_pointer_cast<Scan>(plan)) {
        // If we were doing real projection pushdown into the scan (like selective column reading),
        // we'd record 'needed' here. For this project, we can just return the scan.
        return scan;
    }

    return plan;
}
shared_ptr<LogicalPlan> Optimizer::rule_join_input_swap(shared_ptr<LogicalPlan> plan) {
    if (!plan) return nullptr;

    if (auto hj = dynamic_pointer_cast<HashJoin>(plan)) {
        hj->left = rule_join_input_swap(hj->left);
        hj->right = rule_join_input_swap(hj->right);
        
        PlanStats sl = estimate(hj->left.get());
        PlanStats sr = estimate(hj->right.get());
        
        if (sr.cardinality < sl.cardinality) {
            auto old_left = hj->left;
            hj->left = hj->right;
            hj->right = old_left;
            // Swap condition as well
            auto old_c1 = hj->condition.col1;
            hj->condition.col1 = hj->condition.col2;
            hj->condition.col2 = old_c1;
        }
        return hj;
    }

    // Recurse
    if (auto cp = dynamic_pointer_cast<CrossProduct>(plan)) {
        cp->left = rule_join_input_swap(cp->left);
        cp->right = rule_join_input_swap(cp->right);
    } else if (auto filter = dynamic_pointer_cast<Filter>(plan)) {
        filter->child = rule_join_input_swap(filter->child);
    } else if (auto proj = dynamic_pointer_cast<Project>(plan)) {
        proj->child = rule_join_input_swap(proj->child);
    } else if (auto gb = dynamic_pointer_cast<GroupBy>(plan)) {
        gb->child = rule_join_input_swap(gb->child);
    } else if (auto lim = dynamic_pointer_cast<Limit>(plan)) {
        lim->child = rule_join_input_swap(lim->child);
    }

    return plan;
}

PlanStats Optimizer::estimate(const LogicalPlan* plan) {
    if (!plan) return {0, 0};
    
    // Check cache
    if (stats_cache.count(plan)) return stats_cache[plan];
    
    PlanStats stats = {0, 0};
    
    if (auto scan = dynamic_cast<const Scan*>(plan)) {
        Table* t = catalog->get_table(scan->table_name);
        stats.cardinality = t ? t->row_count : 0;
        stats.cost = stats.cardinality;
    } else if (auto filter = dynamic_cast<const Filter*>(plan)) {
        PlanStats child = estimate(filter->child.get());
        double sel = get_selectivity(filter->pred, filter->child.get());
        stats.cardinality = child.cardinality * sel;
        stats.cost = child.cost + child.cardinality;
    } else if (auto cp = dynamic_cast<const CrossProduct*>(plan)) {
        PlanStats l = estimate(cp->left.get());
        PlanStats r = estimate(cp->right.get());
        stats.cardinality = l.cardinality * r.cardinality;
        stats.cost = l.cost + r.cost + stats.cardinality;
    } else if (auto hj = dynamic_cast<const HashJoin*>(plan)) {
        PlanStats l = estimate(hj->left.get());
        PlanStats r = estimate(hj->right.get());
        
        // Selinger join formula: L * R / max(dist_L, dist_R)
        double dist_l = 1.0;
        double dist_r = 1.0;
        
        Table* tl = catalog->get_table(hj->condition.col1.table);
        if (tl && tl->stats.count(hj->condition.col1.column)) dist_l = tl->stats[hj->condition.col1.column].distinct_count;
        
        Table* tr = catalog->get_table(hj->condition.col2.table);
        if (tr && tr->stats.count(hj->condition.col2.column)) dist_r = tr->stats[hj->condition.col2.column].distinct_count;
        
        stats.cardinality = (l.cardinality * r.cardinality) / std::max(dist_l, dist_r);
        
        // HashJoin cost: L.cost + R.cost + 2*L.card + R.card + output.card
        stats.cost = l.cost + r.cost + 2 * l.cardinality + r.cardinality + stats.cardinality;
    } else if (auto proj = dynamic_cast<const Project*>(plan)) {
        PlanStats child = estimate(proj->child.get());
        stats.cardinality = child.cardinality;
        stats.cost = child.cost + child.cardinality;
    } else if (auto gb = dynamic_cast<const GroupBy*>(plan)) {
        PlanStats child = estimate(gb->child.get());
        // Simple: cardinality is number of groups. 
        // Estimate as distinct count of grouping column.
        double dist = 1.0;
        Table* t = catalog->get_table(gb->col.table);
        if (t && t->stats.count(gb->col.column)) dist = t->stats[gb->col.column].distinct_count;
        
        stats.cardinality = std::min(child.cardinality, dist);
        stats.cost = child.cost + child.cardinality;
    } else if (auto lim = dynamic_cast<const Limit*>(plan)) {
        PlanStats child = estimate(lim->child.get());
        stats.cardinality = std::min(child.cardinality, (double)lim->n);
        stats.cost = child.cost + stats.cardinality;
    }

    stats_cache[plan] = stats;
    return stats;
}

double Optimizer::get_selectivity(const Predicate& pred, const LogicalPlan* child) {
    if (pred.is_col2) return 0.1; // Default for join predicates if used in filter

    Table* t = catalog->get_table(pred.col1.table);
    if (!t || !t->stats.count(pred.col1.column)) return 0.1;
    
    auto& s = t->stats[pred.col1.column];
    double dist = s.distinct_count > 0 ? s.distinct_count : 10.0;
    
    switch(pred.op) {
        case OpType::EQ: return 1.0 / dist;
        case OpType::NEQ: return 1.0 - (1.0 / dist);
        case OpType::LT:
        case OpType::LTE:
        case OpType::GT:
        case OpType::GTE:
            if (s.has_min_max && pred.literal.type != DataType::TEXT) {
                double val = (pred.literal.type == DataType::INT) ? std::get<int>(pred.literal.data) : std::get<double>(pred.literal.data);
                if (val <= s.min_value) return (pred.op == OpType::GT || pred.op == OpType::GTE) ? 1.0 : 0.01;
                if (val >= s.max_value) return (pred.op == OpType::LT || pred.op == OpType::LTE) ? 1.0 : 0.01;
                
                double range = s.max_value - s.min_value;
                if (range <= 0) return 0.5;
                if (pred.op == OpType::LT || pred.op == OpType::LTE) return (val - s.min_value) / range;
                else return (s.max_value - val) / range;
            }
            return 0.33; // Default for range
        default: return 0.1;
    }
}
