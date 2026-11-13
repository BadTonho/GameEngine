#include "api/c/gameengine_c.h"

#include "engine/assets/prepared_assets.hpp"
#include "engine/core/status.hpp"
#include "engine/platform/platform.hpp"
#include "engine/renderer/scene_bridge.hpp"
#include "engine/rhi/rhi.hpp"
#include "engine/scene/scene.hpp"
#include "engine/scene/scene_serialization.hpp"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <new>
#include <vector>

struct GeScene {
    gameengine::scene::Scene scene{};
};

struct GeEngine {
    bool headless_platform_only = false;
    bool initialized = false;
    gameengine::platform::Platform platform{};
    gameengine::rhi::Renderer renderer{};
    gameengine::assets::PreparedAssetBundle assets{};
    GeScene* attached_scene = nullptr;
};

namespace {

[[nodiscard]] GeResult map_status(gameengine::core::Status status) noexcept
{
    using gameengine::core::ErrorCode;
    switch (status.code) {
    case ErrorCode::none:
        return GE_OK;
    case ErrorCode::invalid_argument:
        return GE_ERROR_INVALID_ARGUMENT;
    case ErrorCode::allocation_failed:
        return GE_ERROR_ALLOCATION_FAILED;
    case ErrorCode::not_initialized:
        return GE_ERROR_NOT_INITIALIZED;
    case ErrorCode::already_initialized:
        return GE_ERROR_ALREADY_INITIALIZED;
    case ErrorCode::unsupported_platform:
    case ErrorCode::display_unavailable:
    case ErrorCode::window_creation_failed:
        return GE_ERROR_UNSUPPORTED_PLATFORM;
    case ErrorCode::vulkan_instance_failed:
    case ErrorCode::vulkan_validation_unavailable:
    case ErrorCode::vulkan_surface_failed:
    case ErrorCode::vulkan_device_failed:
    case ErrorCode::vulkan_swapchain_failed:
    case ErrorCode::vulkan_frame_failed:
    case ErrorCode::vulkan_surface_lost:
    case ErrorCode::vulkan_validation_failed:
    case ErrorCode::shader_variant_unavailable:
    case ErrorCode::shader_reload_disabled:
        return GE_ERROR_RENDERER_FAILED;
    default:
        return GE_ERROR_INTERNAL;
    }
}

[[nodiscard]] gameengine::scene::Entity to_cpp_entity(GeEntity entity) noexcept
{
    return gameengine::scene::Entity{entity};
}

[[nodiscard]] std::FILE* open_binary_file(const char* path, const char* mode) noexcept
{
#if defined(_MSC_VER)
    std::FILE* file = nullptr;
    if (fopen_s(&file, path, mode) != 0) {
        return nullptr;
    }
    return file;
#else
    return std::fopen(path, mode);
#endif
}

} // namespace

GeResult ge_get_api_info(GeApiInfo* out_info)
{
    if (out_info == nullptr || out_info->struct_size < sizeof(GeApiInfo)) {
        return GE_ERROR_INVALID_ARGUMENT;
    }
    out_info->major = GE_API_VERSION_MAJOR;
    out_info->minor = GE_API_VERSION_MINOR;
    out_info->patch = GE_API_VERSION_PATCH;
    return GE_OK;
}

const char* ge_result_string(GeResult result)
{
    switch (result) {
    case GE_OK:
        return "GE_OK";
    case GE_ERROR_INVALID_ARGUMENT:
        return "GE_ERROR_INVALID_ARGUMENT";
    case GE_ERROR_ALLOCATION_FAILED:
        return "GE_ERROR_ALLOCATION_FAILED";
    case GE_ERROR_NOT_INITIALIZED:
        return "GE_ERROR_NOT_INITIALIZED";
    case GE_ERROR_ALREADY_INITIALIZED:
        return "GE_ERROR_ALREADY_INITIALIZED";
    case GE_ERROR_UNSUPPORTED_PLATFORM:
        return "GE_ERROR_UNSUPPORTED_PLATFORM";
    case GE_ERROR_INCOMPATIBLE_ABI:
        return "GE_ERROR_INCOMPATIBLE_ABI";
    case GE_ERROR_IO_OR_ASSET_FAILED:
        return "GE_ERROR_IO_OR_ASSET_FAILED";
    case GE_ERROR_RENDERER_FAILED:
        return "GE_ERROR_RENDERER_FAILED";
    case GE_ERROR_INTERNAL:
    default:
        return "GE_ERROR_INTERNAL";
    }
}

