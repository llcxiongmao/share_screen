#pragma once
#include <llc/util/NonCopyable.hpp>
#include <llc/util/Singleton.hpp>
#include <llc/util/StringView.hpp>
#include <llc/util/StringStream.hpp>
#include <llc/util/BlockingQueue.hpp>

#include <vector>
#include <queue>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <cassert>

extern "C" {
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <timeapi.h>

#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/wglext.h>

#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>

#include <uv.h>
}

#include <libavutil/hwcontext_d3d11va.h>

#include <d3d11.h>
#include <wrl.h>
#include <d3dcompiler.h>

using Microsoft::WRL::ComPtr;

#define LLC_DETAIL_STRINGIFY(x) #x
#define LLC_STRINGIFY(x) LLC_DETAIL_STRINGIFY(x)

#define LLC_SRC_LOC __FILE__ "#" LLC_STRINGIFY(__LINE__)

// delete for av
namespace std {
template <>
struct default_delete<AVBufferRef> {
    void operator()(AVBufferRef* p) const {
        av_buffer_unref(&p);
    }
};

template <>
struct default_delete<AVPacket> {
    void operator()(AVPacket* p) const {
        av_packet_free(&p);
    }
};

template <>
struct default_delete<AVCodecParserContext> {
    void operator()(AVCodecParserContext* p) const {
        av_parser_close(p);
    }
};

template <>
struct default_delete<AVCodecContext> {
    void operator()(AVCodecContext* p) const {
        avcodec_free_context(&p);
    }
};

template <>
struct default_delete<AVFrame> {
    void operator()(AVFrame* p) const {
        av_frame_free(&p);
    }
};
}  // namespace std

namespace llc {
namespace share_screen {
namespace log {
/** log tag. */
enum class Tag : uint32_t {
    INFO = 0,
    ERR,
};

//--

/**
 * stream object, auto flash to console when it destroy.
 *
 * use log::i() or log::e() to create stream object. e.g. log::i() << "some log...";
 *
 * @note it is thread safe, but don't create multi object sametime(same thread).
 * e.g.
 * @code
 * auto firstObj = log::i();
 * // bad, because firstObj alive in this thread.
 * auto secondObj = log::i();
 * @endcode
 */
class Stream : public util::NonCopyable {
public:
    static std::mutex& GetMutex() {
        static std::mutex mutex;
        return mutex;
    }

    Stream() {}

    Stream(Tag tag) {
        mData = &GetThreadLocalData();
        assert(!mData->isLogging && "this thread has log not finish");
        mData->isLogging = true;
        mData->tag = tag;
    }

    ~Stream() {
        if (mData) {
            mData->isLogging = false;
            {
                std::lock_guard<std::mutex> lock(GetMutex());
                switch (mData->tag) {
                    case Tag::INFO:
                        std::cout << "[INFO] ";
                        break;
                    case Tag::ERR:
                        std::cout << "[ERR ] ";
                        break;
                    default:
                        break;
                }
                std::cout << mData->strStream.getInternalString();
                std::cout << '\n' << std::flush;
            }
            mData->strStream.clear();
        }
    }

    Stream(Stream&& rhs) noexcept {
        *this = std::move(rhs);
    }

    Stream& operator=(Stream&& rhs) noexcept {
        std::swap(mData, rhs.mData);
        return *this;
    }

    template <typename T>
    Stream& operator<<(const T& v) {
        mData->strStream << v;
        return *this;
    }

private:
    struct ThreadLocalData {
        bool isLogging = false;
        Tag tag = Tag::INFO;
        util::StringStream strStream;
    };

    static ThreadLocalData& GetThreadLocalData() {
        static thread_local ThreadLocalData d;
        return d;
    }

    ThreadLocalData* mData = nullptr;
};

/** create info stream object. */
inline Stream i() {
    return Stream(Tag::INFO);
}

/** create error stream object. */
inline Stream e() {
    return Stream(Tag::ERR);
}

/** output av log, we need sync, because std::cout shared by multi-thread. */
inline void av_log_callback(void* avcl, int level, const char* fmt, va_list vl) {
    std::lock_guard<std::mutex> lock(Stream::GetMutex());
    av_log_default_callback(avcl, level, fmt, vl);
    std::cout << std::flush;
}
}  // namespace log
}  // namespace share_screen
}  // namespace llc

//--

namespace llc {
namespace share_screen {
/** error summary. */
class Error : public std::exception {
public:
    /**
     * get win32 error string.
     *
     * @param err GetLastError() or anyothers.
     * @return return error string.
     */
    static std::string GetWin32String(DWORD err) {
        if (err == 0)
            return "";

        LPSTR buf = nullptr;
        DWORD size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                                        FORMAT_MESSAGE_IGNORE_INSERTS,
                                    NULL,
                                    err,
                                    MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
                                    (LPSTR)&buf,
                                    0,
                                    nullptr);
        std::string str;
        if (size) {
            if (buf[size - 1] == '\n' || buf[size - 1] == '\r')
                buf[--size] = '\0';
        }
        if (size)
            str.assign(buf, size);
        else
            str = "unknown";

        if (buf)
            LocalFree(buf);
        return str;
    }

    /** call as GetWin32String(GetLastError()). */
    static std::string GetWin32String() {
        return GetWin32String(GetLastError());
    }

