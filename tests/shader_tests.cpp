#include "engine/renderer/vulkan/triangle_shaders.hpp"

#include <cstdint>

namespace {

constexpr std::uint32_t spirv_magic = 0x07230203U;

static_assert(sizeof(gameengine::renderer::vulkan::bootstrap::vertex_shader[0]) ==
              sizeof(std::uint32_t));
static_assert(sizeof(gameengine::renderer::vulkan::bootstrap::fragment_shader[0]) ==
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

    if (!is_valid_spirv(vertex_shader) || !is_valid_spirv(fragment_shader)) {
        return 1;
    }
    return 0;
}
