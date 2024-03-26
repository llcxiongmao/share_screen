#pragma once
#include <ss/Common.hpp>

#if defined(XM_OS_MAC)
namespace ss::detail {
class MainThreadImpl : public xm::SingletonBase<MainThreadImpl> {
public:
    using EventType = short;
    
    enum : EventType {
        EVENT_TYPE_WIN_RESIZE,
        EVENT_TYPE_WIN_CLOSE,
        _EVENT_TYPE_APP,
    };

    struct Event {
        EventType type;
        void* data0;
        void* data1;
    };

protected:
    MainThreadImpl();

    ~MainThreadImpl();

    // return user language name(iso-639-1).
    std::string queryUserLanguageName();

    void initWindowAndGl();

    void setWindowTitle(const char* utf8Str);

    void swapBuffers();

    void postEvent(const Event& e);

    void pollEvent(chrono::milliseconds timeout);

    std::optional<Event> peekEvent();

    //
    
    void* mImpl;
};
}  // namespace ss::detail
#endif

