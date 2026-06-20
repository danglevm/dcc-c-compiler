#ifndef DCC_PARSER_HPP
#define DCC_PARSER_HPP

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <cassert>
#include <charconv>

#include <libdcc/lexer.hpp>

namespace {
    /* increase the indent level every level deeper into the tree */
    std::string get_indent(int level) {
        return std::string(level * 2, ' ');
    }
}

namespace dcc {
    struct Program;
    struct Expression;
    struct Statement;
    struct Function;
    struct UnaryOperator;

    enum class ExprType {
        CONSTANT,
        UNARY,
        BINARY
    };

    enum class BinaryOpType { Add, Sub, Mul, Div, Rem};
/*
    Left side is broad categories, abstract base class
    Right side is the concrete subclass

    program = Program(function_definition) 
    function_definition = Function(identifier name, statement body) 
    statement = Return(exp)
    exp = Constant(int)
*/
    struct ASTNode {
        virtual ~ASTNode() = default;

        /* for pretty printing */
        virtual void print(int indent = 0) const = 0;
    };

    struct Expr : public ASTNode {
        virtual ~Expr() = default;
        virtual ExprType getExprType() const = 0;
    };

    struct Constant : public Expr {
        int _value = 0;
        explicit Constant(const int value) : _value(value){};

        ExprType getExprType() const { return ExprType::CONSTANT; }

        void print(int indent = 0) const override {
            std::cout << get_indent(indent) << ". Constant: " << _value << "\n";
        }
    };

    struct UnaryOperator : public Expr {
        virtual ~UnaryOperator() = default;
        ExprType getExprType() { return ExprType::UNARY; }
        virtual char getUnaryOp() const = 0;
        virtual Expr * get_inner_expr() const = 0;
    };

    struct Complement : public UnaryOperator {
        char _unary_operator;
        std::unique_ptr<Expr> _expr;
        explicit Complement(char unary_operator, std::unique_ptr<Expr> expr) 
            : _unary_operator(unary_operator), _expr(std::move(expr)){};

        void print(int indent = 0) const override {
            if (_expr && _unary_operator) {
                std::cout << get_indent(indent) << ". Complement: \n ";
                _expr->print(indent + 1);
            }
        }

        char getUnaryOp() const { return '~'; }
        Expr * get_inner_expr() const { return _expr.get();}
        ExprType getExprType() const override { return ExprType::UNARY; }
    };

    struct Negate : public UnaryOperator {
        char _unary_operator;
        std::unique_ptr<Expr> _expr;
        explicit Negate(char unary_operator, std::unique_ptr<Expr> expr) : _unary_operator(unary_operator), _expr(std::move(expr)){};

        void print(int indent = 0) const override {
            if (_expr && _unary_operator) {
                std::cout << get_indent(indent) << ". Negate: \n ";
                _expr->print(indent + 1);
            }
        }

        char getUnaryOp() const { return '-'; }
        Expr * get_inner_expr() const { return _expr.get();}
        ExprType getExprType() const override { return ExprType::UNARY; }
    };

    struct BinaryOperator : public Expr {
        BinaryOpType _bin_op;
        std::unique_ptr<Expr> _left_expr;
        std::unique_ptr<Expr> _right_expr;

        explicit BinaryOperator(BinaryOpType bin_op, 
                                std::unique_ptr<Expr> left_expr,
                                std::unique_ptr<Expr> right_expr) :
                                _bin_op(bin_op), _left_expr(std::move(left_expr)), _right_expr(std::move(right_expr)) {};
        ExprType getExprType() const override { return ExprType::BINARY; }
        BinaryOpType getBinOp() const { return _bin_op; }
        Expr * getLeftExpr() const { return _left_expr.get(); }
        Expr * getRightExpr() const { return _right_expr.get(); }
        void print(int indent) const override {};
    };

    struct Statement : public ASTNode {
        virtual ~Statement() = default;
        virtual Expr * get_expr() const = 0;
    };

    struct ReturnStmt : public Statement {
        std::unique_ptr<Expr> _expr;
        explicit ReturnStmt(std::unique_ptr<Expr> expr) : _expr(std::move(expr)){};
        void print(int indent = 0) const override {
            if (_expr) {
                std::cout << get_indent(indent) << ". ReturnStatement:\n ";
                _expr->print(indent + 1);
            }
        }

        Expr * get_expr() const { return _expr.get(); }

    };

    struct Function : public ASTNode {
        std::string_view _identifier;
        std::unique_ptr<Statement> _stmt;
        explicit Function(const std::string_view identifier, std::unique_ptr<Statement> stmt) :
            _identifier(std::move(identifier)), _stmt(std::move(stmt)){};
        void print(int indent = 0) const override {
            std::cout << get_indent(indent) << "Function Declaration: " << _identifier << "\n";
            std::cout << get_indent(indent + 1) << "Returns: int\n";
            std::cout << get_indent(indent + 1) << "Body:\n";
            
            if (_stmt) {
                _stmt->print(indent + 2);
            }
        }
    };

    struct Program : public ASTNode { 
        std::unique_ptr<Function> _function_definition;
        explicit Program(std::unique_ptr<Function> function_definition) 
            : _function_definition(std::move(function_definition)){};               
    
        void print(int indent = 0) const override {
            std::cout << get_indent(indent) << "Program:\n";
            if (_function_definition) {
                _function_definition->print(indent + 1);
            }
        }
    };

    /* recursive top-down parsing */
    /* grammar that's to be updated */
    /*
    <program> ::= <function> 
    <function> ::= "int" <identifier> "(" "void" ")" "{" <statement> "}" 
    <statement> ::= "return" <exp> ";" 
    <exp> ::= <int> 
    <identifier> ::= ? An identifier token ? 
    <int> ::= ? A constant token ?
    
    */
    class Parser {
        public: 
            Parser(const std::vector<token>& tokens) : _tokens(std::move(tokens)), _cur(0) {};
            
            // <program> ::= <function>
            std::unique_ptr<Program> parse_program () {
                auto program = std::make_unique<Program>(function());
                //consume EOF or not?
                consume(token_type::ENDOFFILE, "Expected EOF");
                return program;
            };
        private:
            std::vector<token> _tokens;
            std::size_t _cur = 0; 

            void check_invariants () const {
            };

            bool isEOF() const {
                return peek().token == token_type::ENDOFFILE;
            }

            bool check_token(const token_type type) const {
                return peek().token == type;
            }

            bool match(const std::initializer_list<token_type> types) {
                for (auto type : types){
                    if (check_token(type)) {
                        advance();
                        return true;
                    }
                }
                return false;
            }

            token peek() const {
                return _tokens[_cur]; 
            }

            token advance() {
                auto consumed = peek();
                if (!isEOF()) ++_cur;
                return consumed;
            }
            
            std::unique_ptr<Function> function();

            std::unique_ptr<Statement> statement();

            /* <exp> ::= <int> <identifier> ::= ? An identifier token ?*/
            std::unique_ptr<Expr> parse_factor();

            std::unique_ptr<Expr> parse_expr(int min_prec = 0);

            token consume(token_type type, std::string msg) {
                if (check_token(type)) {
                    return advance();
                }

                throw std::runtime_error("Syntax Error: " + msg);
            };
                        
            int get_precedence(token_type type);

    };
}

#endif