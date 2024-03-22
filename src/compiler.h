#ifndef LOX_COMPILER_H
#define LOX_COMPILER_H

#include <cassert>
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
        ChunkConstantLimitExceeded,
        JumpLimitExceeded,
        LoopLimitExceeded
    };

private:
    enum class FunctionType
    {
        SCRIPT,
        FUNCTION
    };

    FunctionObject* _function = nullptr;
    FunctionType _type = FunctionType::SCRIPT;
    ObjectAllocator& _allocator;

    struct Local
    {
        Token name;
        // -1 indicates that the variable is not initialized
        int depth = -1;
    };

    // To keep track of scopes
    int _scope_depth = 0;
    // We reserve the first local slot for internal VM use.
    int _local_count = 1;
    Local _locals[UINT8_MAX + 1] = {{.name = Token{}, .depth = 0}};

    Chunk& _current_chunk()
    {
        assert(_function && "Function is not defined");
        return _function->chunk;
    }

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

    int _emit_jump(OpCode instruction, int line);
    void _patch_jump(int offset, const Token& tok);

    void _emit_loop(uint32_t loop_start, const Token&);

    uint8_t _make_constant(Value value);

    void _compile_and_expression(const BinExprNode&);
    void _compile_or_expression(const BinExprNode&);

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

    std::expected<FunctionObject, Error> compile(const std::vector<ASTNodePtr>& declarations);

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
    void operator()(const IfStmtNode&);
    void operator()(const WhileStmtNode&);
};
} // namespace lox

#endif // LOX_COMPILER_H