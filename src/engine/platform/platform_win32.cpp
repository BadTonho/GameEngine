#include "engine/platform/platform.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>

#include <array>
#include <limits>
#include <string>

namespace gameengine::platform {

namespace {

constexpr const wchar_t* kWindowClassName = L"GameEngineWindowClass";

constexpr std::size_t kMaxPendingEvents = 64;
thread_local std::array<input::Event, kMaxPendingEvents> s_pending_events{};
thread_local std::size_t s_pending_event_count = 0;

void push_pending_event(const input::Event& event) noexcept
{
    if (s_pending_event_count < kMaxPendingEvents) {
        s_pending_events[s_pending_event_count++] = event;
    }
}

[[nodiscard]] input::KeyCode map_key(WPARAM vk) noexcept
{
    using input::KeyCode;

    switch (vk) {
    case VK_ESCAPE:
        return KeyCode::escape;
    case VK_RETURN:
        return KeyCode::enter;
    case VK_TAB:
        return KeyCode::tab;
    case VK_BACK:
        return KeyCode::backspace;
    case VK_SPACE:
        return KeyCode::space;
    case VK_LEFT:
        return KeyCode::left;
    case VK_RIGHT:
        return KeyCode::right;
    case VK_UP:
        return KeyCode::up;
    case VK_DOWN:
        return KeyCode::down;
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
        return KeyCode::shift;
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
        return KeyCode::control;
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
        return KeyCode::alt;
    case VK_LWIN:
    case VK_RWIN:
        return KeyCode::super;
    default:
        break;
    }

    if (vk >= 'A' && vk <= 'Z') {
        return static_cast<KeyCode>(static_cast<core::u16>(KeyCode::a) +
                                    static_cast<core::u16>(vk - 'A'));
    }

    if (vk >= '0' && vk <= '9') {
        return static_cast<KeyCode>(static_cast<core::u16>(KeyCode::digit_0) +
                                    static_cast<core::u16>(vk - '0'));
    }

    return KeyCode::unknown;
}

[[nodiscard]] input::MouseButton map_xbutton(WPARAM wparam) noexcept
{
    const WORD btn = GET_XBUTTON_WPARAM(wparam);
    if (btn == XBUTTON1) {
        return input::MouseButton::x1;
    }
    if (btn == XBUTTON2) {
        return input::MouseButton::x2;
    }
    return input::MouseButton::unknown;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg) {
    case WM_CLOSE: {
        input::Event event{};
        event.type = input::EventType::quit_requested;
        push_pending_event(event);
        return 0;
    }
    case WM_SIZE: {
        const auto width = static_cast<core::u32>(LOWORD(lparam));
        const auto height = static_cast<core::u32>(HIWORD(lparam));
        input::Event event{};
        event.type = input::EventType::window_resized;
        event.width = width;
        event.height = height;
        push_pending_event(event);
        return 0;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        input::Event event{};
        event.type = input::EventType::key_pressed;
        event.key = map_key(wparam);
        event.repeated = (lparam & (1L << 30)) != 0;
        push_pending_event(event);
        return 0;
    }
    case WM_KEYUP:
    case WM_SYSKEYUP: {
        input::Event event{};
        event.type = input::EventType::key_released;
        event.key = map_key(wparam);
        event.repeated = false;
        push_pending_event(event);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        input::Event event{};
        event.type = input::EventType::mouse_button_pressed;
        event.mouse_button = input::MouseButton::left;
        push_pending_event(event);
        return 0;
    }
    case WM_LBUTTONUP: {
        input::Event event{};
        event.type = input::EventType::mouse_button_released;
        event.mouse_button = input::MouseButton::left;
        push_pending_event(event);
        return 0;
    }
    case WM_RBUTTONDOWN: {
        input::Event event{};
        event.type = input::EventType::mouse_button_pressed;
        event.mouse_button = input::MouseButton::right;
        push_pending_event(event);
        return 0;
    }
    case WM_RBUTTONUP: {
        input::Event event{};
        event.type = input::EventType::mouse_button_released;
        event.mouse_button = input::MouseButton::right;
        push_pending_event(event);
        return 0;
    }
    case WM_MBUTTONDOWN: {
        input::Event event{};
        event.type = input::EventType::mouse_button_pressed;
        event.mouse_button = input::MouseButton::middle;
        push_pending_event(event);
        return 0;
    }
    case WM_MBUTTONUP: {
        input::Event event{};
        event.type = input::EventType::mouse_button_released;
        event.mouse_button = input::MouseButton::middle;
        push_pending_event(event);
        return 0;
    }
    case WM_XBUTTONDOWN: {
        input::Event event{};
        event.type = input::EventType::mouse_button_pressed;
        event.mouse_button = map_xbutton(wparam);
        if (event.mouse_button != input::MouseButton::unknown) {
            push_pending_event(event);
        }
        return TRUE;
    }
    case WM_XBUTTONUP: {
        input::Event event{};
        event.type = input::EventType::mouse_button_released;
        event.mouse_button = map_xbutton(wparam);
        if (event.mouse_button != input::MouseButton::unknown) {
            push_pending_event(event);
        }
        return TRUE;
    }
    case WM_MOUSEMOVE: {
        input::Event event{};
        event.type = input::EventType::mouse_moved;
        event.x = static_cast<core::i32>(GET_X_LPARAM(lparam));
        event.y = static_cast<core::i32>(GET_Y_LPARAM(lparam));
        push_pending_event(event);
        return 0;
    }
    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wparam, lparam);
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

