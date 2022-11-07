#pragma once
#include <llc/util/StringView.hpp>

namespace llc {
namespace util {
class StringStream {
public:
    StringStream() {}

    ~StringStream() {}

    const std::string& getInternalString() const {
        return mStr;
    }

    std::string& getInternalString() {
        return mStr;
    }

    void clear() {
        mStr.clear();
    }

    template <typename... ARGS>
    void print(const char* fmt, ARGS... args) {
        static const size_t FIRST_LEN = 4;

        size_t pos0 = mStr.size();
        mStr.append(FIRST_LEN, '0');
        size_t size = snprintf(&mStr[pos0], FIRST_LEN, fmt, args...);

        if (size < FIRST_LEN) {
            mStr.resize(mStr.size() - FIRST_LEN + size);
        } else {
            mStr.append(size + 1 - FIRST_LEN, '0');
            snprintf(&mStr[pos0], size + 1, fmt, args...);
            mStr.resize(mStr.size() - 1);
        }
    }

    template <typename... ARGS>
    void print(size_t maxLen, const char* fmt, ARGS... args) {
        maxLen += 1;
        size_t pos0 = mStr.size();
        mStr.append(maxLen, '0');
        size_t size = snprintf(&mStr[pos0], maxLen, fmt, args...);
        mStr.resize(mStr.size() - maxLen + size);
    }

    StringStream& operator<<(StringView s) {
        mStr.append(s.data(), s.size());
        return *this;
    }

    StringStream& operator<<(char v) {
        mStr.push_back(v);
        return *this;
    }

    StringStream& operator<<(short v) {
        static const size_t MAX_LEN = std::numeric_limits<short>::digits10 + 1;
        print(MAX_LEN, "%hd", v);
        return *this;
    }

    StringStream& operator<<(unsigned short v) {
        static const size_t MAX_LEN = std::numeric_limits<unsigned short>::digits10 + 1;
        print(MAX_LEN, "%hu", v);
        return *this;
    }

    StringStream& operator<<(int v) {
        static const size_t MAX_LEN = std::numeric_limits<int>::digits10 + 1;
        print(MAX_LEN, "%d", v);
        return *this;
    }

    StringStream& operator<<(unsigned int v) {
        static const size_t MAX_LEN = std::numeric_limits<unsigned int>::digits10 + 1;
        print(MAX_LEN, "%u", v);
        return *this;
    }

    StringStream& operator<<(long v) {
        static const size_t MAX_LEN = std::numeric_limits<long>::digits10 + 1;
        print(MAX_LEN, "%ld", v);
        return *this;
    }

    StringStream& operator<<(unsigned long v) {
        static const size_t MAX_LEN = std::numeric_limits<unsigned long>::digits10 + 1;
        print(MAX_LEN, "%lu", v);
        return *this;
    }

    StringStream& operator<<(long long v) {
        static const size_t MAX_LEN = std::numeric_limits<long long>::digits10 + 1;
        print(MAX_LEN, "%lld", v);
        return *this;
    }

    StringStream& operator<<(unsigned long long v) {
        static const size_t MAX_LEN = std::numeric_limits<unsigned long long>::digits10 + 1;
        print(MAX_LEN, "%llu", v);
        return *this;
    }

    StringStream& operator<<(float v) {
        print("%f", v);
        return *this;
    }

    StringStream& operator<<(double v) {
        print("%f", v);
        return *this;
    }

    StringStream& operator<<(StringStream& (*p)(StringStream&)) {
        return p(*this);
    }

private:
    std::string mStr;
};

namespace detail {
struct FmtBool {
    bool b = {};

    template <typename Stream>
    friend Stream& operator<<(Stream& stream, const FmtBool& v) {
        if (v.b)
            stream << "true";
        else
            stream << "false";
        return stream;
    }
};
}  // namespace detail

inline detail::FmtBool fmt_bool(bool b) {
    return {b};
}
}  // namespace util
}  // namespace llc