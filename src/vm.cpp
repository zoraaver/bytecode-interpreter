#include "vm.h"

#include <print>
#include <string_view>

#include "chunk.h"
#include "object.h"
#include "value.h"

namespace lox
{

VM::VM(ObjectAllocator& allocator)
    : _allocator(allocator)
{ }

InterpretResult VM::interpret(FunctionObject& function)
{
    _stack.push(Value{&function});

    _call(&function, 0);

    return _run();
}

bool VM::_call_value(const Value& callee, int arg_count)
{
    if(callee.is_object())
    {
        if(auto func = callee.as_object()->as<FunctionObject>())
        {
            return _call(func, arg_count);
        }
    }

    _runtime_error("Can only call functions and classes.");

    return false;
}

bool VM::_call(const FunctionObject* callee, int arg_count)
{
    if(arg_count != callee->arity)
    {
        _runtime_error("Expected {} arguments but got {}.", callee->arity, arg_count);
        return false;
    }

    if(_frame_count == MAX_FRAMES)
    {
        _runtime_error("Stack overflow.");
        return false;
    }

    _current_frame = &_frames[_frame_count++];

    _current_frame->function = callee;
    _current_frame->ip = callee->chunk.get_code();
    _current_frame->offset = static_cast<int>(_stack.size() - arg_count - 1);

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
            if(_frame_count == 1)
            {
                --_frame_count;
                _stack.pop();
                return InterpretResult::OK;
            }

            auto previous_frame = &_frames[_frame_count - 1];

            --_frame_count;

            _current_frame = &_frames[_frame_count - 1];

            // Pop off all the call frame parameters
            _stack.pop(previous_frame->function->arity + 1);

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
        case OpCode::PRINT:
            print_value(_stack.pop());
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
        }
    }
#undef BINARY_OP
}

} // namespace lox