#pragma once
#include <cassert>
#include <cstring>
#include <type_traits>
#include <stdexcept>
#include <memory>
#include <algorithm>
#include <initializer_list>

namespace xm {
/**
 * if StackSize == 0: same as std::vector,
 * if StackSize > 0: same as llvm::SmallVector.
 * all algorithm follow gcc's std::vector implement.
 */
template <typename T, uint32_t StackSize = 0, typename A = std::allocator<T>>
class Array {
public:
    using allocator_type = A;
    using allocator_traits = std::allocator_traits<A>;
    using value_type = typename allocator_traits::value_type;
    using size_type = typename allocator_traits::size_type;
    using pointer = typename allocator_traits::pointer;
    using const_pointer = typename allocator_traits::const_pointer;
    using reference = value_type&;
    using const_reference = const value_type&;
    using iterator = pointer;
    using const_iterator = const_pointer;
    using reverse_iterator = typename std::reverse_iterator<iterator>;
    using const_reverse_iterator = typename std::reverse_iterator<const_iterator>;

    template <typename Itr, typename Tag>
    using is_itr = typename std::
        is_convertible<typename std::iterator_traits<Itr>::iterator_category, Tag>::type;

    enum : uint32_t {
        STACK_SIZE = StackSize,
    };

    //
    // constructor
    //

    Array() {}

    explicit Array(const allocator_type& alloc) : m_storage(alloc) {}

    explicit Array(size_type n, const allocator_type& alloc = allocator_type()) : m_storage(alloc) {
        if (STACK_SIZE >= n) {
            set_end(uninitialized_default_n_a(begin(), n, get_alloc()));
        } else {
            TmpHeap tmp_heap(this, check_size(n));
            tmp_heap.end = uninitialized_default_n_a(tmp_heap.begin, n, get_alloc());
            tmp_heap.to_parent();
        }
    }

    explicit Array(
        size_type n, const_reference value, const allocator_type& alloc = allocator_type())
            : m_storage(alloc) {
        if (STACK_SIZE >= n) {
            set_end(uninitialized_fill_n_a(begin(), n, value, get_alloc()));
        } else {
            TmpHeap tmp_heap(this, check_size(n));
            tmp_heap.end = uninitialized_fill_n_a(tmp_heap.begin, n, value, get_alloc());
            tmp_heap.to_parent();
        }
    }

    template <
        typename Itr,
        std::enable_if_t<is_itr<Itr, std::input_iterator_tag>::value>* = nullptr>
    Array(Itr first, Itr last, const allocator_type& alloc = allocator_type()) : m_storage(alloc) {
        if (is_itr<Itr, std::forward_iterator_tag>::value) {  // forward itr version.
            auto n = std::distance(first, last);
            if (STACK_SIZE >= n) {
                set_end(uninitialized_copy_a(first, last, begin(), get_alloc()));
            } else {
                TmpHeap tmp_heap(this, check_size(n));
                tmp_heap.end = uninitialized_copy_a(first, last, tmp_heap.begin, get_alloc());
                tmp_heap.to_parent();
            }
        } else {  // input itr version.
            try {
                for (; first != last; ++first) {
                    emplace_back(*first);
                }
            } catch (...) {
                clear();
                throw;
            }
        }
    }

    Array(const Array& other) : Array(other.begin(), other.end(), other.get_alloc()) {}

    Array(const Array& other, const allocator_type& alloc)
            : Array(other.begin(), other.end(), alloc) {}

    Array(
        std::initializer_list<value_type> init_list, const allocator_type& alloc = allocator_type())
            : Array(init_list.begin(), init_list.end(), alloc) {}

    Array(Array&& other) noexcept(std::is_nothrow_move_constructible<value_type>::value)
            : m_storage(std::move(other.get_alloc())) {
        constructor_move(other, std::true_type {});
    }

    Array(Array&& other, const allocator_type& alloc) : m_storage(alloc) {
        constructor_move(other, is_alloc_always_equal(allocator_traits {}));
    }

    ~Array() {
        release();
    }

    Array& operator=(const Array& other) {
        if (std::addressof(other) == this) {
            return *this;
        }
        assign(other.begin(), other.end());
        return *this;
    }

    Array& operator=(std::initializer_list<value_type> init_list) {
        assign(init_list.begin(), init_list.end());
        return *this;
    }

    Array& operator=(Array&& other) noexcept(
        std::is_nothrow_move_constructible<value_type>::value) {
        if (std::addressof(other) == this) {
            return *this;
        }
        using pocma = typename allocator_traits::propagate_on_container_move_assignment;
        constexpr bool v = pocma::value || is_alloc_always_equal(allocator_traits {}).value;
        assignment_move(other, std::integral_constant<bool, v> {});
        return *this;
    }

    ///
    /// get_allocator
    ///

    allocator_type get_allocator() const noexcept {
        return m_storage;
    }

    ///
    /// assign
    ///

