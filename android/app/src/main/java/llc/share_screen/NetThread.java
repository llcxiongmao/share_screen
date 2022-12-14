package llc.share_screen;

import java.io.Closeable;
import java.io.IOException;
import java.net.Inet6Address;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.NetworkInterface;
import java.nio.ByteBuffer;
import java.nio.channels.DatagramChannel;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import java.nio.charset.StandardCharsets;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.Iterator;
import java.util.List;
import java.util.Queue;
import java.util.Set;

/** net thread, handle net broadcast/read/write. */
public class NetThread {
    /** unit test simulate options. */
    private static final boolean UT_FAIL_CREATE = false;
    private static final boolean UT_FAIL_INIT_RESOURCES = false;
    private static final boolean UT_FAIL_READ = false;
    private static final boolean UT_FAIL_WRITE = false;
    private static final boolean UT_FAIL_ACCEPT = false;

    private static NetThread sSingleton;

    public static NetThread GetSingleton() {
        return sSingleton;
    }

    public static void SetSingleton(NetThread obj) {
        sSingleton = obj;
    }

    public static void ReleaseSingleton() {
        sSingleton = null;
    }

    private final ByteBuffer mClientReadBuffer = ByteBuffer.allocate(1);
    private final ByteBuffer mBroadcastData = ByteBuffer.allocate(4);

    private final Object mCloseLock = new Object();
    private boolean mIsClose = false;

    Thread mThread;

    /** pending frames to write. */
    private final Queue<Frame> mPendingFrames = new ArrayDeque<>();
    /** current frame to write. */
    private Frame mCurrentFrame = null;

    /**
     * broadcast counter, we max broadcast 45s, if still no accept any client,
     * trigger timeout.
     */
    private int mBroadcastCounter = 30;

    private Selector mSelector;
    private InetSocketAddress mBroadcastAddress;
    private final List<SelectionKey> mBroadcastKeys = new ArrayList<>();
    private final List<SelectionKey> mAcceptKeys = new ArrayList<>();
    private SelectionKey mClientKey;

    /** create and start thread. */
    NetThread() throws Error {
        if (sSingleton != null)
            throw new AssertionError("bug");

        try {
            mSelector = Selector.open();
            if (UT_FAIL_CREATE)
                throw new IOException("unit test");
        } catch (Exception e) {
            throw new Error("net thread create fail, " + e.getMessage());
        }

        mThread = new Thread(this::run);
        mThread.start();
    }

    /**
     * notify write frame.
     *
     * @param frame frame to write.
     * @throws Error if this closed, see {@link #close}.
     */
    public void notifyWriteFrame(Frame frame) throws Error {
        synchronized (mCloseLock) {
            if (mIsClose)
                throw new Error("net thread already close");
            synchronized (mPendingFrames) {
                mPendingFrames.offer(frame);
            }
        }
        mSelector.wakeup();
    }

    /** close thread, after call notifyWriteFrame will throw Error. */
    public void close() {
        synchronized (mCloseLock) {
            if (!mIsClose) {
                mIsClose = true;
                mThread.interrupt();
            }
        }
    }

    /**
     * join this thread.
     *
     * @return return the rest of pending frame count.
     */
    public int join() {
        try {
            mThread.join();
        } catch (Exception e) {
            throw new AssertionError("wtf");
        }
        int rest = mPendingFrames.size();
        if (mCurrentFrame != null)
            ++rest;
        return rest;
    }

