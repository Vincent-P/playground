#pragma once
#include "exo/prelude.h"

#include <iterator>

// Inspired by https://vector-of-bool.github.io/2020/06/13/cpp20-iter-facade.html

// helper
/**
 * If `T` is a reference type, the `type` member names a
 * std::reference_wrapper<U>, where `U` is the referred-to type of `T`
 * (including cv-qualifierd). For all other types, the `type` member is `T`
 * unmodified.
 */
template <typename T>
struct wrap_refs {
    using type = T;
};

template <typename T>
requires std::is_reference_v<T>  //
    struct wrap_refs<T> {
    using type = std::reference_wrapper<std::remove_reference_t<T>>;
};

/**
 * If the given `T` is a reference, becomes a `std::reference_wrapper<U>` where
 * `U` is the referred-to type. Otherwise, becomes `T` unmodified.
 */
template <typename T>
using wrap_refs_t = typename wrap_refs<T>::type;

template <class Reference>
struct arrow_proxy
{
    wrap_refs_t<Reference> _value;

    explicit constexpr arrow_proxy(Reference&& r) noexcept : _value(std::forward<Reference>(r)) {}
    constexpr auto operator->() noexcept { return std::addressof(**this); }
    constexpr auto operator->() const noexcept { return std::addressof(**this); }
};

template <typename Reference>
arrow_proxy(Reference &&) -> arrow_proxy<Reference>;

/**
   Contains the boilerplate needed to create a std-compatible iterator
 **/

// The main iterator struct facade, it contains iterator functions
template<typename Iterator>
struct IteratorFacade
{
    // CRTP (Curiously recurring template pattern) helpers to use the "real" iterator impl class
    Iterator &self()
    {
        return static_cast<Iterator &>(*this);
    }

    const Iterator &self() const
    {
        return static_cast<const Iterator &>(*this);
    }

    // input iterator
    decltype(auto) operator*() const
    {
        return self().dereference();
    }

    Iterator &operator++()
    {
        self().increment();
        return self();
    }

    // forward iterator
    Iterator operator++(int n)
    {
        auto copy = self();
        for (int i = 0; i < n; i += 1)
        {
            ++(*this);
        }
        return copy;
    }

    friend bool operator==(const Iterator &lhs, const Iterator &rhs)
    {
        return lhs.equal_to(rhs);
    }

    //
    auto operator->() const
    {
        decltype(auto) ref = **this;
        if constexpr (std::is_reference_v<decltype(ref)>)
        {
            // `ref` is a true reference, and we're safe to take its address
            return std::addressof(ref);
        }
        else
        {
            // `ref` is *not* a reference. Returning its address would be the
            // address of a local. Return that thing wrapped in an arrow_proxy.
            return arrow_proxy(std::move(ref));
        }
    }
};

// iterator_traits specialization for the facade

// clang-format off

// Infer the difference type with a fallback to `ptrdiff_t`
template <typename T> concept impls_distance_to = requires(const T it) { it.distance_to(it); };

template <typename>
struct infer_difference_type { using type = std::ptrdiff_t; };

template <impls_distance_to T>
struct infer_difference_type<T>
{
    static const T &it;
    using type = decltype(it.distance_to(it));
};

template <typename Iterator> using infer_difference_type_t = typename infer_difference_type<Iterator>::type;

// clang-format on

namespace std
{
template <typename Iterator>
requires std::is_base_of_v<IteratorFacade<Iterator>, Iterator> struct iterator_traits<Iterator>
{
    static const Iterator &_it;

    using difference_type   = infer_difference_type_t<Iterator>;
    using reference         = decltype(*_it);
    using pointer           = decltype(_it.operator->());
    using value_type        = std::remove_cvref_t<decltype(*_it)>;
    using iterator_category = std::forward_iterator_tag;
};
} // namespace std
