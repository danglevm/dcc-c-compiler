#include <libdcc/tacky.hpp>

namespace tacky {

    void TackyGen::visit(dcc::UnaryOperator* node) {
        /* visits left child and updates _current_val */
        node->get_inner_expr()->accept(*this);
        TackyVal src = _current_val;

        auto dst_name = make_temporary_name();
        auto dst = TackyVar{dst_name};
        /* visits right child and updates _current_val */
        TackyUnaryOp unary_op = convert_unop(node->getUnaryOp());
        _instructions.push_back(std::make_unique<TackyUnary>(unary_op, src, dst));
        _current_val = dst;
    }

    void TackyGen::visit(dcc::BinaryOperator* node) {
        auto op = node->getBinOp();

        if (op == dcc::BinaryOpType::And){
            std::string false_label = make_label_name("and_false");
            std::string end_label = make_label_name("and_else");
            TackyVal result_val = TackyVar{make_temporary_name()};

            /* left side */
            node->getLeftExpr()->accept(*this);
            TackyVal v1 = _current_val;
            _instructions.push_back(std::make_unique<JumpIfZero>(v1, false_label));       

            /* right side */
            node->getRightExpr()->accept(*this);
            TackyVal v2 = _current_val;
            _instructions.push_back(std::make_unique<JumpIfZero>(v2, false_label));   

            /* Success: both true*/
            _instructions.push_back(std::make_unique<TackyCopy>(TackyConstant{1}, result_val));
            _instructions.push_back(std::make_unique<Jump>(end_label));

            /* Failure: at least one false */
            _instructions.push_back(std::make_unique<Label>(false_label));
            _instructions.push_back(std::make_unique<TackyCopy>(TackyConstant{0}, result_val));

            _instructions.push_back(std::make_unique<Label>(end_label));
            _current_val = result_val;
            return;
        }             /* Logical OR (||) - short circuits when left is true (not 0)*/
        else if (node->getBinOp() == dcc::BinaryOpType::Or) {
            std::string true_label = make_label_name("or_true");
            std::string end_label = make_label_name("or_end");
            TackyVal result_val = TackyVar{make_temporary_name()};

            /* left side*/
            node->getLeftExpr()->accept(*this);
            TackyVal v1 = _current_val;
            _instructions.push_back(std::make_unique<JumpIfNotZero>(v1, true_label));    

            /* right side */
            node->getRightExpr()->accept(*this);
            TackyVal v2 = _current_val;
            _instructions.push_back(std::make_unique<JumpIfNotZero>(v2, true_label));
            
            /* Failure: both side false - need to evaluate this after both checks failed */
            _instructions.push_back(std::make_unique<TackyCopy>(TackyConstant{0}, result_val));
            _instructions.push_back(std::make_unique<Jump>(end_label));

            /* SUCCCESS: At least one side is true */
            _instructions.push_back(std::make_unique<Label>(true_label));
            _instructions.push_back(std::make_unique<TackyCopy>(TackyConstant{1}, result_val));

            _instructions.push_back(std::make_unique<Label>(end_label));

            _current_val = result_val;
            return;
        } 

        /* arithmetic and relational operations */
        /* visits left child and updates _current_val */
        node->getLeftExpr()->accept(*this);
        TackyVal src1 = _current_val;

        /* visits right child and updates _current_val */
        node->getRightExpr()->accept(*this);
        TackyVal src2 = _current_val;

        auto dst = TackyVar{make_temporary_name()};
        TackyBinaryOp binary_op = convert_binop(op);

        _instructions.push_back(std::make_unique<TackyBinary>(binary_op, src1, src2, dst));

        _current_val = dst;
    }

    void TackyGen::visit(dcc::ReturnStmt* node) {
        node->get_expr()->accept(*this);
        TackyVal result = _current_val; // Read state
        _instructions.push_back(std::make_unique<TackyReturn>(result));
    }

    void TackyGen::visit(dcc::Assignment* node) {
        node->getRightExpr()->accept(*this);
        TackyVal rhs_val = _current_val;

        auto * lhs_var = dynamic_cast<dcc::Var*>(node->getLeftExpr());
        if (!lhs_var) {
            throw std::runtime_error("TACKY Gen Error: Left side of assignment is not a variable.");
        }

        auto dst = TackyVar{std::string(lhs_var->get_identifier())};
        _instructions.push_back(std::make_unique<TackyCopy>(rhs_val, dst));

        _current_val = dst;
    }

    void TackyGen::visit(dcc::DeclarationVariable* node) {
        /* if there's an initializer, emit instructions to calculate and copy it */
        if (node->_initializer != nullptr) {
            node->_initializer->accept(*this);
            TackyVal result = _current_val;
            TackyVal dst = TackyVar{std::string(node->_identifier)};
            // a.1 = tmp.2
            _instructions.push_back(std::make_unique<TackyCopy>(result, dst));
        }
        //If no initializer (int b;), fall through and do nothing
    }

    void TackyGen::visit(dcc::IfStatement* node) {
        node->_condition_expr->accept(*this);
        auto resolved_condition = _current_val;
        /* universal end label for if construct */
        std::string end_label = make_label_name("if_end");

        /* if the if-construct has an else component */
        if (node->_else_stmt != nullptr) {
            std::string else_label = make_label_name("if_else");

            _instructions.push_back(std::make_unique<JumpIfZero>(resolved_condition, else_label));

            node->_then_stmt->accept(*this);

            _instructions.push_back(std::make_unique<Jump>(end_label));

            _instructions.push_back(std::make_unique<Label>(else_label));

            node->_else_stmt->accept(*this);

            _instructions.push_back(std::make_unique<Label>(end_label));
        } else {
            //if the if-construct has no else
            _instructions.push_back(std::make_unique<JumpIfZero>(resolved_condition, end_label));    

            node->_then_stmt->accept(*this);

            _instructions.push_back(std::make_unique<Label>(end_label));
        }
    }

    void TackyGen::visit(dcc::Conditional* node) {
        node->_condition_expr->accept(*this);
        auto resolved_condition = _current_val;
        std::string e2_label = make_label_name("e2_end");
        std::string end_label = make_label_name("conditional_end");

        TackyVal result_val = TackyVar{make_temporary_name()};

        /* jumps to false branch if condition is 0 */
        _instructions.push_back(std::make_unique<JumpIfZero>(resolved_condition, e2_label));

        /* TRUE branch */
        node->_true_expr->accept(*this);
        _instructions.push_back(std::make_unique<TackyCopy>(_current_val, result_val));
        _instructions.push_back(std::make_unique<Jump>(end_label));
        _instructions.push_back(std::make_unique<Label>(e2_label));
        node->_false_expr->accept(*this);
        _instructions.push_back(std::make_unique<TackyCopy>(_current_val, result_val));     
        _instructions.push_back(std::make_unique<Label>(end_label));

        _current_val = result_val;
    }

    void TackyGen::visit(dcc::ExpressionStmt* node) {
        node->get_expr()->accept(*this);
    }

    void TackyGen::visit(dcc::Function* func) {
        for (const auto& block_item : func->_block->_block_items) {
            block_item->accept(*this);
        }

        /* unconditionally append a Return(0) case. If programmer forgets, this saves us. If they did return, CPU doesn't reach this */
        _instructions.push_back(std::make_unique<TackyReturn>(TackyConstant{0}));
    }
}