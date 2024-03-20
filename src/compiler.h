#ifndef LOX_COMPILER_H
#define LOX_COMPILER_H

#include <cstdint>
#include <expected>

#include "chunk.h"
#include "object.h"
#include "parser.h"

namespace lox
{

class Compiler
{
    Chunk* _current_chunk = nullptr;
    ObjectAllocator& _allocator;

    void _emit_byte(uint8_t byte, int line);

    void _emit_bytecode(OpCode code, int line)
    {
        _emit_byte(static_cast<uint8_t>(code), line);
    };

    void _emit_return(int line)
    {
        _emit_bytecode(OpCode::RETURN, line);
    }

    void _emit_bytes(uint8_t byte_1, uint8_t byte_2, int line)
    {
        _emit_byte(byte_1, line);
        _emit_byte(byte_2, line);
    }

    uint8_t _make_constant(Value value);

public:
    Compiler(ObjectAllocator&);

    enum Error
    {
    };

    std::expected<Chunk, Error> compile(const std::vector<ASTNodePtr>& declarations);

    void operator()(const BinExprNode&);
    void operator()(const ValueNode&);
    void operator()(const GroupExprNode&);
    void operator()(const UnaryExprNode&);
    void operator()(const PrintStmtNode&);
    void operator()(const ExprStmtNode&);
    void operator()(const VarDeclNode&);
    void operator()(const VariableExprNode&);
    void operator()(const AssignmentExprNode&);
};
} // namespace lox

#endif // LOX_COMPILER_H