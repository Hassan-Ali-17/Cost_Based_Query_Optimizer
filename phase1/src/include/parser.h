#pragma once

#include <string>
#include <vector>
#include <memory>
#include "types.h"
using namespace std;

enum class OpType {
    EQ, NEQ, LT, LTE, GT, GTE
};

enum class AggType {
    NONE, SUM, COUNT, AVG, MIN, MAX
};

enum class ExprType {
    COLUMN, AGGREGATE, BINARY_OP, LITERAL
};

struct ColumnRef {
    string table;
    string column;
};

struct Expr {
    ExprType type;
    ColumnRef col1;
    
    // For AGGREGATE
    AggType agg = AggType::NONE;
    
    // For BINARY_OP
    OpType op;
    ColumnRef col2; // if column op column
    
    // For LITERAL
    Value literal;
};

struct Predicate {
    ColumnRef col1;
    OpType op;
    bool is_col2 = false;
    ColumnRef col2;
    Value literal;
};

struct Query {
    bool select_star = false;
    vector<Expr> select_list;
    vector<string> from_tables;
    vector<Predicate> where_preds;
    
    bool has_group_by = false;
    ColumnRef group_by_col;
    
    bool has_limit = false;
    int limit_count = 0;
};

class Parser {
public:
    Query parse(const string& sql);
};
