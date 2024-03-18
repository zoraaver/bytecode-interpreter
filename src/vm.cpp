#include "vm.h"

#include <print>

#include "chunk.h"
#include "value.h"

namespace lox
{

VM::VM(const Chunk& chunk)
    : _chunk(chunk)
    , _ip(chunk.get_code())
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
        _stack.push(a op b);                                                                       \
    } while(false)

    while(true)
    {
#ifdef DEBUG_TRACE_EXECUTION
        _chunk.disassemble_instruction(_ip);
#endif
        switch(auto instruction = static_cast<OpCode>(_read_byte()); instruction)
        {
        case OpCode::RETURN:
            print_value(_stack.pop());
            std::println("");
            return InterpretResult::OK;
        case OpCode::CONSTANT: {
            auto& value = _chunk.get_constant(_read_byte());
            _stack.push(value);
            break;
        }
        case OpCode::NEGATE:
            _stack.push(-_stack.pop());
            break;
        case OpCode::ADD:
            BINARY_OP(+);
            break;
        case OpCode::SUBTRACT:
            BINARY_OP(-);
            break;
        case OpCode::MULTIPLY:
            BINARY_OP(*);
            break;
        case OpCode::DIVIDE:
            BINARY_OP(/);
            break;
        }
    }
#undef BINARY_OP
}

} // namespace lox