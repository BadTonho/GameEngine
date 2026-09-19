#include "engine/scene/scene.hpp"

#include "engine/scene/sparse_set.hpp"

#include <algorithm>
#include <cmath>
#include <new>

namespace gameengine::scene {

namespace {

constexpr core::u32 max_scene_entities = 1'000'000U;

} // namespace

class Scene::Storage final {
public:
    SparseSet<TransformComponent> transforms;
    SparseSet<MeshRendererComponent> mesh_renderers;
    SparseSet<CameraComponent> cameras;
    SparseSet<DirectionalLightComponent> lights;
};

Scene::Scene() noexcept : storage_(new (std::nothrow) Storage{}) {}

Scene::~Scene() noexcept
{
    delete storage_;
    storage_ = nullptr;
}

Entity Scene::create_entity()
{
    if (storage_ == nullptr) {
        return invalid_entity;
    }
    if (generations_.size() >= max_scene_entities && free_indices_.empty()) {
        return invalid_entity;
    }
    core::u32 index = 0;
    if (!free_indices_.empty()) {
        index = free_indices_.back();
        free_indices_.pop_back();
    } else {
        index = static_cast<core::u32>(generations_.size());
        generations_.push_back(1U);
        alive_.push_back(0U);
        child_counts_.push_back(0U);
        active_positions_.push_back(0U);
        traversal_state_.push_back(0U);
    }
    alive_[index] = 1U;
    const Entity entity = Entity::make(index, generations_[index]);
    active_positions_[index] = static_cast<core::u32>(active_entities_.size());
    active_entities_.push_back(entity);
    return entity;
}

core::Status Scene::adopt_entity(Entity entity) noexcept
{
    if (storage_ == nullptr || !entity.valid() || entity.index() >= max_scene_entities ||
        is_alive(entity)) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    const core::u32 required_size = entity.index() + 1U;
    if (required_size == 0U) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    const core::usize old_size = generations_.size();
    if (required_size > generations_.size()) {
        generations_.resize(required_size, 1U);
        alive_.resize(required_size, 0U);
        child_counts_.resize(required_size, 0U);
        active_positions_.resize(required_size, 0U);
        traversal_state_.resize(required_size, 0U);
        for (core::u32 index = static_cast<core::u32>(old_size); index < entity.index(); ++index) {
            free_indices_.push_back(index);
        }
    }
    if (alive_[entity.index()] != 0U || generations_[entity.index()] != 1U &&
        generations_[entity.index()] != entity.generation()) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    generations_[entity.index()] = entity.generation();
    alive_[entity.index()] = 1U;
    active_positions_[entity.index()] = static_cast<core::u32>(active_entities_.size());
    active_entities_.push_back(entity);
    return core::Status{};
}

core::Status Scene::destroy_entity(Entity entity) noexcept
{
    if (!is_alive(entity) || storage_ == nullptr) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    TransformComponent* destroyed_transform = storage_->transforms.get(entity);
    if (destroyed_transform != nullptr && destroyed_transform->parent.valid()) {
        --child_counts_[destroyed_transform->parent.index()];
    }
    if (child_counts_[entity.index()] != 0U) {
        for (const Entity candidate : storage_->transforms.entities()) {
            if (candidate != entity) {
                TransformComponent* component = storage_->transforms.get(candidate);
                if (component != nullptr && component->parent == entity) {
                    component->parent = invalid_entity;
                    --child_counts_[entity.index()];
                }
            }
        }
    }
    storage_->transforms.remove(entity);
    storage_->mesh_renderers.remove(entity);
    storage_->cameras.remove(entity);
    storage_->lights.remove(entity);

    const core::u32 active_position = active_positions_[entity.index()];
    const Entity last_entity = active_entities_.back();
    active_entities_[active_position] = last_entity;
    active_positions_[last_entity.index()] = active_position;
    active_entities_.pop_back();
    alive_[entity.index()] = 0U;
    generations_[entity.index()] = next_generation(generations_[entity.index()]);
    free_indices_.push_back(entity.index());
    return core::Status{};
}

bool Scene::is_alive(Entity entity) const noexcept
{
    return storage_ != nullptr && entity.valid() && entity.index() < generations_.size() &&
           alive_[entity.index()] != 0U && generations_[entity.index()] == entity.generation();
}

void Scene::clear() noexcept
{
    if (storage_ != nullptr) {
        storage_->transforms.clear();
        storage_->mesh_renderers.clear();
        storage_->cameras.clear();
        storage_->lights.clear();
    }
    generations_.clear();
    alive_.clear();
    child_counts_.clear();
    active_positions_.clear();
    free_indices_.clear();
    active_entities_.clear();
    traversal_state_.clear();
    traversal_chain_.clear();
}

void Scene::reserve(core::usize entity_count)
{
    generations_.reserve(entity_count);
    alive_.reserve(entity_count);
    child_counts_.reserve(entity_count);
    active_positions_.reserve(entity_count);
    active_entities_.reserve(entity_count);
    traversal_state_.reserve(entity_count);
}

core::Status Scene::add_transform(Entity entity, const TransformComponent& component) noexcept
{
    const auto finite = [](const math::Vec3& value) noexcept {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    };
    const math::Quaternion rotation = component.local_rotation;
    const core::f32 rotation_length = rotation.x * rotation.x + rotation.y * rotation.y +
                                      rotation.z * rotation.z + rotation.w * rotation.w;
    if (!is_alive(entity) || storage_ == nullptr || !finite(component.local_position) ||
        !finite(component.local_scale) || !std::isfinite(rotation_length) ||
        rotation_length <= 0.0F ||
        storage_->transforms.emplace(entity, component) == nullptr) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    return core::Status{};
}

core::Status Scene::add_mesh_renderer(Entity entity,
                                      const MeshRendererComponent& component) noexcept
{
    if (!is_alive(entity) || storage_ == nullptr || transform(entity) == nullptr ||
        storage_->mesh_renderers.emplace(entity, component) == nullptr) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    return core::Status{};
}

core::Status Scene::add_camera(Entity entity, const CameraComponent& component) noexcept
{
    if (!is_alive(entity) || storage_ == nullptr || transform(entity) == nullptr ||
        !std::isfinite(component.vertical_field_of_view_radians) ||
        !std::isfinite(component.near_plane) || !std::isfinite(component.far_plane) ||
        component.vertical_field_of_view_radians <= 0.0F || component.near_plane <= 0.0F ||
        component.far_plane <= component.near_plane ||
        storage_->cameras.emplace(entity, component) == nullptr) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    return core::Status{};
}

core::Status Scene::add_directional_light(Entity entity,
                                          const DirectionalLightComponent& component) noexcept
{
    const auto finite = [](const math::Vec3& value) noexcept {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    };
    if (!is_alive(entity) || storage_ == nullptr || transform(entity) == nullptr ||
        !std::isfinite(component.intensity) || component.intensity < 0.0F ||
        !finite(component.direction) || !finite(component.color) ||
        storage_->lights.emplace(entity, component) == nullptr) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    return core::Status{};
}

TransformComponent* Scene::transform(Entity entity) noexcept
{
    return storage_ == nullptr || !is_alive(entity) ? nullptr : storage_->transforms.get(entity);
}

const TransformComponent* Scene::transform(Entity entity) const noexcept
{
    return storage_ == nullptr || !is_alive(entity) ? nullptr : storage_->transforms.get(entity);
}

MeshRendererComponent* Scene::mesh_renderer(Entity entity) noexcept
{
    return storage_ == nullptr || !is_alive(entity) ? nullptr : storage_->mesh_renderers.get(entity);
}

const MeshRendererComponent* Scene::mesh_renderer(Entity entity) const noexcept
{
    return storage_ == nullptr || !is_alive(entity) ? nullptr : storage_->mesh_renderers.get(entity);
}

CameraComponent* Scene::camera(Entity entity) noexcept
{
    return storage_ == nullptr || !is_alive(entity) ? nullptr : storage_->cameras.get(entity);
}

const CameraComponent* Scene::camera(Entity entity) const noexcept
{
    return storage_ == nullptr || !is_alive(entity) ? nullptr : storage_->cameras.get(entity);
}

DirectionalLightComponent* Scene::directional_light(Entity entity) noexcept
{
    return storage_ == nullptr || !is_alive(entity) ? nullptr : storage_->lights.get(entity);
}

const DirectionalLightComponent* Scene::directional_light(Entity entity) const noexcept
{
    return storage_ == nullptr || !is_alive(entity) ? nullptr : storage_->lights.get(entity);
}

core::Status Scene::set_parent(Entity child, Entity parent) noexcept
{
    if (!is_alive(child) || transform(child) == nullptr ||
        (parent.valid() && (!is_alive(parent) || transform(parent) == nullptr)) ||
        child == parent) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    Entity current = parent;
    while (current.valid()) {
        if (current == child) {
            return core::Status{core::ErrorCode::invalid_argument};
        }
        const TransformComponent* current_transform = transform(current);
        if (current_transform == nullptr) {
            return core::Status{core::ErrorCode::invalid_argument};
        }
        current = current_transform->parent;
    }
    TransformComponent* child_transform = transform(child);
    if (child_transform->parent == parent) {
        return core::Status{};
    }
    if (child_transform->parent.valid()) {
        --child_counts_[child_transform->parent.index()];
    }
    child_transform->parent = parent;
    if (parent.valid()) {
        ++child_counts_[parent.index()];
    }
    return core::Status{};
}

core::Status Scene::update_transforms() noexcept
{
    if (validate().code != core::ErrorCode::none) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    std::fill(traversal_state_.begin(), traversal_state_.end(), static_cast<core::u8>(0U));
    traversal_chain_.clear();
    for (const Entity entity : active_entities_) {
        if (storage_->transforms.get(entity) == nullptr || traversal_state_[entity.index()] == 2U) {
            continue;
        }
        Entity current = entity;
        while (current.valid() && traversal_state_[current.index()] != 2U) {
            if (traversal_state_[current.index()] == 1U) {
                return core::Status{core::ErrorCode::invalid_argument};
            }
            const TransformComponent* current_transform = transform(current);
            if (current_transform == nullptr) {
                return core::Status{core::ErrorCode::invalid_argument};
            }
            traversal_state_[current.index()] = 1U;
            traversal_chain_.push_back(current);
            current = current_transform->parent;
        }
        while (!traversal_chain_.empty()) {
            const Entity update_entity = traversal_chain_.back();
            traversal_chain_.pop_back();
            TransformComponent* component = transform(update_entity);
            const math::Mat4 local = math::compose_transform(
                component->local_position,
                math::normalize(component->local_rotation),
                component->local_scale);
            const TransformComponent* parent_component = transform(component->parent);
            component->world_matrix = parent_component == nullptr
                ? local
                : math::multiply(parent_component->world_matrix, local);
            traversal_state_[update_entity.index()] = 2U;
        }
    }
    return core::Status{};
}

core::Status Scene::validate() const noexcept
{
    if (storage_ == nullptr) {
        return core::Status{core::ErrorCode::allocation_failed};
    }
    core::u32 active_camera_count = 0;
    for (const Entity entity : active_entities_) {
        const TransformComponent* transform_component = transform(entity);
        if (transform_component != nullptr) {
            const auto finite = [](const math::Vec3& value) noexcept {
                return std::isfinite(value.x) && std::isfinite(value.y) &&
                    std::isfinite(value.z);
            };
            const math::Quaternion rotation = transform_component->local_rotation;
            const core::f32 rotation_length = rotation.x * rotation.x + rotation.y * rotation.y +
                                              rotation.z * rotation.z + rotation.w * rotation.w;
            if (!finite(transform_component->local_position) ||
                !finite(transform_component->local_scale) ||
                !std::isfinite(rotation_length) || rotation_length <= 0.0F) {
                return core::Status{core::ErrorCode::invalid_argument};
            }
            Entity ancestor = transform_component->parent;
            for (core::usize depth = 0; ancestor.valid(); ++depth) {
                if (depth >= active_entities_.size() || ancestor == entity ||
                    !is_alive(ancestor) || transform(ancestor) == nullptr) {
                    return core::Status{core::ErrorCode::invalid_argument};
                }
                ancestor = transform(ancestor)->parent;
            }
        }
        if (const CameraComponent* camera_component = camera(entity); camera_component != nullptr) {
            if (!std::isfinite(camera_component->vertical_field_of_view_radians) ||
                !std::isfinite(camera_component->near_plane) ||
                !std::isfinite(camera_component->far_plane) ||
                camera_component->vertical_field_of_view_radians <= 0.0F ||
                camera_component->near_plane <= 0.0F ||
                camera_component->far_plane <= camera_component->near_plane) {
                return core::Status{core::ErrorCode::invalid_argument};
            }
            if (camera_component->active) {
                ++active_camera_count;
            }
        }
        if (const DirectionalLightComponent* light = directional_light(entity); light != nullptr) {
            if (!std::isfinite(light->direction.x) || !std::isfinite(light->direction.y) ||
                !std::isfinite(light->direction.z) || !std::isfinite(light->color.x) ||
                !std::isfinite(light->color.y) || !std::isfinite(light->color.z) ||
                !std::isfinite(light->intensity) || light->intensity < 0.0F) {
                return core::Status{core::ErrorCode::invalid_argument};
            }
        }
        if (const MeshRendererComponent* mesh = mesh_renderer(entity); mesh != nullptr &&
            (mesh->mesh_id == 0 || mesh->material_id == 0)) {
            return core::Status{core::ErrorCode::invalid_argument};
        }
        if ((mesh_renderer(entity) != nullptr || camera(entity) != nullptr ||
             directional_light(entity) != nullptr) && transform_component == nullptr) {
            return core::Status{core::ErrorCode::invalid_argument};
        }
    }
    if (active_camera_count > 1U) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    return core::Status{};
}

Entity Scene::active_camera() const noexcept
{
    if (storage_ == nullptr) {
        return invalid_entity;
    }
    for (const Entity entity : active_entities_) {
        const CameraComponent* component = camera(entity);
        if (component != nullptr && component->active) {
            return entity;
        }
    }
    return invalid_entity;
}

core::usize Scene::memory_usage_bytes() const noexcept
{
    if (storage_ == nullptr) {
        return 0;
    }
    return generations_.capacity() * sizeof(core::u32) + alive_.capacity() * sizeof(core::u8) +
        child_counts_.capacity() * sizeof(core::u32) +
        active_positions_.capacity() * sizeof(core::u32) + free_indices_.capacity() * sizeof(core::u32) +
        active_entities_.capacity() * sizeof(Entity) + traversal_state_.capacity() * sizeof(core::u8) +
        traversal_chain_.capacity() * sizeof(Entity) + storage_->transforms.memory_usage_bytes() +
        storage_->mesh_renderers.memory_usage_bytes() + storage_->cameras.memory_usage_bytes() +
        storage_->lights.memory_usage_bytes();
}

void Scene::swap(Scene& other) noexcept
{
    using std::swap;
    swap(generations_, other.generations_);
    swap(alive_, other.alive_);
    swap(child_counts_, other.child_counts_);
    swap(active_positions_, other.active_positions_);
    swap(free_indices_, other.free_indices_);
    swap(active_entities_, other.active_entities_);
    swap(traversal_state_, other.traversal_state_);
    swap(traversal_chain_, other.traversal_chain_);
    swap(storage_, other.storage_);
}

core::u32 Scene::next_generation(core::u32 generation) const noexcept
{
    return generation == 0xffffffffU ? 1U : generation + 1U;
}

core::Status create_bootstrap_scene(Scene& scene) noexcept
{
    scene.clear();
    scene.reserve(3U);
    const Entity cube = scene.create_entity();
    const Entity camera = scene.create_entity();
    const Entity light = scene.create_entity();
    if (!cube.valid() || !camera.valid() || !light.valid()) {
        return core::Status{core::ErrorCode::allocation_failed};
    }
    const math::Quaternion yaw = math::quaternion_from_axis_angle({0.0F, 1.0F, 0.0F}, 0.65F);
    const math::Quaternion pitch =
        math::quaternion_from_axis_angle({1.0F, 0.0F, 0.0F}, -0.4F);
    if (!scene.add_transform(cube,
                             {.local_rotation = math::normalize(math::multiply(yaw, pitch))}) ||
        !scene.add_mesh_renderer(cube, {bootstrap_mesh_id, bootstrap_material_id}) ||
        !scene.add_transform(camera, {.local_position = {2.5F, 2.0F, 4.0F}}) ||
        !scene.add_camera(camera, {.active = true}) || !scene.add_transform(light) ||
        !scene.add_directional_light(light) || !scene.update_transforms()) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    return core::Status{};
}

} // namespace gameengine::scene