GeResult ge_engine_create(const GeEngineCreateInfo* create_info, GeEngine** out_engine)
{
    if (create_info == nullptr || out_engine == nullptr) {
        return GE_ERROR_INVALID_ARGUMENT;
    }
    *out_engine = nullptr;
    if (create_info->struct_size < sizeof(GeEngineCreateInfo)) {
        return GE_ERROR_INVALID_ARGUMENT;
    }
    const uint32_t requested_major = (create_info->api_version >> 22U);
    if (requested_major != GE_API_VERSION_MAJOR) {
        return GE_ERROR_INCOMPATIBLE_ABI;
    }

    GeEngine* engine = new (std::nothrow) GeEngine();
    if (engine == nullptr) {
        return GE_ERROR_ALLOCATION_FAILED;
    }
    engine->headless_platform_only = create_info->headless_platform_only;

    if (!engine->headless_platform_only) {
        const gameengine::platform::WindowDescription window_config{
            .title = create_info->title != nullptr ? create_info->title : "GameEngine",
            .width = create_info->width > 0U ? create_info->width : 1280U,
            .height = create_info->height > 0U ? create_info->height : 720U,
        };
        const gameengine::core::Status platform_status = engine->platform.initialize();
        if (!platform_status.ok()) {
            delete engine;
            return map_status(platform_status);
        }
        const gameengine::core::Status window_status = engine->platform.create_window(window_config);
        if (!window_status.ok()) {
            engine->platform.shutdown();
            delete engine;
            return map_status(window_status);
        }
        const gameengine::core::Status renderer_status = engine->renderer.initialize(engine->platform);
        if (!renderer_status.ok()) {
            engine->platform.shutdown();
            delete engine;
            return map_status(renderer_status);
        }
    }

    engine->initialized = true;
    *out_engine = engine;
    return GE_OK;
}

void ge_engine_destroy(GeEngine* engine)
{
    if (engine == nullptr) {
        return;
    }
    if (engine->initialized && !engine->headless_platform_only) {
        if (engine->attached_scene != nullptr) {
            static_cast<void>(gameengine::renderer::scene_bridge::detach_scene(engine->renderer));
            engine->attached_scene = nullptr;
        }
        engine->renderer.shutdown();
        engine->platform.shutdown();
    }
    engine->assets.clear();
    engine->initialized = false;
    delete engine;
}

GeResult ge_engine_attach_scene(GeEngine* engine, GeScene* scene)
{
    if (engine == nullptr || scene == nullptr) {
        return GE_ERROR_INVALID_ARGUMENT;
    }
    if (!engine->initialized) {
        return GE_ERROR_NOT_INITIALIZED;
    }
    const gameengine::core::Status transform_status = scene->scene.update_transforms();
    if (!transform_status.ok()) {
        return map_status(transform_status);
    }
    if (!engine->headless_platform_only) {
        const gameengine::core::Status attach_status =
            gameengine::renderer::scene_bridge::attach_scene(engine->renderer, scene->scene);
        if (!attach_status.ok()) {
            return map_status(attach_status);
        }
    }
    engine->attached_scene = scene;
    return GE_OK;
}

GeResult ge_engine_detach_scene(GeEngine* engine)
{
    if (engine == nullptr) {
        return GE_ERROR_INVALID_ARGUMENT;
    }
    if (!engine->initialized) {
        return GE_ERROR_NOT_INITIALIZED;
    }
    if (!engine->headless_platform_only && engine->attached_scene != nullptr) {
        const gameengine::core::Status detach_status =
            gameengine::renderer::scene_bridge::detach_scene(engine->renderer);
        if (!detach_status.ok()) {
            return map_status(detach_status);
        }
    }
    engine->attached_scene = nullptr;
    return GE_OK;
}

