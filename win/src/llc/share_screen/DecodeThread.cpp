#include <llc/share_screen/DecodeThread.hpp>
#include <llc/share_screen/FrontThread.hpp>
#include <llc/share_screen/PtsThread.hpp>
#include <llc/share_screen/BackThread.hpp>

namespace llc {
namespace share_screen {
using namespace std::chrono;

DecodeThread::DecodeThread() {
    mThread = std::make_unique<std::thread>([this]() { run(); });
}

void DecodeThread::initResources() {
    for (int i = 0; i < PAINT_FRAME_POOL_SIZE; ++i) {
        mFreePaintFrames.push(std::make_unique<PaintFrame>());
    }

    av_log_set_callback(&log::av_log_callback);

    mCachePacket.reset(av_packet_alloc());

    mCodec = avcodec_find_decoder(AV_CODEC_ID_H264);
    THROW_IF(mCodec, "find h264 decoder");

    mCodecCtx.reset(avcodec_alloc_context3(mCodec));

    Render* render = FrontThread::GetSingleton()->getRender();
    switch (render->getType()) {
        case RenderType::GL:
            break;
        case RenderType::DX11: {
            if (Config::GetSingleton()->disableHwaccel)
                break;
            mCodecCtx->get_format = [](AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts) {
                for (const AVPixelFormat* p = pix_fmts; *p != AV_PIX_FMT_NONE; ++p) {
                    if (*p == AV_PIX_FMT_D3D11)
                        return *p;
                }
                // if no hwaccel supported, back to software format.
                return ctx->sw_pix_fmt;
            };
            mCodecCtx->hw_device_ctx = av_buffer_ref(static_cast<Dx11Render*>(render)->getHwCtx());
            mCodecCtx->hwaccel_flags |= AV_HWACCEL_FLAG_ALLOW_PROFILE_MISMATCH;
            break;
        }
        default:
            assert(0);
            break;
    }

    THROW_IF_AV(avcodec_open2(mCodecCtx.get(), mCodec, NULL));

    THROW_IF(!UT_FAIL_INIT_RESOURCES, "unit test");
}

void DecodeThread::run() {
    log::i() << "decode thread run";

    try {
        initResources();
    } catch (const std::exception& e) {
        log::e() << "decode thread init resources fail, " << e.what();
        close();
    }

    clock::time_point now0;
    clock::time_point now1;
    std::unique_ptr<NetFrame> src;
    std::unique_ptr<PaintFrame> dst;
    while (true) {
        if (!mPendingFrames.pop(src))
            break;

        if (!mFreePaintFrames.pop(dst))
            break;

        if (mIsStop) {
            mDumpNetFrames.push_back(std::move(src));
            dst = nullptr;
            continue;
        }

        try {
            dst->pts = src->pts;

            AVPacket* packet = nullptr;
            // check whether need merge to cache packet.
            if (mCachePacket->size || dst->pts == -1) {
                int offset = mCachePacket->size;
                int growSize = src->body->size;
                uint8_t* growData = src->body->data;
                av_grow_packet(mCachePacket.get(), growSize);
                memcpy(mCachePacket->data + offset, growData, growSize);
                packet = mCachePacket.get();
            } else {
                packet = src->body.get();
            }

            if (dst->pts == -1) {
                notifyRecyclePaintFrame(dst);
            } else {
                if (Config::GetSingleton()->debugPrintDecode)
                    now0 = clock::now();

                THROW_IF_AV(avcodec_send_packet(mCodecCtx.get(), packet));
                THROW_IF_AV(avcodec_receive_frame(mCodecCtx.get(), dst->decodeFrame.get()));

                THROW_IF(!UT_FAIL_DECODE, "unit test");

                if (Config::GetSingleton()->debugPrintDecode) {
                    now1 = clock::now();
                    log::i() << "decode time: " << duration_cast<milliseconds>(now1 - now0).count()
                             << "ms, pts: " << dst->pts
                             << ", key frame: " << util::fmt_bool(dst->decodeFrame->key_frame);
                }
                av_packet_unref(mCachePacket.get());
                if (PtsThread::GetSingleton())
                    PtsThread::GetSingleton()->notifyNewFrame(dst);
                else
                    FrontThread::GetSingleton()->notifyPaintFrame(dst);
            }

            BackThread::GetSingleton()->notifyRecycleNetFrame(src);
        } catch (const std::exception& e) {
            log::e() << "decode thread decode fail, " << e.what();
            mIsStop = true;
            close();
        }
        if (src)
            mDumpNetFrames.push_back(std::move(src));
    }

    if (src)
        mDumpNetFrames.push_back(std::move(src));

    FrontThread::GetSingleton()->close();

    log::i() << "decode thread exit";
}
}  // namespace share_screen
}  // namespace llc