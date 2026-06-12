<div align="center">
  
```
██████╗ ██████╗ ███████╗████████╗   ██████╗  █████╗ ███████╗███████╗██████╗ 
██╔════╝██╔═══██╗██╔════╝╚══██╔══╝   ██╔══██╗██╔══██╗██╔════╝██╔════╝██╔══██╗
██║     ██║   ██║███████╗   ██║█████╗██████╔╝███████║███████╗█████╗  ██║  ██║
██║     ██║   ██║╚════██║   ██║╚════╝██╔══██╗██╔══██║╚════██║██╔══╝  ██║  ██║
╚██████╗╚██████╔╝███████║   ██║      ██████╔╝██║  ██║███████║███████╗██████╔╝
 ╚═════╝ ╚═════╝ ╚══════╝   ╚═╝      ╚═════╝ ╚═╝  ╚═╝╚══════╝╚══════╝╚═════╝

███████╗ ██████╗ ██╗              ██████╗ ██╗   ██╗███████╗██████╗ ██╗   ██╗
██╔════╝██╔═══██╗██║             ██╔═══██╗██║   ██║██╔════╝██╔══██╗╚██╗ ██╔╝
███████╗██║   ██║██║             ██║   ██║██║   ██║█████╗  ██████╔╝ ╚████╔╝ 
╚════██║██║▄▄ ██║██║             ██║▄▄ ██║██║   ██║██╔══╝  ██╔══██╗  ╚██╔╝  
███████║╚██████╔╝███████╗        ╚██████╔╝╚██████╔╝███████╗██║  ██║   ██║   
╚══════╝ ╚══▀▀═╝ ╚══════╝         ╚══▀▀═╝  ╚═════╝ ╚══════╝╚═╝  ╚═╝   ╚═╝   

 ██████╗ ██████╗ ████████╗██╗███╗   ███╗██╗███████╗███████╗██████╗ 
██╔═══██╗██╔══██╗╚══██╔══╝██║████╗ ████║██║╚══███╔╝██╔════╝██╔══██╗
██║   ██║██████╔╝   ██║   ██║██╔████╔██║██║  ███╔╝ █████╗  ██████╔╝
██║   ██║██╔═══╝    ██║   ██║██║╚██╔╝██║██║ ███╔╝  ██╔══╝  ██╔══██╗
╚██████╔╝██║        ██║   ██║██║ ╚═╝ ██║██║███████╗███████╗██║  ██║
 ╚═════╝ ╚═╝        ╚═╝   ╚═╝╚═╝     ╚═╝╚═╝╚══════╝╚══════╝╚═╝  ╚═╝

 ██████╗██████╗       ███████╗ ██████╗ ██╗      ██████╗ ██████╗ ████████╗
██╔════╝██╔══██╗      ██╔════╝██╔═══██╗██║     ██╔═══██╗██╔══██╗╚══██╔══╝
██║     ██████╔╝█████╗███████╗██║   ██║██║     ██║   ██║██████╔╝   ██║   
██║     ██╔══██╗╚════╝╚════██║██║▄▄ ██║██║     ██║   ██║██╔═══╝    ██║   
╚██████╗██████╔╝      ███████║╚██████╔╝███████╗╚██████╔╝██║        ██║   
 ╚═════╝╚═════╝       ╚══════╝ ╚══▀▀═╝ ╚══════╝ ╚═════╝ ╚═╝        ╚═╝
```
</div>

> **Advanced Database Management — CS B, 4th Semester**
> **Project 02 | Language: C++17**

---

## Group 12

| Name | Roll Number |
|------|-------------|
| Hassan Ali Shah | BSCS24040 |
| Abdul Moeed | BSCS24140 |
| Ahsen Ali | BSCS24056 |

---

## Table of Contents

