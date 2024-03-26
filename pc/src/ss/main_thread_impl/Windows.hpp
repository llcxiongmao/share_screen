#pragma once
#include <ss/Common.hpp>

#if defined(XM_OS_WINDOWS)
namespace ss::detail {
class MainThreadImpl : public xm::SingletonBase<MainThreadImpl> {
protected:
    using EventType = uint32_t;

    enum : EventType {
        EVENT_TYPE_WIN_RESIZE = WM_SIZE,
        EVENT_TYPE_WIN_CLOSE = WM_CLOSE,
        _EVENT_TYPE_APP = WM_APP,
    };

    struct Event {
        EventType type;
        void* data0;
        void* data1;
    };

    MainThreadImpl();

    // return user language name(iso-639-1).
    std::string queryUserLanguageName();

    void initWindowAndGl();

    void setWindowTitle(const char* utf8Str);

    void swapBuffers();

    void postEvent(const Event& e);

    void pollEvent(chrono::milliseconds timeout);

    std::optional<Event> peekEvent();

    //

    using clock = chrono::high_resolution_clock;

    struct Raii_HWND : xm::NonCopyable {
        explicit Raii_HWND(HWND handle) : mHandle(handle) {}

        ~Raii_HWND() {
            ::DestroyWindow(mHandle);
        }

        HWND handle() const {
            return mHandle;
        }

    private:
        HWND mHandle;
    };

    struct Raii_HDC : xm::NonCopyable {
        explicit Raii_HDC(HWND hwnd, HDC handle) : mHwnd(hwnd), mHandle(handle) {}

        ~Raii_HDC() {
            ::ReleaseDC(mHwnd, mHandle);
        }

        HDC handle() const {
            return mHandle;
        }

    private:
        HWND mHwnd;
        HDC mHandle;
    };

    struct Raii_HGLRC : xm::NonCopyable {
        explicit Raii_HGLRC(HGLRC handle) : mHandle(handle) {}

        ~Raii_HGLRC() {
            wglMakeCurrent(NULL, NULL);
            wglDeleteContext(mHandle);
        }

        HGLRC handle() const {
            return mHandle;
        }

    private:
        HGLRC mHandle;
    };

    LRESULT onWinProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // ----

    DWORD mThreadId = 0;
    std::optional<Raii_HWND> mHwnd;
    std::optional<Raii_HDC> mHdc;
    std::unique_ptr<Raii_HGLRC> mHglrc;
    std::deque<Event> mPendingEvents;
};
}  // namespace ss::detail
#endif
