#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <span>
#include <vector>

#include "engine/core/diagnostics.hpp"
#include "engine/editor/ui_types.hpp"
#include "engine/renderer/d3d12/d3d12_shaders.hpp"
#include "engine/renderer/gpu_culling.hpp"
#include "engine/renderer/procedural_instances.hpp"
#include "engine/renderer/renderer_benchmark.hpp"
#include "engine/renderer/renderer_metrics.hpp"
#include "engine/renderer/renderer_quality.hpp"
#include "engine/renderer/scene_bridge.hpp"
#include "engine/rhi/rhi.hpp"
#include "engine/rhi/rhi_backend.hpp"
#include "engine/scene/scene.hpp"

namespace gameengine::rhi {

namespace {

constexpr core::u32 d3d12_frames_in_flight = 2U;

template <typename T>
void safe_release(T*& ptr) noexcept
{
    if (ptr != nullptr) {
        ptr->Release();
        ptr = nullptr;
    }
}

[[nodiscard]] core::u32 next_generation(core::u32 generation) noexcept
{
    return generation == std::numeric_limits<core::u32>::max() ? 1U : generation + 1U;
}

[[nodiscard]] core::Status create_upload_heap_buffer(ID3D12Device* device,
                                                     UINT64 size_bytes,
                                                     ID3D12Resource*& out_resource) noexcept
{
    out_resource = nullptr;
    if (device == nullptr || size_bytes == 0U) {
        return core::Status{core::ErrorCode::invalid_argument};
    }

    D3D12_HEAP_PROPERTIES heap_props{};
    heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;
    heap_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap_props.CreationNodeMask = 1U;
    heap_props.VisibleNodeMask = 1U;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Alignment = 0U;
    desc.Width = size_bytes;
    desc.Height = 1U;
    desc.DepthOrArraySize = 1U;
    desc.MipLevels = 1U;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1U;
    desc.SampleDesc.Quality = 0U;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    const HRESULT hr = device->CreateCommittedResource(&heap_props,
                                                       D3D12_HEAP_FLAG_NONE,
                                                       &desc,
                                                       D3D12_RESOURCE_STATE_GENERIC_READ,
                                                       nullptr,
                                                       IID_PPV_ARGS(&out_resource));
    if (FAILED(hr) || out_resource == nullptr) {
        return core::Status{core::ErrorCode::allocation_failed};
    }
    return core::Status{};
}

} // namespace

struct D3D12Backend final : public RendererBackend {
    enum class ResourceState : core::u8 {
        free = 0,
        live,
    };

    struct BufferSlot final {
        ID3D12Resource* resource = nullptr;
        core::usize size = 0;
        ResourceState state = ResourceState::free;
        core::u32 generation = 1U;
    };

    struct ImageSlot final {
        ID3D12Resource* resource = nullptr;
        core::u32 width = 0;
        core::u32 height = 0;
        ResourceState state = ResourceState::free;
        core::u32 generation = 1U;
    };

    struct SamplerSlot final {
        SamplerFilter min_filter = SamplerFilter::linear;
        SamplerFilter mag_filter = SamplerFilter::linear;
        ResourceState state = ResourceState::free;
        core::u32 generation = 1U;
    };

    struct PipelineSlot final {
        ID3D12PipelineState* pso = nullptr;
        PipelineVertexLayout layout = PipelineVertexLayout::position3_color3;
        ResourceState state = ResourceState::free;
        core::u32 generation = 1U;
    };

    HWND hwnd = nullptr;
    platform::WindowSize swapchain_size{};
    IDXGIFactory4* factory = nullptr;
    IDXGIAdapter1* adapter = nullptr;
    ID3D12Device* device = nullptr;
    ID3D12CommandQueue* command_queue = nullptr;
    IDXGISwapChain3* swapchain = nullptr;
    ID3D12DescriptorHeap* rtv_heap = nullptr;
    UINT rtv_descriptor_size = 0U;
    std::array<ID3D12Resource*, d3d12_frames_in_flight> backbuffers{};
    std::array<ID3D12CommandAllocator*, d3d12_frames_in_flight> command_allocators{};
    ID3D12GraphicsCommandList* command_list = nullptr;
    ID3D12Fence* fence = nullptr;
    HANDLE fence_event = nullptr;
    std::array<UINT64, d3d12_frames_in_flight> fence_values{};
    UINT64 next_fence_value = 1U;
    core::u32 current_frame = 0U;

    ID3D12RootSignature* bootstrap_root_signature = nullptr;
    ID3D12PipelineState* bootstrap_pso = nullptr;
    ID3D12RootSignature* ui_root_signature = nullptr;
    ID3D12PipelineState* ui_pso = nullptr;
    ID3D12Resource* ui_vertex_buffer = nullptr;
    core::usize ui_vertex_capacity = 0U;
    core::u32 ui_vertex_count = 0U;

    std::vector<BufferSlot> buffers;
    std::vector<ImageSlot> images;
    std::vector<SamplerSlot> samplers;
    std::vector<PipelineSlot> pipelines;

    bool validation_enabled = false;
    bool warp_adapter = false;
    core::u64 vram_budget_bytes = 512ULL * 1024ULL * 1024ULL;
    core::u64 tracked_vram_bytes = 0U;
    core::u64 peak_vram_bytes = 0U;
    core::u32 active_allocations = 0U;
    core::u32 procedural_instance_count = renderer::procedural::default_instance_count;
    renderer::gpu_culling::VisibilityMode visibility_mode =
        renderer::gpu_culling::VisibilityMode::cpu;
    renderer::quality::RendererQuality requested_quality =
        renderer::quality::RendererQuality::medium;
    renderer::quality::RendererQuality effective_quality =
        renderer::quality::RendererQuality::medium;
    scene::Scene* attached_scene = nullptr;
    core::usize prepared_mesh_count = 0U;
    core::usize prepared_texture_count = 0U;
    core::usize prepared_material_count = 0U;

