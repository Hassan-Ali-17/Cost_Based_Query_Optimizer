# Cost-Based Query Optimizer (CBO)

A high-performance SQL query optimizer and execution engine implemented in C++. This project follows the architecture of modern database systems (like Postgres or SparkSQL), featuring a rule-based rewriter and a cost-based selection model.

## 🚀 Phase 2 Features
The current version has completed **Phase 2**, adding intelligent optimization layers to the execution pipeline:

- **Heuristic Rule Rewriter**: 
  - **Predicate Pushdown**: Moves filters past joins to minimize data processing.
  - **Equijoin Conversion**: Converts CrossProducts into efficient HashJoins automatically.
  - **Join Input Swap**: Swaps join children to place the smaller table on the build (left) side.
  - **Projection Pushdown**: Prunes unused columns to reduce memory width.
- **Cost-Based Estimation**:
  - Cardinality estimation using Catalog statistics (distinct counts, nulls, min/max).
  - Selinger-style cost calculation for predicting performance.
- **Accuracy Instrumentation**:
  - Side-by-side comparison of **Estimated vs. Actual** row counts.
  - Fully materialized execution for `SUM`, `COUNT`, `AVG`, `MIN`, and `MAX`.

## 🛠 Build Instructions

### Windows (g++)
```powershell
g++ src/*.cpp -I src/include -std=c++17 -o qopt.exe
.\qopt.exe
```

### Linux/WSL
```bash
g++ src/*.cpp -I src/include -std=c++17 -o qopt
./qopt
```

## 📖 Usage Guide

1.  **Start the Shell**: Run the compiled binary.
2.  **Load Statistics**: Critical for the optimizer to work.
    ```sql
    LOAD .
    ```
3.  **Optimize (EXPLAIN)**: See the optimized logical plan.
    ```sql
    EXPLAIN SELECT name, total FROM customers, orders WHERE id = customer_id AND country = 'PK'
    ```
4.  **Validate Accuracy**: Compare Predicted vs. Real row counts.
    ```sql
    \validate SELECT name, total FROM customers, orders WHERE id = customer_id AND country = 'PK'
    ```
5.  **Execute (QUERY)**:
    ```sql
    QUERY SELECT country, AVG(total) FROM orders, customers WHERE customer_id = id GROUP BY country
    ```

## 📂 Project Structure
- `src/optimizer.cpp`: Heuristic rules and Cost Model logic.
- `src/executor.cpp`: Materialized execution with row counting.
- `src/catalog.cpp`: Metadata management and statistics loading.
- `src/parser.cpp`: SQL Grammar support.
- `src/planner.cpp`: Logical plan generation.

---
*Developed as part of the Database Systems - Cost Based Query Optimizer Project.*
