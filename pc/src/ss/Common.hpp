#pragma once
#include <xm/PlatformDefine.hpp>
// impl for: windows/linux/mac.
#if defined(XM_OS_WINDOWS) || defined(XM_OS_LINUX) || defined(XM_OS_MAC)
#elif
    #error "not impl"
#endif

#include <xm/NonCopyable.hpp>
#include <xm/Array.hpp>
#include <xm/StringStream.hpp>
#include <xm/OtherTool.hpp>
#include <xm/SingletonBase.hpp>

#include <ss/BlockingQueue.hpp>

#include <cstdint>
#include <cstddef>
#include <initializer_list>
#include <mutex>
#include <thread>
#include <memory>
#include <string>
#include <optional>

#include <uv.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>
}

#if defined(XM_OS_LINUX)
    #include <unistd.h>
    #include <X11/Xlib.h>
    #include <poll.h>
    #include <sys/eventfd.h>
    #include <glad/glx.h>
#endif

#if defined(XM_OS_WINDOWS)
    #include <crtdbg.h>
    #include <glad/wgl.h>
    #pragma comment(lib, "opengl32.lib")
#endif

#if defined(XM_OS_MAC)
    #include <glad/gl.h>
#endif

#define SS_MACRO_EXPAND(x) #x
#define SS_STRINGIZING(x) SS_MACRO_EXPAND(x)
#define SS_BUILD_TYPE_NAME SS_STRINGIZING(_SS_BUILD_TYPE_NAME)
#define SS_VERSION_MAJOR _SS_VERSION_MAJOR
#define SS_VERSION_MINOR _SS_VERSION_MINOR
#define SS_VERSION_PATCH _SS_VERSION_PATCH
#define SS_VERSION_CODE _SS_VERSION_CODE

/**
 * throw if CHECK_TRUE == false.
 * usage:
 * SS_THROW(0, "message description, like printf, int: %d", 1314).
 */
#define SS_THROW(CHECK_TRUE, ...)                                                        \
    do {                                                                                 \
        if (!(CHECK_TRUE)) {                                                             \
            xm::StringStream _abcd_ss;                                                   \
            _abcd_ss.appendFormat(__VA_ARGS__);                                          \
            ss::Error _abcd_e(_abcd_ss.data());                                          \
            _abcd_e.callstackCount =                                                     \
                xm::OtherTool::Backtrace(_abcd_e.callstacks, ss::Error::MAX_CALL_STACK); \
            throw _abcd_e;                                                               \
        }                                                                                \
    } while (0)

namespace chrono = std::chrono;

namespace ss {
struct Error : std::runtime_error {
    enum {
        MAX_CALL_STACK = 15,
    };

    Error(const char* msg) : std::runtime_error(msg) {}

    uint32_t callstackCount = 0;
    void* callstacks[MAX_CALL_STACK] = {};
};

////////////////////////////////////////////////////////////////////////////////

enum class LogTag : uint32_t {
    INFO = 0,
    WARNING,
    ERR,
};

class Log : public xm::SingletonBase<Log> {
public:
    static void I_STR(const char* str) {
        Singleton()->write(LogTag::INFO, str);
    }

    static void W_STR(const char* str) {
        Singleton()->write(LogTag::WARNING, str);
    }

    static void E_STR(const char* str) {
        Singleton()->write(LogTag::ERR, str);
    }

    template <uint32_t N>
    static void I(const xm::BasicStringStream<N>& ss) {
        I_STR(ss.data());
    }

    template <uint32_t N>
    static void W(const xm::BasicStringStream<N>& ss) {
        W_STR(ss.data());
    }

    template <uint32_t N>
    static void E(const xm::BasicStringStream<N>& ss) {
        E_STR(ss.data());
    }

    static inline void I(const char* fmt, ...) XM_PRINTF_FORMAT_CHECK(1, 2);

    static inline void W(const char* fmt, ...) XM_PRINTF_FORMAT_CHECK(1, 2);

    static inline void E(const char* fmt, ...) XM_PRINTF_FORMAT_CHECK(1, 2);

    static void PrintError(const Error& e) {
        xm::StringStream ss;
        ss.appendFormat("catch ss::Error: %s", e.what());
        for (uint32_t i = 0; i < e.callstackCount; ++i) {
            ss.appendFormat(
                "\n    %u# %s [%llx]",
                i,
                xm::OtherTool::CallstackToString(e.callstacks[i]).c_str(),
                (unsigned long long)e.callstacks[i]);
        }
        E(ss);
    }

    /** av log callback, we need sync, because stdout shared by multi-thread. */
    static void AvLogCallback(void* avcl, int level, const char* fmt, va_list vl) {
        std::lock_guard<std::mutex> lock(Singleton()->mMutex);
        av_log_default_callback(avcl, level, fmt, vl);
        ::fflush(stdout);
    }

