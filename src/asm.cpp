#include <libdcc/asm.hpp>

#include <string>
#include <iostream>
#include <memory>
#include <vector>

namespace assembly {
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