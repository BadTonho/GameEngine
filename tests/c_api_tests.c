#include "api/c/gameengine_c.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int check_float_eq(float a, float b)
{
    return fabsf(a - b) < 1e-5f;
}

int main(void)
{
    GeApiInfo info;
    memset(&info, 0, sizeof(info));
    if (ge_get_api_info(NULL) != GE_ERROR_INVALID_ARGUMENT) {
        return 1;
    }
    if (ge_get_api_info(&info) != GE_ERROR_INVALID_ARGUMENT) {
        return 2;
    }
    info.struct_size = (uint32_t)sizeof(GeApiInfo);
    if (ge_get_api_info(&info) != GE_OK || info.major != GE_API_VERSION_MAJOR ||
        info.minor != GE_API_VERSION_MINOR || info.patch != GE_API_VERSION_PATCH) {
        return 3;
    }
    if (strcmp(ge_result_string(GE_OK), "GE_OK") != 0 ||
        strcmp(ge_result_string(GE_ERROR_INCOMPATIBLE_ABI), "GE_ERROR_INCOMPATIBLE_ABI") != 0) {
        return 4;
    }

    GeEngine* engine = NULL;
    GeEngineCreateInfo bad_size = {0};
    bad_size.struct_size = 4U;
    bad_size.api_version = GE_API_VERSION;
    if (ge_engine_create(&bad_size, &engine) != GE_ERROR_INVALID_ARGUMENT || engine != NULL) {
        return 5;
    }

    GeEngineCreateInfo bad_abi = {0};
    bad_abi.struct_size = (uint32_t)sizeof(GeEngineCreateInfo);
    bad_abi.api_version = GE_API_MAKE_VERSION(GE_API_VERSION_MAJOR + 1U, 0U, 0U);
    if (ge_engine_create(&bad_abi, &engine) != GE_ERROR_INCOMPATIBLE_ABI || engine != NULL) {
        return 6;
    }

    GeEngineCreateInfo create_info = {0};
    create_info.struct_size = (uint32_t)sizeof(GeEngineCreateInfo);
    create_info.api_version = GE_API_VERSION;
    create_info.width = 640U;
    create_info.height = 480U;
    create_info.title = "C11 Test";
    create_info.headless_platform_only = true;
    if (ge_engine_create(&create_info, &engine) != GE_OK || engine == NULL) {
        return 7;
    }

    GeScene* scene = NULL;
    if (ge_scene_create(&scene) != GE_OK || scene == NULL) {
        ge_engine_destroy(engine);
        return 8;
    }

    GeEntity entity = ge_scene_entity_create(scene);
    if (entity == GE_INVALID_ENTITY || !ge_scene_entity_is_alive(scene, entity) ||
        ge_scene_entity_count(scene) != 1U) {
        ge_scene_destroy(scene);
        ge_engine_destroy(engine);
        return 9;
    }

    GeVec3 pos = {1.5f, -2.0f, 3.25f};
    GeVec3 read_pos = {0.0f, 0.0f, 0.0f};
    if (ge_scene_transform_set_position(scene, entity, pos) != GE_OK ||
        ge_scene_transform_get_position(scene, entity, &read_pos) != GE_OK ||
        !check_float_eq(read_pos.x, pos.x) || !check_float_eq(read_pos.y, pos.y) ||
        !check_float_eq(read_pos.z, pos.z)) {
        ge_scene_destroy(scene);
        ge_engine_destroy(engine);
        return 10;
    }

    if (ge_scene_mesh_renderer_set(scene, entity, 1001ULL, 2002ULL) != GE_OK ||
        ge_scene_update_transforms(scene) != GE_OK) {
        ge_scene_destroy(scene);
        ge_engine_destroy(engine);
        return 11;
    }

    const char* temp_scene_path = "gameengine_c_api_test.gescene";
    if (ge_scene_save_file(scene, temp_scene_path) != GE_OK) {
        remove(temp_scene_path);
        ge_scene_destroy(scene);
        ge_engine_destroy(engine);
        return 12;
    }

    GeScene* loaded_scene = NULL;
    if (ge_scene_create(&loaded_scene) != GE_OK ||
        ge_scene_load_file(loaded_scene, temp_scene_path) != GE_OK) {
        remove(temp_scene_path);
        ge_scene_destroy(loaded_scene);
        ge_scene_destroy(scene);
        ge_engine_destroy(engine);
        return 13;
    }
    remove(temp_scene_path);

    if (ge_scene_entity_count(loaded_scene) != 1U ||
        !ge_scene_entity_is_alive(loaded_scene, entity)) {
        ge_scene_destroy(loaded_scene);
        ge_scene_destroy(scene);
        ge_engine_destroy(engine);
        return 14;
    }

    GeVec3 loaded_pos = {0.0f, 0.0f, 0.0f};
    if (ge_scene_transform_get_position(loaded_scene, entity, &loaded_pos) != GE_OK ||
        !check_float_eq(loaded_pos.x, pos.x) || !check_float_eq(loaded_pos.y, pos.y) ||
        !check_float_eq(loaded_pos.z, pos.z)) {
        ge_scene_destroy(loaded_scene);
        ge_scene_destroy(scene);
        ge_engine_destroy(engine);
        return 15;
    }

    bool should_continue = false;
    if (ge_engine_attach_scene(engine, loaded_scene) != GE_OK ||
        ge_engine_step_frame(engine, &should_continue) != GE_OK || !should_continue ||
        ge_engine_detach_scene(engine) != GE_OK) {
        ge_scene_destroy(loaded_scene);
        ge_scene_destroy(scene);
        ge_engine_destroy(engine);
        return 16;
    }

    if (ge_scene_entity_destroy(scene, entity) != GE_OK ||
        ge_scene_entity_is_alive(scene, entity) ||
        ge_scene_entity_destroy(scene, entity) != GE_ERROR_INVALID_ARGUMENT) {
        ge_scene_destroy(loaded_scene);
        ge_scene_destroy(scene);
        ge_engine_destroy(engine);
        return 17;
    }

    ge_scene_destroy(loaded_scene);
    ge_scene_destroy(scene);
    ge_engine_destroy(engine);
    return 0;
}
