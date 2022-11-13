#pragma once
#include <exo/macros/assert.h>
#include <exo/maths/numerics.h>
#include <initializer_list>

namespace exo
{

namespace details
{
template <typename A, typename B>
inline constexpr bool IsSameV = false;

template <typename A>
inline constexpr bool IsSameV<A, A> = true;

template <typename A, typename B>
concept IsSame = IsSameV<A, B>;

template <typename T>
struct RemoveConstS
{
	using Type = T;
};

template <typename T>
struct RemoveConstS<const T>
{
	using Type = T;
};

template <typename T>
using RemoveConst = typename RemoveConstS<T>::Type;

template <typename T>
struct RemovePtrS
{
	using Type = T;
};

template <typename T>
struct RemovePtrS<T *>
{
	using Type = T;
};

template <typename T>
using RemovePtr = typename RemovePtrS<T>::Type;

// clang-format off
template<typename Collection>
concept HasDataMemberFunc = requires(const Collection& collection) {
	collection.data();
};

template<typename Collection>
concept HasLenMemberFunc = requires(const Collection& collection) {
	{ collection.len() } -> IsSame<usize>;
};

template<typename Collection>
concept HasSizeMemberFunc = requires(const Collection& collection) {
	{ collection.size() } -> IsSame<usize>;
};

template<typename Collection>
concept HasDataAndLenMemberFunc = HasDataMemberFunc<Collection> && HasLenMemberFunc<Collection>;

template<typename Collection>
concept HasDataAndSizeMemberFunc = HasDataMemberFunc<Collection> && HasSizeMemberFunc<Collection>;

template<HasDataMemberFunc Collection>
using CollectionElementType = RemovePtr<decltype(((Collection*)nullptr)->data())>;

template<typename Collection, typename T>
concept IsCollectionSpanConvertible = HasDataAndLenMemberFunc<Collection> && (IsSame<CollectionElementType<Collection>, RemoveConst<T>> || IsSame<CollectionElementType<Collection>, T>);

template<typename Collection, typename T>
concept IsCollectionSpanConvertibleSize = HasDataAndSizeMemberFunc<Collection> && (IsSame<CollectionElementType<Collection>, RemoveConst<T>> || IsSame<CollectionElementType<Collection>, T>);

// clang-format on

}; // namespace details

template <typename T>
struct Span
{
	T    *ptr    = nullptr;
	usize length = 0;

	// --

	Span() = default;
	Span(T *data, usize len) : ptr{data}, length{len} {}
	Span(T *begin, T *end) : ptr{begin}, length(end - begin) {}

	// Constructor from other span without const T (mutable -> const span of same type)
	template <details::IsSame<details::RemoveConst<T>> Other>
	Span(const Span<Other> &other) : Span{other.ptr, other.length}
	{
	}

	template <details::IsSame<details::RemoveConst<T>> Other>
	Span(Span<Other> &&other) : Span{other.ptr, other.length}
	{
	}

	// Constructor from other collections
	template <details::IsCollectionSpanConvertible<T> Collection>
	Span(Collection &collection) : Span{collection.data(), collection.len()}
	{
	}

	template <details::IsCollectionSpanConvertibleSize<T> Collection>
	Span(Collection &collection) : Span{collection.data(), collection.size()}
	{
	}

	template <details::IsCollectionSpanConvertible<T> Collection>
	Span(const Collection &collection) : Span{collection.data(), collection.len()}
	{
	}

	template <details::IsCollectionSpanConvertibleSize<T> Collection>
	Span(const Collection &collection) : Span{collection.data(), collection.size()}
	{
	}

	// Delete rvalue reference constructors
	template <details::IsCollectionSpanConvertible<T> Collection>
	Span(Collection &&collection) = delete;

	template <details::IsCollectionSpanConvertibleSize<T> Collection>
	Span(Collection &&collection) = delete;

	// -- Iterators

	T *begin() const { return ptr; }
	T *end() const { return ptr + length; }

	// -- Element access

	T &operator[](usize i) const
	{
		ASSERT(i < this->length);
		return this->ptr[i];
	}

	T *data() const { return this->ptr; }

	T &back() const { return (*this)[this->length - 1]; }

	// -- Observers

	usize len() const { return this->length; }

	bool empty() const { return this->length == 0; }

	// -- Subviews

	Span subspan(usize offset)
	{
		ASSERT(this->length >= offset);
		return Span{this->ptr + offset, this->length - offset};
	}

	// -- STL compatibility

	usize size_bytes() const { return length * sizeof(T); }
};

// error C2641: cannot deduce template arguments for 'exo::Span'
template <details::HasDataAndLenMemberFunc Collection>
Span(Collection &) -> Span<details::CollectionElementType<Collection>>;

template <details::HasDataAndSizeMemberFunc Collection>
Span(Collection &) -> Span<details::CollectionElementType<Collection>>;

template <typename T>
Span<T> reinterpret_span(Span<u8> bytes)
{
	ASSERT(bytes.size_bytes() % sizeof(T) == 0);
	return Span(reinterpret_cast<T *>(bytes.data()), bytes.size_bytes() / sizeof(T));
}

template <typename T>
Span<const T> reinterpret_span(Span<const u8> bytes)
{
	ASSERT(bytes.size_bytes() % sizeof(T) == 0);
	return Span(reinterpret_cast<const T *>(bytes.data()), bytes.size_bytes() / sizeof(T));
}

template <typename T>
Span<const u8> span_to_bytes(Span<T> elements)
{
	return Span(reinterpret_cast<const u8 *>(elements.data()), elements.size_bytes());
}
}; // namespace exo