    private void run() {
        FrontThread.GetSingleton().notifyInfoLog("net thread run");

        try {
            initResources();
        } catch (Exception e) {
            FrontThread.GetSingleton().notifyErrLog("net thread init resources fail, "
                                                    + e.getMessage());
            close();
        }

        // 1.5 second.
        long _1_5_s = 1500;
        long nowTp = System.currentTimeMillis();
        long endTp = nowTp + _1_5_s;
        while (!mThread.isInterrupted()) {
            try {
                if (mClientKey == null) {
                    nowTp = System.currentTimeMillis();
                    if (nowTp - endTp >= 0) {
                        if (mBroadcastCounter < 1) {
                            throw new Error("accept timeout");
                        }
                        for (SelectionKey k : mBroadcastKeys) {
                            k.interestOps(SelectionKey.OP_WRITE);
                        }
                        --mBroadcastCounter;
                        endTp = nowTp + _1_5_s;
                    }
                    mSelector.select(endTp - nowTp);
                } else {
                    mSelector.select();
                    if (mCurrentFrame == null) {
                        mCurrentFrame = popFrame();
                        if (mCurrentFrame != null) {
                            if (Config.GetSingleton().debug_print_write_start_stop)
                                FrontThread.GetSingleton().notifyInfoLog("start write");
                            mClientKey.interestOps(SelectionKey.OP_WRITE | SelectionKey.OP_READ);
                        }
                    }
                }

                Set<SelectionKey> keys = mSelector.selectedKeys();
                Iterator<SelectionKey> itr = keys.iterator();
                while (itr.hasNext()) {
                    SelectionKey k = itr.next();
                    if (k.isReadable()) {
                        if (k == mClientKey)
                            onClientRead();
                    }
                    if (k.isWritable()) {
                        if (k == mClientKey)
                            onClientWrite();
                        else
                            onBroadcast(k);
                    }
                    if (k.isAcceptable()) {
                        onAccept(k);
                    }
                    itr.remove();
                }
            } catch (Exception e) {
                FrontThread.GetSingleton().notifyErrLog(e.getMessage());
                close();
            }
        }

        // release all system resource.
        for (SelectionKey k : mBroadcastKeys) {
            QuietClose(k.channel());
        }
        mBroadcastKeys.clear();
        for (SelectionKey k : mAcceptKeys) {
            QuietClose(k.channel());
        }
        mAcceptKeys.clear();
        if (mClientKey != null) {
            QuietClose(mClientKey.channel());
            mClientKey = null;
        }
        QuietClose(mSelector);
        mSelector = null;

        FrontThread.GetSingleton().close();

        FrontThread.GetSingleton().notifyInfoLog("net thread exit");
    }

    private void initResources() throws IOException, Error {
        mBroadcastData.put("1314".getBytes(StandardCharsets.UTF_8));
        mBroadcastAddress =
            new InetSocketAddress("255.255.255.255", Config.GetSingleton().broadcast_port);

        Enumeration<NetworkInterface> faceItr = NetworkInterface.getNetworkInterfaces();
        while (faceItr.hasMoreElements()) {
            NetworkInterface face = faceItr.nextElement();
            Enumeration<InetAddress> ipItr = face.getInetAddresses();
            while (ipItr.hasMoreElements()) {
                InetAddress ip = ipItr.nextElement();
                if (ip instanceof Inet6Address)
                    continue;
                if (ip.isMulticastAddress())
                    continue;
                if (ip.isLoopbackAddress())
                    continue;
                if (ip.isAnyLocalAddress())
                    continue;

                DatagramChannel broadcastChannel = null;
                ServerSocketChannel acceptChannel = null;
                try {
                    broadcastChannel = DatagramChannel.open();
                    broadcastChannel.bind(
                        new InetSocketAddress(ip, Config.GetSingleton().broadcast_port));
                    broadcastChannel.socket().setBroadcast(true);
                    broadcastChannel.configureBlocking(false);
                    acceptChannel = ServerSocketChannel.open();
                    acceptChannel.bind(new InetSocketAddress(ip, Config.GetSingleton().port));
                    acceptChannel.configureBlocking(false);
                    FrontThread.GetSingleton().notifyInfoLog("get local ip success, ip: " + ip
                                                             + ", network interface: "
                                                             + face.getName());
                    SelectionKey k;
                    k = broadcastChannel.register(mSelector, SelectionKey.OP_WRITE);
                    mBroadcastKeys.add(k);
                    k = acceptChannel.register(mSelector, SelectionKey.OP_ACCEPT);
                    mAcceptKeys.add(k);
                    break;
                } catch (Exception e) {
                    FrontThread.GetSingleton().notifyInfoLog("get local ip fail: " + e.getMessage()
                                                             + ", ip: " + ip);
                    if (broadcastChannel != null)
                        QuietClose(broadcastChannel);
                    if (acceptChannel != null)
                        QuietClose(acceptChannel);
                }
            }
        }

        if (mBroadcastKeys.isEmpty())
            throw new Error("not found any local ip usable");

        if (UT_FAIL_INIT_RESOURCES)
            throw new Error("unit test");
    }

