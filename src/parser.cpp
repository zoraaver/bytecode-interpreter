#include "parser.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <print>
#include <utility>
#include <variant>
#include <vector>

#include "object.h"
#include "scanner.h"

namespace lox
{
namespace
{

constexpr uint8_t type_to_int(TokenType type)
{
    return static_cast<uint8_t>(type);
}

} // namespace

Parser::ParseRule Parser::_parse_rules[] = {
    [type_to_int(TokenType::LEFT_PAREN)] = {&Parser::_parse_grouping,
                                            &Parser::_parse_call,
                                            Precedence::CALL},
    [type_to_int(TokenType::RIGHT_PAREN)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::LEFT_BRACE)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::RIGHT_BRACE)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::COMMA)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::DOT)] = {nullptr, &Parser::_parse_dot, Precedence::CALL},
    [type_to_int(TokenType::MINUS)] = {&Parser::_parse_unary_expression,
                                       &Parser::_parse_binary_expression,
                                       Precedence::TERM},
    [type_to_int(TokenType::PLUS)] = {nullptr, &Parser::_parse_binary_expression, Precedence::TERM},
    [type_to_int(TokenType::SEMICOLON)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(
        TokenType::SLASH)] = {nullptr, &Parser::_parse_binary_expression, Precedence::FACTOR},
    [type_to_int(
        TokenType::STAR)] = {nullptr, &Parser::_parse_binary_expression, Precedence::FACTOR},
    [type_to_int(TokenType::BANG)] = {&Parser::_parse_unary_expression, nullptr, Precedence::NONE},
    [type_to_int(TokenType::BANG_EQUAL)] = {nullptr,
                                            &Parser::_parse_binary_expression,
                                            Precedence::EQUALITY},
    [type_to_int(TokenType::EQUAL)] = {nullptr,
                                       &Parser::_parse_assignment_expression,
                                       Precedence::ASSIGNMENT},
    [type_to_int(TokenType::EQUAL_EQUAL)] = {nullptr,
                                             &Parser::_parse_binary_expression,
                                             Precedence::EQUALITY},
    [type_to_int(
        TokenType::GREATER)] = {nullptr, &Parser::_parse_binary_expression, Precedence::COMPARISON},
    [type_to_int(TokenType::GREATER_EQUAL)] = {nullptr,
                                               &Parser::_parse_binary_expression,
                                               Precedence::COMPARISON},
    [type_to_int(
        TokenType::LESS)] = {nullptr, &Parser::_parse_binary_expression, Precedence::COMPARISON},
    [type_to_int(TokenType::LESS_EQUAL)] = {nullptr,
                                            &Parser::_parse_binary_expression,
                                            Precedence::COMPARISON},
    [type_to_int(TokenType::IDENTIFIER)] = {&Parser::_parse_variable, nullptr, Precedence::NONE},
    [type_to_int(TokenType::STRING)] = {&Parser::_parse_string, nullptr, Precedence::NONE},
    [type_to_int(TokenType::NUMBER)] = {&Parser::_parse_number, nullptr, Precedence::NONE},
    [type_to_int(TokenType::AND)] = {nullptr, &Parser::_parse_binary_expression, Precedence::AND},
    [type_to_int(TokenType::CLASS)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::ELSE)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::FALSE)] = {&Parser::_parse_literal, nullptr, Precedence::NONE},
    [type_to_int(TokenType::FOR)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::FUN)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::IF)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::NIL)] = {&Parser::_parse_literal, nullptr, Precedence::NONE},
    [type_to_int(TokenType::OR)] = {nullptr, &Parser::_parse_binary_expression, Precedence::OR},
    [type_to_int(TokenType::RETURN)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::SUPER)] = {&Parser::_parse_super, nullptr, Precedence::NONE},
    [type_to_int(TokenType::THIS)] = {&Parser::_parse_this, nullptr, Precedence::NONE},
    [type_to_int(TokenType::TRUE)] = {&Parser::_parse_literal, nullptr, Precedence::NONE},
    [type_to_int(TokenType::VAR)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::WHILE)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::ERROR)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::END_OF_FILE)] = {nullptr, nullptr, Precedence::NONE},
};

