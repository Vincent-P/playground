#pragma once
#include "exo/prelude.h"

template <typename Enum>
concept EnumCount = requires(Enum e)
{
    Enum::Count;
};

template <typename T, EnumCount E>
struct EnumArray
{
    using Self                  = EnumArray<T, E>;
    static constexpr usize SIZE = static_cast<usize>(E::Count);

    constexpr const T &operator[](E e) const;
    constexpr T &      operator[](E e);

    constexpr const T *begin() const;
    constexpr T *      begin();

    constexpr const T *end() const;
    constexpr T *      end();

    // Note that the member data is intentionally public.
    // This allows for aggregate initialization of the
    // object (e.g. EnumArray<int, IntEnum> a = { 0, 3, 2, 4 }; )
    T array[SIZE];
};

template <typename T, EnumCount E>
constexpr const T &EnumArray<T, E>::operator[](E e) const
{
    assert(static_cast<usize>(e) < SIZE);
    return array[static_cast<usize>(e)];
}

template <typename T, EnumCount E>
constexpr T &EnumArray<T, E>::operator[](E e)
{
    return const_cast<T &>(static_cast<const Self &>(*this)[e]);
}

template <typename T, EnumCount E>
constexpr const T *EnumArray<T, E>::begin() const
{
    return &array[0];
}

template <typename T, EnumCount E>
constexpr T *EnumArray<T, E>::begin()
{
    return const_cast<T *>(static_cast<const Self &>(*this).begin());
}

template <typename T, EnumCount E>
constexpr const T *EnumArray<T, E>::end() const
{
    return &array[SIZE];
}

template <typename T, EnumCount E>
constexpr T *EnumArray<T, E>::end()
{
    return const_cast<T *>(static_cast<const Self &>(*this).end());
}
