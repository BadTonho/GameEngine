#include "engine/assets/asset_reader.hpp"
#include "engine/math/math.hpp"
#include "engine/scene/scene.hpp"
#include "engine/scene/scene_serialization.hpp"
#include "engine/scene/sparse_set.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

[[nodiscard]] bool near(float left, float right, float epsilon = 0.0001F) noexcept
{
    return std::fabs(left - right) <= epsilon;
}

void put_u32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value)
{
    for (std::size_t index = 0; index < 4U; ++index) {
        bytes[offset + index] =
            std::byte{static_cast<unsigned char>((value >> (index * 8U)) & 0xffU)};
    }
}

void put_u64(std::vector<std::byte>& bytes, std::size_t offset, std::uint64_t value)
{
    for (std::size_t index = 0; index < 8U; ++index) {
        bytes[offset + index] =
            std::byte{static_cast<unsigned char>((value >> (index * 8U)) & 0xffU)};
    }
}

void append_u32(std::vector<std::byte>& bytes, std::uint32_t value)
{
    const std::size_t offset = bytes.size();
    bytes.resize(offset + 4U);
    put_u32(bytes, offset, value);
}

void append_u64(std::vector<std::byte>& bytes, std::uint64_t value)
{
    const std::size_t offset = bytes.size();
    bytes.resize(offset + 8U);
    put_u64(bytes, offset, value);
}

void append_f32(std::vector<std::byte>& bytes, float value)
{
    append_u32(bytes, std::bit_cast<std::uint32_t>(value));
}

std::uint64_t hash_bytes(std::span<const std::byte> bytes,
                         std::uint64_t seed = 0xcbf29ce484222325ULL) noexcept
{
    for (const std::byte value : bytes) {
        seed ^= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(value));
        seed *= 0x100000001b3ULL;
    }
    return seed;
}

std::vector<std::byte> legacy_scene_bytes(std::uint64_t mesh_id, std::uint64_t material_id)
{
    const std::array<std::byte, 4> magic = {
        std::byte{'G'}, std::byte{'S'}, std::byte{'C'}, std::byte{'N'}};
    std::vector<std::byte> header;
    append_u32(header, 1U);
    append_u32(header, 0U);
    std::vector<std::byte> instance;
    for (std::size_t index = 0; index < 16U; ++index) {
        append_f32(instance,
                   (index == 0U || index == 5U || index == 10U || index == 15U) ? 1.0F : 0.0F);
    }
    append_u64(instance, mesh_id);
    append_u64(instance, material_id);
    const std::array<std::vector<std::byte>, 2> payloads = {header, instance};
    const std::array<std::uint32_t, 2> kinds = {
        gameengine::assets::scene_chunk_header, gameengine::assets::scene_chunk_instances};
    const auto align16 = [](std::size_t value) { return (value + 15U) & ~std::size_t{15U}; };
    std::array<std::size_t, 2> offsets{};
    std::size_t cursor = align16(64U + 2U * 40U);
    std::vector<std::byte> payload;
    for (std::size_t index = 0; index < payloads.size(); ++index) {
        cursor = align16(cursor);
        offsets[index] = cursor;
        payload.insert(payload.end(), payloads[index].begin(), payloads[index].end());
        cursor += payloads[index].size();
    }
    std::vector<std::byte> output(cursor);
    std::copy(magic.begin(), magic.end(), output.begin());
    put_u32(output, 4U, 1U | (64U << 16U));
    put_u32(output, 8U, 1U);
    put_u64(output, 12U, output.size());
    put_u64(output, 28U, hash_bytes(payload));
    std::uint64_t asset_id = hash_bytes(std::span<const std::byte>{magic});
    asset_id = hash_bytes(payload, asset_id);
    put_u64(output, 20U, asset_id);
    put_u32(output, 36U, 2U);
    put_u32(output, 40U, 40U);
    for (std::size_t index = 0; index < payloads.size(); ++index) {
        const std::size_t entry = 64U + index * 40U;
        put_u32(output, entry, kinds[index]);
        put_u64(output, entry + 8U, offsets[index]);
        put_u64(output, entry + 16U, payloads[index].size());
        put_u64(output, entry + 24U, payloads[index].size());
        put_u32(output, entry + 32U, 16U);
        std::copy(payloads[index].begin(), payloads[index].end(),
                  output.begin() + static_cast<std::ptrdiff_t>(offsets[index]));
    }
    return output;
}

} // namespace

