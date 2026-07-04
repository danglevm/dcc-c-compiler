#ifndef DCC_SEMANTIC_HPP
#define DCC_SEMANTIC_HPP
#include <map>
#include <string_view>
#include <stdlib.h>
#include <memory>

#include <libdcc/parser.hpp>


namespace dcc {
    class SemanticAnalyzer : public ASTVisitor {
        public:
            SemanticAnalyzer() = default;

            std::unique_ptr<Expr> resolve(Expr* expr) {
                expr->accept(*this);
                return std::move(_result_expr);
            }

            std::unique_ptr<Statement> resolve(Statement* stmt) {
                stmt->accept(*this);
                return std::move(_result_stmt);
            }

            std::unique_ptr<Declaration> resolve(Declaration* decl) {
                decl->accept(*this);
                return std::move(_result_decl);
            }

            std::unique_ptr<BlockItem> resolve(BlockItem* item) {
                item->accept(*this);
                return std::move(_result_block_item);
            }

        private:
            std::vector<std::map<std::string, std::string>> _variable_maps;
            std::unique_ptr<Expr> _result_expr;
            std::unique_ptr<Statement> _result_stmt;
            std::unique_ptr<Declaration> _result_decl;
            std::unique_ptr<BlockItem> _result_block_item;
            std::unique_ptr<Block> _result_block;

            int _counter = 0;
            std::map<std::string, std::string> _variable_map;

            std::string lookup_variable(const std::string& name) {
                for (auto it = _variable_maps.rbegin(); it != _variable_maps.rend(); ++it) {
                    if (it->count(name)) {
                        return it->at(name);
                    }
                }
                throw std::runtime_error("Undeclared variable: " + name);
            }

            /* maps name to unique name */
            std::string make_temporary_var_name(const std::string_view& original_name) {
                return std::string(original_name) + "." + std::to_string(_counter++);
            }
            void visit(Constant * node) override;
            void visit(Var * node) override;
            void visit(UnaryOperator* node) override;
            void visit(BinaryOperator* node) override;
            void visit(Assignment* node) override;
            void visit(Conditional* node) override;

            void visit(ReturnStmt* node) override;
            void visit(IfStatement* node) override;
            void visit(ExpressionStmt* node) override;
            void visit(CompoundStmt* node) override;
            void visit(NullStmt* node) override;
            void visit(DeclarationVariable* node) override;
            void visit(Block* node) override;

            void visit(Function* node) override;
            void visit(Program* node) override;
    };
}

#endif