Parser::Parser(Scanner& scanner, ObjectAllocator& allocator)
    : _scanner(scanner)
    , _allocator(allocator)
    , _current(_scanner.scan_token())
{ }

std::expected<std::vector<ASTNodePtr>, Parser::Error> Parser::parse()
{
    std::vector<ASTNodePtr> declarations;

    while(!_match(TokenType::END_OF_FILE))
    {
        declarations.push_back(_parse_declaration());
    }

    _consume(TokenType::END_OF_FILE, "Expect end of file.");

    if(_had_error)
    {
        return std::unexpected{Parser::Error::BadToken};
    }

    return declarations;
}

ASTNodePtr Parser::_parse_expression()
{
    return _parse_precedence(Precedence::ASSIGNMENT);
}

ASTNodePtr Parser::_parse_number()
{
    std::string temp{_previous.lexeme};
    Value value{std::stod(temp)};

    return std::make_unique<ASTNode>(ASTNode{ValueNode{_previous, value}});
}

ASTNodePtr Parser::_parse_string()
{
    Value value{
        _allocator.allocate_string(_previous.lexeme.substr(1, _previous.lexeme.size() - 2), false)};

    return std::make_unique<ASTNode>(ASTNode{ValueNode{_previous, value}});
}

ASTNodePtr Parser::_parse_declaration()
{
    ASTNodePtr ret;

    if(_match(TokenType::VAR))
    {
        ret = _parse_var_declaration();
    }
    else if(_match(TokenType::FUN))
    {
        ret = _parse_function_declaration(false);
    }
    else if(_match(TokenType::CLASS))
    {
        ret = _parse_class_declaration();
    }
    else
    {
        ret = _parse_statement();
    }

    if(_panic_mode)
    {
        _synchronize();
    }

    return ret;
}

ASTNodePtr Parser::_parse_class_declaration()
{
    auto class_name = _consume(TokenType::IDENTIFIER, "Expected class name.");

    if(!class_name)
    {
        return nullptr;
    }

    std::optional<Token> superclass;

    if(_match(TokenType::LESS))
    {
        superclass = _consume(TokenType::IDENTIFIER, "Expected superclass name after '<'.");

        if(!superclass)
        {
            return nullptr;
        }
    }

    _consume(TokenType::LEFT_BRACE, "Expected '{' before class body.");

    std::vector<ASTNodePtr> methods;

    while(_current.type != TokenType::RIGHT_BRACE && _current.type != TokenType::END_OF_FILE)
    {
        methods.push_back(_parse_function_declaration(true));
    }

    auto end_brace = _consume(TokenType::RIGHT_BRACE, "Expected '}' after class body.");

    return std::make_unique<ASTNode>(ASTNode{
        ClassDeclNode{class_name.value(), superclass, std::move(methods), end_brace.value()}});
}

ASTNodePtr Parser::_parse_dot(ASTNodePtr left)
{
    auto dot = _previous;
    auto name = _consume(TokenType::IDENTIFIER, "Expected property name after '.'.");

    if(!name)
    {
        return nullptr;
    }

    return std::make_unique<ASTNode>(ASTNode{PropertyExprNode{std::move(left), dot, name.value()}});
}

ASTNodePtr Parser::_parse_block_statement()
{
    std::vector<ASTNodePtr> statements;

    while(_current.type != TokenType::RIGHT_BRACE && _current.type != TokenType::END_OF_FILE)
    {
        statements.push_back(_parse_declaration());
    }

    _consume(TokenType::RIGHT_BRACE, "Expected '}' after block.");

    return std::make_unique<ASTNode>(ASTNode{BlockStmtNode{_previous, std::move(statements)}});
}