    /**
     * get avcodec error string.
     *
     * @param err avcodec fn return value.
     * @return return error string.
     */
    static std::string GetAvString(int err) {
        char str[AV_ERROR_MAX_STRING_SIZE] = {};
        av_make_error_string(str, AV_ERROR_MAX_STRING_SIZE, err);
        return str;
    }

    /**
     * get uv error string.
     *
     * @param err uv fn return value.
     * @return return error string.
     */
    static std::string GetUvString(int err) {
        char str[32] = {};
        return uv_strerror_r(err, str, 32);
    }

    /** constructor. */
    Error() {}

    /**
     * constructor.
     *
     * @param desc message.
     */
    Error(util::StringStream desc) : mDesc(std::move(desc)) {}

    /**
     * constructor.
     *
     * @param desc message.
     * @param throwLoc throw location, @see LLC_SRC_LOC.
     */
    Error(util::StringStream desc, util::StringView throwLoc) : mDesc(std::move(desc)) {
        mDesc << ", throw location: " << throwLoc;
    }

    const char* what() const override {
        return mDesc.getInternalString().c_str();
    }

    template <typename T>
    Error& operator<<(const T& v) {
        mDesc << v;
        return *this;
    }

private:
    util::StringStream mDesc;
};

//--

/** paint frame, contain pts and decode frame. */
struct PaintFrame : util::NonCopyable {
    int64_t pts = 0;
    std::unique_ptr<AVFrame> decodeFrame;

    PaintFrame() {
        decodeFrame.reset(av_frame_alloc());
    }
};

//--

/** net frame, contain pts and body packet. */
struct NetFrame : util::NonCopyable {
    bool recycling = false;
    uv_async_t* pAsync = {};
    int64_t pts = 0;
    std::unique_ptr<AVPacket> body;

    NetFrame(uv_async_t* pAsync) : pAsync(pAsync) {
        pAsync->data = this;
        body.reset(av_packet_alloc());
    }
};

//--

/** app config. */
struct Config : util::Singleton<Config> {
    /** direct connect ip instead of connect by broadcast. */
    std::string ip;
    /** connect port. */
    int port = 1314;
    /** broadcast port. */
    int broadcastPort = 1413;
    /** immediately paint frame instead of sync with pts. */
    bool immediatelyPaint = false;
    /** disable hardware accel, force use software decode. */
    bool disableHwaccel = false;
    /** disable high precision time. */
    bool disableHighPrecisionTime = false;
    /** force use gl render, also disable hardwrae accel. */
    bool useGlRender = false;
    /** print net info. */
    bool debugPrintNet = false;
    /** print pts info. */
    bool debugPrintPts = false;
    /** print decode info. */
    bool debugPrintDecode = false;
};
}  // namespace share_screen
}  // namespace llc

namespace llc {
namespace share_screen {
namespace detail {
/** extract name, e.g. string "some_fn(arg0, arg1)" return "some_fn". */
constexpr util::StringView extract_fn_name(const char* str) {
    for (size_t i = 0;; ++i) {
        if (str[i] == '(' || str[i] == '\0')
            return util::StringView(str, i);
    }
    return {};
}
}  // namespace detail
}  // namespace share_screen
}  // namespace llc

#define THROW_IF(_B, ...)                                              \
    if (!(_B)) {                                                       \
        throw Error(util::StringStream() << __VA_ARGS__, LLC_SRC_LOC); \
    }

#define THROW_IF_AV(...)                                          \
    {                                                             \
        int _avRes = __VA_ARGS__;                                 \
        THROW_IF(!_avRes,                                         \
                 detail::extract_fn_name(#__VA_ARGS__)            \
                     << " fail: " << Error::GetAvString(_avRes)); \
    }

#define THROW_IF_UV(...)                                          \
    {                                                             \
        int _uvRes = __VA_ARGS__;                                 \
        THROW_IF(!_uvRes,                                         \
                 detail::extract_fn_name(#__VA_ARGS__)            \
                     << " fail: " << Error::GetUvString(_uvRes)); \
    }

#define THROW_IF_DX11(...)                                        \
    {                                                             \
        HRESULT _hr = __VA_ARGS__;                                \
        THROW_IF(_hr == S_OK,                                     \
                 detail::extract_fn_name(#__VA_ARGS__)            \
                     << " fail: " << Error::GetWin32String(_hr)); \
    }

#define LLC_SHARE_SCREEN_VERSION_MAJOR 1
#define LLC_SHARE_SCREEN_VERSION_MINOR 0
#define LLC_SHARE_SCREEN_VERSION_PATCH 0

// clang-format off
#define LLC_SHARE_SCREEN_VERSION_NAME \
    LLC_STRINGIFY(LLC_SHARE_SCREEN_VERSION_MAJOR) "." \
    LLC_STRINGIFY(LLC_SHARE_SCREEN_VERSION_MINOR) "." \
    LLC_STRINGIFY(LLC_SHARE_SCREEN_VERSION_PATCH)
// clang-format on

#ifdef NDEBUG
#define LLC_SHARE_SCREEN_BUILD_TYPE "Release"
#else
#define LLC_SHARE_SCREEN_BUILD_TYPE "Debug"
#endif