#import <ss/main_thread_impl/MacOs.hpp>

#import <Cocoa/Cocoa.h>

//////////////////////////////

using namespace ss::detail;

static std::deque<MainThreadImpl::Event>* sPendingEvents = nullptr;

@interface MyWindowDelegate : NSWindow <NSWindowDelegate> {
}
@end
@implementation MyWindowDelegate
- (void)windowWillClose:(NSNotification*)notification {
    sPendingEvents->push_back(
        MainThreadImpl::Event {MainThreadImpl::EVENT_TYPE_WIN_CLOSE, nullptr, nullptr});
}

- (void)windowDidResize:(NSNotification*)notification {
    NSWindow* win = notification.object;
    CGSize size = win.contentView.frame.size;
    sPendingEvents->push_back(MainThreadImpl::Event {
        MainThreadImpl::EVENT_TYPE_WIN_RESIZE,
        (void*)(size_t)size.width,
        (void*)(size_t)size.height});
}
@end

//////////////////////////////

@interface MyOpenGLView : NSOpenGLView {
}
@end

@implementation MyOpenGLView
- (instancetype)initWithFrame:(NSRect)frameRect {
    @autoreleasepool {
        // clang-format off
        NSOpenGLPixelFormatAttribute attris[] = {
            NSOpenGLPFAAccelerated,
            NSOpenGLPFADoubleBuffer,
            NSOpenGLPFAMultisample,
            NSOpenGLPFASampleBuffers, 0,
            NSOpenGLPFASamples, 0,
            NSOpenGLPFAColorSize, 24,
            NSOpenGLPFAAlphaSize, 8,
            0,
        };
        // clang-format on
        NSOpenGLPixelFormat* pf = [NSOpenGLPixelFormat alloc];
        [pf initWithAttributes:attris];
        [pf autorelease];
        self = [super initWithFrame:frameRect pixelFormat:pf];
        [self.openGLContext makeCurrentContext];
        GLint swapInt = 0;  // off vsync.
        [self.openGLContext setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];
        gladLoaderLoadGL();
    }
    return self;
}

- (void)dealloc {
    gladLoaderUnloadGL();
    [super dealloc];
}
@end

/////////////////////////////

@interface MyImpl : NSObject {
    NSWindow* mWindow;
    std::deque<MainThreadImpl::Event> mPendingEvents;
}
@end

@implementation MyImpl
- (instancetype)init {
    mWindow = nullptr;
    sPendingEvents = &mPendingEvents;
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];
    return self;
}

- (void)dealloc {
    @autoreleasepool {
        if (mWindow) {
            [mWindow.delegate release];
            [mWindow release];
        }
        [super dealloc];
    }
}

- (void)initWindowAndGl {
    @autoreleasepool {
        NSRect rect = NSMakeRect(0, 0, 400, 400);
        mWindow = [NSWindow alloc];
        [mWindow initWithContentRect:rect
                           styleMask:NSWindowStyleMaskTitled |          //
                                     NSWindowStyleMaskClosable |        //
                                     NSWindowStyleMaskMiniaturizable |  //
                                     NSWindowStyleMaskResizable
                             backing:NSBackingStoreBuffered
                               defer:NO];
        MyOpenGLView* myView = [MyOpenGLView alloc];
        [myView initWithFrame:rect];
        [myView autorelease];
        MyWindowDelegate* myDelegate = [MyWindowDelegate new];
        mWindow.contentView = myView;
        mWindow.delegate = myDelegate;
        mWindow.releasedWhenClosed = NO;
        [mWindow makeKeyAndOrderFront:nil];
        [mWindow zoom:nil];
    }
}

- (void)setWindowTitle:(const char*)utf8Str {
    @autoreleasepool {
        NSString* s = [NSString stringWithUTF8String:utf8Str];
        mWindow.title = s;
    }
}

- (void)swapBuffers {
    MyOpenGLView* v = (MyOpenGLView*)mWindow.contentView;
    [v.openGLContext flushBuffer];
}

- (void)postEvent:(short)type data0:(void*)data0 data1:(void*)data1 {
    @autoreleasepool {
        NSEvent* event = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                            location:NSMakePoint(0, 0)
                                       modifierFlags:0
                                           timestamp:0
                                        windowNumber:0
                                             context:nil
                                             subtype:type
                                               data1:(NSInteger)data0
                                               data2:(NSInteger)data1];
        [NSApp postEvent:event atStart:NO];
    }
}

- (void)pollEvent:(double)timeout_ms {
    @autoreleasepool {
        double timeout_s = timeout_ms / 1000.0;
        NSEvent* event =
            [NSApp nextEventMatchingMask:NSEventMaskAny
                               untilDate:[NSDate dateWithTimeIntervalSinceNow:timeout_s]
                                  inMode:NSDefaultRunLoopMode
                                 dequeue:YES];
        if (event == nullptr) {
            return;
        }
        if (event.type == NSEventTypeApplicationDefined) {
            mPendingEvents.push_back(
                MainThreadImpl::Event {event.subtype, (void*)event.data1, (void*)event.data2});
        } else {
            [NSApp sendEvent:event];
        }
    }
}

- (std::optional<MainThreadImpl::Event>)peekEvent {
    if (mPendingEvents.empty()) {
        return std::nullopt;
    }
    MainThreadImpl::Event e = mPendingEvents.front();
    mPendingEvents.pop_front();
    return e;
}
@end

namespace ss::detail {
MainThreadImpl::MainThreadImpl() {
    mImpl = [MyImpl new];
}

MainThreadImpl::~MainThreadImpl() {
    [(MyImpl*)mImpl release];
}

std::string MainThreadImpl::queryUserLanguageName() {
    return NSLocale.currentLocale.languageCode.UTF8String;
}

void MainThreadImpl::initWindowAndGl() {
    [(MyImpl*)mImpl initWindowAndGl];
}

void MainThreadImpl::setWindowTitle(const char* utf8Str) {
    [(MyImpl*)mImpl setWindowTitle:utf8Str];
}

void MainThreadImpl::swapBuffers() {
    [(MyImpl*)mImpl swapBuffers];
}

void MainThreadImpl::postEvent(const Event& e) {
    [(MyImpl*)mImpl postEvent:e.type data0:e.data0 data1:e.data1];
}

void MainThreadImpl::pollEvent(chrono::milliseconds timeout) {
    [(MyImpl*)mImpl pollEvent:(double)timeout.count()];
}

std::optional<MainThreadImpl::Event> MainThreadImpl::peekEvent() {
    return [(MyImpl*)mImpl peekEvent];
}
}  // namespace ss::detail
