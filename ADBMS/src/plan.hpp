#ifndef PLAN_HPP
#define PLAN_HPP

#include <memory>
#include <string>
#include <vector>

enum class PredOp {
    EQ,
    NE,
    LT,
    LE,
    GT,
    GE
};

struct Predicate {
    std::string left;
    std::string right;
    PredOp op = PredOp::EQ;
    bool isJoin = false;
};

struct Query {
    std::string selectRaw;
    std::vector<std::string> tables;
    std::vector<Predicate> predicates;
    std::string groupBy;
    bool hasGroupBy = false;
    int limit = 0;
    bool hasLimit = false;
};

enum class PlanType {
    Scan,
    Filter,
    Project,
    HashJoin,
    CrossProduct,
    GroupBy,
    Limit
};

struct PlanNode {
    PlanType type = PlanType::Scan;
    std::string label;
    Predicate pred;
    double estCardinality = 0.0;
    double estCost = 0.0;
    std::shared_ptr<PlanNode> left;
    std::shared_ptr<PlanNode> right;
};

std::shared_ptr<PlanNode> makeScan(const std::string& table);
std::shared_ptr<PlanNode> makeFilter(const Predicate& pred, std::shared_ptr<PlanNode> child);
std::shared_ptr<PlanNode> makeJoin(const Predicate& pred, std::shared_ptr<PlanNode> left, std::shared_ptr<PlanNode> right);
std::shared_ptr<PlanNode> makeCross(std::shared_ptr<PlanNode> left, std::shared_ptr<PlanNode> right);
std::shared_ptr<PlanNode> makeProject(const std::string& selectList, std::shared_ptr<PlanNode> child);
std::shared_ptr<PlanNode> makeGroupBy(const std::string& col, std::shared_ptr<PlanNode> child);
std::shared_ptr<PlanNode> makeLimit(int limit, std::shared_ptr<PlanNode> child);

std::shared_ptr<PlanNode> buildNaivePlan(const Query& q);
void printPlan(const std::shared_ptr<PlanNode>& node, int depth = 0);

#endif
