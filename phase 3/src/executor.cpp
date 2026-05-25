#include "executor.h"
#include <iostream>
#include <stdexcept>
#include <unordered_map>

ExecResult Executor::execute(const shared_ptr<LogicalPlan>& plan) {
    if (!plan) return {};
    ExecResult res;

    if (auto scan = dynamic_cast<const Scan*>(plan.get())) res = execute_scan(scan);
    else if (auto filter = dynamic_cast<const Filter*>(plan.get())) res = execute_filter(filter);
    else if (auto cp = dynamic_cast<const CrossProduct*>(plan.get())) res = execute_crossproduct(cp);
    else if (auto hj = dynamic_cast<const HashJoin*>(plan.get())) res = execute_hashjoin(hj);
    else if (auto proj = dynamic_cast<const Project*>(plan.get())) res = execute_project(proj);
    else if (auto gb = dynamic_cast<const GroupBy*>(plan.get())) res = execute_groupby(gb);
    else if (auto limit = dynamic_cast<const Limit*>(plan.get())) res = execute_limit(limit);

    last_run_stats[plan.get()] = res.rows.size();
    return res;
}

size_t Executor::get_col_index(const ColumnRef& ref, const vector<ColumnRef>& schema) {
    // First try exact match: column + table
    for (size_t i = 0; i < schema.size(); ++i) {
        if (!ref.table.empty() && schema[i].column == ref.column && schema[i].table == ref.table) return i;
    }

    // Next try match by column name only (if unique)
    if (ref.table.empty()) {
        ssize_t found = -1;
        for (size_t i = 0; i < schema.size(); ++i) {
            if (schema[i].column == ref.column) {
                if (found != -1) {
                    throw runtime_error("Ambiguous column reference: " + ref.column);
                }
                found = (ssize_t)i;
            }
        }
        if (found != -1) return (size_t)found;
    }

    throw runtime_error("Column not found: " + (ref.table.empty() ? string(".") : ref.table + ".") + ref.column);
}

