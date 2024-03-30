#include "compiler.h"

#include <cassert>
#include <cstdint>
#include <format>
#include <optional>
#include <print>
#include <string_view>
#include <sys/stat.h>
#include <variant>
#include <vector>

#include "chunk.h"
#include "object.h"
#include "parser.h"
#include "scanner.h"
#include "value.h"

namespace lox
{

Compiler::Compiler(ObjectAllocator& allocator, FunctionType type, Compiler* enclosing)
    : _allocator(allocator)
    , _type(type)
    , _enclosing(enclosing)
    , _current_class(enclosing ? enclosing->_current_class : std::nullopt)
{
    if(_type == FunctionType::METHOD || _type == FunctionType::INITIALIZER)
    {
        _locals[0].name.lexeme = "this";
        _locals[0].name.type = TokenType::THIS;
    }
}

std::expected<FunctionObject*, Compiler::Error>
Compiler::compile(const std::vector<ASTNodePtr>& declarations)
{
    _function = _allocator.allocate<FunctionObject>(false, "", 0);

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

    return _function;
}

void Compiler::_emit_return(int line)
{
    if(_type == FunctionType::INITIALIZER)
    {
        _emit_bytecode(OpCode::GET_LOCAL, line);
        // 'this' is always the first local slot
        _emit_byte(0, line);
    }
    else
    {
        _emit_bytecode(OpCode::NIL, line);
    }

    _emit_bytecode(OpCode::RETURN, line);
}

FunctionObject* Compiler::_compile_function(std::string_view name,
                                            const std::vector<Token>& params,
                                            const std::vector<ASTNodePtr>& declarations)
{
    _function = _allocator.allocate<FunctionObject>(
        false, std::string{name}, static_cast<uint8_t>(params.size()));

    _begin_scope();

    for(auto& param : params)
    {
        _define_variable(param);
    }

    for(auto& node : declarations)
    {
        std::visit(*this, *node);
    }

    _emit_return(0);

    return _function;
}

std::string Compiler::_get_error_message(const Exception& ex) const
{
    switch(ex.error)
    {
    case Error::LocalVariableLimitExceeded:
        return std::format(
            "Local variable limit exceeded: line [{}] at '{}'", ex.token.line, ex.token.lexeme);
    case Error::UpvalueLimitExceeded:
        return std::format(
            "Upvalue variable limit exceeded: line [{}] at '{}'", ex.token.line, ex.token.lexeme);
    case Error::RedefinedVariableInSameScope:
        return std::format(
            "Redefined variable in same scope: line [{}] at '{}'", ex.token.line, ex.token.lexeme);
    case Error::ChunkConstantLimitExceeded:
        return "Chunk constant limit exceeded";
    case Error::JumpLimitExceeded:
        return "Jump limit exceeded";
    case Error::LoopLimitExceeded:
        return "Loop limit exceeded";
    case Error::ReturnOutsideFunction:
        return std::format(
            "Return outside function: line [{}] at '{}'", ex.token.line, ex.token.lexeme);
    case Error::ThisOutsideClass:
        return std::format("This outside class: line [{}] at '{}'", ex.token.line, ex.token.lexeme);
    case Error::ReturnInsideInitializer:
        return std::format(
            "Return inside initializer: line [{}] at '{}'", ex.token.line, ex.token.lexeme);
    case Error::CyclicInheritance:
        return std::format("Cyclic inheritance: line [{}] at '{}'", ex.token.line, ex.token.lexeme);
    case Error::SuperUsedInClassWithNoSuperClass:
        return std::format("Super used in class with no super class: line [{}] at '{}'",
                           ex.token.line,
                           ex.token.lexeme);
    case Error::SuperUsedOutsideClass:
        return std::format(
            "Super used outside class: line [{}] at '{}'", ex.token.line, ex.token.lexeme);
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

void Compiler::operator()(const PropertyExprNode& node)
{
    std::visit(*this, *node.instance);

    auto name = _make_constant(Value{_allocator.allocate_string(node.name.lexeme, false)});

    _emit_bytes(static_cast<uint8_t>(OpCode::GET_PROPERTY), name, node.name.line);
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
    uint32_t loop_start = _current_chunk().size();
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

void Compiler::operator()(const ReturnStmtNode& node)
{
    if(_type == FunctionType::SCRIPT)
    {
        throw Exception{node.keyword, Error::ReturnOutsideFunction};
    }

    if(node.value && _type == FunctionType::INITIALIZER)
    {
        throw Exception{node.keyword, Error::ReturnInsideInitializer};
    }
    else if(node.value)
    {
        std::visit(*this, *node.value);
        _emit_bytecode(OpCode::RETURN, node.keyword.line);
    }
    else
    {
        _emit_return(node.keyword.line);
    }
}

void Compiler::operator()(const FunDeclNode& node)
{
    auto type = FunctionType::FUNCTION;

    if(node.method && node.name.lexeme == "init")
    {
        type = FunctionType::INITIALIZER;
    }
    else if(node.method)
    {
        type = FunctionType::METHOD;
    }

    Compiler func_compiler{_allocator, type, this};

    auto& body = std::get<BlockStmtNode>(*node.body);

    auto func = func_compiler._compile_function(node.name.lexeme, node.params, body.statements);

    _emit_bytes(static_cast<uint8_t>(OpCode::CLOSURE), _make_constant(Value{func}), node.name.line);

    for(int i = 0; i < func->upvalue_count; ++i)
    {
        _emit_byte(func_compiler._upvalues[i].is_local ? 1 : 0, node.name.line);
        _emit_byte(func_compiler._upvalues[i].index, node.name.line);
    }

    if(node.method)
    {
        auto index = _make_constant(Value{_allocator.allocate_string(node.name.lexeme, false)});
        _emit_bytes(static_cast<uint8_t>(OpCode::METHOD), index, node.name.line);
    }
    else
    {
        _define_variable(node.name);
    }
}

void Compiler::operator()(const ClassDeclNode& node)
{
    auto prev = _current_class;
    _current_class = ClassCompiler{};

    auto constant = _make_constant(Value{_allocator.allocate_string(node.name.lexeme, false)});

    _emit_bytes(static_cast<uint8_t>(OpCode::CLASS), constant, node.name.line);

    _define_variable(node.name);

    if(node.superclass)
    {
        if(node.superclass.value().lexeme == node.name.lexeme)
        {
            throw Exception{node.name, Error::CyclicInheritance};
        }

        _begin_scope();
        _define_variable({.type = TokenType::SUPER, .line = node.name.line, .lexeme = "super"});
        // Push the superclass and then the subclass variables onto the stack.
        _compile_named_variable(node.superclass.value());
        _compile_named_variable(node.name);
        _emit_bytecode(OpCode::INHERIT, node.name.line);

        _current_class->has_superclass = true;
    }

    // Push the class on the stack so it's availabe when defining all the
    // methods.
    _compile_named_variable(node.name);

    for(auto& method : node.methods)
    {
        std::visit(*this, *method);
    }

    // Pop the class off the stack
    _emit_bytecode(OpCode::POP, node.end_brace.line);

    if(_current_class->has_superclass)
    {
        _end_scope(node.end_brace);
    }

    _current_class = prev;
}

void Compiler::operator()(const SuperExprNode& node)
{
    if(!_current_class)
    {
        throw Exception{node.super, Error::SuperUsedOutsideClass};
    }

    if(!_current_class->has_superclass)
    {
        throw Exception{node.super, Error::SuperUsedInClassWithNoSuperClass};
    }

    auto index = _make_constant(Value{_allocator.allocate_string(node.method.lexeme, false)});

    _compile_named_variable(node.super);
    _compile_named_variable({TokenType::THIS, node.super.line, "this"});
    _emit_bytes(static_cast<uint8_t>(OpCode::GET_SUPER), index, node.super.line);
}

void Compiler::operator()(const CallNode& node)
{
    auto method = std::get_if<PropertyExprNode>(node.callee.get());
    auto super = std::get_if<SuperExprNode>(node.callee.get());

    // Optimize for the case where we're calling a method on an instance directly.
    if(method)
    {
        // Compile the instance but don't compile the property itself.
        std::visit(*this, *method->instance);
    }
    // Optimize for the case where we're calling a method on the superclass directly.
    else if(!super)
    {
        std::visit(*this, *node.callee);
    }

    for(auto& arg : node.args)
    {
        std::visit(*this, *arg);
    }

    if(method)
    {
        auto name = _make_constant(Value{_allocator.allocate_string(method->name.lexeme, false)});

        _emit_bytes(static_cast<uint8_t>(OpCode::INVOKE), name, node.paren.line);
        _emit_byte(node.args.size(), node.paren.line);
    }
    else if(super)
    {
        auto name = _make_constant(Value{_allocator.allocate_string(super->method.lexeme, false)});
        _compile_named_variable(super->super);
        _emit_bytes(static_cast<uint8_t>(OpCode::SUPER_INVOKE), name, super->method.line);
        _emit_byte(node.args.size(), node.paren.line);
    }
    else
    {
        _emit_bytes(static_cast<uint8_t>(OpCode::CALL), node.args.size(), node.paren.line);
    }
}

void Compiler::_emit_loop(uint32_t loop_start, const Token& tok)
{
    _emit_bytecode(OpCode::LOOP, tok.line);

    uint32_t loop_offset = _current_chunk().size() - loop_start + 2;

    if(loop_offset > UINT16_MAX)
    {
        throw Exception{tok, Error::LoopLimitExceeded};
    }

    _emit_bytes((loop_offset >> 8) & 0xff, loop_offset & 0xff, tok.line);
}

void Compiler::_patch_jump(int offset, const Token& tok)
{
    uint32_t jump = (_current_chunk().size() - 2) - offset;

    if(jump > UINT16_MAX)
    {
        throw Exception{tok, Error::JumpLimitExceeded};
    }

    _current_chunk()[offset] = (jump >> 8) & 0xff;
    _current_chunk()[offset + 1] = jump & 0xff;
}

int Compiler::_emit_jump(OpCode instruction, int line)
{
    _emit_bytecode(instruction, line);
    _emit_bytes(0xff, 0xff, line);

    return _current_chunk().size() - 2;
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
        OpCode op;
        if(_locals[_local_count - 1].is_captured)
        {
            op = OpCode::CLOSE_UPVALUE;
        }
        else
        {
            op = OpCode::POP;
        }
        _emit_bytecode(op, brace.line);
        --_local_count;
    }
}

void Compiler::_add_local(const Token& identifier)
{
    // Check for any local variables declared in the same scope with the same
    // name.
    for(auto i = _local_count - 1; i >= 0; i--)
    {
        auto& local = _locals[i];

        if(local.depth != -1 && local.depth < _scope_depth)
        {
            break;
        }

        if(local.name.lexeme == identifier.lexeme)
        {
            throw Exception{identifier, Error::RedefinedVariableInSameScope};
        }
    }

    if(_local_count == MAX_LOCALS)
    {
        throw Exception{identifier, Error::LocalVariableLimitExceeded};
    }

    _locals[_local_count++] = {.name = identifier, .depth = _scope_depth};
}

void Compiler::_define_variable(const Token& identifier)
{
    if(_scope_depth == 0)
    {
        auto index = _make_constant(Value{_allocator.allocate_string(identifier.lexeme, false)});
        _emit_bytes(static_cast<uint8_t>(OpCode::DEFINE_GLOBAL), index, identifier.line);

        return;
    }

    _add_local(identifier);
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

    _define_variable(node.identifier);
}

void Compiler::operator()(const VariableExprNode& node)
{
    _compile_named_variable(node.var);
}

void Compiler::_compile_named_variable(const Token& name)
{
    if(name.type == TokenType::THIS && !_current_class)
    {
        throw Exception{name, Error::ThisOutsideClass};
    }

    auto arg = _resolve_local(name);
    OpCode op;

    if(arg != -1)
    {
        op = OpCode::GET_LOCAL;
    }
    else if((arg = _resolve_upvalue(name)) != -1)
    {
        op = OpCode::GET_UPVALUE;
    }
    else
    {
        arg = _make_constant(Value{_allocator.allocate_string(name.lexeme, false)});
        op = OpCode::GET_GLOBAL;
    }

    _emit_bytes(static_cast<uint8_t>(op), arg, name.line);
}

void Compiler::operator()(const AssignmentExprNode& node)
{
    auto* var = std::get_if<VariableExprNode>(&node.target);

    if(!var)
    {
        auto property = std::get_if<PropertyExprNode>(&node.target);
        assert(property && "Invalid assignment target");

        // Compile the expression which produces the instance.
        std::visit(*this, *property->instance);

        // Compile the expression to be stored.
        std::visit(*this, *node.value);

        auto name = _make_constant(Value{_allocator.allocate_string(property->name.lexeme, false)});
        _emit_bytes(static_cast<uint8_t>(OpCode::SET_PROPERTY), name, property->name.line);

        return;
    }

    std::visit(*this, *node.value);

    auto arg = _resolve_local(var->var);
    OpCode op;

    if(arg != -1)
    {
        op = OpCode::SET_LOCAL;
    }
    else if((arg = _resolve_upvalue(var->var)) != -1)
    {
        op = OpCode::SET_UPVALUE;
    }
    else
    {
        arg = _make_constant(Value{_allocator.allocate_string(var->var.lexeme, false)});
        op = OpCode::SET_GLOBAL;
    }

    _emit_bytes(static_cast<uint8_t>(op), arg, var->var.line);
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

int Compiler::_resolve_upvalue(const Token& name)
{
    if(_enclosing == nullptr)
    {
        return -1;
    }

    auto local = _enclosing->_resolve_local(name);

    if(local != -1)
    {
        _enclosing->_locals[local].is_captured = true;
        return _add_upvalue(name, local, true);
    }

    auto upvalue = _enclosing->_resolve_upvalue(name);

    if(upvalue != -1)
    {
        return _add_upvalue(name, upvalue, false);
    }

    return -1;
}

int Compiler::_add_upvalue(const Token& tok, uint8_t index, bool is_local)
{
    auto upvalue_count = _function->upvalue_count;

    for(auto i = 0; i < upvalue_count; i++)
    {
        if(_upvalues[i].index == index && _upvalues[i].is_local == is_local)
        {
            return i;
        }
    }

    if(upvalue_count > MAX_UPVALUES)
    {
        throw Exception{tok, Error::UpvalueLimitExceeded};
    }

    _upvalues[upvalue_count] = {.index = index, .is_local = is_local};
    return _function->upvalue_count++;
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
    _current_chunk().write(byte, line);
}

uint8_t Compiler::_make_constant(const Value& value)
{
    auto index = _current_chunk().add_constant(value);

    if(index > UINT8_MAX)
    {
        throw Exception{Token{}, Error::ChunkConstantLimitExceeded};
    }

    return index;
}

} // namespace lox