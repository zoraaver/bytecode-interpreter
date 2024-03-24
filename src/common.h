#ifndef LOX_COMMON_H
#define LOX_COMMON_H

#include "absl/container/flat_hash_map.h"
#include <stack>

namespace lox
{

template <typename T>
using HashMap = absl::flat_hash_map<std::string_view, T>;

template <typename T>
using GreyList = std::stack<T, std::vector<T>>;

class ClosureObject;
struct CallFrame
{
    ClosureObject* closure;
    const uint8_t* ip;
    int offset;
};

} // namespace lox

#endif // LOX_COMMON_H