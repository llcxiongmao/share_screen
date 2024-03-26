#pragma once
#include <ss/Common.hpp>

namespace ss {
/** decode thread, handle frame decode. */
class DecodeThread : public xm::SingletonBase<DecodeThread> {
public:
    DecodeThread();

    void notifyRecyclePaintFrame(PaintFrame* paintFrame) {
        mFreePaintFrames.push_back(paintFrame);
    }

    void notifyDecodeFrame(NetFrame* netFrame) {
        mPendingNetFrames.push_back(netFrame);
    }

    void notifyClose() {
        mClose = true;
        mPendingNetFrames.push_back(nullptr);
        mFreePaintFrames.push_back(nullptr);
    }

    void join() {
        mThread->join();
    }

private:
    struct AVCodecContextDeleter {
        void operator()(AVCodecContext* p) const {
            avcodec_free_context(&p);
        }
    };

    enum : uint8_t {
        PAINT_FRAME_POOL_CAPACITY = 20,

        /** unit test simulate options. */
        UTSO_FAIL_DECODE = 0,
    };

    void run();

    // ----

    bool mClose = false;

    std::optional<std::thread> mThread;

    const AVCodec* mCodec = nullptr;
    std::unique_ptr<AVCodecContext, AVCodecContextDeleter> mCodecCtx;

    std::optional<PaintFrame> mPaintFramePool[PAINT_FRAME_POOL_CAPACITY];

    /** pending net-frame, use as decode src. */
    BlockingQueue<NetFrame*> mPendingNetFrames;
    /** free paint-frame, use as decode dst. */
    BlockingQueue<PaintFrame*> mFreePaintFrames;
};
}  // namespace ss