ASTNodePtr Parser::_parse_function_declaration(bool method)
{
    auto func_name = _consume(TokenType::IDENTIFIER, "Expected function name");

    if(!func_name)
    {
        return nullptr;
    }

    _consume(TokenType::LEFT_PAREN, "Expected '(' after function name.");

    std::vector<Token> params;

    if(_current.type != TokenType::RIGHT_PAREN)
    {
        do
        {
            auto param = _consume(TokenType::IDENTIFIER, "Expected parameter name.");

            if(!param)
            {
                return nullptr;
            }

            params.push_back(param.value());

            if(params.size() >= 255)
            {
                _error("Function cannot take more than 255 parameters.");
                return nullptr;
            }
        } while(_match(TokenType::COMMA));
    }
    _consume(TokenType::RIGHT_PAREN, "Expected ') after parameter list.");
    _consume(TokenType::LEFT_BRACE, "Expected '{' before function body.");

    auto body = _parse_block_statement();

    return std::make_unique<ASTNode>(
        ASTNode{FunDeclNode{func_name.value(), std::move(params), std::move(body), method}});
}

ASTNodePtr Parser::_parse_var_declaration()
{
    _consume(TokenType::IDENTIFIER, "Expected variable name");
    auto identifier = _previous;

    ASTNodePtr initializer = nullptr;

    if(_match(TokenType::EQUAL))
    {
        initializer = _parse_expression();
    }

    _consume(TokenType::SEMICOLON, "Expect ';' after expression.");

    return std::make_unique<ASTNode>(ASTNode{VarDeclNode{identifier, std::move(initializer)}});
}

ASTNodePtr Parser::_parse_this()
{
    return _parse_variable();
}

ASTNodePtr Parser::_parse_super()
{
    auto super = _previous;
    _consume(TokenType::DOT, "Expected '.' after 'super'.");
    auto method = _consume(TokenType::IDENTIFIER, "Expected superclass method name.");

    if(!method)
    {
        return nullptr;
    }

    return std::make_unique<ASTNode>(ASTNode{SuperExprNode{super, method.value()}});
}

ASTNodePtr Parser::_parse_variable()
{
    return std::make_unique<ASTNode>(ASTNode{VariableExprNode{_previous}});
}

ASTNodePtr Parser::_parse_call(ASTNodePtr callee)
{
    auto paren = _previous;

    std::vector<ASTNodePtr> args;
    if(_current.type != TokenType::RIGHT_PAREN)
    {
        do
        {
            args.push_back(_parse_expression());

            if(args.size() >= 255)
            {
                _error("Function cannot take more than 255 arguments.");
                return nullptr;
            }
        } while(_match(TokenType::COMMA));
    }

    _consume(TokenType::RIGHT_PAREN, "Expect ')' after arguments.");

    return std::make_unique<ASTNode>(ASTNode{CallNode{std::move(callee), paren, std::move(args)}});
}

ASTNodePtr Parser::_parse_assignment_expression(ASTNodePtr left)
{
    auto* var = std::get_if<VariableExprNode>(left.get());
    auto* property = std::get_if<PropertyExprNode>(left.get());

    decltype(AssignmentExprNode::target) target;

    if(var)
    {
        target = std::move(*var);
    }
    else if(property)
    {
        target = std::move(*property);
    }
    else
    {
        _error("Invalid assignment target");
        return nullptr;
    }

    auto expr = _parse_expression();

    return std::make_unique<ASTNode>(
        ASTNode{AssignmentExprNode{std::move(target), std::move(expr)}});
}

ASTNodePtr Parser::_parse_statement()
{
    if(_match(TokenType::LEFT_BRACE))
    {
        return _parse_block_statement();
    }
    else if(_match(TokenType::IF))
    {
        return _parse_if_statement();
    }
    else if(_match(TokenType::WHILE))
    {
        return _parse_while_statement();
    }
    else if(_match(TokenType::FOR))
    {
        return _parse_for_statement();
    }
    else if(_match(TokenType::RETURN))
    {
        return _parse_return_statement();
    }
    else
    {
        return _parse_expression_statement();
    }
}

