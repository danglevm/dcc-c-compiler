#include <libdcc/asm.hpp>

#include <string>
#include <iostream>
#include <memory>
#include <vector>


namespace assembly {

    inline CondCode convert_to_condition_code(tacky::TackyBinaryOp op) {
        switch (op) {
            case tacky::TackyBinaryOp::EQUAL:
                return CondCode::E;  // Equal (ZF = 1)
                
            case tacky::TackyBinaryOp::NOT_EQUAL:
                return CondCode::NE; // Not Equal (ZF = 0)
                
            case tacky::TackyBinaryOp::LESS_THAN:
                return CondCode::L;  // Less (SF != OF)
                
            case tacky::TackyBinaryOp::LESS_OR_EQUAL:
                return CondCode::LE; // Less or Equal (ZF = 1 or SF != OF)
                
            case tacky::TackyBinaryOp::GREATER_THAN:
                return CondCode::G;  // Greater (ZF = 0 and SF = OF)
                
            case tacky::TackyBinaryOp::GREATER_OR_EQUAL:
                return CondCode::GE; // Greater or Equal (SF = OF)
                
            default:
                throw std::runtime_error("Invalid operator passed to convert_condition_code: not a relational operator.");
        }
    }

    inline bool is_relational_op(tacky::TackyBinaryOp op) {
        switch (op) {
            case tacky::TackyBinaryOp::EQUAL:
            case tacky::TackyBinaryOp::NOT_EQUAL:
            case tacky::TackyBinaryOp::LESS_THAN:
            case tacky::TackyBinaryOp::LESS_OR_EQUAL:
            case tacky::TackyBinaryOp::GREATER_THAN:
            case tacky::TackyBinaryOp::GREATER_OR_EQUAL:
                return true;
            default:
                return false;
        }
    }

    void CodeGen::visit(tacky::TackyReturn* node) {
        asm_instructions.push_back(std::make_unique<Mov>(
            convert_val(node->_val),
            std::make_unique<Register>(RegASM::AX) // Return values must go in AX
        ));
        asm_instructions.push_back(std::make_unique<Ret>());
    }

    void CodeGen::visit(tacky::TackyUnary* node) {
        if (node->_unary_op == tacky::TackyUnaryOp::NOT) {
            asm_instructions.push_back(std::make_unique<Cmp>(
                std::make_unique<Immediate>(0),                        
                convert_val(node->_src)                    
            ));

            asm_instructions.push_back(std::make_unique<Mov>(
                std::make_unique<Immediate>(0),
                convert_val(node->_dst)
            ));

            asm_instructions.push_back(std::make_unique<SetCC>(
                assembly::CondCode::E,
                convert_val(node->_dst)
            ));

        } else {
            asm_instructions.push_back(std::make_unique<Mov>(
                convert_val(node->_src), 
                convert_val(node->_dst)
            ));
            asm_instructions.push_back(std::make_unique<UnaryASM>(
                convert_op(node->_unary_op), 
                convert_val(node->_dst)
            ));
        }
    }

    void CodeGen::visit(tacky::TackyBinary* node) {
        if (node->_binary_op == tacky::TackyBinaryOp::DIVIDE || node->_binary_op == tacky::TackyBinaryOp::REMAINDER) {
            asm_instructions.push_back(std::make_unique<Mov>(
                convert_val(node->_src1), 
                std::make_unique<Register>(RegASM::AX)
            ));
            asm_instructions.push_back(std::make_unique<Cdq>());
            asm_instructions.push_back(std::make_unique<Idiv>(convert_val(node->_src2)));

            RegASM asm_reg = (node->_binary_op == tacky::TackyBinaryOp::DIVIDE) ? RegASM::AX : RegASM::DX; 

            asm_instructions.push_back(std::make_unique<Mov>(
                std::make_unique<Register>(asm_reg),
                convert_val(node->_dst)
            ));

        } else if (is_relational_op(node->_binary_op)) {
            asm_instructions.push_back(std::make_unique<Cmp>(
                convert_val(node->_src2),
                convert_val(node->_src1)
            ));

            asm_instructions.push_back(std::make_unique<Mov>(
                std::make_unique<Immediate>(0),
                convert_val(node->_dst)
            ));                     
        
            asm_instructions.push_back(std::make_unique<SetCC>(
                convert_to_condition_code(node->_binary_op),
                convert_val(node->_dst)
            ));
        } else {
            /* binary instruction with binary_operator, src2, dst - existing arithmetic logic */
            asm_instructions.push_back(std::make_unique<Mov>( 
                convert_val(node->_src1),
                convert_val(node->_dst)
            ));
            asm_instructions.push_back(std::make_unique<BinaryASM>(
                convert_op(node->_binary_op),
                convert_val(node->_src2),
                convert_val(node->_dst)
            ));
        }
    }

    void CodeGen::visit(tacky::Jump* node) {
        asm_instructions.push_back(std::make_unique<Jmp>(node->_target_identifier));
    }

    void CodeGen::visit(tacky::JumpIfZero* node) {
        asm_instructions.push_back(std::make_unique<Cmp>(
            std::make_unique<Immediate>(0),
            convert_val(node->_condition)
        ));

        asm_instructions.push_back(std::make_unique<JmpCC>(
            CondCode::E,
            node->_target_identifier
        ));
    }

    void CodeGen::visit(tacky::JumpIfNotZero* node) {
        asm_instructions.push_back(std::make_unique<Cmp>(
            std::make_unique<Immediate>(0),
            convert_val(node->_condition)
        ));

        asm_instructions.push_back(std::make_unique<JmpCC>(
            CondCode::NE,
            node->_target_identifier
        ));
    }

    void CodeGen::visit(tacky::TackyCopy* node) {
        asm_instructions.push_back(std::make_unique<Mov>(
            convert_val(node->_src),
            convert_val(node->_dst)
        ));
    }

    void CodeGen::visit(tacky::Label* node) {
        asm_instructions.push_back(std::make_unique<Label>(
            node->_identifier
        ));
    }

    std::unique_ptr<IRFunction> CodeGen::generate_function(const tacky::TackyFunction * tacky_func) {
        asm_instructions.clear();

        for (const auto& instr : tacky_func->_instructions) {
            instr->accept(*this);
        }

        int cur_offset = 0;
        std::unordered_map<std::string, int> offset_map;
        for (auto& inst : asm_instructions) {
            inst->allocate_pseudo(cur_offset, offset_map);
        }
        return std::make_unique<IRFunction>(tacky_func->_identifier, std::move(asm_instructions));     
    }

    std::unique_ptr<Operand> CodeGen::convert_val(const tacky::TackyVal& val) {
        if (std::holds_alternative<tacky::TackyConstant>(val)) {
            int v = std::get<tacky::TackyConstant>(val)._val;
            return std::make_unique<Immediate>(v);
        } else if (std::holds_alternative<tacky::TackyVar>(val)) {
            std::string id = std::get<tacky::TackyVar>(val)._identifier;
            return std::make_unique<Pseudo>(id);
        }
        throw std::runtime_error("Unknown TackyVal");
    }
    
}