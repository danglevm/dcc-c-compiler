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

    enum class BinaryOpASM {
        ADD,
        SUB,
        MULT
    };

    enum class RegASM {
        AX,
        DX,
        R10,
        R11
    };

    enum class OperandASM {
        IMM,
        REG,
        PSEUDO,
        STACK
    };

    enum class CondCode {
        E,
        NE,
        G,
        GE,
        L,
        LE
    };

    inline std::string cc_to_string(CondCode cc) {
        switch (cc) {
            case CondCode::E:  return "e";
            case CondCode::NE: return "ne";
            case CondCode::G:  return "g";
            case CondCode::GE: return "ge";
            case CondCode::L:  return "l";
            case CondCode::LE: return "le";
        }
        return "";
    }

    struct ASMNode {
        virtual ~ASMNode() = default;

        /* for pretty printing - standard 32-bit 4 bytes*/
        virtual std::string code_gen() const { return code_gen(4); }

        virtual std::string code_gen(int size) const = 0;
    };

    struct Operand : public ASMNode {
        virtual ~Operand() = default;

        virtual OperandASM getType() const = 0;        /* offset map for mapping temp variables back to their offset */
        virtual std::unique_ptr<Operand> replace_pseudo (
            int& current_offset,
            std::unordered_map<std::string, int>& offset_map
        ) {
            return nullptr;
        }
    };

    struct Immediate : public Operand {
        int _val = 0;
        explicit Immediate(const int val) :_val(val) {};

        OperandASM getType() const override { return OperandASM::IMM; }

        std::string code_gen(int size = 4) const override {
            return "$" + std::to_string(_val); /* x86 imm syntax */
        }
    };

    struct Register : public Operand {
        RegASM _reg;
        explicit Register(RegASM reg) : _reg(reg){};
        OperandASM getType() const override { return OperandASM::REG; }

        std::string code_gen(int size = 4) const override {
            if (size == 1) {
                switch(_reg) {
                    case RegASM::AX:  return "%al";
                    case RegASM::DX:  return "%dl";
                    case RegASM::R10: return "%r10b";
                    case RegASM::R11: return "%r11b"; 
                    default: throw std::runtime_error("Invalid register for 1-byte access");
                }
            } else { 
                switch(_reg) {
                    case RegASM::AX:  return "%eax";
                    case RegASM::DX:  return "%edx";
                    case RegASM::R10: return "%r10d";
                    case RegASM::R11: return "%r11d";
                }
            }
            return " ";
        }
    };

    struct Stack : public Operand {
        int _val = 0;
        explicit Stack(int val) : _val(val){};
        OperandASM getType() const override { return OperandASM::STACK; }

        std::string code_gen(int size = 4) const override {
            return std::to_string(_val) + "(%rbp)";
        }
    };

    struct Pseudo : public Operand {
        std::string _identifier;
        explicit Pseudo(std::string id) : _identifier(std::move(id)) {}
        OperandASM getType() const override { return OperandASM::PSEUDO; }
        std::string code_gen(int size = 4) const override {
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
        explicit Mov(std::unique_ptr<Operand> src, std::unique_ptr<Operand> dest) 
            : _src(std::move(src)), _dest(std::move(dest)) {}
        std::string code_gen(int size = 4) const override {
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
        std::string code_gen(int size = 4) const override {
            std::string out = "    movq  %rbp, %rsp\n";
            out += "    popq  %rbp\n";
            out += "    ret\n";
            return out;
        }

        void allocate_pseudo(int& current_offset, std::unordered_map<std::string, int>& offset_map) override {}    
    };

    struct UnaryASM : public Instruction {
        UnaryOpASM _unary_op;
        std::unique_ptr<Operand> _operand;

        explicit UnaryASM(UnaryOpASM unary_op, std::unique_ptr<Operand> operand) : _unary_op(unary_op), _operand(std::move(operand)) {}

        std::string code_gen(int size = 4) const override { 
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

    struct BinaryASM : public Instruction {
        BinaryOpASM _binary_op;
        std::unique_ptr<Operand> _op1;
        std::unique_ptr<Operand> _op2;

        explicit BinaryASM(BinaryOpASM binary_op, std::unique_ptr<Operand> op1, std::unique_ptr<Operand> op2) : 
                _binary_op(binary_op), _op1(std::move(op1)), _op2(std::move(op2)) {};
        
        std::string code_gen(int size = 4) const override {
            // Imul destination cannot be memory so route through %r11d.
            if (_binary_op == BinaryOpASM::MULT && _op2->getType() == OperandASM::STACK) {
                return "    movl " + _op2->code_gen() + ", %r11d\n"
                       "    imull " + _op1->code_gen() + ", %r11d\n"
                       "    movl %r11d, " + _op2->code_gen() + "\n";
            }

            if ((_binary_op == BinaryOpASM::ADD || _binary_op == BinaryOpASM::SUB) && 
                _op1->getType() == OperandASM::STACK && _op2->getType() == OperandASM::STACK) {
                std::string op_str = (_binary_op == BinaryOpASM::ADD) ? "addl" : "subl";
            
                //Add/Sub cannot have memory for BOTH source and destination so route through %r10d.
                return "    movl " + _op1->code_gen() + ", %r10d\n"
                       "    " + op_str + " %r10d, " + _op2->code_gen() + "\n";
            }

            /* standard behavior*/
            switch (_binary_op) {
                case BinaryOpASM::ADD:  return "    addl " + _op1->code_gen()+ "," + _op2->code_gen() + "\n";
                case BinaryOpASM::SUB:  return "    subl " + _op1->code_gen()+ "," + _op2->code_gen() + "\n";                
                case BinaryOpASM::MULT: return "    imull " + _op1->code_gen()+ "," + _op2->code_gen() + "\n";
            }
            return "";
        }

        void allocate_pseudo(int& current_offset, std::unordered_map<std::string, int>& offset_map) override { 
            if (auto new_op1 = _op1->replace_pseudo(current_offset, offset_map)) {
                _op1 = std::move(new_op1);
            }

            if (auto new_op2 = _op2->replace_pseudo(current_offset, offset_map)) {
                _op2 = std::move(new_op2);
            }
        }
    };

    struct Cmp : public Instruction {
        std::unique_ptr<Operand> _op1;
        std::unique_ptr<Operand> _op2;

        explicit Cmp(std::unique_ptr<Operand> op1, std::unique_ptr<Operand> op2) 
            : _op1(std::move(op1)), _op2(std::move(op2)) {};
        std::string code_gen(int size = 4) const override {
            if (_op1->getType() == OperandASM::STACK && _op2->getType() == OperandASM::STACK) {
                return "    movl " + _op1->code_gen() + ", %r10d\n"
                       "    cmpl %r10d, " + _op2->code_gen() + "\n";
            }

            if (_op2->getType() == OperandASM::IMM) {
                return "    movl " + _op2->code_gen() + ", %r11d\n"
                       "    cmpl " + _op1->code_gen() + ", %r11d\n";
            }

            return "    cmpl " + _op1->code_gen() + ", " + _op2->code_gen() + "\n";
        }
        
        void allocate_pseudo(int& current_offset, std::unordered_map<std::string, int>& offset_map) override {
            if (auto new_op1 = _op1->replace_pseudo(current_offset, offset_map)) {
                _op1 = std::move(new_op1);
            }

            if (auto new_op2 = _op2->replace_pseudo(current_offset, offset_map)) {
                _op2 = std::move(new_op2);
            }
        }    
    };

    struct Jmp : public Instruction {
        std::string _identifier;

        explicit Jmp(std::string identifier) : _identifier(identifier) {};
        void allocate_pseudo(int& current_offset, std::unordered_map<std::string, int>& offset_map) override {}  
        std::string code_gen(int size = 4) const override {
            return "    jmp .L" + _identifier + "\n";
        }  
    };

    struct JmpCC : public Instruction {
        CondCode _cond_code;
        std::string _identifier;

        explicit JmpCC(CondCode cc, std::string identifier) : _cond_code(cc), _identifier(identifier) {};
        void allocate_pseudo(int& current_offset, std::unordered_map<std::string, int>& offset_map) override {}  
        std::string code_gen(int size = 4) const override {
            return "    j" + cc_to_string(_cond_code) + " .L" + _identifier + "\n";
        }  
    };

    struct SetCC : public Instruction {
        CondCode _cond_code;
        std::unique_ptr<Operand> _op;

        explicit SetCC(CondCode cc, std::unique_ptr<Operand> op) : _cond_code(cc), _op(std::move(op)) {};
        void allocate_pseudo(int& current_offset, std::unordered_map<std::string, int>& offset_map) override {
            if (auto new_op = _op->replace_pseudo(current_offset, offset_map)) {
                _op = std::move(new_op);
            }
        }  

        std::string code_gen(int size = 4) const override {
            /* explicitly requests 1-byte representation */
            return "    set" + cc_to_string(_cond_code) + " " + _op->code_gen(1) + "\n";
        }  
    };

    struct Label : public Instruction {
        std::string _identifier;
        explicit Label(std::string identifier) : _identifier(identifier) {};

        void allocate_pseudo(int& current_offset, std::unordered_map<std::string, int>& offset_map) override {}    
        std::string code_gen(int size = 4) const override {
            return ".L" + _identifier + ":\n";
        } 
    };

    struct Idiv : public Instruction {
        std::unique_ptr<Operand> _op;

        explicit Idiv(std::unique_ptr<Operand> op) : _op(std::move(op)) {};

        std::string code_gen(int size = 4) const override {
            if (_op->getType() == OperandASM::IMM) {
                /* operand is constant (Immediate), route through %r10d */
                return "    movl " + _op->code_gen() + ", %r10d\n"
                       "    idivl %r10d\n";
            }
            return "    idivl " + _op->code_gen() + "\n";
        }

        void allocate_pseudo(int& current_offset, std::unordered_map<std::string, int>& offset_map) override { 
            if (auto new_op = _op->replace_pseudo(current_offset, offset_map)) {
                _op = std::move(new_op);
            }
        }
    };

    /* sign extension */
    struct Cdq : public Instruction {
        std::string code_gen(int size = 4) const override {
            return "    cdq\n";
        }

        void allocate_pseudo(int& current_offset, std::unordered_map<std::string, int>& offset_map) override { }
    };

    struct AllocateStackASM : public Instruction {
        int _val = 0;
        explicit AllocateStackASM(int val) : _val(val) {};
        std::string code_gen(int size = 4) const override {
            return "    subq $" + std::to_string(_val) + ", %rsp\n";
        }
    };

    struct IRFunction : public ASMNode {
        int _stack_size = 0;
        std::string _name;
        std::vector<std::unique_ptr<Instruction>> _instructions;
        explicit IRFunction(std::string name, std::vector<std::unique_ptr<Instruction>> instructions) 
            : _name(name), _instructions(std::move(instructions)) {};

        std::string code_gen(int size = 4) const override {
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
        explicit IRProgram(std::unique_ptr<IRFunction> function) : _function(std::move(function)) {};
        std::string code_gen(int size = 4) const override {
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

    class CodeGen : public tacky::TackyVisitor {
        public:
            std::unique_ptr<IRProgram> generate (const tacky::TackyProgram * tacky_program) {
                const auto * tacky_func = tacky_program->_function.get();
                auto func = generate_function(tacky_func);
                return std::make_unique<IRProgram>(std::move(func));
            }

        private:
            std::vector<std::unique_ptr<Instruction>> asm_instructions;

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

            BinaryOpASM convert_op(const tacky::TackyBinaryOp& val) {
                if (val == tacky::TackyBinaryOp::ADD) {
                    return BinaryOpASM::ADD;
                } else if (val == tacky::TackyBinaryOp::MULTIPLY) {
                    return BinaryOpASM::MULT;
                } else if (val == tacky::TackyBinaryOp::SUBTRACT) {
                    return BinaryOpASM::SUB;
                }
                throw std::runtime_error("Unknown TackyBinaryOp");
             }

            void visit(tacky::TackyReturn* node) override;
            void visit(tacky::TackyUnary* node) override;
            void visit(tacky::TackyBinary* node) override;
            void visit(tacky::Jump* node) override;
            void visit(tacky::JumpIfZero* node) override;
            void visit(tacky::JumpIfNotZero* node) override;
            void visit(tacky::TackyCopy* node) override;
            void visit(tacky::Label* node) override;
    };
    
}

#endif