#include "parser.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace {
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

std::string lowerCopy(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

std::vector<std::string> splitByComma(const std::string& s) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        const std::string t = trim(item);
        if (!t.empty()) {
            out.push_back(t);
        }
    }
    return out;
}

std::vector<std::string> splitPredicates(const std::string& whereRaw) {
    std::vector<std::string> out;
    std::string current;
    int quoteState = 0;
    for (size_t i = 0; i < whereRaw.size(); ++i) {
        const char ch = whereRaw[i];
        if (ch == '\'' || ch == '"') {
            quoteState = 1 - quoteState;
            current.push_back(ch);
            continue;
        }

        if (quoteState == 0 && i + 3 <= whereRaw.size()) {
            const std::string tri = lowerCopy(whereRaw.substr(i, 3));
            if (tri == "and") {
                const char prev = (i == 0) ? ' ' : whereRaw[i - 1];
                const char next = (i + 3 < whereRaw.size()) ? whereRaw[i + 3] : ' ';
                if (std::isspace(static_cast<unsigned char>(prev)) != 0 && std::isspace(static_cast<unsigned char>(next)) != 0) {
                    const std::string t = trim(current);
                    if (!t.empty()) {
                        out.push_back(t);
                    }
                    current.clear();
                    i += 2;
                    continue;
                }
            }
        }

        current.push_back(ch);
    }

    const std::string t = trim(current);
    if (!t.empty()) {
        out.push_back(t);
    }
    return out;
}

bool parsePredicate(const std::string& text, Predicate& out) {
    static const std::vector<std::pair<std::string, PredOp>> ops = {
        {"<=", PredOp::LE},
        {">=", PredOp::GE},
        {"!=", PredOp::NE},
        {"=", PredOp::EQ},
        {"<", PredOp::LT},
        {">", PredOp::GT},
    };

    for (const auto& [tok, op] : ops) {
        const size_t pos = text.find(tok);
        if (pos == std::string::npos) {
            continue;
        }

        std::string lhs = trim(text.substr(0, pos));
        std::string rhs = trim(text.substr(pos + tok.size()));
        if (lhs.empty() || rhs.empty()) {
            return false;
        }

        if ((rhs.front() == '\'' && rhs.back() == '\'') || (rhs.front() == '"' && rhs.back() == '"')) {
            if (rhs.size() >= 2) {
                rhs = rhs.substr(1, rhs.size() - 2);
            }
        }

        out.left = lhs;
        out.right = rhs;
        out.op = op;
        out.isJoin = (lhs.find('.') != std::string::npos) && (rhs.find('.') != std::string::npos);
        return true;
    }

    return false;
}
}

bool parseSql(const std::string& sql, Query& out, std::string& error) {
    out = Query{};
    std::string q = trim(sql);
    if (!q.empty() && q.back() == ';') {
        q.pop_back();
    }

    const std::string lower = lowerCopy(q);
    const size_t sPos = lower.find("select ");
    const size_t fPos = lower.find(" from ");
    if (sPos != 0 || fPos == std::string::npos) {
        error = "expected SELECT ... FROM ...";
        return false;
    }

    const size_t wPos = lower.find(" where ");
    const size_t gPos = lower.find(" group by ");
    const size_t lPos = lower.find(" limit ");

    const size_t selectBegin = 7;
    const size_t selectEnd = fPos;
    out.selectRaw = trim(q.substr(selectBegin, selectEnd - selectBegin));

    size_t fromEnd = q.size();
    if (wPos != std::string::npos && wPos < fromEnd) fromEnd = wPos;
    if (gPos != std::string::npos && gPos < fromEnd) fromEnd = gPos;
    if (lPos != std::string::npos && lPos < fromEnd) fromEnd = lPos;

    out.tables = splitByComma(q.substr(fPos + 6, fromEnd - (fPos + 6)));
    if (out.tables.empty()) {
        error = "no tables in FROM";
        return false;
    }

    if (wPos != std::string::npos) {
        size_t whereEnd = q.size();
        if (gPos != std::string::npos && gPos > wPos) whereEnd = gPos;
        if (lPos != std::string::npos && lPos > wPos && lPos < whereEnd) whereEnd = lPos;
        const auto predicateText = splitPredicates(q.substr(wPos + 7, whereEnd - (wPos + 7)));
        for (const auto& pText : predicateText) {
            Predicate p;
            if (!parsePredicate(pText, p)) {
                error = "invalid predicate: " + pText;
                return false;
            }
            out.predicates.push_back(p);
        }
    }

    if (gPos != std::string::npos) {
        size_t groupEnd = q.size();
        if (lPos != std::string::npos && lPos > gPos) groupEnd = lPos;
        out.groupBy = trim(q.substr(gPos + 10, groupEnd - (gPos + 10)));
        out.hasGroupBy = !out.groupBy.empty();
    }

    if (lPos != std::string::npos) {
        const std::string limText = trim(q.substr(lPos + 7));
        try {
            out.limit = std::stoi(limText);
            out.hasLimit = out.limit > 0;
        } catch (...) {
            error = "invalid LIMIT";
            return false;
        }
    }

    return true;
}
