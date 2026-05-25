# Cost-Based Query Optimizer (Phase 2)

This is an implementation of a Cost-Based Query Optimizer (CBO). 
The project is currently in **Phase 2**, which adds an intelligent **Rule-Based Rewriter** and a **Cost Model** to the execution pipeline.

## Build Instructions

### On Windows (with MinGW/g++)
Ensure `g++` is in your PATH, and run:
```powershell
g++ src/*.cpp -I src/include -std=c++17 -o qopt.exe
.\qopt.exe
```

### On Linux (Ubuntu LTS) / WSL
```bash
g++ src/*.cpp -I src/include -std=c++17 -o qopt
./qopt
```

## How to Run

1.  **Launch the shell**: Run `.\qopt.exe`.
2.  **Load Data**: When the `qopt>` shell opens, you must load the database statistics from the directory containing your CSVs (e.g., `customers.csv`, `orders.csv`):
    ```sql
    LOAD .
    ```
3.  **Optimize & Execute**:
    - Use `EXPLAIN <sql>` to see the **optimized** logical plan, including estimated row counts and costs.
    - Use `QUERY <sql>` to execute the query and see the results.

## Codebase Walkthrough

### 1. `optimizer.h` / `optimizer.cpp` [NEW in Phase 2]
The brain of the system. It transforms the naive plan into an efficient one.
- **RuleRewriter**: Applies heuristic rules like **Predicate Pushdown**, **Projection Pushdown**, and **Constant Folding** in a fixed-point loop.
- **CostModel**: Uses Catalog statistics to estimate the cardinality (row count) of each operator.
- **Join Input Swap**: Swaps the left and right children of a join so that the smaller table is used for the hash build side.

### 2. `executor.h` / `executor.cpp`
The Materialized Execution Engine.
- **Instrumentation**: Now captures **actual** row counts during execution, allowing you to compare the optimizer's estimates against reality in the `EXPLAIN` output.
- Supports `Scan`, `Filter`, `HashJoin`, `Project`, `GroupBy` (with `SUM`, `COUNT`, `AVG`, `MIN`, `MAX`), and `Limit`.

### 3. `types.h`
Defines the `Value` variant and `Row` structures. Now supports arithmetic and hashing to facilitate grouping and cost calculations.

### 4. `catalog.h` / `catalog.cpp`
Manages metadata and computes base statistics (Distinct counts, Min/Max values) from CSV files. These stats are critical for the Cost Model's accuracy.

### 5. `parser.h` / `parser.cpp`
A hand-written recursive-descent SQL parser that supports the grammar required for complex joins and aggregates.

### 6. `planner.h` / `planner.cpp`
Generates a naive, unoptimized plan (left-deep cross-products) that is then handed off to the Optimizer.

## Optimization Rules Implemented
1.  **Predicate Pushdown**: Moves filters past joins to reduce the number of rows processed as early as possible.
2.  **Projection Pushdown**: Prunes unused columns from the data flow.
3.  **Constant Folding**: Evaluates literal expressions (e.g., `WHERE 1=1`) at compile time.
4.  **Join Input Swap**: Reorders joins to put the smaller table on the left (build) side.

## Supported Queries
- `SELECT`, `FROM`, `WHERE`, `GROUP BY`, `LIMIT`.
- Equijoins (e.g., `a.id = b.id`).
- All 5 standard aggregates.
- Logical comparisons.

---
*Next Step: Phase 3 — Join Order Search (Dynamic Programming).*
