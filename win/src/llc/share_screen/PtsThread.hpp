#pragma once
#include <llc/share_screen/common.hpp>

namespace llc {
namespace share_screen {
/** pts thread, handle frame sync with present timestamp. */
class PtsThread : public util::Singleton<PtsThread> {
public:
    /** create and start thread. */
    PtsThread();

    ~PtsThread() {}

    /**
     * notify sync frame, take frame ownership if success, otherwise throw Error.
     *
     * @param frame frame to push.
     * @throws Error if this closed, see #close.
     */
    void notifySyncFrame(std::unique_ptr<PaintFrame>& frame) {
        THROW_IF(mPendingFrames.pushEx(frame), "pts thread already closed");
    }

    /** close thread. after call notifySyncFrame will throw Error. */
    void close() {
        mPendingFrames.close();
    }

    /** join thread. */
    void join() {
        mThread->join();
    }

private:
    typedef std::chrono::high_resolution_clock clock;

    void run();

private:
    bool mIsStop = false;
    int64_t mFirstPts = 0;
    clock::time_point mFirstTp;
    /** pending frames to sync pts. */
    BlockingQueue<std::unique_ptr<PaintFrame>> mPendingFrames;
    std::unique_ptr<std::thread> mThread;
};
}  // namespace share_screen
}  // namespace llc