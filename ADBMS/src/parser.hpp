#ifndef PARSER_HPP
#define PARSER_HPP

#include <string>

#include "plan.hpp"

bool parseSql(const std::string& sql, Query& out, std::string& error);

#endif
