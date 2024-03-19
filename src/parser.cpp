#include "parser.h"
#include "scanner.h"

#include <cstdint>
#include <print>

namespace lox
{

Parser::ParseRule Parser::_parse_rules[] = {
    [type_to_int(TokenType::LEFT_PAREN)] = {&Parser::_parse_grouping, nullptr, Precedence::NONE},
    [type_to_int(TokenType::RIGHT_PAREN)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::LEFT_BRACE)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::RIGHT_BRACE)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::COMMA)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::DOT)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::MINUS)] = {&Parser::_parse_unary_expression,
                                       &Parser::_parse_binary_expression,
                                       Precedence::TERM},
    [type_to_int(TokenType::PLUS)] = {nullptr, &Parser::_parse_binary_expression, Precedence::TERM},
    [type_to_int(TokenType::SEMICOLON)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(
        TokenType::SLASH)] = {nullptr, &Parser::_parse_binary_expression, Precedence::FACTOR},
    [type_to_int(
        TokenType::STAR)] = {nullptr, &Parser::_parse_binary_expression, Precedence::FACTOR},
    [type_to_int(TokenType::BANG)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::BANG_EQUAL)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::EQUAL)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::EQUAL_EQUAL)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::GREATER)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::GREATER_EQUAL)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::LESS)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::LESS_EQUAL)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::IDENTIFIER)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::STRING)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::NUMBER)] = {&Parser::_parse_number, nullptr, Precedence::NONE},
    [type_to_int(TokenType::AND)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::CLASS)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::ELSE)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::FALSE)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::FOR)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::FUN)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::IF)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::NIL)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::OR)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::PRINT)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::RETURN)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::SUPER)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::THIS)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::TRUE)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::VAR)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::WHILE)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::ERROR)] = {nullptr, nullptr, Precedence::NONE},
    [type_to_int(TokenType::END_OF_FILE)] = {nullptr, nullptr, Precedence::NONE},
};

Parser::Parser(Scanner& scanner)
    : _scanner(scanner)
    , _current(_scanner.scan_token())
{ }

ASTNodePtr Parser::parse()
{
    auto expr = _parse_expression();

    _consume(TokenType::END_OF_FILE, "Expect end of file.");

    if(_had_error)
    {
        return nullptr;
    }

    return expr;
}

ASTNodePtr Parser::_parse_expression()
{
    return _parse_precedence(Precedence::ASSIGNMENT);
}

ASTNodePtr Parser::_parse_number()
{
    std::string temp{_previous.lexeme};
    auto value = std::stod(temp);

    return std::make_unique<ASTNode>(ASTNode{value});
}

ASTNodePtr Parser::_parse_grouping()
{
    auto expr = _parse_expression();
    _consume(TokenType::RIGHT_PAREN, "Expect ')' after expression.");

    return std::make_unique<ASTNode>(ASTNode{GroupExprNode{std::move(expr)}});
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

void Parser::_consume(TokenType type, std::string_view message)
{
    if(_current.type == type)
    {
        _advance();
        return;
    }

    _error_at_current(message);
}
} // namespace lox