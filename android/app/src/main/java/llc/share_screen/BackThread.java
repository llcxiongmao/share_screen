package llc.share_screen;

import static android.hardware.display.DisplayManager.VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR;

import android.content.Context;
import android.hardware.display.VirtualDisplay;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.media.projection.MediaProjection;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.util.DisplayMetrics;
import android.view.WindowManager;

import androidx.annotation.NonNull;

import java.io.IOException;
import java.util.ArrayList;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutionException;

/** back thread, handle receive mediacodec result. */
public class BackThread {
    private static BackThread sSingleton;

    public static BackThread GetSingleton() {
        return sSingleton;
    }

    public static void SetSingleton(BackThread obj) {
        sSingleton = obj;
    }

    public static void ReleaseSingleton() {
        sSingleton = null;
    }

    private final MediaProjection mMediaProj;
    private final int mWidth;
    private final int mHeight;
    private final int mDpi;

    private final Thread mThread;
    private Handler mHandler;

    private VirtualDisplay mVirtualDisplay;
    private MediaCodec mCodec;
    /** create frame summary. */
    private int mFrameSummary = 0;
    /** free frames, alloc to write. */
    private final ArrayList<Frame> mFreeFrames = new ArrayList<>();

    /** create and start thread. */
    public BackThread(MediaProjection mediaProj) {
        if (sSingleton != null)
            throw new AssertionError("bug");

        mMediaProj = mediaProj;

        WindowManager winMgr =
            (WindowManager) FrontThread.GetSingleton().getSystemService(Context.WINDOW_SERVICE);
        DisplayMetrics screenMetrics = new DisplayMetrics();
        winMgr.getDefaultDisplay().getRealMetrics(screenMetrics);
        mWidth = screenMetrics.widthPixels;
        mHeight = screenMetrics.heightPixels;
        mDpi = screenMetrics.densityDpi;

        CompletableFuture<Object> guard = new CompletableFuture<>();
        mThread = new Thread(() -> run(guard));
        mThread.start();

        // wait handler setup.
        try {
            guard.get();
        } catch (ExecutionException | InterruptedException e) {
            throw new RuntimeException("wtf");
        }
    }

    /**
     * notify recycle frame.
     *
     * @param frame frame to recycle.
     * @throws Error if this closed, see {@link #close()}.
     */
    public void notifyNewFreeFrame(Frame frame) throws Error {
        notify(MsgId.NEW_FREE_FRAME, frame);
    }

    /**
     * notify connected.
     *
     * @throws Error if this closed, see {@link #close()}.
     */
    public void notifyConnected() throws Error {
        notify(MsgId.CONNECTED, null);
    }

    /** close thread, after call notifyNewFreeFrame notifyConnected will throw Error. */
    public void close() {
        mHandler.getLooper().quit();
    }

    /**
     * join thread.
     *
     * @return first is frame summary, second is free frame count.
     */
    public int[] join() {
        try {
            mThread.join();
        } catch (InterruptedException e) {
            throw new AssertionError("wtf");
        }
        return new int[] {mFrameSummary, mFreeFrames.size()};
    }

    private void notify(MsgId id, Object obj) throws Error {
        Message msg = Message.obtain();
        msg.what = id.ordinal();
        msg.obj = obj;
        if (!mHandler.sendMessage(msg)) {
            throw new Error("back thread already close");
        }
    }

    private void handleMessage(Message msg) {
        switch (MsgId.From(msg.what)) {
            case CONNECTED:
                try {
                    mCodec = MediaCodec.createEncoderByType("video/avc");
                } catch (IOException e) {
                    FrontThread.GetSingleton().notifyErrLog("create encode codec fail: "
                                                            + e.getMessage());
                    close();
                    break;
                }
                MediaFormat format =
                    MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, mHeight, mWidth);
                format.setInteger(MediaFormat.KEY_BIT_RATE,
                                  Config.GetSingleton().bit_rate * 1000 * 1000);
                format.setInteger(MediaFormat.KEY_FRAME_RATE, 60);
                format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL,
                                  Config.GetSingleton().i_frame_interval);
                format.setInteger(MediaFormat.KEY_COLOR_FORMAT,
                                  MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);
                mCodec.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
                mVirtualDisplay = mMediaProj.createVirtualDisplay("share_screen",
                                                                  mHeight,
                                                                  mWidth,
                                                                  mDpi,
                                                                  VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR,
                                                                  mCodec.createInputSurface(),
                                                                  null,
                                                                  null);
                mCodec.setCallback(new CodecCallback(), mHandler);
                mCodec.start();
                break;
            case NEW_FREE_FRAME: {
                Frame frame = (Frame) msg.obj;
                mCodec.releaseOutputBuffer(frame.index, false);
                mFreeFrames.add(frame);
                break;
            }
        }
    }

    private void run(CompletableFuture<Object> guard) {
        FrontThread.GetSingleton().notifyInfoLog("back thread run");

        Looper.prepare();
        mHandler = new Handler(Looper.myLooper()) {
            @Override
            public void handleMessage(Message msg) {
                BackThread.this.handleMessage(msg);
            }
        };
        guard.complete(null);

        Looper.loop();

        if (mCodec != null) {
            mCodec.stop();
            mCodec.release();
        }
        if (mVirtualDisplay != null)
            mVirtualDisplay.release();

        FrontThread.GetSingleton().close();

        FrontThread.GetSingleton().notifyInfoLog("back thread exit");
    }

    ////

    private class CodecCallback extends MediaCodec.Callback {
        @Override
        public void onInputBufferAvailable(@NonNull MediaCodec codec, int index) {}

        @Override
        public void onOutputBufferAvailable(@NonNull MediaCodec codec,
                                            int index,
                                            @NonNull MediaCodec.BufferInfo info) {
            if (Config.GetSingleton().debug_print_encode)
                FrontThread.GetSingleton().notifyInfoLog(
                    "encode output frame, index: " + index + ", flags: " + info.flags
                    + ", size: " + info.size + ", pts: " + info.presentationTimeUs);

            long pts = info.presentationTimeUs;
            if ((info.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0)
                pts = -1;
            Frame frame;
            if (mFreeFrames.isEmpty()) {
                frame = new Frame();
                ++mFrameSummary;
            } else {
                frame = mFreeFrames.get(mFreeFrames.size() - 1);
                mFreeFrames.remove(mFreeFrames.size() - 1);
            }
            frame.index = index;
            frame.headerBuffer.clear();
            frame.headerBuffer.putInt(0, info.size);
            frame.headerBuffer.putLong(4, pts);
            frame.bodyBuffer = codec.getOutputBuffer(index);
            try {
                NetThread.GetSingleton().notifyNewFrame(frame);
            } catch (Exception e) {
                FrontThread.GetSingleton().notifyErrLog(e.getMessage());
                mFreeFrames.add(frame);
                close();
            }
        }

        @Override
        public void onError(@NonNull MediaCodec codec, @NonNull MediaCodec.CodecException e) {
            FrontThread.GetSingleton().notifyErrLog("codec fail: " + e.getMessage());
            close();
        }

        @Override
        public void onOutputFormatChanged(@NonNull MediaCodec codec, @NonNull MediaFormat format) {
            FrontThread.GetSingleton().notifyInfoLog("codec format change: " + format);
        }
    }

    private enum MsgId {
        CONNECTED,
        NEW_FREE_FRAME;

        private static final MsgId[] sAll = MsgId.values();

        public static MsgId From(int id) {
            return sAll[id];
        }
    }
}