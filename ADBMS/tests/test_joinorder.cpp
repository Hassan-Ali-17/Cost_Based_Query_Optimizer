#include <cassert>
#include <iostream>

#include "catalog.hpp"
#include "joinorder.hpp"

int main() {
    Catalog c = makeDefaultCatalog();

    Query q;
    q.selectRaw = "*";
    q.tables = {"customers", "orders"};

    Predicate p;
    p.left = "customers.id";
    p.right = "orders.customer_id";
    p.op = PredOp::EQ;
    p.isJoin = true;
    q.predicates.push_back(p);

    auto plan = buildDpJoinPlan(q, c);
    assert(plan != nullptr);

    std::cout << "test_joinorder: ok\n";
    return 0;
}
