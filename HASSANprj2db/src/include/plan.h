#pragma once

#include <string>
#include <vector>
#include <memory>
#include "parser.h"
using namespace std;

class LogicalPlan {
public:
    virtual ~LogicalPlan() = default;
    virtual void print(int indent = 0) const = 0;
};

class Scan : public LogicalPlan {
public:
    string table_name;
    Scan(const string& table) : table_name(table) {}
    void print(int indent = 0) const override;
};

class Filter : public LogicalPlan {
public:
    Predicate pred;
    shared_ptr<LogicalPlan> child;
    Filter(const Predicate& p, shared_ptr<LogicalPlan> c) : pred(p), child(c) {}
    void print(int indent = 0) const override;
};

class CrossProduct : public LogicalPlan {
public:
    shared_ptr<LogicalPlan> left;
    shared_ptr<LogicalPlan> right;
    CrossProduct(shared_ptr<LogicalPlan> l, shared_ptr<LogicalPlan> r) : left(l), right(r) {}
    void print(int indent = 0) const override;
};

class HashJoin : public LogicalPlan {
public:
    Predicate condition;
    shared_ptr<LogicalPlan> left;
    shared_ptr<LogicalPlan> right;
    HashJoin(const Predicate& cond, shared_ptr<LogicalPlan> l, shared_ptr<LogicalPlan> r)
        : condition(cond), left(l), right(r) {}
    void print(int indent = 0) const override;
};

class Project : public LogicalPlan {
public:
    vector<Expr> exprs;
    shared_ptr<LogicalPlan> child;
    Project(const vector<Expr>& e, shared_ptr<LogicalPlan> c) : exprs(e), child(c) {}
    void print(int indent = 0) const override;
};

class GroupBy : public LogicalPlan {
public:
    ColumnRef col;
    AggType agg;
    shared_ptr<LogicalPlan> child;
    GroupBy(const ColumnRef& c, AggType a, shared_ptr<LogicalPlan> ch) : col(c), agg(a), child(ch) {}
    void print(int indent = 0) const override;
};

class Limit : public LogicalPlan {
public:
    int n;
    shared_ptr<LogicalPlan> child;
    Limit(int limit, shared_ptr<LogicalPlan> c) : n(limit), child(c) {}
    void print(int indent = 0) const override;
};