    void assign(size_type n, const_reference v) {
        if (is_assign_enough(n)) {
            size_type my_size = size();
            if (my_size >= n) {
                erase_to_end(fill_n(begin(), n, v));
            } else {
                fill(begin(), end(), v);
                set_end(uninitialized_fill_n_a(end(), n - my_size, v, get_alloc()));
            }
        } else {
            TmpHeap tmp_heap(this, check_size(n));
            tmp_heap.end = uninitialized_fill_n_a(tmp_heap.begin, n, v, get_alloc());
            tmp_heap.to_parent();
        }
    }

    template <
        typename Itr,
        std::enable_if_t<is_itr<Itr, std::input_iterator_tag>::value>* = nullptr>
    void assign(Itr first, Itr last) {
        if (is_itr<Itr, std::forward_iterator_tag>::value) {  // forward itr version.
            size_type n = std::distance(first, last);
            if (is_assign_enough(n)) {
                size_type my_size = size();
                if (my_size >= n) {
                    iterator pos = copy_range(first, last, begin());
                    erase_to_end(pos);
                } else {
                    Itr mid = first;
                    std::advance(mid, my_size);
                    copy_range(first, mid, begin());
                    set_end(uninitialized_copy_a(mid, last, end(), get_alloc()));
                }
            } else {
                TmpHeap tmp_heap(this, check_size(n));
                tmp_heap.end = uninitialized_copy_a(first, last, tmp_heap.begin, get_alloc());
                tmp_heap.to_parent();
            }
        } else {  // input itr version.
            iterator cur = begin();
            for (; first != last && cur != end(); ++cur, ++first) {
                *cur = *first;
            }
            if (first == last) {
                erase_to_end(cur);
            } else {
                for (; first != last; ++first) {
                    emplace_back(*first);
                }
            }
        }
    }

    void assign(std::initializer_list<value_type> init_list) {
        assign(init_list.begin(), init_list.end());
    }

    ///
    /// element access
    ///

    reference at(size_type i) {
        if (i >= size()) {
            throw std::out_of_range("Array::at out of range");
        }
        return data()[i];
    }

    const_reference at(size_type i) const {
        if (i >= size()) {
            throw std::out_of_range("Array::at out of range");
        }
        return data()[i];
    }

    reference operator[](size_type i) {
        assert(i < size());
        return data()[i];
    }

    const_reference operator[](size_type i) const {
        assert(i < size());
        return data()[i];
    }

    reference front() {
        assert(!empty());
        return *begin();
    }

    const_reference front() const {
        assert(!empty());
        return *begin();
    }

    reference back() {
        assert(!empty());
        return *(end() - 1);
    }

    const_reference back() const {
        assert(!empty());
        return *(end() - 1);
    }

    pointer data() {
        return begin();
    }

    const_pointer data() const {
        return begin();
    }

    ///
    /// iterators
    ///

    iterator begin() {
        return m_storage.begin;
    }

    const_iterator begin() const {
        return m_storage.begin;
    }

    iterator end() {
        return m_storage.end;
    }

    const_iterator end() const {
        return m_storage.end;
    }

    const_iterator cbegin() const {
        return begin();
    }

    const_iterator cend() const {
        return end();
    }

    reverse_iterator rbegin() {
        return reverse_iterator(end());
    }

    const_reverse_iterator rbegin() const {
        return const_reverse_iterator(end());
    }

    reverse_iterator rend() {
        return reverse_iterator(begin());
    }

    const_reverse_iterator rend() const {
        return const_reverse_iterator(begin());
    }

    const_reverse_iterator crbegin() const {
        return rbegin();
    }

    const_reverse_iterator crend() const {
        return rend();
    }

    ///
    /// capacity
    ///

    bool empty() const {
        return end() == begin();
    }

    size_type size() const {
        return static_cast<size_type>(end() - begin());
    }

    size_type max_size() const {
        return allocator_traits::max_size(get_alloc());
    }

    void reserve(size_type n) {
        if (is_assign_enough(n)) {
            return;
        }
        TmpHeap tmp_heap(this, check_size(n));
        tmp_heap.end = uninitialized_move_a(begin(), end(), tmp_heap.begin, get_alloc());
        tmp_heap.to_parent();
    }

    size_type capacity() const {
        return static_cast<size_type>(capacity_itr() - begin());
    }

    void shrink_to_fit() {
        if (!is_from_heap()) {
            return;
        }
        size_type my_size = size();
        if (my_size > STACK_SIZE) {
            if (is_push_enough()) {
                TmpHeap tmp_heap(this, my_size);
                tmp_heap.end = uninitialized_move_a(begin(), end(), tmp_heap.begin, get_alloc());
                tmp_heap.to_parent();
            }
        } else {
            iterator old_begin = begin();
            iterator old_end = end();
            iterator old_capacity_itr = capacity_itr();
            // move to stack.
            iterator end_pos = uninitialized_move_a(begin(), end(), stack_data(), get_alloc());
            m_storage.reset();
            set_end(end_pos);
            // free olds.
            destroy_a(old_begin, old_end, get_alloc());
            allocator_traits::deallocate(get_alloc(), old_begin, old_capacity_itr - old_begin);
        }
    }

