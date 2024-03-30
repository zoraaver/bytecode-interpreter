#ifndef LOX_VM_H
#define LOX_VM_H

#include <cstdint>
#include <print>
#include <string_view>

#include "chunk.h"
#include "common.h"
#include "object.h"
#include "stack.h"
#include "value.h"

namespace lox
{
enum class InterpretResult
{
    OK,
    RUNTIME_ERROR
};

class ObjectAllocator;
class FunctionObject;
struct UpValueObject;
struct ClosureObject;

class VM
{
public:
    void define_native(std::string_view name, NativeFn function);
    InterpretResult interpret(FunctionObject&);

    VM(ObjectAllocator&,
       FixedStack<Value>& stack,
       HashMap<Value>& globals,
       CallStack& callstack,
       std::vector<UpValueObject*>& open_upvalues);

private:
    HashMap<Value>& _globals;
    std::vector<UpValueObject*>& _open_upvalues;

    UpValueObject* _capture_upvalue(Value*);
    void _close_upvalues(Value*);

    CallStack& _callstack;
    CallFrame* _current_frame = nullptr;

    FixedStack<Value>& _stack;
    ObjectAllocator& _allocator;

    template <class... Args>
    constexpr void _runtime_error(std::string_view format, Args&&... args);

    bool _call_value(Value& callee, int arg_count);
    bool _call(ClosureObject* callee, int arg_count);
    bool _bind_method(const ClassObject& klass, std::string_view name);
    bool _invoke(std::string_view name, int arg_count, ClassObject* = nullptr);

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

    inline Chunk& _current_chunk();
};
} // namespace lox

#endif // LOX_VM_H