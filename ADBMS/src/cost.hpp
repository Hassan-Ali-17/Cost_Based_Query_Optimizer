#ifndef COST_HPP
#define COST_HPP

#include "catalog.hpp"
#include "plan.hpp"

double estimateSelectivity(const Predicate& pred, const Catalog& catalog);
void estimatePlan(const std::shared_ptr<PlanNode>& node, const Catalog& catalog);

#endif
