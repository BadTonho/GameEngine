#include "engine/editor/ui_types.hpp"
#include "engine/platform/platform.hpp"
#include "engine/renderer/renderer_metrics.hpp"
#include "engine/renderer/scene_bridge.hpp"
#include "engine/rhi/rhi.hpp"
#include "engine/scene/scene.hpp"

#include <array>
#include <cstddef>
#include <cstdio>

namespace {

int fail(const char* message) noexcept
{
    std::fprintf(stderr, "rhi_tests failed: %s\n", message);
    return 1;
}

[[nodiscard]] int test_capabilities_matrix() noexcept
{
    using namespace gameengine::rhi;

    BackendKind parsed = BackendKind::auto_select;
    if (!parse_backend_kind("auto", parsed) || parsed != BackendKind::auto_select) {
        return fail("parse_backend_kind(auto) failed");
    }
    if (!parse_backend_kind("vulkan", parsed) || parsed != BackendKind::vulkan) {
        return fail("parse_backend_kind(vulkan) failed");
    }
    if (!parse_backend_kind("d3d12", parsed) || parsed != BackendKind::d3d12) {
        return fail("parse_backend_kind(d3d12) failed");
    }
    if (!parse_backend_kind("dx12", parsed) || parsed != BackendKind::d3d12) {
        return fail("parse_backend_kind(dx12) failed");
    }
    if (!parse_backend_kind("metal", parsed) || parsed != BackendKind::metal) {
        return fail("parse_backend_kind(metal) failed");
    }
    if (parse_backend_kind("opengl", parsed)) {
        return fail("parse_backend_kind should reject unknown backend");
    }

    if (backend_name(BackendKind::vulkan) != "vulkan" ||
        backend_name(BackendKind::d3d12) != "d3d12" ||
        backend_name(BackendKind::metal) != "metal") {
        return fail("backend_name mismatch");
    }

    const BackendCapabilities vk_caps = query_static_backend_capabilities(BackendKind::vulkan);
    const BackendCapabilities dx_caps = query_static_backend_capabilities(BackendKind::d3d12);
    const BackendCapabilities mtl_caps = query_static_backend_capabilities(BackendKind::metal);

    if (vk_caps.shader_target != ShaderTarget::spirv ||
        dx_caps.shader_target != ShaderTarget::dxil ||
        mtl_caps.shader_target != ShaderTarget::metal) {
        return fail("static capabilities shader_target mismatch");
    }
    if (shader_target_name(vk_caps.shader_target) != "spirv" ||
        shader_target_name(dx_caps.shader_target) != "dxil" ||
        shader_target_name(mtl_caps.shader_target) != "metal") {
        return fail("shader_target_name mismatch");
    }
    if (!vk_caps.compute_supported || !dx_caps.compute_supported || !mtl_caps.compute_supported) {
        return fail("expected compute_supported across baseline backends");
    }
    return 0;
}

[[nodiscard]] int exercise_backend(gameengine::platform::Platform& platform,
                                   gameengine::rhi::BackendKind backend) noexcept
{
    using namespace gameengine;

    for (int cycle = 0; cycle < 2; ++cycle) {
        rhi::Renderer renderer;
        rhi::RendererConfiguration config{};
        config.backend = backend;
        config.enable_validation = (cycle == 0);

        const core::Status init_status = renderer.initialize(platform, config);
        if (!init_status) {
            return fail("renderer.initialize failed");
        }
        if (renderer.backend_kind() != backend) {
            return fail("renderer.backend_kind mismatch");
        }
        const rhi::BackendCapabilities caps = renderer.capabilities();
        if (!caps.compiled || caps.backend != backend) {
            return fail("renderer.capabilities mismatch");
        }

        // Buffer lifecycle + stale handle rejection
        rhi::BufferHandle buf{};
        if (!renderer.create_buffer({.size = 64U, .usage = rhi::BufferUsage::vertex}, buf) ||
            !buf.valid()) {
            return fail("create_buffer failed");
        }
        std::array<std::byte, 16> bytes{};
        if (!renderer.upload_buffer(buf, bytes)) {
            return fail("upload_buffer failed");
        }
        if (!renderer.destroy_buffer(buf)) {
            return fail("destroy_buffer failed");
        }
        if (renderer.upload_buffer(buf, bytes) || renderer.destroy_buffer(buf)) {
            return fail("stale buffer handle was accepted");
        }

        // Image lifecycle + stale handle rejection
        rhi::ImageHandle img{};
        if (!renderer.create_image(
                {.width = 4U, .height = 4U, .format = rhi::ImageFormat::rgba8_unorm}, img) ||
            !img.valid()) {
            return fail("create_image failed");
        }
        std::array<std::byte, 4U * 4U * 4U> pixels{};
        if (!renderer.upload_image(img, pixels)) {
            return fail("upload_image failed");
        }
        if (!renderer.destroy_image(img)) {
            return fail("destroy_image failed");
        }
        if (renderer.upload_image(img, pixels) || renderer.destroy_image(img)) {
            return fail("stale image handle was accepted");
        }

        // Sampler lifecycle + stale handle rejection
        rhi::SamplerHandle sampler{};
        if (!renderer.create_sampler({}, sampler) || !sampler.valid()) {
            return fail("create_sampler failed");
        }
        if (!renderer.destroy_sampler(sampler)) {
            return fail("destroy_sampler failed");
        }
        if (renderer.destroy_sampler(sampler)) {
            return fail("stale sampler handle was accepted");
        }

        // Pipeline lifecycle + stale handle rejection
        rhi::PipelineHandle pipeline{};
        if (!renderer.create_graphics_pipeline({}, pipeline) || !pipeline.valid()) {
            return fail("create_graphics_pipeline failed");
        }
        if (!renderer.destroy_pipeline(pipeline)) {
            return fail("destroy_pipeline failed");
        }
        if (renderer.destroy_pipeline(pipeline)) {
            return fail("stale pipeline handle was accepted");
        }

        // Scene & Editor bridge parity
        scene::Scene scene;
        const scene::Entity cam = scene.create_entity();
        const scene::Entity light = scene.create_entity();
        const scene::Entity mesh = scene.create_entity();
        if (!cam.valid() || !light.valid() || !mesh.valid() ||
            !scene.add_transform(cam, {.local_position = {0.0F, 0.0F, 3.0F}}) ||
            !scene.add_camera(cam, {.active = true}) ||
            !scene.add_transform(light, {}) ||
            !scene.add_directional_light(light, {}) ||
            !scene.add_transform(mesh, {}) ||
            !scene.add_mesh_renderer(
                mesh,
                {.mesh_id = scene::bootstrap_mesh_id, .material_id = scene::bootstrap_material_id}) ||
            !scene.update_transforms()) {
            return fail("test scene setup failed");
        }
        if (!renderer::scene_bridge::attach_scene(renderer, scene)) {
            return fail("attach_scene failed");
        }
        std::array<editor::UiVertex, 3> ui_verts{{
            {.position = {0.0F, 0.5F}, .uv = {0.0F, 0.0F}, .color = {1.0F, 1.0F, 1.0F, 1.0F}},
            {.position = {0.5F, -0.5F}, .uv = {1.0F, 0.0F}, .color = {1.0F, 1.0F, 1.0F, 1.0F}},
            {.position = {-0.5F, -0.5F}, .uv = {0.0F, 1.0F}, .color = {1.0F, 1.0F, 1.0F, 1.0F}},
        }};
        if (!editor::renderer_bridge::set_ui_vertices(renderer, ui_verts)) {
            return fail("set_ui_vertices failed");
        }
        if (!renderer.render_frame(platform)) {
            return fail("render_frame failed");
        }
        renderer::metrics::FrameTimingReport report{};
        if (!editor::renderer_bridge::read_metrics(renderer, report)) {
            return fail("read_metrics failed");
        }
        if (!renderer::scene_bridge::detach_scene(renderer)) {
            return fail("detach_scene failed");
        }

        renderer.shutdown();
        if (renderer.is_initialized()) {
            return fail("renderer still marked initialized after shutdown");
        }
    }

    return 0;
}

} // namespace

