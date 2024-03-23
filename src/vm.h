#ifndef LOX_VM_H
#define LOX_VM_H

#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <print>

#include "absl/container/flat_hash_map.h"

#include "chunk.h"
#include "object.h"
#include "value.h"

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

    T pop()
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

class VM
{
    struct CallFrame
    {
        const ClosureObject* closure;
        const uint8_t* ip;
        int offset;
    };

    absl::flat_hash_map<std::string_view, Value> _globals;
    std::vector<UpValueObject*> _open_upvalues;

    UpValueObject* _capture_upvalue(Value*);
    void _close_upvalues(Value*);

    static constexpr int MAX_FRAMES = 64;
    std::array<CallFrame, MAX_FRAMES> _frames;
    int _frame_count = 0;
    CallFrame* _current_frame = nullptr;

    static constexpr int STACK_MAX = MAX_FRAMES * (UINT8_MAX + 1);

    Stack<Value, STACK_MAX> _stack;
    ObjectAllocator& _allocator;

    template <class... Args>
    constexpr void _runtime_error(std::string_view format, Args&&... args)
    {
        std::cerr << std::vformat(format, std::make_format_args(args...)) << '\n';

        for(int i = _frame_count - 1; i >= 0; i--)
        {
            auto* frame = &_frames[i];
            const auto& function = frame->closure->function;
            size_t instruction = frame->ip - function.chunk.get_code() - 1;

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

    bool _call_value(const Value& callee, int arg_count);

    bool _call(const ClosureObject* callee, int arg_count);

    InterpretResult _run();

    uint8_t _read_byte()
    {
        return *_current_frame->ip++;
    };

    uint16_t _read_short()
    {
        auto ip = (_current_frame->ip += 2);
        return (ip[-2] << 8) | ip[-1];
    }

    const Chunk& _current_chunk()
    {
        return _current_frame->closure->function.chunk;
    }

public:
    VM(ObjectAllocator&);
    InterpretResult interpret(FunctionObject&);
    void define_native(std::string_view name, NativeFn function);
};
} // namespace lox

#endif // LOX_VM_H