#include "engine/renderer/vulkan/shader_pipeline.hpp"
#include "engine/renderer/vulkan/vulkan_pipeline_cache.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <vector>

namespace {

[[nodiscard]] std::FILE* open_file(const char* path, const char* mode) noexcept
{
#if defined(_WIN32)
    std::FILE* file = nullptr;
    return fopen_s(&file, path, mode) == 0 ? file : nullptr;
#else
    return std::fopen(path, mode);
#endif
}

} // namespace

int main()
{
    using namespace gameengine::renderer::vulkan;

    constexpr std::array<std::uint32_t, 1> code = {0x07230203U};
    const std::array<ShaderArtifact, 3> variants = {
        ShaderArtifact{"base", ShaderStage::vertex, "main", shader_capability_none, 0,
                       code.data(), code.size()},
        ShaderArtifact{"advanced", ShaderStage::vertex, "main", shader_capability_vulkan_1_0, 2,
                       code.data(), code.size()},
        ShaderArtifact{"unsupported", ShaderStage::vertex, "main",
                       shader_capability_vulkan_1_1, 3, code.data(), code.size()},
    };
    const auto* baseline = select_shader_variant(variants, {});
    if (baseline == nullptr || baseline->id != "base") {
        return 1;
    }
    const auto* capable = select_shader_variant(
        variants,
        ShaderDeviceCapabilities{shader_capability_vulkan_1_0});
    if (capable == nullptr || capable->id != "advanced") {
        return 2;
    }
    if (shader_capabilities_supported(
            variants[2], ShaderDeviceCapabilities{shader_capability_vulkan_1_0}) ||
        !shader_capabilities_supported(
            variants[2], ShaderDeviceCapabilities{shader_capability_vulkan_1_1})) {
        return 3;
    }

    VkPhysicalDeviceProperties properties{};
    properties.vendorID = 0x1234U;
    properties.deviceID = 0x5678U;
    properties.driverVersion = 9U;
    properties.pipelineCacheUUID[0] = 0x42U;
    const auto identity = make_pipeline_cache_identity(properties, VK_API_VERSION_1_0);
    constexpr std::array<std::byte, 4> payload = {
        std::byte{0x01},
        std::byte{0x02},
        std::byte{0x03},
        std::byte{0x04},
    };
    constexpr const char* cache_path = "gameengine-shader-pipeline-test.cache";
    std::remove(cache_path);
    std::remove("gameengine-shader-pipeline-test.cache.tmp");

    if (!write_pipeline_cache_file(cache_path, identity, payload)) {
        return 4;
    }
    std::vector<std::byte> loaded;
    if (load_pipeline_cache_file(cache_path, identity, loaded) !=
        PipelineCacheLoadResult::loaded ||
        loaded.size() != payload.size() || loaded != std::vector<std::byte>(payload.begin(), payload.end())) {
        std::remove(cache_path);
        return 5;
    }

    auto incompatible = identity;
    incompatible.device_id += 1U;
    if (load_pipeline_cache_file(cache_path, incompatible, loaded) !=
        PipelineCacheLoadResult::ignored) {
        std::remove(cache_path);
        return 6;
    }

    std::FILE* malformed = open_file(cache_path, "wb");
    const bool malformed_written = malformed != nullptr &&
                                   std::fwrite("bad", 3, 1, malformed) == 1;
    const bool malformed_closed = malformed != nullptr && std::fclose(malformed) == 0;
    if (!malformed_written || !malformed_closed ||
        load_pipeline_cache_file(cache_path, identity, loaded) !=
            PipelineCacheLoadResult::ignored) {
        std::remove(cache_path);
        return 7;
    }

    std::remove(cache_path);
    std::remove("gameengine-shader-pipeline-test.cache.tmp");
    return 0;
}
