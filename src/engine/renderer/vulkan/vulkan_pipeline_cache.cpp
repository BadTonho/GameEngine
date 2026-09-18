#include "engine/renderer/vulkan/vulkan_pipeline_cache.hpp"

#include <cstdio>
#include <cstring>
#include <limits>

namespace gameengine::renderer::vulkan {

namespace {

constexpr core::u32 cache_format_version = 1;

[[nodiscard]] std::FILE* open_file(const char* path, const char* mode) noexcept
{
#if defined(_WIN32)
    std::FILE* file = nullptr;
    return fopen_s(&file, path, mode) == 0 ? file : nullptr;
#else
    return std::fopen(path, mode);
#endif
}

[[nodiscard]] bool identity_matches(const PipelineCacheFileHeader& header,
                                    const PipelineCacheIdentity& identity) noexcept
{
    return header.magic == std::array<char, 4>{'G', 'E', 'P', 'C'} &&
           header.format_version == cache_format_version &&
           header.header_size == sizeof(PipelineCacheFileHeader) &&
           header.vendor_id == identity.vendor_id && header.device_id == identity.device_id &&
           header.driver_version == identity.driver_version &&
           header.api_version == identity.api_version &&
           header.pipeline_cache_uuid == identity.pipeline_cache_uuid;
}

[[nodiscard]] bool build_temporary_path(std::string_view path,
                                        std::array<char, 1024>& temporary_path) noexcept
{
    if (path.empty() || path.size() > temporary_path.size() - 5U) {
        return false;
    }
    const int written = std::snprintf(temporary_path.data(),
                                      temporary_path.size(),
                                      "%.*s.tmp",
                                      static_cast<int>(path.size()),
                                      path.data());
    return written > 0 && static_cast<std::size_t>(written) < temporary_path.size();
}

} // namespace

PipelineCacheIdentity make_pipeline_cache_identity(const VkPhysicalDeviceProperties& properties,
                                                   core::u32 api_version) noexcept
{
    PipelineCacheIdentity identity{};
    identity.vendor_id = properties.vendorID;
    identity.device_id = properties.deviceID;
    identity.driver_version = properties.driverVersion;
    identity.api_version = api_version;
    std::memcpy(identity.pipeline_cache_uuid.data(),
                properties.pipelineCacheUUID,
                identity.pipeline_cache_uuid.size());
    return identity;
}

PipelineCacheLoadResult load_pipeline_cache_file(std::string_view path,
                                                 const PipelineCacheIdentity& expected,
                                                 std::vector<std::byte>& payload) noexcept
{
    payload.clear();
    if (path.empty() || path.size() >= 1024U) {
        return PipelineCacheLoadResult::ignored;
    }

    std::array<char, 1024> path_buffer{};
    std::memcpy(path_buffer.data(), path.data(), path.size());
    path_buffer[path.size()] = '\0';
    std::FILE* file = open_file(path_buffer.data(), "rb");
    if (file == nullptr) {
        return PipelineCacheLoadResult::missing;
    }

    PipelineCacheFileHeader header{};
    const bool header_read = std::fread(&header, sizeof(header), 1, file) == 1;
    if (!header_read || !identity_matches(header, expected) ||
        header.payload_size > pipeline_cache_max_payload ||
        header.payload_size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        std::fclose(file);
        return PipelineCacheLoadResult::ignored;
    }

    if (std::fseek(file, 0, SEEK_END) != 0) {
        std::fclose(file);
        return PipelineCacheLoadResult::ignored;
    }
    const long file_size = std::ftell(file);
    if (file_size < 0 || static_cast<std::uint64_t>(file_size) !=
                             sizeof(PipelineCacheFileHeader) + header.payload_size ||
        std::fseek(file, static_cast<long>(sizeof(PipelineCacheFileHeader)), SEEK_SET) != 0) {
        std::fclose(file);
        return PipelineCacheLoadResult::ignored;
    }

    payload.resize(static_cast<std::size_t>(header.payload_size));
    if (!payload.empty() &&
        std::fread(payload.data(), payload.size(), 1, file) != 1) {
        payload.clear();
        std::fclose(file);
        return PipelineCacheLoadResult::ignored;
    }
    std::fclose(file);
    return PipelineCacheLoadResult::loaded;
}

bool write_pipeline_cache_file(std::string_view path,
                               const PipelineCacheIdentity& identity,
                               std::span<const std::byte> payload) noexcept
{
    if (path.empty() || path.size() >= 1024U || payload.size() > pipeline_cache_max_payload) {
        return false;
    }

    std::array<char, 1024> path_buffer{};
    std::memcpy(path_buffer.data(), path.data(), path.size());
    path_buffer[path.size()] = '\0';
    std::array<char, 1024> temporary_path{};
    if (!build_temporary_path(path, temporary_path)) {
        return false;
    }

    PipelineCacheFileHeader header{};
    header.vendor_id = identity.vendor_id;
    header.device_id = identity.device_id;
    header.driver_version = identity.driver_version;
    header.api_version = identity.api_version;
    header.pipeline_cache_uuid = identity.pipeline_cache_uuid;
    header.payload_size = payload.size();

    std::FILE* file = open_file(temporary_path.data(), "wb");
    if (file == nullptr) {
        return false;
    }
    const bool header_written = std::fwrite(&header, sizeof(header), 1, file) == 1;
    const bool payload_written = payload.empty() || std::fwrite(payload.data(), payload.size(), 1, file) == 1;
    const bool flushed = std::fflush(file) == 0;
    const bool closed = std::fclose(file) == 0;
    if (!header_written || !payload_written || !flushed || !closed) {
        std::remove(temporary_path.data());
        return false;
    }

    std::remove(path_buffer.data());
    if (std::rename(temporary_path.data(), path_buffer.data()) != 0) {
        std::remove(temporary_path.data());
        return false;
    }
    return true;
}

} // namespace gameengine::renderer::vulkan