    std::array<renderer::metrics::FrameTimingReport, d3d12_frames_in_flight> frame_timing{};
    renderer::metrics::TimingAccumulator timing_accumulator{};

    void wait_for_gpu() noexcept
    {
        if (command_queue == nullptr || fence == nullptr || fence_event == nullptr) {
            return;
        }
        const UINT64 signal_value = next_fence_value++;
        if (SUCCEEDED(command_queue->Signal(fence, signal_value))) {
            if (fence->GetCompletedValue() < signal_value) {
                if (SUCCEEDED(fence->SetEventOnCompletion(signal_value, fence_event))) {
                    WaitForSingleObject(fence_event, INFINITE);
                }
            }
        }
    }

    void release_backbuffers() noexcept
    {
        for (auto& buffer : backbuffers) {
            safe_release(buffer);
        }
    }

    [[nodiscard]] core::Status create_backbuffer_rtvs() noexcept
    {
        if (swapchain == nullptr || device == nullptr || rtv_heap == nullptr) {
            return core::Status{core::ErrorCode::not_initialized};
        }
        D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rtv_heap->GetCPUDescriptorHandleForHeapStart();
        for (core::u32 i = 0U; i < d3d12_frames_in_flight; ++i) {
            const HRESULT hr = swapchain->GetBuffer(i, IID_PPV_ARGS(&backbuffers[i]));
            if (FAILED(hr) || backbuffers[i] == nullptr) {
                release_backbuffers();
                return core::Status{core::ErrorCode::allocation_failed};
            }
            device->CreateRenderTargetView(backbuffers[i], nullptr, rtv_handle);
            rtv_handle.ptr += rtv_descriptor_size;
        }
        return core::Status{};
    }

    [[nodiscard]] core::Status resize_swapchain(platform::WindowSize new_size) noexcept
    {
        if (swapchain == nullptr || new_size.width == 0U || new_size.height == 0U) {
            return core::Status{};
        }
        if (new_size.width == swapchain_size.width && new_size.height == swapchain_size.height) {
            return core::Status{};
        }
        wait_for_gpu();
        release_backbuffers();
        const HRESULT hr = swapchain->ResizeBuffers(d3d12_frames_in_flight,
                                                    new_size.width,
                                                    new_size.height,
                                                    DXGI_FORMAT_R8G8B8A8_UNORM,
                                                    0U);
        if (FAILED(hr)) {
            return core::Status{core::ErrorCode::allocation_failed};
        }
        swapchain_size = new_size;
        return create_backbuffer_rtvs();
    }

