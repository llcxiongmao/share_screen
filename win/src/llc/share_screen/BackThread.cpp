#include <llc/share_screen/BackThread.hpp>
#include <llc/share_screen/FrontThread.hpp>
#include <llc/share_screen/PtsThread.hpp>
#include <llc/share_screen/DecodeThread.hpp>

#define CLOSE_UV_HANDLE(_P, _CB)         \
    if (_P) {                            \
        uv_close((uv_handle_t*)_P, _CB); \
        _P = nullptr;                    \
    }

namespace llc {
namespace share_screen {
using namespace std::chrono;

const uv_buf_t BackThread::WRITE_BUF = {1, "a"};

BackThread::BackThread() {
    try {
        THROW_IF_UV(uv_loop_init(&mLoop));
        pLoop = &mLoop;

        THROW_IF_UV(uv_async_init(pLoop, &mClose, [](uv_async_t* handle) {
            BackThread::GetSingleton()->onClose(handle);
        }));
        pClose = &mClose;

        THROW_IF(!UT_FAIL_CREATE, "unit test");
    } catch (const std::exception& e) {
        if (pLoop) {
            CLOSE_UV_HANDLE(pClose, nullptr);
            int uvRes = 0;
            uvRes = uv_run(pLoop, UV_RUN_DEFAULT);
            assert(!uvRes);
            uvRes = uv_loop_close(pLoop);
            assert(!uvRes);
        }
        throw Error() << "back thread create fail, " << e.what();
    }

    mThread = std::make_unique<std::thread>([this]() { run(); });
}

std::unique_ptr<NetFrame> BackThread::obtainNetFrame() {
    if (mFreeNetFrames.empty())
        return nullptr;
    std::unique_ptr<NetFrame> frame = std::move(mFreeNetFrames.back());
    mFreeNetFrames.pop_back();
    return frame;
}

void BackThread::stop() {
    if (!mIsStop) {
        uv_stop(pLoop);
        mIsStop = true;
    }
}

void BackThread::readAlloc(uv_handle_t* handle, size_t suggestedSize, uv_buf_t* buf) {
    assert(mCurrentFrame);

    if (mReadStage == ReadStage::HEAD) {
        buf->base = mHeader + mReadSize;
        buf->len = 12 - mReadSize;
    } else {
        buf->base = (char*)mCurrentFrame->body->data + mReadSize;
        buf->len = mCurrentFrame->body->size - mReadSize;
    }
}

void BackThread::onRecycleNetFrame(uv_async_t* handle) {
    std::unique_ptr<NetFrame> frame((NetFrame*)handle->data);
    frame->recycling = false;
    av_packet_unref(frame->body.get());
    if (mIsStop || mCurrentFrame) {
        mFreeNetFrames.push_back(std::move(frame));
        return;
    }

    try {
        mCurrentFrame = std::move(frame);
        mReadStage = ReadStage::HEAD;
        mReadSize = 0;
        if (Config::GetSingleton()->debugPrintReadStartStop)
            log::i() << "start read";
        THROW_IF_UV(uv_read_start((uv_stream_t*)pClientSocket,
                                  [](uv_handle_t* handle, size_t suggestedSize, uv_buf_t* buf) {
                                      BackThread::GetSingleton()->readAlloc(
                                          handle, suggestedSize, buf);
                                  },
                                  [](uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
                                      BackThread::GetSingleton()->onRead(stream, nread, buf);
                                  }));

        THROW_IF(!UT_FAIL_RECYCLE_NET_FRAME, "unit test");
    } catch (const std::exception& e) {
        log::e() << "back thread recycle net frame fail, " << e.what();
        stop();
    }
}

void BackThread::onClose(uv_async_t* handle) {
    stop();
}

void BackThread::onConnectTimeout(uv_timer_t* handle) {
    if (mIsStop)
        return;

    if (pClientSocket && pClientSocket->data) {
        CLOSE_UV_HANDLE(pConnectTimeout, nullptr);
        return;
    }

    log::e() << "connect timeout";
    stop();
}

void BackThread::onWriteTimer(uv_timer_t* handle) {
    if (mIsStop)
        return;

    try {
        THROW_IF_UV(uv_write(
            &mWriteReq,
            (uv_stream_t*)pClientSocket,
            &WRITE_BUF,
            1,
            [](uv_write_t* req, int status) { BackThread::GetSingleton()->onWrite(req, status); }));

        THROW_IF(!UT_FAIL_WRITE_TIMER, "unit test");
    } catch (const std::exception& e) {
        log::e() << "back thread write fail, " << e.what();
        stop();
    }
}

void BackThread::onBroadcastRead(
    uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const sockaddr* addr, unsigned flags) {
    if (mIsStop)
        return;

    try {
        THROW_IF(nread >= 0, Error::GetUvString(nread));

        if (nread == 4 && strncmp(buf->base, "1314", 4) == 0) {
            // close broadcast read anyway.
            CLOSE_UV_HANDLE(pBroadcastSocket, nullptr);

            char remoteAddrStr[INET_ADDRSTRLEN] = {};
            uv_ip4_name((const sockaddr_in*)addr, remoteAddrStr, INET_ADDRSTRLEN);
            log::i() << "get udp packet, remote address: " << remoteAddrStr
                     << ", try to connect it";

            THROW_IF_UV(uv_tcp_init(pLoop, &mClientSocket));
            pClientSocket = &mClientSocket;
            sockaddr_in localAddr = {};
            uv_ip4_addr("0.0.0.0", Config::GetSingleton()->port, &localAddr);
            THROW_IF_UV(uv_tcp_bind(pClientSocket, (const sockaddr*)&localAddr, 0));
            sockaddr_in remoteAddr = {};
            uv_ip4_addr(remoteAddrStr, Config::GetSingleton()->port, &remoteAddr);
            THROW_IF_UV(uv_tcp_connect(&mConnectReq,
                                       pClientSocket,
                                       (const sockaddr*)&remoteAddr,
                                       [](uv_connect_t* req, int status) {
                                           BackThread::GetSingleton()->onConnect(req, status);
                                       }));
        }

        THROW_IF(!UT_FAIL_BROADCAST_READ, "unit test");
    } catch (const std::exception& e) {
        log::e() << "back thread broadcast read fail, " << e.what();
        stop();
    }
}

void BackThread::onConnect(uv_connect_t* req, int status) {
    if (mIsStop)
        return;

    try {
        THROW_IF(!status, Error::GetUvString(status));

        log::i() << "connect success";

        // flag mean connect success.
        pClientSocket->data = (void*)0x1;

        mCurrentFrame = obtainNetFrame();
        mReadStage = ReadStage::HEAD;
        mReadSize = 0;
        THROW_IF_UV(uv_read_start((uv_stream_t*)pClientSocket,
                                  [](uv_handle_t* handle, size_t suggestedSize, uv_buf_t* buf) {
                                      BackThread::GetSingleton()->readAlloc(
                                          handle, suggestedSize, buf);
                                  },
                                  [](uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
                                      BackThread::GetSingleton()->onRead(stream, nread, buf);
                                  }));
        THROW_IF_UV(uv_write(
            &mWriteReq,
            (uv_stream_t*)pClientSocket,
            &WRITE_BUF,
            1,
            [](uv_write_t* req, int status) { BackThread::GetSingleton()->onWrite(req, status); }));

        THROW_IF(!UT_FAIL_CONNECT, "unit test");
    } catch (const std::exception& e) {
        log::e() << "back thread connect fail, " << e.what();
        stop();
    }
}

void BackThread::onRead(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    if (mIsStop)
        return;

    try {
        if (nread == 0)
            return;

        THROW_IF(nread >= 0, Error::GetUvString(nread));

        mReadSize += nread;
        if (mReadStage == ReadStage::HEAD) {
            assert(mReadSize <= 12);
            if (mReadSize == 12) {
                uint32_t size = GetJavaData<uint32_t>(mHeader);
                int64_t pts = GetJavaData<int64_t>(mHeader + 4);
                if (Config::GetSingleton()->debugPrintNet)
                    log::i() << "read header, body size: " << size << ", pts: " << pts;
                THROW_IF_AV(av_new_packet(mCurrentFrame->body.get(), size));
                mCurrentFrame->pts = pts;
                mReadStage = ReadStage::BODY;
                mReadSize = 0;
            }
        } else {
            assert(mReadSize <= mCurrentFrame->body->size);
            if (mReadSize == mCurrentFrame->body->size) {
                if (Config::GetSingleton()->debugPrintNet)
                    log::i() << "read body, pts: " << mCurrentFrame->pts;

                DecodeThread::GetSingleton()->notifyDecodeFrame(mCurrentFrame);

                mCurrentFrame = obtainNetFrame();
                if (mCurrentFrame) {
                    mReadStage = ReadStage::HEAD;
                    mReadSize = 0;
                } else {
                    if (Config::GetSingleton()->debugPrintReadStartStop)
                        log::i() << "stop read";
                    THROW_IF_UV(uv_read_stop((uv_stream_t*)pClientSocket));
                }
            }
        }

        THROW_IF(!UT_FAIL_READ, "unit test");
    } catch (const std::exception& e) {
        log::e() << "back thread read fail, " << e.what();
        stop();
    }
}

void BackThread::onWrite(uv_write_t* req, int status) {
    if (mIsStop)
        return;

    try {
        THROW_IF(!status, Error::GetUvString(status));

        if (Config::GetSingleton()->debugPrintNet)
            log::i() << "write 1 byte";
        THROW_IF_UV(uv_timer_start(
            pWriteTimer,
            [](uv_timer_t* handle) { BackThread::GetSingleton()->onWriteTimer(handle); },
            2000,
            0));

        THROW_IF(!UT_FAIL_WRITE, "unit test");
    } catch (const std::exception& e) {
        log::e() << "back thread write fail, " << e.what();
        stop();
    }
}

void BackThread::initResources() {
    for (int i = 0; i < NET_FRAME_POOL_SIZE; ++i) {
        THROW_IF_UV(uv_async_init(pLoop, &mNetFrameAsyncs[i], [](uv_async_t* handle) {
            BackThread::GetSingleton()->onRecycleNetFrame(handle);
        }));
        pNetFrameAsyncs[i] = &mNetFrameAsyncs[i];
    }

    for (int i = 0; i < NET_FRAME_POOL_SIZE; ++i) {
        mFreeNetFrames.push_back(std::make_unique<NetFrame>(pNetFrameAsyncs[i]));
    }

    THROW_IF_UV(uv_timer_init(pLoop, &mConnectTimeout));
    pConnectTimeout = &mConnectTimeout;

    THROW_IF_UV(uv_timer_init(pLoop, &mWriteTimer));
    pWriteTimer = &mWriteTimer;

    if (Config::GetSingleton()->ip.empty()) {
        log::i() << "wating, broadcast receiving";

        THROW_IF_UV(uv_udp_init(pLoop, &mBroadcastSocket));
        pBroadcastSocket = &mBroadcastSocket;
        sockaddr_in broadcastRecvAddr = {};
        uv_ip4_addr("0.0.0.0", Config::GetSingleton()->broadcastPort, &broadcastRecvAddr);
        THROW_IF_UV(
            uv_udp_bind(pBroadcastSocket, (const sockaddr*)&broadcastRecvAddr, UV_UDP_REUSEADDR));
        THROW_IF_UV(uv_udp_recv_start(
            pBroadcastSocket,
            [](uv_handle_t* handle, size_t suggestedSize, uv_buf_t* buf) {
                static char d[5] = {};
                buf->base = d;
                buf->len = 5;
            },
            [](uv_udp_t* handle,
               ssize_t nread,
               const uv_buf_t* buf,
               const sockaddr* addr,
               unsigned flags) {
                BackThread::GetSingleton()->onBroadcastRead(handle, nread, buf, addr, flags);
            }));
    } else {
        log::i() << "wating, direct connecting";

        THROW_IF_UV(uv_tcp_init(pLoop, &mClientSocket));
        pClientSocket = &mClientSocket;
        sockaddr_in localAddr = {};
        uv_ip4_addr("0.0.0.0", Config::GetSingleton()->port, &localAddr);
        THROW_IF_UV(uv_tcp_bind(pClientSocket, (const sockaddr*)&localAddr, 0));
        sockaddr_in remoteAddr = {};
        uv_ip4_addr(Config::GetSingleton()->ip.c_str(), Config::GetSingleton()->port, &remoteAddr);
        THROW_IF_UV(uv_tcp_connect(&mConnectReq,
                                   pClientSocket,
                                   (const sockaddr*)&remoteAddr,
                                   [](uv_connect_t* req, int status) {
                                       BackThread::GetSingleton()->onConnect(req, status);
                                   }));
    }

    THROW_IF_UV(uv_timer_start(
        pConnectTimeout,
        [](uv_timer_t* handle) { BackThread::GetSingleton()->onConnectTimeout(handle); },
        30000,
        0));

    THROW_IF(!UT_FAIL_INIT_RESOURCES, "unit test");
}

void BackThread::run() {
    log::i() << "back thread run";

    try {
        initResources();
    } catch (const std::exception& e) {
        log::e() << "back thread init resources fail, " << e.what();
        stop();
    }

    uv_run(pLoop, UV_RUN_DEFAULT);

    close();

    {
        std::lock_guard<std::mutex> lock(mCloseLock);
        mIsClose = true;
        CLOSE_UV_HANDLE(pClose, nullptr);
        CLOSE_UV_HANDLE(pConnectTimeout, nullptr);
        CLOSE_UV_HANDLE(pWriteTimer, nullptr);
        CLOSE_UV_HANDLE(pBroadcastSocket, nullptr);
        CLOSE_UV_HANDLE(pClientSocket, nullptr);
        for (int i = 0; i < NET_FRAME_POOL_SIZE; ++i) {
            CLOSE_UV_HANDLE(pNetFrameAsyncs[i], [](uv_handle_t* handle) {
                NetFrame* frame = (NetFrame*)handle->data;
                // if frame is recycling, we should recycle it at close, avoid leak.
                if (frame->recycling) {
                    BackThread::GetSingleton()->onRecycleNetFrame((uv_async_t*)handle);
                }
            });
        }
    }

    int uvRes = uv_run(pLoop, UV_RUN_DEFAULT);
    assert(!uvRes);
    uvRes = uv_loop_close(pLoop);
    assert(!uvRes);

    FrontThread::GetSingleton()->close();

    log::i() << "back thread exit";
}
}  // namespace share_screen
}  // namespace llc