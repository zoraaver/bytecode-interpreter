#include "vm.h"

#include <algorithm>
#include <cstddef>
#include <ctime>
#include <print>
#include <span>
#include <string_view>
#include <vector>

#include "chunk.h"
#include "object.h"
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

VM::VM(ObjectAllocator& allocator)
    : _allocator(allocator)
{
    define_native("clock", &clock_native);
    define_native("print", &print_native);

    _open_upvalues.reserve(256);
}

InterpretResult VM::interpret(FunctionObject& function)
{
    _stack.push(Value{&function});
    _stack.pop();

    _stack.push(Value{new(_allocator) ClosureObject{function, {}}});

    _call_value(_stack.top(), 0);

    return _run();
}

bool VM::_call_value(const Value& callee, int arg_count)
{
    if(callee.is_object())
    {
        if(auto closure = callee.as_object()->as<ClosureObject>())
        {
            return _call(closure, arg_count);
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

bool VM::_call(const ClosureObject* closure, int arg_count)
{
    if(arg_count != closure->function.arity)
    {
        _runtime_error("Expected {} arguments but got {}.", closure->function.arity, arg_count);
        return false;
    }

    if(_frame_count == MAX_FRAMES)
    {
        _runtime_error("Stack overflow.");
        return false;
    }

    _current_frame = &_frames[_frame_count++];

    _current_frame->closure = closure;
    _current_frame->ip = closure->function.chunk.get_code();
    _current_frame->offset = static_cast<int>(_stack.size() - arg_count - 1);

    return true;
}

void VM::define_native(std::string_view name, NativeFn fn)
{
    _stack.push(Value{_allocator.allocate_string(name)});
    _stack.push(Value{new(_allocator) NativeFunctionObject{fn}});

    _globals[name] = _stack.top();

    _stack.pop();
    _stack.pop();
}

UpValueObject* VM::_capture_upvalue(Value* local)
{
    auto ret = std::ranges::find_if(
        _open_upvalues, [local](UpValueObject* upvalue) { return upvalue->location == local; });

    if(ret != _open_upvalues.end())
    {
        return *ret;
    }
    _open_upvalues.push_back(new(_allocator) UpValueObject{local});

    return _open_upvalues.back();
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
            if(_frame_count == 1)
            {
                --_frame_count;
                _stack.pop();
                return InterpretResult::OK;
            }

            auto previous_frame = &_frames[_frame_count - 1];

            --_frame_count;

            _current_frame = &_frames[_frame_count - 1];

            // Reset the stack to where it was before the function call
            _stack.pop_to(previous_frame->offset);
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
            auto b = _stack.pop();
            auto a = _stack.pop();

            if(a.is_number() && b.is_number())
            {
                _stack.push(Value{a.as_number() + b.as_number()});
                break;
            }
            else if(a.is_object() && b.is_object())
            {
                const auto* a_str = a.as_object()->as<StringObject>();
                const auto* b_str = b.as_object()->as<StringObject>();

                if(a_str && b_str)
                {
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
            const auto* function =
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

            _stack.push(new(_allocator) ClosureObject{*function, std::move(upvalues)});
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
        }
    }
#undef BINARY_OP
}

} // namespace lox