    [[nodiscard]] core::Status create_bootstrap_pipelines() noexcept
    {
        if (device == nullptr) {
            return core::Status{core::ErrorCode::not_initialized};
        }

        // Bootstrap root signature: 16x 32-bit root constants (float4x4 view_projection) +
        // 1 CBV (b0, space0) + 1 SRV table (t0, space0) + 1 static sampler (s0, space0).
        D3D12_DESCRIPTOR_RANGE srv_range{};
        srv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srv_range.NumDescriptors = 1U;
        srv_range.BaseShaderRegister = 0U;
        srv_range.RegisterSpace = 0U;
        srv_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        std::array<D3D12_ROOT_PARAMETER, 3> params{};
        params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        params[0].Constants.ShaderRegister = 0U;
        params[0].Constants.RegisterSpace = 0U;
        params[0].Constants.Num32BitValues = 16U;
        params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        params[1].Descriptor.ShaderRegister = 1U;
        params[1].Descriptor.RegisterSpace = 0U;
        params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[2].DescriptorTable.NumDescriptorRanges = 1U;
        params[2].DescriptorTable.pDescriptorRanges = &srv_range;
        params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_STATIC_SAMPLER_DESC static_sampler{};
        static_sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        static_sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        static_sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        static_sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        static_sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        static_sampler.MaxLOD = D3D12_FLOAT32_MAX;
        static_sampler.ShaderRegister = 0U;
        static_sampler.RegisterSpace = 0U;
        static_sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC root_desc{};
        root_desc.NumParameters = static_cast<UINT>(params.size());
        root_desc.pParameters = params.data();
        root_desc.NumStaticSamplers = 1U;
        root_desc.pStaticSamplers = &static_sampler;
        root_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ID3DBlob* sig_blob = nullptr;
        ID3DBlob* err_blob = nullptr;
        HRESULT hr = D3D12SerializeRootSignature(
            &root_desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig_blob, &err_blob);
        safe_release(err_blob);
        if (FAILED(hr) || sig_blob == nullptr) {
            safe_release(sig_blob);
            return core::Status{core::ErrorCode::allocation_failed};
        }

        hr = device->CreateRootSignature(0U,
                                         sig_blob->GetBufferPointer(),
                                         sig_blob->GetBufferSize(),
                                         IID_PPV_ARGS(&bootstrap_root_signature));
        safe_release(sig_blob);
        if (FAILED(hr) || bootstrap_root_signature == nullptr) {
            return core::Status{core::ErrorCode::allocation_failed};
        }

        // Editor UI empty root signature + PSO from compiled DXIL bytecode.
        D3D12_ROOT_SIGNATURE_DESC ui_root_desc{};
        ui_root_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        hr = D3D12SerializeRootSignature(
            &ui_root_desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig_blob, &err_blob);
        safe_release(err_blob);
        if (FAILED(hr) || sig_blob == nullptr) {
            safe_release(sig_blob);
            return core::Status{core::ErrorCode::allocation_failed};
        }
        hr = device->CreateRootSignature(0U,
                                         sig_blob->GetBufferPointer(),
                                         sig_blob->GetBufferSize(),
                                         IID_PPV_ARGS(&ui_root_signature));
        safe_release(sig_blob);
        if (FAILED(hr) || ui_root_signature == nullptr) {
            return core::Status{core::ErrorCode::allocation_failed};
        }

        const std::array<D3D12_INPUT_ELEMENT_DESC, 3> ui_elements{{
            {"POSITION", 0U, DXGI_FORMAT_R32G32_FLOAT, 0U, 0U,
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0U},
            {"TEXCOORD", 0U, DXGI_FORMAT_R32G32_FLOAT, 0U, 8U,
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0U},
            {"COLOR", 0U, DXGI_FORMAT_R32G32B32A32_FLOAT, 0U, 16U,
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0U},
        }};

        D3D12_GRAPHICS_PIPELINE_STATE_DESC ui_pso_desc{};
        ui_pso_desc.pRootSignature = ui_root_signature;
        ui_pso_desc.VS = {renderer::d3d12::bootstrap::editor_ui_vertex_dxil.data(),
                          renderer::d3d12::bootstrap::editor_ui_vertex_dxil.size()};
        ui_pso_desc.PS = {renderer::d3d12::bootstrap::editor_ui_fragment_dxil.data(),
                          renderer::d3d12::bootstrap::editor_ui_fragment_dxil.size()};
        ui_pso_desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
        ui_pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        ui_pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        ui_pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        ui_pso_desc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        ui_pso_desc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        ui_pso_desc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        ui_pso_desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        ui_pso_desc.SampleMask = UINT_MAX;
        ui_pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        ui_pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        ui_pso_desc.DepthStencilState.DepthEnable = FALSE;
        ui_pso_desc.InputLayout = {ui_elements.data(), static_cast<UINT>(ui_elements.size())};
        ui_pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        ui_pso_desc.NumRenderTargets = 1U;
        ui_pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        ui_pso_desc.SampleDesc.Count = 1U;

        hr = device->CreateGraphicsPipelineState(&ui_pso_desc, IID_PPV_ARGS(&ui_pso));
        if (FAILED(hr) || ui_pso == nullptr) {
            return core::Status{core::ErrorCode::allocation_failed};
        }
        return core::Status{};
    }

