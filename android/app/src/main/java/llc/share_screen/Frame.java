package llc.share_screen;

import java.nio.ByteBuffer;

public class Frame {
    public int index = 0;
    public final ByteBuffer headerBuffer = ByteBuffer.allocate(12);
    public ByteBuffer bodyBuffer;

    Frame() {}
}
