# Cost-Based Query Optimizer (CBO)

A high-performance SQL query optimizer and execution engine implemented in C++. This project follows the architecture of modern database systems (like Postgres or SparkSQL), featuring a rule-based rewriter and a cost-based selection model.

## 🚀 Phase 3 Features
The current version has completed **Phase 3**, incorporating all core optimization features, dynamic programming search, and benchmark verification:

- **Selinger Dynamic Programming Join-Order Search**:
  - Finds the cheapest left-deep binary join tree for queries joining up to 4 tables.
  - Explores the search space bottom-up using bitmask subsets of increasing size.
  - Prunes the search space by preferring connected joins and falling back to cross products only when no join condition connects the subsets.
- **Heuristic Rule Rewriter**: 
  - **Predicate Pushdown**: Moves filters past joins to minimize data processing.
  - **Equijoin Conversion**: Converts CrossProducts into efficient HashJoins automatically.
  - **Join Input Swap**: Swaps join children based on estimated cardinalities to place the smaller table on the build (left) side.
  - **Projection Pushdown**: Prunes unused columns to reduce memory width.
- **Cost-Based Estimation**:
  - Cardinality estimation using Catalog statistics (distinct counts, nulls, min/max).
  - Selinger-style cost calculation for predicting performance.
- **Robust Execution Engine**:
  - **Dynamic Column Resolution**: Updated to seamlessly handle joins with arbitrary left/right table reordering without failing column lookups.
  - **Mixed-Type Value Comparisons**: Fixed comparisons between `INT` and `DOUBLE` values (e.g. comparing columns to literal integers/reals).
  - Side-by-side comparison of **Estimated vs. Actual** row counts.
  - Fully materialized execution for `SUM`, `COUNT`, `AVG`, `MIN`, and `MAX`.

## 🛠 Build Instructions

### Windows (g++)
Since PowerShell doesn't automatically expand wildcards on Windows for `g++`, compile by listing the source files individually:
```powershell
g++ src/catalog.cpp src/executor.cpp src/main.cpp src/optimizer.cpp src/parser.cpp src/plan.cpp src/planner.cpp -I src/include -std=c++17 -o qopt.exe
.\qopt.exe
```

### Linux/WSL
On Unix systems, standard wildcard expansion works:
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

## 💡 Phase 2 Optimization Examples

Below are examples of how Phase 2's optimization rules transform logical plans. First, run `LOAD .` in the shell to load catalog statistics.

### 1. Constant Folding
Redundant or constant filter predicates are evaluated at compile-time and folded.
*   **Query**:
    ```sql
    EXPLAIN SELECT customers.name FROM customers WHERE 1 = 1
    ```
*   **Optimized Plan** (The TRUE filter `1 = 1` is completely eliminated):
    ```text
    Project(customers.name) [Est: 5 rows, cost 15]
      Project(customers.name) [Est: 5 rows, cost 10]
        Scan(customers) [Est: 5 rows, cost 5]
    ```

### 2. Projection Pushdown
Columns that are not needed downstream (for filters, joins, or the final SELECT list) are pruned directly above the Scan nodes to reduce tuple size.
*   **Query**:
    ```sql
    EXPLAIN SELECT customers.name FROM customers, orders WHERE customers.id = orders.customer_id
    ```
*   **Optimized Plan** (Scans are wrapped in Projects limiting column propagation):
    ```text
    Project(customers.name) [Est: 5 rows, cost 45]
      HashJoin(customers.id = orders.customer_id) [Est: 5 rows, cost 40]
        Project(customers.id, customers.name) [Est: 5 rows, cost 10]
          Scan(customers) [Est: 5 rows, cost 5]
        Project(orders.customer_id) [Est: 5 rows, cost 10]
          Scan(orders) [Est: 5 rows, cost 5]
    ```

### 3. Cost-Based Join Input Swap
Reorders join children so that the smaller relation is placed on the build (left) side of the hash join for optimal performance.
*   **Query**:
    ```sql
    EXPLAIN SELECT customers.name, orders.total FROM orders, customers WHERE customers.id = orders.customer_id AND customers.country = 'PK'
    ```
*   **Optimized Plan** (The smaller filtered `customers` relation is swapped to the left side of the `HashJoin`):
    ```text
    Project(customers.name, orders.total) [Est: 2 rows, cost 30]
      HashJoin(orders.customer_id = customers.id) [Est: 2 rows, cost 28]
        Filter(customers.country = 'PK') [Est: 2 rows, cost 10]
          Scan(customers) [Est: 5 rows, cost 5]
        Scan(orders) [Est: 5 rows, cost 5]
    ```

## 📂 Project Structure
- `src/optimizer.cpp`: Heuristic rules and Cost Model logic.
- `src/executor.cpp`: Materialized execution with row counting.
- `src/catalog.cpp`: Metadata management and statistics loading.
- `src/parser.cpp`: SQL Grammar support.
- `src/planner.cpp`: Logical plan generation.

---
*Developed as part of the Database Systems - Cost Based Query Optimizer Project.*
