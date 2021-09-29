#include "cross/uuid.h"

#include <exo/prelude.h>
#if defined(CROSS_WINDOWS)
#include <windows.h>
#include <rpc.h>
#endif

namespace cross
{
UUID UUID::create()
{
    UUID new_uuid;

    auto *win32_uuid = reinterpret_cast<::UUID*>(&new_uuid);
    static_assert(sizeof(::UUID) == sizeof(UUID));
    auto res = ::UuidCreate(win32_uuid);
    ASSERT(res == RPC_S_OK);
    return new_uuid;
}
} // namespace cross
