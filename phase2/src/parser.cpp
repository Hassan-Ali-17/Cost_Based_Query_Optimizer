#include "parser.h"
#include <cctype>
#include <stdexcept>
#include <algorithm>
#include <iostream>

enum class TokenType {
    KEYWORD, IDENTIFIER, NUMBER, STRING,
    COMMA, STAR, EQ, LT, LTE, GT, GTE, NEQ,
    LPAREN, RPAREN, END
};

struct Token {
    TokenType type;
    std::string value;
};

class Tokenizer {
    std::string input;
    size_t pos = 0;

    void skip_whitespace() {
        while (pos < input.size() && std::isspace(input[pos])) pos++;
    }

public:
    Tokenizer(const std::string& in) : input(in) {}

    Token next_token() {
        skip_whitespace();
        if (pos >= input.size()) return {TokenType::END, ""};

        char c = input[pos];

        if (c == ',') { pos++; return {TokenType::COMMA, ","}; }
        if (c == '*') { pos++; return {TokenType::STAR, "*"}; }
        if (c == '(') { pos++; return {TokenType::LPAREN, "("}; }
        if (c == ')') { pos++; return {TokenType::RPAREN, ")"}; }
        if (c == ';') { pos++; return {TokenType::END, ";"}; }

        if (c == '=') { pos++; return {TokenType::EQ, "="}; }
        if (c == '<') {
            if (pos + 1 < input.size() && input[pos+1] == '=') { pos += 2; return {TokenType::LTE, "<="}; }
            pos++; return {TokenType::LT, "<"};
        }
        if (c == '>') {
            if (pos + 1 < input.size() && input[pos+1] == '=') { pos += 2; return {TokenType::GTE, ">="}; }
            pos++; return {TokenType::GT, ">"};
        }
        if (c == '!') {
            if (pos + 1 < input.size() && input[pos+1] == '=') { pos += 2; return {TokenType::NEQ, "!="}; }
        }

        if (c == '\'') {
            pos++;
            std::string str;
            while (pos < input.size() && input[pos] != '\'') {
                str += input[pos++];
            }
            if (pos < input.size()) pos++; // skip closing quote
            return {TokenType::STRING, str};
        }

        if (std::isdigit(c) || c == '-') {
            std::string num;
            if (c == '-') num += input[pos++];
            while (pos < input.size() && (std::isdigit(input[pos]) || input[pos] == '.')) {
                num += input[pos++];
            }
            return {TokenType::NUMBER, num};
        }

        if (std::isalpha(c) || c == '_') {
            std::string id;
            while (pos < input.size() && (std::isalnum(input[pos]) || input[pos] == '_' || input[pos] == '.')) {
                id += input[pos++];
            }
            
            std::string upper_id = id;
            for (auto &x : upper_id) x = std::toupper(x);
            
            if (upper_id == "SELECT" || upper_id == "FROM" || upper_id == "WHERE" || 
                upper_id == "AND" || upper_id == "GROUP" || upper_id == "BY" || 
                upper_id == "LIMIT" || upper_id == "SUM" || upper_id == "COUNT" || 
                upper_id == "AVG" || upper_id == "MIN" || upper_id == "MAX") {
                return {TokenType::KEYWORD, upper_id};
            }
            return {TokenType::IDENTIFIER, id};
        }

        throw std::runtime_error("Unknown character in input: " + std::string(1, c));
    }
};

ColumnRef parse_column_ref(const std::string& id) {
    size_t dot = id.find('.');
    if (dot != std::string::npos) {
        return {id.substr(0, dot), id.substr(dot + 1)};
    }
    return {"", id};
}

OpType parse_op(TokenType t) {
    switch (t) {
        case TokenType::EQ: return OpType::EQ;
        case TokenType::NEQ: return OpType::NEQ;
        case TokenType::LT: return OpType::LT;
        case TokenType::LTE: return OpType::LTE;
        case TokenType::GT: return OpType::GT;
        case TokenType::GTE: return OpType::GTE;
        default: throw std::runtime_error("Invalid operator");
    }
}

