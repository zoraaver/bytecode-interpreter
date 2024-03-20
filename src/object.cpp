#include "object.h"

#include <print>

namespace lox
{

Object* ObjectAllocator::allocate(size_t size)
{
    auto* ptr = static_cast<Object*>(::operator new(size));

    std::println("Object allocated: {} bytes", size);

    _objects.push_back(ptr);

    return ptr;
}

StringObject* ObjectAllocator::allocate_string(std::string_view value)
{
    auto it = _interned_strings.find(value);

    if(it != _interned_strings.end())
    {
        return it->second;
    }

    auto* string = new(*this) StringObject(value);

    _interned_strings.emplace(string->value(), string);

    return string;
}

ObjectAllocator::~ObjectAllocator()
{
    for(auto* ptr : _objects)
    {
        ::operator delete(ptr);
    }
}

void* Object::operator new(size_t size, ObjectAllocator& allocator)
{
    return allocator.allocate(size);
}

Object::~Object() { }

bool StringObject::operator==(const Object& rhs) const
{
    // We intern all strings so comparing addresses is sufficient to show two
    // strings are equal.
    return this == &rhs;
}

StringObject::~StringObject(){};
} // namespace lox