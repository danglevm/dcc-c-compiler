#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <cassert>
#include <charconv>
#include <string>
#include <libdcc/parser.hpp>


namespace dcc {
    std::unique_ptr<Function> Parser::function() {
        consume(token_type::INT_KEYWORD, "Expected 'int' return type");
        auto identifier_token = consume(token_type::IDENTIFIER, "Expected function name");

        consume(token_type::OPEN_PARENTHESIS, "Expected '('");
        consume(token_type::VOID_KEYWORD, "Expected 'void' keyword");
        consume(token_type::CLOSE_PARENTHESIS, "Expected ')'");

        consume(token_type::OPEN_BRACE, "Expected '{'");

        std::vector<std::unique_ptr<BlockItem>> function_body;

        while (peek().token != token_type::CLOSE_BRACE) {
            function_body.push_back(parse_block_item());
        }

        consume(token_type::CLOSE_BRACE, "Expected: '}");

        auto func = std::make_unique<Function>(identifier_token.text, std::move(function_body));
        return func;
    }

    std::unique_ptr<BlockItem> Parser::parse_block_item() {
        if (peek().token == token_type::INT_KEYWORD) {
            return declaration(); // Returns a Declaration AST node
        } else {
            return statement();   // Returns a Statement AST node
        }
    }

    std::unique_ptr<Statement> Parser::statement() {
        /* return statement */
        if (peek().token == token_type::RETURN_KEYWORD) {
            consume(token_type::RETURN_KEYWORD, "Expected 'return' keyword");
            
            std::unique_ptr<Expr> expr = nullptr;        
            if (peek().token != token_type::SEMICOLON) {
                expr = parse_expr();
                if (!expr) {
                    throw std::runtime_error("Cannot parse expression for return");
                }
            }
            consume(token_type::SEMICOLON, "Expected semicolon");
            return std::make_unique<ReturnStmt>(std::move(expr));
        } 

        /* deal with return; which is a null return statement */
        /* null statement */
        else if (peek().token == token_type::SEMICOLON) {
            consume(token_type::SEMICOLON, "Expected semicolon");
            return std::make_unique<NullStmt>();
        }
        
        /* if statements*/
        else if (peek().token == token_type::IF_KEYWORD) {
            consume(token_type::IF_KEYWORD, "Expected 'if' keyword");
            consume(token_type::OPEN_PARENTHESIS, "Expected open parenthesis");
            /* if expression */
            std::unique_ptr<Expr> expr = parse_expr();
            if (!expr) {
                throw std::runtime_error("Cannot parse if expression");
            } 

            consume(token_type::CLOSE_PARENTHESIS, "Expected close parenthesis");

            /* then statement*/
            auto then_stmt = statement();
            if (!then_stmt) {
                throw std::runtime_error("Cannot parse if statement");
            }

            std::unique_ptr<Statement> else_stmt = nullptr;
            if (peek().token == token_type::ELSE_KEYWORD) {
                consume(token_type::ELSE_KEYWORD, "Expected else keyword");
                else_stmt = statement();
                if (!else_stmt) {
                    throw std::runtime_error("Cannot parse else statement");
                }
            }

            return std::make_unique<IfStatement>(std::move(expr), std::move(then_stmt), std::move(else_stmt));
        }
        /* expression statement */
        else {
            auto expr = parse_expr();
            if (!expr) {
                throw std::runtime_error("Cannot parse expression for return statement");
            }
            consume(token_type::SEMICOLON, "Expected semicolon");
            return std::make_unique<ExpressionStmt>(std::move(expr));
        }

    }

    std::unique_ptr<Declaration> Parser::declaration() {
        consume(token_type::INT_KEYWORD, "Expected 'int' in declaration");
        auto identifier_token = consume(token_type::IDENTIFIER, "Expected function name");

        std::unique_ptr<Expr> expr = nullptr;
        if (peek().token == token_type::ASSIGNMENT) {
            consume(token_type::ASSIGNMENT, "Expected assignment in declaration");
            expr = parse_expr();
            if (!expr) {
                throw std::runtime_error("Cannot parse expression for declaration");
            }
        }
        consume(token_type::SEMICOLON, "Expected semicolon");
        return std::make_unique<DeclarationVariable>(identifier_token.text, std::move(expr));
    }

