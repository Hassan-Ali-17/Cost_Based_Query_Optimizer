#include "plan.h"
#include <iostream>

void print_indent(int indent) {
    for (int i = 0; i < indent; ++i) std::cout << "  ";
}

std::string op_to_string(OpType op) {
    switch(op) {
        case OpType::EQ: return "=";
        case OpType::NEQ: return "!=";
        case OpType::LT: return "<";
        case OpType::LTE: return "<=";
        case OpType::GT: return ">";
        case OpType::GTE: return ">=";
    }
    return "";
}

std::string pred_to_string(const Predicate& p) {
    std::string s;
    if (!p.col1.table.empty()) s += p.col1.table + ".";
    s += p.col1.column + " " + op_to_string(p.op) + " ";
    if (p.is_col2) {
        if (!p.col2.table.empty()) s += p.col2.table + ".";
        s += p.col2.column;
    } else {
        if (p.literal.type == DataType::TEXT) s += "'" + std::get<std::string>(p.literal.data) + "'";
        else if (p.literal.type == DataType::INT) s += std::to_string(std::get<int>(p.literal.data));
        else if (p.literal.type == DataType::DOUBLE) s += std::to_string(std::get<double>(p.literal.data));
    }
    return s;
}

void Scan::print(int indent) const {
    print_indent(indent);
    std::cout << "Scan(" << table_name << ")\n";
}

void Filter::print(int indent) const {
    print_indent(indent);
    std::cout << "Filter(" << pred_to_string(pred) << ")\n";
    child->print(indent + 1);
}

void CrossProduct::print(int indent) const {
    print_indent(indent);
    std::cout << "CrossProduct\n";
    left->print(indent + 1);
    right->print(indent + 1);
}

void HashJoin::print(int indent) const {
    print_indent(indent);
    std::cout << "HashJoin(" << pred_to_string(condition) << ")\n";
    left->print(indent + 1);
    right->print(indent + 1);
}

void Project::print(int indent) const {
    print_indent(indent);
    std::cout << "Project(";
    for (size_t i = 0; i < exprs.size(); ++i) {
        if (exprs[i].type == ExprType::COLUMN) {
            if (!exprs[i].col1.table.empty()) std::cout << exprs[i].col1.table << ".";
            std::cout << exprs[i].col1.column;
        } else if (exprs[i].type == ExprType::AGGREGATE) {
            std::string agg_str = "SUM";
            if (exprs[i].agg == AggType::COUNT) agg_str = "COUNT";
            else if (exprs[i].agg == AggType::AVG) agg_str = "AVG";
            else if (exprs[i].agg == AggType::MIN) agg_str = "MIN";
            else if (exprs[i].agg == AggType::MAX) agg_str = "MAX";
            
            std::cout << agg_str << "(";
            if (!exprs[i].col1.table.empty()) std::cout << exprs[i].col1.table << ".";
            std::cout << exprs[i].col1.column;
            std::cout << ")";
        }
        if (i < exprs.size() - 1) std::cout << ", ";
    }
    std::cout << ")\n";
    child->print(indent + 1);
}

void GroupBy::print(int indent) const {
    print_indent(indent);
    std::cout << "GroupBy(";
    if (!col.table.empty()) std::cout << col.table << ".";
    std::cout << col.column << ")\n";
    child->print(indent + 1);
}

void Limit::print(int indent) const {
    print_indent(indent);
    std::cout << "Limit(" << n << ")\n";
    child->print(indent + 1);
}