    private Frame popFrame() {
        synchronized (mPendingFrames) {
            return mPendingFrames.poll();
        }
    }

    private void onClientRead() throws Error {
        try {
            SocketChannel c = (SocketChannel) mClientKey.channel();
            mClientReadBuffer.position(0);
            c.read(mClientReadBuffer);

            if (UT_FAIL_READ)
                throw new Error("unit test");
        } catch (Exception e) {
            throw new Error("net thread read fail, " + e.getMessage());
        }
    }

    private void onClientWrite() throws Error {
        if (mCurrentFrame == null)
            throw new AssertionError("bug");

        try {
            SocketChannel c = (SocketChannel) mClientKey.channel();
            if (mCurrentFrame.headerBuffer.hasRemaining())
                c.write(mCurrentFrame.headerBuffer);
            if (mCurrentFrame.headerBuffer.hasRemaining())
                return;
            if (mCurrentFrame.bodyBuffer.hasRemaining())
                c.write(mCurrentFrame.bodyBuffer);
            if (mCurrentFrame.bodyBuffer.hasRemaining())
                return;

            BackThread.GetSingleton().notifyRecycleFrame(mCurrentFrame);

            mCurrentFrame = popFrame();
            // stop write if no frame.
            if (mCurrentFrame == null) {
                if (Config.GetSingleton().debug_print_write_start_stop)
                    FrontThread.GetSingleton().notifyInfoLog("stop write");
                mClientKey.interestOps(SelectionKey.OP_READ);
            }


            if (UT_FAIL_WRITE)
                throw new Error("unit test");
        } catch (Exception e) {
            throw new Error("net thread write fail, " + e.getMessage());
        }
    }

    private void onBroadcast(SelectionKey k) {
        if (!k.isValid())
            return;

        k.interestOps(0);

        DatagramChannel c = (DatagramChannel) k.channel();
        try {
            mBroadcastData.position(0);
            c.send(mBroadcastData, mBroadcastAddress);
            FrontThread.GetSingleton().notifyInfoLog("broadcast success, local ip: "
                                                     + c.socket().getLocalAddress());
        } catch (Exception e) {
            FrontThread.GetSingleton().notifyInfoLog("broadcast fail, " + e.getMessage()
                                                     + ", local ip: "
                                                     + c.socket().getLocalAddress());
        }
    }

    private void onAccept(SelectionKey k) throws Error {
        if (!k.isValid())
            return;

        ServerSocketChannel serverChannel = (ServerSocketChannel) k.channel();

        SocketChannel clientChannel;
        try {
            clientChannel = serverChannel.accept();
        } catch (Exception e) {
            FrontThread.GetSingleton().notifyErrLog(
                "accept fail, " + e.getMessage()
                + ", local ip: " + serverChannel.socket().getLocalSocketAddress());
            return;
        }

        try {
            if (UT_FAIL_ACCEPT)
                throw new Error("unit test");

            clientChannel.configureBlocking(false);
            mClientKey = clientChannel.register(mSelector, SelectionKey.OP_READ);
            FrontThread.GetSingleton().notifyInfoLog(
                "accept success, local ip: " + clientChannel.getLocalAddress()
                + ", remote ip: " + clientChannel.getRemoteAddress());
            // since connect, remove all broadcast and accept.
            for (SelectionKey kk : mBroadcastKeys) {
                QuietClose(kk.channel());
            }
            mBroadcastKeys.clear();
            for (SelectionKey kk : mAcceptKeys) {
                QuietClose(kk.channel());
            }
            mAcceptKeys.clear();

            if (mClientKey != null)
                BackThread.GetSingleton().notifyConnected();
        } catch (Exception e) {
            if (clientChannel != null)
                QuietClose(clientChannel);
            throw new Error("net thread accept fail, " + e.getMessage());
        }
    }

    private static void QuietClose(Closeable obj) {
        try {
            obj.close();
        } catch (Exception e) {
            // wtf.
        }
    }
}
