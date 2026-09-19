#include "engine/renderer/render_graph/render_graph.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace gameengine::renderer::render_graph {

namespace {

[[nodiscard]] bool contains_resource(std::span<const ResourceHandle> resources,
                                     ResourceHandle candidate) noexcept
{
    return std::find_if(resources.begin(), resources.end(), [candidate](ResourceHandle value) {
               return value.index == candidate.index;
           }) != resources.end();
}

[[nodiscard]] bool contains_pass(std::span<const PassHandle> passes, PassHandle candidate) noexcept
{
    return std::find_if(passes.begin(), passes.end(), [candidate](PassHandle value) {
               return value.index == candidate.index;
           }) != passes.end();
}

} // namespace

void RenderGraph::reset() noexcept
{
    resources_.clear();
    passes_.clear();
    execution_order_.clear();
    compiled_ = false;
    last_error_ = CompileError::none;
}

void RenderGraph::set_error(CompileError error) noexcept
{
    last_error_ = error;
}

core::Status RenderGraph::add_resource(const ResourceDescription& description,
                                       ResourceHandle& handle) noexcept
{
    handle = {};
    if (compiled_) {
        set_error(CompileError::already_compiled);
        return core::Status{core::ErrorCode::invalid_argument};
    }
    if (description.name.empty() || std::any_of(resources_.begin(),
                                                 resources_.end(),
                                                 [&description](const auto& resource) {
                                                     return resource.name == description.name;
                                                 })) {
        set_error(CompileError::duplicate_resource);
        return core::Status{core::ErrorCode::invalid_argument};
    }

    resources_.push_back(description);
    handle.index = static_cast<core::u32>(resources_.size() - 1U);
    last_error_ = CompileError::none;
    return core::Status{};
}

core::Status RenderGraph::add_pass(const PassDescription& description, PassHandle& handle) noexcept
{
    handle = {};
    if (compiled_) {
        set_error(CompileError::already_compiled);
        return core::Status{core::ErrorCode::invalid_argument};
    }
    if (description.name.empty() || std::any_of(passes_.begin(),
                                                 passes_.end(),
                                                 [&description](const auto& pass) {
                                                     return pass.name == description.name;
                                                 })) {
        set_error(CompileError::duplicate_pass);
        return core::Status{core::ErrorCode::invalid_argument};
    }

    StoredPass pass;
    pass.name = description.name;
    pass.reads.assign(description.reads.begin(), description.reads.end());
    pass.writes.assign(description.writes.begin(), description.writes.end());
    pass.dependencies.assign(description.dependencies.begin(), description.dependencies.end());
    pass.draw_calls = description.draw_calls;
    passes_.push_back(std::move(pass));
    handle.index = static_cast<core::u32>(passes_.size() - 1U);
    last_error_ = CompileError::none;
    return core::Status{};
}

