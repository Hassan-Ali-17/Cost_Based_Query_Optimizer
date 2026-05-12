#include "catalog.h"
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

void Catalog::load_from_csv(const std::string& data_dir) {
    // Define schemas according to manual
    vector<TableSchema> schemas = {
        {"customers", {{"id", DataType::INT, true}, {"name", DataType::TEXT}, {"country", DataType::TEXT}, {"age", DataType::INT}}},
        {"orders", {{"id", DataType::INT, true}, {"customer_id", DataType::INT}, {"total", DataType::DOUBLE}, {"year", DataType::INT}, {"status", DataType::TEXT}}},
        {"line_items", {{"order_id", DataType::INT}, {"product_id", DataType::INT}, {"qty", DataType::INT}, {"price", DataType::DOUBLE}}},
        {"products", {{"id", DataType::INT, true}, {"name", DataType::TEXT}, {"category", DataType::TEXT}, {"supplier_id", DataType::INT}}}
    };

    for (const auto& schema : schemas) {
        Table table;
        table.schema = schema;
        table.csv_path = data_dir + "/" + schema.name + ".csv";
        
        ifstream file(table.csv_path);
        if (!file.is_open()) {
            cerr << "Warning: Could not open " << table.csv_path << "\n";
            this->add_table(table);
            continue;
        }

        string line;
        size_t row_count = 0;
        
        vector<unordered_set<string>> distinct_sets(schema.columns.size());
        vector<double> min_vals(schema.columns.size(), numeric_limits<double>::max());
        vector<double> max_vals(schema.columns.size(), numeric_limits<double>::lowest());

        while (getline(file, line)) {
            if (line.empty()) continue;
            row_count++;
            
            stringstream ss(line);
            string cell;
            size_t col_idx = 0;
            Row row;
            while (getline(ss, cell, '|') && col_idx < schema.columns.size()) {
                distinct_sets[col_idx].insert(cell);
                
                if (schema.columns[col_idx].type == DataType::INT) {
                    try {
                        int val = stoi(cell);
                        row.values.push_back(Value(val));
                        if (val < min_vals[col_idx]) min_vals[col_idx] = val;
                        if (val > max_vals[col_idx]) max_vals[col_idx] = val;
                    } catch (...) { row.values.push_back(Value(0)); }
                } else if (schema.columns[col_idx].type == DataType::DOUBLE) {
                    try {
                        double val = stod(cell);
                        row.values.push_back(Value(val));
                        if (val < min_vals[col_idx]) min_vals[col_idx] = val;
                        if (val > max_vals[col_idx]) max_vals[col_idx] = val;
                    } catch (...) { row.values.push_back(Value(0.0)); }
                } else {
                    row.values.push_back(Value(cell));
                }
                col_idx++;
            }
            table.rows.push_back(row);
        }
        
        table.row_count = row_count;
        for (size_t i = 0; i < schema.columns.size(); ++i) {
            ColumnStats cstats;
            cstats.distinct_count = distinct_sets[i].size();
            cstats.null_count = 0;
            if (schema.columns[i].type == DataType::INT || schema.columns[i].type == DataType::DOUBLE) {
                if (row_count > 0) {
                    cstats.has_min_max = true;
                    cstats.min_value = min_vals[i];
                    cstats.max_value = max_vals[i];
                }
            }
            table.stats[schema.columns[i].name] = cstats;
        }
        
        this->add_table(table);
    }
}

void Catalog::save_to_json(const std::string& filename) {
    json j;
    for (const auto& [name, table] : tables) {
        json j_table;
        j_table["name"] = table.schema.name;
        j_table["row_count"] = table.row_count;
        
        json j_cols = json::array();
        for (const auto& col : table.schema.columns) {
            json j_col;
            j_col["name"] = col.name;
            j_col["type"] = static_cast<int>(col.type);
            j_col["is_pk"] = col.is_pk;
            
            auto stat_it = table.stats.find(col.name);
            if (stat_it != table.stats.end()) {
                j_col["distinct_count"] = stat_it->second.distinct_count;
                j_col["has_min_max"] = stat_it->second.has_min_max;
                if (stat_it->second.has_min_max) {
                    j_col["min_value"] = stat_it->second.min_value;
                    j_col["max_value"] = stat_it->second.max_value;
                }
            }
            j_cols.push_back(j_col);
        }
        j_table["columns"] = j_cols;
        j["tables"].push_back(j_table);
    }
    
    ofstream file(filename);
    if (file.is_open()) {
        file << j.dump(4);
    }
}

void Catalog::load_from_json(const std::string& filename) {
    ifstream file(filename);
    if (!file.is_open()) return;
    
    json j;
    file >> j;
    
    tables.clear();
    for (const auto& j_table : j["tables"]) {
        Table table;
        table.schema.name = j_table["name"];
        table.row_count = j_table["row_count"];
        
        for (const auto& j_col : j_table["columns"]) {
            ColumnSchema col;
            col.name = j_col["name"];
            col.type = static_cast<DataType>(j_col["type"]);
            col.is_pk = j_col["is_pk"];
            table.schema.columns.push_back(col);
            
            ColumnStats stats;
            stats.distinct_count = j_col["distinct_count"];
            stats.has_min_max = j_col["has_min_max"];
            if (stats.has_min_max) {
                stats.min_value = j_col["min_value"];
                stats.max_value = j_col["max_value"];
            }
            table.stats[col.name] = stats;
        }
        
        tables[table.schema.name] = table;
    }
}
