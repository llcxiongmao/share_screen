#pragma once
#include <ss/Common.hpp>

namespace ss {
/** net thread, handle net read/write. */
class NetThread : public xm::SingletonBase<NetThread> {
public:
    NetThread();

    void notifyRecycleNetFrame(NetFrame* netFrame) {
        {
            std::lock_guard<std::mutex> lock(mCacheFreeNetFrameLock);
            mCacheFreeNetFrames.push_back(netFrame);
        }
        uv_async_send(&mAsyncRecycle);
    }

    void notifyClose() {
        uv_async_send(&mAsyncClose);
    }

    void join() {
        mThread->join();
    }

private:
    enum : uint8_t {
        NET_FRAME_POOL_CAPACITY = 20,
    };

    enum class ReadStage {
        HEAD,
        BODY,
    };

    struct TagExit {};

    struct RaiiUvLoop : xm::NonCopyable, uv_loop_t {
        RaiiUvLoop() {
            check_libuv(uv_loop_init(this), "uv_loop_init");
        }

        ~RaiiUvLoop() {
            uv_walk(
                this,  //
                [](uv_handle_t* handle, void* arg) {
                    (void)arg;
                    if (uv_is_closing(handle) == 0) {
                        uv_close(handle, nullptr);
                    }
                },
                nullptr);
            int res = 0;
            res = uv_run(this, UV_RUN_DEFAULT);
            assert(res == 0);
            res = uv_loop_close(this);
            assert(res == 0);
        }
    };

    /** java data is big endian, we need check and convert. */
    template <typename T>
    static T GetJavaData(const char* p) {
#if defined(SS_IS_LITTLE)
        alignas(T) char tmp[sizeof(T)];
        for (int i = 0, e = sizeof(T); i < e; ++i) {
            tmp[i] = p[e - i - 1];
        }
        return *(const T*)tmp;
#else
        return *(const T*)p;
#endif
    }

    NetFrame* obtainNetFrame() {
        if (mFreeNetFrames.empty()) {
            return nullptr;
        } else {
            NetFrame* netFrame = mFreeNetFrames.back();
            mFreeNetFrames.pop_back();
            return netFrame;
        }
    }

    void stop(bool setClose) {
        if (setClose) {
            mClose = true;
        }
        uv_stop(&*mLoop);
    }

    bool isStopped() {
        return mLoop->stop_flag == 1;
    }

    void onAsyncRecycle(uv_async_t* handle);

    void onAsyncClose(uv_async_t* handle);

    void onBroadcastRead(
        uv_udp_t* handle,
        ssize_t nread,
        const uv_buf_t* buf,
        const sockaddr* addr,
        unsigned flags);

    void onBroadcastTimer(uv_timer_t* handle);

    void step0();

    void onConnect(uv_connect_t* req, int status);

    void onConnectTimer(uv_timer_t* handle);

    void step1();

    void onWriteTimer(uv_timer_t* handle);

    void onWrite(uv_write_t* req, int status);

    void onReadAlloc(uv_handle_t* handle, size_t suggestedSize, uv_buf_t* buf);

    void onRead(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);

    void step2();

    void run();

    // ----

    bool mClose = false;

    std::optional<NetFrame> mNetFramePool[NET_FRAME_POOL_CAPACITY];
    xm::Array<NetFrame*> mFreeNetFrames;
    std::mutex mCacheFreeNetFrameLock;
    xm::Array<NetFrame*> mCacheFreeNetFrames;  // guard by mCacheFreeNetFrameLock.

    std::string mRemoteIp;

    const uv_buf_t WRITE_BUF = uv_buf_init((char*)"a", 1);
    /** first 4 bytes for packet size, after 8 bytes for pts. */
    char mHeader[12] = {};
    ReadStage mReadStage = ReadStage::HEAD;
    ssize_t mReadSize = 0;
    NetFrame* mCurrentFrame = nullptr;

	uv_connect_t mConnectReq = {};
    uv_write_t mWriteReq = {};
    uv_async_t mAsyncRecycle = {};
    uv_async_t mAsyncClose = {};
    uv_udp_t mBroadcastClient = {};
    uv_timer_t mBroadcastTimer = {};
    uv_timer_t mConnectTimer = {};
    uv_timer_t mWriteTimer = {};
    uv_tcp_t mClient = {};
    std::optional<RaiiUvLoop> mLoop;

    std::optional<std::thread> mThread;
};
}  // namespace ss