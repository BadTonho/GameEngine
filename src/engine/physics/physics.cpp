#include "engine/physics/physics.hpp"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <new>
#include <vector>

#include "engine/core/diagnostics.hpp"

// Jolt.h must be the first Jolt header in every translation unit.
#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

namespace gameengine::physics {
namespace {

namespace object_layers {
constexpr JPH::ObjectLayer non_moving = 0;
constexpr JPH::ObjectLayer moving = 1;
} // namespace object_layers

namespace broad_phase_layers {
constexpr JPH::BroadPhaseLayer non_moving{0};
constexpr JPH::BroadPhaseLayer moving{1};
constexpr JPH::uint count = 2U;
} // namespace broad_phase_layers

class BroadPhaseLayerMapping final : public JPH::BroadPhaseLayerInterface {
public:
    [[nodiscard]] JPH::uint GetNumBroadPhaseLayers() const override
    {
        return broad_phase_layers::count;
    }

    [[nodiscard]] JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override
    {
        return layer == object_layers::moving ? broad_phase_layers::moving
                                              : broad_phase_layers::non_moving;
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    [[nodiscard]] const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override
    {
        return layer == broad_phase_layers::moving ? "moving" : "non_moving";
    }
#endif
};

class ObjectVsBroadPhaseFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer layer,
                                     JPH::BroadPhaseLayer broad_phase_layer) const override
    {
        // Static bodies never need to test against other static bodies.
        return layer == object_layers::moving || broad_phase_layer == broad_phase_layers::moving;
    }
};

class ObjectPairFilter final : public JPH::ObjectLayerPairFilter {
public:
    [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer first, JPH::ObjectLayer second) const override
    {
        return first == object_layers::moving || second == object_layers::moving;
    }
};

void trace_to_log(const char* format, ...)
{
    char buffer[512];
    va_list arguments;
    va_start(arguments, format);
    const int written = std::vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    if (written > 0) {
        core::log(core::LogLevel::warning, buffer);
    }
}

#ifdef JPH_ENABLE_ASSERTS
bool assert_to_log(const char* expression, const char* message, const char* file, JPH::uint line)
{
    char buffer[768];
    std::snprintf(buffer, sizeof(buffer), "jolt assert: %s (%s) at %s:%u", expression,
                  message != nullptr ? message : "", file, static_cast<unsigned>(line));
    core::log(core::LogLevel::error, buffer);
    return true;
}
#endif

// Jolt keeps its allocator hooks, factory and type registry in process-global state. Worlds share
// that state through a reference count so independent worlds can coexist and shut down safely.
std::mutex registry_mutex;
core::u32 registry_references = 0;

void acquire_registry() noexcept
{
    const std::lock_guard<std::mutex> lock(registry_mutex);
    if (registry_references == 0U) {
        JPH::RegisterDefaultAllocator();
        JPH::Trace = trace_to_log;
        JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = assert_to_log;)
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
    }
    ++registry_references;
}

void release_registry() noexcept
{
    const std::lock_guard<std::mutex> lock(registry_mutex);
    GAMEENGINE_ASSERT(registry_references > 0U);
    --registry_references;
    if (registry_references == 0U) {
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
}

[[nodiscard]] bool positive_finite(core::f32 value) noexcept
{
    return std::isfinite(value) && value > 0.0F;
}

[[nodiscard]] bool is_finite(const math::Quaternion& value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) &&
           std::isfinite(value.w);
}

struct BodyRecord final {
    scene::Entity entity{};
    JPH::BodyID body{};
    MotionType motion = MotionType::static_body;
};

} // namespace

core::Status validate_config(const PhysicsConfig& config) noexcept
{
    if (config.max_bodies == 0U || config.max_body_pairs == 0U ||
        config.max_contact_constraints == 0U || config.temp_allocator_bytes < 64U * 1024U ||
        config.max_steps_per_update == 0U || !positive_finite(config.fixed_step_seconds) ||
        !math::is_finite(config.gravity)) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    // Jolt reserves body storage up front; keep requests inside its documented body ID range.
    if (config.max_bodies > JPH::BodyID::cMaxBodyIndex) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    return {};
}

