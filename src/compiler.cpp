#include "compiler.h"

#include <cstdint>
#include <print>
#include <variant>
#include <vector>

#include "chunk.h"
#include "object.h"
#include "parser.h"
#include "scanner.h"
#include "value.h"

namespace lox
{

Compiler::Compiler(ObjectAllocator& allocator)
    : _allocator(allocator)
{ }

std::expected<Chunk, Compiler::Error> Compiler::compile(const std::vector<ASTNodePtr>& declarations)
{
    Chunk chunk;

    _current_chunk = &chunk;

    int line = -1;

    for(auto& node : declarations)
    {
        try
        {
            std::visit(*this, *node);
        }
        catch(const Exception& ex)
        {
            auto msg = _get_error_message(ex);
            std::println("{}", msg);
            return std::unexpected(ex.error);
        }
    }

    _emit_return(0);

    return chunk;
}

std::string Compiler::_get_error_message(const Exception& ex) const
{
    switch(ex.error)
    {
    case Error::LocalVariableLimitExceeded:
        return std::format(
            "Local variable limit exceeded: line [{}] at '{}'", ex.token.line, ex.token.lexeme);
    case Error::RedefinedVariableInSameScope:
        return std::format(
            "Redefined variable in same scope: line [{}] at '{}'", ex.token.line, ex.token.lexeme);
    case Error::ChunkConstantLimitExceeded:
        return "Chunk constant limit exceeded";
    case Error::JumpLimitExceeded:
        return "Jump limit exceeded";
    case Error::LoopLimitExceeded:
        return "Loop limit exceeded";
    }
}

void Compiler::operator()(const BinExprNode& node)
{
    const auto& line = node.op.line;

    std::visit(*this, *node.left);

    if(node.op.type == TokenType::AND)
    {
        _compile_and_expression(node);
        return;
    }
    else if(node.op.type == TokenType::OR)
    {
        _compile_or_expression(node);
        return;
    }

    std::visit(*this, *node.right);

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

void Compiler::_compile_and_expression(const BinExprNode& node)
{
    // At this point, we've already compiled the left hand side of the expression.
    auto jump = _emit_jump(OpCode::JUMP_IF_FALSE, node.op.line);

    _emit_bytecode(OpCode::POP, node.op.line);

    std::visit(*this, *node.right);

    _patch_jump(jump, node.op);
}

void Compiler::_compile_or_expression(const BinExprNode& node)
{
    // At this point, we've already compiled the left hand side of the expression.
    auto jump = _emit_jump(OpCode::JUMP_IF_TRUE, node.op.line);

    _emit_bytecode(OpCode::POP, node.op.line);

    std::visit(*this, *node.right);

    _patch_jump(jump, node.op);
}

void Compiler::operator()(const GroupExprNode& node)
{
    std::visit(*this, *node.expr);
}

void Compiler::operator()(const PrintStmtNode& node)
{
    std::visit(*this, *node.expr);

    _emit_bytecode(OpCode::PRINT, node.token.line);
}

void Compiler::operator()(const ExprStmtNode& node)
{
    std::visit(*this, *node.expr);

    _emit_bytecode(OpCode::POP, node.token.line);
}

void Compiler::operator()(const IfStmtNode& node)
{
    std::visit(*this, *node.condition);

    auto then_jump = _emit_jump(OpCode::JUMP_IF_FALSE, node.if_tok.line);

    // Pop the condition from the stack.
    _emit_bytecode(OpCode::POP, node.if_tok.line);

    std::visit(*this, *node.then_branch);

    auto else_jump = _emit_jump(
        OpCode::JUMP, node.else_tok.has_value() ? node.else_tok.value().line : node.if_tok.line);

    _patch_jump(then_jump, node.if_tok);

    // Pop the condition from the stack.
    _emit_bytecode(OpCode::POP, node.if_tok.line);

    if(node.else_branch)
    {
        std::visit(*this, *node.else_branch);
    }

    _patch_jump(else_jump, node.else_tok.has_value() ? node.else_tok.value() : node.if_tok);
}

void Compiler::operator()(const WhileStmtNode& node)
{
    uint32_t loop_start = _current_chunk->size();
    std::visit(*this, *node.condition);

    auto exit_jump = _emit_jump(OpCode::JUMP_IF_FALSE, node.while_tok.line);

    // Pop the condition from the stack.
    _emit_bytecode(OpCode::POP, node.while_tok.line);

    std::visit(*this, *node.body);

    _emit_loop(loop_start, node.while_tok);

    _patch_jump(exit_jump, node.while_tok);

    // Pop the condition from the stack.
    _emit_bytecode(OpCode::POP, node.while_tok.line);
}

void Compiler::_emit_loop(uint32_t loop_start, const Token& tok)
{
    _emit_bytecode(OpCode::LOOP, tok.line);

    uint32_t loop_offset = _current_chunk->size() - loop_start + 2;

    if(loop_offset > UINT16_MAX)
    {
        throw Exception{tok, Error::LoopLimitExceeded};
    }

    _emit_bytes((loop_offset >> 8) & 0xff, loop_offset & 0xff, tok.line);
}

void Compiler::_patch_jump(int offset, const Token& tok)
{
    uint32_t jump = (_current_chunk->size() - 2) - offset;

    if(jump > UINT16_MAX)
    {
        throw Exception{tok, Error::JumpLimitExceeded};
    }

    (*_current_chunk)[offset] = (jump >> 8) & 0xff;
    (*_current_chunk)[offset + 1] = jump & 0xff;
}

int Compiler::_emit_jump(OpCode instruction, int line)
{
    _emit_bytecode(instruction, line);
    _emit_bytes(0xff, 0xff, line);

    return _current_chunk->size() - 2;
}

void Compiler::operator()(const BlockStmtNode& block_node)
{
    _begin_scope();

    for(const auto& node : block_node.statements)
    {
        std::visit(*this, *node);
    }

    _end_scope(block_node.end_brace);
}

void Compiler::_end_scope(const Token& brace)
{
    --_scope_depth;

    while(_local_count > 0 && _locals[_local_count - 1].depth > _scope_depth)
    {
        _emit_bytecode(OpCode::POP, brace.line);
        --_local_count;
    }
}

void Compiler::operator()(const VarDeclNode& node)
{
    if(node.initializer)
    {
        std::visit(*this, *node.initializer);
    }
    else
    {
        _emit_bytecode(OpCode::NIL, node.identifier.line);
    }

    if(_scope_depth == 0)
    {
        auto index = _make_constant(Value{_allocator.allocate_string(node.identifier.lexeme)});
        _emit_bytes(static_cast<uint8_t>(OpCode::DEFINE_GLOBAL), index, node.identifier.line);

        return;
    }

    // Check for any local variables declared in the same scope with the same
    // name.
    for(auto i = _local_count - 1; i >= 0; i--)
    {
        auto& local = _locals[i];

        if(local.depth != -1 && local.depth < _scope_depth)
        {
            break;
        }

        if(local.name.lexeme == node.identifier.lexeme)
        {
            throw Exception{node.identifier, Error::RedefinedVariableInSameScope};
        }
    }

    if(_local_count == (sizeof(_locals) / sizeof(Local)))
    {
        throw Exception{node.identifier, Error::LocalVariableLimitExceeded};
    }

    _locals[_local_count++] = {.name = node.identifier, .depth = _scope_depth};
}

void Compiler::operator()(const VariableExprNode& node)
{
    auto arg = _resolve_local(node.var);
    OpCode op = OpCode::GET_LOCAL;

    if(arg == -1)
    {
        arg = _make_constant(Value{_allocator.allocate_string(node.var.lexeme)});
        op = OpCode::GET_GLOBAL;
    }

    _emit_bytes(static_cast<uint8_t>(op), arg, node.var.line);
}

void Compiler::operator()(const AssignmentExprNode& node)
{
    std::visit(*this, *node.value);

    auto arg = _resolve_local(node.target.var);
    OpCode op = OpCode::SET_LOCAL;

    if(arg == -1)
    {
        arg = _make_constant(Value{_allocator.allocate_string(node.target.var.lexeme)});
        op = OpCode::SET_GLOBAL;
    }

    _emit_bytes(static_cast<uint8_t>(op), arg, node.target.var.line);
}

int Compiler::_resolve_local(const Token& name)
{
    for(auto i = _local_count - 1; i >= 0; i--)
    {
        auto& local = _locals[i];

        if(local.name.lexeme == name.lexeme)
        {
            return i;
        }
    }

    return -1;
}

void Compiler::operator()(const ValueNode& node)
{
    switch(node.value.get_type())
    {
    case ValueType::OBJECT:
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
        throw Exception{Token{}, Error::ChunkConstantLimitExceeded};
    }

    return index;
}

} // namespace lox