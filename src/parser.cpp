#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <cassert>
#include <charconv>
#include <libdcc/parser.hpp>

namespace dcc {
    std::unique_ptr<Function> Parser::function() {
        consume(token_type::INT_KEYWORD, "Expected 'int' return type");
        auto identifier_token = consume(token_type::IDENTIFIER, "Expected function name");

        consume(token_type::OPEN_PARENTHESIS, "Expected '('");
        consume(token_type::VOID_KEYWORD, "Expected 'void' keyword");
        consume(token_type::CLOSE_PARENTHESIS, "Expected ')'");

        consume(token_type::OPEN_BRACE, "Expected '{'");
        auto stmt = statement();

        consume(token_type::CLOSE_BRACE, "Expected: '}");

        auto func = std::make_unique<Function>(identifier_token.text, std::move(stmt));
        return func;
    }

    std::unique_ptr<Statement> Parser::statement() {
        consume(token_type::RETURN_KEYWORD, "Expected 'return' keyword");

        auto expr = parse_expr();   
        if (!expr)
                throw std::runtime_error("Cannot parse inner expression");

        consume(token_type::SEMICOLON, "Expected semicolon");
        return std::make_unique<ReturnStmt>(std::move(expr));
    }

    std::unique_ptr<Expr> Parser::parse_factor() {
        auto c = peek();
        if (c.token == token_type::CONSTANT) {
            consume(token_type::CONSTANT, "Expected constant expression");
            int i;
            auto result = std::from_chars(c.text.data(), c.text.data() + c.text.size(), i);

            if (result.ec == std::errc::invalid_argument) {
                std::cerr << "Could not convert" << std::endl;
            }
            return std::make_unique<Constant>(i);
        } else if (c.token == token_type::COMPLEMENT || c.token == token_type::MINUS) {
            auto unary_op = consume(c.token, "Expected unary operator");
            // auto inner_expr = expression();
            auto inner_expr = parse_factor();
            if (!inner_expr)
                throw std::runtime_error("Cannot parse inner expression");
            
            switch (c.token) {
                case token_type::COMPLEMENT: return std::make_unique<Complement>('~', std::move(inner_expr));
                case token_type::MINUS: return std::make_unique<Negate>('-', std::move(inner_expr));
            }
            
        } else if (c.token == token_type::OPEN_PARENTHESIS) {
            /* "(" <expr> ")"*/
            consume(token_type::OPEN_PARENTHESIS, "Expected open parenthesis");
            // auto inner_expr = expression();
            //reset back to lowest precedence level inside brackets
            auto inner_expr = parse_expr();
            if (!inner_expr)
                throw std::runtime_error("Cannot parse inner expression");
            consume(token_type::CLOSE_PARENTHESIS, "Expected close parenthesis");
            return inner_expr;
        }
        return nullptr;
    }

    /* 1+ 2 * 3 + 4 should be parsed as (1 + (2 * 3)) + 4  - right operand is 2 * 3 */
    /* parse e1 <op> e2, all operators in e2 has higher precedence than <op> */
    std::unique_ptr<Expr> Parser::parse_expr(int min_prec) {
        auto left = parse_factor();
        if (!left) return nullptr;
        auto c = peek();
        while (get_precedence(c.token) >= min_prec) {
            auto op_token = consume(c.token, "Expected operator");
            BinaryOpType binary_op;

            /* Contextually it must be a unary negation */
            switch (op_token.token) {
                case token_type::ADDITION: binary_op = BinaryOpType::Add; break;
                case token_type::MINUS: binary_op = BinaryOpType::Sub; break;
                case token_type::MULTIPLICATION: binary_op = BinaryOpType::Mul; break;
                case token_type::DIVISION: binary_op = BinaryOpType::Div; break;
                case token_type::REMAINDER: binary_op = BinaryOpType::Rem; break;
                default: throw std::runtime_error("Unknown operator");
            } 
            auto right = parse_expr(get_precedence(op_token.token) + 1);
            if (!right) throw std::runtime_error("Expected expression after operator");

            left = std::make_unique<BinaryOperator>(binary_op, std::move(left), std::move(right));
            c = peek();
        }

        return left;
    }

    int Parser::get_precedence(token_type type) {
        switch (type) {
        case token_type::MULTIPLICATION:
        case token_type::DIVISION:
        case token_type::REMAINDER:
            return 50; // Higher precedence
        case token_type::ADDITION:
        case token_type::MINUS:
            return 45; // Lower precedence
        default:
            return -1; // Not binary operator
    }
    }

}