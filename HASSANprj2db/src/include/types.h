#pragma once

#include <string>
#include <vector>
#include <variant>
#include <stdexcept>
#include <iostream>
using namespace std;

enum class DataType {
    INT,
    DOUBLE,
    TEXT
};

struct Value {
    DataType type;
    variant<int, double, string> data;

    Value() : type(DataType::INT), data(0) {}
    Value(int v) : type(DataType::INT), data(v) {}
    Value(double v) : type(DataType::DOUBLE), data(v) {}
    Value(string v) : type(DataType::TEXT), data(v) {}

    bool operator==(const Value& other) const {
        if (type != other.type) return false;
        if (type == DataType::INT) return get<int>(data) == get<int>(other.data);
        if (type == DataType::DOUBLE) return get<double>(data) == get<double>(other.data);
        return get<string>(data) == get<string>(other.data);
    }
    
    bool operator!=(const Value& other) const { return !(*this == other); }
    
    bool operator<(const Value& other) const {
        if (type != other.type) throw runtime_error("Type mismatch in <");
        if (type == DataType::INT) return get<int>(data) < get<int>(other.data);
        if (type == DataType::DOUBLE) return get<double>(data) < get<double>(other.data);
        return get<string>(data) < get<string>(other.data);
    }
    
    bool operator<=(const Value& other) const { return (*this < other) || (*this == other); }
    bool operator>(const Value& other) const { return !(*this <= other); }
    bool operator>=(const Value& other) const { return !(*this < other); }
};

struct Row {
    vector<Value> values;
};

struct ColumnSchema {
    string name;
    DataType type;
    bool is_pk = false;
};

struct TableSchema {
    string name;
    vector<ColumnSchema> columns;
};
