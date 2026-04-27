#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    int seed = 42;
    std::string out = "./benchdata";

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--seed" && i + 1 < argc) {
            seed = std::stoi(argv[++i]);
        } else if (arg == "--out" && i + 1 < argc) {
            out = argv[++i];
        }
    }

    std::ofstream customers(out + "/customers.csv");
    std::ofstream orders(out + "/orders.csv");
    std::ofstream lineItems(out + "/line_items.csv");
    std::ofstream products(out + "/products.csv");

    if (!customers || !orders || !lineItems || !products) {
        std::cerr << "failed to open files under " << out << "\n";
        return 1;
    }

    customers << "id,name,country,age\n";
    for (int i = 1; i <= 1000; ++i) {
        const char* country = (i % 7 == 0) ? "PK" : ((i % 5 == 0) ? "US" : "DE");
        customers << i << ",customer_" << i << "," << country << "," << (18 + (i % 50)) << "\n";
    }

    orders << "id,customer_id,total,year,status\n";
    for (int i = 1; i <= 10000; ++i) {
        orders << i << "," << ((i % 1000) + 1) << "," << ((i % 1000) + 10) << ".0," << (2020 + (i % 5)) << ",ok\n";
    }

    lineItems << "order_id,product_id,qty,price\n";
    for (int i = 1; i <= 20000; ++i) {
        lineItems << ((i % 10000) + 1) << "," << ((i % 2000) + 1) << "," << ((i % 5) + 1) << "," << ((i % 500) + 1) << ".0\n";
    }

    products << "id,name,category,supplier_id\n";
    for (int i = 1; i <= 2000; ++i) {
        const char* category = (i % 4 == 0) ? "Electronics" : "General";
        products << i << ",product_" << i << "," << category << "," << ((i % 90) + 1) << "\n";
    }

    std::cout << "generated benchmark dataset seed=" << seed << " in " << out << "\n";
    return 0;
}
