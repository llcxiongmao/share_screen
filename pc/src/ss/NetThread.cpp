#include <ss/NetThread.hpp>
#include <ss/MainThread.hpp>
#include <ss/DecodeThread.hpp>

namespace ss {
NetThread::NetThread() {
    for (int i = 0; i < NET_FRAME_POOL_CAPACITY; ++i) {
        mNetFramePool[i].emplace();
        mFreeNetFrames.push_back(&*mNetFramePool[i]);
    }

    mLoop.emplace();

    check_libuv(
        uv_async_init(
            &*mLoop,
            &mAsyncClose,
            [](uv_async_t* handle) { NetThread::Singleton()->onAsyncClose(handle); }),
        "uv_async_init");

    check_libuv(
        uv_async_init(
            &*mLoop,
            &mAsyncRecycle,
            [](uv_async_t* handle) { NetThread::Singleton()->onAsyncRecycle(handle); }),
        "uv_async_init");

    try {
        mThread.emplace([]() { NetThread::Singleton()->run(); });
    } catch (std::exception& e) {
        SS_THROW(0, "create thread fail: %s", e.what());
    }
}

void NetThread::onAsyncRecycle(uv_async_t* handle) {
    (void)handle;

    if (isStopped()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mCacheFreeNetFrameLock);
        for (const auto& i : mCacheFreeNetFrames) {
            mFreeNetFrames.push_back(i);
        }
        mCacheFreeNetFrames.clear();
    }

    if (!mCurrentFrame) {
        mCurrentFrame = obtainNetFrame();
        if (mCurrentFrame) {
            mReadStage = ReadStage::HEAD;
            mReadSize = 0;
            int r = uv_read_start(
                (uv_stream_t*)&mClient,
                [](uv_handle_t* handle, size_t suggestedSize, uv_buf_t* buf) {
                    NetThread::Singleton()->onReadAlloc(handle, suggestedSize, buf);
                },
                [](uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
                    NetThread::Singleton()->onRead(stream, nread, buf);
                });
            if (r != 0) {
                Log::E(
                    "'uv_read_start' fail: %s, at %s:%d",
                    uverror_tostring(r).c_str(),
                    __FILE__,
                    __LINE__);
                stop(true);
            }
        }
    }
}

void NetThread::onAsyncClose(uv_async_t* handle) {
    (void)handle;

    stop(true);
}

void NetThread::onBroadcastRead(
    uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const sockaddr* addr, unsigned flags) {
    (void)handle;
    (void)flags;

    if (isStopped()) {
        return;
    }

    if (nread == 4 && strncmp(buf->base, "1314", 4) == 0) {
        char remoteAddrStr[INET_ADDRSTRLEN] = {};
        uv_ip4_name((const sockaddr_in*)addr, remoteAddrStr, INET_ADDRSTRLEN);
        Log::I("get udp packet, remote address: %s", remoteAddrStr);
        mRemoteIp = remoteAddrStr;
        stop(false);
    }
}

void NetThread::onBroadcastTimer(uv_timer_t* handle) {
    (void)handle;

    if (isStopped()) {
        return;
    }

    static int c = 25;
    --c;
    if (c >= 0) {
        Log::I("broadcast receiving...");
    } else {
        Log::I("broadcast timeout");
        stop(true);
    }
}

void NetThread::step0() {
    if (Config::Singleton()->ip.empty()) {
        check_libuv(uv_udp_init(&*mLoop, &mBroadcastClient), "uv_udp_init");

        check_libuv(uv_timer_init(&*mLoop, &mBroadcastTimer), "uv_timer_init");

        sockaddr_in localAddr;
        uv_ip4_addr("0.0.0.0", Config::Singleton()->broadcastPort, &localAddr);
        check_libuv(
            uv_udp_bind(&mBroadcastClient, (const sockaddr*)&localAddr, UV_UDP_REUSEADDR),
            "uv_udp_bind");

        uv_udp_recv_start(
            &mBroadcastClient,
            [](uv_handle_t* handle, size_t suggestedSize, uv_buf_t* buf) {
                static char d[4] = {};
                buf->base = d;
                buf->len = 4;
            },
            [](uv_udp_t* handle,
               ssize_t nread,
               const uv_buf_t* buf,
               const sockaddr* addr,
               unsigned flags) {
                NetThread::Singleton()->onBroadcastRead(handle, nread, buf, addr, flags);
            });

        check_libuv(
            uv_timer_start(
                &mBroadcastTimer,
                [](uv_timer_t* handle) { NetThread::Singleton()->onBroadcastTimer(handle); },
                0,
                2000),
            "uv_timer_start");

        uv_run(&*mLoop, UV_RUN_DEFAULT);

        uv_close((uv_handle_t*)&mBroadcastClient, nullptr);
        uv_close((uv_handle_t*)&mBroadcastTimer, nullptr);
    } else {
        mRemoteIp = Config::Singleton()->ip;
    }

    if (mClose) {
        throw TagExit {};
    }
}