int main()
{
    if (test_capabilities_matrix() != 0) {
        return 1;
    }

    gameengine::platform::Platform platform;
    if (!platform.initialize()) {
        std::fprintf(stderr, "rhi_tests: platform initialize skipped/unavailable\n");
        return 0;
    }
    if (!platform.create_window({.title = "RHI Test", .width = 320U, .height = 240U})) {
        platform.shutdown();
        std::fprintf(stderr, "rhi_tests: window creation unavailable; backend tests skipped\n");
        return 0;
    }

#if !defined(__APPLE__)
    {
        gameengine::rhi::Renderer metal_renderer;
        const gameengine::core::Status metal_status = metal_renderer.initialize(
            platform, {.backend = gameengine::rhi::BackendKind::metal});
        if (metal_status.code != gameengine::core::ErrorCode::unsupported_platform) {
            platform.shutdown();
            return fail("expected unsupported_platform for Metal on non-Apple host");
        }
    }
#endif

#if defined(GAMEENGINE_RENDERER_HAS_VULKAN) && GAMEENGINE_RENDERER_HAS_VULKAN
    if (exercise_backend(platform, gameengine::rhi::BackendKind::vulkan) != 0) {
        platform.shutdown();
        return 1;
    }
#endif

#if defined(GAMEENGINE_RENDERER_HAS_D3D12) && GAMEENGINE_RENDERER_HAS_D3D12
    if (exercise_backend(platform, gameengine::rhi::BackendKind::d3d12) != 0) {
        platform.shutdown();
        return 1;
    }
#endif

    platform.shutdown();
    return 0;
}
