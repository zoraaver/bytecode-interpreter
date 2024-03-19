#ifndef LOX_VM_H
#define LOX_VM_H

#include <array>
#include <cstdint>

#include "chunk.h"

namespace lox
{
enum class InterpretResult
{
    OK,
    COMPILE_ERROR,
    RUNTIME_ERROR
};

template <typename T, size_t MAX_SIZE>
class Stack
{
    std::array<T, MAX_SIZE> _data;
    size_t _top = 0;

public:
    void push(T&& val)
    {
        _data[_top] = std::move(val);
        ++_top;
    }

    void push(const T& val)
    {
        _data[_top] = val;
        ++_top;
    }

    T pop()
    {
        --_top;
        return _data[_top];
    }

    const T& top() const
    {
        return _data[_top - 1];
    }

    T& top()
    {
        return _data[_top - 1];
    }
};

class VM
{
    const Chunk& _chunk;
    const uint8_t* _ip = nullptr;
    Stack<Value, 256> _stack;

    InterpretResult _run();
    uint8_t _read_byte()
    {
        return *_ip++;
    };

public:
    VM(const Chunk&);
    InterpretResult interpret(std::string_view source);
};
} // namespace lox

#endif // LOX_VM_H