void NetThread::onConnect(uv_connect_t* req, int status) {
    (void)req;

    if (isStopped()) {
        return;
    }

    if (status == 0) {
        Log::I("connect success");
        stop(false);
    } else {
        Log::I("connect fail: %s", uverror_tostring(status).c_str());
        stop(true);
    }
}

void NetThread::onConnectTimer(uv_timer_t* handle) {
    (void)handle;

    if (isStopped()) {
        return;
    }

    static int c = 10;
    --c;
    if (c >= 0) {
        Log::I("connecting...");
    } else {
        Log::I("connect timeout");
        stop(true);
    }
}

void NetThread::step1() {
    Log::I("try to connect: %s", mRemoteIp.c_str());

    check_libuv(uv_tcp_init(&*mLoop, &mClient), "uv_tcp_init");

    check_libuv(uv_timer_init(&*mLoop, &mConnectTimer), "uv_timer_init");

    sockaddr_in localAddr = {};
    uv_ip4_addr("0.0.0.0", Config::Singleton()->port, &localAddr);
    check_libuv(uv_tcp_bind(&mClient, (const sockaddr*)&localAddr, 0), "uv_tcp_bind");

    sockaddr_in remoteAddr = {};
    uv_ip4_addr(mRemoteIp.c_str(), Config::Singleton()->port, &remoteAddr);

    check_libuv(
        uv_tcp_connect(
            &mConnectReq,
            &mClient,
            (const sockaddr*)&remoteAddr,
            [](uv_connect_t* req, int status) { NetThread::Singleton()->onConnect(req, status); }),
        "uv_tcp_connect");

    check_libuv(
        uv_timer_start(
            &mConnectTimer,
            [](uv_timer_t* handle) { NetThread::Singleton()->onConnectTimer(handle); },
            0,
            2000),
        "uv_timer_start");

    uv_run(&*mLoop, UV_RUN_DEFAULT);

    uv_close((uv_handle_t*)&mConnectTimer, nullptr);

    if (mClose) {
        throw TagExit {};
    }
}

void NetThread::onWriteTimer(uv_timer_t* handle) {
    (void)handle;

    if (isStopped()) {
        return;
    }

    int r = uv_write(
        &mWriteReq, (uv_stream_t*)&mClient, &WRITE_BUF, 1, [](uv_write_t* req, int status) {
            NetThread::Singleton()->onWrite(req, status);
        });
    if (r != 0) {
        Log::E("'uv_write' fail: %s, at %s:%d", uverror_tostring(r).c_str(), __FILE__, __LINE__);
        stop(true);
    }
}

void NetThread::onWrite(uv_write_t* req, int status) {
    (void)req;

    if (isStopped()) {
        return;
    }

    if (status != 0) {
        Log::E("net write fail: %s", uverror_tostring(status).c_str());
        stop(true);
        return;
    }

    if (Config::Singleton()->debugNet) {
        Log::I_STR("write 1 byte");
    }

    int r = uv_timer_start(
        (uv_timer_t*)&mWriteTimer,
        [](uv_timer_t* handle) { NetThread::Singleton()->onWriteTimer(handle); },
        2000,
        0);
    if (r != 0) {
        Log::E(
            "'uv_timer_start' fail: %s, at %s:%d", uverror_tostring(r).c_str(), __FILE__, __LINE__);
        stop(true);
    }
}

