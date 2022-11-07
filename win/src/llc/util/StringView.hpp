#pragma once
#include <string>

namespace llc {
namespace util {
class StringView {
public:
    constexpr StringView() : StringView("", 0) {}

    constexpr StringView(const char* data, size_t size) : mBegin(data), mSize(size) {}

    StringView(const char* data) : StringView(data, std::char_traits<char>::length(data)) {}

    StringView(const std::string& data) : StringView(data.c_str(), data.size()) {}

    ~StringView() = default;

    constexpr const char* data() const {
        return mBegin;
    }

    constexpr size_t size() const {
        return mSize;
    }

    constexpr char operator[](size_t index) const {
        return *(mBegin + index);
    }

    constexpr const char* begin() const {
        return mBegin;
    }

    constexpr const char* end() const {
        return mBegin + mSize;
    }

    constexpr char front() const {
        return (*this)[0];
    }

    constexpr char back() const {
        return (*this)[size() - 1];
    }

    constexpr void forward(size_t n) {
        mBegin += n;
        mSize -= n;
    }

    constexpr void backward(size_t n) {
        mSize -= n;
    }

    int compare(StringView rhs) const {
        int r0 = 0;
        size_t minSize = 0;
        if (size() > rhs.size()) {
            minSize = rhs.size();
            r0 = 1;
        } else if (size() < rhs.size()) {
            minSize = size();
            r0 = -1;
        }
        int r = std::char_traits<char>::compare(data(), rhs.data(), minSize);
        if (r) {
            return r;
        } else {
            return r0;
        }
    }

    bool operator==(StringView rhs) const {
        if (size() != rhs.size())
            return false;
        return std::char_traits<char>::compare(data(), rhs.data(), size()) == 0;
    }

    bool operator!=(StringView rhs) const {
        return !(*this == rhs);
    }

    bool operator<(StringView rhs) const {
        return compare(rhs) < 0;
    }

    bool operator<=(StringView rhs) const {
        return compare(rhs) <= 0;
    }

    bool operator>(StringView rhs) const {
        return compare(rhs) > 0;
    }

    bool operator>=(StringView rhs) const {
        return compare(rhs) >= 0;
    }

    template <typename Stream,
              std::enable_if_t<std::is_base_of<std::ostream, Stream>::value, int> = 0>
    friend Stream& operator<<(Stream& s, StringView v) {
        return s.write(v.data(), (std::streamsize)v.size());
    }

private:
    const char* mBegin;
    size_t mSize;
};
}  // namespace util
}  // namespace llc