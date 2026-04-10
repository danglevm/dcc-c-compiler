#ifndef DCC_ASM_HPP
#define DCC_ASM_HPP

#include <string>
#include <iostream>
#include <memory>
#include <vector>

#include <libdcc/parser.hpp>
#include <libdcc/tacky.hpp>


/*
Intermediate Representation in Assembly
program = Program(function_definition) 
function_definition = Function(identifier name, instruction* instructions) 
instruction = Mov(operand src, operand dst) | Ret 
operand = Imm(int) | Register

*/

namespace assembly {
    struct IRProgram;
    struct IRFunction;

    enum class UnaryOpASM { 
        NOT,
        NEG
    };

    enum class RegASM {
        AX,
        R10
    };

    struct ASMNode {
        virtual ~ASMNode() = default;

        /* for pretty printing */
        virtual std::string code_gen() const = 0;
    };

    struct Operand : public ASMNode {
        virtual ~Operand() = default;
    };

    struct Immediate : public Operand {
        int _val = 0;
        Immediate(const int val) :_val(val) {};

        std::string code_gen() const override {
            return "$" + std::to_string(_val); /* x86 imm syntax */
        }
    };

    struct Register : public Operand {
        RegASM _reg;
        Register(RegASM reg) : _reg(reg){};
        std::string code_gen() const override {
             /* x86 reg syntax */
            switch(_reg) {
                case RegASM::AX: return "%eax";
                case RegASM::R10: return "%r10d";
            };
            
            return " ";
        }
    };

    struct Pseudo : public Operand {
        std::string _identifier;
        std::string code_gen() const override {
            
        };
    };

    struct Stack : public Operand {
        int _val = 0;
        Stack(int val) : _val(val){};
        std::string code_gen() const override {
            return std::to_string(_val) + "(%rbp)";
        }
    };

    struct Instruction : public ASMNode {
        virtual ~Instruction() = default;
    };

    struct Mov : public Instruction {
        std::unique_ptr<Immediate> _expr;
        std::unique_ptr<Register> _reg;
        Mov(std::unique_ptr<Immediate> expr, std::unique_ptr<Register> reg) 
            : _expr(std::move(expr)), _reg(std::move(reg)) {}
        std::string code_gen() const override {
        // x86 syntax: movl src, dst
            return "    movl " + _expr->code_gen() + ", " + _reg->code_gen() + "\n";
        }
    };

    struct Ret : public Instruction {
        std::string code_gen() const override {
            return "    ret\n";
        }
    };

    struct UnaryASM : public Instruction {
        UnaryOpASM _unary_op;
        std::unique_ptr<Register> _reg;

        std::string code_gen() const override {
            switch (_unary_op) {
                case UnaryOpASM::NOT: return "    notl " + _reg->code_gen() + "\n";
                case UnaryOpASM::NEG: return "    negl " + _reg->code_gen() + "\n";
            };
        }
    };

    struct AllocateStackASM : public Instruction {
        int _val = 0;
        AllocateStackASM(int val) : _val(val) {};
        std::string code_gen() const override {
            return "    subq $" + std::to_string(_val) + ", %rsp\n";
        }
    };

    struct IRFunction : public ASMNode {
        int _stack_size = 0;
        std::string _name;
        std::vector<std::unique_ptr<Instruction>> _instructions;
        IRFunction(std::string name, std::vector<std::unique_ptr<Instruction>>& instructions) 
            : _name(name), _instructions(std::move(instructions)) {};

        std::string code_gen() const override {
            std::string out = ".globl " + _name + "\n";
            out += _name + ":\n";
            for (const auto& inst : _instructions) {
                out += inst->code_gen();
            }
            return out;
        }
    };

    struct IRProgram : public ASMNode {
        std::unique_ptr<IRFunction> _function; 
        IRProgram(std::unique_ptr<IRFunction> function) : _function(std::move(function)) {};
        std::string code_gen() const override {
            if (_function) { 
                return _function->code_gen();
            }
            return "";
        }
    };

    /*
    TackyProgram                  IRProgram
 ├── TackyFunction("main") ──► ├── IRFunction("main")
 ├── TackyFunction("foo")  ──► ├── IRFunction("foo")
 └── TackyFunction("bar")  ──► └── IRFunction("bar")
    */

    class CodeGen {
        public:
            std::unique_ptr<IRProgram> generate (const tacky::TackyProgram * tacky_program) {
                const auto * tacky_func = tacky_program->_function.get();
                auto func = generate_function(tacky_func);
                return std::make_unique<IRProgram>(func);
            }

        private:
            std::unique_ptr<IRFunction> generate_function(const tacky::TackyFunction * tacky_func) {
                /* chapter 1 */
                auto tacky_instructions = tacky_func->_instructions;
                std::vector<std::unique_ptr<Instruction>> asm_instructions;
                for (const auto& instr : tacky_instructions) {
                    switch (instr.get()->get_instruction_type()) {
                        if (instr->get_instruction_type() == tacky::TackyInstructionType::RET) {
                            auto* ret_inst = static_cast<tacky::TackyReturn*>(instr.get());
                            // TACKY: Return(val)
                            // ASM:   Mov(val, %eax) -> Ret
                            asm_instructions.push_back(std::make_unique<Mov>(
                                convert_val(ret_inst->_val),
                                std::make_unique<Register>(RegASM::AX) // Return values must go in AX
                            ));
                            asm_instructions.push_back(std::make_unique<Ret>());
                        } else if (instr->get_instruction_type() == tacky::TackyInstructionType::UNARY) {
                            auto* unary_inst = static_cast<tacky::TackyUnary*>(instr.get());
                            asm_instructions.push_back(std::make_unique<Mov>(
                                convert_val(unary_inst->_src), 
                                convert_val(unary_inst->_dst)
                            ));
                            asm_instructions.push_back(std::make_unique<UnaryASM>(
                                convert_op(unary_inst->_unary_op), 
                                convert_val(unary_inst->_dst)
                            ));
                        }
                    }
                }
                return std::make_unique<IRFunction>(tacky_func->_identifier, std::move(asm_instructions));      
            }

            std::unique_ptr<Operand> convert_val(const tacky::TackyVal& val) {
                if (std::holds_alternative<tacky::TackyConstant>(val)) {
                    int v = std::get<tacky::TackyConstant>(val)._val;
                    return std::make_unique<Immediate>(v);
                } else if (std::holds_alternative<tacky::TackyVar>(val)) {
                    std::string id = std::get<tacky::TackyVar>(val)._identifier;
                    return std::make_unique<Pseudo>(id); // Create a Pseudo register!
                }
                throw std::runtime_error("Unknown TackyVal");
            }

            UnaryOpASM convert_op(const tacky::TackyUnaryOp& val) {
                if (val == tacky::TackyUnaryOp::COMPLEMENT) {
                    return UnaryOpASM::NOT;
                } else if (val == tacky::TackyUnaryOp::NEGATION) {
                    return UnaryOpASM::NEG;
                }
                throw std::runtime_error("Unknown TackyUnaryOp");
            }
    };

    class PseudoReplacer {
        public:
            void replace_pseudos(assembly::IRFunction* func) {
                int offset = 0;
                std::unordered_map<std::string, int> offset_map;
                for (auto& instr : func->_instructions) {

                }
                func->_stack_size = -offset;
            }
        private:
    }
    
}

#endif