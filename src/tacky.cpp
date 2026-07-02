#include <libdcc/tacky.hpp>

namespace tacky {
    TackyVal TackyGen::emit_tacky(dcc::Expr * expr) {
        if (expr->getExprType() == dcc::ExprType::CONSTANT) {
            return TackyConstant{static_cast<dcc::Constant*>(expr)->_value};
        } else if (expr->getExprType() == dcc::ExprType::VAR) {
            auto var = static_cast<dcc::Var*>(expr);
            return TackyVar{std::string(var->get_identifier())};
        } else if (expr->getExprType() == dcc::ExprType::ASSIGNMENT) {
            auto assign = static_cast<dcc::Assignment*>(expr);
            TackyVal result = emit_tacky(assign->_right_expr.get());
            
            auto var = static_cast<dcc::Var*>(assign->_left_expr.get());
            TackyVal dst = TackyVar{std::string(var->_identifier)};
            _instructions.push_back(std::make_unique<TackyCopy>(result, dst));
            
            return dst;
        } else if (expr->getExprType() == dcc::ExprType::UNARY) {
            auto unary = static_cast<dcc::UnaryOperator*>(expr);
            auto src = emit_tacky(unary->get_inner_expr());
            auto dst_name = make_temporary_name();
            auto dst = TackyVar{dst_name};
            auto tacky_op = unary->getUnaryOp();

            TackyUnaryOp unary_op = convert_unop(unary->getUnaryOp());
            _instructions.push_back(std::make_unique<TackyUnary>(unary_op, src, dst));
            return dst;
        } else if (expr->getExprType() == dcc::ExprType::BINARY) {
            auto binary = static_cast<dcc::BinaryOperator*>(expr);

            /* Logical AND (&&) - short circuits when left is false (0)*/
            if (binary->getBinOp() == dcc::BinaryOpType::And) {
                std::string false_label = make_label_name("and_false");
                std::string end_label = make_label_name("and_else");
                TackyVal result_val = TackyVar{make_temporary_name()};

                /* left side */
                TackyVal v1 = emit_tacky(binary->getLeftExpr());
                _instructions.push_back(std::make_unique<JumpIfZero>(v1, false_label));       

                /* right side */
                TackyVal v2 = emit_tacky(binary->getRightExpr());
                _instructions.push_back(std::make_unique<JumpIfZero>(v2, false_label));   

                /* Success: both true*/
                _instructions.push_back(std::make_unique<TackyCopy>(TackyConstant{1}, result_val));
                _instructions.push_back(std::make_unique<Jump>(end_label));

                /* Failure: at least one false */
                _instructions.push_back(std::make_unique<Label>(false_label));
                _instructions.push_back(std::make_unique<TackyCopy>(TackyConstant{0}, result_val));

                _instructions.push_back(std::make_unique<Label>(end_label));
                return result_val;
            
            } 
            /* Logical OR (||) - short circuits when left is true (not 0)*/
            else if (binary->getBinOp() == dcc::BinaryOpType::Or) {
                std::string true_label = make_label_name("or_true");
                std::string end_label = make_label_name("or_end");
                TackyVal result_val = TackyVar{make_temporary_name()};

                /* left side*/
                TackyVal v1 = emit_tacky(binary->getLeftExpr());
                _instructions.push_back(std::make_unique<JumpIfNotZero>(v1, true_label)); 

                /* right side */
                TackyVal v2 = emit_tacky(binary->getRightExpr());
                _instructions.push_back(std::make_unique<JumpIfNotZero>(v2, true_label));
                
                /* Failure: both side false - need to evaluate this after both checks failed */
                _instructions.push_back(std::make_unique<TackyCopy>(TackyConstant{0}, result_val));
                _instructions.push_back(std::make_unique<Jump>(end_label));
  
                /* SUCCCESS: At least one side is true */
                _instructions.push_back(std::make_unique<Label>(true_label));
                _instructions.push_back(std::make_unique<TackyCopy>(TackyConstant{1}, result_val));

                _instructions.push_back(std::make_unique<Label>(end_label));

                return result_val;
            } else {
                auto src1 = emit_tacky(binary->getLeftExpr());
                auto src2 = emit_tacky(binary->getRightExpr());

                auto dst_name = make_temporary_name();
                auto dst = TackyVar{dst_name};

                TackyBinaryOp binary_op = convert_binop(binary->getBinOp());
                _instructions.push_back(std::make_unique<TackyBinary>(binary_op, src1, src2, dst));
                return dst;
            }
        } 
        return std::monostate {};
    }

    void TackyGen::emit_tacky_for_block_item(dcc::BlockItem* item) {
        if (auto* decl = dynamic_cast<dcc::DeclarationVariable*>(item)) {
            /* if there's an initializer, emit instructions to calculate and copy it */
            if (decl->_initializer != nullptr) {
                TackyVal result = emit_tacky(decl->_initializer.get());
                TackyVal dst = TackyVar{std::string(decl->_identifier)};
                // a.1 = tmp.2
                _instructions.push_back(std::make_unique<TackyCopy>(result, dst));
            }
        //If no initializer (int b;), fall through and do nothing
        }

        else if (auto* stmt = dynamic_cast<dcc::Statement*>(item)) {
            emit_tacky_for_statement(stmt);
        }
    }

    void TackyGen::emit_tacky_for_statement(dcc::Statement* stmt) {
        if (!stmt) return;
        switch (stmt->get_statement_type()) {
            case dcc::StatementType::RETURN: {
                TackyVal result = emit_tacky(stmt->get_expr()); //evaluate right hand side expr
                _instructions.push_back(std::make_unique<TackyReturn>(result));
                break;
            }
            case dcc::StatementType::EXPRESSION: {
                // Evaluate the expression for its side effects (like assignments), 
                // but ignore the returned TackyVal because no one is using it.
                emit_tacky(stmt->get_expr());
                break;
            }
            /* do nothing */
            case dcc::StatementType::NULL_STMT: {
                break;
            }
            default:
                throw std::runtime_error("Unknown statement type in TACKY generation.");
        }
    }

    void TackyGen::emit_tacky_function(dcc::Function* func) {
        if (!func) return;

        for (const auto& block_item : func->_block_items) {
            emit_tacky_for_block_item(block_item.get());
        }

        /* unconditionally append a Return(0) case. If programmer forgets, this saves us. If they did return, CPU doesn't reach this */
        _instructions.push_back(std::make_unique<TackyReturn>(TackyConstant{0}));
    }
}