#include "engine/scripting/scripting.hpp"

#include <chrono>
#include <cstdio>

namespace {

using namespace gameengine;

void run_case(core::u32 update_iterations)
{
    scene::Scene scene;
    const scene::Entity actor = scene.create_entity();
    static_cast<void>(scene.add_transform(actor));
    static_cast<void>(scene.update_transforms());

    scripting::ScriptRuntime runtime;
    if (!runtime.initialize()) {
        std::fprintf(stderr, "scripting benchmark failed to initialize\n");
        return;
    }
    static_cast<void>(runtime.register_entity("Actor", actor));
    constexpr std::string_view bench_script = R"lua(
        local actor = nil
        function start()
            actor = Engine.findEntity("Actor")
        end
        function update(dt)
            actor:rotateY(0.25 * dt)
        end
    )lua";
    if (!runtime.load_script(bench_script) || !runtime.call_start(scene)) {
        std::fprintf(stderr, "scripting benchmark failed to load/start script\n");
        runtime.shutdown();
        return;
    }

    const auto start = std::chrono::steady_clock::now();
    for (core::u32 index = 0; index < update_iterations; ++index) {
        if (!runtime.call_update(scene, 1.0F / 60.0F)) {
            std::fprintf(stderr, "scripting benchmark update failed\n");
            runtime.shutdown();
            return;
        }
    }
    const auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - start).count();

    const scripting::ScriptMetrics metrics = runtime.metrics();
    std::printf("scripting calls=%u total_us=%lld memory_bytes=%zu peak_bytes=%zu\n",
                update_iterations,
                static_cast<long long>(elapsed_ns / 1000),
                metrics.memory_bytes,
                metrics.peak_memory_bytes);
    runtime.shutdown();
}

} // namespace

int main()
{
    run_case(1'000U);
    run_case(10'000U);
    return 0;
}
