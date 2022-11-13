#pragma once
#include <assets/asset_database.h>
#include <exo/maths/numerics.h>
#include <exo/maths/u128.h>

#include "exo/collections/span.h"

#include <meow_hash_x64_aesni.h>

namespace assets
{
inline u64 hash_file64(exo::Span<const u8> content)
{
	void *non_const_data = const_cast<u8 *>(content.data());
	auto  meow_hash      = MeowHash(MeowDefaultSeed, content.len(), non_const_data);
	return static_cast<u64>(_mm_extract_epi64(meow_hash, 0));
}

inline exo::u128 hash_file128(exo::Span<const u8> content)
{
	void *non_const_data = const_cast<u8 *>(content.data());
	auto  meow_hash      = MeowHash(MeowDefaultSeed, content.len(), non_const_data);
	return meow_hash;
}
} // namespace assets