    std::unique_ptr<Expr> Parser::parse_factor() {
        auto c = peek();
        /* a constant */
        if (c.token == token_type::CONSTANT) {
            consume(token_type::CONSTANT, "Expected constant expression");
            int i;
            auto result = std::from_chars(c.text.data(), c.text.data() + c.text.size(), i);

            if (result.ec == std::errc::invalid_argument) {
                std::cerr << "Could not convert" << std::endl;
            }
            return std::make_unique<Constant>(i);

            /* unary operator*/
        } else if (c.token == token_type::IDENTIFIER) {
            auto identifier_token = consume(token_type::IDENTIFIER, "Expected identifier");

            return std::make_unique<Var>(identifier_token.text);
        }
        
        else if (c.token == token_type::COMPLEMENT || c.token == token_type::MINUS || c.token == token_type::NOT) {
            auto unary_op = consume(c.token, "Expected unary operator");
            // auto inner_expr = expression();
            auto inner_expr = parse_factor();
            if (!inner_expr)
                throw std::runtime_error("Cannot parse inner expression");
            
            switch (c.token) {
                case token_type::COMPLEMENT: return std::make_unique<UnaryOperator>(dcc::UnaryOpType::Complement, std::move(inner_expr));
                case token_type::MINUS: return std::make_unique<UnaryOperator>(dcc::UnaryOpType::Negate, std::move(inner_expr));
                case token_type::NOT: return std::make_unique<UnaryOperator>(dcc::UnaryOpType::Not, std::move(inner_expr));
            }
            
            /* expression within */
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
            if (c.token == token_type::ASSIGNMENT) {
                auto op_token = consume(token_type::ASSIGNMENT, "Expected '='");
                auto right = parse_expr(get_precedence(token_type::ASSIGNMENT));
                if (!right) throw std::runtime_error("Expected expression after assignment operator");
                left = std::make_unique<Assignment>(std::move(left), std::move(right));
            } else if (c.token == token_type::QUESTION_MARK_DELIMITER) {
                consume(token_type::QUESTION_MARK_DELIMITER, "Expected '?'");
                auto middle = parse_expr(0); /* this middle expression is wrapped by `?` and `:` */
                consume(token_type::COLON_DELIMITER, "Expected ':'");
                auto right = parse_expr(get_precedence(c.token));
                left = std::make_unique<Conditional>(std::move(left), std::move(middle), std::move(right));
            } else {
                auto op_token = consume(c.token, "Expected operator");
                BinaryOpType binary_op;

                /* Contextually it must be a binary operation */
                switch (op_token.token) {
                    case token_type::ADDITION: binary_op = BinaryOpType::Add; break;
                    case token_type::MINUS: binary_op = BinaryOpType::Sub; break;
                    case token_type::MULTIPLICATION: binary_op = BinaryOpType::Mul; break;
                    case token_type::DIVISION: binary_op = BinaryOpType::Div; break;
                    case token_type::REMAINDER: binary_op = BinaryOpType::Rem; break;
                    case token_type::AND: binary_op = BinaryOpType::And; break;
                    case token_type::OR: binary_op = BinaryOpType::Or; break;
                    case token_type::EQUALITY: binary_op = BinaryOpType::Equal; break;
                    case token_type::NOT_EQUALITY: binary_op =  BinaryOpType::NotEqual; break;
                    case token_type::LESSER: binary_op = BinaryOpType::LessThan; break;
                    case token_type::LESSER_EQUAL: binary_op = BinaryOpType::LessOrEqual; break;
                    case token_type::GREATER: binary_op = BinaryOpType::GreaterThan; break;
                    case token_type::GREATER_EQUAL: binary_op = BinaryOpType::GreaterOrEqual; break;
                    default: throw std::runtime_error("Unknown operator");
                } 
                auto right = parse_expr(get_precedence(op_token.token) + 1);
                if (!right) throw std::runtime_error("Expected expression after operator");

                left = std::make_unique<BinaryOperator>(binary_op, std::move(left), std::move(right));
            }
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
        case token_type::LESSER:
        case token_type::LESSER_EQUAL:
        case token_type::GREATER:
        case token_type::GREATER_EQUAL:
            return 35;
        case token_type::EQUALITY:
        case token_type::NOT_EQUALITY:
            return 30;
        case token_type::AND:
            return 10;
        case token_type::OR:
            return 5;
        case token_type::QUESTION_MARK_DELIMITER:
            return 3;
        case token_type::ASSIGNMENT:
            return 1;
        default:
            return -1; // Not binary operator
    }
    }

}