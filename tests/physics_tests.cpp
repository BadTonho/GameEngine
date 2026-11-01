#include "engine/physics/physics.hpp"

#include <cmath>
#include <cstdio>

namespace {

using namespace gameengine;

bool expect(bool condition, const char* message = "physics test failure")
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
    return condition;
}

bool near_equal(core::f32 left, core::f32 right, core::f32 epsilon = 0.05F)
{
    return std::fabs(left - right) <= epsilon;
}

} // namespace

int main()
{
    bool passed = true;

    // 1. Config validation tests
    {
        physics::PhysicsConfig invalid_config{};
        invalid_config.max_bodies = 0U;
        passed = expect(!physics::validate_config(invalid_config).ok(), "reject 0 max_bodies") && passed;

        invalid_config = {};
        invalid_config.fixed_step_seconds = 0.0F;
        passed = expect(!physics::validate_config(invalid_config).ok(), "reject 0 fixed_step") && passed;

        invalid_config = {};
        invalid_config.gravity = {NAN, 0.0F, 0.0F};
        passed = expect(!physics::validate_config(invalid_config).ok(), "reject NaN gravity") && passed;

        invalid_config = {};
        invalid_config.temp_allocator_bytes = 1024U; // Less than 64KB
        passed = expect(!physics::validate_config(invalid_config).ok(), "reject small temp allocator") && passed;

        passed = expect(physics::validate_config({}).ok(), "default config is valid") && passed;
    }

    // 2. Body descriptor validation
    {
        physics::BodyDesc desc{};
        desc.shape = physics::ShapeType::box;
        desc.half_extents = {0.01F, 0.5F, 0.5F}; // Below minimum_shape_extent
        passed = expect(!physics::validate_body_desc(desc).ok(), "reject tiny box extent") && passed;

        desc = {};
        desc.shape = physics::ShapeType::sphere;
        desc.radius = 0.01F;
        passed = expect(!physics::validate_body_desc(desc).ok(), "reject tiny sphere radius") && passed;

        desc = {};
        desc.motion = physics::MotionType::dynamic_body;
        desc.mass_kilograms = -1.0F;
        passed = expect(!physics::validate_body_desc(desc).ok(), "reject negative mass") && passed;

        desc = {};
        desc.restitution = 1.5F;
        passed = expect(!physics::validate_body_desc(desc).ok(), "reject restitution > 1.0") && passed;

        desc = {};
        desc.friction = -0.1F;
        passed = expect(!physics::validate_body_desc(desc).ok(), "reject negative friction") && passed;

        passed = expect(physics::validate_body_desc({}).ok(), "default body desc is valid") && passed;
    }

    // 3. Lifecycle: multiple initialize and shutdown cycles
    {
        physics::PhysicsWorld world;
        passed = expect(!world.initialized(), "world initially uninitialized") && passed;
        passed = expect(world.initialize().ok(), "first initialize ok") && passed;
        passed = expect(world.initialized(), "world initialized flag is true") && passed;
        passed = expect(!world.initialize().ok(), "double initialize rejected") && passed;
        world.shutdown();
        passed = expect(!world.initialized(), "world uninitialized after shutdown") && passed;
        passed = expect(world.initialize().ok(), "second initialize ok") && passed;
        world.shutdown();
    }

    // 4. Entity requirements and body creation
    {
        scene::Scene scene;
        const scene::Entity parent = scene.create_entity();
        const scene::Entity child = scene.create_entity();
        passed = expect(scene.add_transform(parent, {.local_position = {0.0F, 0.0F, 0.0F}}).ok()) && passed;
        passed = expect(scene.add_transform(child, {.local_position = {0.0F, 1.0F, 0.0F}}).ok()) && passed;
        passed = expect(scene.set_parent(child, parent).ok()) && passed;

        physics::PhysicsWorld world;
        passed = expect(world.initialize({.max_bodies = 2U}).ok()) && passed;

        // Non-alive entity
        passed = expect(!world.add_body(scene, scene::Entity::make(999U, 1U), {}).ok(),
                        "reject non-alive entity") && passed;

        // Child entity with parent
        passed = expect(!world.add_body(scene, child, {}).ok(),
                        "reject entity with parent") && passed;

        // Valid parent entity
        passed = expect(world.add_body(scene, parent, {}).ok(), "add body for parent") && passed;
        passed = expect(world.has_body(parent), "parent has body") && passed;

        // Duplicate body for same entity
        passed = expect(!world.add_body(scene, parent, {}).ok(),
                        "reject duplicate body for entity") && passed;

        // Capacity limit
        const scene::Entity second = scene.create_entity();
        passed = expect(scene.add_transform(second).ok()) && passed;
        passed = expect(world.add_body(scene, second, {}).ok(), "add second body") && passed;

        const scene::Entity third = scene.create_entity();
        passed = expect(scene.add_transform(third).ok()) && passed;
        passed = expect(world.add_body(scene, third, {}).code == core::ErrorCode::physics_capacity_exceeded,
                        "reject body beyond max_bodies capacity") && passed;

        // Removal
        passed = expect(world.remove_body(parent).ok(), "remove body ok") && passed;
        passed = expect(!world.has_body(parent), "parent no longer has body") && passed;
        passed = expect(!world.remove_body(parent).ok(), "remove nonexistent body fails") && passed;

        world.shutdown();
    }

    // 5. Fixed step and accumulator behavior
    {
        physics::PhysicsWorld world;
        const physics::PhysicsConfig config{
            .fixed_step_seconds = 1.0F / 60.0F,
            .max_steps_per_update = 2U,
        };
        passed = expect(world.initialize(config).ok()) && passed;

        core::u32 steps_taken = 99U;
        // Small delta less than 1/60s: accumulator accumulates, 0 steps taken
        passed = expect(world.update(0.005F, &steps_taken).ok()) && passed;
        passed = expect(steps_taken == 0U, "sub-frame delta yields 0 steps") && passed;

        // Another delta completing the step
        passed = expect(world.update(0.015F, &steps_taken).ok()) && passed;
        passed = expect(steps_taken == 1U, "accumulated delta triggers 1 step") && passed;

        // Large delta exceeding max_steps_per_update: clamped to 2 steps, dropped steps tracked
        passed = expect(world.update(1.0F, &steps_taken).ok()) && passed;
        passed = expect(steps_taken == 2U, "large delta clamped to max_steps_per_update") && passed;
        const physics::PhysicsMetrics metrics = world.metrics();
        passed = expect(metrics.total_steps == 3U, "total steps count matches") && passed;
        passed = expect(metrics.dropped_steps > 0U, "dropped steps incremented on spike") && passed;

        world.shutdown();
    }

    // 6. Simulation test: Dynamic box falling onto static ground box
    {
        scene::Scene scene;
        const scene::Entity ground = scene.create_entity();
        const scene::Entity box = scene.create_entity();
        passed = expect(scene.add_transform(ground, {.local_position = {0.0F, 0.0F, 0.0F}}).ok()) && passed;
        passed = expect(scene.add_transform(box, {.local_position = {0.0F, 3.0F, 0.0F}}).ok()) && passed;
        passed = expect(scene.update_transforms().ok()) && passed;

        physics::PhysicsWorld world;
        passed = expect(world.initialize().ok()) && passed;

        // Ground: box of half-extents 5, 0.5, 5 at y = 0 -> top surface is at y = +0.5
        const physics::BodyDesc ground_desc{
            .shape = physics::ShapeType::box,
            .half_extents = {5.0F, 0.5F, 5.0F},
            .motion = physics::MotionType::static_body,
            .friction = 0.5F,
            .restitution = 0.0F,
        };
        passed = expect(world.add_body(scene, ground, ground_desc).ok()) && passed;

        // Dynamic box: half-extents 0.5, 0.5, 0.5 at y = 3.0.
        // Expected resting position: top of ground (+0.5) + box half-height (0.5) = y = 1.0.
        const physics::BodyDesc box_desc{
            .shape = physics::ShapeType::box,
            .half_extents = {0.5F, 0.5F, 0.5F},
            .motion = physics::MotionType::dynamic_body,
            .mass_kilograms = 2.0F,
            .friction = 0.5F,
            .restitution = 0.0F,
        };
        passed = expect(world.add_body(scene, box, box_desc).ok()) && passed;

        // Run simulation for 2 seconds (120 steps at 1/60s)
        for (core::u32 step = 0; step < 120; ++step) {
            passed = expect(world.update(1.0F / 60.0F).ok()) && passed;
        }

        // Sync transforms back to scene
        passed = expect(world.write_transforms(scene).ok()) && passed;

        const scene::TransformComponent* transform = scene.transform(box);
        passed = expect(transform != nullptr, "transform exists") && passed;
        if (transform != nullptr) {
            // Box should have landed at approximately y = 1.0 (ground top 0.5 + box half 0.5)
            passed = expect(near_equal(transform->local_position.y, 1.0F, 0.15F),
                            "box rested on top of ground near y=1.0") && passed;
            passed = expect(near_equal(transform->local_position.x, 0.0F, 0.1F),
                            "box stayed centered on x") && passed;
            passed = expect(near_equal(transform->local_position.z, 0.0F, 0.1F),
                            "box stayed centered on z") && passed;
        }

        world.shutdown();
    }

    // 7. Determinism test: Two identical worlds produce exact same poses
    {
        scene::Scene scene_a;
        scene::Scene scene_b;
        const scene::Entity ground_a = scene_a.create_entity();
        const scene::Entity box_a = scene_a.create_entity();
        const scene::Entity ground_b = scene_b.create_entity();
        const scene::Entity box_b = scene_b.create_entity();

        scene_a.add_transform(ground_a, {.local_position = {0.0F, 0.0F, 0.0F}});
        scene_a.add_transform(box_a, {.local_position = {0.2F, 4.0F, 0.1F}});
        scene_a.update_transforms();

        scene_b.add_transform(ground_b, {.local_position = {0.0F, 0.0F, 0.0F}});
        scene_b.add_transform(box_b, {.local_position = {0.2F, 4.0F, 0.1F}});
        scene_b.update_transforms();

        physics::PhysicsWorld world_a;
        physics::PhysicsWorld world_b;
        world_a.initialize();
        world_b.initialize();

        const physics::BodyDesc ground_desc{
            .shape = physics::ShapeType::box,
            .half_extents = {5.0F, 0.5F, 5.0F},
            .motion = physics::MotionType::static_body,
        };
        const physics::BodyDesc box_desc{
            .shape = physics::ShapeType::box,
            .half_extents = {0.5F, 0.5F, 0.5F},
            .motion = physics::MotionType::dynamic_body,
            .mass_kilograms = 3.0F,
        };

        world_a.add_body(scene_a, ground_a, ground_desc);
        world_a.add_body(scene_a, box_a, box_desc);
        world_b.add_body(scene_b, ground_b, ground_desc);
        world_b.add_body(scene_b, box_b, box_desc);

        for (core::u32 step = 0; step < 60; ++step) {
            world_a.update(1.0F / 60.0F);
            world_b.update(1.0F / 60.0F);
        }

        math::Vec3 pos_a{}, pos_b{};
        math::Quaternion rot_a{}, rot_b{};
        world_a.body_pose(box_a, pos_a, rot_a);
        world_b.body_pose(box_b, pos_b, rot_b);

        passed = expect(pos_a.x == pos_b.x && pos_a.y == pos_b.y && pos_a.z == pos_b.z,
                        "exact deterministic position across worlds") && passed;
        passed = expect(rot_a.x == rot_b.x && rot_a.y == rot_b.y && rot_a.z == rot_b.z && rot_a.w == rot_b.w,
                        "exact deterministic rotation across worlds") && passed;

        world_a.shutdown();
        world_b.shutdown();
    }

    // 8. Sphere shape test
    {
        scene::Scene scene;
        const scene::Entity ground = scene.create_entity();
        const scene::Entity sphere = scene.create_entity();
        scene.add_transform(ground, {.local_position = {0.0F, 0.0F, 0.0F}});
        scene.add_transform(sphere, {.local_position = {0.0F, 2.5F, 0.0F}});
        scene.update_transforms();

        physics::PhysicsWorld world;
        world.initialize();

        world.add_body(scene, ground, {
            .shape = physics::ShapeType::box,
            .half_extents = {5.0F, 0.5F, 5.0F},
            .motion = physics::MotionType::static_body,
        });

        // Radius 0.5 at y = 2.5 -> rests at ground top (0.5) + radius (0.5) = y ~ 1.0
        world.add_body(scene, sphere, {
            .shape = physics::ShapeType::sphere,
            .radius = 0.5F,
            .motion = physics::MotionType::dynamic_body,
            .mass_kilograms = 1.0F,
        });

        for (core::u32 step = 0; step < 120; ++step) {
            world.update(1.0F / 60.0F);
        }

        math::Vec3 sphere_pos{};
        math::Quaternion sphere_rot{};
        world.body_pose(sphere, sphere_pos, sphere_rot);
        passed = expect(near_equal(sphere_pos.y, 1.0F, 0.15F), "sphere rested at y ~ 1.0") && passed;

        world.shutdown();
    }

    return passed ? 0 : 1;
}
