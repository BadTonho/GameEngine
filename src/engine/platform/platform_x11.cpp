#include "engine/platform/platform.hpp"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#undef Status

#include <limits>
#include <string>

namespace gameengine::platform {

namespace {

[[nodiscard]] Display* display_from(std::uintptr_t handle) noexcept
{
    return reinterpret_cast<Display*>(handle);
}

[[nodiscard]] ::Window window_from(std::uintptr_t handle) noexcept
{
    return static_cast<::Window>(handle);
}

[[nodiscard]] input::KeyCode map_key(KeySym key) noexcept
{
    using input::KeyCode;

    switch (key) {
    case XK_Escape:
        return KeyCode::escape;
    case XK_Return:
    case XK_KP_Enter:
        return KeyCode::enter;
    case XK_Tab:
        return KeyCode::tab;
    case XK_BackSpace:
        return KeyCode::backspace;
    case XK_space:
        return KeyCode::space;
    case XK_Left:
        return KeyCode::left;
    case XK_Right:
        return KeyCode::right;
    case XK_Up:
        return KeyCode::up;
    case XK_Down:
        return KeyCode::down;
    case XK_Page_Up:
        return KeyCode::page_up;
    case XK_Page_Down:
        return KeyCode::page_down;
    case XK_Shift_L:
    case XK_Shift_R:
        return KeyCode::shift;
    case XK_Control_L:
    case XK_Control_R:
        return KeyCode::control;
    case XK_Alt_L:
    case XK_Alt_R:
    case XK_Meta_L:
    case XK_Meta_R:
        return KeyCode::alt;
    case XK_Super_L:
    case XK_Super_R:
        return KeyCode::super;
    default:
        break;
    }

    if (key >= XK_a && key <= XK_z) {
        return static_cast<KeyCode>(static_cast<core::u16>(KeyCode::a) +
                                    static_cast<core::u16>(key - XK_a));
    }

    if (key >= XK_A && key <= XK_Z) {
        return static_cast<KeyCode>(static_cast<core::u16>(KeyCode::a) +
                                    static_cast<core::u16>(key - XK_A));
    }

    if (key >= XK_0 && key <= XK_9) {
        return static_cast<KeyCode>(static_cast<core::u16>(KeyCode::digit_0) +
                                    static_cast<core::u16>(key - XK_0));
    }

    return KeyCode::unknown;
}

[[nodiscard]] input::MouseButton map_button(unsigned int button) noexcept
{
    switch (button) {
    case Button1:
        return input::MouseButton::left;
    case Button2:
        return input::MouseButton::middle;
    case Button3:
        return input::MouseButton::right;
    case 8u:
        return input::MouseButton::x1;
    case 9u:
        return input::MouseButton::x2;
    default:
        return input::MouseButton::unknown;
    }
}

} // namespace

Platform::~Platform() noexcept
{
    shutdown();
}

core::Status Platform::initialize() noexcept
{
    if (initialized_) {
        return core::Status{core::ErrorCode::already_initialized};
    }

    Display* display = XOpenDisplay(nullptr);
    if (display == nullptr) {
        return core::Status{core::ErrorCode::display_unavailable};
    }

    native_display_ = reinterpret_cast<std::uintptr_t>(display);
    initialized_ = true;
    return core::Status{};
}

void Platform::shutdown() noexcept
{
    destroy_window();

    if (native_display_ != 0) {
        XCloseDisplay(display_from(native_display_));
        native_display_ = 0;
    }

    initialized_ = false;
    should_close_ = false;
    width_ = 0;
    height_ = 0;
    input_state_.reset();
}

core::Status Platform::create_window(const WindowDescription& description) noexcept
{
    if (!initialized_) {
        return core::Status{core::ErrorCode::not_initialized};
    }

    if (window_created_ || description.width == 0 || description.height == 0 ||
        description.width > std::numeric_limits<unsigned int>::max() ||
        description.height > std::numeric_limits<unsigned int>::max() ||
        description.width > static_cast<core::u32>(std::numeric_limits<int>::max()) ||
        description.height > static_cast<core::u32>(std::numeric_limits<int>::max())) {
        return core::Status{core::ErrorCode::invalid_argument};
    }

    Display* display = display_from(native_display_);
    const int screen = DefaultScreen(display);
    const ::Window root = RootWindow(display, screen);
    const ::Window window = XCreateSimpleWindow(
        display,
        root,
        0,
        0,
        static_cast<unsigned int>(description.width),
        static_cast<unsigned int>(description.height),
        0,
        BlackPixel(display, screen),
        WhitePixel(display, screen));

    if (window == 0) {
        return core::Status{core::ErrorCode::window_creation_failed};
    }

    native_window_ = static_cast<std::uintptr_t>(window);
    width_ = description.width;
    height_ = description.height;
    should_close_ = false;

    long event_mask = StructureNotifyMask | KeyPressMask | KeyReleaseMask |
                      ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
    XSelectInput(display, window, event_mask);

    Atom close_atom = XInternAtom(display, "WM_DELETE_WINDOW", False);
    if (close_atom == None) {
        destroy_window();
        return core::Status{core::ErrorCode::window_creation_failed};
    }
    native_close_atom_ = static_cast<std::uintptr_t>(close_atom);
    XSetWMProtocols(display, window, &close_atom, 1);

    const std::string title(description.title);
    XStoreName(display, window, title.c_str());

    XSizeHints size_hints{};
    size_hints.flags = PSize;
    size_hints.width = static_cast<int>(description.width);
    size_hints.height = static_cast<int>(description.height);
    if (!description.resizable) {
        size_hints.flags |= PMinSize | PMaxSize;
        size_hints.min_width = size_hints.max_width = size_hints.width;
        size_hints.min_height = size_hints.max_height = size_hints.height;
    }
    XSetWMNormalHints(display, window, &size_hints);

    XMapWindow(display, window);
    XFlush(display);
    window_created_ = true;
    return core::Status{};
}

void Platform::destroy_window() noexcept
{
    if (native_display_ != 0 && native_window_ != 0) {
        XDestroyWindow(display_from(native_display_), window_from(native_window_));
        XFlush(display_from(native_display_));
    }

    native_window_ = 0;
    native_close_atom_ = 0;
    window_created_ = false;
    should_close_ = false;
    width_ = 0;
    height_ = 0;
    input_state_.reset();
}

void Platform::poll_events(EventCallback callback, void* user_data) noexcept
{
    if (!initialized_ || !window_created_) {
        return;
    }

    Display* display = display_from(native_display_);
    while (XPending(display) > 0) {
        XEvent native_event{};
        XNextEvent(display, &native_event);

        input::Event event{};
        bool dispatch = false;

        switch (native_event.type) {
        case ClientMessage:
            if (static_cast<Atom>(native_event.xclient.data.l[0]) ==
                static_cast<Atom>(native_close_atom_)) {
                event.type = input::EventType::quit_requested;
                should_close_ = true;
                dispatch = true;
            }
            break;
        case DestroyNotify:
            event.type = input::EventType::quit_requested;
            should_close_ = true;
            dispatch = true;
            break;
        case ConfigureNotify:
            if (width_ != static_cast<core::u32>(native_event.xconfigure.width) ||
                height_ != static_cast<core::u32>(native_event.xconfigure.height)) {
                width_ = static_cast<core::u32>(native_event.xconfigure.width);
                height_ = static_cast<core::u32>(native_event.xconfigure.height);
                event.type = input::EventType::window_resized;
                event.width = width_;
                event.height = height_;
                dispatch = true;
            }
            break;
        case KeyPress:
        case KeyRelease:
            event.type = native_event.type == KeyPress ? input::EventType::key_pressed
                                                        : input::EventType::key_released;
            event.key = map_key(XLookupKeysym(&native_event.xkey, 0));
            event.repeated = false;
            dispatch = true;
            break;
        case ButtonPress:
        case ButtonRelease:
            event.type = native_event.type == ButtonPress
                             ? input::EventType::mouse_button_pressed
                             : input::EventType::mouse_button_released;
            event.mouse_button = map_button(native_event.xbutton.button);
            dispatch = event.mouse_button != input::MouseButton::unknown;
            break;
        case MotionNotify:
            event.type = input::EventType::mouse_moved;
            event.x = native_event.xmotion.x;
            event.y = native_event.xmotion.y;
            dispatch = true;
            break;
        default:
            break;
        }

        if (dispatch) {
            dispatch_event(event, callback, user_data);
        }
    }
}

void Platform::dispatch_event(const input::Event& event,
                               EventCallback callback,
                               void* user_data) noexcept
{
    input_state_.apply(event);
    if (callback != nullptr) {
        callback(event, user_data);
    }
}

} // namespace gameengine::platform