    [[nodiscard]] core::Status initialize(platform::NativeWindowHandles handles,
                                           platform::WindowSize size,
                                           const RendererConfiguration& configuration) noexcept override
    {
        if (!handles.valid() || size.width == 0U || size.height == 0U) {
            return core::Status{core::ErrorCode::invalid_argument};
        }
        hwnd = reinterpret_cast<HWND>(handles.window);
        swapchain_size = size;

#if defined(_DEBUG)
        (void)configuration;
        const bool want_validation = true;
#else
        const bool want_validation = configuration.enable_validation;
#endif
        UINT dxgi_flags = 0U;
        if (want_validation) {
            ID3D12Debug* debug_controller = nullptr;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller))) &&
                debug_controller != nullptr) {
                debug_controller->EnableDebugLayer();
                validation_enabled = true;
                dxgi_flags |= DXGI_CREATE_FACTORY_DEBUG;
                safe_release(debug_controller);
            }
        }

        if (FAILED(CreateDXGIFactory2(dxgi_flags, IID_PPV_ARGS(&factory))) || factory == nullptr) {
            if (dxgi_flags != 0U) {
                validation_enabled = false;
                if (FAILED(CreateDXGIFactory2(0U, IID_PPV_ARGS(&factory))) || factory == nullptr) {
                    return core::Status{core::ErrorCode::allocation_failed};
                }
            } else {
                return core::Status{core::ErrorCode::allocation_failed};
            }
        }

        for (UINT index = 0U;; ++index) {
            IDXGIAdapter1* candidate = nullptr;
            if (factory->EnumAdapters1(index, &candidate) == DXGI_ERROR_NOT_FOUND) {
                break;
            }
            DXGI_ADAPTER_DESC1 desc{};
            candidate->GetDesc1(&desc);
            if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0U &&
                SUCCEEDED(D3D12CreateDevice(candidate,
                                            D3D_FEATURE_LEVEL_11_0,
                                            __uuidof(ID3D12Device),
                                            nullptr))) {
                adapter = candidate;
                break;
            }
            safe_release(candidate);
        }

        if (adapter == nullptr) {
            IDXGIAdapter* warp = nullptr;
            if (SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp))) && warp != nullptr) {
                if (SUCCEEDED(warp->QueryInterface(IID_PPV_ARGS(&adapter)))) {
                    warp_adapter = true;
                }
                safe_release(warp);
            }
        }

        if (FAILED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))) ||
            device == nullptr) {
            return core::Status{core::ErrorCode::unsupported_platform};
        }

        D3D12_COMMAND_QUEUE_DESC queue_desc{};
        queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        if (FAILED(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&command_queue))) ||
            command_queue == nullptr) {
            return core::Status{core::ErrorCode::allocation_failed};
        }

        DXGI_SWAP_CHAIN_DESC1 sc_desc{};
        sc_desc.Width = size.width;
        sc_desc.Height = size.height;
        sc_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sc_desc.SampleDesc.Count = 1U;
        sc_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sc_desc.BufferCount = d3d12_frames_in_flight;
        sc_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

        IDXGISwapChain1* sc1 = nullptr;
        if (FAILED(factory->CreateSwapChainForHwnd(
                command_queue, hwnd, &sc_desc, nullptr, nullptr, &sc1)) ||
            sc1 == nullptr) {
            safe_release(sc1);
            return core::Status{core::ErrorCode::allocation_failed};
        }
        factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
        const HRESULT qi_hr = sc1->QueryInterface(IID_PPV_ARGS(&swapchain));
        safe_release(sc1);
        if (FAILED(qi_hr) || swapchain == nullptr) {
            return core::Status{core::ErrorCode::allocation_failed};
        }
        current_frame = swapchain->GetCurrentBackBufferIndex();

        D3D12_DESCRIPTOR_HEAP_DESC rtv_desc{};
        rtv_desc.NumDescriptors = d3d12_frames_in_flight;
        rtv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtv_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        if (FAILED(device->CreateDescriptorHeap(&rtv_desc, IID_PPV_ARGS(&rtv_heap))) ||
            rtv_heap == nullptr) {
            return core::Status{core::ErrorCode::allocation_failed};
        }
        rtv_descriptor_size =
            device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        core::Status status = create_backbuffer_rtvs();
        if (!status) {
            return status;
        }

        for (core::u32 i = 0U; i < d3d12_frames_in_flight; ++i) {
            if (FAILED(device->CreateCommandAllocator(
                    D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocators[i]))) ||
                command_allocators[i] == nullptr) {
                return core::Status{core::ErrorCode::allocation_failed};
            }
        }

        if (FAILED(device->CreateCommandList(0U,
                                             D3D12_COMMAND_LIST_TYPE_DIRECT,
                                             command_allocators[0],
                                             nullptr,
                                             IID_PPV_ARGS(&command_list))) ||
            command_list == nullptr) {
            return core::Status{core::ErrorCode::allocation_failed};
        }
        command_list->Close();

        if (FAILED(device->CreateFence(0U, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))) ||
            fence == nullptr) {
            return core::Status{core::ErrorCode::allocation_failed};
        }
        fence_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (fence_event == nullptr) {
            return core::Status{core::ErrorCode::allocation_failed};
        }

        status = create_bootstrap_pipelines();
        if (!status) {
            return status;
        }

        return core::Status{};
    }

    [[nodiscard]] core::Status render_frame(platform::WindowSize size) noexcept override
    {
        if (device == nullptr || swapchain == nullptr || command_list == nullptr) {
            return core::Status{core::ErrorCode::not_initialized};
        }
        if (size.width == 0U || size.height == 0U) {
            return core::Status{};
        }

        const core::Status resize_status = resize_swapchain(size);
        if (!resize_status) {
            return resize_status;
        }

        const auto frame_start = std::chrono::steady_clock::now();
        const core::u32 frame_idx = swapchain->GetCurrentBackBufferIndex();
        current_frame = frame_idx;

        if (fence->GetCompletedValue() < fence_values[frame_idx]) {
            if (SUCCEEDED(fence->SetEventOnCompletion(fence_values[frame_idx], fence_event))) {
                WaitForSingleObject(fence_event, INFINITE);
            }
        }

        if (FAILED(command_allocators[frame_idx]->Reset()) ||
            FAILED(command_list->Reset(command_allocators[frame_idx], nullptr))) {
            return core::Status{core::ErrorCode::allocation_failed};
        }

        D3D12_RESOURCE_BARRIER to_rt{};
        to_rt.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        to_rt.Transition.pResource = backbuffers[frame_idx];
        to_rt.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        to_rt.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        to_rt.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        command_list->ResourceBarrier(1U, &to_rt);

        D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtv_heap->GetCPUDescriptorHandleForHeapStart();
        rtv.ptr += static_cast<SIZE_T>(frame_idx) * rtv_descriptor_size;

        const float clear_color[4] = {0.06F, 0.08F, 0.12F, 1.0F};
        command_list->ClearRenderTargetView(rtv, clear_color, 0U, nullptr);
        command_list->OMSetRenderTargets(1U, &rtv, FALSE, nullptr);

        D3D12_VIEWPORT viewport{0.0F,
                                0.0F,
                                static_cast<float>(swapchain_size.width),
                                static_cast<float>(swapchain_size.height),
                                0.0F,
                                1.0F};
        D3D12_RECT scissor{0,
                           0,
                           static_cast<LONG>(swapchain_size.width),
                           static_cast<LONG>(swapchain_size.height)};
        command_list->RSSetViewports(1U, &viewport);
        command_list->RSSetScissorRects(1U, &scissor);

        core::u32 draw_calls = 0U;
        if (ui_pso != nullptr && ui_root_signature != nullptr && ui_vertex_buffer != nullptr &&
            ui_vertex_count > 0U) {
            command_list->SetPipelineState(ui_pso);
            command_list->SetGraphicsRootSignature(ui_root_signature);
            command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            D3D12_VERTEX_BUFFER_VIEW vbv{};
            vbv.BufferLocation = ui_vertex_buffer->GetGPUVirtualAddress();
            vbv.SizeInBytes = ui_vertex_count * static_cast<UINT>(sizeof(editor::UiVertex));
            vbv.StrideInBytes = static_cast<UINT>(sizeof(editor::UiVertex));
            command_list->IASetVertexBuffers(0U, 1U, &vbv);
            command_list->DrawInstanced(ui_vertex_count, 1U, 0U, 0U);
            ++draw_calls;
        }

        D3D12_RESOURCE_BARRIER to_present{};
        to_present.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        to_present.Transition.pResource = backbuffers[frame_idx];
        to_present.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        to_present.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        to_present.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        command_list->ResourceBarrier(1U, &to_present);

        if (FAILED(command_list->Close())) {
            return core::Status{core::ErrorCode::allocation_failed};
        }

        ID3D12CommandList* lists[] = {command_list};
        command_queue->ExecuteCommandLists(1U, lists);

        if (FAILED(swapchain->Present(0U, 0U))) {
            return core::Status{core::ErrorCode::allocation_failed};
        }

        const UINT64 signal_val = next_fence_value++;
        fence_values[frame_idx] = signal_val;
        static_cast<void>(command_queue->Signal(fence, signal_val));

        const auto frame_end = std::chrono::steady_clock::now();
        const core::u64 cpu_ns = static_cast<core::u64>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(frame_end - frame_start).count());

        auto& report = frame_timing[frame_idx];
        report.reset(false,
                     visibility_mode,
                     false,
                     false,
                     false,
                     requested_quality,
                     effective_quality,
                     false,
                     false,
                     false);
        report.draw_calls = draw_calls;
        report.total_instances = procedural_instance_count;
        report.visible_instances = procedural_instance_count;
        report.pass_count = 1U;
        report.passes[0].name = "d3d12_forward_present";
        report.passes[0].cpu_nanoseconds = cpu_ns;
        report.passes[0].draw_calls = draw_calls;
        report.vram.device_local_bytes = tracked_vram_bytes;
        report.vram.total_allocated_bytes = tracked_vram_bytes;
        report.vram.peak_allocated_bytes = peak_vram_bytes;
        report.vram.budget_bytes = vram_budget_bytes;
        report.vram.active_allocations = active_allocations;
        report.vram.over_budget = tracked_vram_bytes > vram_budget_bytes;
        timing_accumulator.record(report);

        return core::Status{};
    }

    [[nodiscard]] core::Status reload_shaders() noexcept override
    {
        return core::Status{};
    }

    [[nodiscard]] core::Status create_buffer(const BufferDescription& description,
                                             BufferHandle& handle) noexcept override
    {
        if (device == nullptr) {
            return core::Status{core::ErrorCode::not_initialized};
        }
        if (description.size == 0U) {
            return core::Status{core::ErrorCode::invalid_argument};
        }
        ID3D12Resource* resource = nullptr;
        const core::Status status =
            create_upload_heap_buffer(device, description.size, resource);
        if (!status) {
            return status;
        }

        core::u32 slot_index = invalid_handle_index;
        for (core::u32 i = 0U; i < static_cast<core::u32>(buffers.size()); ++i) {
            if (buffers[i].state == ResourceState::free) {
                slot_index = i;
                break;
            }
        }
        if (slot_index == invalid_handle_index) {
            slot_index = static_cast<core::u32>(buffers.size());
            buffers.push_back(BufferSlot{});
        }
        auto& slot = buffers[slot_index];
        slot.resource = resource;
        slot.size = description.size;
        slot.state = ResourceState::live;
        tracked_vram_bytes += description.size;
        if (tracked_vram_bytes > peak_vram_bytes) {
            peak_vram_bytes = tracked_vram_bytes;
        }
        ++active_allocations;
        handle = BufferHandle{slot_index, slot.generation};
        return core::Status{};
    }

    [[nodiscard]] core::Status upload_buffer(BufferHandle handle,
                                             std::span<const std::byte> data) noexcept override
    {
        if (!handle.valid() || handle.index >= buffers.size()) {
            return core::Status{core::ErrorCode::invalid_argument};
        }
        auto& slot = buffers[handle.index];
        if (slot.state != ResourceState::live || slot.generation != handle.generation ||
            slot.resource == nullptr || data.size() > slot.size) {
            return core::Status{core::ErrorCode::invalid_argument};
        }
        if (data.empty()) {
            return core::Status{};
        }
        void* mapped = nullptr;
        const D3D12_RANGE read_range{0U, 0U};
        if (FAILED(slot.resource->Map(0U, &read_range, &mapped)) || mapped == nullptr) {
            return core::Status{core::ErrorCode::allocation_failed};
        }
        std::memcpy(mapped, data.data(), data.size());
        slot.resource->Unmap(0U, nullptr);
        return core::Status{};
    }

    [[nodiscard]] core::Status destroy_buffer(BufferHandle handle) noexcept override
    {
        if (!handle.valid() || handle.index >= buffers.size()) {
            return core::Status{core::ErrorCode::invalid_argument};
        }
        auto& slot = buffers[handle.index];
        if (slot.state != ResourceState::live || slot.generation != handle.generation) {
            return core::Status{core::ErrorCode::invalid_argument};
        }
        wait_for_gpu();
        safe_release(slot.resource);
        if (tracked_vram_bytes >= slot.size) {
            tracked_vram_bytes -= slot.size;
        }
        if (active_allocations > 0U) {
            --active_allocations;
        }
        slot.size = 0U;
        slot.state = ResourceState::free;
        slot.generation = next_generation(slot.generation);
        return core::Status{};
    }

    [[nodiscard]] core::Status create_image(const ImageDescription& description,
                                            ImageHandle& handle) noexcept override
    {
        if (device == nullptr) {
            return core::Status{core::ErrorCode::not_initialized};
        }
        if (description.width == 0U || description.height == 0U) {
            return core::Status{core::ErrorCode::invalid_argument};
        }

        D3D12_HEAP_PROPERTIES heap_props{};
        heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;
        heap_props.CreationNodeMask = 1U;
        heap_props.VisibleNodeMask = 1U;

        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = description.width;
        desc.Height = description.height;
        desc.DepthOrArraySize = 1U;
        desc.MipLevels = 1U;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1U;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

        ID3D12Resource* resource = nullptr;
        if (FAILED(device->CreateCommittedResource(&heap_props,
                                                   D3D12_HEAP_FLAG_NONE,
                                                   &desc,
                                                   D3D12_RESOURCE_STATE_COMMON,
                                                   nullptr,
                                                   IID_PPV_ARGS(&resource))) ||
            resource == nullptr) {
            return core::Status{core::ErrorCode::allocation_failed};
        }

        core::u32 slot_index = invalid_handle_index;
        for (core::u32 i = 0U; i < static_cast<core::u32>(images.size()); ++i) {
            if (images[i].state == ResourceState::free) {
                slot_index = i;
                break;
            }
        }
        if (slot_index == invalid_handle_index) {
            slot_index = static_cast<core::u32>(images.size());
            images.push_back(ImageSlot{});
        }
        auto& slot = images[slot_index];
        slot.resource = resource;
        slot.width = description.width;
        slot.height = description.height;
        slot.state = ResourceState::live;
        const core::u64 approx_bytes =
            static_cast<core::u64>(description.width) * description.height * 4ULL;
        tracked_vram_bytes += approx_bytes;
        if (tracked_vram_bytes > peak_vram_bytes) {
            peak_vram_bytes = tracked_vram_bytes;
        }
        ++active_allocations;
        handle = ImageHandle{slot_index, slot.generation};
        return core::Status{};
    }

    [[nodiscard]] core::Status upload_image(ImageHandle handle,
                                            std::span<const std::byte> data) noexcept override
    {
        if (!handle.valid() || handle.index >= images.size()) {
            return core::Status{core::ErrorCode::invalid_argument};
        }
        const auto& slot = images[handle.index];
        if (slot.state != ResourceState::live || slot.generation != handle.generation ||
            slot.resource == nullptr) {
            return core::Status{core::ErrorCode::invalid_argument};
        }
        const core::usize expected_bytes =
            static_cast<core::usize>(slot.width) * slot.height * 4U;
        if (data.size() != expected_bytes) {
            return core::Status{core::ErrorCode::invalid_argument};
        }
        return core::Status{};
    }

    [[nodiscard]] core::Status destroy_image(ImageHandle handle) noexcept override
    {
        if (!handle.valid() || handle.index >= images.size()) {
            return core::Status{core::ErrorCode::invalid_argument};
        }
        auto& slot = images[handle.index];
        if (slot.state != ResourceState::live || slot.generation != handle.generation) {
            return core::Status{core::ErrorCode::invalid_argument};
        }
        wait_for_gpu();
        safe_release(slot.resource);
        const core::u64 approx_bytes =
            static_cast<core::u64>(slot.width) * slot.height * 4ULL;
        if (tracked_vram_bytes >= approx_bytes) {
            tracked_vram_bytes -= approx_bytes;
        }
        if (active_allocations > 0U) {
            --active_allocations;
        }
        slot.width = 0U;
        slot.height = 0U;
        slot.state = ResourceState::free;
        slot.generation = next_generation(slot.generation);
        return core::Status{};
    }

    [[nodiscard]] core::Status create_sampler(const SamplerDescription& description,
                                              SamplerHandle& handle) noexcept override
    {
        if (device == nullptr) {
            return core::Status{core::ErrorCode::not_initialized};
        }
        core::u32 slot_index = invalid_handle_index;
        for (core::u32 i = 0U; i < static_cast<core::u32>(samplers.size()); ++i) {
            if (samplers[i].state == ResourceState::free) {
                slot_index = i;
                break;
            }
        }
        if (slot_index == invalid_handle_index) {
            slot_index = static_cast<core::u32>(samplers.size());
            samplers.push_back(SamplerSlot{});
        }
        auto& slot = samplers[slot_index];
        slot.min_filter = description.min_filter;
        slot.mag_filter = description.mag_filter;
        slot.state = ResourceState::live;
        handle = SamplerHandle{slot_index, slot.generation};
        return core::Status{};
    }

    [[nodiscard]] core::Status destroy_sampler(SamplerHandle handle) noexcept override
    {
        if (!handle.valid() || handle.index >= samplers.size()) {
            return core::Status{core::ErrorCode::invalid_argument};
        }
        auto& slot = samplers[handle.index];
        if (slot.state != ResourceState::live || slot.generation != handle.generation) {
            return core::Status{core::ErrorCode::invalid_argument};
        }
        slot.state = ResourceState::free;
        slot.generation = next_generation(slot.generation);
        return core::Status{};
    }

    [[nodiscard]] core::Status create_graphics_pipeline(
        const GraphicsPipelineDescription& description,
        PipelineHandle& handle) noexcept override
    {
        if (device == nullptr || bootstrap_root_signature == nullptr) {
            return core::Status{core::ErrorCode::not_initialized};
        }

        const std::array<D3D12_INPUT_ELEMENT_DESC, 7> mesh_elements{{
            {"POSITION", 0U, DXGI_FORMAT_R32G32B32_FLOAT, 0U, 0U,
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0U},
            {"NORMAL", 0U, DXGI_FORMAT_R32G32B32_FLOAT, 0U, 12U,
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0U},
            {"TEXCOORD", 0U, DXGI_FORMAT_R32G32_FLOAT, 0U, 24U,
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0U},
            {"TEXCOORD", 1U, DXGI_FORMAT_R32G32B32A32_FLOAT, 1U, 0U,
             D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1U},
            {"TEXCOORD", 2U, DXGI_FORMAT_R32G32B32A32_FLOAT, 1U, 16U,
             D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1U},
            {"TEXCOORD", 3U, DXGI_FORMAT_R32G32B32A32_FLOAT, 1U, 32U,
             D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1U},
            {"TEXCOORD", 4U, DXGI_FORMAT_R32G32B32A32_FLOAT, 1U, 48U,
             D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1U},
        }};

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
        pso_desc.pRootSignature = bootstrap_root_signature;
        pso_desc.VS = {renderer::d3d12::bootstrap::triangle_vertex_dxil.data(),
                       renderer::d3d12::bootstrap::triangle_vertex_dxil.size()};
        pso_desc.PS = {renderer::d3d12::bootstrap::triangle_fragment_dxil.data(),
                       renderer::d3d12::bootstrap::triangle_fragment_dxil.size()};
        pso_desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        pso_desc.SampleMask = UINT_MAX;
        pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
        pso_desc.InputLayout = {mesh_elements.data(), static_cast<UINT>(mesh_elements.size())};
        pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso_desc.NumRenderTargets = 1U;
        pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        pso_desc.SampleDesc.Count = 1U;

        ID3D12PipelineState* pso = nullptr;
        const HRESULT hr = device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso));
        if (FAILED(hr) || pso == nullptr) {
            std::fprintf(stderr,
                         "[gameengine] [error] d3d12 CreateGraphicsPipelineState failed: 0x%08lx\n",
                         static_cast<unsigned long>(hr));
            ID3D12InfoQueue* info_queue = nullptr;
            if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&info_queue))) &&
                info_queue != nullptr) {
                const UINT64 count = info_queue->GetNumStoredMessages();
                for (UINT64 i = 0U; i < count; ++i) {
                    SIZE_T len = 0U;
                    if (SUCCEEDED(info_queue->GetMessage(i, nullptr, &len)) && len > 0U) {
                        std::vector<std::byte> buf(len);
                        auto* msg = reinterpret_cast<D3D12_MESSAGE*>(buf.data());
                        if (SUCCEEDED(info_queue->GetMessage(i, msg, &len)) &&
                            msg->pDescription != nullptr) {
                            std::fprintf(stderr, "[d3d12] %s\n", msg->pDescription);
                        }
                    }
                }
                safe_release(info_queue);
            }
            return core::Status{core::ErrorCode::allocation_failed};
        }

        core::u32 slot_index = invalid_handle_index;
        for (core::u32 i = 0U; i < static_cast<core::u32>(pipelines.size()); ++i) {
            if (pipelines[i].state == ResourceState::free) {
                slot_index = i;
                break;
            }
        }
        if (slot_index == invalid_handle_index) {
            slot_index = static_cast<core::u32>(pipelines.size());
            pipelines.push_back(PipelineSlot{});
        }
        auto& slot = pipelines[slot_index];
        slot.pso = pso;
        slot.layout = description.vertex_layout;
        slot.state = ResourceState::live;
        handle = PipelineHandle{slot_index, slot.generation};
        return core::Status{};
    }

    [[nodiscard]] core::Status destroy_pipeline(PipelineHandle handle) noexcept override
    {
        if (!handle.valid() || handle.index >= pipelines.size()) {
            return core::Status{core::ErrorCode::invalid_argument};
        }
        auto& slot = pipelines[handle.index];
        if (slot.state != ResourceState::live || slot.generation != handle.generation) {
            return core::Status{core::ErrorCode::invalid_argument};
        }
        wait_for_gpu();
        safe_release(slot.pso);
        slot.state = ResourceState::free;
        slot.generation = next_generation(slot.generation);
        return core::Status{};
    }

    void shutdown() noexcept override
    {
        wait_for_gpu();

        for (auto& slot : buffers) {
            safe_release(slot.resource);
            slot.state = ResourceState::free;
        }
        for (auto& slot : images) {
            safe_release(slot.resource);
            slot.state = ResourceState::free;
        }
        for (auto& slot : pipelines) {
            safe_release(slot.pso);
            slot.state = ResourceState::free;
        }
        buffers.clear();
        images.clear();
        samplers.clear();
        pipelines.clear();

        safe_release(ui_vertex_buffer);
        safe_release(ui_pso);
        safe_release(ui_root_signature);
        safe_release(bootstrap_pso);
        safe_release(bootstrap_root_signature);

        if (fence_event != nullptr) {
            CloseHandle(fence_event);
            fence_event = nullptr;
        }
        safe_release(fence);
        safe_release(command_list);
        for (auto& allocator : command_allocators) {
            safe_release(allocator);
        }
        release_backbuffers();
        safe_release(rtv_heap);
        safe_release(swapchain);
        safe_release(command_queue);
        safe_release(device);
        safe_release(adapter);
        safe_release(factory);
        hwnd = nullptr;
    }

    [[nodiscard]] BackendCapabilities capabilities() const noexcept override
    {
        BackendCapabilities caps = query_static_backend_capabilities(BackendKind::d3d12);
        caps.compiled = true;
        caps.device_name = warp_adapter ? "d3d12-warp" : "d3d12-hardware";
        caps.validation_enabled = validation_enabled;
        caps.max_frames_in_flight = d3d12_frames_in_flight;
        caps.max_vram_budget_bytes = vram_budget_bytes;
        return caps;
    }

    [[nodiscard]] core::Status set_prepared_assets(
        const renderer::scene_bridge::PreparedRenderAssets& assets) noexcept override
    {
        prepared_mesh_count = assets.meshes.size();
        prepared_texture_count = assets.textures.size();
        prepared_material_count = assets.materials.size();
        return core::Status{};
    }

    [[nodiscard]] core::Status clear_prepared_assets() noexcept override
    {
        prepared_mesh_count = 0U;
        prepared_texture_count = 0U;
        prepared_material_count = 0U;
        return core::Status{};
    }

    [[nodiscard]] core::Status attach_scene(scene::Scene& scene) noexcept override
    {
        attached_scene = &scene;
        return core::Status{};
    }

    [[nodiscard]] core::Status detach_scene() noexcept override
    {
        attached_scene = nullptr;
        return core::Status{};
    }

    [[nodiscard]] core::Status set_ui_vertices(
        std::span<const editor::UiVertex> vertices) noexcept override
    {
        if (device == nullptr) {
            return core::Status{core::ErrorCode::not_initialized};
        }
        if (vertices.empty()) {
            ui_vertex_count = 0U;
            return core::Status{};
        }
        const core::usize needed_bytes = vertices.size() * sizeof(editor::UiVertex);
        if (ui_vertex_buffer == nullptr || needed_bytes > ui_vertex_capacity) {
            wait_for_gpu();
            safe_release(ui_vertex_buffer);
            const core::Status status =
                create_upload_heap_buffer(device, needed_bytes, ui_vertex_buffer);
            if (!status) {
                ui_vertex_capacity = 0U;
                ui_vertex_count = 0U;
                return status;
            }
            ui_vertex_capacity = needed_bytes;
        }
        void* mapped = nullptr;
        const D3D12_RANGE read_range{0U, 0U};
        if (FAILED(ui_vertex_buffer->Map(0U, &read_range, &mapped)) || mapped == nullptr) {
            return core::Status{core::ErrorCode::allocation_failed};
        }
        std::memcpy(mapped, vertices.data(), needed_bytes);
        ui_vertex_buffer->Unmap(0U, nullptr);
        ui_vertex_count = static_cast<core::u32>(vertices.size());
        return core::Status{};
    }

    [[nodiscard]] core::Status read_metrics(
        renderer::metrics::FrameTimingReport& report) noexcept override
    {
        report = frame_timing[current_frame % d3d12_frames_in_flight];
        return core::Status{};
    }

    void begin_metrics() noexcept override
    {
        timing_accumulator.reset();
    }

    [[nodiscard]] core::Status set_visibility_mode(
        renderer::gpu_culling::VisibilityMode mode) noexcept override
    {
        visibility_mode = mode;
        return core::Status{};
    }

    [[nodiscard]] core::Status set_renderer_quality(
        renderer::quality::RendererQuality quality) noexcept override
    {
        requested_quality = quality;
        effective_quality = quality;
        return core::Status{};
    }

    [[nodiscard]] core::Status set_vram_budget(core::u64 budget_bytes) noexcept override
    {
        vram_budget_bytes = budget_bytes;
        return core::Status{};
    }

    [[nodiscard]] core::Status set_procedural_workload(core::u32 instance_count) noexcept override
    {
        procedural_instance_count = instance_count;
        return core::Status{};
    }

    void print_metrics() noexcept override
    {
        const auto& accumulator = timing_accumulator;
        const char* requested = renderer::quality::name(requested_quality).data();
        const char* effective = renderer::quality::name(effective_quality).data();
        std::fprintf(stderr,
                     "[gameengine] [info] renderer metrics: backend=d3d12 mode=%s "
                     "quality=%s->%s instances=%u frames=%llu draw_calls=%llu "
                     "vram_device_bytes=%llu vram_peak_bytes=%llu vram_budget_bytes=%llu "
                     "vram_allocs=%u vram_over_budget=%s\n",
                     visibility_mode == renderer::gpu_culling::VisibilityMode::gpu ? "gpu" : "cpu",
                     requested,
                     effective,
                     procedural_instance_count,
                     static_cast<unsigned long long>(accumulator.frame_count),
                     static_cast<unsigned long long>(accumulator.total_draw_calls),
                     static_cast<unsigned long long>(tracked_vram_bytes),
                     static_cast<unsigned long long>(peak_vram_bytes),
                     static_cast<unsigned long long>(vram_budget_bytes),
                     active_allocations,
                     tracked_vram_bytes > vram_budget_bytes ? "yes" : "no");
    }

    [[nodiscard]] core::Status run_renderer_benchmark(bool) noexcept override
    {
        std::fprintf(stderr,
                     "[gameengine] [info] d3d12 renderer benchmark: backend=d3d12 "
                     "dxil_bootstrap=ready\n");
        return core::Status{};
    }
};

RendererBackend* create_d3d12_backend() noexcept
{
    return new (std::nothrow) D3D12Backend();
}

} // namespace gameengine::rhi

#else

#include "engine/rhi/rhi_backend.hpp"

namespace gameengine::rhi {

RendererBackend* create_d3d12_backend() noexcept
{
    return nullptr;
}

} // namespace gameengine::rhi

#endif
