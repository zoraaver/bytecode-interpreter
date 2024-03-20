#ifndef LOX_PARSER_H
#define LOX_PARSER_H

#include "object.h"
#include "scanner.h"
#include "value.h"

#include <expected>
#include <functional>
#include <memory>
#include <variant>

namespace lox
{

struct BinExprNode;
struct UnaryExprNode;
struct GroupExprNode;
struct ValueNode;
struct PrintStmtNode;
struct ExprStmtNode;

using ASTNode =
    std::variant<BinExprNode, ValueNode, GroupExprNode, UnaryExprNode, PrintStmtNode, ExprStmtNode>;

struct BinExprNode
{
    Token op;
    std::unique_ptr<ASTNode> left;
    std::unique_ptr<ASTNode> right;
};

struct GroupExprNode
{
    Token token;
    std::unique_ptr<ASTNode> expr;
};

struct UnaryExprNode
{
    Token op;
    std::unique_ptr<ASTNode> right;
};

struct ValueNode
{
    Token token;
    Value value;
};

struct PrintStmtNode
{
    Token token;
    std::unique_ptr<ASTNode> expr;
};

struct ExprStmtNode
{
    Token token;
    std::unique_ptr<ASTNode> expr;
};

using ASTNodePtr = std::unique_ptr<ASTNode>;

class Parser
{
    Scanner& _scanner;
    ObjectAllocator& _allocator;
    Token _current{TokenType::ERROR, 0};
    Token _previous{TokenType::ERROR, 0};
    bool _had_error = false;
    bool _panic_mode = false;

    void _consume(TokenType type, std::string_view message);

    void _error_at_current(std::string_view message);
    void _error(std::string_view message);
    void _error_at(const Token& token, std::string_view message);
    void _advance();
    bool _match(TokenType type);
    void _synchronize();

    enum class Precedence
    {
        NONE,
        ASSIGNMENT, // =
        OR, // or
        AND, // and
        EQUALITY, // == !=
        COMPARISON, // < > <= >=
        TERM, // + -
        FACTOR, // * /
        UNARY, // ! -
        CALL, // . ()
        PRIMARY
    };

    struct ParseRule
    {
        std::function<ASTNodePtr(Parser*)> prefix;
        std::function<ASTNodePtr(Parser*, ASTNodePtr)> infix;
        Precedence precedence;
    };

    ASTNodePtr _parse_number();
    ASTNodePtr _parse_grouping();
    ASTNodePtr _parse_expression();
    ASTNodePtr _parse_literal();
    ASTNodePtr _parse_string();
    ASTNodePtr _parse_unary_expression();
    ASTNodePtr _parse_binary_expression(ASTNodePtr);
    ASTNodePtr _parse_precedence(Precedence precedence);
    ASTNodePtr _parse_declaration();
    ASTNodePtr _parse_statement();
    ASTNodePtr _parse_print_statement();
    ASTNodePtr _parse_expression_statement();

    static ParseRule _parse_rules[];

public:
    enum class Error
    {
        BadToken
    };

    Parser(Scanner& scanner, ObjectAllocator& allocator);
    std::expected<std::vector<ASTNodePtr>, Error> parse();
};
} // namespace lox

#endif // LOX_PARSER_H