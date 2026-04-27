#include <cassert>
#include <iostream>

#include "plan.hpp"
#include "rewriter.hpp"

int main() {
    Query q;
    q.selectRaw = "*";
    q.tables = {"customers"};
    auto root = buildNaivePlan(q);
    assert(root != nullptr);

    auto out = rewritePlan(root);
    assert(out == root);

    std::cout << "test_rewriter: ok\n";
    return 0;
}
