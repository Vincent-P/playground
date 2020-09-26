#pragma once
#include "base/types.hpp"
#include <variant>
#include <vector>

/// --- Handle type (Typed index that can be invalid)
template <typename T> struct Handle
{
    static Handle invalid()
    {
        return Handle();
    }

    Handle()
        : index(u32_invalid)
        , gen(u32_invalid)
    {
    }

    explicit Handle(u32 i)
        : index(i)
    {
        assert(index != u32_invalid);
        static u32 cur_gen = 0;
        gen                = cur_gen++;
    }

    [[nodiscard]] u32 value() const
    {
        return index;
    }
    [[nodiscard]] bool is_valid() const
    {
        return index != u32_invalid && gen != u32_invalid;
    }

    bool operator==(const Handle &b) const = default;

  private:
    u32 index;
    u32 gen;

    friend struct ::std::hash<Handle<T>>;
};

namespace std
{
    template<typename T>
    struct hash<Handle<T>>
    {
        std::size_t operator()(Handle<T> const& handle) const noexcept
        {
            std::size_t h1 = std::hash<u32>{}(handle.index);
            std::size_t h2 = std::hash<u32>{}(handle.gen);
            return h1 ^ (h2 << 1); // or use boost::hash_combine
        }
    };
}

/// --- Pool allocator
template <typename T> class Pool
{
    class Iterator
    {
      public:
        using difference_type   = int;
        using value_type        = std::pair<Handle<T>, T*>;
        using pointer           = value_type *;
        using reference         = value_type &;
        using iterator_category = std::input_iterator_tag;

        Iterator() = default;

        Iterator(Pool &_pool, usize _index = 0)
            : pool{&_pool}
            , current_index{_index}
            , value{}
        {
            for (; current_index < pool->data.size(); current_index++)
            {
                // index() returns a zero-based index of the type
                // 0: handle_type
                // 1: T
                if (pool->data[current_index].index() == 1)
                {
                    break;
                }
            }
        }

        Iterator(const Iterator &rhs)
        {
            this->pool         = rhs.pool;
            this->current_index = rhs.current_index;
        }

        Iterator(Iterator &&rhs)
        {
            this->pool         = rhs.pool;
            this->current_index = rhs.current_index;
        }

        Iterator &operator=(const Iterator &rhs)
        {
            this->pool          = rhs.pool;
            this->current_index = rhs.current_index;
            return *this;
        }

        Iterator &operator=(Iterator &&rhs)
        {
            this->pool         = rhs.pool;
            this->current_index = rhs.current_index;
            return *this;
        }

        bool operator==(const Iterator &rhs) const
        {
            return current_index == rhs.current_index;
        }

        reference operator*()
        {
            assert(this->pool && current_index < this->pool->data.size());
            value = std::make_pair(this->pool->keys[current_index], &this->pool->get_value_internal(current_index));
            return value;
        }

        Iterator &operator++()
        {
            assert(this->pool);
            current_index++;
            for (; current_index < pool->data.size(); current_index++)
            {
                // index() returns a zero-based index of the type
                // 0: handle_type
                // 1: T
                if (pool->data[current_index].index() == 1)
                {
                    break;
                }
            }
            return *this;
        }

        Iterator &operator++(int n)
        {
            assert(this->pool && n > 0);
            for (int i = 0; i < n; i++)
            {
                for (; current_index < pool->data.size(); current_index++)
                {
                    // index() returns a zero-based index of the type
                    // 0: handle_type
                    // 1: T
                    if (pool->data[current_index].index() == 1)
                    {
                        break;
                    }
                }
            }
            return *this;
        }
    private:
        Pool *pool        = nullptr;
        usize current_index = 0;
        value_type value = {};
    };

    // static_assert(std::input_iterator<Iterator>);

    using handle_type  = Handle<T>;
    using element_type = std::variant<handle_type, T>; // < next_free_ptr, value >

    handle_type &get_handle_internal(handle_type handle)
    {
        return std::get<handle_type>(data[handle.value()]);
    }

    T &get_value_internal(u32 index)
    {
        return std::get<T>(data[index]);
    }

    T &get_value_internal(handle_type handle)
    {
        return get_value_internal(handle.value());
    }

  public:
    Pool() = default;
    Pool(usize capacity)
    {
        data.reserve(capacity);
        keys.reserve(capacity);
    }

    handle_type add(T &&value)
    {
        data_size += 1;

        if (!first_free.is_valid())
        {
            data.push_back(std::move(value));
            auto handle = handle_type(data.size() - 1);
            keys.push_back(handle);
            return handle;
        }

        // Pop the free list
        handle_type old_first_free = first_free;
        first_free                 = get_handle_internal(old_first_free);

        // put the value in
        data[old_first_free.value()] = std::move(value);
        keys[old_first_free.value()] = old_first_free;

        return old_first_free;
    }

    T *get(handle_type handle)
    {
        if (!handle.is_valid())
        {
            assert(!"invalid handle");
            return nullptr;
        }

        if (0 && handle != keys[handle.value()])
        {
            assert(!"use after free");
            return nullptr;
        }

        return &get_value_internal(handle);
    }

    void remove(handle_type handle)
    {
        assert(data_size != 0);
        if (!handle.is_valid())
        {
            assert(false);
        }

        data_size -= 1;

        // replace the value to remove with the head of the free list
        auto &data_element = data[handle.value()];
        auto &key_element  = keys[handle.value()];
        data_element       = first_free;
        key_element        = handle_type::invalid();

        // set the new head of the free list to the handle that was just removed
        first_free    = handle;
    }

    Iterator begin()
    {
        return Iterator(*this, 0);
    }

    Iterator end()
    {
        return Iterator(*this, data.size());
    }

    bool operator==(const Pool &rhs) const = default;

    usize size() const
    {
        return data_size;
    }

  private:
    handle_type first_free; // free list head ptr
    std::vector<element_type> data;
    std::vector<handle_type> keys;
    usize data_size{0};

    friend class Iterator;
};