    ///
    /// modifier
    ///

    void clear() {
        destroy_a(begin(), end(), get_alloc());
        set_end(begin());
    }

    void push_back(const_reference v) {
        emplace_back(v);
    }

    void push_back(value_type&& v) {
        emplace_back(std::move(v));
    }

    template <typename... Args>
    reference emplace_back(Args&&... args) {
        if (is_push_enough()) {
            iterator ret = end();
            allocator_traits::construct(get_alloc(), end(), std::forward<Args>(args)...);
            set_end(end() + 1);
            return *ret;
        } else {
            TmpHeap tmp_heap(this, capacity_after_grow(1));
            iterator ret = tmp_heap.begin + size();
            // construct first, avoid early move.
            allocator_traits::construct(get_alloc(), ret, std::forward<Args>(args)...);
            tmp_heap.begin = ret;
            tmp_heap.end = ret + 1;
            uninitialized_move_a(begin(), end(), tmp_heap.data, get_alloc());
            tmp_heap.begin = tmp_heap.data;
            tmp_heap.to_parent();
            return *ret;
        }
    }

    void pop_back() {
        assert(!empty());
        set_end(end() - 1);
        allocator_traits::destroy(get_alloc(), end());
    }

    iterator insert(const_iterator pos, const_reference v) {
        return emplace(pos, v);
    }

    iterator insert(const_iterator pos, value_type&& v) {
        return emplace(pos, std::move(v));
    }

    template <typename... Args>
    iterator emplace(const_iterator c_pos, Args&&... args) {
        auto pos = const_cast<iterator>(c_pos);
        assert(pos >= begin() && pos <= end());
        if (is_push_enough()) {
            if (pos == end()) {
                allocator_traits::construct(get_alloc(), end(), std::forward<Args>(args)...);
                set_end(end() + 1);
            } else {
                // use tmp value, avoid early move.
                TmpValue tmp_value(this, std::forward<Args>(args)...);
                allocator_traits::construct(get_alloc(), end(), std::move(*(end() - 1)));
                set_end(end() + 1);
                std::move_backward(pos, end() - 2, end() - 1);
                *pos = std::move(*tmp_value.get());
            }
            return pos;
        } else {
            TmpHeap tmp_heap(this, capacity_after_grow(1));
            iterator ret = tmp_heap.begin + (pos - begin());
            // construct first, avoid early move.
            allocator_traits::construct(get_alloc(), ret, std::forward<Args>(args)...);
            tmp_heap.begin = ret;
            tmp_heap.end = ret + 1;
            tmp_heap.end = uninitialized_move_a(pos, end(), tmp_heap.end, get_alloc());
            uninitialized_move_a(begin(), pos, tmp_heap.data, get_alloc());
            tmp_heap.begin = tmp_heap.data;
            tmp_heap.to_parent();
            return ret;
        }
    }

    iterator insert(const_iterator c_pos, size_type n, const_reference value) {
        auto pos = const_cast<iterator>(c_pos);
        assert(pos >= begin() && pos <= end());
        if (n == 0) {
            return pos;
        }
        if (is_insert_enough(n)) {
            if (pos == end()) {
                set_end(uninitialized_fill_n_a(end(), n, value, get_alloc()));
            } else {
                TmpValue tmp_value(this, value);
                size_type ele_after = end() - pos;
                iterator old_end = end();
                if (ele_after > n) {
                    set_end(uninitialized_move_a(old_end - n, old_end, old_end, get_alloc()));
                    std::move_backward(pos, old_end - n, old_end);
                    fill(pos, pos + n, *tmp_value.get());
                } else {
                    set_end(uninitialized_fill_n_a(
                        old_end, n - ele_after, *tmp_value.get(), get_alloc()));
                    set_end(uninitialized_move_a(pos, old_end, end(), get_alloc()));
                    fill(pos, old_end, *tmp_value.get());
                }
            }
            return pos;
        } else {
            TmpHeap tmp_heap(this, capacity_after_grow(n));
            iterator ret = tmp_heap.begin + (pos - begin());
            tmp_heap.begin = ret;
            tmp_heap.end = ret;
            // fill value first, avoid early move.
            tmp_heap.end = uninitialized_fill_n_a(tmp_heap.end, n, value, get_alloc());
            tmp_heap.end = uninitialized_move_a(pos, end(), tmp_heap.end, get_alloc());
            uninitialized_move_a(begin(), pos, tmp_heap.data, get_alloc());
            tmp_heap.begin = tmp_heap.data;
            tmp_heap.to_parent();
            return ret;
        }
    }

