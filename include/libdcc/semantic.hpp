#ifndef DCC_SEMANTIC_HPP
#define DCC_SEMANTIC_HPP
#include <map>
#include <string_view>
#include <stdlib.h>
#include <memory>

#include <libdcc/parser.hpp>


namespace dcc {
    class SemanticAnalyzer {
        public:
            SemanticAnalyzer() = default;
        
            std::unique_ptr<Program> resolve_program(Program * program) {
                auto resolved_func = resolve_function(program->_function_definition.get());
                return std::make_unique<Program>(std::move(resolved_func));
            }
        private:
            int _counter = 0;
            std::map<std::string, std::string> _variable_map;
        
            std::unique_ptr<Function> resolve_function(Function * func) {
                _variable_map.clear();
                std::vector<std::unique_ptr<BlockItem>> resolved_body;

                for (auto& item : func->_block_items) {
                    resolved_body.push_back(resolve_block_item(item.get()));
                }

                return std::make_unique<Function>(func->_identifier, std::move(resolved_body));
            }

            std::unique_ptr<BlockItem> resolve_block_item(BlockItem* item) {
                if (!item) {
                    throw std::runtime_error("Encountered null BlockItem!");
                }

                // dynamic_cast works perfectly with raw pointers
                if (auto* decl = dynamic_cast<const DeclarationVariable*>(item)) {
                    return resolve_declaration(decl);
                }
                else if (auto* stmt = dynamic_cast<const Statement*>(item)) {
                    return resolve_statement(stmt);
                }
                
                throw std::runtime_error("Unknown BlockItem type!");
            }

            std::unique_ptr<Expr> resolve_expr(const Expr* expr);

            std::unique_ptr<Statement> resolve_statement(const Statement* stmt);

            std::unique_ptr<DeclarationVariable> resolve_declaration(const DeclarationVariable* decl);

            /* maps name to unique name */
            std::string make_temporary_var_name(const std::string_view& original_name) {
                return std::string(original_name) + "." + std::to_string(_counter++);
            }
    };
}

#endif