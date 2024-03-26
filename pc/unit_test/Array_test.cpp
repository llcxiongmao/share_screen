#include <xm/Array.hpp>

#include "Common.hpp"

#include <array>
#include <vector>
#include <iostream>
#include <sstream>
#include <iterator>

template <typename T>
struct Alloc0 : std::allocator<T> {
    using is_always_equal = std::false_type;

    template <typename U>
    struct rebind {
        typedef Alloc0<U> other;
    };

    Alloc0() = default;

    template <typename U>
    bool operator==(const Alloc0<U>& rhs) {
        return true;
    }

    template <typename U>
    bool operator!=(const Alloc0<U>& rhs) {
        return false;
    }
};

template <typename T>
struct Alloc1 : std::allocator<T> {
    using is_always_equal = std::false_type;
    using propagate_on_container_move_assignment = std::false_type;

    template <typename U>
    struct rebind {
        typedef Alloc1<U> other;
    };

    Alloc1() = default;

    template <typename U>
    bool operator==(const Alloc1<U>& rhs) {
        return false;
    }

    template <typename U>
    bool operator!=(const Alloc1<U>& rhs) {
        return true;
    }
};

struct T0 {
    using ele_type = int;
    using alloc_type = std::allocator<ele_type>;
    static constexpr uint32_t stack_size() {
        return 0;
    }
};

struct T1 {
    using ele_type = int;
    using alloc_type = std::allocator<ele_type>;
    static constexpr uint32_t stack_size() {
        return 6;
    }
};

struct T2 {
    using ele_type = xm::Array<int>;
    using alloc_type = std::allocator<ele_type>;
    static constexpr uint32_t stack_size() {
        return 0;
    }
};

struct T3 {
    using ele_type = xm::Array<int>;
    using alloc_type = std::allocator<ele_type>;
    static constexpr uint32_t stack_size() {
        return 6;
    }
};

struct T4 {
    using ele_type = xm::Array<int>;
    using alloc_type = Alloc0<ele_type>;
    static constexpr uint32_t stack_size() {
        return 0;
    }
};

struct T5 {
    using ele_type = xm::Array<int>;
    using alloc_type = Alloc1<ele_type>;
    static constexpr uint32_t stack_size() {
        return 6;
    }
};

template <typename T>
struct ArrayTest : testing::Test {};

using TestTypes = ::testing::Types<T0, T1, T2, T3, T4, T5>;

TYPED_TEST_SUITE(ArrayTest, TestTypes);

#define ARRAY_TEST(name) TYPED_TEST(ArrayTest, name)

ARRAY_TEST(constructor) {
    using ele_type = typename TypeParam::ele_type;
    using alloc_type = typename TypeParam::alloc_type;
    using TestVec = xm::Array<ele_type, TypeParam::stack_size(), alloc_type>;
    using StdTestVec = std::vector<ele_type>;

    {
        TestVec v;
        E_EQ(v.size(), 0);
        E_EQ(v.capacity(), TestVec::STACK_SIZE);
    }

    // construct(n)
    FOR_I(20) {
        StdTestVec std_v(i);
        TestVec v(i);
        E_THAT(v, testing::ElementsAreArray(std_v));
    }

    // construct(n, v)
    FOR_I(20) {
        StdTestVec std_v(i, ele_type(i));
        TestVec v(i, ele_type(i));
        E_THAT(v, testing::ElementsAreArray(std_v));
    }

    // construct(copy)
    // construct(first, last)
    // construct(std::initializer_list)
    FOR_I(20) {
        StdTestVec std_v(i, ele_type(i));
        TestVec v(std_v.begin(), std_v.end());
        E_THAT(v, testing::ElementsAreArray(std_v));

        TestVec v1 = v;
        E_THAT(v, testing::ElementsAreArray(std_v));
    }
    {
        auto init_list = {ele_type(0), ele_type(1), ele_type(2), ele_type(3)};
        TestVec v = init_list;
        E_THAT(v, testing::ElementsAreArray(init_list));
    }

    // construct(move)
    FOR_I(20) {
        StdTestVec std_v(i, ele_type(i));
        TestVec v0(std_v.begin(), std_v.end());
        TestVec v1(std::move(v0));
        E_EQ(v0.size(), 0);
        E_THAT(v1, testing::ElementsAreArray(std_v));

        alloc_type tmp_alloc;
        TestVec v2(std::move(v1), tmp_alloc);
        E_EQ(v1.size(), 0);
        E_THAT(v2, testing::ElementsAreArray(std_v));
    }
}