    template <
        typename Itr,
        std::enable_if_t<is_itr<Itr, std::input_iterator_tag>::value>* = nullptr>
    iterator insert(const_iterator c_pos, Itr first, Itr last) {
        auto pos = const_cast<iterator>(c_pos);
        assert(pos >= begin() && pos <= end());
        if (is_itr<Itr, std::forward_iterator_tag>::value) {  // forward itr version.
            size_type n = std::distance(first, last);
            if (n == 0) {
                return pos;
            }
            if (is_insert_enough(n)) {
                if (pos == end()) {
                    set_end(uninitialized_copy_a(first, last, end(), get_alloc()));
                } else {
                    size_type ele_after = end() - pos;
                    iterator old_end = end();
                    if (ele_after > n) {
                        set_end(uninitialized_move_a(old_end - n, old_end, old_end, get_alloc()));
                        std::move_backward(pos, old_end - n, old_end);
                        copy_range(first, last, pos);
                    } else {
                        Itr mid = first;
                        std::advance(mid, ele_after);
                        set_end(uninitialized_copy_a(mid, last, end(), get_alloc()));
                        set_end(uninitialized_move_a(pos, old_end, end(), get_alloc()));
                        copy_range(first, mid, pos);
                    }
                }
                return pos;
            } else {
                TmpHeap tmp_heap(this, capacity_after_grow(n));
                tmp_heap.end = uninitialized_move_a(begin(), pos, tmp_heap.begin, get_alloc());
                iterator ret = tmp_heap.end;
                tmp_heap.end = uninitialized_copy_a(first, last, tmp_heap.end, get_alloc());
                tmp_heap.end = uninitialized_move_a(pos, end(), tmp_heap.end, get_alloc());
                tmp_heap.to_parent();
                return ret;
            }
        } else {  // input itr version.
            size_type pos_record = pos - begin();
            if (pos == end()) {
                for (; first != last; ++first) {
                    emplace_back(*first);
                }
            } else {
                Array<T, 0, A> tmp(first, last, get_alloc());
                insert(
                    pos, std::make_move_iterator(tmp.begin()), std::make_move_iterator(tmp.end()));
            }
            return begin() + pos_record;
        }
    }

    iterator insert(const_iterator c_pos, std::initializer_list<value_type> init_list) {
        return insert(c_pos, init_list.begin(), init_list.end());
    }

    iterator erase(const_iterator c_pos) {
        auto pos = const_cast<iterator>(c_pos);
        assert(pos >= begin() && pos < end());
        move_range(pos + 1, end(), pos);
        set_end(end() - 1);
        allocator_traits::destroy(get_alloc(), end());
        return pos;
    }

    iterator erase(const_iterator c_first, const_iterator c_last) {
        auto first = const_cast<iterator>(c_first);
        auto last = const_cast<iterator>(c_last);
        assert(first >= begin() && first <= last && last <= end());
        if (first != last) {
            move_range(last, end(), first);
            erase_to_end(first + (end() - last));
        }
        return first;
    }

    void resize(size_type new_size) {
        size_type my_size = size();
        if (new_size > my_size) {
            if (is_assign_enough(new_size)) {
                set_end(uninitialized_default_n_a(end(), new_size - my_size, get_alloc()));
            } else {
                TmpHeap tmp_heap(this, check_size(new_size));
                tmp_heap.end = uninitialized_move_a(begin(), end(), tmp_heap.begin, get_alloc());
                tmp_heap.end =
                    uninitialized_default_n_a(tmp_heap.end, new_size - my_size, get_alloc());
                tmp_heap.to_parent();
            }
        } else if (new_size < my_size) {
            erase_to_end(begin() + new_size);
        }
    }

    void resize(size_type new_size, const_reference value) {
        size_type my_size = size();
        if (new_size > my_size) {
            if (is_assign_enough(new_size)) {
                set_end(uninitialized_fill_n_a(end(), new_size - my_size, value, get_alloc()));
            } else {
                TmpHeap tmp_heap(this, check_size(new_size));
                tmp_heap.end = uninitialized_move_a(begin(), end(), tmp_heap.begin, get_alloc());
                tmp_heap.end =
                    uninitialized_fill_n_a(tmp_heap.end, new_size - my_size, value, get_alloc());
                tmp_heap.to_parent();
            }
        } else if (new_size < my_size) {
            erase_to_end(begin() + new_size);
        }
    }

    void swap(Array& other) noexcept(std::is_nothrow_move_constructible<value_type>::value) {
        using pocs = typename allocator_traits::propagate_on_container_swap;
        constexpr bool v = pocs::value || is_alloc_always_equal(allocator_traits {}).value;
        swap_impl(other, std::integral_constant<bool, v> {});
    }

    ///
    /// operators
    ///

    bool operator==(const Array& other) const {
        return (size() == other.size() && std::equal(begin(), end(), other.begin()));
    }

    bool operator!=(const Array& other) const {
        return !((*this) == other);
    }

    bool operator<(const Array& other) const {
        return std::lexicographical_compare(begin(), end(), other.begin(), other.end());
    }

    bool operator>(const Array& other) const {
        return other < (*this);
    }

    bool operator<=(const Array& other) const {
        return !(other < (*this));
    }

    bool operator>=(const Array& other) const {
        return !((*this) < other);
    }

    ///
    /// non-standard
    ///

