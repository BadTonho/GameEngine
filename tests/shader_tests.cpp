#include "engine/renderer/vulkan/triangle_shaders.hpp"

#include <cstdint>

namespace {

constexpr std::uint32_t spirv_magic = 0x07230203U;

static_assert(sizeof(gameengine::renderer::vulkan::bootstrap::vertex_shader[0]) ==
              sizeof(std::uint32_t));
static_assert(sizeof(gameengine::renderer::vulkan::bootstrap::fragment_shader[0]) ==
              sizeof(std::uint32_t));
static_assert(sizeof(gameengine::renderer::vulkan::bootstrap::compute_shader[0]) ==
              sizeof(std::uint32_t));
static_assert(sizeof(gameengine::renderer::vulkan::bootstrap::benchmark_compute_shader[0]) ==
              sizeof(std::uint32_t));
static_assert(sizeof(gameengine::renderer::vulkan::bootstrap::forward_plus_vertex_shader[0]) ==
              sizeof(std::uint32_t));
static_assert(sizeof(gameengine::renderer::vulkan::bootstrap::forward_plus_fragment_shader[0]) ==
              sizeof(std::uint32_t));
static_assert(sizeof(gameengine::renderer::vulkan::bootstrap::forward_plus_high_vertex_shader[0]) ==
              sizeof(std::uint32_t));
static_assert(sizeof(gameengine::renderer::vulkan::bootstrap::forward_plus_high_fragment_shader[0]) ==
              sizeof(std::uint32_t));
static_assert(sizeof(gameengine::renderer::vulkan::bootstrap::shadow_vertex_shader[0]) ==
              sizeof(std::uint32_t));
static_assert(sizeof(gameengine::renderer::vulkan::bootstrap::environment_compute_shader[0]) ==
              sizeof(std::uint32_t));
static_assert(sizeof(gameengine::renderer::vulkan::bootstrap::editor_ui_vertex_shader[0]) ==
              sizeof(std::uint32_t));
static_assert(sizeof(gameengine::renderer::vulkan::bootstrap::editor_ui_fragment_shader[0]) ==
              sizeof(std::uint32_t));

template <typename ShaderArray>
bool is_valid_spirv(const ShaderArray& shader) noexcept
{
    return !shader.empty() && shader.front() == spirv_magic;
}

} // namespace

