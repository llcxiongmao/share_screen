#pragma once
#include <ss/main_thread_impl/Windows.hpp>
#include <ss/main_thread_impl/Linux.hpp>
#include <ss/main_thread_impl/MacOs.hpp>
#include <ss/GlRender.hpp>

namespace ss {
/** ui/main thread, handle frame draw. */
class MainThread : protected detail::MainThreadImpl {
public:
    using base_t = detail::MainThreadImpl;

    static MainThread* Singleton() {
        return static_cast<MainThread*>(base_t::Singleton());
    }

    MainThread();

    void notifyPaintFrame(PaintFrame* paintFrame);

    void notifyClose();

    void loop();

private:
    enum : uint32_t {
        EVENT_TYPE_PAINT_FRAME = _EVENT_TYPE_APP,
        EVENT_TYPE_CLOSE,
    };

    struct Locale {
        std::string title_connecting;
        std::string title_connected;

        void asZh() {
            title_connecting = u8"连接中。。。";
            title_connected = u8"已连接，帧率：%d";
        }

        void asEn() {
            title_connecting = "connecting...";
            title_connected = "connected, fps: %d";
        }
    };

    void draw(PaintFrame* paintFrame);

    //
    using clock = chrono::high_resolution_clock;

    bool mClose = false;
    size_t mWinWidth = 0;
    size_t mWinHeight = 0;
    clock::time_point mFpsTp;
    uint32_t mFps;
    Locale mLocale;
    std::optional<GlRender> mRender;
};
}  // namespace ss