core::Status validate_body_desc(const BodyDesc& desc) noexcept
{
    switch (desc.shape) {
    case ShapeType::box:
        if (!math::is_finite(desc.half_extents) || desc.half_extents.x < minimum_shape_extent ||
            desc.half_extents.y < minimum_shape_extent ||
            desc.half_extents.z < minimum_shape_extent) {
            return core::Status{core::ErrorCode::invalid_argument};
        }
        break;
    case ShapeType::sphere:
        if (!std::isfinite(desc.radius) || desc.radius < minimum_shape_extent) {
            return core::Status{core::ErrorCode::invalid_argument};
        }
        break;
    default:
        return core::Status{core::ErrorCode::invalid_argument};
    }

    if (desc.motion != MotionType::static_body && desc.motion != MotionType::dynamic_body) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    if (desc.motion == MotionType::dynamic_body && !positive_finite(desc.mass_kilograms)) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    if (!std::isfinite(desc.friction) || desc.friction < 0.0F || !std::isfinite(desc.restitution) ||
        desc.restitution < 0.0F || desc.restitution > 1.0F) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    return {};
}

class PhysicsWorld::Impl final {
public:
    explicit Impl(const PhysicsConfig& world_config)
        : config(world_config),
          temp_allocator(world_config.temp_allocator_bytes),
          job_system(JPH::cMaxPhysicsJobs)
    {
    }

    [[nodiscard]] std::vector<BodyRecord>::iterator lower_bound(scene::Entity entity) noexcept
    {
        return std::lower_bound(records.begin(), records.end(), entity.index(),
                                [](const BodyRecord& record, core::u32 index) noexcept {
                                    return record.entity.index() < index;
                                });
    }

    [[nodiscard]] const BodyRecord* find(scene::Entity entity) const noexcept
    {
        const auto iterator =
            std::lower_bound(records.begin(), records.end(), entity.index(),
                             [](const BodyRecord& record, core::u32 index) noexcept {
                                 return record.entity.index() < index;
                             });
        if (iterator == records.end() || iterator->entity != entity) {
            return nullptr;
        }
        return &*iterator;
    }

    PhysicsConfig config;
    JPH::TempAllocatorImpl temp_allocator;
    JPH::JobSystemSingleThreaded job_system;
    // Layer objects are referenced by the physics system and must outlive it.
    BroadPhaseLayerMapping broad_phase_layer_mapping;
    ObjectVsBroadPhaseFilter object_vs_broad_phase_filter;
    ObjectPairFilter object_pair_filter;
    JPH::PhysicsSystem system;
    // Sorted by entity index so lookups are logarithmic and iteration order is deterministic.
    std::vector<BodyRecord> records;
    core::f32 accumulator_seconds = 0.0F;
    PhysicsMetrics metrics{};
};

PhysicsWorld::~PhysicsWorld() noexcept
{
    shutdown();
}

core::Status PhysicsWorld::initialize(const PhysicsConfig& config) noexcept
{
    if (impl_ != nullptr) {
        return core::Status{core::ErrorCode::already_initialized};
    }
    const core::Status config_status = validate_config(config);
    if (!config_status) {
        return config_status;
    }

    acquire_registry();
    impl_ = new (std::nothrow) Impl(config);
    if (impl_ == nullptr) {
        release_registry();
        return core::Status{core::ErrorCode::allocation_failed};
    }

    impl_->system.Init(config.max_bodies, 0U, config.max_body_pairs,
                       config.max_contact_constraints, impl_->broad_phase_layer_mapping,
                       impl_->object_vs_broad_phase_filter, impl_->object_pair_filter);
    impl_->system.SetGravity(JPH::Vec3(config.gravity.x, config.gravity.y, config.gravity.z));
    impl_->records.reserve(config.max_bodies);
    return {};
}