ASTNodePtr Parser::_parse_return_statement()
{
    auto keyword = _previous;

    ASTNodePtr value = nullptr;

    if(_current.type != TokenType::SEMICOLON)
    {
        value = _parse_expression();
    }

    _consume(TokenType::SEMICOLON, "Expected ';' after return statement.");

    return std::make_unique<ASTNode>(ASTNode{ReturnStmtNode{keyword, std::move(value)}});
}

ASTNodePtr Parser::_parse_for_statement()
{
    _consume(TokenType::LEFT_PAREN, "Expected '(' before for condition.");

    auto while_tok = _previous;

    ASTNodePtr initializer;

    if(_match(TokenType::SEMICOLON))
    {
        /* Do nothing - empty initializer */
    }
    else if(_match(TokenType::VAR))
    {
        initializer = _parse_var_declaration();
    }
    else
    {
        initializer = _parse_expression_statement();
    }

    ASTNodePtr condition;

    if(_match(TokenType::SEMICOLON))
    {
        /* Do nothing - empty condition */
    }
    else
    {
        condition = _parse_expression();
        _consume(TokenType::SEMICOLON, "Expected ';' after for condition.");
    }

    ASTNodePtr increment;
    Token increment_tok;

    if(_match(TokenType::RIGHT_PAREN))
    {
        /* Do nothing - empty increment */
    }
    else
    {
        increment_tok = _previous;
        increment = _parse_expression();
        _consume(TokenType::RIGHT_PAREN, "Expected ')' after for increment.");
    }

    ASTNodePtr body = _parse_statement();

    if(increment)
    {
        auto expr_stmt =
            std::make_unique<ASTNode>(ExprStmtNode{increment_tok, std::move(increment)});
        std::vector<ASTNodePtr> stmts;
        stmts.push_back(std::move(body));
        stmts.push_back(std::move(expr_stmt));
        body = std::make_unique<ASTNode>(BlockStmtNode{_previous, std::move(stmts)});
    }

    if(!condition)
    {
        condition = std::make_unique<ASTNode>(ValueNode{while_tok, true});
    }

    body =
        std::make_unique<ASTNode>(WhileStmtNode{while_tok, std::move(condition), std::move(body)});

    if(initializer)
    {
        std::vector<ASTNodePtr> stmts;
        stmts.push_back(std::move(initializer));
        stmts.push_back(std::move(body));
        body = std::make_unique<ASTNode>(BlockStmtNode{_previous, std::move(stmts)});
    }

    return body;
}

ASTNodePtr Parser::_parse_while_statement()
{
    auto while_tok = _previous;

    _consume(TokenType::LEFT_PAREN, "Expect '(' after 'while'.");
    auto condition = _parse_expression();
    _consume(TokenType::RIGHT_PAREN, "Expect ')' after while condition.");

    auto body = _parse_statement();

    return std::make_unique<ASTNode>(
        ASTNode{WhileStmtNode{while_tok, std::move(condition), std::move(body)}});
}

ASTNodePtr Parser::_parse_if_statement()
{
    auto if_tok = _previous;

    _consume(TokenType::LEFT_PAREN, "Expect '(' after 'if'.");
    auto condition = _parse_expression();
    _consume(TokenType::RIGHT_PAREN, "Expect ')' after if condition.");

    auto then_branch = _parse_statement();
    ASTNodePtr else_branch = nullptr;

    std::optional<Token> else_tok;

    if(_match(TokenType::ELSE))
    {
        else_tok = _previous;
        else_branch = _parse_statement();
    }

    return std::make_unique<ASTNode>(ASTNode{IfStmtNode{
        if_tok, else_tok, std::move(condition), std::move(then_branch), std::move(else_branch)}});
}

ASTNodePtr Parser::_parse_expression_statement()
{
    auto expr = _parse_expression();
    _consume(TokenType::SEMICOLON, "Expect ';' after expression.");

    return std::make_unique<ASTNode>(ASTNode{ExprStmtNode{_previous, std::move(expr)}});
}

