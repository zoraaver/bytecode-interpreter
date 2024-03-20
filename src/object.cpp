#include "object.h"

namespace lox
{

void* Object::operator new(size_t size)
{
    // TODO: free objects somewhere i.e. add garbage collection support
    std::cout << "Object allocated: " << size << std::endl;

    return ::operator new(size);
}

Object::~Object() { }

bool StringObject::operator==(const Object& rhs) const
{
    if(auto string_rhs = rhs.as<StringObject>())
    {
        return _value == string_rhs->value();
    }

    return false;
}

StringObject::~StringObject(){};
} // namespace lox