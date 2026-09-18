#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <vulkan/vulkan.h>

#include "engine/core/types.hpp"

namespace gameengine::renderer::vulkan {

constexpr core::usize pipeline_cache_max_payload = 64U * 1024U * 1024U;

struct PipelineCacheIdentity final {
    core::u32 vendor_id = 0;
    core::u32 device_id = 0;
    core::u32 driver_version = 0;
    core::u32 api_version = 0;
    std::array<std::uint8_t, VK_UUID_SIZE> pipeline_cache_uuid{};
};

#pragma pack(push, 1)
struct PipelineCacheFileHeader final {
    std::array<char, 4> magic = {'G', 'E', 'P', 'C'};
    core::u32 format_version = 1;
    core::u32 header_size = sizeof(PipelineCacheFileHeader);
    core::u32 vendor_id = 0;
    core::u32 device_id = 0;
    core::u32 driver_version = 0;
    core::u32 api_version = 0;
    std::array<std::uint8_t, VK_UUID_SIZE> pipeline_cache_uuid{};
    std::uint64_t payload_size = 0;
};
#pragma pack(pop)

static_assert(sizeof(PipelineCacheFileHeader) == 52);

enum class PipelineCacheLoadResult : core::u8 {
    missing = 0,
    loaded,
    ignored,
};

[[nodiscard]] PipelineCacheIdentity make_pipeline_cache_identity(
    const VkPhysicalDeviceProperties& properties,
    core::u32 api_version) noexcept;

[[nodiscard]] PipelineCacheLoadResult load_pipeline_cache_file(
    std::string_view path,
    const PipelineCacheIdentity& expected,
    std::vector<std::byte>& payload) noexcept;

[[nodiscard]] bool write_pipeline_cache_file(
    std::string_view path,
    const PipelineCacheIdentity& identity,
    std::span<const std::byte> payload) noexcept;

} // namespace gameengine::renderer::vulkan
