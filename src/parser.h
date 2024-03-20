#ifndef LOX_PARSER_H
#define LOX_PARSER_H

#include "scanner.h"
#include "value.h"

#include <functional>
#include <memory>
#include <variant>

namespace lox
{

struct BinExprNode;
struct UnaryExprNode;
struct GroupExprNode;
struct ValueNode;

using ASTNode = std::variant<BinExprNode, ValueNode, GroupExprNode, UnaryExprNode>;
using ASTExprNode = std::variant<BinExprNode, ValueNode, GroupExprNode, UnaryExprNode>;

struct BinExprNode
{
    Token op;
    std::unique_ptr<ASTExprNode> left;
    std::unique_ptr<ASTExprNode> right;
};

struct GroupExprNode
{
    Token token;
    std::unique_ptr<ASTExprNode> expr;
};

struct UnaryExprNode
{
    Token op;
    std::unique_ptr<ASTExprNode> right;
};

struct ValueNode
{
    Token token;
    Value value;
};

using ASTNodePtr = std::unique_ptr<ASTNode>;

class Parser
{
    Scanner& _scanner;
    Token _current{TokenType::ERROR, 0};
    Token _previous{TokenType::ERROR, 0};
    bool _had_error = false;
    bool _panic_mode = false;

    void _consume(TokenType type, std::string_view message);

    void _error_at_current(std::string_view message);
    void _error(std::string_view message);
    void _error_at(const Token& token, std::string_view message);
    void _advance();

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
    ASTNodePtr _parse_unary_expression();
    ASTNodePtr _parse_binary_expression(ASTNodePtr);
    ASTNodePtr _parse_precedence(Precedence precedence);

    static ParseRule _parse_rules[];

public:
    Parser(Scanner& scanner);
    ASTNodePtr parse();
};
} // namespace lox

#endif // LOX_PARSER_H