#pragma once

#include "engine/core/status.hpp"
#include "engine/core/types.hpp"
#include "engine/math/math.hpp"
#include "engine/scene/scene.hpp"

namespace gameengine::physics {

// Engine-facing physics API. The Jolt backend stays private to physics.cpp so no Jolt type,
// header or compile definition leaks into runtime, editor or scene code.

enum class MotionType : core::u8 {
    static_body = 0,
    dynamic_body,
};

enum class ShapeType : core::u8 {
    box = 0,
    sphere,
};

struct BodyDesc final {
    ShapeType shape = ShapeType::box;
    math::Vec3 half_extents{0.5F, 0.5F, 0.5F};
    core::f32 radius = 0.5F;
    MotionType motion = MotionType::dynamic_body;
    core::f32 mass_kilograms = 1.0F;
    core::f32 friction = 0.5F;
    core::f32 restitution = 0.0F;
};

struct PhysicsConfig final {
    core::u32 max_bodies = 1'024U;
    core::u32 max_body_pairs = 4'096U;
    core::u32 max_contact_constraints = 4'096U;
    core::u32 temp_allocator_bytes = 4U * 1024U * 1024U;
    core::f32 fixed_step_seconds = 1.0F / 60.0F;
    core::u32 max_steps_per_update = 4U;
    math::Vec3 gravity{0.0F, -9.81F, 0.0F};
};

struct PhysicsMetrics final {
    core::u32 body_count = 0;
    core::u32 dynamic_body_count = 0;
    core::u64 total_steps = 0;
    core::u64 dropped_steps = 0;
};

// Shapes below this size are rejected because Jolt requires box half extents to be at least
// its convex radius; the same lower bound is applied to spheres for consistency.
inline constexpr core::f32 minimum_shape_extent = 0.05F;

[[nodiscard]] core::Status validate_config(const PhysicsConfig& config) noexcept;
[[nodiscard]] core::Status validate_body_desc(const BodyDesc& desc) noexcept;

// Owns one Jolt simulation. All memory (body storage, temp allocator, entity records) is reserved
// in initialize(); update() and write_transforms() do not allocate. Not thread-safe: a world must
// be used from one thread at a time. Creating and destroying worlds is serialized internally
// because Jolt's type registry is process-global.
class PhysicsWorld final {
public:
    PhysicsWorld() noexcept = default;
    ~PhysicsWorld() noexcept;

    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    // Allocates the simulation. Expensive: reserves all memory sized by the config.
    [[nodiscard]] core::Status initialize(const PhysicsConfig& config = {}) noexcept;
    void shutdown() noexcept;
    [[nodiscard]] bool initialized() const noexcept { return impl_ != nullptr; }

    // Creates a body at the entity's current local transform. The entity must be alive, have a
    // transform, have no parent and not already own a body. Scale is ignored by the simulation.
    [[nodiscard]] core::Status add_body(const scene::Scene& scene,
                                        scene::Entity entity,
                                        const BodyDesc& desc) noexcept;
    [[nodiscard]] core::Status remove_body(scene::Entity entity) noexcept;
    [[nodiscard]] bool has_body(scene::Entity entity) const noexcept;

    // Advances the simulation in fixed steps. Time beyond max_steps_per_update is dropped so a
    // long frame cannot trigger an unbounded catch-up loop.
    [[nodiscard]] core::Status update(core::f32 delta_seconds,
                                      core::u32* steps_taken = nullptr) noexcept;

    // Copies dynamic body poses into the scene transforms and refreshes world matrices. Fails
    // without writing anything when a tracked entity was destroyed or re-parented.
    [[nodiscard]] core::Status write_transforms(scene::Scene& scene) const noexcept;

    [[nodiscard]] core::Status body_pose(scene::Entity entity,
                                         math::Vec3& position,
                                         math::Quaternion& rotation) const noexcept;
    [[nodiscard]] PhysicsMetrics metrics() const noexcept;

private:
    class Impl;
    Impl* impl_ = nullptr;
};

} // namespace gameengine::physics
