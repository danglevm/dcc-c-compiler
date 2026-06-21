#ifndef DCC_LEXER_HPP
#define DCC_LEXER_HPP

#include <string_view>
#include <cassert>
#include <cctype>
#include <iostream>

namespace dcc {
    enum class token_type {
        INT_KEYWORD,
        MAIN_KEYWORD,
        RETURN_KEYWORD,
        VOID_KEYWORD,
        IDENTIFIER,
        OPEN_PARENTHESIS,
        CLOSE_PARENTHESIS,
        OPEN_BRACE,
        CLOSE_BRACE,
        CONSTANT,
        SEMICOLON,
        EQUALITY, 
        ASSIGNMENT,
        ADDITION,
        ADDITION_EQUALS,
        SUBTRACTION,
        SUBTRACTION_EQUALS,
        MULTIPLICATION,
        DIVISION,
        REMAINDER,
        INCREMENT,
        DECREMENT,
        ENDOFFILE,
        INVALID,
        COMPLEMENT,
        NEGATION,
        MINUS,
        NOT, //!
        NOT_EQUALITY, //!=
        AND, //&&
        OR, //|| 
        LESSER, //<
        GREATER, //>
        LESSER_EQUAL, //<=
        GREATER_EQUAL //>=
    };

    struct token {
        token_type token;
        std::string_view text;
        int line;
        int column;
    };

    class Lexer {
        public:
            Lexer(std::string_view source) 
                : _source(source), _pos(0), _line(1), _col(1) {}

            token get_next_token();
            token scan_identifier();
            token scan_constant();

        private:
            std::string_view _source;
            size_t _pos;
            int _line;
            int _col;

            char peek () const {
                if (_pos >= _source.length()) return '\0';
                return _source[_pos];
            };

            char advance () {
                if (_pos >= _source.length()) {
                    return '\0';
                } 
                char c = _source[_pos];
                _pos++;
                
                if (c == '\n') {
                    ++_line;
                    _col = 1;
                } else {
                    ++_col;
                }

                return c;
            };

            void check_invariants () const {
                assert(_line >= 1 && "Line cannot be smaller than 1");
                assert(_col >= 1 && "Column cannot be smaller than 1");
                assert(_pos < _source.length() && "Advanced Past EOF");         
            };

            token_type check_keyword (std::string_view text);
    };
}

#endif