ARRAY_TEST(assign) {
    using ele_type = typename TypeParam::ele_type;
    using alloc_type = typename TypeParam::alloc_type;
    using TestVec = xm::Array<ele_type, TypeParam::stack_size(), alloc_type>;
    using StdTestVec = std::vector<ele_type>;

    // assign(n, v)
    FOR_I(20) {
        StdTestVec std_v(i, ele_type(i));
        TestVec v(i, ele_type(i));
        FOR_J(i + 10) {
            std_v.assign(j, ele_type(j));
            v.assign(j, ele_type(j));
            E_THAT(v, testing::ElementsAreArray(std_v));
        }
    }

    // =(copy)
    // =(std::initializer_list)
    // assign(first, last)
    FOR_I(20) {
        StdTestVec std_v(i, ele_type(i));
        TestVec v(i, ele_type(i));
        FOR_J(i + 10) {
            StdTestVec std_v1(j, ele_type(j));
            TestVec v1(j, ele_type(j));
            std_v1 = std_v;
            v1 = v;
            E_THAT(v1, testing::ElementsAreArray(std_v1));
        }
    }

    // =(move)
    FOR_I(20) {
        StdTestVec std_v(i, ele_type(i));
        TestVec v(i, ele_type(i));
        FOR_J(i + 10) {
            StdTestVec std_v1(j, ele_type(j));
            TestVec v1(j, ele_type(j));
            std_v1 = std::move(std_v);
            v1 = std::move(v);
            E_EQ(v.size(), 0);
            E_THAT(v1, testing::ElementsAreArray(std_v1));
        }
    }
}

ARRAY_TEST(capacity) {
    using ele_type = typename TypeParam::ele_type;
    using alloc_type = typename TypeParam::alloc_type;
    using TestVec = xm::Array<ele_type, TypeParam::stack_size(), alloc_type>;
    using StdTestVec = std::vector<ele_type>;

    // reserve
    FOR_I(20) {
        FOR_J(i + 10) {
            StdTestVec std_v(i, ele_type(i));
            TestVec v(i, ele_type(i));
            std_v.reserve(j);
            v.reserve(j);
            E_THAT(v, testing::ElementsAreArray(std_v));
        }
    }

    // shrink_to_fit
    FOR_I(20) {
        StdTestVec std_v(i, ele_type(i));
        TestVec v(i, ele_type(i));
        v.reserve(i + 3);
        v.shrink_to_fit();
        E_THAT(v, testing::ElementsAreArray(std_v));
        if (v.size() > TestVec::STACK_SIZE) {
            E_EQ(v.capacity(), v.size());
        } else {
            E_EQ(v.capacity(), TestVec::STACK_SIZE);
        }
    }
}

