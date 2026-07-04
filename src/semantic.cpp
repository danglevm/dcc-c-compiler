#include <libdcc/semantic.hpp>
#include <memory>

namespace dcc {
    void SemanticAnalyzer::visit(Program * node) {
        _variable_map.clear();
        /* global scope by the program */
        _variable_maps.push_back({});

        node->_function_definition->accept(*this);
        /* close the global block */
        _variable_maps.pop_back();
    }

    void SemanticAnalyzer::visit(Function * node) {
        node->_block->accept(*this);
    }

    void SemanticAnalyzer::visit(Block * node) {
        _variable_maps.push_back({});

        std::vector<std::unique_ptr<BlockItem>> resolved_items;
        for (const auto &item : node->_block_items) {
            item->accept(*this);
            resolved_items.push_back(std::move(_result_block_item));
        }

        _variable_maps.pop_back();
        _result_block = std::make_unique<Block>(std::move(resolved_items));
    }

    void SemanticAnalyzer::visit(BinaryOperator* node) {
        _result_expr = std::make_unique<BinaryOperator>(
            node->_bin_op, 
            resolve(node->_left_expr.get()), 
            resolve(node->_right_expr.get())
        );
    }

    void SemanticAnalyzer::visit(UnaryOperator * node) {
        _result_expr = std::make_unique<UnaryOperator>(
            node->getUnaryOp(), 
            resolve(node->get_inner_expr())
        );
    }

    void SemanticAnalyzer::visit(Conditional* node) {
        _result_expr = std::make_unique<Conditional>(
            std::move(resolve(node->_condition_expr.get())), 
            std::move(resolve(node->_true_expr.get())), 
            std::move(resolve(node->_false_expr.get()))
        );
    }

    void SemanticAnalyzer::visit(Var* node) {
        std::string name = std::string(node->_identifier);
        _result_expr = std::make_unique<Var>(lookup_variable(name));
    }

    void SemanticAnalyzer::visit(Constant* node) {
        _result_expr = std::make_unique<Constant>(node->_value);
    }

    void SemanticAnalyzer::visit(DeclarationVariable* node) {
        auto name = std::string(node->_identifier);
        /* checks the innermost scope */
        if (_variable_maps.back().count(name)) {
            throw std::runtime_error("Variable already declared: " + name);
        }
        auto unique_name = make_temporary_var_name(node->_identifier);

        /* write to innermost scope */
        _variable_maps.back()[name] = unique_name;

        std::unique_ptr<Expr> init = nullptr;
        if (node->_initializer != nullptr) {
            init = resolve(node->_initializer.get());
        }

        _result_decl = std::make_unique<DeclarationVariable>(unique_name, std::move(init));
    }

    void SemanticAnalyzer::visit(Assignment* node) {
        //left side must be a variable or illegal
        auto* left_var = dynamic_cast<const Var*>(node->_left_expr.get());
        if (!left_var) {
            throw std::runtime_error("Invalid lvalue in assignment: left side must be a variable.");
        }
        auto resolved_left = resolve(node->_left_expr.get());
        auto resolved_right = resolve(node->_right_expr.get());
        _result_expr = std::make_unique<Assignment>(std::move(resolved_left), std::move(resolved_right));
    }
    
    void SemanticAnalyzer::visit(ReturnStmt* node) {
        _result_stmt = std::make_unique<ReturnStmt>(std::move(resolve(node->get_expr())));
    }

    void SemanticAnalyzer::visit(ExpressionStmt* node) {
        _result_stmt = std::make_unique<ExpressionStmt>(std::move(resolve(node->get_expr())));
    }

    void SemanticAnalyzer::visit(NullStmt* node) {
       _result_stmt = std::make_unique<NullStmt>();
    }

    void SemanticAnalyzer::visit(CompoundStmt* node) {
        node->_block->accept(*this);
        // Add _result_block here
        _result_stmt = std::make_unique<CompoundStmt>(std::move(_result_block));
    }

    void SemanticAnalyzer::visit(IfStatement* node) {
        auto resolved_cond = resolve(node->_condition_expr.get());
        auto resolved_then = resolve(node->_then_stmt.get());
        std::unique_ptr<Statement> resolved_else = nullptr;
        if (node->_else_stmt) {
            resolved_else = resolve(node->_else_stmt.get());
        }
        _result_stmt = std::make_unique<IfStatement>(std::move(resolved_cond), std::move(resolved_then), std::move(resolved_else));
    }
}