Query Parser::parse(const std::string& sql) {
    Tokenizer tokenizer(sql);
    Query q;
    
    Token t = tokenizer.next_token();
    if (t.type != TokenType::KEYWORD || t.value != "SELECT") throw std::runtime_error("Expected SELECT");

    // parse select list
    t = tokenizer.next_token();
    if (t.type == TokenType::STAR) {
        q.select_star = true;
        t = tokenizer.next_token();
    } else {
        while (true) {
            Expr expr;
            if (t.type == TokenType::KEYWORD && (t.value == "SUM" || t.value == "COUNT" || t.value == "AVG" || t.value == "MIN" || t.value == "MAX")) {
                expr.type = ExprType::AGGREGATE;
                if (t.value == "SUM") expr.agg = AggType::SUM;
                else if (t.value == "COUNT") expr.agg = AggType::COUNT;
                else if (t.value == "AVG") expr.agg = AggType::AVG;
                else if (t.value == "MIN") expr.agg = AggType::MIN;
                else if (t.value == "MAX") expr.agg = AggType::MAX;
                
                t = tokenizer.next_token();
                if (t.type != TokenType::LPAREN) throw std::runtime_error("Expected (");
                t = tokenizer.next_token();
                if (t.type != TokenType::IDENTIFIER) throw std::runtime_error("Expected column inside agg");
                expr.col1 = parse_column_ref(t.value);
                t = tokenizer.next_token();
                
                // Hack to support SUM(qty * price) for Q2
                if (t.type == TokenType::STAR) { // multiplication
                    expr.type = ExprType::BINARY_OP;
                    // Note: grammar says aggregate(column) or column op column.
                    // But Q2 has SUM(line_items.qty * line_items.price).
                    // So we tweak AST: if it's agg, we check if inner is binop.
                    // To keep simple, we parse the expr inside
                    // Actually let's manually parse `qty * price` inside SUM.
                    t = tokenizer.next_token();
                    if (t.type != TokenType::IDENTIFIER) throw std::runtime_error("Expected column inside agg *");
                    expr.col2 = parse_column_ref(t.value);
                    // store the op somewhere? Wait, grammar says expr := column op column.
                    // And aggregate := SUM etc. We need aggregate(expr). 
                    // Let's assume we can just map this specifically for Q2 by using BINARY_OP inside AGG, but Expr struct is flat.
                    // Oh, Expr struct has agg, op, col1, col2. If agg != NONE and op is set, then it's agg(col1 op col2).
                    // I will use op for the inner operator if it exists.
                    // Wait, STAR is a token type, but we don't have OpType::MUL. Let's add it?
                    // Q2: SUM(line_items.qty * line_items.price)
                    // Let's just treat it as a special aggregate type or extend OpType.
                    throw std::runtime_error("Multiplication not fully supported in flat AST yet.");
                }
                
                if (t.type != TokenType::RPAREN) throw std::runtime_error("Expected )");
                t = tokenizer.next_token();
            } else if (t.type == TokenType::IDENTIFIER) {
                expr.type = ExprType::COLUMN;
                expr.col1 = parse_column_ref(t.value);
                t = tokenizer.next_token();
                
                // could be column op column
                if (t.type == TokenType::STAR) {
                    throw std::runtime_error("expr = col * col not fully implemented");
                }
            } else {
                throw std::runtime_error("Unexpected token in SELECT");
            }
            
            q.select_list.push_back(expr);
            if (t.type == TokenType::COMMA) {
                t = tokenizer.next_token();
            } else {
                break;
            }
        }
    }

    if (t.type != TokenType::KEYWORD || t.value != "FROM") throw std::runtime_error("Expected FROM");
    
    // parse FROM list
    while (true) {
        t = tokenizer.next_token();
        if (t.type != TokenType::IDENTIFIER) throw std::runtime_error("Expected table name");
        q.from_tables.push_back(t.value);
        
        t = tokenizer.next_token();
        if (t.type == TokenType::COMMA) {
            continue;
        } else {
            break;
        }
    }
    
    if (t.type == TokenType::END) return q;
    
    if (t.type == TokenType::KEYWORD && t.value == "WHERE") {
        while (true) {
            t = tokenizer.next_token();
            if (t.type != TokenType::IDENTIFIER && t.type != TokenType::NUMBER && t.type != TokenType::STRING) throw std::runtime_error("Expected column or literal in WHERE");
            
            Predicate p;
            if (t.type == TokenType::IDENTIFIER) {
                p.col1 = parse_column_ref(t.value);
            } else {
                // literal op col? simplify and say left side must be column.
            }
            
            t = tokenizer.next_token();
            p.op = parse_op(t.type);
            
            t = tokenizer.next_token();
            if (t.type == TokenType::IDENTIFIER) {
                p.is_col2 = true;
                p.col2 = parse_column_ref(t.value);
            } else if (t.type == TokenType::NUMBER) {
                p.is_col2 = false;
                if (t.value.find('.') != std::string::npos) p.literal = Value(std::stod(t.value));
                else p.literal = Value(std::stoi(t.value));
            } else if (t.type == TokenType::STRING) {
                p.is_col2 = false;
                p.literal = Value(t.value);
            }
            
            q.where_preds.push_back(p);
            
            t = tokenizer.next_token();
            if (t.type == TokenType::KEYWORD && t.value == "AND") {
                continue;
            } else {
                break;
            }
        }
    }
    
    if (t.type == TokenType::KEYWORD && t.value == "GROUP") {
        t = tokenizer.next_token();
        if (t.type != TokenType::KEYWORD || t.value != "BY") throw std::runtime_error("Expected BY");
        t = tokenizer.next_token();
        if (t.type != TokenType::IDENTIFIER) throw std::runtime_error("Expected column in GROUP BY");
        
        q.has_group_by = true;
        q.group_by_col = parse_column_ref(t.value);
        
        t = tokenizer.next_token();
    }
    
    if (t.type == TokenType::KEYWORD && t.value == "LIMIT") {
        t = tokenizer.next_token();
        if (t.type != TokenType::NUMBER) throw std::runtime_error("Expected number in LIMIT");
        q.has_limit = true;
        q.limit_count = std::stoi(t.value);
        t = tokenizer.next_token();
    }
    
    return q;
}
