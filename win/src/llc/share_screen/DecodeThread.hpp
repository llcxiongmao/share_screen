#pragma once
#include <llc/share_screen/common.hpp>

namespace llc {
namespace share_screen {
/** decode thread, handle frame decode. */
class DecodeThread : public util::Singleton<DecodeThread> {
public:
    /** create and start thread. */
    DecodeThread();

    /**
     * notify recycle paint frame, take frame ownership if success, otherwise throw Error.
     *
     * @param frame frame to recycle.
     * @throws Error if this closed.
     */
    void notifyNewFreePaintFrame(std::unique_ptr<PaintFrame>& frame) {
        av_frame_unref(frame->decodeFrame.get());
        THROW_IF(mFreePaintFrames.pushEx(frame), "decode thread already closed");
    }

    /**
     * notify decode frame, take frame ownership if success, otherwise throw Error.
     *
     * @param frame frame to decode.
     * @throws Error if this closed.
     */
    void notifyNewFrame(std::unique_ptr<NetFrame>& frame) {
        THROW_IF(mPendingFrames.pushEx(frame), "decode thread already closed");
    }

    /** close thread, after call notifyNewFreePaintFrame notifyNewFrame will throw Error.  */
    void close() {
        mFreePaintFrames.close();
        mPendingFrames.close();
    }

    /** join thread. */
    void join() {
        mThread->join();
    }

private:
    static constexpr int PAINT_FRAME_POOL_SIZE = 20;

    typedef std::chrono::high_resolution_clock clock;

    void init();

    void run();

private:
    bool mIsStop = false;
    /** free paint frame, it can use as decode dst. */
    BlockingQueue<std::unique_ptr<PaintFrame>> mFreePaintFrames;
    /**
     * when pts == -1 the frame not output image(it only contain config information),
     * we cache it until image frame come in, then merge together to decode.
     */
    std::unique_ptr<AVPacket> mCachePacket;
    const AVCodec* mCodec = nullptr;
    std::unique_ptr<AVCodecContext> mCodecCtx;
    /** pending frames to decode. */
    BlockingQueue<std::unique_ptr<NetFrame>> mPendingFrames;
    std::unique_ptr<std::thread> mThread;
};
}  // namespace share_screen
}  // namespace llc
