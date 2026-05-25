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
        if (type == other.type) {
            if (type == DataType::INT) return get<int>(data) == get<int>(other.data);
            if (type == DataType::DOUBLE) return get<double>(data) == get<double>(other.data);
            return get<string>(data) == get<string>(other.data);
        }
        if (type == DataType::INT && other.type == DataType::DOUBLE) {
            return get<int>(data) == get<double>(other.data);
        }
        if (type == DataType::DOUBLE && other.type == DataType::INT) {
            return get<double>(data) == get<int>(other.data);
        }
        return false;
    }
    
    bool operator!=(const Value& other) const { return !(*this == other); }
    
    bool operator<(const Value& other) const {
        if (type == other.type) {
            if (type == DataType::INT) return get<int>(data) < get<int>(other.data);
            if (type == DataType::DOUBLE) return get<double>(data) < get<double>(other.data);
            return get<string>(data) < get<string>(other.data);
        }
        if (type == DataType::INT && other.type == DataType::DOUBLE) {
            return get<int>(data) < get<double>(other.data);
        }
        if (type == DataType::DOUBLE && other.type == DataType::INT) {
            return get<double>(data) < get<int>(other.data);
        }
        throw runtime_error("Type mismatch in <");
    }
    
    bool operator<=(const Value& other) const { return (*this < other) || (*this == other); }
    bool operator>(const Value& other) const { return !(*this <= other); }
    bool operator>=(const Value& other) const { return !(*this < other); }

    Value operator+(const Value& other) const {
        if (type == DataType::INT && other.type == DataType::INT) return Value(get<int>(data) + get<int>(other.data));
        if (type == DataType::DOUBLE && other.type == DataType::DOUBLE) return Value(get<double>(data) + get<double>(other.data));
        if (type == DataType::INT && other.type == DataType::DOUBLE) return Value(get<int>(data) + get<double>(other.data));
        if (type == DataType::DOUBLE && other.type == DataType::INT) return Value(get<double>(data) + get<int>(other.data));
        throw runtime_error("Invalid types for +");
    }

    Value operator/(int count) const {
        if (count == 0) return Value(0.0);
        if (type == DataType::INT) return Value(static_cast<double>(get<int>(data)) / count);
        if (type == DataType::DOUBLE) return Value(get<double>(data) / count);
        throw runtime_error("Invalid types for /");
    }
};

// Hash for Value to use in unordered_map
namespace std {
    template <>
    struct hash<Value> {
        size_t operator()(const Value& v) const {
            if (v.type == DataType::INT) return hash<int>{}(get<int>(v.data));
            if (v.type == DataType::DOUBLE) return hash<double>{}(get<double>(v.data));
            return hash<string>{}(get<string>(v.data));
        }
    };
}

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
