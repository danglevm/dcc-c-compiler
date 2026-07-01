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
        VAR,
        UNARY,
        BINARY,
        ASSIGNMENT
    };

    enum class StatementType {
        RETURN,
        EXPRESSION,
        NULL_STMT
    };

    enum class UnaryOpType { Complement, Negate, Not};

    enum class BinaryOpType { Add, Sub, Mul, Div, Rem, And, Or, Equal, NotEqual, 
                                LessThan, LessOrEqual, GreaterThan, GreaterOrEqual };
/*
    Left side is broad categories, abstract base class
    Right side is the concrete subclass

    program = Program(function_definition) 
    function_definition = Function(identifier name, statement body) 
    statement = Return(exp)
    exp = Constant(int)
    declaration

    Declaration is different from statements; declarations are not statements, because statements are executed when program runs
    while declarations tell the compiler a statement exist and can be used later.
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
        UnaryOpType _unary_operator;
        std::unique_ptr<Expr> _expr;
        explicit UnaryOperator(UnaryOpType unary_operator, std::unique_ptr<Expr> expr) 
            : _unary_operator(unary_operator), _expr(std::move(expr)){};

        UnaryOpType getUnaryOp() const { return _unary_operator;}
        Expr * get_inner_expr() const { return _expr.get();}
        ExprType getExprType() const { return ExprType::UNARY; }

        void print (int indent = 0) const override {
            if (_expr) {
                switch(_unary_operator) {
                    case UnaryOpType::Complement: std::cout << get_indent(indent) << ". Complement: \n ";
                    case UnaryOpType::Negate: std::cout << get_indent(indent) << ". Negate: \n ";
                    case UnaryOpType::Not: std::cout << get_indent(indent) << ". Not: \n ";
                }
            }
            _expr->print(indent + 1);
        }
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

    struct Var : public Expr {
        std::string_view _identifier;

        explicit Var(std::string_view identifier) : _identifier(identifier) {};
        std::string_view get_identifier() { return _identifier; }
        ExprType getExprType() const override { return ExprType::VAR; }
        void print(int indent) const override {};
    };

    struct Assignment : public Expr {
        std::unique_ptr<Expr> _left_expr;
        std::unique_ptr<Expr> _right_expr;

        explicit Assignment(std::unique_ptr<Expr> left_expr, std::unique_ptr<Expr> right_expr) : 
                        _left_expr(std::move(left_expr)), _right_expr(std::move(right_expr)) {};

        Expr * getLeftExpr() const { return _left_expr.get(); }
        Expr * getRightExpr() const { return _right_expr.get(); }
        ExprType getExprType() const override { return ExprType::ASSIGNMENT; }
        void print(int indent) const override {};
    };

    struct BlockItem : public ASTNode {
        void print(int indent = 0) const override {};
    };

    struct Statement : public BlockItem {
        virtual ~Statement() = default;
        virtual Expr * get_expr() const = 0;
        virtual StatementType get_statement_type() const = 0;

        void print(int indent = 0) const override {};
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

        Expr * get_expr() const override { return _expr.get(); }
        StatementType get_statement_type() const override { return StatementType::RETURN; }
    };

    struct ExpressionStmt : public Statement {
        std::unique_ptr<Expr> _expr;
        explicit ExpressionStmt(std::unique_ptr<Expr> expr) : _expr(std::move(expr)){};
        void print(int indent = 0) const override {};

        Expr * get_expr() const override { return _expr.get(); }
        StatementType get_statement_type() const override { return StatementType::EXPRESSION; }
    };

    struct NullStmt : public Statement {
        void print(int indent = 0) const override {};
        Expr * get_expr() const override { return nullptr; }
        StatementType get_statement_type() const override { return StatementType::NULL_STMT; }
    };
    
    struct Declaration : public BlockItem {
        virtual ~Declaration() = default;
        void print(int indent = 0) const override {};
    };

    struct DeclarationVariable : public Declaration {
        std::string_view _identifier;
        std::unique_ptr<Expr> _initializer;

        explicit DeclarationVariable(std::string_view identifier, std::unique_ptr<Expr> initializer = nullptr) 
            : _identifier(std::move(identifier)), _initializer(std::move(initializer)) {}
        
        void print(int indent = 0) const override {};
    };

    struct Function : public ASTNode {
        std::string_view _identifier;
        std::vector<std::unique_ptr<BlockItem>> _block_items;
        explicit Function(const std::string_view identifier, std::vector<std::unique_ptr<BlockItem>> block_items) :
            _identifier(std::move(identifier)), _block_items(std::move(block_items)){};
        void print(int indent = 0) const override {
            std::cout << get_indent(indent) << "Function Declaration: " << _identifier << "\n";
            std::cout << get_indent(indent + 1) << "Returns: int\n";
            std::cout << get_indent(indent + 1) << "Body:\n";
            
            // if (_stmt) {
            //     _stmt->print(indent + 2);
            // }
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

            std::unique_ptr<BlockItem> parse_block_item();

            std::unique_ptr<Declaration> declaration();

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