int main()
{
    using namespace gameengine;
    using scene::Entity;

    const math::Quaternion quarter_turn =
        math::quaternion_from_axis_angle({0.0F, 1.0F, 0.0F}, 1.5707963F);
    const math::Mat4 quaternion_matrix = math::rotation(quarter_turn);
    if (!near(quaternion_matrix.at(0, 0), 0.0F, 0.001F) ||
        !near(quaternion_matrix.at(2, 0), -1.0F, 0.001F)) {
        return 1;
    }

    scene::Scene world;
    const Entity root = world.create_entity();
    const Entity child = world.create_entity();
    if (!root.valid() || !child.valid() || root.index() != 0U || child.index() != 1U ||
        !world.add_transform(root).ok() || !world.add_transform(child).ok()) {
        return 2;
    }
    auto* root_transform = world.transform(root);
    auto* child_transform = world.transform(child);
    root_transform->local_position = {2.0F, 0.0F, 0.0F};
    child_transform->local_position = {0.0F, 3.0F, 0.0F};
    if (!world.set_parent(child, root).ok() || !world.update_transforms().ok() ||
        !near(world.transform(child)->world_matrix.at(0, 3), 2.0F) ||
        !near(world.transform(child)->world_matrix.at(1, 3), 3.0F)) {
        return 3;
    }
    if (world.set_parent(root, child).code != core::ErrorCode::invalid_argument ||
        world.set_parent(root, root).code != core::ErrorCode::invalid_argument) {
        return 4;
    }

    if (!world.add_mesh_renderer(child, {scene::bootstrap_mesh_id, scene::bootstrap_material_id}).ok() ||
        !world.add_camera(root, {.active = true}).ok() ||
        !world.add_directional_light(root).ok() ||
        world.validate().code != core::ErrorCode::none) {
        return 5;
    }

    const Entity stale = root;
    if (!world.destroy_entity(root).ok() || world.is_alive(stale) ||
        world.transform(child)->parent.valid() || world.destroy_entity(stale).code !=
            core::ErrorCode::invalid_argument) {
        return 6;
    }
    const Entity reused = world.create_entity();
    if (reused.index() != root.index() || reused.generation() == root.generation() ||
        world.is_alive(stale)) {
        return 7;
    }

    scene::Scene serializable;
    const Entity serial_root = serializable.create_entity();
    const Entity serial_child = serializable.create_entity();
    if (!serializable.add_transform(serial_root).ok() ||
        !serializable.add_transform(serial_child).ok() ||
        !serializable.set_parent(serial_child, serial_root).ok() ||
        !serializable.add_mesh_renderer(
            serial_child, {scene::bootstrap_mesh_id, scene::bootstrap_material_id}).ok() ||
        !serializable.add_camera(serial_root, {.active = true}).ok() ||
        !serializable.add_directional_light(serial_root).ok()) {
        return 8;
    }
    serializable.transform(serial_root)->local_position = {1.0F, 2.0F, 3.0F};
    serializable.transform(serial_child)->local_scale = {2.0F, 2.0F, 2.0F};

    std::vector<std::byte> first;
    std::vector<std::byte> second;
    if (!scene::serialize_scene(serializable, first).ok() ||
        !scene::serialize_scene(serializable, second).ok() || first != second) {
        return 9;
    }
    assets::SceneView view;
    if (!assets::read_scene(first, view).ok() || !view.extended || view.entity_count != 2U ||
        !assets::validate_scene_references(
             view,
             std::span<const core::u64>{&scene::bootstrap_mesh_id, 1},
             std::span<const core::u64>{&scene::bootstrap_material_id, 1})
             .ok()) {
        return 10;
    }

    scene::Scene restored;
    if (!scene::deserialize_scene(first, restored).ok() || restored.entity_count() != 2U ||
        restored.active_camera().valid() == false || !restored.update_transforms().ok() ||
        !near(restored.transform(Entity::make(1U, 1U))->world_matrix.at(0, 3), 1.0F)) {
        return 11;
    }
    const auto legacy_bytes =
        legacy_scene_bytes(scene::bootstrap_mesh_id, scene::bootstrap_material_id);
    scene::Scene legacy;
    if (!scene::deserialize_scene(legacy_bytes, legacy).ok() || legacy.entity_count() != 1U ||
        legacy.mesh_renderer(Entity::make(0U, 1U)) == nullptr) {
        return 12;
    }
    scene::SparseSet<int> pool;
    const Entity pool_a = Entity::make(7U, 1U);
    const Entity pool_b = Entity::make(8U, 1U);
    if (pool.size() != 0U || pool.emplace(pool_a, 11) == nullptr ||
        pool.emplace(pool_b, 22) == nullptr || !pool.contains(pool_a) ||
        pool.get(pool_b) == nullptr || *pool.get(pool_b) != 22 ||
        pool.entities().size() != 2U || !pool.remove(pool_a) || pool.contains(pool_a) ||
        pool.size() != 1U || pool.remove(pool_a)) {
        return 13;
    }
    return 0;
}
