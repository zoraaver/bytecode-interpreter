#ifndef LOX_COMPILER_H
#define LOX_COMPILER_H

#include <cassert>
#include <cstdint>
#include <expected>
#include <string_view>

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
        LoopLimitExceeded,
        ReturnOutsideFunction,
        UpvalueLimitExceeded,
        ThisOutsideClass,
        ReturnInsideInitializer
    };

private:
    FunctionObject* _function = nullptr;
    ObjectAllocator& _allocator;
    Compiler* _enclosing = nullptr;

    enum class FunctionType
    {
        SCRIPT,
        FUNCTION,
        METHOD,
        INITIALIZER
    };

    const FunctionType _type = FunctionType::SCRIPT;

    struct ClassCompiler
    {
        ClassCompiler(ClassCompiler* enclosing)
            : enclosing(enclosing)
        { }
        ClassCompiler* enclosing = nullptr;
    };

    ClassCompiler _current_class;

    struct Local
    {
        Token name;
        // -1 indicates that the variable is not initialized
        int depth = -1;
        bool is_captured = false;
    };

    struct UpValue
    {
        uint8_t index;
        bool is_local;
    };

    // To keep track of scopes
    int _scope_depth = 0;
    // We reserve the first local slot for internal VM use.
    int _local_count = 1;

    static constexpr int MAX_LOCALS = UINT8_MAX + 1;
    static constexpr int MAX_UPVALUES = UINT8_MAX + 1;

    Local _locals[MAX_LOCALS] = {{.name = Token{}, .depth = 0}};
    UpValue _upvalues[MAX_UPVALUES];

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
    int _resolve_upvalue(const Token& name);
    int _add_upvalue(const Token& tok, uint8_t index, bool is_local);

    void _end_scope(const Token&);
    void _emit_byte(uint8_t byte, int line);

    void _emit_bytecode(OpCode code, int line)
    {
        _emit_byte(static_cast<uint8_t>(code), line);
    };

    void _emit_return(int line);

    void _emit_bytes(uint8_t byte_1, uint8_t byte_2, int line)
    {
        _emit_byte(byte_1, line);
        _emit_byte(byte_2, line);
    }

    int _emit_jump(OpCode instruction, int line);
    void _patch_jump(int offset, const Token& tok);

    void _emit_loop(uint32_t loop_start, const Token&);

    uint8_t _make_constant(const Value& value);

    void _compile_and_expression(const BinExprNode&);
    void _compile_or_expression(const BinExprNode&);

    void _define_variable(const Token& identifier);
    // We allocate a function on the heap and return since the object needs to
    // live for the duration of the program.
    FunctionObject* _compile_function(std::string_view name,
                                      const std::vector<Token>& params,
                                      const std::vector<ASTNodePtr>& declarations);

    void _compile_named_variable(const Token& name);

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
    Compiler(ObjectAllocator&, FunctionType = FunctionType::SCRIPT, Compiler* = nullptr);

    std::expected<FunctionObject*, Error> compile(const std::vector<ASTNodePtr>& declarations);

    void operator()(const BinExprNode&);
    void operator()(const ValueNode&);
    void operator()(const GroupExprNode&);
    void operator()(const UnaryExprNode&);
    void operator()(const ExprStmtNode&);
    void operator()(const BlockStmtNode&);
    void operator()(const VarDeclNode&);
    void operator()(const VariableExprNode&);
    void operator()(const AssignmentExprNode&);
    void operator()(const PropertyExprNode&);
    void operator()(const IfStmtNode&);
    void operator()(const WhileStmtNode&);
    void operator()(const ReturnStmtNode&);
    void operator()(const FunDeclNode&);
    void operator()(const ClassDeclNode&);
    void operator()(const CallNode&);
};
} // namespace lox

#endif // LOX_COMPILER_H