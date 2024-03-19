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
    default:
        return;
    }
}

void Compiler::operator()(const GroupExprNode& node)
{
    std::visit(*this, *node.expr);
}

void Compiler::operator()(const ValueNode& node)
{
    _emit_bytes(
        static_cast<uint8_t>(OpCode::CONSTANT), _make_constant(node.value), node.token.line);
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
    default:
        return; // Unreachable.
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