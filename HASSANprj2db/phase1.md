# Cost-Based Query Optimizer

This is an implementation of the Cost-Based Query Optimizer (Project 02).
Currently, the codebase covers **Phase 1**, which includes parsing SQL, building the naive logical plan, loading catalog statistics, and executing the unoptimized queries using a materialized execution model.

## Build Instructions

### On Linux (Ubuntu LTS) / WSL
You can build the project using CMake:
```bash
cmake -S . -B build
cmake --build build
./build/qopt
```

### On Windows (with MinGW/g++)
Ensure `g++` is in your PATH, and run:
```powershell
g++ src/*.cpp -I src/include -std=c++17 -o qopt.exe
.\qopt.exe
```

## How to Run

When the `qopt>` shell opens, you must first load data. If you have the CSV files (`customers.csv`, `orders.csv`, `line_items.csv`, `products.csv`) in the same directory, run:
```sql
LOAD .
```
You can view the loaded statistics using the `\stats` command.

## Codebase Walkthrough

Here is what each file in the `src/` directory is responsible for:

### 1. `types.h`
Defines the core data structures used by the engine. 
- `Value`: A variant that can hold an `INT`, `DOUBLE`, or `TEXT` (string) value.
- `Row`: A simple vector of `Value`s representing a single row of data.
- `TableSchema` & `ColumnSchema`: Defines the names and types of columns for a given table.

### 2. `catalog.h` / `catalog.cpp`
Manages the metadata and physical data for the database.
- `Catalog`: Stores a map of table names to `Table` structs.
- `load_from_csv()`: Opens the CSV files, parses the delimited data into typed `Row`s, and calculates base statistics (row count, min/max values, distinct counts). 
- Currently hardcoded to recognize the 4 tables defined in the project specification.

### 3. `parser.h` / `parser.cpp`
A hand-written recursive-descent SQL parser.
- **Tokenizer**: Scans raw string input and converts it into usable tokens (keywords, identifiers, literals, operators).
- **Parser**: Traverses the tokens and builds an Abstract Syntax Tree (AST) defined by the `Query` struct, capturing the `SELECT` list, `FROM` tables, `WHERE` predicates, `GROUP BY`, and `LIMIT`.

### 4. `plan.h` / `plan.cpp`
Defines the **Logical Plan Tree**.
- `LogicalPlan`: The base class for all operators.
- Contains nodes for `Scan`, `Filter`, `CrossProduct`, `HashJoin`, `Project`, `GroupBy`, and `Limit`.
- Implements `print(indent)` to neatly format the tree for the `EXPLAIN` command.

### 5. `planner.h` / `planner.cpp`
The Naive Plan Builder.
- Takes the parsed `Query` AST and blindly constructs an unoptimized execution tree.
- It iterates through the `FROM` clause and stacks `CrossProduct` operators, then places all `WHERE` clauses on top as `Filter` operators. This results in incredibly slow execution for large datasets—exactly what we will optimize in Phase 2.

### 6. `executor.h` / `executor.cpp`
The Materialized Execution Engine.
- Evaluates the logical plan tree from the bottom up.
- Each `execute_*` function processes input rows and returns a completely materialized `ExecResult` (containing the new `TableSchema` and the `std::vector<Row>`).
- Physically performs nested loops for `CrossProduct`, iterates and evaluates conditions for `Filter`, and truncates rows for `Limit`.

### 7. `main.cpp`
The Entry Point and REPL Shell.
- Wires the Catalog, Parser, Planner, and Executor together.
- Handles standard user commands (`LOAD`, `QUERY`, `EXPLAIN`, `\stats`, `exit`).
- Neatly formats and prints the column headers and row data returned by the Executor.

## Supported Queries & Limitations
- Supports `SELECT`, `FROM`, `WHERE`, `GROUP BY`, and `LIMIT`.
- Supports basic aggregates (`SUM`, `COUNT`, `AVG`, `MIN`, `MAX`) and operators (`=`, `<`, `<=`, `>`, `>=`, `!=`).
- **Limitations**: `OR` conditions, Subqueries, `ORDER BY`, `HAVING`, `DISTINCT`, and `OUTER JOIN` are intentionally out of scope for this project subset. Phase 1 plans are unoptimized left-deep cross-products.
