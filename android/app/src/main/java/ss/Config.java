package ss;

public class Config {
    public static Config Singleton() {
        return sSingleton;
    }

    public Config() {
        if (sSingleton != null) {
            throw new AssertionError("bug");
        }

        bit_rate = new IntValue(1, 100, 8, "bit_rate", 8);
        i_interval = new IntValue(1, 100, 10, "i_interval", 10);
        port = new IntValue(1, 65535, 1314, "port", 1314);
        broadcast_port = new IntValue(1, 65535, 1413, "broadcast_port", 1413);
        debug_encode = new BoolValue(false, "debug_encode", false);
        debug_net = new BoolValue(false, "debug_net", false);
        sSingleton = this;
    }

    private static Config sSingleton;

    public final IntValue bit_rate;
    public final IntValue i_interval;
    public final IntValue port;
    public final IntValue broadcast_port;
    public final BoolValue debug_encode;
    public final BoolValue debug_net;

    public static class IntValue {
        public IntValue(int min, int max, int defaultValue, String name, int value) {
            this.min = min;
            this.max = max;
            this.defaultValue = defaultValue;
            this.name = name;
            this.value = value;
        }

        final int min;
        final int max;
        final int defaultValue;
        final String name;
        int value;
    }

    public static class BoolValue {
        public BoolValue(boolean defaultValue, String name, boolean value) {
            this.defaultValue = defaultValue;
            this.name = name;
            this.value = value;
        }

        final boolean defaultValue;
        final String name;
        boolean value;
    }
}