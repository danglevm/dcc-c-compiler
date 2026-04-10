#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <cassert>
#include <charconv>


#include <libdcc/parser.hpp>

namespace {


}

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

        auto expr = expression();   
        if (!expr)
                throw std::runtime_error("Cannot parse inner expression");

        consume(token_type::SEMICOLON, "Expected semicolon");
        return std::make_unique<ReturnStmt>(std::move(expr));
    }

    std::unique_ptr<Expr> Parser::expression() {
        auto c = peek();
        if (c.token == token_type::CONSTANT) {
            consume(token_type::CONSTANT, "Expected constant expression");
            int i;
            auto result = std::from_chars(c.text.data(), c.text.data() + c.text.size(), i);

            if (result.ec == std::errc::invalid_argument) {
                std::cerr << "Could not convert" << std::endl;
            }
            return std::make_unique<Constant>(i);
        } else if (c.token == token_type::COMPLEMENT || c.token == token_type::NEGATION) {
            auto unary_op = consume(c.token, "Expected unary operator");
            std::cout << "\nTesting" << c.text;
            auto inner_expr = expression();

            if (!inner_expr)
                throw std::runtime_error("Cannot parse inner expression");
            
            switch (c.token) {
                case token_type::COMPLEMENT: return std::make_unique<Complement>('~', std::move(inner_expr));
                case token_type::NEGATION: return std::make_unique<Negate>('-', std::move(inner_expr));
            }
            
        } else if (c.token == token_type::OPEN_PARENTHESIS) {
            /* "(" <expr> ")"*/
            consume(token_type::OPEN_PARENTHESIS, "Expected open parenthesis");
            auto inner_expr = expression();
            if (!inner_expr)
                throw std::runtime_error("Cannot parse inner expression");
            consume(token_type::CLOSE_PARENTHESIS, "Expected close parenthesis");
            return inner_expr;
        }

        return nullptr;
    }

}