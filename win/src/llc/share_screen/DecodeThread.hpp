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
    void notifyRecyclePaintFrame(std::unique_ptr<PaintFrame>& frame) {
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

    /** close thread, after call notifyRecyclePaintFrame notifyNewFrame will throw Error.  */
    void close() {
        mFreePaintFrames.close();
        mPendingFrames.close();
    }

    /** join thread. */
    void join() {
        mThread->join();
    }

private:
    /** unit test simulazte options. */
    static constexpr bool UT_FAIL_INIT_RESOURCES = false;
    static constexpr bool UT_FAIL_DECODE = false;

    static constexpr int PAINT_FRAME_POOL_SIZE = 20;

    typedef std::chrono::high_resolution_clock clock;

    void initResources();

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

    /** keep net frame alive, because back thread may access. */
    std::vector<std::unique_ptr<NetFrame>> mDumpNetFrames;
};
}  // namespace share_screen
}  // namespace llc
