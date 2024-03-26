package ss;

import android.util.Log;

import java.io.Closeable;
import java.io.IOException;
import java.net.Inet6Address;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.NetworkInterface;
import java.net.SocketException;
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

public class NetThread {
    public static NetThread Singleton() {
        return sSingleton;
    }

    public static void SetSingleton(NetThread s) {
        sSingleton = s;
    }

    NetThread() throws IOException {
        if (sSingleton != null) {
            throw new AssertionError("bug");
        }

        mPendingWriteFrames = new ArrayDeque<>();
        mCachePendingWriteFrames = new ArrayList<>();
        mBroadcastKeys = new ArrayList<>();
        mAcceptKeys = new ArrayList<>();
        mCurrentFrame = null;
        mClientReadBuffer = ByteBuffer.allocate(1);
        mClientKey = null;
        mSelector = Selector.open();

        if (UTSO_FAIL_CREATE) {
            throw new IOException("unit test simulate");
        }

        mThread = new Thread(this::run);
        mThread.start();
    }

    public void notifyClose() {
        mClose = true;
        mThread.interrupt();
    }

    public void notifyWriteFrame(Frame frame) {
        synchronized (mCachePendingWriteFrames) {
            mCachePendingWriteFrames.add(frame);
        }
        mSelector.wakeup();
    }

    public void join() {
        try {
            mThread.join();
        } catch (Exception e) {
            //
        }
    }

    private static void QuietClose(Closeable obj) {
        try {
            obj.close();
        } catch (Exception e) {
            // wtf.
        }
    }

    private void collectLocalAddress() {
        Enumeration<NetworkInterface> faceItr;
        try {
            faceItr = NetworkInterface.getNetworkInterfaces();
        } catch (SocketException e) {
            return;
        }
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
                        new InetSocketAddress(ip, Config.Singleton().broadcast_port.value));
                    broadcastChannel.socket().setBroadcast(true);
                    broadcastChannel.configureBlocking(false);

                    acceptChannel = ServerSocketChannel.open();
                    acceptChannel.bind(new InetSocketAddress(ip, Config.Singleton().port.value));
                    acceptChannel.configureBlocking(false);

                    SelectionKey broadcastKey =
                        broadcastChannel.register(mSelector, SelectionKey.OP_WRITE);
                    SelectionKey acceptKey =
                        acceptChannel.register(mSelector, SelectionKey.OP_ACCEPT);
                    mBroadcastKeys.add(broadcastKey);
                    mAcceptKeys.add(acceptKey);

