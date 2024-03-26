#include <ss/DecodeThread.hpp>
#include <ss/MainThread.hpp>
#include <ss/PtsThread.hpp>
#include <ss/NetThread.hpp>

namespace ss {
DecodeThread::DecodeThread() {
    av_log_set_callback(&Log::AvLogCallback);

    try {
        mThread.emplace([this]() { run(); });
    } catch (const std::exception& e) {
        SS_THROW(0, "create thread fail: %s", e.what());
    }
}

void DecodeThread::run() {
    struct TagExit {};
    using clock = chrono::high_resolution_clock;

    Log::I_STR("decode thread run");

    try {
        mCodec = avcodec_find_decoder(AV_CODEC_ID_H264);
        SS_THROW(mCodec, "find h264 decoder fail");
        mCodecCtx.reset(avcodec_alloc_context3(mCodec));
        SS_THROW(mCodecCtx, "'avcodec_alloc_context3' fail");
        check_libav(avcodec_open2(mCodecCtx.get(), mCodec, NULL), "avcodec_open2");

        for (int i = 0; i < PAINT_FRAME_POOL_CAPACITY; ++i) {
            mPaintFramePool[i].emplace();
            mFreePaintFrames.push_back(&*mPaintFramePool[i]);
        }

        // when pts == -1 the frame not output image(it only contain config information),
        // we cache it until image frame come in, then merge together to decode.
        struct AVPacketDeleter {
            void operator()(AVPacket* p) const {
                av_packet_free(&p);
            }
        };
        std::unique_ptr<AVPacket, AVPacketDeleter> cachePacket;
        cachePacket.reset(av_packet_alloc());
        SS_THROW(cachePacket, "av_packet_alloc fail");

        clock::time_point now0;
        clock::time_point now1;
        NetFrame* src = nullptr;
        PaintFrame* dst = nullptr;
        while (!mClose) {
            if (!src) {
                std::optional<NetFrame*> tmp =
                    mPendingNetFrames.pop_front(std::chrono::milliseconds(2000));
                if (!tmp) {
                    continue;
                } else {
                    src = *tmp;
                    if (!src) {
                        throw TagExit {};
                    }
                }
            }

            if (!dst) {
                std::optional<PaintFrame*> tmp =
                    mFreePaintFrames.pop_front(std::chrono::milliseconds(2000));
                if (!tmp) {
                    continue;
                } else {
                    dst = *tmp;
                    if (!dst) {
                        throw TagExit {};
                    }
                }
            }

            dst->pts = src->pts;

            AVPacket* packet = nullptr;
            // check whether need merge to cache packet.
            if (cachePacket->size || dst->pts == -1) {
                int offset = cachePacket->size;
                int growSize = src->body->size;
                uint8_t* growData = src->body->data;
                av_grow_packet(cachePacket.get(), growSize);
                ::memcpy(cachePacket->data + offset, growData, growSize);
                packet = cachePacket.get();
            } else {
                packet = src->body;
            }

            if (dst->pts == -1) {
                notifyRecyclePaintFrame(dst);
            } else {
                if (Config::Singleton()->debugDecode) {
                    now0 = clock::now();
                }

                check_libav(avcodec_send_packet(mCodecCtx.get(), packet), "avcodec_send_packet");
                check_libav(
                    avcodec_receive_frame(mCodecCtx.get(), dst->decodeFrame),
                    "avcodec_receive_frame");

                SS_THROW(!UTSO_FAIL_DECODE, "unit test simulate");

                if (Config::Singleton()->debugDecode) {
                    now1 = clock::now();
                    Log::I(
                        "decode time: %lldms, pts: %lld, key frame: %s",
                        (long long)chrono::duration_cast<chrono::milliseconds>(now1 - now0).count(),
                        (long long)dst->pts,
                        dst->decodeFrame->key_frame ? "true" : "false");
                }
                av_packet_unref(cachePacket.get());
                if (PtsThread::Singleton()) {
                    PtsThread::Singleton()->notifySyncFrame(dst);
                } else {
                    MainThread::Singleton()->notifyPaintFrame(dst);
                }
            }

            NetThread::Singleton()->notifyRecycleNetFrame(src);

            src = nullptr;
            dst = nullptr;
        }
    } catch (const TagExit&) {  //
    } catch (const Error& e) {
        Log::PrintError(e);
    } catch (const std::exception& e) {
        Log::E("catch %s: %s, %s#%d", typeid(e).name(), e.what(), __FILE__, __LINE__);
    }

    MainThread::Singleton()->notifyClose();

    Log::I("decode thread exit");
}
}  // namespace ss