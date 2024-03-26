#include <ss/main_thread_impl/Linux.hpp>

#define GLAD_GLX_IMPLEMENTATION
#include <glad/glx.h>

#define _NET_WM_STATE_REMOVE 0
#define _NET_WM_STATE_ADD 1
#define _NET_WM_STATE_TOGGLE 2

namespace ss::detail {
static GLADapiproc (*s_glXGetProcAddressARB)(const char* name) = nullptr;

MainThreadImpl::MainThreadImpl() {
    int _eventFd = ::eventfd(0, EFD_NONBLOCK);
    check_errno(_eventFd != -1, "eventfd");
    mEventFd.emplace(_eventFd);

    Display* _display = XOpenDisplay(nullptr);
    SS_THROW(_display, "'XOpenDisplay' fail");
    mDisplay.reset(_display);

    mDefaultScreen = DefaultScreen(mDisplay.get());
    mGlxLoader.emplace(mDisplay.get(), mDefaultScreen);
    mRootWindow = RootWindow(mDisplay.get(), mDefaultScreen);
}

std::string MainThreadImpl::queryUserLanguageName() {
    char* l = setlocale(LC_CTYPE, "");
    if (!l) {
        return "en";
    }
    size_t len = strlen(l);
    if (len < 2) {
        return "en";
    }
    return std::string(l, 2);
}

void MainThreadImpl::initWindowAndGl() {
    // clang-format off
    static int visualAttribs[] = {
        GLX_RGBA,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        GLX_DOUBLEBUFFER, True,
        None,
    };
    // clang-format on
    XVisualInfo* _visualInfo = glXChooseVisual(mDisplay.get(), mDefaultScreen, visualAttribs);
    SS_THROW(_visualInfo, "'glXChooseVisual' fail");
    mVisualInfo.reset(_visualInfo);

    Colormap _colormap =
        XCreateColormap(mDisplay.get(), mRootWindow, mVisualInfo->visual, AllocNone);
    mColormap.emplace(mDisplay.get(), _colormap);

    XSetWindowAttributes swa = {};
    swa.colormap = mColormap->id();
    swa.background_pixmap = None;
    swa.border_pixel = 0;
    swa.event_mask = StructureNotifyMask;
    Window _win = XCreateWindow(
        mDisplay.get(),
        mRootWindow,
        0,
        0,
        400,
        400,
        0,
        mVisualInfo->depth,
        InputOutput,
        mVisualInfo->visual,
        CWColormap | CWEventMask,
        &swa);
    mWin.emplace(mDisplay.get(), _win);

    XMapWindow(mDisplay.get(), mWin->id());

    m_WM_DELETE_WINDOW = XInternAtom(mDisplay.get(), "WM_DELETE_WINDOW", False);
    m_NET_WM_STATE = XInternAtom(mDisplay.get(), "_NET_WM_STATE", False);
    m_NET_WM_STATE_MAXIMIZED_HORZ =
        XInternAtom(mDisplay.get(), "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    m_NET_WM_STATE_MAXIMIZED_VERT =
        XInternAtom(mDisplay.get(), "_NET_WM_STATE_MAXIMIZED_VERT", False);
    m_NET_WM_NAME = XInternAtom(mDisplay.get(), "_NET_WM_NAME", False);
    m_NET_WM_ICON_NAME = XInternAtom(mDisplay.get(), "_NET_WM_ICON_NAME", False);
    m_UTF8_STRING = XInternAtom(mDisplay.get(), "UTF8_STRING", False);

    XSetWMProtocols(mDisplay.get(), mWin->id(), &m_WM_DELETE_WINDOW, 1);

    GLXContext _tmpCtx = glXCreateContext(mDisplay.get(), mVisualInfo.get(), 0, 1);
    SS_THROW(_tmpCtx, "'glXCreateContext' fail");
    std::unique_ptr<RaiiContext> tmpCtx = std::make_unique<RaiiContext>(mDisplay.get(), _tmpCtx);

    SS_THROW(glXMakeCurrent(mDisplay.get(), mWin->id(), tmpCtx->ctx()), "'glXMakeCurrent' fail");

    s_glXGetProcAddressARB =
        (decltype(s_glXGetProcAddressARB))glad_dlsym_handle(_glx_handle, "glXGetProcAddressARB");
    gladLoadGL(s_glXGetProcAddressARB);

    SS_THROW(glGetString != nullptr, "miss 'glGetString', you may not install graphics driver");

    // check tmp opengl version, if >= 2.1 we use it, otherwise we create by
    // 'wglCreateContextAttribsARB'.
    const char* tmpGlVersion = (const char*)glGetString(GL_VERSION);
    int tmpGlMajor = 0;
    int tmpGlMinor = 0;
    (void)sscanf(tmpGlVersion, "%d.%d", &tmpGlMajor, &tmpGlMinor);
    if (tmpGlMajor > 2 || (tmpGlMajor == 2 && tmpGlMinor >= 1)) {
        mCtx = std::move(tmpCtx);
    } else {
        SS_THROW(GLAD_GLX_ARB_create_context, "miss 'GLX_ARB_create_context'");
        Log::I("found 'GLX_ARB_create_context'");

        // clang-format off
        int visualAttribs2[] = {
            GLX_X_RENDERABLE, True,
            GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
            GLX_RENDER_TYPE, GLX_RGBA_BIT,
            GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
            GLX_RED_SIZE, 8,
            GLX_GREEN_SIZE, 8,
            GLX_BLUE_SIZE, 8,
            GLX_ALPHA_SIZE, 8,
            GLX_DOUBLEBUFFER, True,
            None,
        };
        // clang-format on
        int nFbCfg = 0;
        GLXFBConfig* fbCfgs =
            glXChooseFBConfig(mDisplay.get(), mDefaultScreen, visualAttribs2, &nFbCfg);
        SS_THROW(nFbCfg > 0, "'glXChooseFBConfig' fail, no 'GLXFBConfig' found");

        // clang-format off
        int ctxAttribs[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 2,
            GLX_CONTEXT_MINOR_VERSION_ARB, 1,
            None,
        };
        // clang-format on
        GLXContext _ctx =
            glXCreateContextAttribsARB(mDisplay.get(), fbCfgs[0], 0, True, ctxAttribs);
        SS_THROW(_ctx, "'glXCreateContextAttribsARB' fail");
        mCtx = std::make_unique<RaiiContext>(mDisplay.get(), _ctx);
    }

    tmpCtx = nullptr;
    SS_THROW(glXMakeCurrent(mDisplay.get(), mWin->id(), mCtx->ctx()), "'glXMakeCurrent' fail");
    Log::I("opengl version: %s", (const char*)glGetString(GL_VERSION));

    // try to disable vsync.
    if (GLAD_GLX_EXT_swap_control) {
        Log::I("found 'GLX_EXT_swap_control'");
        glXSwapIntervalEXT(mDisplay.get(), mWin->id(), 0);
        Log::I("disable vsync");
    } else {
        Log::I("miss 'GLX_EXT_swap_control'");
    }

    XEvent maxShowEvent;
    memset(&maxShowEvent, 0, sizeof(maxShowEvent));
    maxShowEvent.type = ClientMessage;
    maxShowEvent.xclient.window = mWin->id();
    maxShowEvent.xclient.message_type = m_NET_WM_STATE;
    maxShowEvent.xclient.format = 32;
    maxShowEvent.xclient.data.l[0] = _NET_WM_STATE_ADD;
    maxShowEvent.xclient.data.l[1] = m_NET_WM_STATE_MAXIMIZED_HORZ;
    maxShowEvent.xclient.data.l[2] = m_NET_WM_STATE_MAXIMIZED_VERT;
    XSendEvent(mDisplay.get(), mRootWindow, False, SubstructureNotifyMask, &maxShowEvent);

    XFlush(mDisplay.get());
}

void MainThreadImpl::setWindowTitle(const char* utf8Str) {
    int len = (int)strlen(utf8Str);
    XChangeProperty(
        mDisplay.get(),
        mWin->id(),
        m_NET_WM_NAME,
        m_UTF8_STRING,
        8,
        PropModeReplace,
        (const unsigned char*)utf8Str,
        len);
    XChangeProperty(
        mDisplay.get(),
        mWin->id(),
        m_NET_WM_ICON_NAME,
        m_UTF8_STRING,
        8,
        PropModeReplace,
        (const unsigned char*)utf8Str,
        len);
    XFlush(mDisplay.get());
}

void MainThreadImpl::swapBuffers() {
    glXSwapBuffers(mDisplay.get(), mWin->id());
}

void MainThreadImpl::postEvent(const Event& e) {
    {
        std::lock_guard<std::mutex> lock(mCacheEventLock);
        mCacheEvents.push_back(e);
    }

    SS_THROW(wakeup(), "wakeup fail");
}

void MainThreadImpl::pollEvent(chrono::milliseconds timeout) {
    int displayFd = ConnectionNumber(mDisplay.get());
    pollfd fds[] = {
        pollfd {displayFd, POLLIN, 0},
        pollfd {mEventFd->fd(), POLLIN, 0},
    };
    int r = ::poll(fds, 2, timeout.count());
    check_errno(r >= 0, "poll");

    if (!r) {
        return;
    }

    if (fds[0].revents & POLLIN) {
        while (XPending(mDisplay.get())) {
            XEvent event;
            XNextEvent(mDisplay.get(), &event);
            if (event.type == ConfigureNotify) {
                mPendingEvents.push_back(Event {
                    EVENT_TYPE_WIN_RESIZE,
                    (void*)(size_t)event.xconfigure.width,
                    (void*)(size_t)event.xconfigure.height});
            } else if (
                event.type == ClientMessage &&  //
                event.xclient.data.l[0] == m_WM_DELETE_WINDOW) {
                mPendingEvents.push_back(Event {EVENT_TYPE_WIN_CLOSE, nullptr, nullptr});
            }
        }
    }
    if (fds[1].revents & POLLIN) {
        uint64_t v = 1;
        ssize_t read_n = ::read(fds[1].fd, &v, 8);
        SS_THROW(read_n == 8, "eventfd read %zd bytes(should be 8)", read_n);

        {
            std::lock_guard<std::mutex> lock(mCacheEventLock);
            for (const auto& i : mCacheEvents) {
                mPendingEvents.push_back(i);
            }
            mCacheEvents.clear();
        }
    }
}

std::optional<MainThreadImpl::Event> MainThreadImpl::peekEvent() {
    if (mPendingEvents.empty()) {
        return std::nullopt;
    }
    Event r = mPendingEvents.front();
    mPendingEvents.pop_front();
    return r;
}
}  // namespace ss::detail