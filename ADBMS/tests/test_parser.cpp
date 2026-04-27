#include <cassert>
#include <iostream>

#include "parser.hpp"

int main() {
    Query q;
    std::string error;
    const bool ok = parseSql(
        "SELECT customers.name FROM customers, orders WHERE customers.id = orders.customer_id AND orders.year = 2024 LIMIT 10;",
        q,
        error);
    assert(ok);
    assert(q.tables.size() == 2);
    assert(q.predicates.size() == 2);
    assert(q.hasLimit && q.limit == 10);
    std::cout << "test_parser: ok\n";
    return 0;
}
