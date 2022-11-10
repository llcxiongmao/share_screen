#include <llc/share_screen/FrontThread.hpp>
#include <llc/share_screen/DecodeThread.hpp>
#include <llc/share_screen/BackThread.hpp>

namespace llc {
namespace share_screen {
FrontThread::FrontThread() {
    try {
        mThreadId = GetCurrentThreadId();

        HINSTANCE hinstance = GetModuleHandle(NULL);
        WNDCLASSEX wc = {};
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = 0;
        wc.lpfnWndProc = &FrontThread::WinProc;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = hinstance;
        wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszMenuName = NULL;
        wc.lpszClassName = "llc_share_screen";
        wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
        THROW_IF(RegisterClassExA(&wc), "RegisterClassExA fail: " << Error::GetWin32String());
        mHwnd = CreateWindowExA(WS_EX_CLIENTEDGE,
                                "llc_share_screen",
                                "share_screen",
                                WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT,
                                CW_USEDEFAULT,
                                800,
                                600,
                                NULL,
                                NULL,
                                hinstance,
                                NULL);
        THROW_IF(mHwnd, "CreateWindowExA fail: " << Error::GetWin32String());

        if (Config::GetSingleton()->useGlRender) {
            mRender = std::make_unique<GlRender>();
            log::i() << "use gl render";
        } else {
            try {
                mRender = std::make_unique<Dx11Render>();
                log::i() << "use dx11 render";
            } catch (const std::exception& e) {
                log::e() << e.what() << ", create dx11 render fail, now use gl render";
                mRender = std::make_unique<GlRender>();
            }
        }

        THROW_IF(!UT_FAIL_CREATE, "unit test");
    } catch (const std::exception& e) {
        this->~FrontThread();
        throw Error() << "front thread create fail, " << e.what();
    }
}

FrontThread::~FrontThread() {
    mRender = nullptr;

    if (mHwnd) {
        DestroyWindow(mHwnd);
        mHwnd = 0;
    }
}

void FrontThread::loop() {
    MSG msg = {};
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        switch ((MsgId)msg.message) {
            case MsgId::PAINT:
                onPaint(msg);
                break;
            default:
                break;
        }
    }
}

LRESULT FrontThread::winProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT:
            ValidateRect(mHwnd, nullptr);
            break;
        case WM_CLOSE:
            close();
            break;
        case WM_SIZE:
            mRender->resize(LOWORD(lParam), HIWORD(lParam));
            break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void FrontThread::onPaint(const MSG& msg) {
    std::unique_ptr<PaintFrame> frame((PaintFrame*)msg.lParam);

    if (mIsStop)
        return;

    try {
        if (!mIsWinShow) {
            ShowWindow(mHwnd, SW_MAXIMIZE);
            mIsWinShow = true;
        }

        mRender->paint(frame.get());
        THROW_IF(!UT_FAIL_PAINT, "unit test");
        DecodeThread::GetSingleton()->notifyRecyclePaintFrame(frame);

        ++mFps;
        clock::time_point nowTp = clock::now();
        if (nowTp - mFpsTp > std::chrono::seconds(1)) {
            mWinTitle.clear();
            mWinTitle << "FPS = " << mFps;
            SetWindowTextA(mHwnd, mWinTitle.getInternalString().c_str());
            mFpsTp = nowTp;
            mFps = 0;
        }
    } catch (const std::exception& e) {
        log::e() << "front thread paint fail, " << e.what();
        mIsStop = true;
        close();
    }
}
}  // namespace share_screen
}  // namespace llc
