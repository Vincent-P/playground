#include "exo/os/uuid.h"

#include "exo/collections/map.h"

#if defined(CROSS_WINDOWS)
#    include <windows.h>
#    include <rpc.h>
#endif
#include <fmt/core.h>

#include <span>

namespace
{
void write_uuid_string(u32 (&data)[4], char (&str)[exo::UUID::STR_LEN])
{
    std::memset(str, 0, exo::UUID::STR_LEN);
    fmt::format_to(str, "{:08x}-{:08x}-{:08x}-{:08x}", data[0], data[1], data[2], data[3]);
}
} // namespace

namespace exo
{
UUID UUID::create()
{
    UUID new_uuid = {};

#if defined(CROSS_WINDOWS)
    while (!new_uuid.is_valid())
    {
        auto *win32_uuid = reinterpret_cast<::UUID *>(&new_uuid.data);
        static_assert(sizeof(::UUID) == sizeof(u32) * 4);
        auto res = ::UuidCreate(win32_uuid);
        ASSERT(res == RPC_S_OK);
    }
#endif

    write_uuid_string(new_uuid.data, new_uuid.str);
    return new_uuid;
}

UUID UUID::from_string(const char *s, usize len)
{
    UUID new_uuid;

    ASSERT(len == UUID::STR_LEN);

    // 83ce0c20-4bb21feb-e6957dbb-5fcc54d5
    for (usize i = 0; i < UUID::STR_LEN; i += 1)
    {
        if (i == 8 || i == 17 || i == 26)
        {
            ASSERT(s[i] == '-');
        }
        else
        {
            ASSERT(('0' <= s[i] && s[i] <= '9') || ('a' <= s[i] && s[i] <= 'f'));
        }

        new_uuid.str[i] = s[i];
    }

    for (usize i_data = 0; i_data < 4; i_data += 1)
    {
        // 0..8, 1..17, 18..26, 27..35
        for (usize i = (i_data * 9); i < (i_data + 1) * 8 + i_data; i += 1)
        {
            uint n                = static_cast<uint>(s[i] >= 'a' ? s[i] - 'a' + 10 : s[i] - '0');
            new_uuid.data[i_data] = new_uuid.data[i_data] * 16 + n;
        }
    }

    return new_uuid;
}

UUID UUID::from_values(const u32 *values)
{
    UUID new_uuid;
    for (usize i_data = 0; i_data < 4; i_data += 1)
    {
        new_uuid.data[i_data] = values[i_data];
    }
    write_uuid_string(new_uuid.data, new_uuid.str);
    return new_uuid;
}
} // namespace exo

namespace exo
{
usize hash_value(const exo::UUID &uuid)
{
    return phmap::HashState().combine(uuid.data[0], uuid.data[1], uuid.data[2], uuid.data[3]);
}

template <>
void Serializer::serialize<exo::UUID>(exo::UUID &data)
{
    serialize(data.data);
    if (this->is_writing == false)
    {
        write_uuid_string(data.data, data.str);
    }
}
} // namespace exo
