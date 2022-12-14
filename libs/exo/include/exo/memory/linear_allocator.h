#pragma once
#include "exo/maths/numerics.h"

namespace exo
{
struct LinearAllocator
{
public:
	static LinearAllocator with_external_memory(void *p, usize len);

	LinearAllocator()                             = default;
	LinearAllocator(const LinearAllocator &other) = delete;
	LinearAllocator &operator=(const LinearAllocator &other) = delete;
	LinearAllocator(LinearAllocator &&other) noexcept;
	LinearAllocator &operator=(LinearAllocator &&other) noexcept;

	void                    *allocate(usize size);
	template <typename T> T *allocate(usize nb_element)
	{
		return reinterpret_cast<T *>(this->allocate(nb_element * sizeof(T)));
	}

	void rewind(void *p);

	void *get_ptr() const { return ptr; }

private:
	u8 *base_address = nullptr;
	u8 *ptr          = nullptr;
	u8 *end          = nullptr;
};

inline u8                tls_data[256 << 10];
inline auto tls_allocator = LinearAllocator::with_external_memory(tls_data, sizeof(tls_data));
}; // namespace exo
