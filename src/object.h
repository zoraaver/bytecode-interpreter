#ifndef LOX_OBJECT_H
#define LOX_OBJECT_H

#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <absl/container/flat_hash_map.h>

#include "chunk.h"

namespace lox
{

class ObjectAllocator;
class Object
{
public:
    template <typename T>
    const T* as() const
    {
        return dynamic_cast<const T*>(this);
    }

    void* operator new(size_t size, ObjectAllocator&);
    void* operator new(size_t size) = delete;

    virtual std::string to_string() const = 0;

    virtual ~Object();
};

class StringObject : public Object
{
    std::string _value;

public:
    StringObject(std::string value)
        : _value(std::move(value)){};

    StringObject(std::string_view value)
        : _value(value){};

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

    virtual ~FunctionObject();
};

struct UpValueObject : public Object
{
    UpValueObject(Value* location)
        : location(location)
        , closed()
    { }

    Value* location = nullptr;
    UpValueObject* next = nullptr;
    Value closed;

    std::string to_string() const override
    {
        return "<upvalue>";
    }

    virtual ~UpValueObject(){};
};

struct ClosureObject : public Object
{
    ClosureObject(const FunctionObject& function, std::vector<UpValueObject*> upvalues)
        : function(function)
        , upvalues(std::move(upvalues))
    { }

    const FunctionObject& function;
    const std::vector<UpValueObject*> upvalues;

    std::string to_string() const override
    {
        return function.to_string();
    }

    virtual ~ClosureObject(){};
};

using NativeFn = Value (*)(std::span<Value>);

struct NativeFunctionObject : public Object
{

    NativeFunctionObject(NativeFn native_fn)
        : native_fn(native_fn)
    { }

    NativeFn native_fn;

    std::string to_string() const override
    {
        return "<native fn>";
    }

    virtual ~NativeFunctionObject(){};
};

class ObjectAllocator
{
    std::vector<Object*> _objects;
    absl::flat_hash_map<std::string_view, StringObject*> _interned_strings;

public:
    Object* allocate(size_t size);
    StringObject* allocate_string(std::string_view value);
    ~ObjectAllocator();
};

} // namespace lox

#endif // LOX_OBJECT_H