#include <ss/main_thread_impl/Windows.hpp>

#include <ss/_resource.h>

#define GLAD_WGL_IMPLEMENTATION
#include <glad/wgl.h>

#if defined(XM_OS_WINDOWS)
namespace ss::detail {
MainThreadImpl::MainThreadImpl() {
    mThreadId = ::GetCurrentThreadId();
}

std::string MainThreadImpl::queryUserLanguageName() {
    std::string r;
    wchar_t wname[10];
    ::GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SISO639LANGNAME, wname, 10);
    for (auto i : wname) {
        if (!i) {
            break;
        }
        r.push_back((char)i);
    }
    return r;
}

void MainThreadImpl::initWindowAndGl() {
    HINSTANCE hinstance = GetModuleHandle(NULL);
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = 0;
    wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
        return MainThreadImpl::Singleton()->onWinProc(hwnd, msg, wParam, lParam);
    };
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hinstance;
    wc.hIcon = LoadIcon(hinstance, MAKEINTRESOURCE(IDI_ICON1));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "llc_share_screen";
    wc.hIconSm = LoadIcon(hinstance, MAKEINTRESOURCE(IDI_ICON1));
    check_lasterror(::RegisterClassExA(&wc), "RegisterClassExA");
    DWORD winStyle = WS_OVERLAPPEDWINDOW;
    HWND _hwnd = ::CreateWindowExA(
        0,
        "llc_share_screen",
        "share_screen",
        winStyle,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        400,
        400,
        NULL,
        NULL,
        hinstance,
        NULL);
    check_lasterror(_hwnd, "CreateWindowExA");
    mHwnd.emplace(_hwnd);
    ::ShowWindow(mHwnd->handle(), SW_MAXIMIZE);

    HDC _hdc = ::GetDC(mHwnd->handle());
    check_lasterror(_hdc, "GetDC");
    mHdc.emplace(mHwnd->handle(), _hdc);

    PIXELFORMATDESCRIPTOR pfd = {};
    ::memset(&pfd, 0, sizeof(PIXELFORMATDESCRIPTOR));
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.iLayerType = PFD_MAIN_PLANE;
    int pixelFormat = ::ChoosePixelFormat(mHdc->handle(), &pfd);
    check_lasterror(pixelFormat, "ChoosePixelFormat");
    check_lasterror(::SetPixelFormat(mHdc->handle(), pixelFormat, &pfd), "SetPixelFormat");

    // create tmp opengl context.
    HGLRC _tmpHglRc = wglCreateContext(mHdc->handle());
    check_lasterror(_tmpHglRc, "wglCreateContext");
    auto tmpHglRc = std::make_unique<Raii_HGLRC>(_tmpHglRc);

    check_lasterror(wglMakeCurrent(mHdc->handle(), tmpHglRc->handle()), "wglMakeCurrent");

    gladLoaderLoadWGL(mHdc->handle());
    gladLoadGL(
        [](const char* name) -> GLADapiproc { return (GLADapiproc)wglGetProcAddress(name); });

    SS_THROW(glGetString != nullptr, "miss 'glGetString', you may not install graphics driver");

    // check tmp opengl version, if >= 2.1 we use it, otherwise we create by
    // 'wglCreateContextAttribsARB'.
    const char* tmpGlVersion = (const char*)glGetString(GL_VERSION);
    int tmpGlMajor = 0;
    int tmpGlMinor = 0;
    (void)::sscanf(tmpGlVersion, "%d.%d", &tmpGlMajor, &tmpGlMinor);
    // tmpGlMajor = 1;
    if (tmpGlMajor > 2 || (tmpGlMajor == 2 && tmpGlMinor >= 1)) {
        mHglrc = std::move(tmpHglRc);
    } else {
        SS_THROW(GLAD_WGL_ARB_create_context, "miss 'WGL_ARB_create_context'");
        Log::I("found 'WGL_ARB_create_context'");

        static constexpr int contextAttribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB,
            2,
            WGL_CONTEXT_MINOR_VERSION_ARB,
            1,
            WGL_CONTEXT_FLAGS_ARB,
            0,
            0,
        };
        HGLRC _hglRc = wglCreateContextAttribsARB(mHdc->handle(), 0, contextAttribs);
        check_lasterror(_hglRc, "wglCreateContextAttribsARB");
        mHglrc = std::make_unique<Raii_HGLRC>(_hglRc);
    }

    tmpHglRc = nullptr;
    check_lasterror(wglMakeCurrent(mHdc->handle(), mHglrc->handle()), "wglMakeCurrent");
    Log::I("opengl version: %s", (const char*)glGetString(GL_VERSION));

    // try to disable vsync.
    if (GLAD_WGL_EXT_swap_control) {
        Log::I("found 'WGL_EXT_swap_control'");
        wglSwapIntervalEXT(0);
        Log::I("disable vsync");
    } else {
        Log::I("miss 'WGL_EXT_swap_control'");
    }
}

void MainThreadImpl::setWindowTitle(const char* utf8Str) {
    wchar_t wStr[512];
    int wStrLen = ::MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, wStr, 512);
    assert(wStrLen > 0);
    ::SetWindowTextW(mHwnd->handle(), wStr);
}

void MainThreadImpl::swapBuffers() {
    ::SwapBuffers(mHdc->handle());
}

void MainThreadImpl::postEvent(const Event& e) {
    check_lasterror(
        ::PostThreadMessageA(mThreadId, (UINT)e.type, (WPARAM)e.data0, (LPARAM)e.data1),
        "PostThreadMessageA");
}

void MainThreadImpl::pollEvent(chrono::milliseconds timeout) {
    DWORD r = ::MsgWaitForMultipleObjects(0, NULL, FALSE, timeout.count(), QS_ALLINPUT);
    check_lasterror(r != WAIT_FAILED, "MsgWaitForMultipleObjects");
    MSG msg;
    while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
        if (msg.message >= WM_APP && msg.message <= 0xBFFF) {
            mPendingEvents.push_back(
                Event {(EventType)msg.message, (void*)msg.wParam, (void*)msg.lParam});
        } else {
            ::TranslateMessage(&msg);
            ::DispatchMessageA(&msg);
        }
    }
}

std::optional<MainThreadImpl::Event> MainThreadImpl::peekEvent() {
    if (mPendingEvents.empty()) {
        return std::nullopt;
    }
    Event e = mPendingEvents.front();
    mPendingEvents.pop_front();
    return e;
}

LRESULT MainThreadImpl::onWinProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    LRESULT r = 0;
    switch (msg) {
        case WM_PAINT:
            ::ValidateRect(hwnd, nullptr);
            break;
        case WM_CLOSE:
            mPendingEvents.push_back(Event {EVENT_TYPE_WIN_CLOSE, nullptr, nullptr});
            break;
        case WM_SIZE:
            mPendingEvents.push_back(Event {
                EVENT_TYPE_WIN_RESIZE,
                (void*)(size_t)LOWORD(lParam),
                (void*)(size_t)HIWORD(lParam)});
            break;
        default:
            r = ::DefWindowProcA(hwnd, msg, wParam, lParam);
            break;
    }
    return r;
}
}  // namespace ss::detail
#endif