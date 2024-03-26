package ss;

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
import android.util.Log;
import android.view.WindowManager;

import androidx.annotation.NonNull;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Objects;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutionException;

public class EncodeThread {
    public static EncodeThread Singleton() {
        return sSingleton;
    }

    public static void SetSingleton(EncodeThread s) {
        sSingleton = s;
    }

    public EncodeThread(MediaProjection mediaProj)
        throws IOException, ExecutionException, InterruptedException {
        if (sSingleton != null) {
            throw new AssertionError("bug");
        }

        mFreeFrames = new ArrayList<>();

        mMediaProj = mediaProj;
        mEncoder = MediaCodec.createEncoderByType("video/avc");

        if (UTSO_FAIL_CODEC_CREATE) {
            throw new IOException("unit test simulate");
        }

        CompletableFuture<Object> handlerOk = new CompletableFuture<>();
        mThread = new Thread(() -> run(handlerOk));
        mThread.start();
        // ensure 'mHandle' has be created.
        handlerOk.get();
    }

    public void notifyStartEncode() {
        Message msg = Message.obtain();
        msg.what = MSG_START_ENCODE;
        mHandler.sendMessage(msg);
    }

    public void notifyRecycleFrame(Frame frame) {
        Message msg = Message.obtain();
        msg.what = MSG_RECYCLE_FRAME;
        msg.obj = frame;
        mHandler.sendMessage(msg);
    }

    public void notifyClose() {
        Message msg = Message.obtain();
        msg.what = MSG_CLOSE;
        mHandler.sendMessage(msg);
    }

    public void join() {
        try {
            mThread.join();
        } catch (Exception e) {
            //
        }
    }

    private void run(CompletableFuture<Object> handlerOk) {
        MainThread.Singleton().notifyLog('I', "encode thread run");

        Looper.prepare();
        mHandler = new Handler(Objects.requireNonNull(Looper.myLooper())) {
            @Override
            public void handleMessage(@NonNull Message msg) {
                if (msg.what == MSG_START_ENCODE) {
                    WindowManager winMgr = (WindowManager) MainThread.Singleton().getSystemService(
                        Context.WINDOW_SERVICE);
                    DisplayMetrics screenMetrics = new DisplayMetrics();
                    winMgr.getDefaultDisplay().getRealMetrics(screenMetrics);
                    int h = screenMetrics.heightPixels;
                    int w = screenMetrics.widthPixels;
                    MediaFormat format =
                        MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, h, w);
                    format.setInteger(
                        MediaFormat.KEY_BIT_RATE, Config.Singleton().bit_rate.value * 1000 * 1000);
                    format.setInteger(MediaFormat.KEY_FRAME_RATE, 60);
                    format.setInteger(
                        MediaFormat.KEY_I_FRAME_INTERVAL, Config.Singleton().i_interval.value);
                    format.setInteger(
                        MediaFormat.KEY_COLOR_FORMAT,
                        MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);
                    mEncoder.setCallback(new EncoderCallback(), mHandler);
                    mEncoder.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
                    mVirtualDisplay = mMediaProj.createVirtualDisplay(
                        "share_screen",
                        h,
                        w,
                        screenMetrics.densityDpi,
                        VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR,
                        mEncoder.createInputSurface(),
                        null,
                        null);
                    mEncoder.start();
                } else if (msg.what == MSG_RECYCLE_FRAME) {
                    Frame frame = (Frame) msg.obj;
                    mEncoder.releaseOutputBuffer(frame.index, false);
                    mFreeFrames.add(frame);
                } else if (msg.what == MSG_CLOSE) {
                    Objects.requireNonNull(Looper.myLooper()).quit();
                }
            }
        };
        handlerOk.complete(null);

        try {
            Looper.loop();
        } catch (TagExit ignored) {
        }

        mEncoder.stop();
        mEncoder.release();

        if (mVirtualDisplay != null) {
            mVirtualDisplay.release();
        }

        SessionThread.Singleton().notifyClose();

        MainThread.Singleton().notifyLog('I', "encode thread exit");
    }

    private static class TagExit extends RuntimeException {}

    private class EncoderCallback extends MediaCodec.Callback {
        @Override
        public void onInputBufferAvailable(@NonNull MediaCodec codec, int index) {}

        @Override
        public void onOutputBufferAvailable(
            @NonNull MediaCodec codec, int index, @NonNull MediaCodec.BufferInfo info) {
            if (Config.Singleton().debug_encode.value) {
                MainThread.Singleton().notifyLog(
                    'I',
                    String.format(
                        "encode frame, index: %d, flags: %d, size: %d, pts: %d",
                        index,
                        info.flags,
                        info.size,
                        info.presentationTimeUs));
            }

            long pts = info.presentationTimeUs;
            if ((info.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0) {
                pts = -1;
            }

            Frame frame;
            if (mFreeFrames.isEmpty()) {
                frame = new Frame();
                ++mFrameObjectCount;
                MainThread.Singleton().notifyLog(
                    'I', String.format("frame object count: %d", mFrameObjectCount));
            } else {
                frame = mFreeFrames.remove(mFreeFrames.size() - 1);
            }

            frame.index = index;
            frame.headerBuffer.clear();
            frame.headerBuffer.putInt(0, info.size);
            frame.headerBuffer.putLong(4, pts);
            frame.bodyBuffer = mEncoder.getOutputBuffer(index);
            NetThread.Singleton().notifyWriteFrame(frame);
        }

        @Override
        public void onError(@NonNull MediaCodec codec, @NonNull MediaCodec.CodecException e) {
            MainThread.Singleton().notifyLog('E', Log.getStackTraceString(e));
            throw new TagExit();
        }

        @Override
        public void onOutputFormatChanged(@NonNull MediaCodec codec, @NonNull MediaFormat format) {
            MainThread.Singleton().notifyLog(
                'I', String.format("encode format change: %s", format));
        }
    }

    private static final int MSG_START_ENCODE = 0;
    private static final int MSG_RECYCLE_FRAME = 1;
    private static final int MSG_CLOSE = 2;

    /** unit test simulate options. */
    private static final boolean UTSO_FAIL_CODEC_CREATE = MainThread.ENABLE_UTSO && false;

    private static EncodeThread sSingleton;

    private int mFrameObjectCount = 0;
    private final ArrayList<Frame> mFreeFrames;

    private final MediaProjection mMediaProj;
    private final MediaCodec mEncoder;
    private Handler mHandler = null;
    private final Thread mThread;
    VirtualDisplay mVirtualDisplay = null;
}