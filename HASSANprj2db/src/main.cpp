#include <iostream>
#include <string>
#include <sstream>
#include "parser.h"
#include "planner.h"
#include "executor.h"
#include "catalog.h"
using namespace std;
void print_help() {
    cout << "qopt shell commands:\n";
    cout << "  LOAD <data_dir>   - Load catalog from CSV files\n";
    cout << "  QUERY <sql>       - Execute a SQL query\n";
    cout << "  EXPLAIN <sql>     - Show the logical plan for a query\n";
    cout << "  \\stats           - Print catalog statistics\n";
    cout << "  exit              - Exit the shell\n";
}

int main(int argc, char** argv) {
    Catalog catalog;
    Parser parser;
    Planner planner;
    Executor executor(&catalog);

    // Optional command line args for initial load
    if (argc == 3 && string(argv[1]) == "--data") {
        catalog.load_from_csv(argv[2]);
        cout << "qopt: stats loaded for " << catalog.tables.size() << " tables\n";
    }

    string line;
    while (true) {
        cout << "qopt> ";
        if (!getline(cin, line)) break;
        if (line.empty()) continue;

        if (line == "exit" || line == "quit") {
            break;
        } else if (line == "\\stats") {
            for (const auto& [name, table] : catalog.tables) {
                cout << "Table: " << name << " (" << table.row_count << " rows)\n";
                for (const auto& col : table.schema.columns) {
                    size_t dist = 0;
                    if (table.stats.count(col.name)) dist = table.stats.at(col.name).distinct_count;
                    cout << "  " << col.name << " (Distinct: " << dist << ")\n";
                }
            }
        } else if (line.rfind("LOAD ", 0) == 0 || line.rfind("load ", 0) == 0) {
            string dir = line.substr(5);
            catalog.load_from_csv(dir);
            cout << "Loaded catalog from " << dir << "\n";
        } else if (line.rfind("EXPLAIN ", 0) == 0) {
            string sql = line.substr(8);
            try {
                Query q = parser.parse(sql);
                auto plan = planner.build_naive_plan(q);
                cout << "plan:\n";
                if (plan) plan->print(1);
                else cout << "  (empty plan)\n";
            } catch (const exception& e) {
                cout << "Error parsing query: " << e.what() << "\n";
            }
        } else if (line.rfind("QUERY ", 0) == 0 || line.rfind("SELECT", 0) == 0 || line.rfind("select", 0) == 0) {
            string sql = line;
            if (line.rfind("QUERY ", 0) == 0) sql = line.substr(6);
            
            try {
                Query q = parser.parse(sql);
                auto plan = planner.build_naive_plan(q);
                ExecResult results = executor.execute(plan);
                
                // Print schema headers
                for (size_t i = 0; i < results.schema.size(); ++i) {
                    if (!results.schema[i].table.empty()) cout << results.schema[i].table << ".";
                    cout << results.schema[i].column;
                    if (i < results.schema.size() - 1) cout << " | ";
                }
                cout << "\n------------------------------------------------\n";
                
                // Print rows
                for (const auto& row : results.rows) {
                    for (size_t i = 0; i < row.values.size(); ++i) {
                        if (row.values[i].type == DataType::INT) cout << get<int>(row.values[i].data);
                        else if (row.values[i].type == DataType::DOUBLE) cout << get<double>(row.values[i].data);
                        else cout << get<string>(row.values[i].data);
                        
                        if (i < row.values.size() - 1) cout << " | ";
                    }
                    cout << "\n";
                }
                cout << "(" << results.rows.size() << " rows)\n";
            } catch (const exception& e) {
                cout << "Error: " << e.what() << "\n";
            }
        } else {
            print_help();
        }
    }

    return 0;
}
