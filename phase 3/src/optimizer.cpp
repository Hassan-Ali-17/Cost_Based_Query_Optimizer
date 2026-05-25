#include "optimizer.h"
#include <algorithm>
#include <unordered_map>
#include <cmath>

shared_ptr<LogicalPlan> Optimizer::optimize(shared_ptr<LogicalPlan> plan) {
    // 1. Apply heuristic rules (Fixed-point)
    plan = apply_rules(plan);
    
    // 2. Join-order search (Selinger DP)
    plan = optimize_joins(plan);
    
    // 3. Cost estimation
    clear_stats();
    estimate(plan.get());
    
    // 4. Physical reordering (Join Input Swap)
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

static bool evaluate_constant_predicate(const Predicate& pred) {
    Value v1 = pred.literal1;
    Value v2 = pred.literal;
    switch(pred.op) {
        case OpType::EQ: return v1 == v2;
        case OpType::NEQ: return v1 != v2;
        case OpType::LT: return v1 < v2;
        case OpType::LTE: return v1 <= v2;
        case OpType::GT: return v1 > v2;
        case OpType::GTE: return v1 >= v2;
    }
    return false;
}

shared_ptr<LogicalPlan> Optimizer::rule_constant_folding(shared_ptr<LogicalPlan> plan, bool& changed) {
    if (!plan) return nullptr;

    // Recurse first
    if (auto filter = dynamic_pointer_cast<Filter>(plan)) {
        filter->child = rule_constant_folding(filter->child, changed);
        
        if (!filter->pred.is_col1 && !filter->pred.is_col2) {
            bool val = evaluate_constant_predicate(filter->pred);
            changed = true;
            if (val) {
                return filter->child; // remove TRUE filter
            }
        }
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
            
            bool uses_left = filter->pred.is_col1 && left_tables.count(filter->pred.col1.table);
            bool uses_right = filter->pred.is_col1 && right_tables.count(filter->pred.col1.table);
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
            
            bool uses_left = filter->pred.is_col1 && left_tables.count(filter->pred.col1.table);
            bool uses_right = filter->pred.is_col1 && right_tables.count(filter->pred.col1.table);
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
    if (p.is_col1) cols.insert(p.col1);
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
        auto left_needed = needed;
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
        if (!gb->agg_col.column.empty()) needed.insert(gb->agg_col);
        gb->child = rule_projection_pushdown(gb->child, needed);
        return gb;
    } else if (auto lim = dynamic_pointer_cast<Limit>(plan)) {
        lim->child = rule_projection_pushdown(lim->child, needed);
        return lim;
    } else if (auto scan = dynamic_pointer_cast<Scan>(plan)) {
        Table* t = catalog->get_table(scan->table_name);
        if (!t) return scan;
        
        vector<Expr> proj_exprs;
        for (const auto& col : t->schema.columns) {
            ColumnRef ref = {scan->table_name, col.name};
            if (needed.empty() || needed.count(ref)) {
                Expr e;
                e.type = ExprType::COLUMN;
                e.col1 = ref;
                proj_exprs.push_back(e);
            }
        }
        
        if (proj_exprs.empty()) {
            Expr e;
            e.type = ExprType::COLUMN;
            e.col1 = {scan->table_name, t->schema.columns[0].name};
            proj_exprs.push_back(e);
        }
        
        if (proj_exprs.size() < t->schema.columns.size()) {
            return make_shared<Project>(proj_exprs, scan);
        }
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
    if (pred.is_col1 && pred.is_col2) return 0.1; // Default for join predicates if used in filter
    
    if (!pred.is_col1 && !pred.is_col2) {
        // literal op literal
        Value v1 = pred.literal1;
        Value v2 = pred.literal;
        bool res = false;
        switch(pred.op) {
            case OpType::EQ: res = (v1 == v2); break;
            case OpType::NEQ: res = (v1 != v2); break;
            case OpType::LT: res = (v1 < v2); break;
            case OpType::LTE: res = (v1 <= v2); break;
            case OpType::GT: res = (v1 > v2); break;
            case OpType::GTE: res = (v1 >= v2); break;
        }
        return res ? 1.0 : 0.0;
    }
    
    ColumnRef col = pred.is_col1 ? pred.col1 : pred.col2;
    Value lit = pred.is_col1 ? pred.literal : pred.literal1;
    
    Table* t = catalog->get_table(col.table);
    if (!t || !t->stats.count(col.column)) return 0.1;
    
    auto& s = t->stats[col.column];
    double dist = s.distinct_count > 0 ? s.distinct_count : 10.0;
    
    OpType op = pred.op;
    if (!pred.is_col1) {
        switch(op) {
            case OpType::LT: op = OpType::GT; break;
            case OpType::LTE: op = OpType::GTE; break;
            case OpType::GT: op = OpType::LT; break;
            case OpType::GTE: op = OpType::LTE; break;
            default: break;
        }
    }
    
    switch(op) {
        case OpType::EQ: return 1.0 / dist;
        case OpType::NEQ: return 1.0 - (1.0 / dist);
        case OpType::LT:
        case OpType::LTE:
        case OpType::GT:
        case OpType::GTE:
            if (s.has_min_max && lit.type != DataType::TEXT) {
                double val = (lit.type == DataType::INT) ? std::get<int>(lit.data) : std::get<double>(lit.data);
                if (val <= s.min_value) return (op == OpType::GT || op == OpType::GTE) ? 1.0 : 0.01;
                if (val >= s.max_value) return (op == OpType::LT || op == OpType::LTE) ? 1.0 : 0.01;
                
                double range = s.max_value - s.min_value;
                if (range <= 0) return 0.5;
                if (op == OpType::LT || op == OpType::LTE) return (val - s.min_value) / range;
                else return (s.max_value - val) / range;
            }
            return 0.33; // Default for range
        default: return 0.1;
    }
}

// ============================================================
// Phase 3: Selinger DP Join-Order Search
// ============================================================

// Extract base table plans (leaf nodes under joins/cross products)
static void extract_base_tables(
    shared_ptr<LogicalPlan> plan,
    std::vector<shared_ptr<LogicalPlan>>& base_tables)
{
    if (auto hj = dynamic_pointer_cast<HashJoin>(plan)) {
        extract_base_tables(hj->left, base_tables);
        extract_base_tables(hj->right, base_tables);
    } else if (auto cp = dynamic_pointer_cast<CrossProduct>(plan)) {
        extract_base_tables(cp->left, base_tables);
        extract_base_tables(cp->right, base_tables);
    } else {
        // Leaf: Scan, Filter(Scan), Project(Scan), etc.
        base_tables.push_back(plan);
    }
}

// Extract join conditions from the plan tree
static void extract_join_conditions(
    shared_ptr<LogicalPlan> plan,
    std::vector<Predicate>& conditions)
{
    if (auto hj = dynamic_pointer_cast<HashJoin>(plan)) {
        conditions.push_back(hj->condition);
        extract_join_conditions(hj->left, conditions);
        extract_join_conditions(hj->right, conditions);
    } else if (auto cp = dynamic_pointer_cast<CrossProduct>(plan)) {
        extract_join_conditions(cp->left, conditions);
        extract_join_conditions(cp->right, conditions);
    }
    // Leaves (Scan, Filter, Project) have no join conditions
}

// Check if a plan subtree is a join tree (contains HashJoin or CrossProduct at root)
static bool is_join_tree(shared_ptr<LogicalPlan> plan) {
    return dynamic_pointer_cast<HashJoin>(plan) != nullptr ||
           dynamic_pointer_cast<CrossProduct>(plan) != nullptr;
}

// Get the set of table names referenced by a base table plan
static std::set<std::string> get_tables_in_plan(shared_ptr<LogicalPlan> plan) {
    std::set<std::string> tables;
    if (auto scan = dynamic_pointer_cast<Scan>(plan)) {
        tables.insert(scan->table_name);
    } else if (auto filter = dynamic_pointer_cast<Filter>(plan)) {
        return get_tables_in_plan(filter->child);
    } else if (auto proj = dynamic_pointer_cast<Project>(plan)) {
        return get_tables_in_plan(proj->child);
    } else if (auto hj = dynamic_pointer_cast<HashJoin>(plan)) {
        auto l = get_tables_in_plan(hj->left);
        auto r = get_tables_in_plan(hj->right);
        tables.insert(l.begin(), l.end());
        tables.insert(r.begin(), r.end());
    } else if (auto cp = dynamic_pointer_cast<CrossProduct>(plan)) {
        auto l = get_tables_in_plan(cp->left);
        auto r = get_tables_in_plan(cp->right);
        tables.insert(l.begin(), l.end());
        tables.insert(r.begin(), r.end());
    }
    return tables;
}

// Find the join condition connecting tables in left_tables to table_name
static Predicate* find_join_condition(
    const std::set<std::string>& left_tables,
    const std::string& right_table,
    std::vector<Predicate>& conditions)
{
    for (auto& cond : conditions) {
        if (!cond.is_col1 || !cond.is_col2) continue; // skip non-join predicates
        std::string t1 = cond.col1.table;
        std::string t2 = cond.col2.table;
        if ((left_tables.count(t1) && right_table == t2) ||
            (left_tables.count(t2) && right_table == t1)) {
            return &cond;
        }
    }
    return nullptr;
}

shared_ptr<LogicalPlan> Optimizer::optimize_joins(shared_ptr<LogicalPlan> plan) {
    if (!plan) return nullptr;

    // Recursively optimize joins in children first
    if (auto proj = dynamic_pointer_cast<Project>(plan)) {
        proj->child = optimize_joins(proj->child);
        return proj;
    } else if (auto filter = dynamic_pointer_cast<Filter>(plan)) {
        filter->child = optimize_joins(filter->child);
        return filter;
    } else if (auto gb = dynamic_pointer_cast<GroupBy>(plan)) {
        gb->child = optimize_joins(gb->child);
        return gb;
    } else if (auto lim = dynamic_pointer_cast<Limit>(plan)) {
        lim->child = optimize_joins(lim->child);
        return lim;
    }

    // If this node is a join tree root, run Selinger DP
    if (!is_join_tree(plan)) return plan;

    // 1. Extract base tables and join conditions
    std::vector<shared_ptr<LogicalPlan>> base_tables;
    std::vector<Predicate> join_conditions;
    extract_base_tables(plan, base_tables);
    extract_join_conditions(plan, join_conditions);

    int n = (int)base_tables.size();
    if (n <= 1) return plan; // nothing to reorder

    // Build a mapping: table index -> table names set
    std::vector<std::set<std::string>> table_names(n);
    for (int i = 0; i < n; i++) {
        table_names[i] = get_tables_in_plan(base_tables[i]);
    }

    // 2. Selinger DP using bitmask subsets
    int total_subsets = 1 << n;
    
    // dp[mask] = best plan for joining the subset represented by mask
    std::vector<shared_ptr<LogicalPlan>> dp_plan(total_subsets, nullptr);
    std::vector<double> dp_cost(total_subsets, 1e18);

    // Initialize singletons
    for (int i = 0; i < n; i++) {
        int mask = 1 << i;
        dp_plan[mask] = base_tables[i];
        clear_stats();
        PlanStats s = estimate(base_tables[i].get());
        dp_cost[mask] = s.cost;
    }

    // 3. Fill DP table for subsets of size >= 2, in increasing size order
    for (int size = 2; size <= n; size++) {
        for (int mask = 0; mask < total_subsets; mask++) {
            if (__builtin_popcount(mask) != size) continue;

            // First pass: prefer connected joins (skip cross-products)
            for (int i = 0; i < n; i++) {
                int t_bit = 1 << i;
                if (!(mask & t_bit)) continue; // t not in this subset

                int left_mask = mask & ~t_bit;
                if (left_mask == 0) continue; // left can't be empty
                if (!dp_plan[left_mask]) continue; // no valid left plan

                // Get tables covered by left_mask
                std::set<std::string> left_tbls;
                for (int j = 0; j < n; j++) {
                    if (left_mask & (1 << j)) {
                        left_tbls.insert(table_names[j].begin(), table_names[j].end());
                    }
                }

                // Get a table name from right side
                std::string right_table;
                for (auto& tname : table_names[i]) {
                    right_table = tname;
                    break;
                }

                // Find a join condition connecting left to right
                Predicate* cond = find_join_condition(left_tbls, right_table, join_conditions);

                if (!cond) continue; // skip cross-product in first pass

                shared_ptr<LogicalPlan> candidate = make_shared<HashJoin>(*cond, dp_plan[left_mask], dp_plan[t_bit]);

                clear_stats();
                PlanStats s = estimate(candidate.get());
                if (s.cost < dp_cost[mask]) {
                    dp_cost[mask] = s.cost;
                    dp_plan[mask] = candidate;
                }
            }

            // Second pass: if no connected join was found, allow cross-products as a fallback
            if (!dp_plan[mask]) {
                for (int i = 0; i < n; i++) {
                    int t_bit = 1 << i;
                    if (!(mask & t_bit)) continue;

                    int left_mask = mask & ~t_bit;
                    if (left_mask == 0) continue;
                    if (!dp_plan[left_mask]) continue;

                    shared_ptr<LogicalPlan> candidate = make_shared<CrossProduct>(dp_plan[left_mask], dp_plan[t_bit]);
                    clear_stats();
                    PlanStats s = estimate(candidate.get());
                    if (s.cost < dp_cost[mask]) {
                        dp_cost[mask] = s.cost;
                        dp_plan[mask] = candidate;
                    }
                }
            }
        }
    }

    int full_mask = total_subsets - 1;
    if (dp_plan[full_mask]) {
        return dp_plan[full_mask];
    }

    return plan; // fallback
}
