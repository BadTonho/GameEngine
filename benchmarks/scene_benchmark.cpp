#include "engine/scene/scene.hpp"

#include <chrono>
#include <cstdio>

namespace {

using clock_type = std::chrono::steady_clock;

[[nodiscard]] double elapsed_ms(clock_type::time_point start,
                                clock_type::time_point finish) noexcept
{
    return std::chrono::duration<double, std::milli>(finish - start).count();
}

} // namespace

int main()
{
    using namespace gameengine::scene;
    for (const gameengine::core::usize count : {1000U, 10000U, 100000U}) {
        Scene scene;
        scene.reserve(count);
        const auto create_start = clock_type::now();
        Entity previous{};
        for (gameengine::core::usize index = 0; index < count; ++index) {
            const Entity entity = scene.create_entity();
            if (!scene.add_transform(entity).ok()) {
                return 1;
            }
            if (previous.valid() && (index % 4U == 0U) && !scene.set_parent(entity, previous)) {
                return 2;
            }
            previous = entity;
        }
        const auto create_finish = clock_type::now();
        const auto update_start = clock_type::now();
        if (!scene.update_transforms()) {
            return 3;
        }
        const auto update_finish = clock_type::now();
        const auto query_start = clock_type::now();
        for (const Entity entity : scene.entities()) {
            if (!scene.is_alive(entity)) {
                return 4;
            }
        }
        const auto query_finish = clock_type::now();
        const auto destroy_start = clock_type::now();
        while (scene.entity_count() != 0U) {
            if (!scene.destroy_entity(scene.entities().back())) {
                return 5;
            }
        }
        const auto destroy_finish = clock_type::now();
        std::printf("entities=%zu create_ms=%.3f update_ms=%.3f query_ms=%.3f destroy_ms=%.3f memory_bytes=%zu\n",
                    count,
                    elapsed_ms(create_start, create_finish),
                    elapsed_ms(update_start, update_finish),
                    elapsed_ms(query_start, query_finish),
                    elapsed_ms(destroy_start, destroy_finish),
                    scene.memory_usage_bytes());
    }
    return 0;
}