ARRAY_TEST(modifier) {
    using ele_type = typename TypeParam::ele_type;
    using alloc_type = typename TypeParam::alloc_type;
    using TestVec = xm::Array<ele_type, TypeParam::stack_size(), alloc_type>;
    using StdTestVec = std::vector<ele_type>;

    // clear
    FOR_I(20) {
        TestVec v(i, ele_type(i));
        v.clear();
        E_EQ(v.size(), 0);
    }

    // push_back
    // emplace_back
    {
        StdTestVec std_v;
        TestVec v;
        FOR_I(20) {
            std_v.emplace_back(i);
            std_v.emplace_back(std_v.back());
            std_v.emplace_back(std_v.front());
            v.emplace_back(i);
            v.emplace_back(v.back());
            v.emplace_back(v.front());
            E_THAT(v, testing::ElementsAreArray(std_v));
        }
    }

    // pop_back
    FOR_I(20) {
        StdTestVec std_v(i + 1);
        TestVec v(i + 1);
        size_t capacity = v.capacity();
        std_v.pop_back();
        v.pop_back();
        E_EQ(capacity, v.capacity());
        E_THAT(v, testing::ElementsAreArray(std_v));
    }

    // insert(pos, v)
    // emplace(pos, v)
    FOR_I(20) {
        FOR_J(i + 1) {
            StdTestVec std_v(i, ele_type(i));
            TestVec v(i, ele_type(i));
            auto std_itr = std_v.emplace(std_v.begin() + j, j);
            auto v_itr = v.emplace(v.begin() + j, j);
            E_EQ(std_itr - std_v.begin(), v_itr - v.begin());
            E_THAT(v, testing::ElementsAreArray(std_v));
        }
    }
    FOR_I(20) {
        if (i == 0) {
            continue;
        }
        FOR_J(i + 1) {
            StdTestVec std_v(i, ele_type(i));
            TestVec v(i, ele_type(i));
            auto std_itr = std_v.emplace(std_v.begin() + j, std_v.back());
            auto v_itr = v.emplace(v.begin() + j, v.back());
            E_EQ(std_itr - std_v.begin(), v_itr - v.begin());
            E_THAT(v, testing::ElementsAreArray(std_v));
        }
    }

    // insert(pos, n, v)
    FOR_I(20) {
        FOR_J(i + 1) {
            StdTestVec std_v(i, ele_type(i));
            TestVec v(i, ele_type(i));
            auto std_itr = std_v.insert(std_v.begin() + j, j, ele_type(i));
            auto v_itr = v.insert(v.begin() + j, j, ele_type(i));
            E_EQ(std_itr - std_v.begin(), v_itr - v.begin());
            E_THAT(v, testing::ElementsAreArray(std_v));
        }
    }
    FOR_I(20) {
        if (i == 0) {
            continue;
        }
        FOR_J(i + 1) {
            StdTestVec std_v(i, ele_type(i));
            TestVec v(i, ele_type(i));
            auto std_itr = std_v.insert(std_v.begin() + j, j, std_v.back());
            auto v_itr = v.insert(v.begin() + j, j, v.back());
            E_EQ(std_itr - std_v.begin(), v_itr - v.begin());
            E_THAT(v, testing::ElementsAreArray(std_v));
        }
    }

    // insert(pos, std::initializer_list)
    // insert(pos, first, last)
    StdTestVec tmp_v(10, ele_type(10));
    FOR_I(20) {
        FOR_J(i + 1) {
            StdTestVec std_v(i, ele_type(i));
            TestVec v(i, ele_type(i));
            v.reserve(i * 2);
            auto std_itr = std_v.insert(std_v.begin() + j, tmp_v.begin(), tmp_v.end());
            auto v_itr = v.insert(v.begin() + j, tmp_v.begin(), tmp_v.end());
            E_EQ(std_itr - std_v.begin(), v_itr - v.begin());
            E_THAT(v, testing::ElementsAreArray(std_v));
        }
    }

    // erase(pos)
    FOR_I(20) {
        FOR_J(i) {
            StdTestVec std_v(i, ele_type(i));
            TestVec v(i, ele_type(i));
            auto std_itr = std_v.erase(std_v.begin() + j);
            auto v_itr = v.erase(v.begin() + j);
            E_EQ(std_itr - std_v.begin(), v_itr - v.begin());
            E_THAT(v, testing::ElementsAreArray(std_v));
        }
    }

    // erase(first, last)
    FOR_I(20) {
        FOR_J(i + 1) {
            StdTestVec std_v(i, ele_type(i));
            TestVec v(i, ele_type(i));
            auto std_itr = std_v.erase(std_v.begin(), std_v.begin() + j);
            auto v_itr = v.erase(v.begin(), v.begin() + j);
            E_EQ(std_itr - std_v.begin(), v_itr - v.begin());
            E_THAT(v, testing::ElementsAreArray(std_v));
        }
    }

    // reisze(n)
    FOR_I(20) {
        FOR_J(i + 10) {
            StdTestVec std_v(i, ele_type(i));
            TestVec v(i, ele_type(i));
            std_v.resize(j);
            v.resize(j);
            E_THAT(v, testing::ElementsAreArray(std_v));
        }
    }

    // reisze(n, v)
    FOR_I(20) {
        FOR_J(i + 10) {
            StdTestVec std_v(i, ele_type(i));
            TestVec v(i, ele_type(i));
            std_v.resize(j, ele_type(j));
            v.resize(j, ele_type(j));
            E_THAT(v, testing::ElementsAreArray(std_v));
        }
    }

    // swap
    FOR_I(20) {
        FOR_J(i + 10) {
            StdTestVec std_v(i, ele_type(i));
            StdTestVec std_v1(j, ele_type(j));
            std_v.swap(std_v1);
            TestVec v(i, ele_type(i));
            TestVec v1(j, ele_type(j));
            v.swap(v1);
            if (!TestVec::allocator_traits::propagate_on_container_swap::value &&
                v.get_allocator() != v1.get_allocator()) {
                continue;
            }
            E_THAT(v, testing::ElementsAreArray(std_v));
            E_THAT(v1, testing::ElementsAreArray(std_v1));
        }
    }
}