GeResult ge_engine_load_prepared_assets(GeEngine* engine, const char* directory_path)
{
    if (engine == nullptr || directory_path == nullptr || directory_path[0] == '\0') {
        return GE_ERROR_INVALID_ARGUMENT;
    }
    if (!engine->initialized) {
        return GE_ERROR_NOT_INITIALIZED;
    }

    gameengine::assets::PreparedAssetBundle bundle;
    const gameengine::core::Status load_status = bundle.load_directory(directory_path);
    if (!load_status.ok()) {
        return load_status.code == gameengine::core::ErrorCode::allocation_failed
                   ? GE_ERROR_ALLOCATION_FAILED
                   : GE_ERROR_IO_OR_ASSET_FAILED;
    }

    if (!engine->headless_platform_only) {
        const gameengine::core::Status upload_status =
            gameengine::renderer::scene_bridge::set_prepared_assets(
                engine->renderer,
                gameengine::renderer::scene_bridge::make_prepared_render_assets(bundle));
        if (!upload_status.ok()) {
            return map_status(upload_status);
        }
    }

    engine->assets = static_cast<gameengine::assets::PreparedAssetBundle&&>(bundle);
    return GE_OK;
}

GeResult ge_engine_step_frame(GeEngine* engine, bool* out_should_continue)
{
    if (engine == nullptr) {
        return GE_ERROR_INVALID_ARGUMENT;
    }
    if (!engine->initialized) {
        return GE_ERROR_NOT_INITIALIZED;
    }
    if (engine->attached_scene != nullptr) {
        const gameengine::core::Status transform_status =
            engine->attached_scene->scene.update_transforms();
        if (!transform_status.ok()) {
            return map_status(transform_status);
        }
    }

    if (engine->headless_platform_only) {
        if (out_should_continue != nullptr) {
            *out_should_continue = true;
        }
        return GE_OK;
    }

    engine->platform.poll_events();
    const bool should_continue = !engine->platform.should_close();
    if (out_should_continue != nullptr) {
        *out_should_continue = should_continue;
    }
    if (!should_continue) {
        return GE_OK;
    }

    const gameengine::core::Status render_status = engine->renderer.render_frame(engine->platform);
    return map_status(render_status);
}

GeResult ge_scene_create(GeScene** out_scene)
{
    if (out_scene == nullptr) {
        return GE_ERROR_INVALID_ARGUMENT;
    }
    *out_scene = nullptr;
    GeScene* scene = new (std::nothrow) GeScene();
    if (scene == nullptr) {
        return GE_ERROR_ALLOCATION_FAILED;
    }
    *out_scene = scene;
    return GE_OK;
}

void ge_scene_destroy(GeScene* scene)
{
    delete scene;
}

