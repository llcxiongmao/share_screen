#pragma once
#include <ss/Common.hpp>

#if defined(XM_OS_LINUX)
namespace ss::detail {
class MainThreadImpl : public xm::SingletonBase<MainThreadImpl> {
protected:
    using EventType = uint32_t;

    enum : EventType {
        EVENT_TYPE_WIN_RESIZE,
        EVENT_TYPE_WIN_CLOSE,
        _EVENT_TYPE_APP,
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

    // ----

    bool wakeup() {
        uint64_t v = 1;
        int n = write(mEventFd->fd(), &v, sizeof(v));
        return n == sizeof(v);
    }

    struct DisplayDeleter {
        void operator()(Display* p) const {
            XCloseDisplay(p);
        }
    };

    struct XVisualInfoDeleter {
        void operator()(XVisualInfo* p) const {
            XFree(p);
        }
    };

    struct RaiiFd : xm::NonCopyable {
        explicit RaiiFd(int fd) : mFd(fd) {
            assert(fd >= 0);
        }

        ~RaiiFd() {
            ::close(mFd);
        }

        int fd() const {
            return mFd;
        }

    private:
        int mFd;
    };

    struct RaiiGlxLoader : xm::NonCopyable {
        RaiiGlxLoader(Display* display, int screen) {
            int version = gladLoaderLoadGLX(display, screen);
            SS_THROW(version, "gladLoaderLoadGLX fail");
            Log::I("glx version: %d.%d", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
        }

        ~RaiiGlxLoader() {
            gladLoaderUnloadGLX();
        }
    };

    struct RaiiContext : xm::NonCopyable {
        RaiiContext(Display* display, GLXContext ctx) : mDisplay(display), mCtx(ctx) {}

        ~RaiiContext() {
            glXMakeCurrent(mDisplay, 0, nullptr);
            glXDestroyContext(mDisplay, mCtx);
        }

        GLXContext ctx() const {
            return mCtx;
        }

    private:
        Display* mDisplay;
        GLXContext mCtx;
    };

    struct RaiiColormap : xm::NonCopyable {
        RaiiColormap(Display* display, Colormap id) : mDisplay(display), mId(id) {}

        ~RaiiColormap() {
            XFreeColormap(mDisplay, mId);
        }

        Colormap id() const {
            return mId;
        }

    private:
        Display* mDisplay;
        Colormap mId;
    };

    struct RaiiWindow : xm::NonCopyable {
        RaiiWindow(Display* display, Window id) : mDisplay(display), mId(id) {}

        ~RaiiWindow() {
            XDestroyWindow(mDisplay, mId);
        }

        Window id() const {
            return mId;
        }

    private:
        Display* mDisplay;
        Window mId;
    };

    Atom m_WM_DELETE_WINDOW = {};
    Atom m_NET_WM_STATE = {};
    Atom m_NET_WM_STATE_MAXIMIZED_HORZ = {};
    Atom m_NET_WM_STATE_MAXIMIZED_VERT = {};
    Atom m_NET_WM_NAME = {};
    Atom m_NET_WM_ICON_NAME = {};
    Atom m_UTF8_STRING = {};

    int mDefaultScreen = 0;
    Window mRootWindow = 0;
    std::optional<RaiiFd> mEventFd;
    std::optional<RaiiGlxLoader> mGlxLoader; // long life than Display, otherwise crash.
    std::unique_ptr<Display, DisplayDeleter> mDisplay;
    std::unique_ptr<XVisualInfo, XVisualInfoDeleter> mVisualInfo;
    std::optional<RaiiColormap> mColormap;
    std::optional<RaiiWindow> mWin;
    std::unique_ptr<RaiiContext> mCtx;

    std::mutex mCacheEventLock;
    xm::Array<Event> mCacheEvents;  // guard by mCacheEventLock.
    std::deque<Event> mPendingEvents;
};
}  // namespace ss::detail
#endif