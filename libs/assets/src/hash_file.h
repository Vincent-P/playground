#pragma once
#include <exo/maths/numerics.h>

#include <span>

#include <meow_hash_x64_aesni.h>

namespace assets
{
static u64 hash_file(std::span<const u8> content)
{
	void *non_const_data = const_cast<u8 *>(content.data());
	auto  meow_hash      = MeowHash(MeowDefaultSeed, content.size(), non_const_data);
	return static_cast<u64>(_mm_extract_epi64(meow_hash, 0));
}
} // namespace assets