    /**
     * append element(emplace way),
     * guarantee(after append): capacity() - size() >= keep_free_capacity.
     * return true if has re-allocate in append, otherwise return false.
     */
    template <typename... Args>
    bool emplace_append(size_type keep_free_capacity, Args&&... args) {
        size_type required_capacity = 1 + keep_free_capacity;
        if (is_insert_enough(required_capacity)) {
            allocator_traits::construct(get_alloc(), end(), std::forward<Args>(args)...);
            set_end(end() + 1);
            return false;
        } else {
            TmpHeap tmp_heap(this, capacity_after_grow(required_capacity));
            iterator ret = tmp_heap.begin + size();
            // construct first, avoid early move.
            allocator_traits::construct(get_alloc(), ret, std::forward<Args>(args)...);
            tmp_heap.begin = ret;
            tmp_heap.end = ret + 1;
            uninitialized_move_a(begin(), end(), tmp_heap.data, get_alloc());
            tmp_heap.begin = tmp_heap.data;
            tmp_heap.to_parent();
            return true;
        }
    }

    /**
     * append n default element,
     * guarantee(after append): capacity() - size() >= keep_free_capacity.
     * return true if has re-allocate in append, otherwise return false.
     */
    bool default_append(size_type n, size_type keep_free_capacity = 0) {
        size_type required_capacity = n + keep_free_capacity;
        if (required_capacity == 0) {
            return false;
        }
        if (is_insert_enough(required_capacity)) {
            set_end(uninitialized_default_n_a(end(), n, get_alloc()));
            return false;
        } else {
            TmpHeap tmp_heap(this, capacity_after_grow(required_capacity));
            tmp_heap.end = uninitialized_move_a(begin(), end(), tmp_heap.begin, get_alloc());
            tmp_heap.end = uninitialized_default_n_a(tmp_heap.end, n, get_alloc());
            tmp_heap.to_parent();
            return true;
        }
    }

    /**
     * append n element copy from value,
     * guarantee(after append): capacity() - size() >= keep_free_capacity.
     * return true if has re-allocate in append, otherwise return false.
     */
    bool append(size_type n, const_reference value, size_type keep_free_capacity = 0) {
        size_type required_capacity = n + keep_free_capacity;
        if (required_capacity == 0) {
            return false;
        }
        if (is_insert_enough(required_capacity)) {
            set_end(uninitialized_fill_n_a(end(), n, value, get_alloc()));
            return false;
        } else {
            TmpHeap tmp_heap(this, capacity_after_grow(required_capacity));
            iterator ret = tmp_heap.begin + size();
            tmp_heap.begin = ret;
            tmp_heap.end = ret;
            // fill value first, avoid early move.
            tmp_heap.end = uninitialized_fill_n_a(tmp_heap.end, n, value, get_alloc());
            uninitialized_move_a(begin(), end(), tmp_heap.data, get_alloc());
            tmp_heap.begin = tmp_heap.data;
            tmp_heap.to_parent();
            return true;
        }
    }

    /**
     * append range[first, last],
     * guarantee(after append): capacity() - size() >= keep_free_capacity.
     * return true if has re-allocate in append, otherwise return false.
     */
    template <
        typename Itr,
        std::enable_if_t<is_itr<Itr, std::forward_iterator_tag>::value>* = nullptr>
    bool append(Itr first, Itr last, size_type keep_free_capacity = 0) {
        size_type n = std::distance(first, last);
        size_type required_capacity = n + keep_free_capacity;
        if (required_capacity == 0) {
            return false;
        }
        if (is_insert_enough(required_capacity)) {
            set_end(uninitialized_copy_a(first, last, end(), get_alloc()));
            return false;
        } else {
            TmpHeap tmp_heap(this, capacity_after_grow(required_capacity));
            tmp_heap.end = uninitialized_move_a(begin(), end(), tmp_heap.begin, get_alloc());
            tmp_heap.end = uninitialized_copy_a(first, last, tmp_heap.end, get_alloc());
            tmp_heap.to_parent();
            return true;
        }
    }

    /**
     * append list,
     * guarantee(after append): capacity() - size() >= keep_free_capacity.
     * return true if has re-allocate in append, otherwise return false.
     */
    bool append(std::initializer_list<value_type> init_list, size_type keep_free_capacity = 0) {
        return append(init_list.begin(), init_list.end(), keep_free_capacity);
    }

    /**
     * append n element, keep these element uninitialized, typical for pod data.
     * guarantee(after append): capacity() - size() >= keep_free_capacity.
     * return true if has re-allocate in append, otherwise return false.
     */
    bool uninitialized_append(size_type n, size_type keep_free_capacity = 0) {
        size_type required_capacity = n + keep_free_capacity;
        if (required_capacity == 0) {
            return false;
        }
        if (is_insert_enough(required_capacity)) {
            set_end(end() + n);
            return false;
        } else {
            TmpHeap tmp_heap(this, capacity_after_grow(required_capacity));
            tmp_heap.end = uninitialized_move_a(begin(), end(), tmp_heap.begin, get_alloc());
            tmp_heap.end += n;
            tmp_heap.to_parent();
            return true;
        }
    }

private:
    void constructor_move(Array& other, std::true_type) {
        if (STACK_SIZE == 0 || other.is_from_heap()) {
            set_begin(other.begin());
            set_end(other.end());
            set_capacity_itr(other.capacity_itr());
            other.m_storage.reset();
        } else {
            set_end(uninitialized_move_a(other.begin(), other.end(), begin(), get_alloc()));
            other.clear();
        }
    }