bool Executor::evaluate_predicate(const Predicate& pred, const Row& row, const vector<ColumnRef>& schema) {
    Value v1 = pred.is_col1 ? row.values[get_col_index(pred.col1, schema)] : pred.literal1;
    Value v2 = pred.is_col2 ? row.values[get_col_index(pred.col2, schema)] : pred.literal;
    
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

ExecResult Executor::execute_scan(const Scan* scan) {
    Table* table = catalog->get_table(scan->table_name);
    if (!table) throw runtime_error("Table not found: " + scan->table_name);
    
    ExecResult res;
    for (const auto& col : table->schema.columns) {
        res.schema.push_back({scan->table_name, col.name});
    }
    res.rows = table->rows;
    return res;
}

ExecResult Executor::execute_filter(const Filter* filter) {
    ExecResult child_res = execute(filter->child);
    ExecResult res;
    res.schema = child_res.schema;
    
    for (const auto& row : child_res.rows) {
        if (evaluate_predicate(filter->pred, row, res.schema)) {
            res.rows.push_back(row);
        }
    }
    return res;
}

ExecResult Executor::execute_crossproduct(const CrossProduct* cp) {
    ExecResult left_res = execute(cp->left);
    ExecResult right_res = execute(cp->right);
    
    ExecResult res;
    res.schema = left_res.schema;
    res.schema.insert(res.schema.end(), right_res.schema.begin(), right_res.schema.end());
    
    for (const auto& l_row : left_res.rows) {
        for (const auto& r_row : right_res.rows) {
            Row new_row;
            new_row.values = l_row.values;
            new_row.values.insert(new_row.values.end(), r_row.values.begin(), r_row.values.end());
            res.rows.push_back(new_row);
        }
    }
    return res;
}

ExecResult Executor::execute_hashjoin(const HashJoin* hj) {
    // HashJoin is an equijoin.
    // For Phase 1 we use CrossProduct primarily via naive planner, but let's implement HashJoin just in case.
    ExecResult left_res = execute(hj->left);
    ExecResult right_res = execute(hj->right);
    
    ExecResult res;
    res.schema = left_res.schema;
    res.schema.insert(res.schema.end(), right_res.schema.begin(), right_res.schema.end());
    
    // Determine which column belongs to left and which to right
    size_t l_idx, r_idx;
    bool col1_on_left = false;
    try {
        l_idx = get_col_index(hj->condition.col1, left_res.schema);
        col1_on_left = true;
    } catch (...) {
        // col1 not on left, so col2 must be on left
    }
    
    try {
        if (col1_on_left) {
            r_idx = get_col_index(hj->condition.col2, right_res.schema);
        } else {
            l_idx = get_col_index(hj->condition.col2, left_res.schema);
            r_idx = get_col_index(hj->condition.col1, right_res.schema);
        }
    } catch (const std::exception& e) {
        std::cerr << "HashJoin resolution failed! Condition: " 
                  << hj->condition.col1.table << "." << hj->condition.col1.column << " = "
                  << hj->condition.col2.table << "." << hj->condition.col2.column << "\n";
        std::cerr << "col1_on_left: " << (col1_on_left ? "true" : "false") << "\n";
        std::cerr << "Left Schema:\n";
        for (const auto& col : left_res.schema) {
            std::cerr << "  " << col.table << "." << col.column << "\n";
        }
        std::cerr << "Right Schema:\n";
        for (const auto& col : right_res.schema) {
            std::cerr << "  " << col.table << "." << col.column << "\n";
        }
        throw;
    }
    
    // Simplistic hash join:
    for (const auto& l_row : left_res.rows) {
        for (const auto& r_row : right_res.rows) {
            if (l_row.values[l_idx] == r_row.values[r_idx]) {
                Row new_row;
                new_row.values = l_row.values;
                new_row.values.insert(new_row.values.end(), r_row.values.begin(), r_row.values.end());
                res.rows.push_back(new_row);
            }
        }
    }
    return res;
}

ExecResult Executor::execute_project(const Project* proj) {
    ExecResult child_res = execute(proj->child);
    ExecResult res;
    
    // In Phase 1, Project often follows GroupBy. 
    // We need to map the "agg" column from GroupBy to the aggregate Expr in Project.
    for (const auto& expr : proj->exprs) {
        if (expr.type == ExprType::COLUMN) {
            res.schema.push_back(expr.col1);
        } else if (expr.type == ExprType::AGGREGATE) {
            res.schema.push_back({"", "agg"}); // Placeholder for aggregate name
        }
    }
    
    for (const auto& row : child_res.rows) {
        Row new_row;
        for (const auto& expr : proj->exprs) {
            if (expr.type == ExprType::COLUMN) {
                new_row.values.push_back(row.values[get_col_index(expr.col1, child_res.schema)]);
            } else if (expr.type == ExprType::AGGREGATE) {
                // Find the aggregate column in the child (GroupBy result)
                new_row.values.push_back(row.values[get_col_index({"", "agg"}, child_res.schema)]);
            }
        }
        res.rows.push_back(new_row);
    }
    return res;
}

struct AggState {
    Value val;
    int count = 0;
    bool first = true;
};

ExecResult Executor::execute_groupby(const GroupBy* gb) {
    ExecResult child_res = execute(gb->child);
    ExecResult res;
    
    unordered_map<Value, AggState> groups;
    bool has_group_col = !gb->col.column.empty();
    
    size_t agg_col_idx = 0;
    bool has_agg_col = !gb->agg_col.column.empty();
    if (has_agg_col) {
        agg_col_idx = get_col_index(gb->agg_col, child_res.schema);
    }
    
    size_t group_col_idx = 0;
    if (has_group_col) {
        group_col_idx = get_col_index(gb->col, child_res.schema);
    }
    
    // Process rows
    for (const auto& row : child_res.rows) {
        Value group_key = has_group_col ? row.values[group_col_idx] : Value(0);
        Value agg_val = has_agg_col ? row.values[agg_col_idx] : Value(1); // 1 for COUNT(*)
        
        AggState& state = groups[group_key];
        if (state.first) {
            state.val = (gb->agg == AggType::COUNT) ? Value(1) : agg_val;
            state.count = 1;
            state.first = false;
        } else {
            state.count++;
            switch(gb->agg) {
                case AggType::SUM:
                case AggType::AVG:
                    state.val = state.val + agg_val;
                    break;
                case AggType::COUNT:
                    state.val = Value(get<int>(state.val.data) + 1);
                    break;
                case AggType::MIN:
                    if (agg_val < state.val) state.val = agg_val;
                    break;
                case AggType::MAX:
                    if (agg_val > state.val) state.val = agg_val;
                    break;
                default: break;
            }
        }
    }
    
    // Build output
    if (has_group_col) res.schema.push_back(gb->col);
    res.schema.push_back({"", "agg"});
    
    for (auto& [key, state] : groups) {
        Row out;
        if (has_group_col) out.values.push_back(key);
        
        Value final_val = state.val;
        if (gb->agg == AggType::AVG) {
            final_val = state.val / state.count;
        }
        out.values.push_back(final_val);
        res.rows.push_back(out);
    }
    
    return res;
}

ExecResult Executor::execute_limit(const Limit* limit) {
    ExecResult child_res = execute(limit->child);
    if (child_res.rows.size() > static_cast<size_t>(limit->n)) {
        child_res.rows.resize(limit->n);
    }
    return child_res;
}
