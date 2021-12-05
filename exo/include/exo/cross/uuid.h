#pragma once
#include "exo/maths/numerics.h"
#include "exo/collections/map.h"
#include "exo/cross/prelude.h"
#include "exo/serializer.h"

#include <fmt/format.h>

#include <string_view>

namespace cross
{
struct UUID
{
    static constexpr usize STR_LEN      = 35;
    u32                    data[4]      = {};
    char                   str[STR_LEN] = {};

    static UUID create();
    static UUID from_string(const char *s, usize len);
    static UUID from_values(const u32 *values);

    bool operator==(const UUID &other) const = default;

    // clang-format off
    bool is_valid() const { return data[0] != 0 || data[1] != 0 || data[2] != 0 || data[3] != 0; }
    std::string_view as_string() const { return std::string_view{this->str, STR_LEN}; }
    // clang-format on

    friend size_t hash_value(const UUID &uuid);
};
} // namespace cross

static_assert(phmap::has_hash_value<cross::UUID>::value);

template <>
struct fmt::formatter<cross::UUID>
{
    constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin())
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const cross::UUID &uuid, FormatContext &ctx) -> decltype(ctx.out())
    {
        return format_to(ctx.out(), "{:.{}}", uuid.str, cross::UUID::STR_LEN);
    }
};

template<>
void Serializer::serialize<cross::UUID>(cross::UUID &data);
