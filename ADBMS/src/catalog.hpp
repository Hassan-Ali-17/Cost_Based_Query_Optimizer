#ifndef CATALOG_HPP
#define CATALOG_HPP

#include <string>
#include <unordered_map>
#include <vector>

struct ColumnStat {
    std::string name;
    double distinctCount = 0.0;
    double minValue = 0.0;
    double maxValue = 0.0;
};

struct TableStat {
    std::string name;
    double rowCount = 0.0;
    std::vector<ColumnStat> columns;
};

struct Catalog {
    std::vector<TableStat> tables;
};

struct TableData {
    std::string name;
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
    std::unordered_map<std::string, size_t> columnIndex;
};

struct Database {
    std::string dataDir;
    std::unordered_map<std::string, TableData> tables;
};

Catalog makeDefaultCatalog();
Catalog buildCatalogFromDatabase(const Database& db);
const TableStat* findTable(const Catalog& catalog, const std::string& table);
const ColumnStat* findColumn(const Catalog& catalog, const std::string& tableDotCol);
void printCatalog(const Catalog& catalog);
bool loadDatabaseFromCsv(const std::string& dataDir, Database& db, std::string& error);
bool loadOrCreateCatalogCache(const Database& db, Catalog& outCatalog, std::string& error);

#endif
