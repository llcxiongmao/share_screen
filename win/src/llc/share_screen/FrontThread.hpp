#pragma once
#include <llc/share_screen/Render.hpp>

namespace llc {
namespace share_screen {
/** front thread, handle frame draw. */
class FrontThread : public util::Singleton<FrontThread> {
public:
    /** create window, create render, throw Error if fail. */
    FrontThread();

    ~FrontThread();

    HWND getHwnd() const {
        return mHwnd;
    }

    Render* getRender() const {
        return mRender.get();
    }

    /**
     * notify paint frame. success take frame ownership, otherwise throw Error.
     *
     * @param frame frame to paint.
     * @throws Error if this closed, see #close.
     */
    void notifyPaintFrame(std::unique_ptr<PaintFrame>& frame) {
        std::lock_guard<std::mutex> lock(mCloseLock);
        THROW_IF(!mIsClose, "front thread already closed");
        PostThreadMessageA(mThreadId, (UINT)MsgId::PAINT, 0, (LPARAM)frame.get());
        frame.release();
    }

    /** close thread, after call notifyPaintFrame will throw Error. */
    void close() {
        std::lock_guard<std::mutex> lock(mCloseLock);
        if (!mIsClose) {
            PostThreadMessageA(mThreadId, WM_QUIT, 0, 0);
            mIsClose = true;
        }
    }

    /** exec message loop. */
    void loop();

private:
    /** unit test simulate options. */
    static constexpr bool UT_FAIL_CREATE = false;
    static constexpr bool UT_FAIL_PAINT = false;

    typedef std::chrono::high_resolution_clock clock;

    enum class MsgId : UINT {
        PAINT = WM_APP + 1,
    };

    static LRESULT CALLBACK WinProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        return FrontThread::GetSingleton()->winProc(hwnd, msg, wParam, lParam);
    }

    LRESULT winProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void onPaint(const MSG& msg);

private:
    bool mIsStop = false;
    std::mutex mCloseLock;
    bool mIsClose = false;
    DWORD mThreadId = 0;
    HWND mHwnd = 0;
    bool mIsWinShow = false;
    std::unique_ptr<Render> mRender;
    util::StringStream mWinTitle;
    clock::time_point mFpsTp;
    uint32_t mFps = 0;
};
}  // namespace share_screen
}  // namespace llc
