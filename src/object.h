#ifndef LOX_OBJECT_H
#define LOX_OBJECT_H

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

    virtual bool operator==(const Object& rhs) const = 0;
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

    bool operator==(const Object& rhs) const override;

    virtual ~StringObject();
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