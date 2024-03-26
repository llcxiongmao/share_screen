#pragma once
#include <xm/PlatformDefine.hpp>
#include <xm/Array.hpp>

#include <cstdarg>
#include <limits>

namespace xm {
/**
 * helper for `vsnprintf`, auto manage size, use `Array<char, StackSize>` as internal data.
 * require: StackSize >= 1, we always keep '\0' at tail.
 *
 * usage:
 * @code
 * BasicStringStream<120> ss;
 * ss.appendFormat("this is int: %d", 1314); // append "this is int: 1314"
 * ss.appendFormat("this is float: %f", 1314.f); // append "this is float: 1314.000"
 * ......
 * @endcode
 */
template <uint32_t StackSize>
class BasicStringStream {
public:
    static_assert(StackSize >= 1, "require: StackSize >= 1");

    using data_type = Array<char, StackSize>;

    BasicStringStream() {
        // keep '\0' at tail.
        *m_data.end() = '\0';
    }

    ~BasicStringStream() = default;

    size_t size() const {
        return m_data.size();
    }

    char* data() {
        return m_data.data();
    }

    const char* data() const {
        return m_data.data();
    }

    void clear() {
        m_data.clear();
        // keep '\0' at tail.
        *m_data.end() = '\0';
    }

    BasicStringStream& append(char v) {
        m_data.emplace_append(1, v);
        // keep '\0' at tail.
        *m_data.end() = '\0';
        return *this;
    }

    BasicStringStream& append(const char* str) {
        size_t str_size = strlen(str);
        if (str_size == 0) {
            return *this;
        }
        m_data.append(str, str + str_size, 1);
        // keep '\0' at tail.
        *m_data.end() = '\0';
        return *this;
    }

    /** append format string, syntax follow `print`. */
    inline BasicStringStream& appendFormat(const char* fmt, ...) XM_PRINTF_FORMAT_CHECK(2, 3);

    /** append format string, syntax follow `print`. */
    BasicStringStream& vappendFormat(const char* fmt, va_list args) {
        va_list args2;
        va_copy(args2, args);
        size_t old_size = m_data.size();
        size_t free_capacity = m_data.capacity() - old_size;
        size_t append_size = vsnprintf(m_data.end(), free_capacity, fmt, args);
        if (m_data.uninitialized_append(append_size, 1)) {
            vsnprintf(m_data.data() + old_size, append_size + 1, fmt, args2);
        }
        va_end(args2);
        return *this;
    }

private:
    data_type m_data;
};

template <uint32_t N>
inline BasicStringStream<N>& BasicStringStream<N>::appendFormat(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vappendFormat(fmt, args);
    va_end(args);
    return *this;
}

using StringStream = BasicStringStream<120>;
}  // namespace xm