void NetThread::onReadAlloc(uv_handle_t* handle, size_t suggestedSize, uv_buf_t* buf) {
    (void)handle;
    (void)suggestedSize;
    assert(mCurrentFrame);

    if (mReadStage == ReadStage::HEAD) {
        buf->base = mHeader + mReadSize;
        buf->len = 12 - mReadSize;
    } else {
        buf->base = (char*)mCurrentFrame->body->data + mReadSize;
        buf->len = mCurrentFrame->body->size - mReadSize;
    }
}

void NetThread::onRead(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    (void)stream;

    if (isStopped()) {
        return;
    }

    if (nread == 0) {
        return;
    }

    if (nread < 0) {
        Log::E("net read fail: %s", uverror_tostring(nread).c_str());
        stop(true);
        return;
    }

    mReadSize += nread;
    if (mReadStage == ReadStage::HEAD) {
        assert(mReadSize <= 12);
        if (mReadSize == 12) {
            uint32_t size = GetJavaData<uint32_t>(mHeader);
            int64_t pts = GetJavaData<int64_t>(mHeader + 4);
            if (Config::Singleton()->debugNet) {
                Log::I("read header, body size: %u, pts: %lld", (unsigned)size, (long long)pts);
            }

            if (mCurrentFrame->body->buf &&
                av_buffer_get_ref_count(mCurrentFrame->body->buf) == 1 &&
                mCurrentFrame->body->buf->size >= size) {
                // we can reuse buffer:
                // - if buffer not null.
                // - if ref_count == 1, no use by outside.
                // - if buffer capacity big than required size.
                AVBufferRef* tmp = av_buffer_ref(mCurrentFrame->body->buf);
                av_packet_unref(mCurrentFrame->body);
                mCurrentFrame->body->buf = tmp;
                mCurrentFrame->body->data = tmp->data;
                mCurrentFrame->body->size = size;
            } else {
                av_packet_unref(mCurrentFrame->body);
                av_new_packet(mCurrentFrame->body, size);
            }

            mCurrentFrame->pts = pts;
            mReadStage = ReadStage::BODY;
            mReadSize = 0;
        }
    } else {
        assert(mReadSize <= mCurrentFrame->body->size);
        if (mReadSize == mCurrentFrame->body->size) {
            if (Config::Singleton()->debugNet) {
                Log::I("read body, pts: %lld", (long long)mCurrentFrame->pts);
            }

            DecodeThread::Singleton()->notifyDecodeFrame(mCurrentFrame);

            mCurrentFrame = obtainNetFrame();
            if (mCurrentFrame) {
                mReadStage = ReadStage::HEAD;
                mReadSize = 0;
            } else {
                uv_read_stop((uv_stream_t*)&mClient);
            }
        }
    }
}

void NetThread::step2() {
    check_libuv(uv_timer_init(&*mLoop, &mWriteTimer), "uv_timer_init");

    mReadStage = ReadStage::HEAD;
    mReadSize = 0;
    mCurrentFrame = obtainNetFrame();
    check_libuv(
        uv_read_start(
            (uv_stream_t*)&mClient,
            [](uv_handle_t* handle, size_t suggestedSize, uv_buf_t* buf) {
                NetThread::Singleton()->onReadAlloc(handle, suggestedSize, buf);
            },
            [](uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
                NetThread::Singleton()->onRead(stream, nread, buf);
            }),
        "uv_read_start");

    check_libuv(
        uv_write(
            &mWriteReq,
            (uv_stream_t*)&mClient,
            &WRITE_BUF,
            1,
            [](uv_write_t* req, int status) { NetThread::Singleton()->onWrite(req, status); }),
        "uv_write");

    uv_run(&*mLoop, UV_RUN_DEFAULT);
}

void NetThread::run() {
    try {
        step0();
        step1();
        step2();
    } catch (const TagExit&) {  //
    } catch (const Error& e) {
        Log::PrintError(e);
    } catch (const std::exception& e) {
        Log::E("catch %s: %s, %s#%d", typeid(e).name(), e.what(), __FILE__, __LINE__);
    }

    MainThread::Singleton()->notifyClose();

    Log::I_STR("net thread exit");
}
}  // namespace ss