#ifndef REWRITER_HPP
#define REWRITER_HPP

#include "plan.hpp"

std::shared_ptr<PlanNode> rewritePlan(const std::shared_ptr<PlanNode>& root);

#endif
