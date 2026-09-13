#include "engine/platform/platform.hpp"
#include "engine/rhi/rhi.hpp"

#include <X11/Xlib.h>

int main()
{
    gameengine::platform::Platform platform;
    if (!platform.initialize().ok()) {
        return 1;
    }

    if (!platform.create_window({
            .title = "GameEngine Vulkan resize test",
            .width = 320,
            .height = 240,
            .resizable = true,
        })) {
        return 2;
    }

    gameengine::rhi::Renderer renderer;
    if (!renderer.initialize(platform).ok()) {
        return 3;
    }
    if (!renderer.render_frame(platform).ok()) {
        return 4;
    }

    const auto handles = platform.native_window_handles();
    auto* display = reinterpret_cast<Display*>(handles.display);
    const auto window = static_cast<::Window>(handles.window);
    XResizeWindow(display, window, 640, 480);
    XFlush(display);
    XSync(display, False);
    platform.poll_events();

    const auto size = platform.window_size();
    if (size.width != 640 || size.height != 480) {
        return 5;
    }
    if (!renderer.render_frame(platform).ok()) {
        return 6;
    }

    renderer.shutdown();
    platform.shutdown();
    return 0;
}
