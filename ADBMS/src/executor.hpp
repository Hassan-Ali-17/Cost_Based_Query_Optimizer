#ifndef EXECUTOR_HPP
#define EXECUTOR_HPP

#include <string>
#include <unordered_map>
#include <vector>

#include "catalog.hpp"
#include "plan.hpp"

using ExecRow = std::unordered_map<std::string, std::string>;

struct ExecResult {
    double runtimeMs = 0.0;
    long long resultRows = 0;
    std::vector<ExecRow> rows;
};

ExecResult executePlan(const std::shared_ptr<PlanNode>& plan, const Database& db);

#endif