- [Overview](#overview)
- [What This Project Does](#what-this-project-does)
- [Architecture](#architecture)
- [Features](#features)
- [Prerequisites](#prerequisites)
- [Project Structure](#project-structure)
- [Building the Project](#building-the-project)
- [Running the Optimizer](#running-the-optimizer)
- [Supported SQL Subset](#supported-sql-subset)
- [Shell Commands](#shell-commands)
- [Benchmark](#benchmark)
- [Implementation Phases](#implementation-phases)
- [Optimization Components](#optimization-components)
- [Example Session](#example-session)
- [Known Limitations](#known-limitations)
- [Required Reading](#required-reading)

---

## Overview

`CB-SQLopt` is a cost-based SQL query optimizer built from scratch in **C++17**. It accepts a small but realistic SQL subset, maintains per-column catalog statistics, applies four rewrite rules, and uses the **Selinger dynamic programming algorithm** (the same algorithm at the core of System R, PostgreSQL, and DB2 since 1979) to choose the cheapest join order.

The optimizer makes a visible, measurable difference: on adversarial multi-table queries it achieves speedups of **100× to 1000×** over the unoptimized baseline by pushing filters early and reordering joins to minimize intermediate result sizes.

---

## What This Project Does

Given a SQL query like:

```sql
SELECT customers.name, SUM(line_items.qty * line_items.price)
FROM customers, orders, line_items
WHERE customers.id = orders.customer_id
  AND orders.id = line_items.order_id
  AND customers.country = 'PK'
  AND orders.year = 2024;
```

**Without the optimizer**, the engine naively follows the `FROM` clause order, producing a 5-billion-row intermediate before filtering — taking ~13 seconds.

**With the optimizer**, it pushes `country = 'PK'` down to the scan, filters customers to ~41 rows, then joins — taking ~0.12 seconds.

**Speedup: 103×.**

---

## Architecture

The system has six strictly separated components:

```
[SQL query string]
        |
        v
  +-----------+
  |  Parser   |  Hand-written recursive-descent parser (parser.cpp)
  +-----------+
        |
        v
  +-----------+
  |  Catalog  |  Table schemas, row counts, per-column statistics (catalog.cpp)
  +-----------+
        |
        v
  +-----------+
  |  Rewriter |  4 rule-based transformations (optimizer.cpp)
  +-----------+
        |
        v
  +-----------+
  | Cost Model|  Cardinality estimation using catalog stats (optimizer.cpp)
  +-----------+
        |
        v
  +-----------+
  | Join-Order|  Selinger DP over subsets of base tables (optimizer.cpp)
  |  Search   |
  +-----------+
        |
        v
  +-----------+
  | Executor  |  Materialized operator model (executor.cpp)
  +-----------+
```

---

## Features

- Hand-written recursive-descent SQL parser — no parser generators (parser.cpp / parser.h)
- Catalog with per-table and per-column statistics: row count, distinct values, min/max (catalog.cpp / catalog.h)
- Statistics cached to `catalog.json` after first load
- **Four rewrite rules** (optimizer.cpp):
  - Predicate pushdown (most impactful)
  - Projection pushdown
  - Constant folding
  - Join input swap
- **Selinger DP join ordering** — bitmap-based, O(n² · 2ⁿ), handles up to 4 tables
- Materialized executor with 7 operators: Scan, Filter, Project, HashJoin, CrossProduct, Limit, GroupBy (executor.cpp)
- Interactive shell with `EXPLAIN`, `\stats`, `\validate`, and benchmark mode
- Side-by-side plan comparison (estimated vs. actual row counts)

---

## Prerequisites

On Ubuntu/Debian:

```bash
sudo apt update
sudo apt install build-essential g++ make cmake
```

Verify your compiler:

```bash
g++ --version   # GCC 9+ with C++17 support recommended
```

No external libraries required except **nlohmann/json** for catalog serialization — automatically fetched via CMake's `FetchContent`.

---

## Project Structure

```
hassan-ali-17-cost_based_query_optimizer/
├── README.md
├── LICENSE
├── phase1/                        # Phase 1: Parser, Catalog, Executor (no optimizer)
│   ├── CMakeLists.txt
│   ├── customers.csv
│   ├── orders.csv
│   ├── line_items.csv
│   ├── products.csv
│   └── src/
│       ├── main.cpp               # Shell entry point
│       ├── parser.cpp             # Hand-written recursive-descent SQL parser
│       ├── catalog.cpp            # Table/column statistics, catalog.json I/O
│       ├── plan.cpp               # Logical plan tree representation
│       ├── planner.cpp            # Naive plan builder (FROM order, cross products)
│       ├── executor.cpp           # Materialized operator model (7 operators)
│       └── include/
│           ├── parser.h
│           ├── catalog.h
│           ├── plan.h
│           ├── planner.h
│           ├── executor.h
│           └── types.h
│
├── phase2/                        # Phase 2: Rule Rewriter + Cost Model
│   ├── CMakeLists.txt
│   ├── customers.csv
│   ├── orders.csv
│   ├── line_items.csv
│   ├── products.csv
│   └── src/
│       ├── main.cpp
│       ├── parser.cpp
│       ├── catalog.cpp
│       ├── plan.cpp
│       ├── planner.cpp
│       ├── executor.cpp
│       ├── optimizer.cpp          # Rule rewriter + cost model (Phase 2)
│       └── include/
│           ├── parser.h
│           ├── catalog.h
│           ├── plan.h
│           ├── planner.h
│           ├── executor.h
│           ├── optimizer.h
│           └── types.h
│
└── phase 3/                       # Phase 3: Selinger DP Join-Order Search (Complete)
    ├── CMakeLists.txt
    ├── customers.csv
    ├── orders.csv
    ├── line_items.csv
    ├── products.csv
    └── src/
        ├── main.cpp               # Shell with EXPLAIN, \validate, \benchmark
        ├── parser.cpp
        ├── catalog.cpp
        ├── plan.cpp
        ├── planner.cpp
        ├── executor.cpp
        ├── optimizer.cpp          # Full optimizer: rules + Selinger DP
        └── include/
            ├── parser.h
            ├── catalog.h
            ├── plan.h
            ├── planner.h
            ├── executor.h
            ├── optimizer.h
            └── types.h
```

---

## Building the Project

### Using CMake (recommended, any phase)

```bash
# Build Phase 3 (complete optimizer)
cd "phase 3"
cmake -S . -B build
cmake --build build
./build/qopt
```

### Using g++ directly

**Linux / WSL:**
```bash
cd "phase 3"
g++ src/*.cpp -I src/include -std=c++17 -o qopt
./qopt
```

**Windows (MinGW/g++):**
```powershell
cd "phase 3"
g++ src/catalog.cpp src/executor.cpp src/main.cpp src/optimizer.cpp src/parser.cpp src/plan.cpp src/planner.cpp -I src/include -std=c++17 -o qopt.exe
.\qopt.exe
```

> **Note:** On Windows, PowerShell does not auto-expand wildcards for g++, so list source files individually as shown above.

---

## Running the Optimizer

```bash
# Start the optimizer shell
./qopt

# Or load data directory at launch
./qopt --data .
```

You will see the `qopt>` prompt. Load the CSV data:

```
qopt> LOAD .
```

The shell will scan the CSV files, compute statistics, and cache them to `catalog.json`.

---

## Supported SQL Subset

```sql
SELECT select_list
FROM table [, table ...]
[WHERE pred [AND pred ...]]
[GROUP BY column]
[LIMIT integer]
```

**Select list:** `*`, columns, aggregates (`SUM`, `COUNT`, `AVG`, `MIN`, `MAX`)

**Predicates:**
- `column = literal` — equality filter
- `column < literal` — range filter (also `<=`, `>`, `>=`, `!=`)
- `column = column` — join condition

**Not supported:** `OR`, `ORDER BY`, `HAVING`, `DISTINCT`, `OUTER JOIN`, subqueries, `SUM(col * col)` expressions

---

## Shell Commands

| Command | Description |
|---------|-------------|
| `LOAD <dir>` | Load catalog statistics from CSV files in `<dir>` |
| `SELECT ...` | Run a query through the full optimizer pipeline |
| `EXPLAIN SELECT ...` | Show the chosen plan with estimated costs and cardinalities |
| `\validate <sql>` | Execute and compare estimated vs. actual row counts per operator |
| `\stats` | Print catalog statistics (table/column info) |
| `\benchmark` | Run the 5 required benchmark queries against all 4 optimizer configurations |
| `exit` / `quit` | Exit the shell |

---

## Benchmark

The `\benchmark` command runs 5 queries against 4 optimizer configurations:

| Config | Description |
|--------|-------------|
| `no-opt` | No optimization — naive FROM-order cross products |
| `rules-only` | Predicate/projection pushdown + constant folding |
| `dp-only` | Join ordering via Selinger DP without rewrite rules |
| `full` | Rules + Selinger DP (the complete optimizer) |

### Benchmark Queries

**Q1** — Two-table, selective filter:
```sql
SELECT * FROM customers, orders
WHERE customers.id = orders.customer_id
  AND customers.country = 'PK';
```

**Q2** — Three-table, selective:
```sql
SELECT customers.name, SUM(line_items.qty * line_items.price)
FROM customers, orders, line_items
WHERE customers.id = orders.customer_id
  AND orders.id = line_items.order_id
  AND customers.country = 'PK'
  AND orders.year = 2024;
```

**Q3** — Four-table, highly selective:
```sql
SELECT customers.name, products.name
FROM customers, orders, line_items, products
WHERE customers.id = orders.customer_id
  AND orders.id = line_items.order_id
  AND line_items.product_id = products.id
  AND customers.country = 'PK'
  AND products.category = 'Electronics';
```

**Q4** — Aggregation with GROUP BY:
```sql
SELECT customers.country, SUM(orders.total)
FROM customers, orders
WHERE customers.id = orders.customer_id
  AND orders.year = 2024
GROUP BY customers.country;
```

**Q5** — Adversarial (selective filters on the larger side):
```sql
SELECT customers.name FROM customers, orders
WHERE customers.id = orders.customer_id
  AND orders.total > 1000
  AND customers.age > 30;
```

---

## Implementation Phases

### Phase 1 — Parser, Catalog, and Executor (`phase1/`)

End-to-end query pipeline with no optimizer. Queries parse into a naive plan (FROM order, cross products on top), execute correctly, and produce results. The catalog loads statistics but does not yet use them for planning.

Key files: `parser.cpp`, `catalog.cpp`, `planner.cpp`, `executor.cpp`

### Phase 2 — Rule Rewriter and Cost Model (`phase2/`)

Adds the `optimizer.cpp` module with four rewrite rules applied in a fixed-point loop, plus the cost model using catalog statistics for cardinality estimation. `EXPLAIN` now shows estimated costs and row counts per operator.

Key additions: `optimizer.cpp` / `optimizer.h`

### Phase 3 — Join-Order Search and Benchmark (`phase 3/`)

Adds the Selinger DP algorithm to `optimizer.cpp`. After rule rewriting, the optimizer extracts base tables and join conditions, runs DP over all bitmask subsets to find the cheapest left-deep join tree, and replaces the plan. The `\benchmark` and `\validate` commands are also added in this phase.

---

## Optimization Components

### Predicate Pushdown
Moves `Filter` nodes below `Join`/`CrossProduct` nodes so filters are applied as close to the scan as possible. Single-table conjuncts are pushed down; multi-table conjuncts become join conditions. This also converts `CrossProduct` nodes into `HashJoin` nodes when an equijoin predicate connects both sides.

### Projection Pushdown
Propagates only the columns needed downstream (SELECT list, predicates, join keys) through the plan tree, inserting `Project` nodes above `Scan` nodes to prune unused columns early.

### Constant Folding
Evaluates predicates where both sides are literals at planning time (`1 = 1` → remove filter; `1 = 2` → return empty). Runs before predicate pushdown so that trivial branches can be pruned.

### Join Input Swap
After cost estimation, swaps the left and right children of a `HashJoin` if the right side has a smaller estimated cardinality, ensuring the smaller relation is used as the hash build side.

### Selinger DP (Phase 3)
Enumerates all bitmask subsets of base tables in increasing size order. For each subset, tries every way to split it into a left sub-plan and a single right table, picks the split with the lowest estimated cost, and records the best plan. Cross-product splits are skipped unless no join condition exists. The full-subset entry is the final optimal plan.

**Cost formulas:**

| Operator | Cost |
|----------|------|
| `Scan(t)` | `t.row_count` |
| `Filter(pred, child)` | `child.cost + child.cardinality` |
| `HashJoin(cond, L, R)` | `L.cost + R.cost + 2×L.card + R.card + output.card` |
| `CrossProduct(L, R)` | `L.cost + R.cost + L.card × R.card` |
| `Project / Limit / GroupBy` | `child.cost + child.cardinality` |

**Cardinality formulas:**

| Operator | Estimate |
|----------|---------|
| `Scan(t)` | `t.row_count` |
| `Filter(col = lit)` | `child.card / distinct_count(col)` |
| `Filter(col < lit)` | `child.card × (lit - min) / (max - min)` |
| `HashJoin(col1 = col2)` | `L.card × R.card / max(distinct(col1), distinct(col2))` |

---

## Example Session

```
$ ./qopt --data .

qopt> EXPLAIN SELECT customers.name FROM customers, orders
      WHERE customers.id = orders.customer_id AND customers.country = 'PK'

optimized plan:
  Project(customers.name) [Est: 2 rows, cost 30]
    HashJoin(customers.id = orders.customer_id) [Est: 2 rows, cost 28]
      Filter(customers.country = 'PK') [Est: 2 rows, cost 10]
        Scan(customers) [Est: 5 rows, cost 5]
      Scan(orders) [Est: 5 rows, cost 5]

qopt> \validate SELECT customers.name FROM customers, orders
      WHERE customers.id = orders.customer_id AND customers.country = 'PK'

validation plan (Estimated vs Actual):
  Project(customers.name) [Est: 2 rows, cost 30] [Actual: 3 rows]
    HashJoin(customers.id = orders.customer_id) [Est: 2 rows] [Actual: 3 rows]
      Filter(customers.country = 'PK') [Est: 2 rows] [Actual: 3 rows]
        Scan(customers) [Est: 5 rows] [Actual: 5 rows]
      Scan(orders) [Est: 5 rows] [Actual: 5 rows]
```

---

## Known Limitations

- Maximum 4 tables per query in the base DP (the search is O(n² · 2ⁿ))
- No `OR` predicates, no subqueries, no `ORDER BY`
- No `SUM(col * col)` expressions in the select list (parser limitation)
- Independence assumption for multi-column selectivity (no cross-column correlations)
- Left-deep join trees only
- No per-column histograms — min/max range estimate used for range predicates
- No indexes — all scans are sequential
- No transactions, no concurrent queries, no DML (INSERT / UPDATE / DELETE)

---

## Required Reading

1. Selinger et al. "Access Path Selection in a Relational Database Management System." ACM SIGMOD, 1979.
2. Ramakrishnan & Gehrke. *Database Management Systems*, 3rd ed., Chapters 12 and 14.
3. Graefe. "Query Evaluation Techniques for Large Databases." ACM Computing Surveys, 1993.
4. Leis et al. "How Good Are Query Optimizers, Really?" VLDB, 2015.
5. Pavlo. CMU 15-445 Database Systems, Lectures 12–16.

---

## Submission

**Archive name:** `Group12_Project02_Optimizer.zip`

**Contents:**
- Complete source tree (all three phases)
- `CMakeLists.txt` per phase
- `README.md`
- `design.pdf` (max 13 pages)
