#include "object.h"
#include "common.h"

#include <new>
#include <print>
#include <vector>

namespace lox
{

void ObjectAllocator::_deallocate(Object* object)
{
#ifdef DEBUG_LOG_GC
    std::println(
        "Object deallocated: {:p}, object: {}", static_cast<void*>(object), object->to_string());
#endif // DEBUG_LOG_GC
    _bytes_allocated -= object->size();
    ::operator delete(object);
}

void ObjectAllocator::collect_garbage()
{
#ifdef DEBUG_LOG_GC
    std::println("-- GC begin --");
    size_t before = _bytes_allocated;
#endif // DEBUG_LOG_GC

    _mark_roots();
    _trace_references();
    _remove_white_strings();
    _sweep();

    _next_collection = _bytes_allocated * _growth_factor;
#ifdef DEBUG_LOG_GC
    std::println("-- GC end --");
    std::println("   Collected {} bytes (from {} to {}). Next collection at {}.",
                 before - _bytes_allocated,
                 before,
                 _bytes_allocated,
                 _next_collection);
#endif // DEBUG_LOG_GC
}

void ObjectAllocator::_mark_roots()
{
    // Always mark the last allocated object. This is to prevent freeing
    // temporaries which are yet to be placed on the stack.
    _objects.back()->mark(_grey_list);

    for(auto i = 0; i < _stack.size(); ++i)
    {
        _stack[i].mark(_grey_list);
    }

    for(auto i = 0; i < _callstack.size(); ++i)
    {
        _callstack[i].closure->mark(_grey_list);
    }

    for(auto upvalue : _open_upvalues)
    {
        upvalue->mark(_grey_list);
    }

    for(auto& [key, value] : _globals)
    {
        value.mark(_grey_list);
    }
}

void ObjectAllocator::_trace_references()
{
    while(!_grey_list.empty())
    {
        auto obj = _grey_list.top();
        _grey_list.pop();
        obj->blacken(_grey_list);
    }
}

void ObjectAllocator::_sweep()
{
    std::erase_if(_objects, [this](auto* ptr) {
        auto unreachable = !ptr->unmark();

        if(unreachable)
        {
            _deallocate(ptr);
        }

        return unreachable;
    });
}

void ObjectAllocator::_remove_white_strings()
{
    for(auto it = _interned_strings.begin(), end = _interned_strings.end(); it != end;)
    {
        // erase() will invalidate the iterator, so advance it first.
        auto temp = it++;

        if(!temp->second->is_marked())
        {
            _interned_strings.erase(temp);
        }
    }
}

ObjectAllocator::~ObjectAllocator()
{
    for(auto* ptr : _objects)
    {
        _deallocate(ptr);
    }
}

void Object::mark(GreyList<Object*>& grey_list)
{
    if(_is_marked)
    {
        return;
    }

#ifdef DEBUG_LOG_GC
    std::println("Object marked: {:p}, object: {}", static_cast<void*>(this), to_string());
#endif // DEBUG_LOG_GC

    _is_marked = true;
    grey_list.push(this);
}

StringObject* ObjectAllocator::allocate_string(std::string_view value, bool collect)
{
    auto it = _interned_strings.find(value);

    if(it != _interned_strings.end())
    {
        return it->second;
    }

    auto* string = allocate<StringObject>(false, value);

    _interned_strings[string->value()] = string;

    return string;
}

Object::~Object() { }
StringObject::~StringObject(){};
FunctionObject::~FunctionObject() { }
} // namespace lox