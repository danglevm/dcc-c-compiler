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
    struct TackyProgram;

    enum class TackyUnaryOp {
        COMPLEMENT,
        NEGATION
    };

    enum class TackyInstructionType {
        RET,
        UNARY,
        BINARY
    };

    enum class TackyBinaryOp {
        ADD,
        SUBTRACT,
        MULTIPLY,
        DIVIDE,
        REMAINDER
    };

    /* need to think whether this should be virtual or not */
    struct TackyVar {
        std::string _identifier;
    };

    struct TackyConstant {
        int _val;
    };

    using TackyVal = std::variant<std::monostate, TackyConstant, TackyVar>;


    struct TackyNode {
        virtual ~TackyNode() = default;
    };

    struct TackyProgram : public TackyNode {
        std::unique_ptr<TackyFunction> _function; 
        TackyProgram(std::unique_ptr<TackyFunction> function) : _function(std::move(function)) {};
    };

    struct TackyFunction : public TackyNode {
        std::string _identifier;
        std::vector<std::unique_ptr<TackyInstruction>> _instructions;
        TackyFunction(std::string identifier, std::vector<std::unique_ptr<TackyInstruction>> instructions) 
            : _identifier(identifier), _instructions(std::move(instructions)) {};

    };

    struct TackyInstruction : public TackyNode {
        virtual ~TackyInstruction() = default;
        virtual TackyInstructionType get_instruction_type() const = 0;
    };

    struct TackyReturn : public TackyInstruction {
        TackyVal _val;
        TackyReturn(TackyVal val) : _val(val){}; 
        TackyInstructionType get_instruction_type() const { return TackyInstructionType::RET; }
    };

    struct TackyUnary : public TackyInstruction {
        TackyUnaryOp _unary_op;
        TackyVal _src;
        TackyVal _dst;
        TackyUnary(TackyUnaryOp unary_op, TackyVal src, TackyVal dst) :
            _unary_op(unary_op), _src(src), _dst(dst){};
        TackyInstructionType get_instruction_type() const { return TackyInstructionType::UNARY; }

    };

    struct TackyBinary : public TackyInstruction {
        TackyBinaryOp _binary_op;
        TackyVal _src1;
        TackyVal _src2;
        TackyVal _dst;
        TackyBinary(TackyBinaryOp binary_op, TackyVal src1, TackyVal src2, TackyVal dst) :
            _binary_op(binary_op), _src1(src1), _src2(src2), _dst(dst) {};
        TackyInstructionType get_instruction_type() const { return TackyInstructionType::BINARY; }
    };

    class TackyGen {
        private:
            size_t _tmp_counter = 0;
            std::vector<std::unique_ptr<TackyInstruction>> _instructions;

            std::string make_temporary_name() {
                return "tmp." + std::to_string(_tmp_counter++);
            }

            TackyUnaryOp convert_unop(char ast_op) {
                switch(ast_op) {
                    case '~': return TackyUnaryOp::COMPLEMENT;
                    case '-': return TackyUnaryOp::NEGATION;
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
                }
                throw std::runtime_error("Unknown binary operator");
            }

            TackyVal emit_tacky(dcc::Expr * expr);

        public:
            std::unique_ptr<TackyProgram> generate_tacky(const dcc::Program* ast) {
                const auto* func = ast->_function_definition.get();
                auto* stmt = func->_stmt.get();
                std::string identifier = std::string{func->_identifier};

                TackyVal ret_val = emit_tacky(stmt->get_expr());

                /* add the final return instruction */
                _instructions.push_back(std::make_unique<TackyReturn>(ret_val));
                auto tacky_func = std::make_unique<TackyFunction>(identifier, std::move(_instructions));
                return std::make_unique<tacky::TackyProgram>(std::move(tacky_func));
            }
    };
}

#endif