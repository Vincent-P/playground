#include "cross/uuid.h"

#include <exo/prelude.h>
#if defined(CROSS_WINDOWS)
#include <windows.h>
#include <rpc.h>
#endif
#include <exo/collections/map.h>

#include <fmt/core.h>

namespace cross
{

namespace
{
    void write_uuid_string(u32 (&data)[4], char (&str)[UUID::STR_LEN])
    {
        std::memset(str, 0, UUID::STR_LEN);
        fmt::format_to(str, "{:x}-{:x}-{:x}-{:x}", data[0], data[1], data[2], data[3]);
    }
}

UUID UUID::create()
{
    UUID new_uuid;

    auto *win32_uuid = reinterpret_cast<::UUID*>(&new_uuid.data);
    static_assert(sizeof(::UUID) == sizeof(u32) * 4);
    auto res = ::UuidCreate(win32_uuid);
    ASSERT(res == RPC_S_OK);
    write_uuid_string(new_uuid.data, new_uuid.str);
    return new_uuid;
}

usize hash_value(const cross::UUID &uuid)
{
    return phmap::HashState().combine(uuid.data[0], uuid.data[1], uuid.data[2], uuid.data[3]);
}

} // namespace cross
