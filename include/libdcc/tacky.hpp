#ifndef DCC_TACKY_HPP
#define DCC_TACKY_HPP

#include <string>
#include <iostream>
#include <memory>
#include <vector>

#include <libdcc/parser.hpp>
#include <variant>

/*
program = Program(function_definition) 
function_definition = Function(identifier, 1 instruction* body) 
instruction = Return(val) | Unary(unary_operator, val src, val dst)
val = Constant(int) | Var(identifier) 
unary_operator = Complement | Negate
*/

namespace tacky {
    struct TackyFunction;
    struct TackyInstruction;
    struct TackyReturn;
    struct TackyUnary;
    struct TackyBinary;
    struct TackyCopy;
    struct Jump;
    struct JumpIfZero;
    struct JumpIfNotZero;
    struct Label;

    enum class TackyUnaryOp { COMPLEMENT, NEGATION, NOT};

    enum class TackyInstructionType {
        RET,
        UNARY,
        BINARY,
        COPY,
        JUMP,
        JUMP_IF_ZERO,
        JUMP_IF_NOT_ZERO,
        LABEL
    };

    enum class TackyBinaryOp {
        ADD,
        SUBTRACT,
        MULTIPLY,
        DIVIDE,
        REMAINDER,
        EQUAL,
        NOT_EQUAL,
        LESS_THAN,
        LESS_OR_EQUAL,
        GREATER_THAN,
        GREATER_OR_EQUAL
    };

    class TackyVisitor {
    public:
        /* this is for Assembly Gen, which takes this generated IR */
        virtual ~TackyVisitor() = default;
        virtual void visit(TackyReturn* node) = 0;
        virtual void visit(TackyUnary* node) = 0;
        virtual void visit(TackyBinary* node) = 0;
        virtual void visit(TackyCopy* node) = 0;
        virtual void visit(Jump* node) = 0;
        virtual void visit(JumpIfZero* node) = 0;
        virtual void visit(JumpIfNotZero* node) = 0;
        virtual void visit(Label* node) = 0;
    };

    /* need to think whether this should be virtual or not */
    struct TackyVar {
        std::string _identifier;
        explicit TackyVar(std::string identifier) : _identifier(identifier) {};
    };

    struct TackyConstant {
            int _val;
            explicit TackyConstant(int val) : _val(val) {};
    };

    using TackyVal = std::variant<std::monostate, TackyConstant, TackyVar>;


    struct TackyNode {
        virtual ~TackyNode() = default;
    };

    struct TackyProgram : public TackyNode {
        std::unique_ptr<TackyFunction> _function; 
        explicit TackyProgram(std::unique_ptr<TackyFunction> function) : _function(std::move(function)) {};
    };

    struct TackyFunction : public TackyNode {
        std::string _identifier;
        std::vector<std::unique_ptr<TackyInstruction>> _instructions;
        explicit TackyFunction(std::string identifier, std::vector<std::unique_ptr<TackyInstruction>> instructions) 
            : _identifier(identifier), _instructions(std::move(instructions)) {};

    };

    struct TackyInstruction : public TackyNode {
        virtual ~TackyInstruction() = default;
        virtual void accept(TackyVisitor& visitor) = 0;
    };

    struct TackyReturn : public TackyInstruction {
        TackyVal _val;
        TackyReturn(TackyVal val) : _val(val){};
        void accept(TackyVisitor& visitor) override { visitor.visit(this); } 
    };

    struct TackyUnary : public TackyInstruction {
        TackyUnaryOp _unary_op;
        TackyVal _src;
        TackyVal _dst;
        explicit TackyUnary(TackyUnaryOp unary_op, TackyVal src, TackyVal dst) :
            _unary_op(unary_op), _src(src), _dst(dst){};
        void accept(TackyVisitor& visitor) override { visitor.visit(this); }
    };

    struct TackyBinary : public TackyInstruction {
        TackyBinaryOp _binary_op;
        TackyVal _src1;
        TackyVal _src2;
        TackyVal _dst;
        explicit TackyBinary(TackyBinaryOp binary_op, TackyVal src1, TackyVal src2, TackyVal dst) :
            _binary_op(binary_op), _src1(src1), _src2(src2), _dst(dst) {};
        void accept(TackyVisitor& visitor) override { visitor.visit(this); }
    };

    struct TackyCopy : public TackyInstruction {
        TackyVal _src;
        TackyVal _dst;
        explicit TackyCopy(TackyVal src, TackyVal dst) : _src(src), _dst(dst) {};
        void accept(TackyVisitor& visitor) override { visitor.visit(this); }
    };

    struct Jump : public TackyInstruction {
        std::string _target_identifier;
        explicit Jump(std::string target_identifier) : _target_identifier(target_identifier) {};
        void accept(TackyVisitor& visitor) override { visitor.visit(this); }
    };

