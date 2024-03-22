#ifndef LOX_PARSER_H
#define LOX_PARSER_H

#include "object.h"
#include "scanner.h"
#include "value.h"

#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace lox
{

struct BinExprNode;
struct UnaryExprNode;
struct GroupExprNode;
struct ValueNode;
struct PrintStmtNode;
struct ExprStmtNode;
struct BlockStmtNode;
struct VarDeclNode;
struct VariableExprNode;
struct AssignmentExprNode;
struct IfStmtNode;
struct WhileStmtNode;
struct FunDeclNode;
struct CallNode;
struct ReturnStmtNode;

using ASTNode = std::variant<BinExprNode,
                             ValueNode,
                             GroupExprNode,
                             UnaryExprNode,
                             PrintStmtNode,
                             ExprStmtNode,
                             VarDeclNode,
                             VariableExprNode,
                             AssignmentExprNode,
                             BlockStmtNode,
                             IfStmtNode,
                             WhileStmtNode,
                             FunDeclNode,
                             CallNode,
                             ReturnStmtNode>;

using ASTNodePtr = std::unique_ptr<ASTNode>;

struct BinExprNode
{
    Token op;
    ASTNodePtr left;
    ASTNodePtr right;
};

struct GroupExprNode
{
    Token token;
    ASTNodePtr expr;
};

struct UnaryExprNode
{
    Token op;
    ASTNodePtr right;
};

struct ValueNode
{
    Token token;
    Value value;
};

struct CallNode
{
    ASTNodePtr callee;
    Token paren;
    std::vector<ASTNodePtr> args;
};

struct PrintStmtNode
{
    Token token;
    ASTNodePtr expr;
};

struct ExprStmtNode
{
    Token token;
    ASTNodePtr expr;
};

struct BlockStmtNode
{
    Token end_brace;
    std::vector<ASTNodePtr> statements;
};

struct IfStmtNode
{
    Token if_tok;
    std::optional<Token> else_tok;
    ASTNodePtr condition;
    ASTNodePtr then_branch;
    ASTNodePtr else_branch;
};

struct WhileStmtNode
{
    Token while_tok;
    ASTNodePtr condition;
    ASTNodePtr body;
};

struct ReturnStmtNode
{
    Token keyword;
    ASTNodePtr value;
};

struct FunDeclNode
{
    Token name;
    std::vector<Token> params;
    ASTNodePtr body;
};

struct VarDeclNode
{
    Token identifier;
    ASTNodePtr initializer;
};

struct VariableExprNode
{
    Token var;
};

struct AssignmentExprNode
{
    VariableExprNode target;
    ASTNodePtr value;
};

class Parser
{
    Scanner& _scanner;
    ObjectAllocator& _allocator;
    Token _current{TokenType::ERROR, 0};
    Token _previous{TokenType::ERROR, 0};
    bool _had_error = false;
    bool _panic_mode = false;

    std::optional<Token> _consume(TokenType type, std::string_view message);

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
    ASTNodePtr _parse_block_statement();
    ASTNodePtr _parse_if_statement();
    ASTNodePtr _parse_while_statement();
    ASTNodePtr _parse_for_statement();
    ASTNodePtr _parse_return_statement();
    ASTNodePtr _parse_var_declaration();
    ASTNodePtr _parse_function_declaration();
    ASTNodePtr _parse_variable();
    ASTNodePtr _parse_call(ASTNodePtr);
    ASTNodePtr _parse_assignment_expression(ASTNodePtr);

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