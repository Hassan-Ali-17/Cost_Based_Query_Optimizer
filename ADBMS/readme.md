# Cost-Based SQL Query Optimizer (`CB-SQLopt`)

> **Advanced Database Management — CS B, 4th Semester**
> **Project 02 | Language: C / C++**

---

## Group 12


| Name            | RoleNumber |
| --------------- | ---------- |
| Hassan Ali Shah | BSCS24040  |
| Abdul Moeed     | BSCS24140  |
| Ahsen Ali       | BSCS24056  |

---

## Table of Contents

- [Overview](#overview)
- [What This Project Does](#what-this-project-does)
- [Architecture](#architecture)
- [Features](#features)
- [Prerequisites](#prerequisites)
- [Building the Project](#building-the-project)
- [Running the Optimizer](#running-the-optimizer)
- [Supported SQL Subset](#supported-sql-subset)
- [Shell Commands](#shell-commands)
- [Benchmark](#benchmark)
- [Project Structure](#project-structure)
- [Implementation Phases](#implementation-phases)
- [Optimization Components](#optimization-components)
- [Example Session](#example-session)
- [Running Tests](#running-tests)
- [Known Limitations](#known-limitations)
- [Required Reading](#required-reading)

---

## Overview

`CB-SQLopt` is a cost-based SQL query optimizer built from scratch in C/C++. It accepts a small but realistic SQL subset, maintains per-column catalog statistics, applies four rewrite rules, and uses the **Selinger dynamic programming algorithm** (the same algorithm at the core of System R, PostgreSQL, and DB2 since 1979) to choose the cheapest join order.

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
  |  Parser   |  Hand-written recursive-descent, ~250 lines of C
  +-----------+
        |
        v
  +-----------+
  |  Catalog  |  Table schemas, row counts, per-column statistics
  +-----------+
        |
        v
  +-----------+
  |  Rewriter |  4 rule-based transformations (predicate pushdown, etc.)
  +-----------+
        |
        v
  +-----------+
  | Cost Model|  Cardinality estimation using catalog stats
  +-----------+
        |
        v
  +-----------+
  | Join-Order|  Selinger DP over subsets of base tables
  |  Search   |
  +-----------+
        |
        v
  +-----------+
  | Executor  |  Materialized operator model (no Volcano iterators)
  +-----------+
```

---

## Features

- Hand-written recursive-descent SQL parser (no parser generators)
- Catalog with per-table and per-column statistics (row count, distinct values, min/max)
- Statistics cached to `catalog.json` after first load
- **Four rewrite rules:**
  - Predicate pushdown (most impactful)
  - Projection pushdown
  - Constant folding
  - Join input swap
- **Selinger DP join ordering** — bitmap-based, O(n² · 2ⁿ), handles up to 4 tables (8 with bonus)
- Materialized executor with 7 operators: Scan, Filter, Project, HashJoin, CrossProduct, Limit, GroupBy
- Interactive shell with `EXPLAIN`, `\stats`, and benchmark mode
- Side-by-side plan comparison (optimized vs unoptimized)

---

## Prerequisites

On Ubuntu/Debian:

```bash
sudo apt update
sudo apt install build-essential gcc g++ make
```

No external libraries required. Everything is implemented from scratch including the parser, catalog, cost model, and executor.

Verify your compiler:

```bash
gcc --version   # GCC 9+ recommended
g++ --version
```

---

## Building the Project

```bash
# Clone the repository
git clone https://github.com/Hassan-Ali-17/Cost_Based_Query_Optimizer.git
cd Group12_Project02_Optimizer

# Build everything (optimizer + data generator + benchmark driver)
make

# Clean build artifacts
make clean

# Build and run tests
make test
```

The `make` command produces three binaries:


| Binary      | Purpose                         |
| ----------- | ------------------------------- |
| `CB-SQLopt` | The main optimizer shell        |
| `datagen`   | Benchmark dataset CSV generator |
| `bench`     | Automated benchmark driver      |

---

## Running the Optimizer

```bash
# Generate the benchmark dataset first (required for BENCH)
./datagen --seed 42 --out ./benchdata

# Start the optimizer shell
./qopt --data ./benchdata
```

You will see:

```
qopt: opened catalog with 4 tables
  customers   10,000 rows  (id INT PK, name TEXT, country TEXT, age INT)
  orders     500,000 rows  (id INT PK, customer_id INT, total DOUBLE, year INT, status TEXT)
  line_items 2,000,000 rows (order_id INT, product_id INT, qty INT, price DOUBLE)
  products    50,000 rows  (id INT PK, name TEXT, category TEXT, supplier_id INT)
qopt: stats loaded for 4 tables, 16 columns
qopt>
```

---

## Supported SQL Subset

```sql
SELECT select_list
FROM table [, table ...]
[WHERE pred [AND pred ...]]
[GROUP BY column]
[LIMIT integer]
```

**Select list:** `*`, columns, aggregates (`SUM`, `COUNT`, `AVG`, `MIN`, `MAX`), expressions (`qty * price`)

**Predicates:**

- `column = literal` — equality filter
- `column < literal` — range filter (also `<=`, `>`, `>=`, `!=`)
- `column = column` — join condition

**Not supported:** `OR`, `ORDER BY`, `HAVING`, `DISTINCT`, `OUTER JOIN`, subqueries

---

## Shell Commands


| Command                    | Description                                                                 |
| -------------------------- | --------------------------------------------------------------------------- |
| `SELECT ...`               | Run a query through the full optimizer pipeline                             |
| `EXPLAIN SELECT ...`       | Show the chosen plan with estimated costs and cardinalities                 |
| `EXPLAIN NOOPT SELECT ...` | Show the unoptimized naive plan for comparison                              |
| `\stats`                   | Print session statistics (queries run, average speedup, plan time)          |
| `\catalog`                 | Print the full catalog with all column statistics                           |
| `BENCH run`                | Run the 5 required benchmark queries against all 4 optimizer configurations |
| `\quit`                    | Exit the shell                                                              |

---

## Benchmark

The benchmark runs 5 queries against 4 optimizer configurations:


| Config     | Description                                        |
| ---------- | -------------------------------------------------- |
| None       | No optimization — naive FROM-order cross products |
| Rules only | Predicate/projection pushdown + constant folding   |
| DP only    | Join ordering without rewrite rules                |
| Full       | Rules + Selinger DP (the complete optimizer)       |

### Benchmark Queries

**Q1** — Two-table, selective filter:

```sql
SELECT * FROM customers, orders
WHERE customers.id = orders.customer_id
  AND customers.country = 'PK';
```

**Q2** — Three-table, selective (the example from the session above):

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
-- Constructed to trip up naive optimizers
-- Full optimizer still wins
```

Run the benchmark:

```bash
./qopt --data ./benchdata
qopt> BENCH run
```

Expected output is a 5×4 table of speedups. Full optimizer should achieve at least **100× speedup** on Q2 and Q3.

---

## Project Structure

```
Group12_Project02_Optimizer/
├── Makefile
├── README.md
├── src/
│   ├── main.c            # Shell entry point
│   ├── parser.c          # Hand-written recursive-descent SQL parser
│   ├── parser.h
│   ├── catalog.c         # Table/column statistics, catalog.json I/O
│   ├── catalog.h
│   ├── plan.c            # Logical plan tree representation
│   ├── plan.h
│   ├── rewriter.c        # Four rewrite rules (fixed-point loop)
│   ├── rewriter.h
│   ├── cost.c            # Cardinality estimation + cost function
│   ├── cost.h
│   ├── joinorder.c       # Selinger DP over bitmap subsets
│   ├── joinorder.h
│   ├── executor.c        # Materialized operator model (7 operators)
│   └── executor.h
├── datagen/
│   └── datagen.c         # Benchmark CSV generator (fixed seed)
├── benchmark/
│   ├── bench.c           # Automated benchmark driver
│   ├── run_bench.sh      # Shell script to run full benchmark
│   └── results/          # Final benchmark output (populated after run)
├── tests/
│   ├── test_parser.c     # Parser unit tests
│   ├── test_rewriter.c   # Each rewrite rule in isolation
│   ├── test_cost.c       # Cardinality estimator vs hand-computed values
│   ├── test_joinorder.c  # DP on 3-table example with known optimal plan
│   └── test_e2e.sh       # End-to-end query correctness tests
├── benchdata/            # Generated benchmark CSVs (after running datagen)
│   ├── customers.csv
│   ├── orders.csv
│   ├── line_items.csv
│   └── products.csv
└── design.pdf            # Architecture and design document (max 13 pages)
```

---

## Implementation Phases

### Phase 1 — Parser, Catalog, and Executor

**Goal:** End-to-end query pipeline with no optimizer.

- Hand-written SQL parser for the supported grammar
- Logical plan tree (`Scan`, `Filter`, `Join`, `Project`, `GroupBy`, `Limit`)
- Catalog loaded from CSV files at startup, cached to `catalog.json`
- Materialized executor with all 7 operators
- Naive plan builder (FROM order, all predicates on top)
- `qopt` shell with `LOAD`, `QUERY`, `EXPLAIN`, `\stats`

**Success criterion:** Every benchmark query runs and produces correct results.

---

### Phase 2 — Rule Rewriter and Cost Model

**Goal:** Add rewrite rules and cardinality estimation.

**Four rewrite rules (applied in this order):**

1. **Constant folding** — `2024 = 2024` → `TRUE`, `1 > 2` → prune branch
2. **Predicate pushdown** — move filters below joins, as close to scans as possible
3. **Projection pushdown** — carry only needed columns through the plan
4. Repeat 1–3 until fixed point, then **join input swap** after DP

**Cost model (System R formulas):**


| Operator                | Cardinality estimate                                     |
| ----------------------- | -------------------------------------------------------- |
| `Scan(t)`               | `t.row_count`                                            |
| `Filter(pred, child)`   | `child.card × selectivity(pred)`                        |
| `Join(col1=col2, L, R)` | `L.card × R.card / max(distinct(col1), distinct(col2))` |

**Selectivity formulas:**

- `col = literal` → `1 / distinct_count(col)`
- `col < literal` → `(literal - min) / (max - min)` clamped to [0,1]
- Multiple AND predicates → multiply selectivities (independence assumption)

---

### Phase 3 — Join-Order Search and Benchmark

**Goal:** Selinger DP and the full benchmark.

**Selinger DP algorithm:**

```
For each subset S of base tables (in size order, bottom-up):
  For each way to split S into L + {t} (left plan + single right table):
    If no join condition exists between L and t: skip (avoid cross products)
    cost = cost(plan(L)) + cost(HashJoin(condition, plan(L), Scan(t)))
    If cost < best[S]: best[S] = cost, record split
Answer = best[{t1, t2, ..., tn}]
```

Subsets represented as bitmaps. With n ≤ 4 tables: 2⁴ = 16 subsets, essentially instantaneous.

**Cost function:**


| Operator               | Cost                                                      |
| ---------------------- | --------------------------------------------------------- |
| `Scan(t)`              | `t.row_count`                                             |
| `Filter(pred, child)`  | `child.cost + child.card`                                 |
| `HashJoin(cond, L, R)` | `L.cost + R.cost + 2×L.card + R.card + out.card`         |
| `CrossProduct(L, R)`   | `L.cost + R.cost + L.card × R.card` (intentionally huge) |

---

## Example Session

```
$ ./qopt --data ./benchdata

qopt> SELECT customers.name, SUM(line_items.qty * line_items.price)
      FROM customers, orders, line_items
      WHERE customers.id = orders.customer_id
        AND orders.id = line_items.order_id
        AND customers.country = 'PK'
        AND orders.year = 2024;

--- WITHOUT optimizer (left-deep, FROM order, no rewrites) ---
estimated cost:  5,247,180,200
actual time:     12.83 seconds
result rows:     412

--- WITH optimizer (rules + cost-based DP join ordering) ---
estimated cost:  2,840,120
actual time:     0.124 seconds
result rows:     412

speedup: 103.5x   |   plan cost ratio: 1847x
estimate error:  customers 5%, orders 0.4%, joined 7%

qopt> \stats
queries executed:    1
optimizer wins:      1  (avg speedup 103.5x)
average plan time:   2.1 ms
average exec time:   124 ms
```

---

## Running Tests

```bash
# Run all unit tests
make test

# Run individual test suites
./tests/test_parser
./tests/test_rewriter
./tests/test_cost
./tests/test_joinorder

# Run end-to-end correctness tests
bash tests/test_e2e.sh

# Run the full benchmark (requires benchdata/)
./datagen --seed 42 --out ./benchdata
bash benchmark/run_bench.sh
```

---

## Known Limitations

- Maximum 4 tables per query (8 with bushy-tree bonus)
- No `OR` predicates, no subqueries, no `ORDER BY`
- Independence assumption for multi-column selectivity (no cross-column correlations)
- Left-deep join trees only in base implementation
- No per-column histograms (min–max range estimate used for range predicates)
- No indexes — all scans are sequential
- No transactions, no concurrent queries, no updates or deletes

---

## Required Reading

1. Selinger et al. "Access Path Selection in a Relational Database Management System." ACM SIGMOD, 1979. *(The original paper — required before Phase 3)*
2. Ramakrishnan & Gehrke. *Database Management Systems*, 3rd ed., Chapters 12 and 14.
3. Graefe. "Query Evaluation Techniques for Large Databases." ACM Computing Surveys, 1993.
4. Leis et al. "How Good Are Query Optimizers, Really?" VLDB, 2015.
5. Pavlo. CMU 15-445 Database Systems, Lectures 12–16. *(Freely available online)*

---

## Submission

**Archive name:** `Group12_Project02_Optimizer.zip`

**Contents:**

- Complete source tree
- `Makefile`
- `README.md`
- `design.pdf` (max 13 pages)
- `benchmark/` directory with results
- `tests/` directory with unit tests

---
