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
        !is_valid_spirv(compute_shader)) {
        return 1;
    }

    if (shader_source_sha256.size() != 64U || compute_shader_source_sha256.size() != 64U ||
        vertex_shader_id.size() != 64U || fragment_shader_id.size() != 64U ||
        compute_shader_id.size() != 64U ||
        vertex_shader_artifact.spirv != vertex_shader.data() ||
        fragment_shader_artifact.spirv != fragment_shader.data() ||
        compute_shader_artifact.spirv != compute_shader.data() ||
        vertex_shader_artifact.spirv_word_count != vertex_shader.size() ||
        fragment_shader_artifact.spirv_word_count != fragment_shader.size() ||
        compute_shader_artifact.spirv_word_count != compute_shader.size() ||
        vertex_shader_variants.size() != 1U || fragment_shader_variants.size() != 1U ||
        compute_shader_variants.size() != 1U) {
        return 2;
    }

    if (vertex_shader_artifact.entry_point != "vertex_main" ||
        fragment_shader_artifact.entry_point != "fragment_main" ||
        compute_shader_artifact.entry_point != "compute_main" ||
        shader_vertex_layout != "position3_normal3_uv2+instance_model4" ||
        shader_push_constant_size != 64U ||
        shader_vertex_inputs !=
            "location0:position3,location1:normal3,location2:uv2,location3:model_column0,"
            "location4:model_column1,location5:model_column2,location6:model_column3" ||
        shader_resource_layout != "set0:uniform_buffer+sampled_image+sampler" ||
        compute_shader_resource_layout !=
            "set0:storage_buffer+storage_buffer+storage_buffer" ||
        compute_shader_workgroup_size != 64U ||
        vertex_shader_artifact.required_capabilities !=
            gameengine::renderer::vulkan::shader_capability_vulkan_1_0 ||
        fragment_shader_artifact.required_capabilities !=
            gameengine::renderer::vulkan::shader_capability_vulkan_1_0 ||
        compute_shader_artifact.required_capabilities !=
            gameengine::renderer::vulkan::shader_capability_vulkan_1_0) {
        return 3;
    }

    return 0;
}
