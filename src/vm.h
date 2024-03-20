#ifndef LOX_VM_H
#define LOX_VM_H

#include <array>
#include <cstdint>
#include <iostream>
#include <print>

#include "chunk.h"
#include "object.h"

namespace lox
{
enum class InterpretResult
{
    OK,
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
    ObjectAllocator& _allocator;

    template <class... Args>
    constexpr void _runtime_error(std::string_view format, Args&&... args)
    {
        std::cerr << std::vformat(format, std::make_format_args(args...)) << '\n';

        size_t instruction = _ip - _chunk.get_code() - 1;
        int line = _chunk.get_line(instruction);
        std::println(stderr, "[line {}] in script", line);
    }

    InterpretResult _run();
    uint8_t _read_byte()
    {
        return *_ip++;
    };

public:
    VM(const Chunk&, ObjectAllocator&);
    InterpretResult interpret();
};
} // namespace lox

#endif // LOX_VM_H