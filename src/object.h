#ifndef LOX_OBJECT_H
#define LOX_OBJECT_H

#include <iostream>
#include <new>
#include <string>
#include <string_view>

namespace lox
{

class Object
{
public:
    template <typename T>
    const T* as() const
    {
        return dynamic_cast<const T*>(this);
    }

    void* operator new(size_t size);

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

} // namespace lox

#endif // LOX_OBJECT_H