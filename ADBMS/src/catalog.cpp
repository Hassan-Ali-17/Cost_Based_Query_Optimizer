#include "catalog.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace {
std::vector<std::string> splitCsvLine(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    bool inQuotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (ch == '"') {
            inQuotes = !inQuotes;
            continue;
        }
        if (ch == ',' && !inQuotes) {
            out.push_back(cur);
            cur.clear();
            continue;
        }
        cur.push_back(ch);
    }
    out.push_back(cur);
    return out;
}

bool isNumeric(const std::string& s, double& value) {
    if (s.empty()) {
        return false;
    }
    char* end = nullptr;
    value = std::strtod(s.c_str(), &end);
    return end != nullptr && *end == '\0';
}

bool readTableCsv(const fs::path& path, const std::string& tableName, TableData& out, std::string& error) {
    std::ifstream in(path);
    if (!in) {
        error = "failed to open csv: " + path.string();
        return false;
    }

    std::string header;
    if (!std::getline(in, header)) {
        error = "empty csv: " + path.string();
        return false;
    }

    out = TableData{};
    out.name = tableName;
    out.columns = splitCsvLine(header);
    for (size_t i = 0; i < out.columns.size(); ++i) {
        out.columnIndex[out.columns[i]] = i;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        std::vector<std::string> row = splitCsvLine(line);
        if (row.size() < out.columns.size()) {
            row.resize(out.columns.size());
        }
        out.rows.push_back(std::move(row));
    }

    return true;
}

fs::path cachePath(const std::string& dataDir) {
    return fs::path(dataDir) / "catalog.json";
}

bool saveCatalogJson(const std::string& dataDir, const Catalog& c, std::string& error) {
    const fs::path path = cachePath(dataDir);
    std::ofstream out(path);
    if (!out) {
        error = "failed to write cache: " + path.string();
        return false;
    }

    out << "{\n";
    out << "  \"tables\": [\n";
    for (size_t i = 0; i < c.tables.size(); ++i) {
        const auto& t = c.tables[i];
        out << "    {\n";
        out << "      \"name\": \"" << t.name << "\",\n";
        out << "      \"row_count\": " << t.rowCount << ",\n";
        out << "      \"columns\": [\n";
        for (size_t j = 0; j < t.columns.size(); ++j) {
            const auto& col = t.columns[j];
            out << "        {\"name\": \"" << col.name
                << "\", \"distinct\": " << col.distinctCount
                << ", \"min\": " << col.minValue
                << ", \"max\": " << col.maxValue << "}";
            if (j + 1 < t.columns.size()) {
                out << ",";
            }
            out << "\n";
        }
        out << "      ]\n";
        out << "    }";
        if (i + 1 < c.tables.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";

    return true;
}

std::string trim(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i])) != 0) {
        ++i;
    }
    size_t j = s.size();
    while (j > i && std::isspace(static_cast<unsigned char>(s[j - 1])) != 0) {
        --j;
    }
    return s.substr(i, j - i);
}

std::string extractQuoted(const std::string& line) {
    const size_t first = line.find('"');
    if (first == std::string::npos) {
        return "";
    }
    const size_t second = line.find('"', first + 1);
    if (second == std::string::npos) {
        return "";
    }
    const size_t third = line.find('"', second + 1);
    if (third == std::string::npos) {
        return "";
    }
    const size_t fourth = line.find('"', third + 1);
    if (fourth == std::string::npos) {
        return "";
    }
    return line.substr(third + 1, fourth - third - 1);
}

double extractNumber(const std::string& line, const std::string& key) {
    const std::string token = "\"" + key + "\":";
    size_t pos = line.find(token);
    if (pos == std::string::npos) {
        pos = line.find("\"" + key + "\": ");
        if (pos == std::string::npos) {
            return 0.0;
        }
        pos += key.size() + 4;
    } else {
        pos += token.size();
    }
    std::string tail = trim(line.substr(pos));
    while (!tail.empty() && (tail.back() == ',' || tail.back() == '}')) {
        tail.pop_back();
    }
    tail = trim(tail);
    return std::strtod(tail.c_str(), nullptr);
}

bool loadCatalogJson(const std::string& dataDir, Catalog& out, std::string& error) {
    const fs::path path = cachePath(dataDir);
    std::ifstream in(path);
    if (!in) {
        return false;
    }

    out = Catalog{};
    std::string line;
    TableStat currentTable;
    bool inTable = false;
    while (std::getline(in, line)) {
        const std::string t = trim(line);
        if (t.find("\"name\":") != std::string::npos && t.find("\"row_count\"") == std::string::npos && t.find("\"distinct\"") == std::string::npos) {
            if (!inTable) {
                currentTable = TableStat{};
                inTable = true;
            }
            currentTable.name = extractQuoted(t);
            continue;
        }
        if (t.find("\"row_count\":") != std::string::npos) {
            currentTable.rowCount = extractNumber(t, "row_count");
            continue;
        }
        if (t.find("\"distinct\"") != std::string::npos) {
            ColumnStat col;
            col.name = extractQuoted(t);
            col.distinctCount = extractNumber(t, "distinct");
            col.minValue = extractNumber(t, "min");
            col.maxValue = extractNumber(t, "max");
            currentTable.columns.push_back(col);
            continue;
        }
        if (inTable && t == "},") {
            out.tables.push_back(currentTable);
            inTable = false;
            continue;
        }
        if (inTable && t == "}") {
            out.tables.push_back(currentTable);
            inTable = false;
            continue;
        }
    }

    if (out.tables.empty()) {
        error = "invalid or empty catalog cache";
        return false;
    }

    return true;
}