core::Status RenderGraph::compile() noexcept
{
    if (compiled_) {
        set_error(CompileError::already_compiled);
        return core::Status{core::ErrorCode::invalid_argument};
    }
    execution_order_.clear();

    std::vector<PassHandle> writers(resources_.size());
    for (std::size_t index = 0; index < writers.size(); ++index) {
        writers[index] = {};
    }

    for (std::size_t pass_index = 0; pass_index < passes_.size(); ++pass_index) {
        const StoredPass& pass = passes_[pass_index];
        for (ResourceHandle resource : pass.reads) {
            if (!valid_resource(resource)) {
                set_error(CompileError::invalid_resource);
                return core::Status{core::ErrorCode::invalid_argument};
            }
            if (contains_resource(pass.writes, resource) ||
                std::count_if(pass.reads.begin(), pass.reads.end(), [resource](auto value) {
                    return value.index == resource.index;
                }) != 1) {
                set_error(CompileError::same_resource_read_write);
                return core::Status{core::ErrorCode::invalid_argument};
            }
        }
        for (ResourceHandle resource : pass.writes) {
            if (!valid_resource(resource)) {
                set_error(CompileError::invalid_resource);
                return core::Status{core::ErrorCode::invalid_argument};
            }
            if (std::count_if(pass.writes.begin(), pass.writes.end(), [resource](auto value) {
                    return value.index == resource.index;
                }) != 1) {
                set_error(CompileError::duplicate_access);
                return core::Status{core::ErrorCode::invalid_argument};
            }
            PassHandle& writer = writers[resource.index];
            if (writer.valid()) {
                set_error(CompileError::conflicting_write);
                return core::Status{core::ErrorCode::invalid_argument};
            }
            writer.index = static_cast<core::u32>(pass_index);
        }
        for (PassHandle dependency : pass.dependencies) {
            if (!valid_pass(dependency)) {
                set_error(CompileError::invalid_pass);
                return core::Status{core::ErrorCode::invalid_argument};
            }
            if (dependency.index == pass_index) {
                set_error(CompileError::dependency_cycle);
                return core::Status{core::ErrorCode::invalid_argument};
            }
            if (std::count_if(pass.dependencies.begin(),
                              pass.dependencies.end(),
                              [dependency](auto value) {
                                  return value.index == dependency.index;
                              }) != 1) {
                set_error(CompileError::duplicate_access);
                return core::Status{core::ErrorCode::invalid_argument};
            }
        }
    }

    std::vector<std::vector<PassHandle>> edges(passes_.size());
    std::vector<core::u32> indegree(passes_.size(), 0U);
    const auto add_edge = [&edges, &indegree](PassHandle before, PassHandle after) noexcept {
        auto& outgoing = edges[before.index];
        if (std::find_if(outgoing.begin(), outgoing.end(), [after](auto value) {
                return value.index == after.index;
            }) == outgoing.end()) {
            outgoing.push_back(after);
            ++indegree[after.index];
        }
    };

    for (std::size_t pass_index = 0; pass_index < passes_.size(); ++pass_index) {
        const PassHandle current{static_cast<core::u32>(pass_index)};
        const StoredPass& pass = passes_[pass_index];
        for (PassHandle dependency : pass.dependencies) {
            add_edge(dependency, current);
        }
        for (ResourceHandle resource : pass.reads) {
            const PassHandle writer = writers[resource.index];
            if (writer.valid()) {
                add_edge(writer, current);
            }
        }
    }

    std::vector<bool> emitted(passes_.size(), false);
    while (execution_order_.size() < passes_.size()) {
        PassHandle next{};
        for (std::size_t index = 0; index < indegree.size(); ++index) {
            if (!emitted[index] && indegree[index] == 0U) {
                next.index = static_cast<core::u32>(index);
                break;
            }
        }
        if (!next.valid()) {
            execution_order_.clear();
            set_error(CompileError::dependency_cycle);
            return core::Status{core::ErrorCode::invalid_argument};
        }

        emitted[next.index] = true;
        execution_order_.push_back(next);
        for (PassHandle outgoing : edges[next.index]) {
            --indegree[outgoing.index];
        }
    }

    compiled_ = true;
    last_error_ = CompileError::none;
    return core::Status{};
}

const ResourceDescription* RenderGraph::resource(ResourceHandle handle) const noexcept
{
    return valid_resource(handle) ? &resources_[handle.index] : nullptr;
}

std::string_view RenderGraph::pass_name(PassHandle handle) const noexcept
{
    return valid_pass(handle) ? passes_[handle.index].name : std::string_view{};
}

std::span<const ResourceHandle> RenderGraph::pass_reads(PassHandle handle) const noexcept
{
    return valid_pass(handle) ? std::span<const ResourceHandle>{passes_[handle.index].reads}
                              : std::span<const ResourceHandle>{};
}

std::span<const ResourceHandle> RenderGraph::pass_writes(PassHandle handle) const noexcept
{
    return valid_pass(handle) ? std::span<const ResourceHandle>{passes_[handle.index].writes}
                              : std::span<const ResourceHandle>{};
}

core::u32 RenderGraph::pass_draw_calls(PassHandle handle) const noexcept
{
    return valid_pass(handle) ? passes_[handle.index].draw_calls : 0U;
}

bool RenderGraph::valid_resource(ResourceHandle handle) const noexcept
{
    return handle.valid() && handle.index < resources_.size();
}

bool RenderGraph::valid_pass(PassHandle handle) const noexcept
{
    return handle.valid() && handle.index < passes_.size();
}

} // namespace gameengine::renderer::render_graph
