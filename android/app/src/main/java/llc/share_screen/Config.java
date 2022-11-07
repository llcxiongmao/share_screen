package llc.share_screen;

public class Config {
    private static Config sSingleton;

    public static Config GetSingleton() {
        return sSingleton;
    }

    public static void SetSingleton(Config obj) {
        sSingleton = obj;
    }

    public static void ReleaseSingleton() {
        sSingleton = null;
    }

    Config() {
        if (sSingleton != null)
            throw new AssertionError("bug");
    }

    int bit_rate = 4;
    int i_frame_interval = 10;
    int port = 1314;
    int broadcast_port = 1413;
    boolean debug_print_encode = false;
    boolean debug_print_net = false;
}