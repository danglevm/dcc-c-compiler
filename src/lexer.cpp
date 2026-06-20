#include <libdcc/lexer.hpp>
#include <iostream>

namespace {
    /* trim starting whitespace */
    auto trim(std::string_view s) {
        auto begin = s.find_first_not_of(" \t\r\n");
        if (begin == s.npos) {
            s = {};
            return s;
        }
        s.remove_prefix(begin);
        return s;
    }
}

namespace dcc {
    token Lexer::get_next_token ()  {
        check_invariants();

        while (std::isspace(peek())) {
            /* whitespace */
            if (char c = advance(); c == '\0') {
                return token{token_type::ENDOFFILE, "\0", _line, _col};
            }
        }

        size_t start_pos = 0;
        if (_pos - 1 >= 0) {
            start_pos = _pos - 1;
        }
        auto start_line = _line;
        auto start_col = _col;
        char c;
        if (c = advance(); c == '\0') {
            return token{token_type::ENDOFFILE, "\0", _line, _col};
        }

        if(std::isdigit(c)) {
            return scan_constant();
        }
        
        if (std::isalpha(c) || c == '_') {
            return scan_identifier();
        }
        
        /* valid case */
        switch(c) {
            case '=':
                if (_pos < _source.length() && peek() == '=') {
                    /* double equality case */
                    advance();
                    return token{token_type::EQUALITY, _source.substr(start_pos, _pos - start_pos), start_line, start_col};
                } 
                /* equality case */
                return token{token_type::ASSIGNMENT, _source.substr(start_pos, _pos - start_pos), start_line, start_col};
                
            case '+':
                /* check for '++' or '+=' */
                if (_pos < _source.length()) {
                    if (peek() == '+') {
                        advance();
                        return token{token_type::INCREMENT, _source.substr(start_pos, _pos - start_pos), start_line, start_col};

                    } else if (peek() == '=') {
                        advance();
                        return token{token_type::ADDITION_EQUALS, _source.substr(start_pos, _pos - start_pos), start_line, start_col};
                    }
                }
                return token{token_type::ADDITION, "+", start_line, start_col};
            case ';':
                return token{token_type::SEMICOLON, ";", start_line, start_col};
            case '{':
                return token{token_type::OPEN_BRACE, "{", start_line, start_col};
            case '}':
                return token{token_type::CLOSE_BRACE, "}", start_line, start_col};
            case '(':
                return token{token_type::OPEN_PARENTHESIS, "(", start_line, start_col};
            case ')':
                return token{token_type::CLOSE_PARENTHESIS, ")", start_line, start_col};
            case '~':
                return token{token_type::COMPLEMENT, "~", start_line, start_col};
            case '-':
                /* check for '--' and '-=' */
                if (_pos < _source.length()) {
                    if (peek() == '-') {
                        advance();
                        return token{token_type::DECREMENT, _source.substr(start_pos, _pos - start_pos), start_line, start_col};

                    } else if (peek() == '=') {
                        advance();
                        return token{token_type::SUBTRACTION_EQUALS, _source.substr(start_pos, _pos - start_pos), start_line, start_col};
                    }
                }

                /* we call this MINUS, because both negation and subtraction uses the same sign */
                return token{token_type::MINUS, "-", start_line, start_col};                
            
            case '*':
                return token{token_type::MULTIPLICATION, "*", start_line, start_col};
            case '/':
                return token{token_type::DIVISION, "/", start_line, start_col};
            case '%':
                return token{token_type::REMAINDER, "%", start_line, start_col};
        }   
        /* invalid case */
        return token{token_type::INVALID, _source.substr(start_pos, _pos - start_pos), start_line, start_col};
    }

    token_type Lexer::check_keyword(std::string_view text) {
        switch (text.length()) {
            case 3:
                if (text == "int") return token_type::INT_KEYWORD;
                break;
            case 4:
                if (text == "void") return token_type::VOID_KEYWORD;
            case 6:
                if (text == "return") return token_type::RETURN_KEYWORD;
                break;
        } 
        return token_type::IDENTIFIER; 
    }

    token Lexer::scan_identifier() {
        size_t start_pos = 0;
        if (_pos - 1 >= 0) {
            start_pos = _pos - 1;
        }
        auto start_line = _line;
        auto start_col =  _col;

        // Consume \w* until \b boundary
        while (_pos < _source.length() &&
                (std::isalnum(peek()) || peek() == '_')) {
                    advance();
        }
        /* slice the text */
        std::string_view text = _source.substr(start_pos, _pos - start_pos);
        
        auto type = check_keyword(text);

        return token{type, text, start_line, start_col};
    }
    
    /* could improve this for edge cases */
    token Lexer::scan_constant() {
        size_t start_pos = 0;
        if (_pos - 1 >= 0) {
            start_pos = _pos - 1;
        }
        auto start_line = _line;
        auto start_col =  _col;

        while (_pos < _source.length() && std::isdigit(peek())) {
            advance(); 
        }

        // \b - a number cannot be immediately followed by a letter 
        /* 123a is an example */
        if (_pos < _source.length() && std::isalpha(peek())) {
            /* eat the rest of the string and report invalid*/
            while (_pos < _source.length() && std::isalnum(peek())) {
                advance();
            }
            std::string_view text = _source.substr(start_pos, _pos - start_pos);
            return token{token_type::INVALID, text, start_line, start_col};

        }
        std::string_view text = _source.substr(start_pos, _pos - start_pos);
        return token{token_type::CONSTANT, text, start_line, start_col};
    }
}