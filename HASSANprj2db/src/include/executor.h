#pragma once

#include "plan.h"
#include "catalog.h"
#include <vector>
#include <memory>
#include <string>   
using namespace std;
struct ExecResult {
    vector<ColumnRef> schema;
    vector<Row> rows;
};

class Executor {
    Catalog* catalog;
    
    ExecResult execute_scan(const Scan* scan);
    ExecResult execute_filter(const Filter* filter);
    ExecResult execute_crossproduct(const CrossProduct* cp);
    ExecResult execute_hashjoin(const HashJoin* hj);
    ExecResult execute_project(const Project* proj);
    ExecResult execute_groupby(const GroupBy* gb);
    ExecResult execute_limit(const Limit* limit);
    
    // Helper to evaluate predicate on a row
    bool evaluate_predicate(const Predicate& pred, const Row& row, const vector<ColumnRef>& schema);
    size_t get_col_index(const ColumnRef& ref, const vector<ColumnRef>& schema);

public:
    Executor(Catalog* c) : catalog(c) {}
    
    ExecResult execute(const shared_ptr<LogicalPlan>& plan);
};
