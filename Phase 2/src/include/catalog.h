#pragma once

#include "types.h"
#include <string>
#include <vector>
#include <unordered_map>
using namespace std;
struct ColumnStats {
    size_t distinct_count = 0;
    bool has_min_max = false;
    double min_value = 0;
    double max_value = 0;
    size_t null_count = 0;
};

struct Table {
    TableSchema schema;
    size_t row_count = 0;
    unordered_map<string, ColumnStats> stats;
    string csv_path;
    vector<Row> rows;
};

class Catalog {
public:
    unordered_map<string, Table> tables;

    void add_table(const Table& table) {
        tables[table.schema.name] = table;
    }

    Table* get_table(const string& name) {
        auto it = tables.find(name);
        if (it != tables.end()) return &it->second;
        return nullptr;
    }

    // Load tables from CSVs and generate statistics
    void load_from_csv(const string& data_dir);

    // Save/Load to catalog.json
    void save_to_json(const string& filename);
    void load_from_json(const string& filename);
};
