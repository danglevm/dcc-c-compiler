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

    std::unique_ptr<IRFunction> CodeGen::generate_function(const tacky::TackyFunction * tacky_func) {
        auto& tacky_instructions = tacky_func->_instructions;
        std::vector<std::unique_ptr<Instruction>> asm_instructions;
        for (const auto& instr : tacky_instructions) {
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

                /* !x is equivalent to x == 0 */
                if (unary_inst->_unary_op == tacky::TackyUnaryOp::NOT) {
                    asm_instructions.push_back(std::make_unique<Cmp>(
                        std::make_unique<Immediate>(0),                        
                        convert_val(unary_inst->_src)                    
                    ));

                    asm_instructions.push_back(std::make_unique<Mov>(
                        std::make_unique<Immediate>(0),
                        convert_val(unary_inst->_dst)
                    ));

                    asm_instructions.push_back(std::make_unique<SetCC>(
                        assembly::CondCode::E,
                        convert_val(unary_inst->_dst)
                    ));

                } else {
                    asm_instructions.push_back(std::make_unique<Mov>(
                        convert_val(unary_inst->_src), 
                        convert_val(unary_inst->_dst)
                    ));
                    asm_instructions.push_back(std::make_unique<UnaryASM>(
                        convert_op(unary_inst->_unary_op), 
                        convert_val(unary_inst->_dst)
                    ));
                }
        
            } else if (instr->get_instruction_type() == tacky::TackyInstructionType::BINARY) {
                auto* binary_inst = static_cast<tacky::TackyBinary*>(instr.get());
                if (binary_inst->_binary_op == tacky::TackyBinaryOp::DIVIDE || binary_inst->_binary_op == tacky::TackyBinaryOp::REMAINDER) {
                    asm_instructions.push_back(std::make_unique<Mov>(
                        convert_val(binary_inst->_src1), 
                        std::make_unique<Register>(RegASM::AX)
                    ));
                    asm_instructions.push_back(std::make_unique<Cdq>());
                    asm_instructions.push_back(std::make_unique<Idiv>(convert_val(binary_inst->_src2)));

                    RegASM asm_reg = (binary_inst->_binary_op == tacky::TackyBinaryOp::DIVIDE) ? RegASM::AX : RegASM::DX; 

                    asm_instructions.push_back(std::make_unique<Mov>(
                        std::make_unique<Register>(asm_reg),
                        convert_val(binary_inst->_dst)
                    ));

                } else if (is_relational_op(binary_inst->_binary_op)) {
                    asm_instructions.push_back(std::make_unique<Cmp>(
                        convert_val(binary_inst->_src2),
                        convert_val(binary_inst->_src1)
                    ));

                    asm_instructions.push_back(std::make_unique<Mov>(
                        std::make_unique<Immediate>(0),
                        convert_val(binary_inst->_dst)
                    ));                     
                
                    asm_instructions.push_back(std::make_unique<SetCC>(
                        convert_to_condition_code(binary_inst->_binary_op),
                        convert_val(binary_inst->_dst)
                    ));
                } else {
                    /* binary instruction with binary_operator, src2, dst - existing arithmetic logic */
                    asm_instructions.push_back(std::make_unique<Mov>( 
                        convert_val(binary_inst->_src1),
                        convert_val(binary_inst->_dst)
                    ));
                    asm_instructions.push_back(std::make_unique<BinaryASM>(
                        convert_op(binary_inst->_binary_op),
                        convert_val(binary_inst->_src2),
                        convert_val(binary_inst->_dst)
                    ));
                }
            } else if (instr->get_instruction_type() == tacky::TackyInstructionType::JUMP) {
                auto * jump_instr = static_cast<tacky::Jump*>(instr.get());
                asm_instructions.push_back(std::make_unique<Jmp>(jump_instr->_target_identifier));
            } else if (instr->get_instruction_type() == tacky::TackyInstructionType::JUMP_IF_ZERO) {
                auto * jump_instr = static_cast<tacky::JumpIfZero*>(instr.get());
                asm_instructions.push_back(std::make_unique<Cmp>(
                    std::make_unique<Immediate>(0),
                    convert_val(jump_instr->_condition)
                ));

                asm_instructions.push_back(std::make_unique<JmpCC>(
                    CondCode::E,
                    jump_instr->_target_identifier
                ));
            
            } else if (instr->get_instruction_type() == tacky::TackyInstructionType::JUMP_IF_NOT_ZERO) {
                auto * jump_instr = static_cast<tacky::JumpIfNotZero*>(instr.get());
                asm_instructions.push_back(std::make_unique<Cmp>(
                    std::make_unique<Immediate>(0),
                    convert_val(jump_instr->_condition)
                ));

                asm_instructions.push_back(std::make_unique<JmpCC>(
                    CondCode::NE,
                    jump_instr->_target_identifier
                ));
            } else if (instr->get_instruction_type() == tacky::TackyInstructionType::COPY) {
                auto* copy_inst = static_cast<tacky::TackyCopy*>(instr.get());
                asm_instructions.push_back(std::make_unique<Mov>(
                    convert_val(copy_inst->_src),
                    convert_val(copy_inst->_dst)
                ));
            } else if (instr->get_instruction_type() == tacky::TackyInstructionType::LABEL) {
                auto* label_instr = static_cast<tacky::Label*>(instr.get());
                asm_instructions.push_back(std::make_unique<Label>(
                    label_instr->_identifier
                ));
            } 
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