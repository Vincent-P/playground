#pragma once
#include "exo/macros/assert.h"
#include "exo/maths/numerics.h"
#include <type_traits>
#include <span>
#include <initializer_list>

namespace exo
{
template <typename T, usize CAPACITY>
struct DynamicArray
{
    static_assert(std::is_default_constructible<T>());

    using Self                  = DynamicArray<T, CAPACITY>;

    constexpr DynamicArray() = default;

    constexpr DynamicArray(std::span<const T> values)
    {
        ASSERT(values.size() < CAPACITY);
        array_size = values.size();
        for (usize i = 0; i < array_size; i += 1)
        {
            array[i] = values[i];
        }
    }

    constexpr DynamicArray(std::initializer_list<T> list)
        : DynamicArray(std::span{list})
        {}

    constexpr DynamicArray(const DynamicArray &other) = default;
    constexpr DynamicArray &operator=(const DynamicArray &other) = default;

    constexpr DynamicArray(DynamicArray &&other) = default;
    constexpr DynamicArray &operator=(DynamicArray &&other) = default;

    constexpr ~DynamicArray()
    {
        if constexpr (std::is_trivially_destructible<T>() == false)
        {
            for (usize i = 0; i < array_size; i += 1)
            {
                array[i].~T();
            }
        }
    }

    constexpr const T &operator[](usize i) const;
    constexpr T &      operator[](usize i);

    // clang-format off
    constexpr const T *begin() const noexcept  { return &array[0]; }
    constexpr T *      begin() noexcept        { return &array[0]; }
    constexpr const T *end() const noexcept    { return begin() + array_size; }
    constexpr T *      end() noexcept          { return begin() + array_size; }
    constexpr usize size() const noexcept      { return end() - begin(); }
    constexpr usize capacity() const noexcept      { return CAPACITY; }
    constexpr bool empty() const noexcept      { return array_size == 0; }
    constexpr const T *data() const noexcept   { return &array[0]; }
    constexpr T *data() noexcept               { return &array[0]; }
    // clang-format on

    // std::vector-like interface
    constexpr void push_back(T &&value) noexcept;
    constexpr void push_back(const T &value) noexcept;
    constexpr void clear() noexcept;
    constexpr void resize(usize new_size) noexcept;
    constexpr T& back() noexcept;
    constexpr const T& back() const noexcept;

    usize array_size = {0};
    T  array[CAPACITY] = {};
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
constexpr void DynamicArray<T, C>::push_back(const T &value) noexcept
{
    ASSERT(array_size + 1 < C);
    array[array_size] = value;
    array_size += 1;
}


template <typename T, usize C>
constexpr void DynamicArray<T, C>::clear() noexcept
{
    array_size = 0;

    if constexpr (std::is_trivially_destructible<T>() == false)
    {
        for (usize i = 0; i < array_size; i += 1)
        {
            array[i].~T();
        }
    }
}

template <typename T, usize C>
constexpr void DynamicArray<T, C>::resize(usize new_size) noexcept
{
    ASSERT(new_size <= C);

    // Default-initialize new elements if new_size > old_size
    for (usize i = array_size; i < new_size; i += 1)
    {
        array[i] = {};
    }

    array_size = new_size;
}

template <typename T, usize C>
constexpr T& DynamicArray<T, C>::back() noexcept
{
    return array[array_size - 1];
}

template <typename T, usize C>
constexpr const T& DynamicArray<T, C>::back() const noexcept
{
    return array[array_size - 1];
}

template <typename T, usize C1, usize C2>
constexpr bool operator==(const DynamicArray<T, C1> &lhs, const DynamicArray<T, C2> &rhs)
{
    if (lhs.array_size != rhs.array_size)
    {
        return false;
    }

    for (usize i = 0; i < lhs.array_size; i += 1)
    {
        if (lhs[i] != rhs[i])
        {
            return false;
        }
    }

    return true;
}
}