    void constructor_move(Array& other, std::false_type) {
        if (get_alloc() == other.get_alloc()) {
            constructor_move(other, std::true_type {});
        } else {
            size_type other_size = other.size();
            if (STACK_SIZE >= other_size) {
                set_end(uninitialized_move_a(other.begin(), other.end(), begin(), get_alloc()));
            } else {
                TmpHeap tmp_heap(this, other_size);
                tmp_heap.end =
                    uninitialized_move_a(other.begin(), other.end(), tmp_heap.begin, get_alloc());
                tmp_heap.to_parent();
            }
            other.clear();
        }
    }

    void assignment_move(Array& other, std::true_type) {
        if (STACK_SIZE == 0 || other.is_from_heap()) {
            release();
            set_begin(other.begin());
            set_end(other.end());
            set_capacity_itr(other.capacity_itr());
            other.m_storage.reset();
        } else {
            size_type my_size = size();
            if (my_size >= other.size()) {
                erase_to_end(move_range(other.begin(), other.end(), begin()));
            } else {
                iterator mid = other.begin() + my_size;
                move_range(other.begin(), mid, begin());
                set_end(uninitialized_move_a(mid, other.end(), end(), get_alloc()));
            }
            other.clear();
        }
        using pocma = typename std::allocator_traits<A>::propagate_on_container_move_assignment;
        if (pocma::value) {
            get_alloc() = std::move(other.get_alloc());
        }
    }

    void assignment_move(Array& other, std::false_type) {
        if (get_alloc() == other.get_alloc()) {
            assignment_move(other, std::true_type {});
        } else {
            size_type other_size = other.size();
            if (other_size > capacity()) {
                TmpHeap tmp_heap(this, other_size);
                tmp_heap.end =
                    uninitialized_move_a(other.begin(), other.end(), tmp_heap.begin, get_alloc());
                tmp_heap.to_parent();
            } else {
                size_type my_size = size();
                if (my_size >= other_size) {
                    erase_to_end(std::move(other.begin(), other.end(), begin()));
                } else {
                    iterator mid = other.begin() + my_size;
                    move_range(other.begin(), mid, begin());
                    set_end(uninitialized_move_a(mid, other.end(), end(), get_alloc()));
                }
            }
            other.clear();
        }
    }

    void swap_impl(Array& other, std::true_type) {
        if ((STACK_SIZE == 0) || (is_from_heap() && other.is_from_heap())) {
            std::swap(m_storage.begin, other.m_storage.begin);
            std::swap(m_storage.end, other.m_storage.end);
            std::swap(m_storage.capacity_itr, other.m_storage.capacity_itr);
        } else if (is_from_heap()) {
            iterator other_begin = other.begin();
            iterator other_end = other.end();
            // move this -> other.
            other.set_begin(begin());
            other.set_end(end());
            other.set_capacity_itr(capacity_itr());
            // move other -> this.
            m_storage.reset();
            set_end(uninitialized_move_a(other_begin, other_end, begin(), get_alloc()));
            destroy_a(other_begin, other_end, get_alloc());
        } else if (other.is_from_heap()) {
            iterator this_begin = begin();
            iterator this_end = end();
            // move other -> this.
            set_begin(other.begin());
            set_end(other.end());
            set_capacity_itr(other.capacity_itr());
            // move this -> other.
            other.m_storage.reset();
            other.set_end(
                uninitialized_move_a(this_begin, this_end, other.begin(), other.get_alloc()));
            // need still call destroy after move.
            destroy_a(this_begin, this_end, get_alloc());
        } else {
            size_type my_size = size();
            size_type other_size = other.size();
            size_type min_size = std::min(my_size, other_size);
            // swap shared part.
            for (size_type i = 0; i < min_size; ++i) {
                std::swap(data()[i], other.data()[i]);
            }
            // move exceed part to smaller one.
            if (my_size > other_size) {
                iterator pos = begin() + other_size;
                other.set_end(uninitialized_move_a(pos, end(), other.end(), other.get_alloc()));
                erase_to_end(pos);
            } else if (other_size > my_size) {
                iterator pos = other.begin() + my_size;
                set_end(uninitialized_move_a(pos, other.end(), end(), get_alloc()));
                other.erase_to_end(pos);
            }
        }

        using pocs = typename std::allocator_traits<A>::propagate_on_container_swap;
        if (pocs::value) {
            std::swap(get_alloc(), other.get_alloc());
        }
    }

    void swap_impl(Array& other, std::false_type) {
        if (get_alloc() == other.get_alloc()) {
            swap_impl(other, std::true_type {});
        } else {
            // allocator_traits::propagate_on_container_swap is false_type and
            // allocators not equal, it is undefined.
            // ref: https://en.cppreference.com/w/cpp/container/vector/swap
        }
    }

