#ifndef DCC_ASM_HPP
#define DCC_ASM_HPP

#include <string>
#include <iostream>
#include <memory>
#include <vector>
#include <unordered_map>

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

    enum class OperandASM {
        IMM,
        REG,
        PSEUDO,
        STACK
    };

    struct ASMNode {
        virtual ~ASMNode() = default;

        /* for pretty printing */
        virtual std::string code_gen() const = 0;
    };

    struct Operand : public ASMNode {
        virtual ~Operand() = default;

        virtual OperandASM getType() const = 0;
        /* offset map for mapping temp variables back to their offset */
        virtual std::unique_ptr<Operand> replace_pseudo (
            int& current_offset,
            std::unordered_map<std::string, int>& offset_map
        ) {
            return nullptr;
        }
    };

    struct Immediate : public Operand {
        int _val = 0;
        Immediate(const int val) :_val(val) {};

        OperandASM getType() const override { return OperandASM::IMM; }

        std::string code_gen() const override {
            return "$" + std::to_string(_val); /* x86 imm syntax */
        }
    };

    struct Register : public Operand {
        RegASM _reg;
        Register(RegASM reg) : _reg(reg){};
        OperandASM getType() const override { return OperandASM::REG; }

        std::string code_gen() const override {
             /* x86 reg syntax */
            switch(_reg) {
                case RegASM::AX: return "%eax";
                case RegASM::R10: return "%r10d";
            };
            
            return " ";
        }
    };

    struct Stack : public Operand {
        int _val = 0;
        Stack(int val) : _val(val){};
        OperandASM getType() const override { return OperandASM::STACK; }

        std::string code_gen() const override {
            return std::to_string(_val) + "(%rbp)";
        }
    };

    struct Pseudo : public Operand {
        std::string _identifier;
        Pseudo(std::string id) : _identifier(std::move(id)) {}
        OperandASM getType() const override { return OperandASM::PSEUDO; }
        std::string code_gen() const override {
            return "";
        };

        /* if source is pseudo then we replace it */
        std::unique_ptr<Operand> replace_pseudo(
            int& current_offset,
            std::unordered_map<std::string, int>& offset_map
        ) override {
            if (offset_map.find(_identifier) ==  offset_map.end()) {
                current_offset -= 4;
                offset_map[_identifier] = current_offset; 
            }

            return std::make_unique<Stack>(offset_map[_identifier]);
        }
    };

    struct Instruction : public ASMNode {
        virtual ~Instruction() = default;
        virtual void allocate_pseudo(
            int& current_offset,
            std::unordered_map<std::string, int>& offset_map) = 0;
    };

    /* can move Register to Stack or Immediate to Stack */
    struct Mov : public Instruction {
        std::unique_ptr<Operand> _src;
        std::unique_ptr<Operand> _dest;
        Mov(std::unique_ptr<Operand> src, std::unique_ptr<Operand> dest) 
            : _src(std::move(src)), _dest(std::move(dest)) {}
        std::string code_gen() const override {
            /* if they are both memory addresses, we need to route through %r10d register */
            if (_src->getType() == OperandASM::STACK && _dest->getType() == OperandASM::STACK) {
                return "    movl " + _src->code_gen() + ", %r10d\n"
                "    movl %r10d, " + _dest->code_gen() + "\n";
            }

        // x86 syntax: movl src, dst
            return "    movl " + _src->code_gen() + ", " + _dest->code_gen() + "\n";
        }
        
        void allocate_pseudo(int& current_offset, std::unordered_map<std::string, int>& offset_map) override {
            if (auto src_new = _src->replace_pseudo(current_offset, offset_map)) {
                _src = std::move(src_new);
            }

            if (auto dest_new = _dest->replace_pseudo(current_offset, offset_map)) {
                _dest = std::move(dest_new);
            }
        } 

    };

    struct Ret : public Instruction {
        std::string code_gen() const override {
            std::string out = "    movq  %rbp, %rsp\n";
            out += "    popq  %rbp\n";
            out += "    ret\n";
            return out;
        }

    void allocate_pseudo(int& current_offset, std::unordered_map<std::string, int>& offset_map) override {}    };

    struct UnaryASM : public Instruction {
        UnaryOpASM _unary_op;
        std::unique_ptr<Operand> _operand;

        UnaryASM(UnaryOpASM unary_op, std::unique_ptr<Operand> operand) : _unary_op(unary_op), _operand(std::move(operand)) {}

        std::string code_gen() const override { 
            switch (_unary_op) {
                case UnaryOpASM::NOT: return "    notl " + _operand->code_gen() + "\n";
                case UnaryOpASM::NEG: return "    negl " + _operand->code_gen() + "\n";
            };

            return "";
        }

        void allocate_pseudo(int& current_offset, std::unordered_map<std::string, int>& offset_map) override {
            if (auto new_op = _operand->replace_pseudo(current_offset, offset_map)) {
                _operand = std::move(new_op);
            }
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
        IRFunction(std::string name, std::vector<std::unique_ptr<Instruction>> instructions) 
            : _name(name), _instructions(std::move(instructions)) {};

        std::string code_gen() const override {
            std::string out = ".globl " + _name + "\n";
            out += _name + ":\n";
            out += "    pushq %rbp\n";
            out += "    movq  %rsp, %rbp\n";
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
                return std::make_unique<IRProgram>(std::move(func));
            }

        private:
            std::unique_ptr<IRFunction> generate_function(const tacky::TackyFunction * tacky_func);

            std::unique_ptr<Operand> convert_val(const tacky::TackyVal& val);

            UnaryOpASM convert_op(const tacky::TackyUnaryOp& val) {
                if (val == tacky::TackyUnaryOp::COMPLEMENT) {
                    return UnaryOpASM::NOT;
                } else if (val == tacky::TackyUnaryOp::NEGATION) {
                    return UnaryOpASM::NEG;
                }
                throw std::runtime_error("Unknown TackyUnaryOp");
            }
    };
    
}

#endif