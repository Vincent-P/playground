#pragma once
#include "exo/prelude.h"
#include <type_traits>

template <typename T, usize CAPACITY>
struct DynamicArray
{
    using Self                  = DynamicArray<T, CAPACITY>;

    static_assert(std::is_standard_layout<T>());
    // we dont call destructors here, and we dont memcpy or init to 0 so only trivial destructor is needed
    static_assert(std::is_trivially_destructible<T>());

    constexpr const T &operator[](usize i) const;
    constexpr T &      operator[](usize i);
    // clang-format off
    constexpr const T *begin() const noexcept  { return &array[0]; }
    constexpr T *      begin() noexcept        { return &array[0]; }
    constexpr const T *end() const noexcept    { return begin() + array_size; }
    constexpr T *      end() noexcept          { return begin() + array_size; }
    constexpr usize size() const noexcept      { return end() - begin(); }
    // clang-format on

    // std::vector-like interface
    constexpr void push_back(T &&value) noexcept;
    constexpr void clear() noexcept;
    constexpr void resize(usize new_size) noexcept;

    T  array[CAPACITY];
    usize array_size;
};

template <typename T, usize C>
constexpr const T &DynamicArray<T, C>::operator[](usize i) const
{
    ASSERT(i < array_size);
    return array[i];
}

template <typename T, usize C>
constexpr T &DynamicArray<T, C>::operator[](usize i)
{
    return const_cast<T &>(static_cast<const Self &>(*this)[i]);
}

template <typename T, usize C>
constexpr void DynamicArray<T, C>::push_back(T &&value) noexcept
{
    ASSERT(array_size + 1 < C);
    array[array_size] = std::move(value);
    array_size += 1;
}

template <typename T, usize C>
constexpr void DynamicArray<T, C>::clear() noexcept
{
    array_size = 0;
}

template <typename T, usize C>
constexpr void DynamicArray<T, C>::resize(usize new_size) noexcept
{
    ASSERT(new_size < C);

    // Default-initialize new elements if new_size > old_size
    for (usize i = array_size; i < new_size; i += 1)
    {
        array[i] = {};
    }

    array_size = new_size;
}
