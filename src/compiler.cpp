#include "compiler.h"

#include <cstdint>
#include <print>
#include <stdexcept>
#include <variant>

#include "chunk.h"
#include "parser.h"
#include "value.h"

namespace lox
{

Compiler::Compiler() { }

std::expected<Chunk, Compiler::Error> Compiler::compile(const ASTNode& ast)
{
    Chunk chunk;

    _current_chunk = &chunk;

    int line = -1;

    std::visit(*this, ast);

    _emit_return(0);

    return chunk;
}

void Compiler::operator()(const BinExprNode& node)
{
    std::visit(*this, *node.left);
    std::visit(*this, *node.right);

    const auto& line = node.op.line;

    switch(node.op.type)
    {
    case TokenType::PLUS:
        _emit_bytecode(OpCode::ADD, line);
        break;
    case TokenType::MINUS:
        _emit_bytecode(OpCode::SUBTRACT, line);
        break;
    case TokenType::STAR:
        _emit_bytecode(OpCode::MULTIPLY, line);
        break;
    case TokenType::SLASH:
        _emit_bytecode(OpCode::DIVIDE, line);
        break;
    case TokenType::EQUAL_EQUAL:
        _emit_bytecode(OpCode::EQUAL, line);
        break;
    case TokenType::BANG_EQUAL:
        _emit_bytes(static_cast<uint8_t>(OpCode::EQUAL), static_cast<uint8_t>(OpCode::NOT), line);
        break;
    case TokenType::GREATER:
        _emit_bytecode(OpCode::GREATER, line);
        break;
    case TokenType::LESS:
        _emit_bytecode(OpCode::LESS, line);
        break;
    case TokenType::GREATER_EQUAL:
        _emit_bytes(static_cast<uint8_t>(OpCode::LESS), static_cast<uint8_t>(OpCode::NOT), line);
        break;
    case TokenType::LESS_EQUAL:
        _emit_bytes(static_cast<uint8_t>(OpCode::GREATER), static_cast<uint8_t>(OpCode::NOT), line);
        break;
    default:
        std::unreachable();
    }
}

void Compiler::operator()(const GroupExprNode& node)
{
    std::visit(*this, *node.expr);
}

void Compiler::operator()(const ValueNode& node)
{
    switch(node.value.get_type())
    {
    case ValueType::NUMBER:
        _emit_bytes(
            static_cast<uint8_t>(OpCode::CONSTANT), _make_constant(node.value), node.token.line);
        break;
    case ValueType::BOOL:
        _emit_bytecode(node.value.as_bool() ? OpCode::TRUE : OpCode::FALSE, node.token.line);
        break;
    case ValueType::NIL:
        _emit_bytecode(OpCode::NIL, node.token.line);
        break;
    }
}

void Compiler::operator()(const UnaryExprNode& node)
{
    std::visit(*this, *node.right);

    // Emit the operator instruction.
    switch(node.op.type)
    {
    case TokenType::MINUS:
        _emit_bytecode(OpCode::NEGATE, node.op.line);
        break;
    case TokenType::BANG:
        _emit_bytecode(OpCode::NOT, node.op.line);
        break;
    default:
        std::unreachable();
    }
}

void Compiler::_emit_byte(uint8_t byte, int line)
{
    _current_chunk->write(byte, line);
}

uint8_t Compiler::_make_constant(Value value)
{
    auto index = _current_chunk->add_constant(value);

    if(index > UINT8_MAX)
    {
        throw std::runtime_error("Too many constants in one chunk.");
    }

    return index;
}

} // namespace lox