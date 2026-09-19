#include "engine/renderer/renderer_benchmark.hpp"

#include <array>
#include <cmath>
#include <string_view>

namespace {

bool test_case_matrix() noexcept
{
    using namespace gameengine::renderer::benchmark;
    if (case_count != 36U || make_case(0U).path != LightingPath::forward ||
        make_case(0U).instance_count != 1'000U || make_case(0U).light_count != 1U ||
        make_case(2U).light_count != 256U || make_case(3U).instance_count != 10'000U ||
        make_case(9U).path != LightingPath::forward_plus ||
        make_case(35U).path != LightingPath::deferred ||
        make_case(35U).instance_count != 100'000U || make_case(35U).light_count != 256U) {
        return false;
    }
    return std::string_view{path_name(LightingPath::clustered)} == "clustered";
}

bool test_dispatch_workloads() noexcept
{
    using namespace gameengine::renderer::benchmark;
    return tile_count(1280U, 720U) == 3'600U && cluster_count(1280U, 720U) == 86'400U &&
           dispatch_work_items(LightingPath::forward, 1280U, 720U, 100'000U, 256U) == 0U &&
           dispatch_work_items(LightingPath::forward_plus, 1280U, 720U, 1'000U, 32U) ==
               3'600U &&
           dispatch_work_items(LightingPath::clustered, 1280U, 720U, 1'000U, 256U) ==
               691'200U &&
           dispatch_work_items(LightingPath::deferred, 1280U, 720U, 100'000U, 1U) ==
               100'000U;
}

bool test_light_generation() noexcept
{
    using namespace gameengine::renderer::benchmark;
    std::array<PointLight, max_point_lights> first{};
    std::array<PointLight, max_point_lights> second{};
    if (!generate_point_lights(256U, first) || !generate_point_lights(256U, second) ||
        generate_point_lights(0U, first) || generate_point_lights(257U, first)) {
        return false;
    }
    for (gameengine::core::u32 index = 0; index < 256U; ++index) {
        if (first[index].position.x != second[index].position.x ||
            first[index].position.y != second[index].position.y ||
            first[index].position.z != second[index].position.z ||
            first[index].radius != second[index].radius ||
            first[index].intensity != second[index].intensity ||
            !std::isfinite(first[index].position.x) || !std::isfinite(first[index].radius)) {
            return false;
        }
    }
    return true;
}

} // namespace

int main()
{
    return test_case_matrix() && test_dispatch_workloads() && test_light_generation() ? 0 : 1;
}