void PhysicsWorld::shutdown() noexcept
{
    if (impl_ == nullptr) {
        return;
    }
    JPH::BodyInterface& bodies = impl_->system.GetBodyInterface();
    for (const BodyRecord& record : impl_->records) {
        bodies.RemoveBody(record.body);
        bodies.DestroyBody(record.body);
    }
    impl_->records.clear();
    delete impl_;
    impl_ = nullptr;
    release_registry();
}

core::Status PhysicsWorld::add_body(const scene::Scene& scene,
                                    scene::Entity entity,
                                    const BodyDesc& desc) noexcept
{
    if (impl_ == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    const core::Status desc_status = validate_body_desc(desc);
    if (!desc_status) {
        return desc_status;
    }
    if (!scene.is_alive(entity)) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    const scene::TransformComponent* transform = scene.transform(entity);
    if (transform == nullptr || transform->parent.valid() ||
        !math::is_finite(transform->local_position) || !is_finite(transform->local_rotation)) {
        return core::Status{core::ErrorCode::invalid_argument};
    }

    const auto insert_position = impl_->lower_bound(entity);
    if (insert_position != impl_->records.end() &&
        insert_position->entity.index() == entity.index()) {
        // Either a duplicate or a stale record whose entity was destroyed without remove_body().
        return core::Status{core::ErrorCode::invalid_argument};
    }
    if (impl_->records.size() >= impl_->config.max_bodies) {
        return core::Status{core::ErrorCode::physics_capacity_exceeded};
    }

    JPH::ShapeSettings::ShapeResult shape_result;
    if (desc.shape == ShapeType::box) {
        JPH::BoxShapeSettings settings(
            JPH::Vec3(desc.half_extents.x, desc.half_extents.y, desc.half_extents.z));
        settings.SetEmbedded();
        shape_result = settings.Create();
    } else {
        JPH::SphereShapeSettings settings(desc.radius);
        settings.SetEmbedded();
        shape_result = settings.Create();
    }
    if (shape_result.HasError()) {
        return core::Status{core::ErrorCode::invalid_argument};
    }

    const math::Quaternion rotation = math::normalize(transform->local_rotation);
    const bool dynamic = desc.motion == MotionType::dynamic_body;
    JPH::BodyCreationSettings settings(
        shape_result.Get(),
        JPH::RVec3(transform->local_position.x, transform->local_position.y,
                   transform->local_position.z),
        JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w),
        dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
        dynamic ? object_layers::moving : object_layers::non_moving);
    settings.mFriction = desc.friction;
    settings.mRestitution = desc.restitution;
    if (dynamic) {
        settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = desc.mass_kilograms;
    }

    const JPH::BodyID body = impl_->system.GetBodyInterface().CreateAndAddBody(
        settings, dynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
    if (body.IsInvalid()) {
        return core::Status{core::ErrorCode::physics_capacity_exceeded};
    }

    // Capacity was reserved in initialize(), so this insert never reallocates.
    impl_->records.insert(insert_position, BodyRecord{entity, body, desc.motion});
    ++impl_->metrics.body_count;
    if (dynamic) {
        ++impl_->metrics.dynamic_body_count;
    }
    return {};
}

core::Status PhysicsWorld::remove_body(scene::Entity entity) noexcept
{
    if (impl_ == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    const auto iterator = impl_->lower_bound(entity);
    if (iterator == impl_->records.end() || iterator->entity != entity) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    JPH::BodyInterface& bodies = impl_->system.GetBodyInterface();
    bodies.RemoveBody(iterator->body);
    bodies.DestroyBody(iterator->body);
    --impl_->metrics.body_count;
    if (iterator->motion == MotionType::dynamic_body) {
        --impl_->metrics.dynamic_body_count;
    }
    impl_->records.erase(iterator);
    return {};
}

bool PhysicsWorld::has_body(scene::Entity entity) const noexcept
{
    return impl_ != nullptr && impl_->find(entity) != nullptr;
}

core::Status PhysicsWorld::update(core::f32 delta_seconds, core::u32* steps_taken) noexcept
{
    if (steps_taken != nullptr) {
        *steps_taken = 0U;
    }
    if (impl_ == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    if (!std::isfinite(delta_seconds) || delta_seconds < 0.0F) {
        return core::Status{core::ErrorCode::invalid_argument};
    }

    const core::f32 step = impl_->config.fixed_step_seconds;
    impl_->accumulator_seconds += delta_seconds;
    core::u32 steps = 0U;
    while (impl_->accumulator_seconds >= step && steps < impl_->config.max_steps_per_update) {
        const JPH::EPhysicsUpdateError error =
            impl_->system.Update(step, 1, &impl_->temp_allocator, &impl_->job_system);
        impl_->accumulator_seconds -= step;
        ++steps;
        ++impl_->metrics.total_steps;
        if (steps_taken != nullptr) {
            *steps_taken = steps;
        }
        if (error != JPH::EPhysicsUpdateError::None) {
            // The step completed but some contacts were ignored; the configured pair or
            // constraint budget is too small for the scene.
            return core::Status{core::ErrorCode::physics_capacity_exceeded};
        }
    }
    if (impl_->accumulator_seconds >= step) {
        const core::f32 dropped = std::floor(impl_->accumulator_seconds / step);
        impl_->metrics.dropped_steps += static_cast<core::u64>(dropped);
        impl_->accumulator_seconds -= dropped * step;
    }
    return {};
}

core::Status PhysicsWorld::write_transforms(scene::Scene& scene) const noexcept
{
    if (impl_ == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    // Validate everything first so a stale record never leaves the scene half updated.
    for (const BodyRecord& record : impl_->records) {
        if (!scene.is_alive(record.entity)) {
            return core::Status{core::ErrorCode::invalid_argument};
        }
        const scene::TransformComponent* transform = scene.transform(record.entity);
        if (transform == nullptr || transform->parent.valid()) {
            return core::Status{core::ErrorCode::invalid_argument};
        }
    }

    const JPH::BodyInterface& bodies = impl_->system.GetBodyInterface();
    bool changed = false;
    for (const BodyRecord& record : impl_->records) {
        if (record.motion != MotionType::dynamic_body) {
            continue;
        }
        JPH::RVec3 position;
        JPH::Quat rotation;
        bodies.GetPositionAndRotation(record.body, position, rotation);
        scene::TransformComponent* transform = scene.transform(record.entity);
        transform->local_position = {static_cast<core::f32>(position.GetX()),
                                     static_cast<core::f32>(position.GetY()),
                                     static_cast<core::f32>(position.GetZ())};
        transform->local_rotation = {rotation.GetX(), rotation.GetY(), rotation.GetZ(),
                                     rotation.GetW()};
        changed = true;
    }
    return changed ? scene.update_transforms() : core::Status{};
}

core::Status PhysicsWorld::body_pose(scene::Entity entity,
                                     math::Vec3& position,
                                     math::Quaternion& rotation) const noexcept
{
    if (impl_ == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    const BodyRecord* record = impl_->find(entity);
    if (record == nullptr) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    JPH::RVec3 body_position;
    JPH::Quat body_rotation;
    impl_->system.GetBodyInterface().GetPositionAndRotation(record->body, body_position,
                                                            body_rotation);
    position = {static_cast<core::f32>(body_position.GetX()),
                static_cast<core::f32>(body_position.GetY()),
                static_cast<core::f32>(body_position.GetZ())};
    rotation = {body_rotation.GetX(), body_rotation.GetY(), body_rotation.GetZ(),
                body_rotation.GetW()};
    return {};
}

PhysicsMetrics PhysicsWorld::metrics() const noexcept
{
    return impl_ != nullptr ? impl_->metrics : PhysicsMetrics{};
}

} // namespace gameengine::physics