    Log() {
        I("share screen, version: %d.%d.%d, code: %d, build type: %s",
          SS_VERSION_MAJOR,
          SS_VERSION_MINOR,
          SS_VERSION_PATCH,
          SS_VERSION_CODE,
          SS_BUILD_TYPE_NAME);
    }

    ~Log() {}

    void write(LogTag tag, const char* str) {
        std::lock_guard<std::mutex> lock(mMutex);
        switch (tag) {
            case LogTag::INFO:
                ::printf("[I] %s\n", str);
                ::fflush(stdout);
                break;
            case LogTag::WARNING:
                ::printf("[W] %s\n", str);
                ::fflush(stdout);
                break;
            case LogTag::ERR:
                ::printf("[E] %s\n", str);
                ::fflush(stdout);
                break;
            default:
                break;
        }
    }

private:
    std::mutex mMutex;
};

inline void Log::I(const char* fmt, ...) {
    xm::StringStream ss;
    va_list args;
    va_start(args, fmt);
    ss.vappendFormat(fmt, args);
    va_end(args);
    I(ss);
}

inline void Log::W(const char* fmt, ...) {
    xm::StringStream ss;
    va_list args;
    va_start(args, fmt);
    ss.vappendFormat(fmt, args);
    va_end(args);
    W(ss);
}

inline void Log::E(const char* fmt, ...) {
    xm::StringStream ss;
    va_list args;
    va_start(args, fmt);
    ss.vappendFormat(fmt, args);
    va_end(args);
    E(ss);
}

////////////////////////////////////////////////////////////////////////////////

/** libavcodec error to string. */
inline std::string averror_tostring(int err) {
    char str[AV_ERROR_MAX_STRING_SIZE] = {};
    av_make_error_string(str, AV_ERROR_MAX_STRING_SIZE, err);
    return str;
}

/** libuv error to string. */
inline std::string uverror_tostring(int err) {
    char str[256] = {};
    return uv_strerror_r(err, str, 255);
}

////////////////////////////////////////////////////////////////////////////////

/** throw if res != 0, use for check libuv result. */
XM_FORCE_INLINE void check_libuv(int res, const char* api_name) {
    SS_THROW(res == 0, "'%s' fail: %s", api_name, ss::uverror_tostring(res).c_str());
}

/** throw if res != 0, use for check libav result. */
XM_FORCE_INLINE void check_libav(int res, const char* api_name) {
    SS_THROW(res == 0, "'%s' fail: %s", api_name, ss::averror_tostring(res).c_str());
}

#if defined(XM_OS_WINDOWS)
/** throw if check_true == false, use for check windows call(GetLastError as message description).
 */
XM_FORCE_INLINE void check_lasterror(bool check_true, const char* api_name) {
    if (!check_true) {
        DWORD code = ::GetLastError();
        SS_THROW(0, "'%s' fail: %s", api_name, xm::OtherTool::LastErrorToString(code).c_str());
    }
}
#endif

#if defined(XM_OS_LINUX)
/** throw if check_true == false, use for check linux call(errno as message description). */
XM_FORCE_INLINE void check_errno(bool check_true, const char* api_name) {
    if (!check_true) {
        int code = errno;
        char str[256] = {};
        strerror_r(code, str, 255);
        SS_THROW(0, "'%s' fail: %s", api_name, str);
    }
}
#endif

////////////////////////////////////////////////////////////////////////////////

struct PaintFrame : xm::NonCopyable {
    PaintFrame() {
        decodeFrame = av_frame_alloc();
        SS_THROW(decodeFrame, "'av_frame_alloc' fail");
    }

    ~PaintFrame() {
        av_frame_free(&decodeFrame);
    }

    int64_t pts = 0;
    AVFrame* decodeFrame;
};

struct NetFrame : xm::NonCopyable {
    NetFrame() {
        body = av_packet_alloc();
        SS_THROW(body, "'av_packet_alloc' fail");
    }

    ~NetFrame() {
        av_packet_free(&body);
    }

    int64_t pts = 0;
    AVPacket* body;
};

////////////////////////////////////////////////////////////////////////////////

/** app config. */
struct Config : xm::SingletonBase<Config> {
    /** direct connect ip instead of connect by broadcast. */
    std::string ip;
    /** connect port. */
    int port = 1314;
    /** broadcast port. */
    int broadcastPort = 1413;
    /** immediately paint frame instead of sync with pts. */
    bool immediatelyPaint = false;
    /** print net info. */
    bool debugNet = false;
    /** print pts info. */
    bool debugPts = false;
    /** print decode info. */
    bool debugDecode = false;
};
}  // namespace ss
