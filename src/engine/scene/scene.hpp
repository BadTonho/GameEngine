#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "engine/core/status.hpp"
#include "engine/core/types.hpp"
#include "engine/math/math.hpp"

namespace gameengine::scene {

struct Entity final {
    core::u64 value = 0;

    [[nodiscard]] static constexpr Entity make(core::u32 index, core::u32 generation) noexcept
    {
        return {static_cast<core::u64>(index) |
                (static_cast<core::u64>(generation) << 32U)};
    }

    [[nodiscard]] constexpr bool valid() const noexcept
    {
        return value != 0 && generation() != 0;
    }

    [[nodiscard]] constexpr core::u32 index() const noexcept
    {
        return static_cast<core::u32>(value & 0xffffffffULL);
    }

    [[nodiscard]] constexpr core::u32 generation() const noexcept
    {
        return static_cast<core::u32>(value >> 32U);
    }

    friend constexpr bool operator==(Entity, Entity) noexcept = default;
};

inline constexpr Entity invalid_entity{};
inline constexpr core::u64 bootstrap_mesh_id = 0x424f4f545354524dULL;
inline constexpr core::u64 bootstrap_material_id = 0x424f4f544d41544cULL;

struct TransformComponent final {
    math::Vec3 local_position{};
    math::Quaternion local_rotation = math::Quaternion::identity();
    math::Vec3 local_scale{1.0F, 1.0F, 1.0F};
    Entity parent{};
    math::Mat4 world_matrix = math::Mat4::identity();
};

struct MeshRendererComponent final {
    core::u64 mesh_id = 0;
    core::u64 material_id = 0;
};

struct CameraComponent final {
    core::f32 vertical_field_of_view_radians = 1.04719755F;
    core::f32 near_plane = 0.1F;
    core::f32 far_plane = 100.0F;
    bool active = false;
};

struct DirectionalLightComponent final {
    math::Vec3 direction{-0.45F, -0.8F, -0.35F};
    math::Vec3 color{1.0F, 1.0F, 1.0F};
    core::f32 intensity = 4.0F;
};

class Scene final {
public:
    Scene() noexcept;
    ~Scene() noexcept;

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    [[nodiscard]] Entity create_entity();
    [[nodiscard]] core::Status adopt_entity(Entity entity) noexcept;
    [[nodiscard]] core::Status destroy_entity(Entity entity) noexcept;
    [[nodiscard]] bool is_alive(Entity entity) const noexcept;
    [[nodiscard]] core::usize entity_count() const noexcept { return active_entities_.size(); }
    [[nodiscard]] std::span<const Entity> entities() const noexcept { return active_entities_; }
    void clear() noexcept;
    void reserve(core::usize entity_count);

    [[nodiscard]] core::Status add_transform(
        Entity entity, const TransformComponent& component = {}) noexcept;
    [[nodiscard]] core::Status add_mesh_renderer(
        Entity entity, const MeshRendererComponent& component = {}) noexcept;
    [[nodiscard]] core::Status add_camera(
        Entity entity, const CameraComponent& component = {}) noexcept;
    [[nodiscard]] core::Status add_directional_light(
        Entity entity, const DirectionalLightComponent& component = {}) noexcept;

    [[nodiscard]] TransformComponent* transform(Entity entity) noexcept;
    [[nodiscard]] const TransformComponent* transform(Entity entity) const noexcept;
    [[nodiscard]] MeshRendererComponent* mesh_renderer(Entity entity) noexcept;
    [[nodiscard]] const MeshRendererComponent* mesh_renderer(Entity entity) const noexcept;
    [[nodiscard]] CameraComponent* camera(Entity entity) noexcept;
    [[nodiscard]] const CameraComponent* camera(Entity entity) const noexcept;
    [[nodiscard]] DirectionalLightComponent* directional_light(Entity entity) noexcept;
    [[nodiscard]] const DirectionalLightComponent* directional_light(Entity entity) const noexcept;

    [[nodiscard]] core::Status set_parent(Entity child, Entity parent) noexcept;
    [[nodiscard]] core::Status update_transforms() noexcept;
    [[nodiscard]] core::Status validate() const noexcept;
    [[nodiscard]] Entity active_camera() const noexcept;
    [[nodiscard]] core::usize memory_usage_bytes() const noexcept;
    void swap(Scene& other) noexcept;

private:
    [[nodiscard]] core::u32 next_generation(core::u32 generation) const noexcept;

    std::vector<core::u32> generations_;
    std::vector<core::u8> alive_;
    std::vector<core::u32> child_counts_;
    std::vector<core::u32> active_positions_;
    std::vector<core::u32> free_indices_;
    std::vector<Entity> active_entities_;
    std::vector<core::u8> traversal_state_;
    std::vector<Entity> traversal_chain_;

    class Storage;
    Storage* storage_ = nullptr;
};

[[nodiscard]] core::Status create_bootstrap_scene(Scene& scene) noexcept;

} // namespace gameengine::scene