    bool is_from_heap() const {
        return data() != stack_data();
    }

    void release() {
        destroy_a(begin(), end(), get_alloc());
        if (is_from_heap()) {
            allocator_traits::deallocate(get_alloc(), data(), capacity());
        }
    }

    void erase_to_end(iterator pos) {
        destroy_a(pos, end(), get_alloc());
        set_end(pos);
    }

    allocator_type& get_alloc() noexcept {
        return m_storage;
    }

    const allocator_type& get_alloc() const noexcept {
        return m_storage;
    }

    pointer stack_data() const {
        return m_storage.stack_data();
    }

    iterator capacity_itr() {
        return m_storage.capacity_itr;
    }

    const_iterator capacity_itr() const {
        return m_storage.capacity_itr;
    }

    void set_begin(iterator begin) {
        m_storage.begin = begin;
    }

    void set_end(iterator end) {
        m_storage.end = end;
    }

    void set_capacity_itr(iterator capacity_itr) {
        m_storage.capacity_itr = capacity_itr;
    }

    bool is_push_enough() const {
        return end() != capacity_itr();
    }

    bool is_assign_enough(size_type n) const {
        return static_cast<size_type>(capacity_itr() - begin()) >= n;
    }

    bool is_insert_enough(size_type n) const {
        return static_cast<size_type>(capacity_itr() - end()) >= n;
    }

    size_type check_size(size_type n) const {
        if (n > max_size()) {
            throw std::length_error("Array reach max_size");
        }
        return n;
    }

    /** return new capacity size after grow, throw std::length_error if reach max_size. */
    size_type capacity_after_grow(size_type grow_size) {
        size_type my_size = size();
        size_type max_s = max_size();
        if (grow_size > max_s - my_size) {
            throw std::length_error("Array reach max_size");
        }
        size_type new_capacity = capacity();
        if (new_capacity > max_s - new_capacity) {
            new_capacity = max_s;
        } else {
            new_capacity += new_capacity;
        }
        size_type new_size = my_size + grow_size;
        if (new_capacity < new_size) {
            new_capacity = new_size;
        }
        return new_capacity;
    }

private:
    template <uint32_t N, typename let_gcc_happy>
    struct StackData {
        pointer stack_data() const {
            return reinterpret_cast<pointer>(const_cast<char*>(data));
        }

        alignas(value_type) char data[sizeof(value_type[N])];
    };

    template <typename let_gcc_happy>
    struct StackData<0, let_gcc_happy> {
        pointer stack_data() const {
            return nullptr;
        }
    };

    struct Storage :
            // for empty class optimize.
            StackData<StackSize, void>,
            allocator_type {
        Storage() {
            reset();
        }

        Storage(const allocator_type& alloc) : allocator_type(alloc) {
            reset();
        }

        Storage(allocator_type&& alloc) : allocator_type(std::move(alloc)) {
            reset();
        }

        void reset() {
            begin = this->stack_data();
            end = begin;
            capacity_itr = begin + StackSize;
        }

        iterator begin;
        iterator end;
        iterator capacity_itr;
    };
    Storage m_storage;

    ////////////////////////////////////////////////////////////////////////////////////////////////

    ///
    /// help tools.
    ///

    /** check alloc is std::allocator. */
    template <typename _A>
    struct is_std_allocator : std::false_type {};

    /** check alloc is std::allocator. */
    template <typename _T>
    struct is_std_allocator<std::allocator<_T>> : std::true_type {};

    // tmp heap, raii style, use when need re-allocate(avoid leak).
    struct TmpHeap {
        TmpHeap(Array* parent, size_type capacity) : parent(parent) {
            assert(capacity > StackSize);
            data = allocator_traits::allocate(parent->get_alloc(), capacity);
            begin = data;
            end = begin;
            capacity_itr = begin + capacity;
        }

        ~TmpHeap() {
            if (data) {
                destroy_a(begin, end, parent->get_alloc());
                allocator_traits::deallocate(parent->get_alloc(), data, capacity_itr - data);
            }
        }

        TmpHeap(const TmpHeap&) = delete;
        TmpHeap& operator=(const TmpHeap&) = delete;

        void to_parent() {
            parent->release();
            parent->set_begin(begin);
            parent->set_end(end);
            parent->set_capacity_itr(capacity_itr);
            data = nullptr;
        }

        Array* parent;
        pointer data;
        iterator begin;
        iterator end;
        iterator capacity_itr;
    };

    /** tmp cache value, avoid early move, see emplace/insert. */
    struct TmpValue {
        template <typename... Args>
        TmpValue(Array* parent, Args&&... args) : parent(parent) {
            allocator_traits::construct(parent->get_alloc(), get(), std::forward<Args>(args)...);
        }

        ~TmpValue() {
            allocator_traits::destroy(parent->get_alloc(), get());
        }

        pointer get() {
            return reinterpret_cast<pointer>(data);
        }

        TmpValue(const TmpValue&) = delete;
        TmpValue& operator=(const TmpValue&) = delete;

