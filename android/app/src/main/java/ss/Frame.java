package ss;

import java.nio.ByteBuffer;

public class Frame {
    public int index;
    public final ByteBuffer headerBuffer = ByteBuffer.allocate(12);
    public ByteBuffer bodyBuffer;
}
