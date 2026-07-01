#include <libdcc/semantic.hpp>
#include <memory>

namespace dcc {
    std::unique_ptr<DeclarationVariable> SemanticAnalyzer::resolve_declaration(const DeclarationVariable* decl) {
        auto name = std::string(decl->_identifier);
        if (_variable_map.count(name)) {
            throw std::runtime_error("Variable already declared: " + name);
        }
        auto unique_name = make_temporary_var_name(decl->_identifier);
        _variable_map[name] = unique_name;

        std::unique_ptr<Expr> init = nullptr;
        if (decl->_initializer != nullptr) {
            init = resolve_expr(decl->_initializer.get());
        }

        return std::make_unique<DeclarationVariable>(unique_name, std::move(init));
    };

    std::unique_ptr<Statement> SemanticAnalyzer::resolve_statement(const Statement * stmt) {
            if (!stmt) {
                throw std::runtime_error("Encountered null Statement pointer during resolution.");
            }
            switch (stmt->get_statement_type()) {
                case dcc::StatementType::RETURN:
                    return std::make_unique<ReturnStmt>(resolve_expr(stmt->get_expr()));
                case dcc::StatementType::EXPRESSION:
                    return std::make_unique<ExpressionStmt>(resolve_expr(stmt->get_expr()));
                case dcc::StatementType::NULL_STMT:
                    return std::make_unique<NullStmt>();
                default:
                    throw std::runtime_error("Unknown statement type encountered during resolution.");
            }
    }

    /* recursively resolve all expr statements by replacing variable name with unique identifer from variable map */
    std::unique_ptr<Expr> SemanticAnalyzer::resolve_expr(const Expr* expr) {
        /* runtime type safety*/
        if (auto* assign = dynamic_cast<const Assignment*>(expr)) {

            //left side must be a variable or illegal
            auto* left_var = dynamic_cast<const Var*>(assign->_left_expr.get());
            if (!left_var) {
                throw std::runtime_error("Invalid lvalue in assignment: left side must be a variable.");
            }
            auto resolved_left = resolve_expr(assign->_left_expr.get());
            auto resolved_right = resolve_expr(assign->_right_expr.get());
            return std::make_unique<Assignment>(std::move(resolved_left), std::move(resolved_right));
        } else if (auto* var = dynamic_cast<const Var*>(expr)) {
            if (_variable_map.count(std::string(var->_identifier))) {
                return std::make_unique<Var>(_variable_map[std::string(var->_identifier)]);
            } else {
                throw std::runtime_error("Undeclared variable: " + std::string(var->_identifier));
            }
        } else if (auto* unary = dynamic_cast<const UnaryOperator*>(expr)) {
            return std::make_unique<UnaryOperator>(unary->getUnaryOp(), resolve_expr(unary->get_inner_expr()));
        } else if (auto* binary = dynamic_cast<const BinaryOperator*>(expr)) {
            return std::make_unique<BinaryOperator>(
                binary->_bin_op, 
                resolve_expr(binary->_left_expr.get()), 
                resolve_expr(binary->_right_expr.get())
            );
        }

        else if (auto * constant = dynamic_cast<const Constant*>(expr)) {
            return std::make_unique<Constant>(constant->_value);

        }

        throw std::runtime_error("Unknown expression type");
    }

}