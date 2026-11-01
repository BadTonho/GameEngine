#include "engine/physics/physics.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

using namespace gameengine;

void run_case(core::u32 count)
{
    scene::Scene scene;
    scene.reserve(count + 1U);

    const scene::Entity ground = scene.create_entity();
    static_cast<void>(scene.add_transform(ground, {.local_position = {0.0F, 0.0F, 0.0F}}));

    std::vector<scene::Entity> entities;
    entities.reserve(count);

    const core::u32 side = static_cast<core::u32>(std::ceil(std::sqrt(static_cast<core::f32>(count))));
    constexpr core::f32 spacing = 2.0F;

    for (core::u32 index = 0; index < count; ++index) {
        const core::u32 x_idx = index % side;
        const core::u32 z_idx = index / side;
        const core::f32 x = (static_cast<core::f32>(x_idx) - static_cast<core::f32>(side) * 0.5F) * spacing;
        const core::f32 z = (static_cast<core::f32>(z_idx) - static_cast<core::f32>(side) * 0.5F) * spacing;
        const core::f32 y = 5.0F + static_cast<core::f32>(index % 10U) * 1.5F;

        const scene::Entity entity = scene.create_entity();
        entities.push_back(entity);
        static_cast<void>(scene.add_transform(entity, {.local_position = {x, y, z}}));
    }
    static_cast<void>(scene.update_transforms());

    physics::PhysicsWorld world;
    const physics::PhysicsConfig config{
        .max_bodies = count + 64U,
        .max_body_pairs = (count + 64U) * 4U,
        .max_contact_constraints = (count + 64U) * 4U,
        .temp_allocator_bytes = 16U * 1024U * 1024U,
    };

    if (!world.initialize(config)) {
        std::fprintf(stderr, "benchmark failed to initialize physics world\n");
        return;
    }

    const core::f32 ground_extent = static_cast<core::f32>(side) * spacing;
    static_cast<void>(world.add_body(scene, ground, {
        .shape = physics::ShapeType::box,
        .half_extents = {ground_extent, 1.0F, ground_extent},
        .motion = physics::MotionType::static_body,
    }));

    const physics::BodyDesc body_desc{
        .shape = physics::ShapeType::box,
        .half_extents = {0.5F, 0.5F, 0.5F},
        .motion = physics::MotionType::dynamic_body,
        .mass_kilograms = 1.0F,
    };

    for (const scene::Entity entity : entities) {
        static_cast<void>(world.add_body(scene, entity, body_desc));
    }

    // Warm-up step
    static_cast<void>(world.update(1.0F / 60.0F));

    // Timed step
    const auto start = std::chrono::steady_clock::now();
    static_cast<void>(world.update(1.0F / 60.0F));
    const auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - start).count();

    const physics::PhysicsMetrics metrics = world.metrics();
    std::printf("physics bodies=%u update_us=%lld total_steps=%llu dropped_steps=%llu\n",
                count,
                static_cast<long long>(elapsed_ns / 1000),
                static_cast<unsigned long long>(metrics.total_steps),
                static_cast<unsigned long long>(metrics.dropped_steps));

    world.shutdown();
}

} // namespace

int main()
{
    run_case(1'000U);
    run_case(10'000U);
    return 0;
}
