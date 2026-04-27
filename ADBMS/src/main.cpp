#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "catalog.hpp"
#include "cost.hpp"
#include "executor.hpp"
#include "joinorder.hpp"
#include "parser.hpp"
#include "rewriter.hpp"

namespace {
struct SessionStats {
    int queriesExecuted = 0;
    int optimizerWins = 0;
    double totalSpeedup = 0.0;
    double totalPlanMs = 0.0;
    double totalExecMs = 0.0;
};

enum class OptMode {
    None,
    RulesOnly,
    DpOnly,
    Full
};

struct AppState {
    Database db;
    Catalog catalog;
    SessionStats stats;
    bool ready = false;
    std::string dataDir = "./benchdata";
};

bool startsWithCi(const std::string& s, const std::string& pfx) {
    if (s.size() < pfx.size()) {
        return false;
    }
    for (size_t i = 0; i < pfx.size(); ++i) {
        const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
        const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(pfx[i])));
        if (a != b) {
            return false;
        }
    }
    return true;
}

std::shared_ptr<PlanNode> buildPlan(const Query& q, const Catalog& catalog, OptMode mode) {
    if (mode == OptMode::None) {
        return buildNaivePlan(q);
    }

    if (mode == OptMode::RulesOnly) {
        return rewritePlan(buildNaivePlan(q));
    }

    if (mode == OptMode::DpOnly) {
        return buildDpJoinPlan(q, catalog);
    }

    return rewritePlan(buildDpJoinPlan(q, catalog));
}

bool loadData(AppState& state, const std::string& path) {
    std::string error;
    Database db;
    if (!loadDatabaseFromCsv(path, db, error)) {
        std::cout << "load error: " << error << "\n";
        return false;
    }

    Catalog catalog;
    std::string cacheError;
    if (!loadOrCreateCatalogCache(db, catalog, cacheError)) {
        std::cout << "catalog warning: " << cacheError << "\n";
    }

    state.db = std::move(db);
    state.catalog = std::move(catalog);
    state.ready = true;
    state.dataDir = path;

    std::cout << "qopt: opened catalog with " << state.catalog.tables.size() << " tables\n";
    return true;
}

ExecResult runQuery(const std::string& sql, const AppState& state, bool explain, OptMode mode) {
    Query q;
    std::string error;
    if (!parseSql(sql, q, error)) {
        std::cout << "parse error: " << error << "\n";
        return ExecResult{};
    }

    auto plan = buildPlan(q, state.catalog, mode);
    if (!plan) {
        std::cout << "failed to build plan\n";
        return ExecResult{};
    }

    estimatePlan(plan, state.catalog);
    const ExecResult r = executePlan(plan, state.db);

    if (explain) {
        printPlan(plan, 0);
    } else {
        std::cout << "rows=" << r.resultRows << " time=" << r.runtimeMs << " ms est_cost=" << plan->estCost << "\n";
    }

    return r;
}

void printStats(const SessionStats& s) {
    std::cout << "queries executed: " << s.queriesExecuted << "\n";
    std::cout << "optimizer wins:   " << s.optimizerWins << "\n";
    if (s.optimizerWins > 0) {
        std::cout << "average speedup:  " << (s.totalSpeedup / s.optimizerWins) << "x\n";
    } else {
        std::cout << "average speedup:  0x\n";
    }
    if (s.queriesExecuted > 0) {
        std::cout << "avg plan time:    " << (s.totalPlanMs / s.queriesExecuted) << " ms\n";
        std::cout << "avg exec time:    " << (s.totalExecMs / s.queriesExecuted) << " ms\n";
    }
}

}

int main(int argc, char** argv) {
    AppState state;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--data" && i + 1 < argc) {
            state.dataDir = argv[++i];
        }
    }

    loadData(state, state.dataDir);

    std::string line;
    while (true) {
        std::cout << "qopt> ";
        if (!std::getline(std::cin, line)) {
            break;
        }

        if (line == "\\quit" || line == "quit" || line == "exit") {
            break;
        }

        if (line == "\\catalog") {
            if (!state.ready) {
                std::cout << "no data loaded\n";
                continue;
            }
            printCatalog(state.catalog);
            continue;
        }

        if (line == "\\stats") {
            printStats(state.stats);
            continue;
        }

        if (startsWithCi(line, "LOAD ")) {
            const std::string path = line.substr(5);
            loadData(state, path);
            continue;
        }

        if (!state.ready) {
            std::cout << "no data loaded; use LOAD <path>\n";
            continue;
        }

        if (startsWithCi(line, "EXPLAIN NOOPT ")) {
            runQuery(line.substr(14), state, true, OptMode::None);
            continue;
        }

        if (startsWithCi(line, "EXPLAIN ")) {
            runQuery(line.substr(8), state, true, OptMode::None);
            continue;
        }

        if (startsWithCi(line, "QUERY ")) {
            const ExecResult r = runQuery(line.substr(6), state, false, OptMode::None);
            state.stats.queriesExecuted += 1;
            state.stats.totalPlanMs += 1.0;
            state.stats.totalExecMs += r.runtimeMs;
            continue;
        }

        if (startsWithCi(line, "SELECT ")) {
            const ExecResult r = runQuery(line, state, false, OptMode::None);
            state.stats.queriesExecuted += 1;
            state.stats.totalPlanMs += 1.0;
            state.stats.totalExecMs += r.runtimeMs;
            continue;
        }

        if (startsWithCi(line, "BENCH run")) {
            std::cout << "BENCH run is enabled in Phase 3; current build is focused on Phase 1 requirements.\n";
            continue;
        }

        std::cout << "unknown command\n";
    }

    return 0;
}
