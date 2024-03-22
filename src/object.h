#ifndef LOX_OBJECT_H
#define LOX_OBJECT_H

#include "chunk.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <absl/container/flat_hash_map.h>

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

    virtual ~StringObject();
};

struct FunctionObject : public Object
{
    FunctionObject(std::string name, int arity)
        : name(std::move(name))
        , arity(arity){};

    const uint8_t arity;
    const std::string name;
    Chunk chunk;

    virtual ~FunctionObject();
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