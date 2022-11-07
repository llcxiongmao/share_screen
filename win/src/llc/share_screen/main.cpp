#include <llc/share_screen/FrontThread.hpp>
#include <llc/share_screen/PtsThread.hpp>
#include <llc/share_screen/DecodeThread.hpp>
#include <llc/share_screen/BackThread.hpp>

#include <crtdbg.h>

using namespace llc;
using namespace llc::share_screen;

static std::unique_ptr<Config> parse_config(int argc, char* argv[]) {
    std::unique_ptr<Config> cfg = std::make_unique<Config>();
    for (int i = 1; i < argc; ++i) {
        size_t len = strlen(argv[i]);
        if (len > 4 && strncmp(argv[i], "-ip=", 4) == 0) {
            cfg->ip = argv[i] + 4;
            in_addr ip = {};
            THROW_IF(!uv_inet_pton(AF_INET, cfg->ip.c_str(), &ip),
                     "parse direct ip address fail, check: " << argv[i]);
        } else if (len > 6 && strncmp(argv[i], "-port=", 6) == 0) {
            THROW_IF(sscanf(argv[i] + 6, "%d", &cfg->port) == 1,
                     "parse port fail, check: " << argv[i]);
            THROW_IF(cfg->port >= 0 && cfg->port < 65536, "port out of range, require: [0, 65535]");
        } else if (len > 16 && strncmp(argv[i], "-broadcast-port=", 16) == 0) {
            THROW_IF(sscanf(argv[i] + 16, "%d", &cfg->broadcastPort) == 1,
                     "parse broadcast port fail, check: " << argv[i]);
            THROW_IF(cfg->broadcastPort >= 0 && cfg->broadcastPort < 65536,
                     "broadcast port out of range, require: [0, 65535]");
        } else if (strcmp(argv[i], "-immediately-paint") == 0) {
            cfg->immediatelyPaint = true;
        } else if (strcmp(argv[i], "-disable-hwaccel") == 0) {
            cfg->disableHwaccel = true;
        } else if (strcmp(argv[i], "-disable-high-precision-time") == 0) {
            cfg->disableHighPrecisionTime = true;
        } else if (strcmp(argv[i], "-use-gl-render") == 0) {
            cfg->useGlRender = true;
        } else if (strcmp(argv[i], "-debug-print-net") == 0) {
            cfg->debugPrintNet = true;
        } else if (strcmp(argv[i], "-debug-print-pts") == 0) {
            cfg->debugPrintPts = true;
        } else if (strcmp(argv[i], "-debug-print-decode") == 0) {
            cfg->debugPrintDecode = true;
        } else {
            THROW_IF(0, "unknown command line arg, check: " << argv[i]);
            return 0;
        }
    }

    // clang-format off
    log::i() << "current config:\nip: " << (cfg->ip.empty() ? "empty" : cfg->ip)
             << "\nport: " << cfg->port 
             << "\nbroadcast port: " << cfg->broadcastPort
             << "\nimmedlately paint: " << util::fmt_bool(cfg->immediatelyPaint)
             << "\ndisable hardware accel: " << util::fmt_bool(cfg->disableHwaccel)
             << "\ndisable high precision time: " << util::fmt_bool(cfg->disableHighPrecisionTime)
             << "\nuse gl render: " << util::fmt_bool(cfg->useGlRender)
             << "\ndebug print net: " << util::fmt_bool(cfg->debugPrintNet)
             << "\ndebug print pts: " << util::fmt_bool(cfg->debugPrintPts)
             << "\ndebug print decode: " << util::fmt_bool(cfg->debugPrintDecode);
    // clang-format on
    return cfg;
}

int main(int argc, char* argv[]) {
    // enable memory leak check in debug build.
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    SetConsoleTitleA("share_screen");

    if (argc == 2 && strcmp(argv[1], "-help") == 0) {
        log::i() << "command line usage:\n"
                    "-ip=[addr], direct ip adress, e.g. -ip=192.168.1.1\n"
                    "-port=[port], connect port, e.g. -port=1314\n"
                    "-broadcast-port=[port], broadcast port, e.g. -broadcast-port=1413\n"
                    "-debug-print-net, print net info to log\n"
                    "-debug-print-pts, print pts info to log\n"
                    "-debug-print-decode, print decode info to log\n"
                    "-immediately-paint, enable immediately paint\n"
                    "-disable-hwaccel, disable hardware decode\n"
                    "-use-gl-render, force use gl render\n";
        return 0;
    }

    log::i() << "share screen, version: " LLC_SHARE_SCREEN_VERSION_NAME
                ", build type: " LLC_SHARE_SCREEN_BUILD_TYPE;

    std::unique_ptr<Config> cfg;
    std::unique_ptr<FrontThread> frontThread;
    try {
        cfg = parse_config(argc, argv);
        frontThread = std::make_unique<FrontThread>();
    } catch (const std::exception& e) {
        log::e() << e.what();
        return 0;
    }

    std::unique_ptr<PtsThread> ptsThread;
    std::unique_ptr<DecodeThread> decodeThread;
    std::unique_ptr<BackThread> backThread;
    try {
        if (!Config::GetSingleton()->immediatelyPaint)
            ptsThread = std::make_unique<PtsThread>();
        decodeThread = std::make_unique<DecodeThread>();
        backThread = std::make_unique<BackThread>();
        // THROW_IF(0, "unit test");
    } catch (const std::exception& e) {
        log::e() << e.what();
        frontThread->close();
    }

    frontThread->loop();

    if (backThread) {
        backThread->close();
        backThread->join();
    }

    if (decodeThread) {
        decodeThread->close();
        decodeThread->join();
    }

    if (ptsThread) {
        ptsThread->close();
        ptsThread->join();
    }

    backThread = nullptr;
    decodeThread = nullptr;
    ptsThread = nullptr;
    frontThread = nullptr;
    return 0;
}