        Array* parent;
        alignas(value_type) char data[sizeof(value_type)];
    };

    /** same as std::destroy(c++17), but with allocator. */
    template <typename _Itr, typename _A>
    static void destroy_a(_Itr first, _Itr last, _A& alloc) {
        using _value_type = typename std::allocator_traits<_A>::value_type;
        if (std::is_trivially_destructible<_value_type>::value && is_std_allocator<_A>::value) {
            // nothing on trivially_destructible.
        } else {
            for (; first != last; ++first) {
                std::allocator_traits<_A>::destroy(alloc, std::addressof(*first));
            }
        }
    }

    /** same as std::uninitialized_default_construct_n(c++17), but with allocator. */
    template <typename _Itr, typename _Size, typename _A>
    static _Itr uninitialized_default_n_a(_Itr first, _Size n, _A& alloc) {
        using _value_type = typename std::allocator_traits<_A>::value_type;
        if (std::is_trivially_default_constructible<_value_type>::value &&
            is_std_allocator<_A>::value && std::is_standard_layout<_value_type>::value &&
            !std::is_volatile<_value_type>::value) {
            size_t sum = sizeof(*first) * n;
            if (sum > 0) {
                // zero init.
                memset(std::addressof(*first), 0, sum);
            }
            return first + n;
        } else {
            _Itr cur = first;
            try {
                for (; n > 0; --n, ++cur) {
                    std::allocator_traits<_A>::construct(alloc, std::addressof(*cur));
                }
                return cur;
            } catch (...) {
                destroy_a(first, cur, alloc);
                throw;
            }
        }
    }

    /** same as std::uninitialized_fill_n, but with allocator. */
    template <typename _Itr, typename _Size, typename _V, typename _A>
    static _Itr uninitialized_fill_n_a(_Itr first, _Size n, const _V& x, _A& alloc) {
        if (is_std_allocator<_A>::value) {
            if (n == 0) {
                return first;
            }
            // just call std version, it already has optimize.
            return std::uninitialized_fill_n(first, n, x);
        } else {
            _Itr cur = first;
            try {
                for (; n > 0; --n, ++cur) {
                    std::allocator_traits<_A>::construct(alloc, std::addressof(*cur), x);
                }
                return cur;
            } catch (...) {
                destroy_a(first, cur, alloc);
                throw;
            }
        }
    }

    /** same as std::uninitialized_copy, but with allocator. */
    template <typename _InputIterator, typename _OutputIterator, typename _A>
    static _OutputIterator uninitialized_copy_a(
        _InputIterator first, _InputIterator last, _OutputIterator result, _A& alloc) {
        if (is_std_allocator<_A>::value) {
            if (first == last) {
                return result;
            }
            // just call std version, it already has optimize.
            return std::uninitialized_copy(first, last, result);
        } else {
            _OutputIterator cur = result;
            try {
                for (; first != last; ++first, ++cur) {
                    std::allocator_traits<_A>::construct(alloc, std::addressof(*cur), *first);
                }
                return cur;
            } catch (...) {
                destroy_a(result, cur, alloc);
                throw;
            }
        }
    }

    /** same as std::uninitialized_move(c++17), but with allocator. */
    template <typename _InputIterator, typename _OutputIterator, typename _A>
    static _OutputIterator uninitialized_move_a(
        _InputIterator first, _InputIterator last, _OutputIterator result, _A& alloc) {
        return uninitialized_copy_a(
            std::make_move_iterator(first), std::make_move_iterator(last), result, alloc);
    }

    /** same as std::move. */
    template <typename _InputIterator, typename _OutputIterator>
    static _OutputIterator move_range(
        _InputIterator first, _InputIterator last, _OutputIterator result) {
        if (first == last) {
            return result;
        }
        return std::move(first, last, result);
    }

    /** same as std::copy. */
    template <class _InputIterator, class _OutputIterator>
    static _OutputIterator copy_range(
        _InputIterator first, _InputIterator last, _OutputIterator result) {
        if (first == last) {
            return result;
        }
        return std::copy(first, last, result);
    }

    /** same as std::fill. */
    template <typename _ForwardIterator, typename _T>
    static void fill(_ForwardIterator first, _ForwardIterator last, const _T& value) {
        if (first == last) {
            return;
        }
        std::fill(first, last, value);
    }

    /** same as std::fill_n. */
    template <typename _OutputIterator, typename _Size, typename _T>
    static _OutputIterator fill_n(_OutputIterator first, _Size n, const _T& value) {
        if (n == 0) {
            return first;
        }
        return std::fill_n(first, n, value);
    }

    /** return allocator_trait::is_always_equal if exist. */
    template <typename AllocTrait>
    static constexpr auto is_alloc_always_equal(const AllocTrait&)
        -> decltype((typename AllocTrait::is_always_equal())) {
        return typename AllocTrait::is_always_equal();
    }

    /** return std::false_type if allocator_trait::is_always_equal not exist. */
    static constexpr std::false_type is_alloc_always_equal(...) {
        return std::false_type {};
    }
};
}  // namespace xm