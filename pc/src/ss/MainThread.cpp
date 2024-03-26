#include <ss/MainThread.hpp>
#include <ss/DecodeThread.hpp>

namespace ss {
MainThread::MainThread() {
    mFpsTp = clock::now();
    mFps = 0;

    std::string languageName = queryUserLanguageName();
    Log::I("system language: %s", languageName.c_str());
    if (languageName == "zh") {
        mLocale.asZh();
    } else {
        mLocale.asEn();
    }

    initWindowAndGl();
    setWindowTitle(mLocale.title_connecting.c_str());
    mRender.emplace();
}

void MainThread::notifyPaintFrame(PaintFrame* paintFrame) {
    postEvent({EVENT_TYPE_PAINT_FRAME, paintFrame, nullptr});
}

void MainThread::notifyClose() {
    mClose = true;
    try {
        postEvent({EVENT_TYPE_CLOSE, nullptr, nullptr});
    } catch (...) {
        // ignore any error, we can still exit by timeout.
    }
}

void MainThread::loop() {
    while (!mClose) {
        pollEvent(chrono::milliseconds(2000));
        for (std::optional<Event> e = peekEvent(); e; e = peekEvent()) {
            switch (e->type) {
                case EVENT_TYPE_WIN_RESIZE: {
                    size_t newWidth = (size_t)e->data0;
                    size_t newHeight = (size_t)e->data1;
                    if (mWinWidth != newWidth || mWinHeight != newHeight) {
                        Log::I(
                            "win size changed: (%d, %d) -> (%d, %d)",
                            (int)mWinWidth,
                            (int)mWinHeight,
                            (int)newWidth,
                            (int)newHeight);
                        mWinWidth = newWidth;
                        mWinHeight = newHeight;
                        draw(nullptr);
                    }
                    break;
                }
                case EVENT_TYPE_PAINT_FRAME: {
                    PaintFrame* paintFrame = (PaintFrame*)e->data0;
                    draw(paintFrame);
                    DecodeThread::Singleton()->notifyRecyclePaintFrame(paintFrame);

                    ++mFps;
                    clock::time_point nowTp = clock::now();
                    if (nowTp - mFpsTp > std::chrono::seconds(1)) {
                        char titleStr[512];
                        snprintf(titleStr, 512, mLocale.title_connected.c_str(), (int)mFps);
                        setWindowTitle(titleStr);
                        mFpsTp = nowTp;
                        mFps = 0;
                    }
                    break;
                }
                case EVENT_TYPE_WIN_CLOSE:
                case EVENT_TYPE_CLOSE:
                    return;
            }
        }
    }
}

void MainThread::draw(PaintFrame* paintFrame) {
    if (!mWinWidth || !mWinHeight) {
        return;
    }

    glViewport(0, 0, mWinWidth, mWinHeight);
    glScissor(0, 0, mWinWidth, mWinHeight);
    mRender->paint(paintFrame);
    swapBuffers();
}
}  // namespace ss
