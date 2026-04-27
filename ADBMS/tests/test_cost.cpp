#include <cassert>
#include <iostream>

#include "catalog.hpp"
#include "cost.hpp"
#include "plan.hpp"

int main() {
    Catalog c = makeDefaultCatalog();

    Query q;
    q.selectRaw = "*";
    q.tables = {"customers"};

    Predicate p;
    p.left = "customers.country";
    p.right = "PK";
    p.op = PredOp::EQ;
    p.isJoin = false;
    q.predicates.push_back(p);

    auto root = buildNaivePlan(q);
    assert(root != nullptr);

    estimatePlan(root, c);
    assert(root->estCardinality > 0);
    assert(root->estCost > 0);

    std::cout << "test_cost: ok\n";
    return 0;
}
