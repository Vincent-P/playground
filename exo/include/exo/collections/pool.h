#pragma once

#include "exo/prelude.h"
#include "exo/handle.h"
#include "exo/collections/vector.h"

#include <variant>
#include <type_traits>

/**
   A Pool is a linear allocator with a free-list.
   Performance:
     Adding/removing elements is O(1).
     Iterating is O(capacity) and elements are NOT tighly packed because of the free-list.

   Handle is (u32 index, u32 gen).

   Pool is (vector<variant<T, Handle>> elements, vector<Handle> keys).
   std::variant is used to make the free-list.
   keys are separated to check use-after-free. (this is bad :()
 **/

/// --- Pool allocator
template <typename T>
class Pool
{
    // static_assert(std::is_trivial<T>::value);

    class Iterator
    {
      public:
        using difference_type   = int;
        using value_type        = std::pair<Handle<T>, T*>;
        using pointer           = value_type *;
        using reference         = value_type &;
        using iterator_category = std::input_iterator_tag;

        Iterator() = default;

        Iterator(Pool &_pool, u32 _index = 0)
            : pool{&_pool}
            , current_index{_index}
            , value{}
        {
            for (; current_index < static_cast<u32>(pool->data.size()); current_index++)
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
            assert(this->pool && current_index < static_cast<u32>(this->pool->data.size()));
            value = std::make_pair(this->pool->keys[current_index], &this->pool->get_value_internal(current_index));
            return value;
        }

        Iterator &operator++()
        {
            assert(this->pool);
            current_index++;
            for (; current_index < static_cast<u32>(pool->data.size()); current_index++)
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
                for (; current_index < static_cast<u32>(pool->data.size()); current_index++)
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
        u32 current_index = 0;
        value_type value = {};
    };
    class ConstIterator
    {
      public:
        using difference_type   = int;
        using value_type        = std::pair<Handle<T>, const T*>;
        using pointer           = const value_type *;
        using reference         = const value_type &;
        using iterator_category = std::input_iterator_tag;

        ConstIterator() = default;

        ConstIterator(const Pool &_pool, u32 _index = 0)
            : pool{&_pool}
            , current_index{_index}
            , value{}
        {
            for (; current_index < static_cast<u32>(pool->data.size()); current_index++)
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

        ConstIterator(const ConstIterator &rhs)
        {
            this->pool         = rhs.pool;
            this->current_index = rhs.current_index;
        }

        ConstIterator(ConstIterator &&rhs)
        {
            this->pool         = rhs.pool;
            this->current_index = rhs.current_index;
        }

        ConstIterator &operator=(const ConstIterator &rhs)
        {
            this->pool          = rhs.pool;
            this->current_index = rhs.current_index;
            return *this;
        }

        ConstIterator &operator=(ConstIterator &&rhs)
        {
            this->pool         = rhs.pool;
            this->current_index = rhs.current_index;
            return *this;
        }

        bool operator==(const ConstIterator &rhs) const
        {
            return current_index == rhs.current_index;
        }

        reference operator*()
        {
            assert(this->pool && current_index < static_cast<u32>(this->pool->data.size()));
            value = std::make_pair(this->pool->keys[current_index], &this->pool->get_value_internal(current_index));
            return value;
        }

        ConstIterator &operator++()
        {
            assert(this->pool);
            current_index++;
            for (; current_index < static_cast<u32>(pool->data.size()); current_index++)
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

        ConstIterator &operator++(int n)
        {
            assert(this->pool && n > 0);
            for (int i = 0; i < n; i++)
            {
                for (; current_index < static_cast<u32>(pool->data.size()); current_index++)
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
        const Pool *pool        = nullptr;
        u32 current_index = 0;
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

    const T &get_value_internal(u32 index) const
    {
        return std::get<T>(data[index]);
    }

    T &get_value_internal(handle_type handle)
    {
        return get_value_internal(handle.value());
    }

    const T &get_value_internal(handle_type handle) const
    {
        return get_value_internal(handle.value());
    }

  public:
    Pool() = default;
    Pool(u32 capacity)
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
            auto handle = handle_type(static_cast<u32>(data.size() - 1));
            keys.push_back(handle);
            return handle;
        }

        // Pop the free list
        handle_type old_first_free = first_free;
        old_first_free.gen += 1;

        first_free                 = get_handle_internal(old_first_free);

        // put the value in
        data[old_first_free.value()] = std::move(value);
        keys[old_first_free.value()] = old_first_free;

        return old_first_free;
    }

    handle_type add(const T &value)
    {
        data_size += 1;

        if (!first_free.is_valid())
        {
            data.push_back(value);
            auto handle = handle_type(static_cast<u32>(data.size() - 1));
            keys.push_back(handle);
            return handle;
        }

        // Pop the free list
        handle_type old_first_free = first_free;
        old_first_free.gen += 1;

        first_free                 = get_handle_internal(old_first_free);

        // put the value in
        data[old_first_free.value()] = value;
        keys[old_first_free.value()] = old_first_free;

        return old_first_free;
    }

    const T *get(handle_type handle) const
    {
        if (!handle.is_valid())
        {
            return nullptr;
        }

        if (handle != keys[handle.value()])
        {
            assert(!"use after free");
            return nullptr;
        }

        return &get_value_internal(handle);
    }

    T *get(handle_type handle)
    {
        if (!handle.is_valid())
        {
            return nullptr;
        }

        if (handle != keys[handle.value()])
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

    ConstIterator begin() const
    {
        return ConstIterator(*this, 0);
    }

    Iterator end()
    {
        return Iterator(*this, static_cast<u32>(data.size()));
    }

    ConstIterator end() const
    {
        return ConstIterator(*this, static_cast<u32>(data.size()));
    }

    bool operator==(const Pool &rhs) const = default;

    u32 size() const
    {
        return data_size;
    }

    inline const void *data_ptr() const { return data.data(); }

  private:
    handle_type first_free; // free list head ptr
    Vec<element_type> data;
    Vec<handle_type> keys;
    u32 data_size{0};

    friend class Iterator;
};
