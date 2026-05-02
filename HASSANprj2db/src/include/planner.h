#pragma once

#include "plan.h"
#include <memory>
using namespace std;
class Planner {
public:
    shared_ptr<LogicalPlan> build_naive_plan(const Query& query);
};
