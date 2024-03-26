#include <ss/PtsThread.hpp>
#include <ss/MainThread.hpp>
#include <ss/DecodeThread.hpp>

#if defined(XM_OS_WINDOWS)
    #include <timeapi.h>
    #pragma comment(lib, "Winmm.lib")
#endif

namespace ss {
PtsThread::PtsThread() {
#if defined(XM_OS_WINDOWS)
    ::timeBeginPeriod(1);
#endif

    try {
        mThread.emplace([this]() { run(); });
    } catch (const std::exception& e) {
        SS_THROW(0, "create thread fail: %s", e.what());
    }
}

void PtsThread::run() {
    Log::I_STR("pts thread run");

    struct TagExit {};

    try {
        while (!mClose) {
            PaintFrame* paintFrame = nullptr;
            std::optional<PaintFrame*> tmp =
                mPendingPaintFrames.pop_front(std::chrono::milliseconds(2000));
            if (!tmp) {
                continue;
            } else {
                paintFrame = *tmp;
                if (!paintFrame) {
                    throw TagExit {};
                }
            }

            if (mFirstPts == 0) {
                mFirstPts = paintFrame->pts;
                mFirstTp = clock::now();
                MainThread::Singleton()->notifyPaintFrame(paintFrame);
            } else {
                clock::time_point now0 = clock::now();
                chrono::milliseconds expectWait = chrono::duration_cast<chrono::milliseconds>(
                    mFirstTp + chrono::microseconds(paintFrame->pts - mFirstPts) - now0);

                if (expectWait > chrono::milliseconds(0)) {
                    std::this_thread::sleep_for(expectWait);
                }

                if (Config::Singleton()->debugPts) {
                    clock::time_point now1 = clock::now();
                    chrono::milliseconds realWait =
                        chrono::duration_cast<chrono::milliseconds>(now1 - now0);
                    Log::I(
                        "expect wait: %lldms, real wait: %lldms, pts: %lld",
                        (long long)expectWait.count(),
                        (long long)realWait.count(),
                        (long long)paintFrame->pts);
                }

                MainThread::Singleton()->notifyPaintFrame(paintFrame);
            }
        }
    } catch (const TagExit&) {  //
    } catch (const Error& e) {
        Log::PrintError(e);
    } catch (const std::exception& e) {
        Log::E("catch %s: %s, %s#%d", typeid(e).name(), e.what(), __FILE__, __LINE__);
    }

    MainThread::Singleton()->notifyClose();

    Log::I_STR("pts thread exit");
}
}  // namespace ss