                    MainThread.Singleton().notifyLog(
                        'I',
                        String.format(
                            "get local ip: %s, network interface: %s", ip, face.getName()));
                    // we already success get ip on this interface, so go to next interface.
                    break;
                } catch (Exception e) {
                    MainThread.Singleton().notifyLog(
                        'W', String.format("get local ip fail: %s, ip: %s", e.getMessage(), ip));
                    if (broadcastChannel != null) {
                        QuietClose(broadcastChannel);
                    }
                    if (acceptChannel != null) {
                        QuietClose(acceptChannel);
                    }
                }
            }
        }
    }

    private void onBroadcast(DatagramChannel channel, InetSocketAddress address, ByteBuffer data) {
        try {
            data.position(0);
            channel.send(data, address);
            MainThread.Singleton().notifyLog(
                'I', String.format("broadcast: %s", channel.socket().getLocalAddress()));
        } catch (Exception e) {
            MainThread.Singleton().notifyLog(
                'W',
                String.format(
                    "broadcast fail: %s, local ip: %s",
                    e.getMessage(),
                    channel.socket().getLocalAddress()));
        }
    }

    private void onAccept(ServerSocketChannel channel) throws TagNext {
        SocketChannel clientChannel = null;
        try {
            clientChannel = channel.accept();
            clientChannel.configureBlocking(false);
            mClientKey = clientChannel.register(mSelector, SelectionKey.OP_READ);

            if (UTSO_FAIL_ACCEPT) {
                throw new IOException("unit test simulate: net accept");
            }

            MainThread.Singleton().notifyLog(
                'I',
                String.format(
                    "accept client, local ip: %s, remote ip: %s",
                    clientChannel.getLocalAddress(),
                    clientChannel.getRemoteAddress()));
            throw new TagNext();
        } catch (IOException e) {
            if (clientChannel != null) {
                QuietClose(clientChannel);
            }
            mClientKey = null;
            MainThread.Singleton().notifyLog(
                'W',
                String.format(
                    "accept client fail: %s, local ip: %s",
                    e.getMessage(),
                    channel.socket().getLocalSocketAddress()));
        }
    }

    private void step0() throws TagExit, IOException {
        collectLocalAddress();
        if (mBroadcastKeys.isEmpty()) {
            throw new TagExit("not find any local ip usable");
        }

        try {
            InetSocketAddress broadcastAddress =
                new InetSocketAddress("255.255.255.255", Config.Singleton().broadcast_port.value);
            ByteBuffer broadcastData = ByteBuffer.allocate(4);
            broadcastData.put("1314".getBytes(StandardCharsets.UTF_8));

            int broadcastCount = 30;
            long tp0 = System.currentTimeMillis();
            long tp1 = tp0 + 2000;
            while (!mClose) {
                tp0 = System.currentTimeMillis();
                if (tp0 - tp1 >= 0) {
                    if (broadcastCount < 1) {
                        throw new TagExit("accept timeout");
                    }
                    for (SelectionKey k : mBroadcastKeys) {
                        k.interestOps(SelectionKey.OP_WRITE);
                    }
                    --broadcastCount;
                    tp1 = tp0 + 2000;
                }

                mSelector.select(tp1 - tp0);

                Set<SelectionKey> keys = mSelector.selectedKeys();
                Iterator<SelectionKey> itr = keys.iterator();
                while (itr.hasNext()) {
                    SelectionKey k = itr.next();
                    if (k.isWritable()) {
                        k.interestOps(0);
                        onBroadcast((DatagramChannel) k.channel(), broadcastAddress, broadcastData);
                    }
                    if (k.isAcceptable()) {
                        onAccept((ServerSocketChannel) k.channel());
                    }
                    itr.remove();
                }
            }
        } catch (TagNext ignored) {
        }

        if (mClose) {
            throw new TagExit();
        }

        if (mClientKey == null) {
            throw new AssertionError("bug");
        }

        for (SelectionKey i : mBroadcastKeys) {
            QuietClose(i.channel());
        }
        mBroadcastKeys.clear();
        for (SelectionKey i : mAcceptKeys) {
            QuietClose(i.channel());
        }
        mAcceptKeys.clear();

        EncodeThread.Singleton().notifyStartEncode();
    }

    private void onClientRead() throws TagExit {
        SocketChannel c = (SocketChannel) mClientKey.channel();
        mClientReadBuffer.position(0);
        try {
            c.read(mClientReadBuffer);
        } catch (IOException e) {
            throw new TagExit("net client read fail: " + e.getMessage());
        }

        if (UTSO_FAIL_READ) {
            throw new TagExit("unit test simulate: net client read");
        }
    }

    private void onClientWrite() throws TagExit {
        SocketChannel c = (SocketChannel) mClientKey.channel();

        if (mCurrentFrame == null) {
            throw new AssertionError("bug");
        }

        try {
            if (mCurrentFrame.headerBuffer.hasRemaining())
                c.write(mCurrentFrame.headerBuffer);
            if (mCurrentFrame.headerBuffer.hasRemaining())
                return;
            if (mCurrentFrame.bodyBuffer.hasRemaining())
                c.write(mCurrentFrame.bodyBuffer);
            if (mCurrentFrame.bodyBuffer.hasRemaining())
                return;
        } catch (IOException e) {
            throw new TagExit("net write fail: " + e.getMessage());
        }

        EncodeThread.Singleton().notifyRecycleFrame(mCurrentFrame);
        mCurrentFrame = mPendingWriteFrames.poll();
        if (mCurrentFrame == null) {
            // no pending frames, so remove OP_WRITE.
            mClientKey.interestOps(SelectionKey.OP_READ);
        }

        if (UTSO_FAIL_WRITE) {
            throw new TagExit("unit test simulate: net client write");
        }
    }

    private void step1() throws IOException, TagExit {
        while (!mClose) {
            mSelector.select(2000);

            synchronized (mCachePendingWriteFrames) {
                for (Frame i : mCachePendingWriteFrames) {
                    mPendingWriteFrames.offer(i);
                }
                mCachePendingWriteFrames.clear();
            }
            if (mCurrentFrame == null) {
                mCurrentFrame = mPendingWriteFrames.poll();
                if (mCurrentFrame != null) {
                    mClientKey.interestOps(SelectionKey.OP_WRITE | SelectionKey.OP_READ);
                }
            }

            Set<SelectionKey> keys = mSelector.selectedKeys();
            Iterator<SelectionKey> itr = keys.iterator();
            while (itr.hasNext()) {
                SelectionKey k = itr.next();
                if (k.isReadable()) {
                    onClientRead();
                }
                if (k.isWritable()) {
                    onClientWrite();
                }
                itr.remove();
            }
        }
    }

    private void run() {
        MainThread.Singleton().notifyLog('I', "net thread run");

        try {
            // step0: send broadcast and wait client.
            // step1: write encode frame to client...
            step0();
            step1();
        } catch (TagExit e) {
            if (e.getMessage() != null) {
                MainThread.Singleton().notifyLog('E', e.getMessage());
            }
        } catch (Exception e) {
            MainThread.Singleton().notifyLog('E', Log.getStackTraceString(e));
        }

        for (SelectionKey i : mBroadcastKeys) {
            QuietClose(i.channel());
        }
        mBroadcastKeys.clear();
        for (SelectionKey i : mAcceptKeys) {
            QuietClose(i.channel());
        }
        mAcceptKeys.clear();

        if (mClientKey != null) {
            QuietClose(mClientKey.channel());
        }
        mClientKey = null;

        SessionThread.Singleton().notifyClose();

        MainThread.Singleton().notifyLog('I', "net thread exit");
    }

    private static class TagExit extends Exception {
        TagExit() {}

        TagExit(String message) {
            super(message);
        }
    }
    private static class TagNext extends Exception {}

    /** unit test simulate options. */
    private static final boolean UTSO_FAIL_CREATE = MainThread.ENABLE_UTSO && false;
    private static final boolean UTSO_FAIL_READ = MainThread.ENABLE_UTSO && false;
    private static final boolean UTSO_FAIL_WRITE = MainThread.ENABLE_UTSO && false;
    private static final boolean UTSO_FAIL_ACCEPT = MainThread.ENABLE_UTSO && false;

    private static NetThread sSingleton;

    private boolean mClose = false;
    private final Queue<Frame> mPendingWriteFrames;
    private final List<Frame> mCachePendingWriteFrames;
    ArrayList<SelectionKey> mBroadcastKeys;
    ArrayList<SelectionKey> mAcceptKeys;
    private Frame mCurrentFrame;
    private final ByteBuffer mClientReadBuffer;
    private SelectionKey mClientKey;
    private final Selector mSelector;
    private final Thread mThread;
}
