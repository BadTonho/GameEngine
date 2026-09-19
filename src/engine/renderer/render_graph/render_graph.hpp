#pragma once

#include <span>
#include <string_view>
#include <vector>

#include "engine/core/status.hpp"

namespace gameengine::renderer::render_graph {

constexpr core::u32 invalid_handle_index = 0xffffffffU;

enum class ResourceKind : core::u8 {
    color_attachment = 0,
    depth_attachment,
    storage_buffer,
    vertex_buffer,
    indirect_buffer,
};

struct ResourceHandle final {
    core::u32 index = invalid_handle_index;

    [[nodiscard]] constexpr bool valid() const noexcept
    {
        return index != invalid_handle_index;
    }
};

struct PassHandle final {
    core::u32 index = invalid_handle_index;

    [[nodiscard]] constexpr bool valid() const noexcept
    {
        return index != invalid_handle_index;
    }
};

enum class CompileError : core::u8 {
    none = 0,
    already_compiled,
    duplicate_resource,
    invalid_resource,
    duplicate_pass,
    invalid_pass,
    duplicate_access,
    same_resource_read_write,
    conflicting_write,
    dependency_cycle,
};

struct ResourceDescription final {
    std::string_view name{};
    ResourceKind kind = ResourceKind::color_attachment;
    bool imported = false;
};

struct PassDescription final {
    std::string_view name{};
    std::span<const ResourceHandle> reads{};
    std::span<const ResourceHandle> writes{};
    std::span<const PassHandle> dependencies{};
    core::u32 draw_calls = 0;
    core::u32 dispatch_calls = 0;
};

class RenderGraph final {
public:
    void reset() noexcept;

    [[nodiscard]] core::Status add_resource(const ResourceDescription& description,
                                             ResourceHandle& handle) noexcept;
    [[nodiscard]] core::Status add_pass(const PassDescription& description,
                                        PassHandle& handle) noexcept;
    [[nodiscard]] core::Status compile() noexcept;

    [[nodiscard]] bool compiled() const noexcept { return compiled_; }
    [[nodiscard]] CompileError last_error() const noexcept { return last_error_; }
    [[nodiscard]] core::usize resource_count() const noexcept { return resources_.size(); }
    [[nodiscard]] core::usize pass_count() const noexcept { return passes_.size(); }
    [[nodiscard]] std::span<const PassHandle> execution_order() const noexcept
    {
        return execution_order_;
    }

    [[nodiscard]] const ResourceDescription* resource(ResourceHandle handle) const noexcept;
    [[nodiscard]] std::string_view pass_name(PassHandle handle) const noexcept;
    [[nodiscard]] std::span<const ResourceHandle> pass_reads(PassHandle handle) const noexcept;
    [[nodiscard]] std::span<const ResourceHandle> pass_writes(PassHandle handle) const noexcept;
    [[nodiscard]] core::u32 pass_draw_calls(PassHandle handle) const noexcept;
    [[nodiscard]] core::u32 pass_dispatch_calls(PassHandle handle) const noexcept;

private:
    struct StoredPass final {
        std::string_view name{};
        std::vector<ResourceHandle> reads;
        std::vector<ResourceHandle> writes;
        std::vector<PassHandle> dependencies;
        core::u32 draw_calls = 0;
        core::u32 dispatch_calls = 0;
    };

    [[nodiscard]] bool valid_resource(ResourceHandle handle) const noexcept;
    [[nodiscard]] bool valid_pass(PassHandle handle) const noexcept;
    void set_error(CompileError error) noexcept;

    std::vector<ResourceDescription> resources_;
    std::vector<StoredPass> passes_;
    std::vector<PassHandle> execution_order_;
    bool compiled_ = false;
    CompileError last_error_ = CompileError::none;
};

} // namespace gameengine::renderer::render_graph
