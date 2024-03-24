#include "value.h"
#include "common.h"
#include "object.h"

#include <print>

namespace lox
{

std::string Value::to_string() const
{
    switch(type)
    {
    case ValueType::BOOL:
        return as.boolean ? "true" : "false";
    case ValueType::NUMBER:
        return std::to_string(as.number);
    case ValueType::NIL:
        return "nil";
    case ValueType::OBJECT:
        return as.obj->to_string();
    }
}

void Value::mark(GreyList<Object*>& grey_list)
{
    if(type == ValueType::OBJECT)
    {
        as.obj->mark(grey_list);
    }
}

} // namespace lox