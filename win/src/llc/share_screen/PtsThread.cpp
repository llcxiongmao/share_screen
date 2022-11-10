#include <llc/share_screen/PtsThread.hpp>
#include <llc/share_screen/FrontThread.hpp>

namespace llc {
namespace share_screen {
using namespace std::chrono;

PtsThread::PtsThread() {
    if (!Config::GetSingleton()->disableHighPrecisionTime)
        timeBeginPeriod(1);

    mThread = std::make_unique<std::thread>([this]() { run(); });
}

void PtsThread::run() {
    log::i() << "pts thread run";

    while (true) {
        std::unique_ptr<PaintFrame> frame;
        if (!mPendingFrames.pop(frame))
            break;

        try {
            if (mIsStop) {
                frame = nullptr;
                continue;
            }

            if (mFirstPts == 0) {
                mFirstPts = frame->pts;
                mFirstTp = clock::now();
                FrontThread::GetSingleton()->notifyPaintFrame(frame);
            } else {
                clock::time_point now0 = clock::now();
                milliseconds expectWait = duration_cast<milliseconds>(
                    mFirstTp + microseconds(frame->pts - mFirstPts) - now0);
                if (expectWait > milliseconds(0))
                    Sleep(expectWait.count());

                if (Config::GetSingleton()->debugPrintPts) {
                    clock::time_point now1 = clock::now();
                    milliseconds realWait = duration_cast<milliseconds>(now1 - now0);
                    log::i() << "expect wait: " << expectWait.count()
                             << "ms, real wait:" << realWait.count() << "ms, pts: " << frame->pts;
                }
                FrontThread::GetSingleton()->notifyPaintFrame(frame);
            }
        } catch (const std::exception& e) {
            log::e() << "pts thread sync fail, " << e.what();
            mIsStop = true;
            close();
        }
    }

    FrontThread::GetSingleton()->close();

    log::i() << "pts thread exit";
}
}  // namespace share_screen
}  // namespace llc