#ifndef JOINORDER_HPP
#define JOINORDER_HPP

#include "catalog.hpp"
#include "plan.hpp"

std::shared_ptr<PlanNode> buildDpJoinPlan(const Query& q, const Catalog& catalog);

#endif
