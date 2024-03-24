#ifndef LOX_STACK_H
#define LOX_STACK_H

#include <cstddef>
#include <memory>

#include "common.h"

namespace lox
{

template <typename T, size_t MAX_SIZE>
class Stack
{
    std::unique_ptr<T[]> _data;
    size_t _top = 0;

public:
    Stack()
        : _data(new T[MAX_SIZE])
    { }

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

    size_t size() const
    {
        return _top;
    }

    T* data()
    {
        return _data.get();
    }

    T& pop()
    {
        --_top;
        return _data[_top];
    }

    void pop_by(size_t n)
    {
        _top -= n;
    }

    void pop_to(size_t n)
    {
        _top = n;
    }

    T* top_addr()
    {
        return &top();
    }

    const T& top() const
    {
        return _data[_top - 1];
    }

    const T& operator[](size_t i) const
    {
        return _data[i];
    }

    T& operator[](size_t i)
    {
        return _data[i];
    }

    T& top()
    {
        return _data[_top - 1];
    }
};

inline constexpr int MAX_FRAMES = 64;
inline constexpr int STACK_MAX = MAX_FRAMES * (UINT8_MAX + 1);

template <typename T>
using FixedStack = Stack<T, STACK_MAX>;

using CallStack = Stack<CallFrame, MAX_FRAMES>;

} // namespace lox

#endif // LOX_STACK_H