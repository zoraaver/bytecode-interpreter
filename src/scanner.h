#ifndef LOX_SCANNER_H
#define LOX_SCANNER_H

#include <string>
#include <string_view>

namespace lox
{

enum class TokenType
{
    // Single-character tokens.
    LEFT_PAREN,
    RIGHT_PAREN,
    LEFT_SQUARE_PAREN,
    RIGHT_SQUARE_PAREN,
    LEFT_BRACE,
    RIGHT_BRACE,
    COMMA,
    DOT,
    MINUS,
    PLUS,
    SEMICOLON,
    SLASH,
    STAR,
    // One or two character tokens.
    BANG,
    BANG_EQUAL,
    EQUAL,
    EQUAL_EQUAL,
    GREATER,
    GREATER_EQUAL,
    LESS,
    LESS_EQUAL,
    // Literals.
    IDENTIFIER,
    STRING,
    NUMBER,
    // Keywords.
    AND,
    CLASS,
    ELSE,
    FALSE,
    FOR,
    FUN,
    IF,
    NIL,
    OR,
    RETURN,
    SUPER,
    THIS,
    TRUE,
    VAR,
    WHILE,

    ERROR,
    END_OF_FILE
};

const char* get_token_type_name(TokenType type);

struct Token
{
    TokenType type;
    int line = 0;
    std::string_view lexeme;
};

class Scanner
{
    std::string_view _source;
    int _line = 1;
    int _start = 0;
    int _current = 0;

    bool _is_at_end() const;
    Token _make_token(TokenType type) const;
    Token _make_error_token(const char* message) const;
    char _advance();
    bool _match(char expected);
    void _skip_whitespace();
    char _peek(int offset = 0) const;
    Token _scan_string();
    Token _scan_number();
    Token _scan_identifier();

public:
    Scanner(const std::string& source)
        : _source(source)
    { }
    Token scan_token();
};
} // namespace lox

#endif // LOX_SCANNER_H