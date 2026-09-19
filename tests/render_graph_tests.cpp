#include "engine/renderer/render_graph/render_graph.hpp"

#include <array>

namespace {

using gameengine::renderer::render_graph::CompileError;
using gameengine::renderer::render_graph::PassDescription;
using gameengine::renderer::render_graph::PassHandle;
using gameengine::renderer::render_graph::RenderGraph;
using gameengine::renderer::render_graph::ResourceDescription;
using gameengine::renderer::render_graph::ResourceHandle;
using gameengine::renderer::render_graph::ResourceKind;

bool test_deterministic_order_and_resource_dependency() noexcept
{
    RenderGraph graph;
    ResourceHandle color;
    if (!graph.add_resource({"color", ResourceKind::color_attachment, true}, color).ok()) {
        return false;
    }

    PassHandle consumer;
    const std::array<ResourceHandle, 1> reads = {color};
    if (!graph.add_pass({"consumer", reads, {}, {}, 3U}, consumer).ok()) {
        return false;
    }

    PassHandle producer;
    const std::array<ResourceHandle, 1> writes = {color};
    if (!graph.add_pass({"producer", {}, writes, {}, 1U}, producer).ok() ||
        !graph.compile().ok()) {
        return false;
    }
    const auto order = graph.execution_order();
    return order.size() == 2U && order[0].index == producer.index &&
           order[1].index == consumer.index && graph.pass_draw_calls(consumer) == 3U &&
           graph.pass_dispatch_calls(consumer) == 0U;
}

bool test_validation_errors() noexcept
{
    RenderGraph duplicate_resources;
    ResourceHandle first;
    ResourceHandle second;
    if (!duplicate_resources.add_resource({"color", ResourceKind::color_attachment, true}, first)
             .ok() ||
        duplicate_resources.add_resource({"color", ResourceKind::color_attachment, true}, second)
                .ok() ||
        duplicate_resources.last_error() != CompileError::duplicate_resource) {
        return false;
    }

    RenderGraph duplicate_passes;
    PassHandle duplicate_first_pass;
    PassHandle duplicate_second_pass;
    if (!duplicate_passes.add_pass({"same", {}, {}, {}, 0U}, duplicate_first_pass).ok() ||
        duplicate_passes.add_pass({"same", {}, {}, {}, 0U}, duplicate_second_pass).ok() ||
        duplicate_passes.last_error() != CompileError::duplicate_pass) {
        return false;
    }

    RenderGraph invalid_dependency;
    const std::array<PassHandle, 1> missing_dependency = {PassHandle{99U}};
    PassHandle invalid_dependency_pass;
    if (!invalid_dependency
             .add_pass({"invalid_dependency", {}, {}, missing_dependency, 0U},
                       invalid_dependency_pass)
             .ok() ||
        invalid_dependency.compile().ok() ||
        invalid_dependency.last_error() != CompileError::invalid_pass) {
        return false;
    }

    RenderGraph conflicting_writes;
    ResourceHandle color;
    if (!conflicting_writes.add_resource({"color", ResourceKind::color_attachment, true}, color)
             .ok()) {
        return false;
    }
    const std::array<ResourceHandle, 1> writes = {color};
    PassHandle first_pass;
    PassHandle second_pass;
    if (!conflicting_writes.add_pass({"first", {}, writes, {}, 1U}, first_pass).ok() ||
        !conflicting_writes.add_pass({"second", {}, writes, {}, 1U}, second_pass).ok() ||
        conflicting_writes.compile().ok() ||
        conflicting_writes.last_error() != CompileError::conflicting_write) {
        return false;
    }

    RenderGraph invalid_access;
    ResourceHandle valid_resource;
    if (!invalid_access.add_resource({"color", ResourceKind::color_attachment, true},
                                     valid_resource)
             .ok()) {
        return false;
    }
    const ResourceHandle missing_resource{99U};
    const std::array<ResourceHandle, 1> invalid_reads = {missing_resource};
    PassHandle invalid_pass;
    if (!invalid_access.add_pass({"invalid", invalid_reads, {}, {}, 0U}, invalid_pass).ok() ||
        invalid_access.compile().ok() ||
        invalid_access.last_error() != CompileError::invalid_resource) {
        return false;
    }
    return true;
}

bool test_cycle_and_same_pass_conflict() noexcept
{
    RenderGraph cycle;
    PassHandle first;
    PassHandle second;
    if (!cycle.add_pass({"first", {}, {}, {}, 0U}, first).ok() ||
        !cycle.add_pass({"second", {}, {}, {}, 0U}, second).ok()) {
        return false;
    }
    cycle.reset();
    if (!cycle.add_pass({"first", {}, {}, std::array<PassHandle, 1>{PassHandle{1U}}, 0U}, first)
             .ok() ||
        !cycle.add_pass({"second", {}, {}, std::array<PassHandle, 1>{PassHandle{0U}}, 0U}, second)
                 .ok() ||
        cycle.compile().ok() || cycle.last_error() != CompileError::dependency_cycle) {
        return false;
    }

    RenderGraph conflict;
    ResourceHandle color;
    if (!conflict.add_resource({"color", ResourceKind::color_attachment, true}, color).ok()) {
        return false;
    }
    const std::array<ResourceHandle, 1> accesses = {color};
    PassHandle pass;
    if (!conflict.add_pass({"conflict", accesses, accesses, {}, 1U}, pass).ok() ||
        conflict.compile().ok() ||
        conflict.last_error() != CompileError::same_resource_read_write) {
        return false;
    }
    return true;
}

bool test_gpu_cull_dependency() noexcept
{
    RenderGraph graph;
    ResourceHandle source;
    ResourceHandle visible;
    ResourceHandle indirect;
    ResourceHandle color;
    if (!graph.add_resource({"instance_source", ResourceKind::storage_buffer, true}, source)
             .ok() ||
        !graph.add_resource({"visible_instances", ResourceKind::vertex_buffer, true}, visible)
             .ok() ||
        !graph.add_resource({"indirect_command", ResourceKind::indirect_buffer, true}, indirect)
             .ok() ||
        !graph.add_resource({"color", ResourceKind::color_attachment, true}, color).ok()) {
        return false;
    }
    const std::array<ResourceHandle, 1> cull_reads = {source};
    const std::array<ResourceHandle, 2> cull_writes = {visible, indirect};
    PassHandle cull;
    if (!graph.add_pass({"gpu_cull", cull_reads, cull_writes, {}, 0U}, cull).ok()) {
        return false;
    }
    const std::array<ResourceHandle, 2> forward_reads = {visible, indirect};
    const std::array<ResourceHandle, 1> forward_writes = {color};
    PassHandle forward;
    if (!graph.add_pass({"forward_opaque", forward_reads, forward_writes, {}, 1U}, forward)
             .ok() ||
        !graph.compile().ok()) {
        return false;
    }
    const auto order = graph.execution_order();
    return order.size() == 2U && order[0].index == cull.index &&
           order[1].index == forward.index && graph.pass_draw_calls(cull) == 0U &&
           graph.pass_dispatch_calls(cull) == 0U && graph.pass_draw_calls(forward) == 1U;
}

bool test_lighting_prototype_dependencies() noexcept
{
    RenderGraph graph;
    ResourceHandle input;
    ResourceHandle output;
    ResourceHandle color;
    if (!graph.add_resource({"light_input", ResourceKind::storage_buffer, true}, input).ok() ||
        !graph.add_resource({"light_lists", ResourceKind::storage_buffer, true}, output).ok() ||
        !graph.add_resource({"color", ResourceKind::color_attachment, true}, color).ok()) {
        return false;
    }

    const std::array<ResourceHandle, 1> compute_reads = {input};
    const std::array<ResourceHandle, 1> compute_writes = {output};
    PassHandle compute;
    if (!graph.add_pass({"forward_plus_light_cull", compute_reads, compute_writes, {}, 0U, 1U},
                        compute)
             .ok()) {
        return false;
    }
    const std::array<ResourceHandle, 1> forward_reads = {output};
    const std::array<ResourceHandle, 1> forward_writes = {color};
    PassHandle forward;
    if (!graph.add_pass({"forward_opaque", forward_reads, forward_writes, {}, 1U, 0U}, forward)
             .ok() ||
        !graph.compile().ok()) {
        return false;
    }
    const auto order = graph.execution_order();
    return order.size() == 2U && order[0].index == compute.index &&
           order[1].index == forward.index && graph.pass_dispatch_calls(compute) == 1U;
}

} // namespace

int main()
{
    return test_deterministic_order_and_resource_dependency() && test_validation_errors() &&
                   test_cycle_and_same_pass_conflict() && test_gpu_cull_dependency() &&
                   test_lighting_prototype_dependencies()
               ? 0
               : 1;
}