GeResult ge_scene_load_file(GeScene* scene, const char* path)
{
    if (scene == nullptr || path == nullptr || path[0] == '\0') {
        return GE_ERROR_INVALID_ARGUMENT;
    }
    std::FILE* file = open_binary_file(path, "rb");
    if (file == nullptr) {
        return GE_ERROR_IO_OR_ASSET_FAILED;
    }
    if (std::fseek(file, 0, SEEK_END) != 0) {
        static_cast<void>(std::fclose(file));
        return GE_ERROR_IO_OR_ASSET_FAILED;
    }
    const long size = std::ftell(file);
    if (size < 0 || std::fseek(file, 0, SEEK_SET) != 0) {
        static_cast<void>(std::fclose(file));
        return GE_ERROR_IO_OR_ASSET_FAILED;
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    if (size > 0 && std::fread(bytes.data(), 1U, bytes.size(), file) != bytes.size()) {
        static_cast<void>(std::fclose(file));
        return GE_ERROR_IO_OR_ASSET_FAILED;
    }
    static_cast<void>(std::fclose(file));

    const gameengine::core::Status status =
        gameengine::scene::deserialize_scene(bytes, scene->scene);
    if (!status.ok()) {
        return status.code == gameengine::core::ErrorCode::allocation_failed
                   ? GE_ERROR_ALLOCATION_FAILED
                   : GE_ERROR_IO_OR_ASSET_FAILED;
    }
    return GE_OK;
}

GeResult ge_scene_save_file(const GeScene* scene, const char* path)
{
    if (scene == nullptr || path == nullptr || path[0] == '\0') {
        return GE_ERROR_INVALID_ARGUMENT;
    }
    std::vector<std::byte> bytes;
    const gameengine::core::Status status =
        gameengine::scene::serialize_scene(scene->scene, bytes);
    if (!status.ok()) {
        return map_status(status);
    }

    std::FILE* file = open_binary_file(path, "wb");
    if (file == nullptr) {
        return GE_ERROR_IO_OR_ASSET_FAILED;
    }
    const bool wrote =
        bytes.empty() || std::fwrite(bytes.data(), 1U, bytes.size(), file) == bytes.size();
    const bool closed = std::fclose(file) == 0;
    return (wrote && closed) ? GE_OK : GE_ERROR_IO_OR_ASSET_FAILED;
}

size_t ge_scene_entity_count(const GeScene* scene)
{
    if (scene == nullptr) {
        return 0U;
    }
    return scene->scene.entity_count();
}

GeEntity ge_scene_entity_create(GeScene* scene)
{
    if (scene == nullptr) {
        return GE_INVALID_ENTITY;
    }
    return scene->scene.create_entity().value;
}

GeResult ge_scene_entity_destroy(GeScene* scene, GeEntity entity)
{
    if (scene == nullptr) {
        return GE_ERROR_INVALID_ARGUMENT;
    }
    return map_status(scene->scene.destroy_entity(to_cpp_entity(entity)));
}

bool ge_scene_entity_is_alive(const GeScene* scene, GeEntity entity)
{
    if (scene == nullptr) {
        return false;
    }
    return scene->scene.is_alive(to_cpp_entity(entity));
}

GeResult ge_scene_transform_set_position(GeScene* scene, GeEntity entity, GeVec3 position)
{
    if (scene == nullptr) {
        return GE_ERROR_INVALID_ARGUMENT;
    }
    const gameengine::scene::Entity cpp_entity = to_cpp_entity(entity);
    if (!scene->scene.is_alive(cpp_entity)) {
        return GE_ERROR_INVALID_ARGUMENT;
    }
    gameengine::scene::TransformComponent* existing = scene->scene.transform(cpp_entity);
    if (existing != nullptr) {
        existing->local_position = {position.x, position.y, position.z};
        return GE_OK;
    }
    gameengine::scene::TransformComponent component{};
    component.local_position = {position.x, position.y, position.z};
    return map_status(scene->scene.add_transform(cpp_entity, component));
}

GeResult ge_scene_transform_get_position(const GeScene* scene,
                                         GeEntity entity,
                                         GeVec3* out_position)
{
    if (scene == nullptr || out_position == nullptr) {
        return GE_ERROR_INVALID_ARGUMENT;
    }
    const gameengine::scene::TransformComponent* existing =
        scene->scene.transform(to_cpp_entity(entity));
    if (existing == nullptr) {
        return GE_ERROR_INVALID_ARGUMENT;
    }
    out_position->x = existing->local_position.x;
    out_position->y = existing->local_position.y;
    out_position->z = existing->local_position.z;
    return GE_OK;
}

GeResult ge_scene_mesh_renderer_set(GeScene* scene,
                                    GeEntity entity,
                                    uint64_t mesh_id,
                                    uint64_t material_id)
{
    if (scene == nullptr) {
        return GE_ERROR_INVALID_ARGUMENT;
    }
    const gameengine::scene::Entity cpp_entity = to_cpp_entity(entity);
    if (!scene->scene.is_alive(cpp_entity)) {
        return GE_ERROR_INVALID_ARGUMENT;
    }
    gameengine::scene::MeshRendererComponent* existing = scene->scene.mesh_renderer(cpp_entity);
    if (existing != nullptr) {
        existing->mesh_id = mesh_id;
        existing->material_id = material_id;
        return GE_OK;
    }
    return map_status(scene->scene.add_mesh_renderer(cpp_entity,
                                                     {.mesh_id = mesh_id, .material_id = material_id}));
}

GeResult ge_scene_update_transforms(GeScene* scene)
{
    if (scene == nullptr) {
        return GE_ERROR_INVALID_ARGUMENT;
    }
    return map_status(scene->scene.update_transforms());
}