int main()
{
    using namespace gameengine::renderer::vulkan::bootstrap;

    if (!is_valid_spirv(vertex_shader) || !is_valid_spirv(fragment_shader) ||
        !is_valid_spirv(compute_shader) || !is_valid_spirv(benchmark_compute_shader) ||
        !is_valid_spirv(forward_plus_vertex_shader) ||
        !is_valid_spirv(forward_plus_fragment_shader) ||
        !is_valid_spirv(forward_plus_high_vertex_shader) ||
        !is_valid_spirv(forward_plus_high_fragment_shader) ||
        !is_valid_spirv(shadow_vertex_shader) ||
        !is_valid_spirv(environment_compute_shader) ||
        !is_valid_spirv(forward_plus_compute_shader) ||
        !is_valid_spirv(editor_ui_vertex_shader) ||
        !is_valid_spirv(editor_ui_fragment_shader)) {
        return 1;
    }

    if (shader_source_sha256.size() != 64U || compute_shader_source_sha256.size() != 64U ||
        vertex_shader_id.size() != 64U || fragment_shader_id.size() != 64U ||
        compute_shader_id.size() != 64U || benchmark_compute_shader_source_sha256.size() != 64U ||
        benchmark_compute_shader_id.size() != 64U ||
        forward_plus_vertex_shader_id.size() != 64U ||
        forward_plus_fragment_shader_id.size() != 64U || shadow_vertex_shader_id.size() != 64U ||
        forward_plus_high_vertex_shader_id.size() != 64U ||
        forward_plus_high_fragment_shader_id.size() != 64U ||
        environment_compute_shader_id.size() != 64U ||
        forward_plus_compute_shader_id.size() != 64U ||
        editor_ui_vertex_shader_id.size() != 64U ||
        editor_ui_fragment_shader_id.size() != 64U ||
        vertex_shader_artifact.spirv != vertex_shader.data() ||
        fragment_shader_artifact.spirv != fragment_shader.data() ||
        compute_shader_artifact.spirv != compute_shader.data() ||
        vertex_shader_artifact.spirv_word_count != vertex_shader.size() ||
        fragment_shader_artifact.spirv_word_count != fragment_shader.size() ||
        compute_shader_artifact.spirv_word_count != compute_shader.size() ||
        benchmark_compute_shader_artifact.spirv != benchmark_compute_shader.data() ||
        benchmark_compute_shader_artifact.spirv_word_count != benchmark_compute_shader.size() ||
        forward_plus_vertex_shader_artifact.spirv != forward_plus_vertex_shader.data() ||
        forward_plus_fragment_shader_artifact.spirv != forward_plus_fragment_shader.data() ||
        forward_plus_high_vertex_shader_artifact.spirv != forward_plus_high_vertex_shader.data() ||
        forward_plus_high_fragment_shader_artifact.spirv != forward_plus_high_fragment_shader.data() ||
        shadow_vertex_shader_artifact.spirv != shadow_vertex_shader.data() ||
        environment_compute_shader_artifact.spirv != environment_compute_shader.data() ||
        forward_plus_compute_shader_artifact.spirv != forward_plus_compute_shader.data() ||
        editor_ui_vertex_shader_artifact.spirv != editor_ui_vertex_shader.data() ||
        editor_ui_fragment_shader_artifact.spirv != editor_ui_fragment_shader.data() ||
        vertex_shader_variants.size() != 1U || fragment_shader_variants.size() != 1U ||
        compute_shader_variants.size() != 1U || benchmark_compute_shader_variants.size() != 1U ||
        forward_plus_vertex_shader_variants.size() != 1U ||
        forward_plus_fragment_shader_variants.size() != 1U ||
        forward_plus_high_vertex_shader_variants.size() != 1U ||
        forward_plus_high_fragment_shader_variants.size() != 1U ||
        shadow_vertex_shader_variants.size() != 1U ||
        editor_ui_vertex_shader_variants.size() != 1U ||
        editor_ui_fragment_shader_variants.size() != 1U) {
        return 2;
    }

    if (environment_compute_shader_variants.size() != 1U ||
        forward_plus_compute_shader_variants.size() != 1U) {
        return 2;
    }

    if (vertex_shader_artifact.entry_point != "vertex_main" ||
        fragment_shader_artifact.entry_point != "fragment_main" ||
        compute_shader_artifact.entry_point != "compute_main" ||
        benchmark_compute_shader_artifact.entry_point != "benchmark_compute_main" ||
        forward_plus_vertex_shader_artifact.entry_point != "forward_plus_vertex_main" ||
        forward_plus_fragment_shader_artifact.entry_point != "forward_plus_fragment_main" ||
        forward_plus_high_vertex_shader_artifact.entry_point != "forward_plus_high_vertex_main" ||
        forward_plus_high_fragment_shader_artifact.entry_point != "forward_plus_high_fragment_main" ||
        shadow_vertex_shader_artifact.entry_point != "shadow_vertex_main" ||
        environment_compute_shader_artifact.entry_point != "environment_compute_main" ||
        forward_plus_compute_shader_artifact.entry_point != "forward_plus_light_list_main" ||
        editor_ui_vertex_shader_artifact.entry_point != "editor_ui_vertex_main" ||
        editor_ui_fragment_shader_artifact.entry_point != "editor_ui_fragment_main" ||
        shader_vertex_layout != "position3_normal3_uv2+instance_model4" ||
        shader_push_constant_size != 64U ||
        shader_vertex_inputs !=
            "location0:position3,location1:normal3,location2:uv2,location3:model_column0,"
            "location4:model_column1,location5:model_column2,location6:model_column3" ||
        shader_resource_layout != "set0:uniform_buffer+sampled_image+sampler" ||
        compute_shader_resource_layout !=
            "set0:storage_buffer+storage_buffer+storage_buffer" ||
        compute_shader_workgroup_size != 64U ||
        benchmark_compute_resource_layout !=
            "set0:storage_buffer+storage_buffer+storage_buffer" ||
        benchmark_compute_workgroup_size != 64U ||
        forward_plus_resource_layout !=
            "set0:uniform_buffer+sampled_image+sampler+storage_buffer+storage_buffer+storage_buffer" ||
        forward_plus_workgroup_size != 16U || shadow_resource_layout != "push_constant_only" ||
        editor_ui_vertex_layout != "position2_uv2_color4" || editor_ui_resource_layout != "none" ||
        forward_plus_high_resource_layout !=
            "set0:uniform_buffer+sampled_image+sampler+storage_buffer+storage_buffer+storage_buffer+sampled_image+sampled_image" ||
        forward_plus_compute_resource_layout !=
            "set0:storage_buffer+storage_buffer+storage_buffer" ||
        forward_plus_compute_workgroup_size != 16U ||
        vertex_shader_artifact.required_capabilities !=
            gameengine::renderer::vulkan::shader_capability_vulkan_1_0 ||
        fragment_shader_artifact.required_capabilities !=
            gameengine::renderer::vulkan::shader_capability_vulkan_1_0 ||
        compute_shader_artifact.required_capabilities !=
            gameengine::renderer::vulkan::shader_capability_vulkan_1_0 ||
        benchmark_compute_shader_artifact.required_capabilities !=
            gameengine::renderer::vulkan::shader_capability_vulkan_1_0) {
        return 3;
    }

    return 0;
}