void addColumn(TableStat& table, const std::string& name, double distinctCount, double minValue, double maxValue) {
    table.columns.push_back(ColumnStat{name, distinctCount, minValue, maxValue});
}
}

Catalog makeDefaultCatalog() {
    Catalog c;

    TableStat customers{"customers", 10000.0, {}};
    addColumn(customers, "id", 10000, 1, 10000);
    addColumn(customers, "country", 240, 0, 0);
    addColumn(customers, "age", 70, 18, 88);
    c.tables.push_back(customers);

    TableStat orders{"orders", 500000.0, {}};
    addColumn(orders, "id", 500000, 1, 500000);
    addColumn(orders, "customer_id", 10000, 1, 10000);
    addColumn(orders, "year", 10, 2016, 2025);
    addColumn(orders, "total", 50000, 1, 10000);
    c.tables.push_back(orders);

    TableStat lineItems{"line_items", 2000000.0, {}};
    addColumn(lineItems, "order_id", 500000, 1, 500000);
    addColumn(lineItems, "product_id", 50000, 1, 50000);
    addColumn(lineItems, "qty", 20, 1, 20);
    addColumn(lineItems, "price", 100000, 1, 5000);
    c.tables.push_back(lineItems);

    TableStat products{"products", 50000.0, {}};
    addColumn(products, "id", 50000, 1, 50000);
    addColumn(products, "category", 100, 0, 0);
    c.tables.push_back(products);

    return c;
}

Catalog buildCatalogFromDatabase(const Database& db) {
    Catalog c;
    for (const auto& [tableName, table] : db.tables) {
        TableStat ts;
        ts.name = tableName;
        ts.rowCount = static_cast<double>(table.rows.size());

        for (size_t colIdx = 0; colIdx < table.columns.size(); ++colIdx) {
            ColumnStat col;
            col.name = table.columns[colIdx];
            std::set<std::string> distinct;
            bool haveNum = false;
            double minV = 0.0;
            double maxV = 0.0;

            for (const auto& row : table.rows) {
                if (colIdx >= row.size()) {
                    continue;
                }
                const std::string& v = row[colIdx];
                distinct.insert(v);

                double num = 0.0;
                if (isNumeric(v, num)) {
                    if (!haveNum) {
                        minV = num;
                        maxV = num;
                        haveNum = true;
                    } else {
                        minV = std::min(minV, num);
                        maxV = std::max(maxV, num);
                    }
                }
            }

            col.distinctCount = static_cast<double>(distinct.size());
            col.minValue = haveNum ? minV : 0.0;
            col.maxValue = haveNum ? maxV : 0.0;
            ts.columns.push_back(col);
        }

        c.tables.push_back(std::move(ts));
    }

    return c;
}

const TableStat* findTable(const Catalog& catalog, const std::string& table) {
    for (const auto& t : catalog.tables) {
        if (t.name == table) {
            return &t;
        }
    }
    return nullptr;
}

const ColumnStat* findColumn(const Catalog& catalog, const std::string& tableDotCol) {
    const size_t dot = tableDotCol.find('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 >= tableDotCol.size()) {
        return nullptr;
    }
    const std::string table = tableDotCol.substr(0, dot);
    const std::string col = tableDotCol.substr(dot + 1);

    const TableStat* t = findTable(catalog, table);
    if (!t) {
        return nullptr;
    }
    for (const auto& c : t->columns) {
        if (c.name == col) {
            return &c;
        }
    }
    return nullptr;
}

void printCatalog(const Catalog& catalog) {
    std::cout << "catalog: " << catalog.tables.size() << " tables\n";
    for (const auto& t : catalog.tables) {
        std::cout << "  " << t.name << " " << t.rowCount << " rows\n";
        for (const auto& c : t.columns) {
            std::cout << "    " << c.name
                      << " distinct=" << c.distinctCount
                      << " min=" << c.minValue
                      << " max=" << c.maxValue
                      << "\n";
        }
    }
}

bool loadDatabaseFromCsv(const std::string& dataDir, Database& db, std::string& error) {
    db = Database{};
    db.dataDir = dataDir;

    const std::vector<std::string> expected = {
        "customers",
        "orders",
        "line_items",
        "products",
    };

    for (const auto& t : expected) {
        TableData table;
        const fs::path p = fs::path(dataDir) / (t + ".csv");
        if (!readTableCsv(p, t, table, error)) {
            return false;
        }
        db.tables[t] = std::move(table);
    }

    return true;
}

bool loadOrCreateCatalogCache(const Database& db, Catalog& outCatalog, std::string& error) {
    Catalog cached;
    if (loadCatalogJson(db.dataDir, cached, error)) {
        outCatalog = std::move(cached);
        return true;
    }

    outCatalog = buildCatalogFromDatabase(db);
    std::string writeErr;
    if (!saveCatalogJson(db.dataDir, outCatalog, writeErr)) {
        error = writeErr;
    }
    return true;
}