    struct JumpIfZero : public TackyInstruction {
        TackyVal _condition;
        std::string _target_identifier;
        explicit JumpIfZero(TackyVal condition, std::string target_identifier) : _condition(condition), _target_identifier(target_identifier){};
        void accept(TackyVisitor& visitor) override { visitor.visit(this); }
    };

    struct JumpIfNotZero : public TackyInstruction {
        TackyVal _condition;
        std::string _target_identifier;
        explicit JumpIfNotZero(TackyVal condition, std::string target_identifier) : _condition(condition), _target_identifier(target_identifier){};
        void accept(TackyVisitor& visitor) override { visitor.visit(this); }
    };

    struct Label : public TackyInstruction {
        std::string _identifier;
        explicit Label(std::string identifier) : _identifier(identifier) {};
        void accept(TackyVisitor& visitor) override { visitor.visit(this); }
    };

    class TackyGen : public dcc::ASTVisitor {
        private:
            size_t _tmp_counter = 0;
            size_t _label_counter = 0;
            TackyVal _current_val; /* shared memory space to pass the data back up the tree */
            std::vector<std::unique_ptr<TackyInstruction>> _instructions;

            std::string make_temporary_name() {
                return "tmp." + std::to_string(_tmp_counter++);
            }

            std::string make_label_name(std::string prefix = ".L") {
                // Or if you pass a prefix like "and_false", it returns "and_false0"
                return prefix + std::to_string(_label_counter++);
            }

            TackyUnaryOp convert_unop(dcc::UnaryOpType ast_op) {
                switch(ast_op) {
                    case dcc::UnaryOpType::Complement: return TackyUnaryOp::COMPLEMENT;
                    case dcc::UnaryOpType::Negate: return TackyUnaryOp::NEGATION;
                    case dcc::UnaryOpType::Not: return TackyUnaryOp::NOT;
                    default: break;
                }
                throw std::runtime_error("Unknown unary operator");
            }

            TackyBinaryOp convert_binop(dcc::BinaryOpType bin_op) {
                switch(bin_op) {
                    case dcc::BinaryOpType::Add: return TackyBinaryOp::ADD;
                    case dcc::BinaryOpType::Sub: return TackyBinaryOp::SUBTRACT;
                    case dcc::BinaryOpType::Mul: return TackyBinaryOp::MULTIPLY;
                    case dcc::BinaryOpType::Div: return TackyBinaryOp::DIVIDE;
                    case dcc::BinaryOpType::Rem: return TackyBinaryOp::REMAINDER;
                    case dcc::BinaryOpType::Equal: return TackyBinaryOp::EQUAL;
                    case dcc::BinaryOpType::NotEqual: return TackyBinaryOp::NOT_EQUAL;
                    case dcc::BinaryOpType::LessThan: return TackyBinaryOp::LESS_THAN;
                    case dcc::BinaryOpType::LessOrEqual: return TackyBinaryOp::LESS_OR_EQUAL;
                    case dcc::BinaryOpType::GreaterThan: return TackyBinaryOp::GREATER_THAN;
                    case dcc::BinaryOpType::GreaterOrEqual: return TackyBinaryOp::GREATER_OR_EQUAL;
                    default: break;
                }
                throw std::runtime_error("Unknown binary operator");
            }

        public:
            std::unique_ptr<TackyProgram> generate_tacky(dcc::Program* ast) {
                _instructions.clear();
                _tmp_counter = 0;
                _label_counter = 0;
                ast->accept(*this);

                auto* func = ast->_function_definition.get();                
                return std::make_unique<TackyProgram>(
                    std::make_unique<TackyFunction>(std::string(func->_identifier), std::move(_instructions))
                );
            }

            /* current val is updated in these two cases*/
            void visit(dcc::Constant * node) override {
                _current_val = TackyConstant{node->_value};
            }
            void visit(dcc::Var * node) override {
                _current_val = TackyVar(std::string(node->get_identifier()));
            }
            void visit(dcc::UnaryOperator* node) override;
            void visit(dcc::BinaryOperator* node) override;
            void visit(dcc::Assignment* node) override;
            void visit(dcc::Conditional* node) override;

            /* Statements and Declarations */
            void visit(dcc::ReturnStmt* node) override;
            void visit(dcc::IfStatement* node) override;
            void visit(dcc::ExpressionStmt* node) override;
            void visit(dcc::NullStmt* node) override {};
            void visit(dcc::DeclarationVariable* node) override;
            
            // Top-level
            void visit(dcc::Function* node) override;
            void visit(dcc::Program* node) override {
                if (node->_function_definition) {
                    node->_function_definition->accept(*this);
                }
            }

            void visit(dcc::CompoundStmt* node) override {
                node->_block->accept(*this);
            }

            void visit(dcc::Block* node) override {
                for (const auto& item : node->_block_items) {
                    item->accept(*this);
                }
            }
    };
}

#endif