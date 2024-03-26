#pragma once
#include <ss/Common.hpp>

namespace ss {
/** pts thread, handle frame sync with present timestamp. */
class PtsThread : public xm::SingletonBase<PtsThread> {
public:
    PtsThread();

    void notifySyncFrame(PaintFrame* paintFrame) {
        mPendingPaintFrames.push_back(paintFrame);
    }

    void notifyClose() {
        mClose = true;
        mPendingPaintFrames.push_back(nullptr);
    }

    void join() {
        mThread->join();
    }

private:
    using clock = std::chrono::high_resolution_clock;

    void run();

    // ----

    bool mClose = false;
    int64_t mFirstPts = 0;
    clock::time_point mFirstTp;
    BlockingQueue<PaintFrame*> mPendingPaintFrames;
    std::optional<std::thread> mThread;
};
}  // namespace ss