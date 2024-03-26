#include <ss/MainThread.hpp>
#include <ss/PtsThread.hpp>
#include <ss/DecodeThread.hpp>
#include <ss/NetThread.hpp>

static std::unique_ptr<ss::Config> parse_config(int argc, char* argv[]) {
    auto cfg = std::make_unique<ss::Config>();
    for (int i = 1; i < argc; ++i) {
        size_t len = ::strlen(argv[i]);
        if (len > 4 && ::strncmp(argv[i], "-ip=", 4) == 0) {
            cfg->ip = argv[i] + 4;
            sockaddr_in tmpAddr = {};
            SS_THROW(
                uv_ip4_addr(cfg->ip.c_str(), 1314, &tmpAddr) == 0,
                "parse direct ip address fail: %s",
                argv[i]);
        } else if (len > 6 && ::strncmp(argv[i], "-port=", 6) == 0) {
            SS_THROW(::sscanf(argv[i] + 6, "%d", &cfg->port) == 1, "parse port fail: %s", argv[i]);
            SS_THROW(
                cfg->port >= 0 && cfg->port < 65536,
                "port out of range: %d, acceptable range: [0, 65536)",
                cfg->port);
        } else if (len > 16 && ::strncmp(argv[i], "-broadcast-port=", 16) == 0) {
            SS_THROW(
                ::sscanf(argv[i] + 16, "%d", &cfg->broadcastPort) == 1,
                "parse broadcast port fail: %s",
                argv[i]);
            SS_THROW(
                cfg->broadcastPort >= 0 && cfg->broadcastPort < 65536,
                "broadcast port out of range: %d, acceptable range: [0, 65536)",
                cfg->broadcastPort);
        } else if (::strcmp(argv[i], "-immediately-paint") == 0) {
            cfg->immediatelyPaint = true;
        } else if (::strcmp(argv[i], "-debug-net") == 0) {
            cfg->debugNet = true;
        } else if (::strcmp(argv[i], "-debug-pts") == 0) {
            cfg->debugPts = true;
        } else if (::strcmp(argv[i], "-debug-decode") == 0) {
            cfg->debugDecode = true;
        } else {
            SS_THROW(0, "unknown command line arg: %s", argv[i]);
        }
    }

    // clang-format off
    ss::Log::I(
        "current config:\n"
        "- ip: %s\n"
        "- port: %d\n"
        "- broadcast port: %d\n"
        "- immedlately paint: %s",
        cfg->ip.empty() ? "empty" : cfg->ip.c_str(),
        cfg->port,
        cfg->broadcastPort,
        cfg->immediatelyPaint ? "true" : "false"
    );
    // clang-format on
    return cfg;
}

int main(int argc, char* argv[]) {
    // use crtdbg for windows memory leak check.
#if defined(XM_OS_WINDOWS) && !defined(NDEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    // set output to stdout.
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDOUT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDOUT);
#endif

    // load pdb files for windows, use for query backtrace info.
#if defined(XM_OS_WINDOWS)
    if (!::SymInitialize(GetCurrentProcess(), NULL, TRUE)) {
        ::printf("SymInitialize fail!\n");
    }
#endif

    auto log = std::make_unique<ss::Log>();

    if (argc == 2 && ::strcmp(argv[1], "-help") == 0) {
        ss::Log::I_STR(
            "command line usage:\n"
            "-help, print this\n"
            "-ip=[addr], direct ip adress, e.g. -ip=192.168.1.1\n"
            "-port=[port], connect port, e.g. -port=1314\n"
            "-broadcast-port=[port], broadcast port, e.g. -broadcast-port=1413\n"
            "-immediately-paint, enable immediately paint\n"
            "-debug-net, print net info to log\n"
            "-debug-pts, print pts info to log\n"
            "-debug-decode, print decode info to log");
        return 0;
    }

    std::unique_ptr<ss::Config> cfg;
    std::unique_ptr<ss::MainThread> mainThread;
    std::unique_ptr<ss::PtsThread> ptsThread;
    std::unique_ptr<ss::DecodeThread> decodeThread;
    std::unique_ptr<ss::NetThread> netThread;

    try {
        cfg = parse_config(argc, argv);
        mainThread = std::make_unique<ss::MainThread>();
        if (!cfg->immediatelyPaint) {
            ptsThread = std::make_unique<ss::PtsThread>();
        }
        decodeThread = std::make_unique<ss::DecodeThread>();
        netThread = std::make_unique<ss::NetThread>();
    } catch (const ss::Error& e) {
        ss::Log::PrintError(e);
        return 0;
    } catch (const std::exception& e) {
        ss::Log::E("catch %s: %s, %s#%d", typeid(e).name(), e.what(), __FILE__, __LINE__);
        return 0;
    }

    mainThread->loop();

    if (netThread) {
        netThread->notifyClose();
        netThread->join();
    }

    if (decodeThread) {
        decodeThread->notifyClose();
        decodeThread->join();
    }

    if (ptsThread) {
        ptsThread->notifyClose();
        ptsThread->join();
    }

    netThread = nullptr;
    decodeThread = nullptr;
    ptsThread = nullptr;
    mainThread = nullptr;
    return 0;
}