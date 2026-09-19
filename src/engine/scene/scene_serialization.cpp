#include "engine/scene/scene_serialization.hpp"

#include "engine/assets/asset_reader.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

namespace gameengine::scene {

namespace {

constexpr core::usize header_size = 64;
constexpr core::usize chunk_entry_size = 40;
constexpr core::usize alignment = 16;
constexpr core::u64 fnv_offset = 0xcbf29ce484222325ULL;
constexpr core::u64 fnv_prime = 0x100000001b3ULL;

struct Chunk final {
    core::u32 kind = 0;
    std::vector<std::byte> data;
};

[[nodiscard]] core::Status invalid() noexcept
{
    return core::Status{core::ErrorCode::invalid_argument};
}

[[nodiscard]] core::u64 hash_bytes(std::span<const std::byte> bytes,
                                   core::u64 seed = fnv_offset) noexcept
{
    core::u64 hash = seed;
    for (const std::byte value : bytes) {
        hash ^= static_cast<core::u64>(std::to_integer<core::u8>(value));
        hash *= fnv_prime;
    }
    return hash;
}

void append_u32(std::vector<std::byte>& data, core::u32 value)
{
    for (core::u32 index = 0; index < 4U; ++index) {
        data.push_back(std::byte{static_cast<unsigned char>((value >> (index * 8U)) & 0xffU)});
    }
}

void append_u64(std::vector<std::byte>& data, core::u64 value)
{
    for (core::u32 index = 0; index < 8U; ++index) {
        data.push_back(std::byte{static_cast<unsigned char>((value >> (index * 8U)) & 0xffU)});
    }
}

void append_f32(std::vector<std::byte>& data, core::f32 value)
{
    append_u32(data, std::bit_cast<core::u32>(value));
}

void write_u32(std::vector<std::byte>& data, core::usize offset, core::u32 value) noexcept
{
    for (core::u32 index = 0; index < 4U; ++index) {
        data[offset + index] =
            std::byte{static_cast<unsigned char>((value >> (index * 8U)) & 0xffU)};
    }
}

void write_u64(std::vector<std::byte>& data, core::usize offset, core::u64 value) noexcept
{
    for (core::u32 index = 0; index < 8U; ++index) {
        data[offset + index] =
            std::byte{static_cast<unsigned char>((value >> (index * 8U)) & 0xffU)};
    }
}

[[nodiscard]] core::usize align_up(core::usize value) noexcept
{
    return (value + alignment - 1U) & ~(alignment - 1U);
}

[[nodiscard]] std::vector<std::byte> build_asset(const std::vector<Chunk>& chunks)
{
    core::usize cursor = align_up(header_size + chunks.size() * chunk_entry_size);
    std::vector<core::usize> offsets;
    offsets.reserve(chunks.size());
    std::vector<std::byte> payload;
    for (const Chunk& chunk : chunks) {
        cursor = align_up(cursor);
        offsets.push_back(cursor);
        payload.insert(payload.end(), chunk.data.begin(), chunk.data.end());
        cursor += chunk.data.size();
    }
    std::vector<std::byte> output(cursor);
    output[0] = std::byte{'G'};
    output[1] = std::byte{'S'};
    output[2] = std::byte{'C'};
    output[3] = std::byte{'N'};
    output[4] = std::byte{1};
    output[5] = std::byte{0};
    output[6] = std::byte{64};
    output[7] = std::byte{0};
    write_u32(output, 8, 1U);
    write_u64(output, 12, output.size());
    write_u64(output, 28, hash_bytes(payload));
    core::u64 asset_id = hash_bytes(std::span<const std::byte>{output.data(), 4});
    asset_id = hash_bytes(payload, asset_id);
    write_u64(output, 20, asset_id);
    write_u32(output, 36, static_cast<core::u32>(chunks.size()));
    write_u32(output, 40, static_cast<core::u32>(chunk_entry_size));
    for (core::usize index = 0; index < chunks.size(); ++index) {
        const core::usize entry = header_size + index * chunk_entry_size;
        write_u32(output, entry, chunks[index].kind);
        write_u64(output, entry + 8, offsets[index]);
        write_u64(output, entry + 16, chunks[index].data.size());
        write_u64(output, entry + 24, chunks[index].data.size());
        write_u32(output, entry + 32, static_cast<core::u32>(alignment));
        std::copy(chunks[index].data.begin(), chunks[index].data.end(),
                  output.begin() + static_cast<std::ptrdiff_t>(offsets[index]));
    }
    return output;
}

[[nodiscard]] core::f32 read_f32(std::span<const std::byte> data, core::usize offset) noexcept
{
    return std::bit_cast<core::f32>(assets::read_u32(data, offset));
}

[[nodiscard]] bool near(core::f32 left, core::f32 right, core::f32 epsilon = 0.0001F) noexcept
{
    return std::fabs(left - right) <= epsilon;
}

[[nodiscard]] bool decompose_matrix(std::span<const std::byte> bytes,
                                    math::Vec3& position,
                                    math::Quaternion& rotation,
                                    math::Vec3& scale) noexcept
{
    std::array<core::f32, 16> matrix{};
    for (core::u32 index = 0; index < matrix.size(); ++index) {
        matrix[index] = read_f32(bytes, index * 4U);
        if (!std::isfinite(matrix[index])) {
            return false;
        }
    }
    if (!near(matrix[3], 0.0F) || !near(matrix[7], 0.0F) || !near(matrix[11], 0.0F) ||
        !near(matrix[15], 1.0F)) {
        return false;
    }
    const math::Vec3 x_axis{matrix[0], matrix[1], matrix[2]};
    const math::Vec3 y_axis{matrix[4], matrix[5], matrix[6]};
    const math::Vec3 z_axis{matrix[8], matrix[9], matrix[10]};
    scale = {std::sqrt(math::dot(x_axis, x_axis)),
             std::sqrt(math::dot(y_axis, y_axis)),
             std::sqrt(math::dot(z_axis, z_axis))};
    if (scale.x <= 0.0F || scale.y <= 0.0F || scale.z <= 0.0F) {
        return false;
    }
    const math::Vec3 x = math::multiply(x_axis, 1.0F / scale.x);
    const math::Vec3 y = math::multiply(y_axis, 1.0F / scale.y);
    const math::Vec3 z = math::multiply(z_axis, 1.0F / scale.z);
    if (std::fabs(math::dot(x, y)) > 0.001F || std::fabs(math::dot(x, z)) > 0.001F ||
        std::fabs(math::dot(y, z)) > 0.001F) {
        return false;
    }
    position = {matrix[12], matrix[13], matrix[14]};
    const core::f32 trace = x.x + y.y + z.z;
    if (trace > 0.0F) {
        const core::f32 s = std::sqrt(trace + 1.0F) * 2.0F;
        rotation = {(y.z - z.y) / s, (z.x - x.z) / s, (x.y - y.x) / s, 0.25F * s};
    } else if (x.x > y.y && x.x > z.z) {
        const core::f32 s = std::sqrt(1.0F + x.x - y.y - z.z) * 2.0F;
        rotation = {0.25F * s, (x.y + y.x) / s, (x.z + z.x) / s, (y.z - z.y) / s};
    } else if (y.y > z.z) {
        const core::f32 s = std::sqrt(1.0F + y.y - x.x - z.z) * 2.0F;
        rotation = {(x.y + y.x) / s, 0.25F * s, (y.z + z.y) / s, (z.x - x.z) / s};
    } else {
        const core::f32 s = std::sqrt(1.0F + z.z - x.x - y.y) * 2.0F;
        rotation = {(x.z + z.x) / s, (y.z + z.y) / s, 0.25F * s, (x.y - y.x) / s};
    }
    rotation = math::normalize(rotation);
    return true;
}

[[nodiscard]] const assets::ChunkView* chunk(const assets::SceneView& view,
                                              core::u32 kind) noexcept
{
    return assets::find_chunk(view.asset, kind);
}

} // namespace

core::Status serialize_scene(const Scene& scene, std::vector<std::byte>& output) noexcept
{
    if (scene.validate().code != core::ErrorCode::none || scene.entity_count() == 0) {
        return invalid();
    }
    std::vector<Entity> entities(scene.entities().begin(), scene.entities().end());
    std::sort(entities.begin(), entities.end(), [](Entity left, Entity right) {
        return left.index() < right.index();
    });
    std::vector<Chunk> chunks;
    chunks.reserve(6);
    Chunk header{assets::scene_chunk_header};
    append_u32(header.data, static_cast<core::u32>(entities.size()));
    append_u32(header.data, 0U);
    append_u64(header.data, scene.active_camera().value);
    Chunk entity_data{assets::scene_chunk_entities};
    Chunk transform_data{assets::scene_chunk_transforms};
    Chunk mesh_data{assets::scene_chunk_mesh_renderers};
    Chunk camera_data{assets::scene_chunk_cameras};
    Chunk light_data{assets::scene_chunk_lights};
    for (const Entity entity : entities) {
        const TransformComponent* transform_component = scene.transform(entity);
        if (transform_component == nullptr) {
            return invalid();
        }
        append_u64(entity_data.data, entity.value);
        append_u64(entity_data.data, transform_component->parent.value);
        append_u64(transform_data.data, entity.value);
        append_f32(transform_data.data, transform_component->local_position.x);
        append_f32(transform_data.data, transform_component->local_position.y);
        append_f32(transform_data.data, transform_component->local_position.z);
        append_f32(transform_data.data, transform_component->local_rotation.x);
        append_f32(transform_data.data, transform_component->local_rotation.y);
        append_f32(transform_data.data, transform_component->local_rotation.z);
        append_f32(transform_data.data, transform_component->local_rotation.w);
        append_f32(transform_data.data, transform_component->local_scale.x);
        append_f32(transform_data.data, transform_component->local_scale.y);
        append_f32(transform_data.data, transform_component->local_scale.z);
        if (const MeshRendererComponent* component = scene.mesh_renderer(entity); component != nullptr) {
            append_u64(mesh_data.data, entity.value);
            append_u64(mesh_data.data, component->mesh_id);
            append_u64(mesh_data.data, component->material_id);
        }
        if (const CameraComponent* component = scene.camera(entity); component != nullptr) {
            append_u64(camera_data.data, entity.value);
            append_f32(camera_data.data, component->vertical_field_of_view_radians);
            append_f32(camera_data.data, component->near_plane);
            append_f32(camera_data.data, component->far_plane);
            append_u32(camera_data.data, component->active ? 1U : 0U);
        }
        if (const DirectionalLightComponent* component = scene.directional_light(entity); component != nullptr) {
            append_u64(light_data.data, entity.value);
            append_f32(light_data.data, component->direction.x);
            append_f32(light_data.data, component->direction.y);
            append_f32(light_data.data, component->direction.z);
            append_f32(light_data.data, component->color.x);
            append_f32(light_data.data, component->color.y);
            append_f32(light_data.data, component->color.z);
            append_f32(light_data.data, component->intensity);
            append_u32(light_data.data, 0U);
        }
    }
    chunks.push_back(std::move(header));
    chunks.push_back(std::move(entity_data));
    chunks.push_back(std::move(transform_data));
    chunks.push_back(std::move(mesh_data));
    if (!camera_data.data.empty()) {
        chunks.push_back(std::move(camera_data));
    }
    if (!light_data.data.empty()) {
        chunks.push_back(std::move(light_data));
    }
    output = build_asset(chunks);
    return core::Status{};
}

core::Status deserialize_scene(std::span<const std::byte> bytes, Scene& output) noexcept
{
    assets::SceneView view;
    if (!assets::read_scene(bytes, view)) {
        return invalid();
    }
    Scene decoded;
    if (view.extended) {
        decoded.reserve(view.entity_count);
        for (core::u32 index = 0; index < view.entity_count; ++index) {
            const core::usize offset = static_cast<core::usize>(index) * 16;
            if (!decoded.adopt_entity(Entity{assets::read_u64(view.entities, offset)})) {
                return invalid();
            }
            const core::usize transform_offset = static_cast<core::usize>(index) * 48;
            TransformComponent component{};
            component.local_position = {read_f32(view.transforms, transform_offset + 8),
                                        read_f32(view.transforms, transform_offset + 12),
                                        read_f32(view.transforms, transform_offset + 16)};
            component.local_rotation = math::normalize(math::Quaternion{
                read_f32(view.transforms, transform_offset + 20),
                read_f32(view.transforms, transform_offset + 24),
                read_f32(view.transforms, transform_offset + 28),
                read_f32(view.transforms, transform_offset + 32)});
            component.local_scale = {read_f32(view.transforms, transform_offset + 36),
                                     read_f32(view.transforms, transform_offset + 40),
                                     read_f32(view.transforms, transform_offset + 44)};
            if (!decoded.add_transform(Entity{assets::read_u64(view.transforms, transform_offset)}, component)) {
                return invalid();
            }
        }
        for (core::u32 index = 0; index < view.entity_count; ++index) {
            const core::usize offset = static_cast<core::usize>(index) * 16;
            const Entity entity{assets::read_u64(view.entities, offset)};
            const Entity parent{assets::read_u64(view.entities, offset + 8)};
            if (!decoded.set_parent(entity, parent)) {
                return invalid();
            }
        }
        for (core::usize offset = 0; offset < view.mesh_renderers.size(); offset += 24) {
            if (!decoded.add_mesh_renderer(
                    Entity{assets::read_u64(view.mesh_renderers, offset)},
                    {assets::read_u64(view.mesh_renderers, offset + 8),
                     assets::read_u64(view.mesh_renderers, offset + 16)})) {
                return invalid();
            }
        }
        for (core::usize offset = 0; offset < view.cameras.size(); offset += 24) {
            if (!decoded.add_camera(
                    Entity{assets::read_u64(view.cameras, offset)},
                    {read_f32(view.cameras, offset + 8),
                     read_f32(view.cameras, offset + 12),
                     read_f32(view.cameras, offset + 16),
                     assets::read_u32(view.cameras, offset + 20) != 0U})) {
                return invalid();
            }
        }
        for (core::usize offset = 0; offset < view.lights.size(); offset += 40) {
            if (assets::read_u32(view.lights, offset + 36) != 0U ||
                !decoded.add_directional_light(
                    Entity{assets::read_u64(view.lights, offset)},
                    {{read_f32(view.lights, offset + 8),
                      read_f32(view.lights, offset + 12),
                      read_f32(view.lights, offset + 16)},
                     {read_f32(view.lights, offset + 20),
                      read_f32(view.lights, offset + 24),
                      read_f32(view.lights, offset + 28)},
                     read_f32(view.lights, offset + 32)})) {
                return invalid();
            }
        }
        if (decoded.active_camera().value != view.active_camera_value ||
            !decoded.update_transforms() || !decoded.validate()) {
            return invalid();
        }
    } else {
        decoded.reserve(view.instance_count);
        for (core::u32 index = 0; index < view.instance_count; ++index) {
            const Entity entity = Entity::make(index, 1U);
            if (!decoded.adopt_entity(entity)) {
                return invalid();
            }
            const core::usize offset = static_cast<core::usize>(index) * 80;
            math::Vec3 position;
            math::Quaternion rotation;
            math::Vec3 scale;
            if (!decompose_matrix(view.instances.subspan(offset, 64), position, rotation, scale) ||
                !decoded.add_transform(entity, {position, rotation, scale}) ||
                !decoded.add_mesh_renderer(entity,
                                            {assets::read_u64(view.instances, offset + 64),
                                             assets::read_u64(view.instances, offset + 72)})) {
                return invalid();
            }
        }
        if (!decoded.update_transforms() || !decoded.validate()) {
            return invalid();
        }
    }
    output.swap(decoded);
    return core::Status{};
}

} // namespace gameengine::scene
