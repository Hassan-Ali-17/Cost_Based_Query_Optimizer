#include <iostream>
#include <string>
#include <sstream>
#include <chrono>
#include "parser.h"
#include "planner.h"
#include "executor.h"
#include "catalog.h"
#include "optimizer.h"

using namespace std;

void print_help() {
    cout << "qopt shell commands:\n";
    cout << "  LOAD <data_dir>   - Load catalog from CSV files\n";
    cout << "  QUERY <sql>       - Execute a SQL query (with optimization)\n";
    cout << "  EXPLAIN <sql>     - Show the optimized logical plan\n";
    cout << "  \\validate <sql>  - Compare Estimated vs Actual row counts\n";
    cout << "  \\stats           - Print catalog statistics\n";
    cout << "  exit              - Exit the shell\n";
}

int main(int argc, char** argv) {
    Catalog catalog;
    Parser parser;
    Planner planner;
    Executor executor(&catalog);
    Optimizer optimizer(&catalog);

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
                
                optimizer.clear_stats();
                auto optimized_plan = optimizer.optimize(plan);
                
                cout << "optimized plan:\n";
                if (optimized_plan) optimized_plan->print(1, &optimizer, &executor);
                else cout << "  (empty plan)\n";
            } catch (const exception& e) {
                cout << "Error parsing/optimizing query: " << e.what() << "\n";
            }
        } else if (line.rfind("\\validate ", 0) == 0) {
            string sql = line.substr(10);
            try {
                Query q = parser.parse(sql);
                auto plan = planner.build_naive_plan(q);
                
                optimizer.clear_stats();
                auto optimized_plan = optimizer.optimize(plan);
                
                // Run it to get actual stats
                executor.last_run_stats.clear();
                executor.execute(optimized_plan);
                
                cout << "validation plan (Estimated vs Actual):\n";
                if (optimized_plan) optimized_plan->print(1, &optimizer, &executor);
                else cout << "  (empty plan)\n";
            } catch (const exception& e) {
                cout << "Error during validation: " << e.what() << "\n";
            }
        } else if (line == "\\benchmark") {
            // Simple benchmark runner for the five project queries
            vector<string> queries = {
                "SELECT * FROM customers, orders WHERE customers.id = orders.customer_id AND customers.country = 'PK'",
                "SELECT customers.name, SUM(line_items.qty * line_items.price) FROM customers, orders, line_items WHERE customers.id = orders.customer_id AND orders.id = line_items.order_id AND customers.country = 'PK' AND orders.year = 2024",
                "SELECT customers.name, products.name FROM customers, orders, line_items, products WHERE customers.id = orders.customer_id AND orders.id = line_items.order_id AND line_items.product_id = products.id AND customers.country = 'PK' AND products.category = 'Electronics'",
                "SELECT customers.country, SUM(orders.total) FROM customers, orders WHERE customers.id = orders.customer_id AND orders.year = 2024 GROUP BY customers.country",
                "SELECT customers.name FROM customers, orders WHERE customers.id = orders.customer_id AND orders.total > 1000 AND customers.age > 30"
            };

            vector<string> configs = {"no-opt", "rules-only", "dp-only", "full"};

            for (size_t qi = 0; qi < queries.size(); ++qi) {
                cout << "\n=== Query Q" << (qi+1) << " ===\n";
                string sql = queries[qi];
                for (const auto& cfg : configs) {
                    try {
                        Query q = parser.parse(sql);
                        auto base_plan = planner.build_naive_plan(q);

                        shared_ptr<LogicalPlan> plan_to_run;

                        if (cfg == "no-opt") {
                            plan_to_run = base_plan;
                        } else if (cfg == "rules-only") {
                            optimizer.clear_stats();
                            plan_to_run = optimizer.apply_rules(base_plan);
                        } else if (cfg == "dp-only") {
                            optimizer.clear_stats();
                            plan_to_run = optimizer.optimize_joins(base_plan);
                        } else { // full
                            optimizer.clear_stats();
                            plan_to_run = optimizer.optimize(base_plan);
                        }

                        // Get estimated cost
                        optimizer.clear_stats();
                        PlanStats est = optimizer.estimate(plan_to_run.get());

                        // Execute and time
                        executor.last_run_stats.clear();
                        auto t0 = chrono::steady_clock::now();
                        ExecResult res = executor.execute(plan_to_run);
                        auto t1 = chrono::steady_clock::now();
                        double ms = chrono::duration_cast<chrono::milliseconds>(t1 - t0).count();

                        cout << cfg << ": est_cost=" << est.cost << ", est_rows=" << est.cardinality << ", time_ms=" << ms << ", rows=" << res.rows.size() << "\n";
                    } catch (const exception& e) {
                        cout << cfg << ": ERROR: " << e.what() << "\n";
                    }
                }
            }
        } else if (line.rfind("QUERY ", 0) == 0 || line.rfind("SELECT", 0) == 0 || line.rfind("select", 0) == 0) {
            string sql = line;
            if (line.rfind("QUERY ", 0) == 0) sql = line.substr(6);
            
            try {
                Query q = parser.parse(sql);
                auto plan = planner.build_naive_plan(q);
                
                optimizer.clear_stats();
                auto optimized_plan = optimizer.optimize(plan);
                
                ExecResult results = executor.execute(optimized_plan);
                
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
    return 0;
}