ARRAY_TEST(input_itr) {
    using ele_type = int;
    using alloc_type = std::allocator<int>;
    using TestVec = xm::Array<ele_type, 6, alloc_type>;
    using StdTestVec = std::vector<ele_type>;

    FOR_I(20) {
        std::stringstream str0;
        FOR_J(i + 1) {
            str0 << j << " ";
        }
        StdTestVec std_v(std::istream_iterator<int>{str0}, std::istream_iterator<int>{});
        std::stringstream str1;
        FOR_J(i + 1) {
            str1 << j << " ";
        }
        TestVec v(std::istream_iterator<int>{str1}, std::istream_iterator<int>{});
        E_THAT(v, testing::ElementsAreArray(std_v));
    }

    FOR_I(20) {
        std::stringstream str0;
        FOR_K(5) {
            str0 << k << " ";
        }
        std::stringstream str1;
        FOR_K(5) {
            str1 << k << " ";
        }
        StdTestVec std_v(i, i);
        TestVec v(i, i);
        std_v.assign(std::istream_iterator<int>{str0}, std::istream_iterator<int>{});
        v.assign(std::istream_iterator<int>{str1}, std::istream_iterator<int>{});
        E_THAT(v, testing::ElementsAreArray(std_v));
    }

    FOR_I(20) {
        FOR_J(i + 1) {
            StdTestVec std_v(i, i);
            TestVec v(i, i);
            std::stringstream str0;
            FOR_K(j + 1) {
                str0 << k << " ";
            }
            std::stringstream str1;
            FOR_K(j + 1) {
                str1 << k << " ";
            }
            auto std_itr = std_v.insert(
                std_v.begin() + j, std::istream_iterator<int>{str0}, std::istream_iterator<int>{});
            auto v_itr = v.insert(
                v.begin() + j, std::istream_iterator<int>{str1}, std::istream_iterator<int>{});
            E_EQ(std_itr - std_v.begin(), v_itr - v.begin());
            E_THAT(v, testing::ElementsAreArray(std_v));
        }
    }
}

ARRAY_TEST(non_standard) {
    using ele_type = typename TypeParam::ele_type;
    using alloc_type = typename TypeParam::alloc_type;
    using TestVec = xm::Array<ele_type, TypeParam::stack_size(), alloc_type>;
    using StdTestVec = std::vector<ele_type>;

    {
        StdTestVec std_v;
        TestVec v;
        FOR_I(20) {
            std_v.push_back(ele_type(i));
            v.emplace_append(1, ele_type(i));
            E_THAT(v, testing::ElementsAreArray(std_v));
            E_GE(v.capacity() - v.size(), 1);
        }
    }

    {
        StdTestVec std_v;
        TestVec v;
        FOR_I(20) {
            std_v.insert(std_v.end(), i, ele_type{});
            v.default_append(i, 5);
            E_THAT(v, testing::ElementsAreArray(std_v));
            E_GE(v.capacity() - v.size(), 5);
        }
    }

    {
        StdTestVec std_v;
        TestVec v;
        FOR_I(20) {
            std_v.insert(std_v.end(), i, ele_type(2));
            v.append(i, ele_type(2), 5);
            E_THAT(v, testing::ElementsAreArray(std_v));
            E_GE(v.capacity() - v.size(), 5);
        }
    }

    {
        FOR_I(10) {
            StdTestVec tmp_v(i, ele_type(i));
            StdTestVec std_v;
            TestVec v;
            FOR_J(10) {
                std_v.insert(std_v.end(), tmp_v.begin(), tmp_v.end());
                v.append(tmp_v.begin(), tmp_v.end(), 5);
                E_THAT(v, testing::ElementsAreArray(std_v));
                E_GE(v.capacity() - v.size(), 5);
            }
        }
    }
}

ARRAY_TEST(uninitialized_append) {
    using ele_type = int;
    using alloc_type = std::allocator<int>;
    using TestVec = xm::Array<ele_type, 6, alloc_type>;

    size_t sum = 0;
    TestVec v;
    FOR_I(20) {
        v.uninitialized_append(i, 5);
        sum += i;
        E_EQ(v.size(), sum);
        E_GE(v.capacity() - v.size(), 5);
    }
}

ARRAY_TEST(op) {
    using ele_type = typename TypeParam::ele_type;
    using alloc_type = typename TypeParam::alloc_type;
    using TestVec = xm::Array<ele_type, TypeParam::stack_size(), alloc_type>;

    TestVec a, b;
    E_FALSE(a < b);
    E_FALSE(b < a);
    E_FALSE(a > b);
    E_FALSE(b > a);
    E_TRUE(a <= b);
    E_TRUE(b <= a);
    E_TRUE(a >= b);
    E_TRUE(b >= a);
    b.push_back(ele_type(13));
    E_TRUE(a < b);
    E_FALSE(b < a);
    E_FALSE(a > b);
    E_TRUE(b > a);
    E_TRUE(a <= b);
    E_FALSE(b <= a);
    E_FALSE(a >= b);
    E_TRUE(b >= a);
}
