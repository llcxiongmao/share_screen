#include <xm/StringStream.hpp>

#include "Common.hpp"

struct C1 {
    static constexpr uint32_t stack_size() {
        return 1;
    }
};

struct C10 {
    static constexpr uint32_t stack_size() {
        return 10;
    }
};

template <typename T>
struct StringStreamTest : testing::Test {};

using TestTypes = ::testing::Types<C1, C10>;

TYPED_TEST_SUITE(StringStreamTest, TestTypes);

#define STRING_STREAM_TEST(name) TYPED_TEST(StringStreamTest, name)

constexpr char str[] = "0123456789abcdefghiegk";

STRING_STREAM_TEST(all) {
    using SS = xm::BasicStringStream<TypeParam::stack_size()>;

    {
        std::stringstream std_s;
        SS s;
        FOR_I(20) {
            std_s << str[i];
            auto tmp_str = std_s.str();
            s.append(str[i]);
            E_EQ(s.size(), tmp_str.size());
            E_STREQ(s.data(), tmp_str.c_str());
        }
    }

    {
        std::stringstream std_s;
        SS s;
        FOR_I(20) {
            std_s << str + i;
            auto tmp_str = std_s.str();
            s.append(str + i);
            E_EQ(s.size(), tmp_str.size());
            E_STREQ(s.data(), tmp_str.c_str());
        }
    }

    {
        std::stringstream std_s;
        SS s;
        FOR_I(20) {
            std_s << str + i;
            auto tmp_str = std_s.str();
            s.appendFormat("%s", str + i);
            E_EQ(s.size(), tmp_str.size());
            E_STREQ(s.data(), tmp_str.c_str());
        }
    }

    {
        FOR_I(20) {
            SS s;
            s.appendFormat("%s", str + i);
            s.clear();
            E_EQ(s.size(), 0);
            E_STREQ(s.data(), "");
        }
    }
}