    const HINSTANCE hinstance = GetModuleHandleW(nullptr);
    if (hinstance == nullptr) {
        return core::Status{core::ErrorCode::display_unavailable};
    }

    native_display_ = reinterpret_cast<std::uintptr_t>(hinstance);
    initialized_ = true;
    return core::Status{};
}

void Platform::shutdown() noexcept
{
    destroy_window();

    if (native_display_ != 0) {
        UnregisterClassW(kWindowClassName, reinterpret_cast<HINSTANCE>(native_display_));
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
        description.width > static_cast<core::u32>(std::numeric_limits<int>::max()) ||
        description.height > static_cast<core::u32>(std::numeric_limits<int>::max())) {
        return core::Status{core::ErrorCode::invalid_argument};
    }

    const auto hinstance = reinterpret_cast<HINSTANCE>(native_display_);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hinstance;
    wc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    wc.lpszClassName = kWindowClassName;

    if (RegisterClassExW(&wc) == 0) {
        const DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS) {
            return core::Status{core::ErrorCode::window_creation_failed};
        }
    }

    DWORD style = WS_OVERLAPPEDWINDOW;
    if (!description.resizable) {
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    }

    RECT rect{0, 0, static_cast<LONG>(description.width), static_cast<LONG>(description.height)};
    AdjustWindowRect(&rect, style, FALSE);
    const int window_width = rect.right - rect.left;
    const int window_height = rect.bottom - rect.top;

    std::wstring wide_title;
    if (!description.title.empty()) {
        const int len = MultiByteToWideChar(
            CP_UTF8,
            0,
            description.title.data(),
            static_cast<int>(description.title.size()),
            nullptr,
            0);
        if (len > 0) {
            wide_title.resize(static_cast<std::size_t>(len));
            MultiByteToWideChar(
                CP_UTF8,
                0,
                description.title.data(),
                static_cast<int>(description.title.size()),
                wide_title.data(),
                len);
        }
    }

    const HWND hwnd = CreateWindowExW(
        0,
        kWindowClassName,
        wide_title.empty() ? L"GameEngine" : wide_title.c_str(),
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        window_width,
        window_height,
        nullptr,
        nullptr,
        hinstance,
        nullptr);

    if (hwnd == nullptr) {
        return core::Status{core::ErrorCode::window_creation_failed};
    }

    native_window_ = reinterpret_cast<std::uintptr_t>(hwnd);
    native_close_atom_ = 0;
    width_ = description.width;
    height_ = description.height;
    should_close_ = false;

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    window_created_ = true;
    return core::Status{};
}

void Platform::destroy_window() noexcept
{
    if (native_window_ != 0) {
        const auto hwnd = reinterpret_cast<HWND>(native_window_);
        DestroyWindow(hwnd);
        native_window_ = 0;
    }

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

    s_pending_event_count = 0;

    const auto hwnd = reinterpret_cast<HWND>(native_window_);
    MSG msg{};
    while (PeekMessageW(&msg, hwnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    const std::size_t count = s_pending_event_count;
    for (std::size_t i = 0; i < count; ++i) {
        const auto& event = s_pending_events[i];
        if (event.type == input::EventType::quit_requested) {
            should_close_ = true;
        } else if (event.type == input::EventType::window_resized) {
            width_ = event.width;
            height_ = event.height;
        }
        dispatch_event(event, callback, user_data);
    }

    s_pending_event_count = 0;
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
