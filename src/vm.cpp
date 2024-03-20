#include "vm.h"

#include <print>
#include <string_view>

#include "chunk.h"
#include "object.h"
#include "value.h"

namespace lox
{

VM::VM(const Chunk& chunk, ObjectAllocator& allocator)
    : _chunk(chunk)
    , _ip(chunk.get_code())
    , _allocator(allocator)
{ }

InterpretResult VM::interpret()
{
    return _run();
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
        _chunk.disassemble_instruction(_ip);
#endif
        switch(auto instruction = static_cast<OpCode>(_read_byte()); instruction)
        {
        case OpCode::RETURN:
            return InterpretResult::OK;
        case OpCode::CONSTANT: {
            auto& value = _chunk.get_constant(_read_byte());
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
            else if(auto a_str = a.as_object<StringObject>())
            {
                auto b_str = b.as_object<StringObject>();
                if(b_str)
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
            const auto* global_name = _chunk.get_constant(_read_byte()).as_object<StringObject>();

            _globals.insert({global_name->value(), _stack.top()});
            _stack.pop();

            break;
        }
        case OpCode::GET_GLOBAL: {
            const auto* global_name = _chunk.get_constant(_read_byte()).as_object<StringObject>();

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
            const auto* global_name = _chunk.get_constant(_read_byte()).as_object<StringObject>();

            auto it = _globals.find(global_name->value());

            if(it == _globals.end())
            {
                _runtime_error("Undefined variable '{}'.", global_name->value());
                return InterpretResult::RUNTIME_ERROR;
            }

            it->second = _stack.top();
            break;
        }
        }
    }
#undef BINARY_OP
}

} // namespace lox