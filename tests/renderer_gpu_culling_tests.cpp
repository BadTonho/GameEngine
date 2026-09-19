#include "engine/renderer/gpu_culling.hpp"

#include <array>
#include <cmath>

namespace {

bool test_layouts_and_dispatch() noexcept
{
    using namespace gameengine::renderer::gpu_culling;
    return sizeof(GpuCullInstance) == 80U && sizeof(GpuCullPushConstants) == 112U &&
           sizeof(IndirectCommand) == 20U && dispatch_group_count(0U) == 0U &&
           dispatch_group_count(1U) == 1U && dispatch_group_count(64U) == 1U &&
           dispatch_group_count(65U) == 2U && dispatch_group_count(100'000U) == 1'563U;
}

bool test_visibility_mode_selection() noexcept
{
    using namespace gameengine::renderer::gpu_culling;
    return resolve_visibility_mode(VisibilityMode::cpu, false) == VisibilityMode::cpu &&
           resolve_visibility_mode(VisibilityMode::cpu, true) == VisibilityMode::cpu &&
           resolve_visibility_mode(VisibilityMode::gpu, false) == VisibilityMode::cpu &&
           resolve_visibility_mode(VisibilityMode::gpu, true) == VisibilityMode::gpu;
}

bool test_push_constants_and_indirect_command() noexcept
{
    using namespace gameengine;
    math::Frustum frustum{};
    frustum.valid = true;
    frustum.planes[0] = {{1.0F, 0.0F, 0.0F}, 2.0F};
    frustum.planes[5] = {{0.0F, 0.0F, -1.0F}, 3.0F};
    const auto constants = renderer::gpu_culling::make_push_constants(frustum, 123U);
    if (constants.instance_count != 123U || constants.planes[0].x != 1.0F ||
        constants.planes[0].distance != 2.0F || constants.planes[5].z != -1.0F ||
        constants.planes[5].distance != 3.0F) {
        return false;
    }

    const auto command = renderer::gpu_culling::initial_indirect_command();
    return command.index_count == renderer::gpu_culling::indirect_index_count &&
           command.instance_count == 0U && command.first_index == 0U &&
           command.vertex_offset == 0U && command.first_instance == 0U;
}

bool test_instance_conversion() noexcept
{
    using namespace gameengine;
    renderer::procedural::ProceduralInstance source{};
    source.data.model = math::Mat4::identity();
    source.center = {1.0F, 2.0F, 3.0F};
    source.radius = 4.0F;
    const auto converted = renderer::gpu_culling::to_gpu_instance(source);
    return converted.model.at(0, 0) == 1.0F && converted.model.at(3, 3) == 1.0F &&
           converted.center.x == 1.0F && converted.center.y == 2.0F &&
           converted.center.z == 3.0F && std::isfinite(converted.radius) &&
           converted.radius == 4.0F;
}

} // namespace

int main()
{
    return test_layouts_and_dispatch() && test_visibility_mode_selection() &&
                   test_push_constants_and_indirect_command() &&
                   test_instance_conversion()
               ? 0
               : 1;
}
