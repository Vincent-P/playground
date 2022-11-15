#include "exo/string_view.h"

namespace exo
{
struct ScopeStack;
StringView                   formatf(ScopeStack &scope, const char *fmt...);
inline constexpr const char *bool_fmt(const bool value) { return value ? "true" : "false"; }
}; // namespace exo
