#include "executor.h"
#include <iostream>
#include <stdexcept>
#include <unordered_map>

ExecResult Executor::execute(const shared_ptr<LogicalPlan>& plan) {
    if (!plan) return {};
    
    if (auto scan = dynamic_cast<const Scan*>(plan.get())) return execute_scan(scan);
    if (auto filter = dynamic_cast<const Filter*>(plan.get())) return execute_filter(filter);
    if (auto cp = dynamic_cast<const CrossProduct*>(plan.get())) return execute_crossproduct(cp);
    if (auto hj = dynamic_cast<const HashJoin*>(plan.get())) return execute_hashjoin(hj);
    if (auto proj = dynamic_cast<const Project*>(plan.get())) return execute_project(proj);
    if (auto gb = dynamic_cast<const GroupBy*>(plan.get())) return execute_groupby(gb);
    if (auto limit = dynamic_cast<const Limit*>(plan.get())) return execute_limit(limit);
    
    return {};
}

size_t Executor::get_col_index(const ColumnRef& ref, const vector<ColumnRef>& schema) {
    for (size_t i = 0; i < schema.size(); ++i) {
        if (schema[i].column == ref.column) {
            if (ref.table.empty() || schema[i].table == ref.table) return i;
        }
    }
    throw runtime_error("Column not found: " + ref.table + "." + ref.column);
}

bool Executor::evaluate_predicate(const Predicate& pred, const Row& row, const vector<ColumnRef>& schema) {
    size_t idx1 = get_col_index(pred.col1, schema);
    Value v1 = row.values[idx1];
    Value v2 = pred.literal;
    
    if (pred.is_col2) {
        size_t idx2 = get_col_index(pred.col2, schema);
        v2 = row.values[idx2];
    }
    
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
    
    // Simplistic hash join:
    size_t l_idx = get_col_index(hj->condition.col1, left_res.schema);
    size_t r_idx = get_col_index(hj->condition.col2, right_res.schema);
    
    // Hash map doesn't easily support Value as key without custom hasher, use nested loop for now
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
    
    for (const auto& expr : proj->exprs) {
        if (expr.type == ExprType::COLUMN) {
            res.schema.push_back(expr.col1);
        } else if (expr.type == ExprType::AGGREGATE) {
            res.schema.push_back({"", "agg"});
        }
    }
    
    // If there is an aggregate but no group by, we expect GroupBy to handle it.
    // In naive planner, GroupBy handles aggregates.
    for (const auto& row : child_res.rows) {
        Row new_row;
        for (const auto& expr : proj->exprs) {
            if (expr.type == ExprType::COLUMN) {
                new_row.values.push_back(row.values[get_col_index(expr.col1, child_res.schema)]);
            } else {
                new_row.values.push_back(Value(0)); // Aggregates handled in GroupBy
            }
        }
        res.rows.push_back(new_row);
    }
    return res;
}

ExecResult Executor::execute_groupby(const GroupBy* gb) {
    ExecResult child_res = execute(gb->child);
    ExecResult res;
    
    if (gb->col.column.empty()) {
        // Global aggregate
        res.schema.push_back({"", "agg"});
        if (child_res.rows.empty()) return res;
        
        // Very basic aggregation placeholder
        Row out; out.values.push_back(Value(0));
        res.rows.push_back(out);
        return res;
    }
    
    size_t group_idx = get_col_index(gb->col, child_res.schema);
    res.schema.push_back(gb->col);
    res.schema.push_back({"", "agg"});
    
    // basic group by
    for (const auto& row : child_res.rows) {
        // placeholder grouping
        Row new_row;
        new_row.values.push_back(row.values[group_idx]);
        new_row.values.push_back(Value(1));
        res.rows.push_back(new_row);
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