ASTNodePtr Parser::_parse_literal()
{
    Value value{};

    switch(_previous.type)
    {
    case TokenType::TRUE:
        value = true;
        break;
    case TokenType::FALSE:
        value = false;
        break;
    case TokenType::NIL:
        // The default value is NIL
        break;
    default:
        std::unreachable();
    }

    return std::make_unique<ASTNode>(ASTNode{ValueNode{_previous, value}});
}

ASTNodePtr Parser::_parse_grouping()
{
    auto tok = _previous;
    auto expr = _parse_expression();
    _consume(TokenType::RIGHT_PAREN, "Expect ')' after expression.");

    return std::make_unique<ASTNode>(ASTNode{GroupExprNode{tok, std::move(expr)}});
}

ASTNodePtr Parser::_parse_unary_expression()
{
    auto op = _previous;
    auto right = _parse_precedence(Precedence::UNARY);

    return std::make_unique<ASTNode>(ASTNode{UnaryExprNode{op, std::move(right)}});
}

ASTNodePtr Parser::_parse_binary_expression(ASTNodePtr left)
{
    auto op = _previous;
    const auto& rule = _parse_rules[type_to_int(op.type)];

    auto right =
        _parse_precedence(static_cast<Precedence>(static_cast<uint8_t>(rule.precedence) + 1));

    return std::make_unique<ASTNode>(ASTNode{BinExprNode{op, std::move(left), std::move(right)}});
}

ASTNodePtr Parser::_parse_precedence(Precedence precedence)
{
    _advance();
    auto prefix_rule = _parse_rules[type_to_int(_previous.type)].prefix;

    if(prefix_rule == nullptr)
    {
        _error("Expected expression.");
        return nullptr;
    }

    auto left = prefix_rule(this);

    while(precedence <= _parse_rules[type_to_int(_current.type)].precedence)
    {
        _advance();
        auto infix_rule = _parse_rules[type_to_int(_previous.type)].infix;
        left = infix_rule(this, std::move(left));
    }

    return left;
}

void Parser::_advance()
{
    _previous = _current;
    _current = _scanner.scan_token();

    if(_current.type == TokenType::ERROR)
    {
        _error_at_current(_current.lexeme);
    }
}

void Parser::_error_at_current(std::string_view message)
{
    _error_at(_current, message);
}

void Parser::_error(std::string_view message)
{
    _error_at(_previous, message);
}

void Parser::_error_at(const Token& token, std::string_view message)
{
    if(_panic_mode)
        return;

    std::print(stderr, "[line {}] Error", token.line);

    if(token.type == TokenType::END_OF_FILE)
    {
        std::print(stderr, " at end");
    }
    else if(token.type == TokenType::ERROR)
    {
        // Nothing.
    }
    else
    {
        std::print(stderr, " at '{:{}s}'", token.lexeme, token.lexeme.size());
    }

    std::println(stderr, ": {}", message);
    _had_error = true;
    _panic_mode = true;
}

bool Parser::_match(TokenType type)
{
    if(_current.type != type)
        return false;

    _advance();
    return true;
}

std::optional<Token> Parser::_consume(TokenType type, std::string_view message)
{
    if(_current.type == type)
    {
        const auto temp = _current;
        _advance();
        return temp;
    }

    _error_at_current(message);

    return std::nullopt;
}

void Parser::_synchronize()
{
    _panic_mode = false;

    while(_current.type != TokenType::END_OF_FILE)
    {
        if(_previous.type == TokenType::SEMICOLON)
            return;

        switch(_current.type)
        {
        case TokenType::CLASS:
        case TokenType::FUN:
        case TokenType::VAR:
        case TokenType::FOR:
        case TokenType::IF:
        case TokenType::WHILE:
        case TokenType::RETURN:
            return;
        default:
            break;
        }

        _advance();
    }
}
} // namespace lox