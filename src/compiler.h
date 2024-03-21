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
public:
    enum class Error
    {
        LocalVariableLimitExceeded,
        RedefinedVariableInSameScope,
        ChunkConstantLimitExceeded
    };

private:
    Chunk* _current_chunk = nullptr;
    ObjectAllocator& _allocator;

    struct Local
    {
        Token name;
        // -1 indicates that the variable is not initialized
        int depth = -1;
    };

    // To keep track of scopes
    int _scope_depth = 0;
    int _local_count = 0;
    Local _locals[UINT8_MAX + 1];

    void _begin_scope()
    {
        ++_scope_depth;
    }

    int _resolve_local(const Token& name);

    void _end_scope(const Token&);
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

    class Exception : public std::exception
    {
    public:
        Exception(const Token& token, Error error)
            : token(token)
            , error(error)
        { }
        const char* what() const noexcept override
        {
            return "Compilation error";
        }

        const Token token;
        const Error error;
    };

    std::string _get_error_message(const Exception&) const;

public:
    Compiler(ObjectAllocator&);

    std::expected<Chunk, Error> compile(const std::vector<ASTNodePtr>& declarations);

    void operator()(const BinExprNode&);
    void operator()(const ValueNode&);
    void operator()(const GroupExprNode&);
    void operator()(const UnaryExprNode&);
    void operator()(const PrintStmtNode&);
    void operator()(const ExprStmtNode&);
    void operator()(const BlockStmtNode&);
    void operator()(const VarDeclNode&);
    void operator()(const VariableExprNode&);
    void operator()(const AssignmentExprNode&);
};
} // namespace lox

#endif // LOX_COMPILER_H