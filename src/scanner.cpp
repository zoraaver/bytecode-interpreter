#include "scanner.h"

#include <array>

namespace lox
{
const char* get_token_type_name(TokenType type)
{
    switch(type)
    {
#define CASE(x)                                                                                    \
    case TokenType::x:                                                                             \
        return #x;
        CASE(LEFT_PAREN)
        CASE(RIGHT_PAREN)
        CASE(LEFT_SQUARE_PAREN)
        CASE(RIGHT_SQUARE_PAREN)
        CASE(LEFT_BRACE)
        CASE(RIGHT_BRACE)
        CASE(COMMA)
        CASE(DOT)
        CASE(MINUS)
        CASE(PLUS)
        CASE(SEMICOLON)
        CASE(SLASH)
        CASE(STAR)
        CASE(BANG)
        CASE(BANG_EQUAL)
        CASE(EQUAL)
        CASE(EQUAL_EQUAL)
        CASE(GREATER)
        CASE(GREATER_EQUAL)
        CASE(LESS)
        CASE(LESS_EQUAL)
        CASE(IDENTIFIER)
        CASE(STRING)
        CASE(NUMBER)
        CASE(AND)
        CASE(CLASS)
        CASE(ELSE)
        CASE(FALSE)
        CASE(FOR)
        CASE(FUN)
        CASE(IF)
        CASE(NIL)
        CASE(OR)
        CASE(RETURN)
        CASE(SUPER)
        CASE(THIS)
        CASE(TRUE)
        CASE(VAR)
        CASE(WHILE)
        CASE(ERROR)
        CASE(END_OF_FILE)
#undef CASE
    }
}

namespace
{
constexpr std::array<std::pair<std::string_view, TokenType>, 16> keywords = {{
    {"and", TokenType::AND},
    {"class", TokenType::CLASS},
    {"else", TokenType::ELSE},
    {"false", TokenType::FALSE},
    {"for", TokenType::FOR},
    {"fun", TokenType::FUN},
    {"if", TokenType::IF},
    {"nil", TokenType::NIL},
    {"or", TokenType::OR},
    {"return", TokenType::RETURN},
    {"super", TokenType::SUPER},
    {"this", TokenType::THIS},
    {"true", TokenType::TRUE},
    {"var", TokenType::VAR},
    {"while", TokenType::WHILE},
}};

// const std::unordered_map
bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

bool is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool is_alpha_numeric(char c)
{
    return is_alpha(c) || is_digit(c);
}
} // namespace

bool Scanner::_is_at_end() const
{
    return _source[_current] == '\0';
}

Token Scanner::_make_token(TokenType type) const
{
    return Token{type, _line, _source.substr(_start, _current - _start)};
}

Token Scanner::_make_error_token(const char* message) const
{
    return Token{TokenType::ERROR, _line, message};
}

char Scanner::_advance()
{
    return _source[_current++];
}

bool Scanner::_match(char expected)
{
    if(_is_at_end())
    {
        return false;
    }

    if(_source[_current] != expected)
    {
        return false;
    }

    _current++;
    return true;
}

Token Scanner::scan_token()
{
    _skip_whitespace();

    _start = _current;

    if(_is_at_end())
    {
        return _make_token(TokenType::END_OF_FILE);
    }

    switch(auto c = _advance(); c)
    {
    case '(':
        return _make_token(TokenType::LEFT_PAREN);
    case ')':
        return _make_token(TokenType::RIGHT_PAREN);
    case '{':
        return _make_token(TokenType::LEFT_BRACE);
    case '}':
        return _make_token(TokenType::RIGHT_BRACE);
    case ';':
        return _make_token(TokenType::SEMICOLON);
    case ',':
        return _make_token(TokenType::COMMA);
    case '.':
        return _make_token(TokenType::DOT);
    case '-':
        return _make_token(TokenType::MINUS);
    case '+':
        return _make_token(TokenType::PLUS);
    case '/':
        return _make_token(TokenType::SLASH);
    case '*':
        return _make_token(TokenType::STAR);
    case '[':
        return _make_token(TokenType::LEFT_SQUARE_PAREN);
    case ']':
        return _make_token(TokenType::RIGHT_SQUARE_PAREN);
    case '!':
        return _make_token(_match('=') ? TokenType::BANG_EQUAL : TokenType::BANG);
    case '=':
        return _make_token(_match('=') ? TokenType::EQUAL_EQUAL : TokenType::EQUAL);
    case '<':
        return _make_token(_match('=') ? TokenType::LESS_EQUAL : TokenType::LESS);
    case '>':
        return _make_token(_match('=') ? TokenType::GREATER_EQUAL : TokenType::GREATER);
    case '"':
        return _scan_string();
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        return _scan_number();
    default:
        return _scan_identifier();
    }

    return _make_error_token("Unexpected character");
}

Token Scanner::_scan_string()
{
    while(_peek() != '"' && !_is_at_end())
    {
        if(_peek() == '\n')
        {
            _line++;
        }
        _advance();
    }

    if(_is_at_end())
    {
        return _make_error_token("Unterminated string");
    }

    // The closing ".
    _advance();

    return _make_token(TokenType::STRING);
}

Token Scanner::_scan_number()
{
    while(is_digit(_peek()))
    {
        _advance();
    }

    if(_peek() == '.' && is_digit(_peek(1)))
    {
        // Consume the ".".
        _advance();

        while(is_digit(_peek()))
        {
            _advance();
        }
    }

    return _make_token(TokenType::NUMBER);
}

Token Scanner::_scan_identifier()
{
    while(is_alpha_numeric(_peek()))
    {
        _advance();
    }

    auto text = _source.substr(_start, _current - _start);

    for(const auto& keyword : keywords)
    {
        if(text == keyword.first)
        {
            return _make_token(keyword.second);
        }
    }

    return _make_token(TokenType::IDENTIFIER);
}

void Scanner::_skip_whitespace()
{
    while(true)
    {
        switch(auto c = _peek(); c)
        {
        case ' ':
        case '\r':
        case '\t':
            _advance();
            break;
        case '\n':
            _line++;
            _advance();
            break;
        case '/':
            if(_peek(1) == '/')
            {
                // A comment goes until the end of the line.
                while(_peek() != '\n' && !_is_at_end())
                {
                    _advance();
                }
            }
            else
            {
                return;
            }
            break;
        default:
            return;
        }
    }
}

char Scanner::_peek(int offset) const
{
    if(_is_at_end())
    {
        return '\0';
    }

    return _source[_current + offset];
}

} // namespace lox