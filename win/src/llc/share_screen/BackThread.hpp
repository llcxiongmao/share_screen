#pragma once
#include <llc/share_screen/common.hpp>

namespace llc {
namespace share_screen {
/** back thread, handle net read/write. */
class BackThread : public util::Singleton<BackThread> {
public:
    /** create and start thread, throw Error if fail. */
    BackThread();

    /**
     * notify recycle net frame, take frame ownership if success, otherwise throw Error.
     *
     * @param frame frame to recycle.
     * @throws Error if this closed, see #close.
     */
    void notifyRecycleNetFrame(std::unique_ptr<NetFrame>& frame) {
        std::lock_guard<std::mutex> lock(mCloseLock);
        THROW_IF(!mIsClose, "back thread already closed");
        frame->recycling = true;
        uv_async_send(frame->pAsync);
        frame.release();
    }

    /** close thread, after call notifyRecycleNetFrame will throw Error. */
    void close() {
        std::lock_guard<std::mutex> lock(mCloseLock);
        if (!mIsClose) {
            mIsClose = true;
            uv_async_send(pClose);
        }
    }

    /** join thread. */
    void join() {
        mThread->join();
    }

private:
    /** unit test simulate options. */
    static constexpr bool UT_FAIL_CREATE = false;
    static constexpr bool UT_FAIL_INIT_RESOURCES = false;
    static constexpr bool UT_FAIL_RECYCLE_NET_FRAME = false;
    static constexpr bool UT_FAIL_WRITE_TIMER = false;
    static constexpr bool UT_FAIL_BROADCAST_READ = false;
    static constexpr bool UT_FAIL_CONNECT = false;
    static constexpr bool UT_FAIL_READ = false;
    static constexpr bool UT_FAIL_WRITE = false;

    enum class ReadStage {
        HEAD,
        BODY,
    };

    typedef std::chrono::high_resolution_clock clock;

    static constexpr int NET_FRAME_POOL_SIZE = 20;

    static const uv_buf_t WRITE_BUF;

    /**
     * java data is big endian, but most local environment is little endian,
     * so we need check and convert.
     *
     * @param p big endia data.
     * @return return local value.
     */
    template <typename T>
    static T GetJavaData(const char* p) {
        constexpr bool isLittleEndian = 'ABCD' == 0x41424344UL;
        union {
            T t;
            char c[1];
        };
        if (isLittleEndian) {
            for (int i = 0, e = sizeof(T); i < e; ++i) {
                c[i] = p[e - i - 1];
            }
        } else {
            memcpy(c, p, sizeof(T));
        }
        return t;
    }

    std::unique_ptr<NetFrame> obtainNetFrame();

    void stop();

    void readAlloc(uv_handle_t* handle, size_t suggestedSize, uv_buf_t* buf);

    void onRecycleNetFrame(uv_async_t* handle);

    void onClose(uv_async_t* handle);

    void onConnectTimeout(uv_timer_t* handle);

    void onWriteTimer(uv_timer_t* handle);

    void onBroadcastRead(
        uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const sockaddr* addr, unsigned flags);

    void onConnect(uv_connect_t* req, int status);

    void onRead(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);

    void onWrite(uv_write_t* req, int status);

    void initResources();

    void run();

private:
    std::vector<std::unique_ptr<NetFrame>> mFreeNetFrames;
    std::mutex mCloseLock;
    bool mIsClose = false;

    bool mIsStop = false;

    uv_connect_t mConnectReq = {};
    uv_write_t mWriteReq = {};

    uv_loop_t mLoop = {};
    uv_async_t mClose = {};
    uv_timer_t mConnectTimeout = {};
    uv_timer_t mWriteTimer = {};
    uv_udp_t mBroadcastSocket = {};
    uv_tcp_t mClientSocket = {};
    /** use for recycle netframe. */
    uv_async_t mNetFrameAsyncs[NET_FRAME_POOL_SIZE] = {};

    uv_loop_t* pLoop = nullptr;
    uv_async_t* pClose = nullptr;
    uv_timer_t* pConnectTimeout = nullptr;
    uv_timer_t* pWriteTimer = nullptr;
    uv_udp_t* pBroadcastSocket = nullptr;
    uv_tcp_t* pClientSocket = nullptr;
    uv_async_t* pNetFrameAsyncs[NET_FRAME_POOL_SIZE] = {};

    /** first 4 bytes for packet size, after 8 bytes for pts. */
    char mHeader[12];
    ReadStage mReadStage = ReadStage::HEAD;
    ssize_t mReadSize = 0;
    /** current read frame. */
    std::unique_ptr<NetFrame> mCurrentFrame;

    std::unique_ptr<std::thread> mThread;
};
}  // namespace share_screen
}  // namespace llc
