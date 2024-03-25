#ifndef LOX_OBJECT_H
#define LOX_OBJECT_H

#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <vector>

#include "chunk.h"
#include "common.h"
#include "stack.h"
#include "value.h"

namespace lox
{

class ObjectAllocator;

#define ADD_SIZE_METHOD(type)                                                                      \
    constexpr size_t size() const override                                                         \
    {                                                                                              \
        return sizeof(type);                                                                       \
    }

class Object
{
    bool _is_marked = false;

public:
    Object(const Object&) = delete;
    Object(Object&&) = delete;

    Object& operator=(const Object&) = delete;
    Object& operator=(Object&&) = delete;

    Object() = default;

    template <typename T>
    T* as()
    {
        return dynamic_cast<T*>(this);
    }

    void mark(GreyList<Object*>&);
    virtual constexpr size_t size() const = 0;

    // Returns true if the object was previously marked.
    bool unmark()
    {
        auto ret = _is_marked;
        _is_marked = false;
        return ret;
    }

    bool is_marked() const
    {
        return _is_marked;
    }

    virtual void blacken(GreyList<Object*>&) { }
    virtual std::string to_string() const = 0;
    virtual ~Object();
};

struct ClassObject : public Object
{
    ClassObject(std::string name)
        : name(std::move(name))
    { }

    ADD_SIZE_METHOD(ClassObject)

    const std::string name;

    std::string to_string() const override
    {
        return std::format("<class {}>", name);
    }
};

struct InstanceObject : public Object
{
    InstanceObject(ClassObject& klass)
        : klass(klass)
    { }

    ADD_SIZE_METHOD(InstanceObject)

    ClassObject& klass;
    HashMap<Value> fields;

    void blacken(GreyList<Object*>& grey_list) override
    {
        Object::blacken(grey_list);

        klass.mark(grey_list);

        for(auto& [key, value] : fields)
        {
            value.mark(grey_list);
        }
    }

    std::string to_string() const override
    {
        return std::format("<instance {}>", klass.name);
    }
};

class StringObject : public Object
{
    std::string _value;

public:
    StringObject(std::string value)
        : _value(std::move(value)){};

    StringObject(std::string_view value)
        : _value(value){};

    ADD_SIZE_METHOD(StringObject)

    const std::string& value() const
    {
        return _value;
    }

    std::string to_string() const override
    {
        return std::format("'{}'", _value);
    }

    virtual ~StringObject();
};

struct FunctionObject : public Object
{
    FunctionObject(std::string name, int arity)
        : name(std::move(name))
        , arity(arity)
    { }

    ADD_SIZE_METHOD(FunctionObject)

    const uint8_t arity;
    const std::string name;
    int upvalue_count = 0;
    Chunk chunk;

    std::string to_string() const override
    {
        if(name.empty())
        {
            return "<script>";
        }

        return "<fn " + name + ">";
    }

    void blacken(GreyList<Object*>& grey_list) override
    {
        Object::blacken(grey_list);

        for(auto constant : chunk.get_constants())
        {
            constant.mark(grey_list);
        }
    }

    virtual ~FunctionObject();
};

struct UpValueObject : public Object
{
    UpValueObject(Value* location)
        : location(location)
        , closed()
    { }

    ADD_SIZE_METHOD(UpValueObject)

    Value* location = nullptr;
    Value closed;

    std::string to_string() const override
    {
        return "<upvalue>";
    }

    void blacken(GreyList<Object*>& grey_list) override
    {
        Object::blacken(grey_list);

        closed.mark(grey_list);
    }

    virtual ~UpValueObject(){};
};

struct ClosureObject : public Object
{
    ClosureObject(FunctionObject& function, std::vector<UpValueObject*> upvalues)
        : function(function)
        , upvalues(std::move(upvalues))
    { }

    ADD_SIZE_METHOD(ClosureObject)

    FunctionObject& function;
    const std::vector<UpValueObject*> upvalues;

    std::string to_string() const override
    {
        return std::format("<closure {}>", function.name.empty() ? "script" : function.name);
    }

    void blacken(GreyList<Object*>& grey_list) override
    {
        function.mark(grey_list);
        for(auto upvalue : upvalues)
        {
            upvalue->mark(grey_list);
        }
    }

    virtual ~ClosureObject(){};
};

struct NativeFunctionObject : public Object
{

    NativeFunctionObject(NativeFn native_fn)
        : native_fn(native_fn)
    { }

    ADD_SIZE_METHOD(NativeFunctionObject)

    NativeFn native_fn;

    std::string to_string() const override
    {
        return "<native fn>";
    }

    virtual ~NativeFunctionObject(){};
};

class ObjectAllocator
{
    size_t _bytes_allocated = 0;
    size_t _next_collection = 1024 * 1024;
    static constexpr size_t _growth_factor = 2;

    std::vector<Object*> _objects;
    HashMap<StringObject*> _interned_strings;
    FixedStack<Value>& _stack;
    HashMap<Value>& _globals;
    CallStack& _callstack;
    std::vector<UpValueObject*>& _open_upvalues;
    std::stack<Object*, std::vector<Object*>> _grey_list;

    void _deallocate(Object* object);
    void _mark_roots();
    void _trace_references();
    void _sweep();
    void _remove_white_strings();

public:
    ObjectAllocator(FixedStack<Value>& stack,
                    HashMap<Value>& globals,
                    CallStack& callstack,
                    std::vector<UpValueObject*>& open_upvalues)
        : _stack(stack)
        , _globals(globals)
        , _callstack(callstack)
        , _open_upvalues(open_upvalues)
    { }

    void collect_garbage();

    template <typename T, typename... Args>
    T* allocate(bool collect, Args&&... args)
    {
        auto* ptr = ::new T{std::forward<Args>(args)...};
        _bytes_allocated += sizeof(T);

#ifdef DEBUG_LOG_GC
        std::println("Object allocated: {} bytes", sizeof(T));
#endif // DEBUG_LOG_GC

        _objects.push_back(ptr);

#ifdef DEBUG_STRESS_GC
        if(collect)
        {
            collect_garbage();
        }
#else
        if(_bytes_allocated > _next_collection && collect)
        {
            collect_garbage();
        }
#endif // DEBUG_STRESS_GC

        return ptr;
    }

    StringObject* allocate_string(std::string_view value, bool collect = true);

    ~ObjectAllocator();
};

} // namespace lox

#endif // LOX_OBJECT_H