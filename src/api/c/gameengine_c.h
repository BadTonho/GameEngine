#ifndef GAMEENGINE_C_H
#define GAMEENGINE_C_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GE_API_VERSION_MAJOR 1U
#define GE_API_VERSION_MINOR 0U
#define GE_API_VERSION_PATCH 0U

#define GE_API_MAKE_VERSION(major, minor, patch) \
    ((((uint32_t)(major)) << 22U) | (((uint32_t)(minor)) << 12U) | ((uint32_t)(patch)))

#define GE_API_VERSION \
    GE_API_MAKE_VERSION(GE_API_VERSION_MAJOR, GE_API_VERSION_MINOR, GE_API_VERSION_PATCH)

#if defined(_WIN32) && defined(GAMEENGINE_C_API_SHARED)
#if defined(GAMEENGINE_C_API_EXPORTS)
#define GE_API __declspec(dllexport)
#else
#define GE_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) && defined(GAMEENGINE_C_API_SHARED)
#define GE_API __attribute__((visibility("default")))
#else
#define GE_API
#endif

typedef enum GeResult {
    GE_OK = 0,
    GE_ERROR_INVALID_ARGUMENT = 1,
    GE_ERROR_ALLOCATION_FAILED = 2,
    GE_ERROR_NOT_INITIALIZED = 3,
    GE_ERROR_ALREADY_INITIALIZED = 4,
    GE_ERROR_UNSUPPORTED_PLATFORM = 5,
    GE_ERROR_INCOMPATIBLE_ABI = 6,
    GE_ERROR_IO_OR_ASSET_FAILED = 7,
    GE_ERROR_RENDERER_FAILED = 8,
    GE_ERROR_INTERNAL = 9
} GeResult;

typedef struct GeApiInfo {
    uint32_t struct_size;
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
} GeApiInfo;

typedef struct GeEngine GeEngine;
typedef struct GeScene GeScene;

typedef uint64_t GeEntity;
#define GE_INVALID_ENTITY ((GeEntity)0ULL)

typedef struct GeVec3 {
    float x;
    float y;
    float z;
} GeVec3;

typedef struct GeEngineCreateInfo {
    uint32_t struct_size;
    uint32_t api_version;
    uint32_t width;
    uint32_t height;
    const char* title;
    bool headless_platform_only;
} GeEngineCreateInfo;

GE_API GeResult ge_get_api_info(GeApiInfo* out_info);
GE_API const char* ge_result_string(GeResult result);

GE_API GeResult ge_engine_create(const GeEngineCreateInfo* create_info, GeEngine** out_engine);
GE_API void ge_engine_destroy(GeEngine* engine);
GE_API GeResult ge_engine_attach_scene(GeEngine* engine, GeScene* scene);
GE_API GeResult ge_engine_detach_scene(GeEngine* engine);
GE_API GeResult ge_engine_load_prepared_assets(GeEngine* engine, const char* directory_path);
GE_API GeResult ge_engine_step_frame(GeEngine* engine, bool* out_should_continue);

GE_API GeResult ge_scene_create(GeScene** out_scene);
GE_API void ge_scene_destroy(GeScene* scene);
GE_API GeResult ge_scene_load_file(GeScene* scene, const char* path);
GE_API GeResult ge_scene_save_file(const GeScene* scene, const char* path);
GE_API size_t ge_scene_entity_count(const GeScene* scene);
GE_API GeEntity ge_scene_entity_create(GeScene* scene);
GE_API GeResult ge_scene_entity_destroy(GeScene* scene, GeEntity entity);
GE_API bool ge_scene_entity_is_alive(const GeScene* scene, GeEntity entity);

GE_API GeResult ge_scene_transform_set_position(GeScene* scene, GeEntity entity, GeVec3 position);
GE_API GeResult ge_scene_transform_get_position(const GeScene* scene,
                                                GeEntity entity,
                                                GeVec3* out_position);
GE_API GeResult ge_scene_mesh_renderer_set(GeScene* scene,
                                           GeEntity entity,
                                           uint64_t mesh_id,
                                           uint64_t material_id);
GE_API GeResult ge_scene_update_transforms(GeScene* scene);

#ifdef __cplusplus
}
#endif

#endif /* GAMEENGINE_C_H */
