#include "vm.h"

#include <algorithm>
#include <cstddef>
#include <ctime>
#include <print>
#include <span>
#include <string_view>
#include <vector>

#include "chunk.h"
#include "common.h"
#include "object.h"
#include "stack.h"
#include "value.h"

namespace lox
{
namespace
{

Value clock_native(std::span<Value>)
{
    return Value{(double)clock() / CLOCKS_PER_SEC};
}

Value print_native(std::span<Value> args)
{
    for(size_t i = 0; i < args.size() - 1; ++i)
    {
        std::print("{}, ", args[i].to_string());
    }

    std::println("{}", args.back().to_string());

    return Value{};
}

} // namespace

VM::VM(ObjectAllocator& allocator,
       FixedStack<Value>& stack,
       HashMap<Value>& globals,
       CallStack& callstack,
       std::vector<UpValueObject*>& open_upvalues)
    : _allocator(allocator)
    , _stack(stack)
    , _globals(globals)
    , _callstack(callstack)
    , _open_upvalues(open_upvalues)
{
    define_native("clock", &clock_native);
    define_native("print", &print_native);

    _open_upvalues.reserve(256);
}

InterpretResult VM::interpret(FunctionObject& function)
{
    _stack.push(
        Value{_allocator.allocate<ClosureObject>(false, function, std::vector<UpValueObject*>{})});

    _call_value(_stack.top(), 0);

    return _run();
}

bool VM::_call_value(Value& callee, int arg_count)
{
    if(callee.is_object())
    {
        if(auto closure = callee.as_object()->as<ClosureObject>())
        {
            return _call(closure, arg_count);
        }
        else if(auto klass = callee.as_object()->as<ClassObject>())
        {
            _stack[_stack.size() - arg_count - 1] =
                Value{_allocator.allocate<InstanceObject>(true, *klass)};
            auto* initializer = klass->methods["init"];

            if(initializer)
            {
                return _call(initializer, arg_count);
            }
            else if(arg_count != 0)
            {
                _runtime_error("Expected 0 arguments but got {}.", arg_count);
                return false;
            }

            return true;
        }
        else if(auto bound_method = callee.as_object()->as<BoundMethodObject>())
        {
            _stack[_stack.size() - arg_count - 1] = bound_method->receiver;
            return _call(bound_method->method, arg_count);
        }
        else if(auto native_func = callee.as_object()->as<NativeFunctionObject>())
        {
            auto ret =
                native_func->native_fn({_stack.top_addr() - arg_count + 1, _stack.top_addr() + 1});

            _stack.pop_by(arg_count + 1);
            _stack.push(ret);

            return true;
        }
    }

    _runtime_error("Can only call functions and classes.");

    return false;
}

bool VM::_call(ClosureObject* closure, int arg_count)
{
    if(arg_count != closure->function.arity)
    {
        _runtime_error("Expected {} arguments but got {}.", closure->function.arity, arg_count);
        return false;
    }

    if(_callstack.size() == MAX_FRAMES)
    {
        _runtime_error("Stack overflow.");
        return false;
    }

    _callstack.push({
        .closure = closure,
        .ip = closure->function.chunk.get_code(),
        .offset = static_cast<int>(_stack.size() - arg_count - 1),
    });

    _current_frame = _callstack.top_addr();

    return true;
}

bool VM::_invoke(std::string_view name, int arg_count, ClassObject* klass)
{
    auto* receiver = _stack[_stack.size() - arg_count - 1].as_object()->as<InstanceObject>();
    klass = klass ? klass : &receiver->klass;

    if(!receiver)
    {
        _runtime_error("Only instances have methods.");
        return false;
    }

    auto method_it = klass->methods.find(name);

    if(method_it == klass->methods.end())
    {
        // This is a super call so only methods are allowed.
        if(klass != &receiver->klass)
        {
            _runtime_error("Undefined method '{}' for superclass {}.", name, klass->name);
            return false;
        }

        auto field_it = receiver->fields.find(name);
        if(field_it == receiver->fields.end())
        {
            _runtime_error("Undefined property '{}'.", name);
            return false;
        }

        _stack[_stack.size() - arg_count - 1] = field_it->second;

        return _call_value(field_it->second, arg_count);
    }

    return _call(method_it->second, arg_count);
}

Chunk& VM::_current_chunk()
{
    return _current_frame->closure->function.chunk;
}

void VM::define_native(std::string_view name, NativeFn fn)
{
    _globals[name] = Value{_allocator.allocate<NativeFunctionObject>(false, fn)};
}

UpValueObject* VM::_capture_upvalue(Value* local)
{
    auto ret = std::ranges::find_if(
        _open_upvalues, [local](UpValueObject* upvalue) { return upvalue->location == local; });

    if(ret != _open_upvalues.end())
    {
        return *ret;
    }
    _open_upvalues.push_back(_allocator.allocate<UpValueObject>(true, local));

    return _open_upvalues.back();
}

template <class... Args>
constexpr void VM::_runtime_error(std::string_view format, Args&&... args)
{
    std::cerr << std::vformat(format, std::make_format_args(args...)) << '\n';

    for(int i = _callstack.size() - 1; i >= 0; i--)
    {
        const auto& frame = _callstack[i];
        const auto& function = frame.closure->function;
        size_t instruction = frame.ip - function.chunk.get_code() - 1;

        int line = function.chunk.get_line(instruction);

        std::print(stderr, "[line {}] in ", line);

        if(function.name.empty())
        {
            std::println(stderr, "script");
        }
        else
        {
            std::println(stderr, "{}()", function.name);
        }
    }
}

void VM::_close_upvalues(Value* last)
{
    std::erase_if(_open_upvalues, [last](UpValueObject* upvalue) {
        if(upvalue->location < last)
        {
            return false;
        }

        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;

        return true;
    });
}

bool VM::_bind_method(const ClassObject& klass, std::string_view name)
{
    auto method_it = klass.methods.find(name);

    if(method_it == klass.methods.end())
    {
        return false;
    }

    auto* bound_method =
        _allocator.allocate<BoundMethodObject>(true, _stack.top(), method_it->second);

    _stack.pop();
    _stack.push(Value{bound_method});

    return true;
}

InterpretResult VM::_run()
{
#define BINARY_OP(op)                                                                              \
    do                                                                                             \
    {                                                                                              \
        auto b = _stack.pop();                                                                     \
        auto a = _stack.pop();                                                                     \
        if(!a.is_number() || !b.is_number())                                                       \
        {                                                                                          \
            _runtime_error("{}", "Operands must be numbers.");                                     \
            return InterpretResult::RUNTIME_ERROR;                                                 \
        }                                                                                          \
        _stack.push(Value{a.as_number() op b.as_number()});                                        \
    } while(false)

    while(true)
    {
#ifdef DEBUG_TRACE_EXECUTION
        _current_chunk().disassemble_instruction(_current_frame->ip);
#endif
        switch(auto instruction = static_cast<OpCode>(_read_byte()); instruction)
        {
        case OpCode::RETURN: {
            auto ret = _stack.pop();

            // Returning from top level script function
            if(_callstack.size() == 1)
            {
                _callstack.pop();
                _stack.pop();
                return InterpretResult::OK;
            }

            auto& previous_frame = _callstack.pop();

            _current_frame = _callstack.top_addr();

            // Reset the stack to where it was before the function call
            _stack.pop_to(previous_frame.offset);
            // Close over any stack values from the returning function
            _close_upvalues(_stack.top_addr());

            // Push the return value
            _stack.push(ret);

            break;
        }
        case OpCode::CONSTANT: {
            auto& value = _current_chunk().get_constant(_read_byte());
            _stack.push(value);
            break;
        }
        case OpCode::NEGATE:
            if(!_stack.top().is_number())
            {
                _runtime_error("{}", "Operand must be a number.");
                return InterpretResult::RUNTIME_ERROR;
            }
            _stack.top().negate();
            break;
        case OpCode::ADD: {
            auto& b = _stack.top();
            auto& a = _stack[_stack.size() - 2];

            if(a.is_number() && b.is_number())
            {
                _stack.pop_by(2);
                _stack.push(Value{a.as_number() + b.as_number()});
                break;
            }
            else if(a.is_object() && b.is_object())
            {
                const auto* a_str = a.as_object()->as<StringObject>();
                const auto* b_str = b.as_object()->as<StringObject>();

                if(a_str && b_str)
                {
                    _stack.pop_by(2);
                    _stack.push(Value{_allocator.allocate_string(a_str->value() + b_str->value())});
                    break;
                }
            }
            _runtime_error("{}", "Operands to + must both be numbers or strings.");
            return InterpretResult::RUNTIME_ERROR;
        }
        case OpCode::SUBTRACT:
            BINARY_OP(-);
            break;
        case OpCode::MULTIPLY:
            BINARY_OP(*);
            break;
        case OpCode::DIVIDE:
            BINARY_OP(/);
            break;
        case OpCode::TRUE:
            _stack.push(true);
            break;
        case OpCode::FALSE:
            _stack.push(false);
            break;
        case OpCode::NIL:
            _stack.push(Value{});
            break;
        case OpCode::NOT:
            _stack.top().not_op();
            break;
        case OpCode::EQUAL: {
            auto a = _stack.pop();
            auto b = _stack.pop();
            _stack.push(a == b);
            break;
        }
        case OpCode::GREATER:
            BINARY_OP(>);
            break;
        case OpCode::LESS:
            BINARY_OP(<);
            break;
        case OpCode::POP:
            _stack.pop();
            break;
        case OpCode::DEFINE_GLOBAL: {
            const auto* global_name =
                _current_chunk().get_constant(_read_byte()).as_object()->as<StringObject>();

            _globals[global_name->value()] = _stack.top();
            _stack.pop();

            break;
        }
        case OpCode::GET_GLOBAL: {
            const auto* global_name =
                _current_chunk().get_constant(_read_byte()).as_object()->as<StringObject>();

            auto it = _globals.find(global_name->value());

            if(it == _globals.end())
            {
                _runtime_error("Undefined variable '{}'.", global_name->value());
                return InterpretResult::RUNTIME_ERROR;
            }

            _stack.push(it->second);
            break;
        }
        case OpCode::SET_GLOBAL: {
            const auto* global_name =
                _current_chunk().get_constant(_read_byte()).as_object()->as<StringObject>();

            auto it = _globals.find(global_name->value());

            if(it == _globals.end())
            {
                _runtime_error("Undefined variable '{}'.", global_name->value());
                return InterpretResult::RUNTIME_ERROR;
            }

            it->second = _stack.top();
            break;
        }
        case OpCode::GET_LOCAL: {
            auto slot = _read_byte();
            _stack.push(_stack[slot + _current_frame->offset]);
            break;
        }
        case OpCode::SET_LOCAL: {
            auto slot = _read_byte();
            _stack[slot + _current_frame->offset] = _stack.top();
            break;
        }
        case OpCode::JUMP_IF_FALSE: {
            auto jmp = _read_short();
            if(_stack.top().is_falsey())
                _current_frame->ip += jmp;
            break;
        }
        case OpCode::JUMP_IF_TRUE: {
            auto jmp = _read_short();
            if(!_stack.top().is_falsey())
                _current_frame->ip += jmp;
            break;
        }
        case OpCode::JUMP: {
            _current_frame->ip += _read_short();
            break;
        }
        case OpCode::LOOP: {
            _current_frame->ip -= _read_short();
            break;
        }
        case OpCode::CALL: {
            auto arg_count = _read_byte();
            if(!_call_value(_stack[_stack.size() - arg_count - 1], arg_count))
            {
                return InterpretResult::RUNTIME_ERROR;
            }
            break;
        }
        case OpCode::CLOSURE: {
            auto* function =
                _current_chunk().get_constant(_read_byte()).as_object()->as<FunctionObject>();

            std::vector<UpValueObject*> upvalues;
            upvalues.reserve(function->upvalue_count);

            for(int i = 0; i < function->upvalue_count; ++i)
            {
                auto is_local = static_cast<bool>(_read_byte());
                auto index = _read_byte();

                if(is_local)
                {
                    upvalues.push_back(_capture_upvalue(&_stack[index + _current_frame->offset]));
                }
                else
                {
                    upvalues.push_back(_current_frame->closure->upvalues[i]);
                }
            }

            _stack.push(_allocator.allocate<ClosureObject>(true, *function, std::move(upvalues)));
            break;
        }
        case OpCode::GET_UPVALUE: {
            auto slot = _read_byte();
            _stack.push(*_current_frame->closure->upvalues[slot]->location);
            break;
        }
        case OpCode::SET_UPVALUE: {
            auto slot = _read_byte();
            *_current_frame->closure->upvalues[slot]->location = _stack.top();
            break;
        }
        case OpCode::CLOSE_UPVALUE: {
            _close_upvalues(_stack.top_addr());
            _stack.pop();
            break;
        }
        case OpCode::CLASS: {
            auto& value = _current_chunk().get_constant(_read_byte());
            _stack.push(Value{_allocator.allocate<ClassObject>(
                true, value.as_object()->as<StringObject>()->value())});
            break;
        }
        case OpCode::GET_PROPERTY: {
            auto* instance = _stack.top().as_object()->as<InstanceObject>();
            auto* name =
                _current_chunk().get_constant(_read_byte()).as_object()->as<StringObject>();

            if(!instance)
            {
                _runtime_error("Only instances have properties.");
                return InterpretResult::RUNTIME_ERROR;
            }

            auto field_it = instance->fields.find(name->value());

            if(field_it != instance->fields.end())
            {
                _stack.pop();
                _stack.push(field_it->second);
                break;
            }

            if(!_bind_method(instance->klass, name->value()))
            {
                _runtime_error("Undefined property '{}'.", name->value());
                return InterpretResult::RUNTIME_ERROR;
            }

            break;
        }
        case OpCode::SET_PROPERTY: {
            auto* instance = _stack[_stack.size() - 2].as_object()->as<InstanceObject>();

            if(!instance)
            {
                _runtime_error("Only instances have fields.");
                return InterpretResult::RUNTIME_ERROR;
            }

            auto name = _current_chunk().get_constant(_read_byte()).as_object()->as<StringObject>();

            instance->fields[name->value()] = _stack.top();

            auto& val = _stack.pop();
            // Replace the instance on the top of the stack with the assigned value.
            _stack.top() = val;

            break;
        }
        case OpCode::METHOD: {
            auto name = _current_chunk().get_constant(_read_byte()).as_object()->as<StringObject>();

            auto& method = _stack.top();
            auto* klass = _stack[_stack.size() - 2].as_object()->as<ClassObject>();

            klass->methods[name->value()] = method.as_object()->as<ClosureObject>();

            _stack.pop();

            break;
        }
        case OpCode::INVOKE: {
            auto method =
                _current_chunk().get_constant(_read_byte()).as_object()->as<StringObject>();
            auto arg_count = _read_byte();

            if(!_invoke(method->value(), arg_count))
            {
                return InterpretResult::RUNTIME_ERROR;
            }
            break;
        }
        case OpCode::INHERIT: {
            auto* subclass = _stack.top().as_object()->as<ClassObject>();
            auto* superclass = _stack[_stack.size() - 2].as_object()->as<ClassObject>();

            if(!superclass)
            {
                _runtime_error("Superclass must be a class");
                return InterpretResult::RUNTIME_ERROR;
            }

            subclass->methods = superclass->methods;

            // Pop the subclass and superclass.
            _stack.pop();

            break;
        }
        case OpCode::GET_SUPER: {
            auto* superclass = _stack[_stack.size() - 2].as_object()->as<ClassObject>();

            auto* method =
                _current_chunk().get_constant(_read_byte()).as_object()->as<StringObject>();

            if(!_bind_method(*superclass, method->value()))
            {
                return InterpretResult::RUNTIME_ERROR;
            }

            break;
        }
        case OpCode::SUPER_INVOKE: {
            auto* method =
                _current_chunk().get_constant(_read_byte()).as_object()->as<StringObject>();

            auto arg_count = _read_byte();

            auto* superclass = _stack.pop().as_object()->as<ClassObject>();

            if(!_invoke(method->value(), arg_count, superclass))
            {
                return InterpretResult::RUNTIME_ERROR;
            }

            break;
        }
        }
#undef BINARY_OP
    }
}

} // namespace lox