cmake_minimum_required(VERSION 3.25)

project(GameEngine VERSION 0.0.1 LANGUAGES CXX C)

option(GAMEENGINE_BUILD_TESTS "Build the GameEngine tests" ON)
option(GAMEENGINE_BUILD_VULKAN "Build the Vulkan renderer" ON)
if(WIN32)
    option(GAMEENGINE_BUILD_D3D12 "Build the Direct3D 12 renderer" ON)
else()
    option(GAMEENGINE_BUILD_D3D12 "Build the Direct3D 12 renderer" OFF)
endif()
if(APPLE)
    option(GAMEENGINE_BUILD_METAL "Build the Metal renderer" ON)
else()
    option(GAMEENGINE_BUILD_METAL "Build the Metal renderer" OFF)
endif()
option(GAMEENGINE_BUILD_BENCHMARKS "Build optional scene benchmarks" OFF)
option(GAMEENGINE_BUILD_EDITOR "Build the separate GameEngine editor" ON)
option(GAMEENGINE_BUILD_ANIMATION "Build the optional transform animation module" ON)
option(GAMEENGINE_BUILD_AUDIO "Build the optional procedural audio module" ON)
# Off by default so a clean clone configures without downloading third-party code; the
# development presets enable it.
option(GAMEENGINE_BUILD_PHYSICS "Build the optional Jolt-backed physics module" OFF)
option(GAMEENGINE_BUILD_LUA "Build the optional Lua gameplay scripting module" OFF)
option(GAMEENGINE_MINIMAL_GRAPHICS "Disable all optional Phase 12 advanced graphics features" OFF)
option(GAMEENGINE_ENABLE_TAA "Compile TAA and temporal reconstruction" ON)
option(GAMEENGINE_ENABLE_UPSCALING "Compile spatial/temporal upscaling" ON)
option(GAMEENGINE_ENABLE_SSAO "Compile screen-space ambient occlusion" ON)
option(GAMEENGINE_ENABLE_SSR "Compile screen-space reflections" ON)
option(GAMEENGINE_ENABLE_CONTACT_SHADOWS "Compile screen-space contact shadows" ON)
option(GAMEENGINE_ENABLE_VOLUMETRIC_FOG "Compile volumetric fog froxel pass" ON)
option(GAMEENGINE_ENABLE_GPU_PARTICLES "Compile GPU particle compute pass" ON)
option(GAMEENGINE_ENABLE_REALTIME_GI "Compile real-time GI probe pass" ON)
option(GAMEENGINE_ENABLE_RAY_TRACING "Compile capability-gated ray tracing pass" ON)
option(GAMEENGINE_ENABLE_VIRTUALIZED_GEOMETRY "Compile capability-gated virtualized geometry pass" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

function(gameengine_configure_target target_name)
    target_compile_features(${target_name} PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(${target_name} PRIVATE
            /W4
            /WX
            /permissive-
            /EHs-c-
            /GR-
        )
        target_compile_definitions(${target_name} PRIVATE
            _HAS_EXCEPTIONS=0
        )
    else()
        target_compile_options(${target_name} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
            -fno-exceptions
            -fno-rtti
        )
    endif()
endfunction()

add_library(gameengine_core STATIC
    src/engine/core/core.cpp
    src/engine/core/core.hpp
    src/engine/core/clock.cpp
    src/engine/core/clock.hpp
    src/engine/core/diagnostics.cpp
    src/engine/core/diagnostics.hpp
    src/engine/core/status.hpp
    src/engine/core/types.hpp
    src/engine/input/input.hpp
)

target_include_directories(gameengine_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

gameengine_configure_target(gameengine_core)

if(GAMEENGINE_BUILD_AUDIO)
    find_package(Threads REQUIRED)
    add_library(gameengine_audio STATIC
        src/engine/audio/audio.cpp
        src/engine/audio/audio.hpp
        src/engine/audio/audio_backend.hpp
    )
    target_link_libraries(gameengine_audio PUBLIC gameengine_core Threads::Threads)
    target_include_directories(gameengine_audio
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    )
    target_compile_definitions(gameengine_audio PUBLIC GAMEENGINE_BUILD_AUDIO=1)

    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        target_sources(gameengine_audio PRIVATE src/engine/audio/audio_backend_wasapi.cpp)
        target_link_libraries(gameengine_audio PRIVATE ole32)
        target_compile_definitions(gameengine_audio PRIVATE GAMEENGINE_AUDIO_HAS_NATIVE_BACKEND=1)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(GAMEENGINE_ALSA QUIET IMPORTED_TARGET alsa)
        endif()
        if(TARGET PkgConfig::GAMEENGINE_ALSA)
            target_sources(gameengine_audio PRIVATE src/engine/audio/audio_backend_alsa.cpp)
            target_link_libraries(gameengine_audio PRIVATE PkgConfig::GAMEENGINE_ALSA)
            target_compile_definitions(gameengine_audio PRIVATE GAMEENGINE_AUDIO_HAS_NATIVE_BACKEND=1)
        else()
            target_sources(gameengine_audio PRIVATE src/engine/audio/audio_backend_stub.cpp)
            target_compile_definitions(gameengine_audio PRIVATE GAMEENGINE_AUDIO_HAS_NATIVE_BACKEND=0)
        endif()
    else()
        target_sources(gameengine_audio PRIVATE src/engine/audio/audio_backend_stub.cpp)
        target_compile_definitions(gameengine_audio PRIVATE GAMEENGINE_AUDIO_HAS_NATIVE_BACKEND=0)
    endif()
    gameengine_configure_target(gameengine_audio)

    if(GAMEENGINE_BUILD_BENCHMARKS)
        add_executable(gameengine_audio_benchmark
            benchmarks/audio_benchmark.cpp
        )
        target_link_libraries(gameengine_audio_benchmark PRIVATE gameengine_audio)
        gameengine_configure_target(gameengine_audio_benchmark)
    endif()
endif()

add_library(gameengine_assets STATIC
    src/engine/assets/asset_reader.cpp
    src/engine/assets/asset_reader.hpp
    src/engine/assets/prepared_assets.cpp
    src/engine/assets/prepared_assets.hpp
)
target_link_libraries(gameengine_assets PUBLIC gameengine_core)
target_include_directories(gameengine_assets
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)
gameengine_configure_target(gameengine_assets)

add_library(gameengine_scene STATIC
    src/engine/scene/scene.cpp
    src/engine/scene/scene.hpp
    src/engine/scene/scene_serialization.cpp
    src/engine/scene/scene_serialization.hpp
    src/engine/scene/sparse_set.hpp
)
target_link_libraries(gameengine_scene PUBLIC gameengine_core gameengine_assets)
target_include_directories(gameengine_scene
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)
gameengine_configure_target(gameengine_scene)

if(GAMEENGINE_BUILD_BENCHMARKS)
    add_executable(gameengine_scene_benchmark
        benchmarks/scene_benchmark.cpp
    )
    target_link_libraries(gameengine_scene_benchmark PRIVATE gameengine_scene)
    gameengine_configure_target(gameengine_scene_benchmark)
endif()

if(GAMEENGINE_BUILD_ANIMATION)
    add_library(gameengine_animation STATIC
        src/engine/animation/animation.cpp
        src/engine/animation/animation.hpp
    )
    target_link_libraries(gameengine_animation PUBLIC gameengine_scene)
    target_include_directories(gameengine_animation
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    )
    target_compile_definitions(gameengine_animation PUBLIC GAMEENGINE_BUILD_ANIMATION=1)
    gameengine_configure_target(gameengine_animation)

    if(GAMEENGINE_BUILD_BENCHMARKS)
        add_executable(gameengine_animation_benchmark
            benchmarks/animation_benchmark.cpp
        )
        target_link_libraries(gameengine_animation_benchmark PRIVATE
            gameengine_animation
            gameengine_scene
        )
        gameengine_configure_target(gameengine_animation_benchmark)
    endif()
endif()

if(GAMEENGINE_BUILD_PHYSICS)
    include(FetchContent)

    # Jolt is pinned to a release archive and verified by hash. Set
    # FETCHCONTENT_SOURCE_DIR_JOLTPHYSICS to a local extracted copy to configure offline.
    # Every Jolt option below is chosen explicitly so its PUBLIC compile definitions stay stable.
    set(JOLT_PHYSICS_VERSION 5.6.0)
    set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF)
    set(ENABLE_INSTALL OFF)
    set(ENABLE_ALL_WARNINGS OFF)
    set(OVERRIDE_CXX_FLAGS OFF)
    set(INTERPROCEDURAL_OPTIMIZATION OFF)
    set(CPP_EXCEPTIONS_ENABLED OFF)
    set(CPP_RTTI_ENABLED OFF)
    set(FLOATING_POINT_EXCEPTIONS_ENABLED OFF)
    set(ENABLE_OBJECT_STREAM OFF)
    set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF)
    set(PROFILER_IN_DEBUG_AND_RELEASE OFF)
    set(DOUBLE_PRECISION OFF)
    set(CROSS_PLATFORM_DETERMINISTIC OFF)
    # Jolt GPU compute would pull D3D12/Vulkan/Metal into physics and bypass the RHI boundary.
    set(JPH_USE_DX12 OFF)
    set(JPH_USE_VK OFF)
    set(JPH_USE_MTL OFF)
    set(JPH_USE_CPU_COMPUTE OFF)
    # Baseline x86-64 target: SSE4.2 only, so physics does not raise the minimum CPU requirement.
    set(USE_AVX OFF)
    set(USE_AVX2 OFF)
    set(USE_AVX512 OFF)
    set(USE_LZCNT OFF)
    set(USE_TZCNT OFF)
    set(USE_F16C OFF)
    set(USE_FMADD OFF)

    FetchContent_Declare(JoltPhysics
        URL https://github.com/jrouwe/JoltPhysics/archive/refs/tags/v${JOLT_PHYSICS_VERSION}.tar.gz
        URL_HASH SHA256=6e069ee0172478cc78182047aac87e5310ba14a67a53348ae14cc37801fd3f8e
        SOURCE_SUBDIR Build
        SYSTEM
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(JoltPhysics)
    set_property(TARGET Jolt PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

    add_library(gameengine_physics STATIC
        src/engine/physics/physics.cpp
        src/engine/physics/physics.hpp
    )
    target_link_libraries(gameengine_physics PUBLIC gameengine_scene PRIVATE Jolt::Jolt)
    target_include_directories(gameengine_physics
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    )
    target_compile_definitions(gameengine_physics PUBLIC GAMEENGINE_BUILD_PHYSICS=1)
    gameengine_configure_target(gameengine_physics)

    if(GAMEENGINE_BUILD_BENCHMARKS)
        add_executable(gameengine_physics_benchmark
            benchmarks/physics_benchmark.cpp
        )
        target_link_libraries(gameengine_physics_benchmark PRIVATE
            gameengine_physics
            gameengine_scene
        )
        gameengine_configure_target(gameengine_physics_benchmark)
    endif()
endif()

if(GAMEENGINE_BUILD_LUA)
    include(FetchContent)

    # Lua 5.4.8 is pinned by release tarball and verified by SHA-256. Set
    # FETCHCONTENT_SOURCE_DIR_LUA to a local extracted copy to configure offline.
    set(LUA_VERSION 5.4.8)
    FetchContent_Declare(lua
        URL https://www.lua.org/ftp/lua-${LUA_VERSION}.tar.gz
        URL_HASH SHA256=4F18DDAE154E793E46EEAB727C59EF1C0C0C2B744E7B94219710D76F530629AE
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(lua)

    add_library(gameengine_lua_vm STATIC
        ${lua_SOURCE_DIR}/src/lapi.c
        ${lua_SOURCE_DIR}/src/lauxlib.c
        ${lua_SOURCE_DIR}/src/lbaselib.c
        ${lua_SOURCE_DIR}/src/lcode.c
        ${lua_SOURCE_DIR}/src/lcorolib.c
        ${lua_SOURCE_DIR}/src/lctype.c
        ${lua_SOURCE_DIR}/src/ldblib.c
        ${lua_SOURCE_DIR}/src/ldebug.c
        ${lua_SOURCE_DIR}/src/ldo.c
        ${lua_SOURCE_DIR}/src/ldump.c
        ${lua_SOURCE_DIR}/src/lfunc.c
        ${lua_SOURCE_DIR}/src/lgc.c
        ${lua_SOURCE_DIR}/src/linit.c
        ${lua_SOURCE_DIR}/src/liolib.c
        ${lua_SOURCE_DIR}/src/llex.c
        ${lua_SOURCE_DIR}/src/lmathlib.c
        ${lua_SOURCE_DIR}/src/lmem.c
        ${lua_SOURCE_DIR}/src/loadlib.c
        ${lua_SOURCE_DIR}/src/lobject.c
        ${lua_SOURCE_DIR}/src/lopcodes.c
        ${lua_SOURCE_DIR}/src/loslib.c
        ${lua_SOURCE_DIR}/src/lparser.c
        ${lua_SOURCE_DIR}/src/lstate.c
        ${lua_SOURCE_DIR}/src/lstring.c
        ${lua_SOURCE_DIR}/src/lstrlib.c
        ${lua_SOURCE_DIR}/src/ltable.c
        ${lua_SOURCE_DIR}/src/ltablib.c
        ${lua_SOURCE_DIR}/src/ltm.c
        ${lua_SOURCE_DIR}/src/lundump.c
        ${lua_SOURCE_DIR}/src/lutf8lib.c
        ${lua_SOURCE_DIR}/src/lvm.c
        ${lua_SOURCE_DIR}/src/lzio.c
    )
    set_target_properties(gameengine_lua_vm PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
    )
    target_include_directories(gameengine_lua_vm SYSTEM PUBLIC ${lua_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_definitions(gameengine_lua_vm PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(gameengine_lua_vm PRIVATE /W3)
    elseif(UNIX)
        target_compile_definitions(gameengine_lua_vm PRIVATE LUA_USE_POSIX)
        target_link_libraries(gameengine_lua_vm PRIVATE m)
    endif()

    add_library(gameengine_scripting STATIC
        src/engine/scripting/scripting.cpp
        src/engine/scripting/scripting.hpp
    )
    target_link_libraries(gameengine_scripting
        PUBLIC gameengine_scene
        PRIVATE gameengine_lua_vm
    )
    target_include_directories(gameengine_scripting
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    )
    target_compile_definitions(gameengine_scripting PUBLIC GAMEENGINE_BUILD_LUA=1)
    gameengine_configure_target(gameengine_scripting)

    if(GAMEENGINE_BUILD_BENCHMARKS)
        add_executable(gameengine_scripting_benchmark
            benchmarks/scripting_benchmark.cpp
        )
        target_link_libraries(gameengine_scripting_benchmark PRIVATE
            gameengine_scripting
            gameengine_scene
        )
        gameengine_configure_target(gameengine_scripting_benchmark)
    endif()
endif()

add_library(gameengine_platform STATIC
    src/engine/platform/platform.hpp
)

target_link_libraries(gameengine_platform PRIVATE gameengine_core)
target_include_directories(gameengine_platform
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(X11 REQUIRED)
    target_sources(gameengine_platform PRIVATE src/engine/platform/platform_x11.cpp)
    target_link_libraries(gameengine_platform PRIVATE X11::X11)
    target_compile_definitions(gameengine_platform PUBLIC GAMEENGINE_PLATFORM_HAS_WINDOW=1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_sources(gameengine_platform PRIVATE src/engine/platform/platform_win32.cpp)
    target_compile_definitions(gameengine_platform PUBLIC GAMEENGINE_PLATFORM_HAS_WINDOW=1)
else()
    target_sources(gameengine_platform PRIVATE src/engine/platform/platform_stub.cpp)
    target_compile_definitions(gameengine_platform PUBLIC GAMEENGINE_PLATFORM_HAS_WINDOW=0)
endif()

gameengine_configure_target(gameengine_platform)

add_library(gameengine_rhi INTERFACE)
target_include_directories(gameengine_rhi
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

add_library(gameengine_renderer STATIC
    src/engine/rhi/rhi.hpp
    src/engine/rhi/rhi.cpp
    src/engine/rhi/rhi_backend.hpp
    src/engine/rhi/rhi_capabilities.hpp
    src/engine/rhi/rhi_capabilities.cpp
    src/engine/renderer/gpu_culling.cpp
    src/engine/renderer/gpu_culling.hpp
    src/engine/renderer/renderer_benchmark.cpp
    src/engine/renderer/renderer_benchmark.hpp
    src/engine/renderer/renderer_quality.cpp
    src/engine/renderer/renderer_quality.hpp
    src/engine/renderer/forward_plus.cpp
    src/engine/renderer/forward_plus.hpp
    src/engine/renderer/procedural_instances.cpp
    src/engine/renderer/procedural_instances.hpp
    src/engine/renderer/scene_bridge.hpp
    src/engine/renderer/render_graph/render_graph.cpp
    src/engine/renderer/render_graph/render_graph.hpp
    src/engine/renderer/renderer_metrics.hpp
    src/engine/renderer/advanced_graphics/advanced_features.hpp
    src/engine/renderer/advanced_graphics/advanced_features.cpp
    src/engine/renderer/advanced_graphics/advanced_pipeline.hpp
    src/engine/renderer/advanced_graphics/advanced_pipeline.cpp
    src/engine/renderer/advanced_graphics/advanced_shaders.hpp
)
if(NOT GAMEENGINE_MINIMAL_GRAPHICS AND GAMEENGINE_ENABLE_TAA)
    target_sources(gameengine_renderer PRIVATE
        src/engine/renderer/advanced_graphics/temporal_aa.hpp
        src/engine/renderer/advanced_graphics/temporal_aa.cpp
    )
    target_compile_definitions(gameengine_renderer PUBLIC GAMEENGINE_HAS_TAA=1)
else()
    target_compile_definitions(gameengine_renderer PUBLIC GAMEENGINE_HAS_TAA=0)
endif()
if(NOT GAMEENGINE_MINIMAL_GRAPHICS AND GAMEENGINE_ENABLE_UPSCALING)
    target_sources(gameengine_renderer PRIVATE
        src/engine/renderer/advanced_graphics/upscaling.hpp
        src/engine/renderer/advanced_graphics/upscaling.cpp
    )
    target_compile_definitions(gameengine_renderer PUBLIC GAMEENGINE_HAS_UPSCALING=1)
else()
    target_compile_definitions(gameengine_renderer PUBLIC GAMEENGINE_HAS_UPSCALING=0)
endif()
if(NOT GAMEENGINE_MINIMAL_GRAPHICS AND (GAMEENGINE_ENABLE_SSAO OR GAMEENGINE_ENABLE_SSR OR GAMEENGINE_ENABLE_CONTACT_SHADOWS))
    target_sources(gameengine_renderer PRIVATE
        src/engine/renderer/advanced_graphics/screen_space.hpp
        src/engine/renderer/advanced_graphics/screen_space.cpp
    )
endif()
if(NOT GAMEENGINE_MINIMAL_GRAPHICS AND GAMEENGINE_ENABLE_SSAO)
    target_compile_definitions(gameengine_renderer PUBLIC GAMEENGINE_HAS_SSAO=1)
else()
    target_compile_definitions(gameengine_renderer PUBLIC GAMEENGINE_HAS_SSAO=0)
endif()
if(NOT GAMEENGINE_MINIMAL_GRAPHICS AND GAMEENGINE_ENABLE_SSR)
    target_compile_definitions(gameengine_renderer PUBLIC GAMEENGINE_HAS_SSR=1)
else()
    target_compile_definitions(gameengine_renderer PUBLIC GAMEENGINE_HAS_SSR=0)
endif()
if(NOT GAMEENGINE_MINIMAL_GRAPHICS AND GAMEENGINE_ENABLE_CONTACT_SHADOWS)
    target_compile_definitions(gameengine_renderer PUBLIC GAMEENGINE_HAS_CONTACT_SHADOWS=1)
else()
    target_compile_definitions(gameengine_renderer PUBLIC GAMEENGINE_HAS_CONTACT_SHADOWS=0)
endif()
if(NOT GAMEENGINE_MINIMAL_GRAPHICS AND (GAMEENGINE_ENABLE_VOLUMETRIC_FOG OR GAMEENGINE_ENABLE_GPU_PARTICLES))
    target_sources(gameengine_renderer PRIVATE
        src/engine/renderer/advanced_graphics/volumetrics_and_particles.hpp
        src/engine/renderer/advanced_graphics/volumetrics_and_particles.cpp
    )
endif()
if(NOT GAMEENGINE_MINIMAL_GRAPHICS AND GAMEENGINE_ENABLE_VOLUMETRIC_FOG)
    target_compile_definitions(gameengine_renderer PUBLIC GAMEENGINE_HAS_VOLUMETRIC_FOG=1)
else()
    target_compile_definitions(gameengine_renderer PUBLIC GAMEENGINE_HAS_VOLUMETRIC_FOG=0)
endif()
if(NOT GAMEENGINE_MINIMAL_GRAPHICS AND GAMEENGINE_ENABLE_GPU_PARTICLES)
    target_compile_definitions(gameengine_renderer PUBLIC GAMEENGINE_HAS_GPU_PARTICLES=1)
else()
    target_compile_definitions(gameengine_renderer PUBLIC GAMEENGINE_HAS_GPU_PARTICLES=0)
endif()
if(NOT GAMEENGINE_MINIMAL_GRAPHICS AND (GAMEENGINE_ENABLE_REALTIME_GI OR GAMEENGINE_ENABLE_RAY_TRACING OR GAMEENGINE_ENABLE_VIRTUALIZED_GEOMETRY))
    target_sources(gameengine_renderer PRIVATE
        src/engine/renderer/advanced_graphics/high_end_features.hpp
        src/engine/renderer/advanced_graphics/high_end_features.cpp
    )
endif()
if(NOT GAMEENGINE_MINIMAL_GRAPHICS AND GAMEENGINE_ENABLE_REALTIME_GI)
    target_compile_definitions(gameengine_renderer PUBLIC GAMEENGINE_HAS_REALTIME_GI=1)
else()
    target_compile_definitions(gameengine_renderer PUBLIC GAMEENGINE_HAS_REALTIME_GI=0)
endif()
if(NOT GAMEENGINE_MINIMAL_GRAPHICS AND GAMEENGINE_ENABLE_RAY_TRACING)
    target_compile_definitions(gameengine_renderer PUBLIC GAMEENGINE_HAS_RAY_TRACING=1)
else()
    target_compile_definitions(gameengine_renderer PUBLIC GAMEENGINE_HAS_RAY_TRACING=0)
endif()
if(NOT GAMEENGINE_MINIMAL_GRAPHICS AND GAMEENGINE_ENABLE_VIRTUALIZED_GEOMETRY)
    target_compile_definitions(gameengine_renderer PUBLIC GAMEENGINE_HAS_VIRTUALIZED_GEOMETRY=1)
else()
    target_compile_definitions(gameengine_renderer PUBLIC GAMEENGINE_HAS_VIRTUALIZED_GEOMETRY=0)
endif()
target_link_libraries(gameengine_renderer PUBLIC gameengine_rhi gameengine_platform gameengine_scene)
target_include_directories(gameengine_renderer
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

if(GAMEENGINE_BUILD_VULKAN AND (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Windows"))
    find_package(Vulkan REQUIRED)
    target_sources(gameengine_renderer PRIVATE
        src/engine/renderer/vulkan/renderer_vulkan.cpp
        src/engine/renderer/vulkan/shader_pipeline.cpp
        src/engine/renderer/vulkan/triangle_shaders.hpp
        src/engine/renderer/vulkan/vulkan_pipeline_cache.cpp
        src/engine/renderer/vulkan/vulkan_pipeline_cache.hpp
        src/engine/renderer/vulkan/vulkan_utils.cpp
        src/engine/renderer/vulkan/vulkan_utils.hpp
    )
    target_link_libraries(gameengine_renderer PRIVATE Vulkan::Vulkan)
    target_compile_definitions(gameengine_renderer PUBLIC GAMEENGINE_RENDERER_HAS_VULKAN=1)
else()
    target_sources(gameengine_renderer PRIVATE src/engine/renderer/vulkan/renderer_stub.cpp)
    target_compile_definitions(gameengine_renderer PUBLIC GAMEENGINE_RENDERER_HAS_VULKAN=0)
endif()

if(GAMEENGINE_BUILD_D3D12 AND WIN32)
    target_sources(gameengine_renderer PRIVATE
        src/engine/renderer/d3d12/renderer_d3d12.cpp
        src/engine/renderer/d3d12/d3d12_shaders.hpp
    )
    target_link_libraries(gameengine_renderer PRIVATE d3d12 dxgi)
    target_compile_definitions(gameengine_renderer PUBLIC GAMEENGINE_RENDERER_HAS_D3D12=1)
else()
    target_sources(gameengine_renderer PRIVATE src/engine/renderer/d3d12/renderer_d3d12_stub.cpp)
    target_compile_definitions(gameengine_renderer PUBLIC GAMEENGINE_RENDERER_HAS_D3D12=0)
endif()

if(GAMEENGINE_BUILD_METAL AND APPLE)
    target_sources(gameengine_renderer PRIVATE
        src/engine/renderer/metal/renderer_metal.mm
        src/engine/renderer/metal/metal_shaders.hpp
    )
    target_link_libraries(gameengine_renderer PRIVATE
        "-framework Metal"
        "-framework QuartzCore"
        "-framework Foundation"
    )
    target_compile_definitions(gameengine_renderer PUBLIC GAMEENGINE_RENDERER_HAS_METAL=1)
else()
    target_sources(gameengine_renderer PRIVATE src/engine/renderer/metal/renderer_metal_stub.cpp)
    target_compile_definitions(gameengine_renderer PUBLIC GAMEENGINE_RENDERER_HAS_METAL=0)
endif()

gameengine_configure_target(gameengine_renderer)
if(WIN32)
    target_link_libraries(gameengine_renderer PRIVATE psapi)
endif()

set(GAMEENGINE_SLANGC "" CACHE FILEPATH "Path to the pinned slangc executable")
add_custom_target(gameengine_compile_bootstrap_shaders
    COMMAND ${CMAKE_COMMAND}
        -DGAMEENGINE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DGAMEENGINE_SLANGC=${GAMEENGINE_SLANGC}
        -DGAMEENGINE_SHADER_CACHE_DIR=${CMAKE_CURRENT_BINARY_DIR}/shader-cache
        -DGAMEENGINE_SHADER_CONFIGURATION=$<CONFIG>
        -DGAMEENGINE_SHADER_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/src/engine/renderer/vulkan/triangle_shaders.hpp
        -DGAMEENGINE_SHADER_REFLECTION_DIR=${CMAKE_CURRENT_BINARY_DIR}/shader-reflection
        -P ${CMAKE_CURRENT_SOURCE_DIR}/scripts/compile_bootstrap_shaders.cmake
    VERBATIM
    COMMENT "Compiling bootstrap Slang shaders offline"
)

find_program(GAMEENGINE_CARGO cargo)
if(GAMEENGINE_CARGO)
    add_custom_target(gameengine_asset_tools
        COMMAND ${GAMEENGINE_CARGO} test --manifest-path ${CMAKE_CURRENT_SOURCE_DIR}/tools/Cargo.toml
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        VERBATIM
        COMMENT "Running offline Rust asset pipeline tests"
    )
endif()

add_executable(gameengine_runtime
    src/runtime/main.cpp
)

target_link_libraries(gameengine_runtime PRIVATE gameengine_core gameengine_platform gameengine_renderer)
if(GAMEENGINE_BUILD_ANIMATION)
    target_link_libraries(gameengine_runtime PRIVATE gameengine_animation)
    target_compile_definitions(gameengine_runtime PRIVATE GAMEENGINE_BUILD_ANIMATION=1)
endif()
if(GAMEENGINE_BUILD_AUDIO)
    target_link_libraries(gameengine_runtime PRIVATE gameengine_audio)
    target_compile_definitions(gameengine_runtime PRIVATE GAMEENGINE_BUILD_AUDIO=1)
endif()
if(GAMEENGINE_BUILD_PHYSICS)
    target_link_libraries(gameengine_runtime PRIVATE gameengine_physics)
    target_compile_definitions(gameengine_runtime PRIVATE GAMEENGINE_BUILD_PHYSICS=1)
endif()
if(GAMEENGINE_BUILD_LUA)
    target_link_libraries(gameengine_runtime PRIVATE gameengine_scripting)
    target_compile_definitions(gameengine_runtime PRIVATE GAMEENGINE_BUILD_LUA=1)
endif()
gameengine_configure_target(gameengine_runtime)

if(GAMEENGINE_BUILD_EDITOR)
    add_library(gameengine_editor_core STATIC
        src/engine/editor/asset_catalog.cpp
        src/engine/editor/asset_catalog.hpp
        src/engine/editor/console.cpp
        src/engine/editor/console.hpp
        src/engine/editor/project.cpp
        src/engine/editor/project.hpp
        src/engine/editor/profiler.cpp
        src/engine/editor/profiler.hpp
        src/engine/editor/ui.cpp
        src/engine/editor/ui.hpp
        src/engine/editor/ui_types.hpp
    )
    target_link_libraries(gameengine_editor_core PUBLIC gameengine_core gameengine_renderer)
    target_include_directories(gameengine_editor_core
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    )
    gameengine_configure_target(gameengine_editor_core)
    if(WIN32)
        target_link_libraries(gameengine_editor_core PRIVATE psapi)
    endif()

    add_executable(gameengine_editor
        src/editor/main.cpp
    )
    target_link_libraries(gameengine_editor PRIVATE
        gameengine_core
        gameengine_platform
        gameengine_renderer
        gameengine_editor_core
    )
    if(GAMEENGINE_BUILD_ANIMATION)
        target_link_libraries(gameengine_editor PRIVATE gameengine_animation)
        target_compile_definitions(gameengine_editor PRIVATE GAMEENGINE_BUILD_ANIMATION=1)
    endif()
    if(GAMEENGINE_BUILD_AUDIO)
        target_link_libraries(gameengine_editor PRIVATE gameengine_audio)
        target_compile_definitions(gameengine_editor PRIVATE GAMEENGINE_BUILD_AUDIO=1)
    endif()
    gameengine_configure_target(gameengine_editor)
endif()

add_library(gameengine_c_api STATIC
    src/api/c/gameengine_c.h
    src/api/c/gameengine_c.cpp
)
target_include_directories(gameengine_c_api PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(gameengine_c_api PRIVATE
    gameengine_core
    gameengine_assets
    gameengine_scene
    gameengine_platform
    gameengine_renderer
)
gameengine_configure_target(gameengine_c_api)

if(GAMEENGINE_BUILD_TESTS)
    include(CTest)

    add_executable(gameengine_tests
        tests/core_tests.cpp
    )

    target_link_libraries(gameengine_tests PRIVATE gameengine_core)
    gameengine_configure_target(gameengine_tests)

    add_test(NAME gameengine_core COMMAND gameengine_tests)

    add_executable(gameengine_c_api_tests
        tests/c_api_tests.c
    )
    target_compile_features(gameengine_c_api_tests PRIVATE c_std_11)
    set_target_properties(gameengine_c_api_tests PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
        LINKER_LANGUAGE CXX
    )
    if(MSVC)
        target_compile_options(gameengine_c_api_tests PRIVATE /W4 /WX /permissive-)
    else()
        target_compile_options(gameengine_c_api_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    target_link_libraries(gameengine_c_api_tests PRIVATE gameengine_c_api)
    add_test(NAME gameengine_c_api COMMAND gameengine_c_api_tests)
    set_tests_properties(gameengine_c_api PROPERTIES LABELS "unit")

    add_executable(gameengine_hardening_tests
        tests/hardening_tests.cpp
    )
    target_link_libraries(gameengine_hardening_tests PRIVATE
        gameengine_c_api
        gameengine_audio
        gameengine_scene
        gameengine_assets
        gameengine_core
    )
    if(GAMEENGINE_BUILD_PHYSICS)
        target_link_libraries(gameengine_hardening_tests PRIVATE gameengine_physics)
        target_compile_definitions(gameengine_hardening_tests PRIVATE GAMEENGINE_BUILD_PHYSICS=1)
    endif()
    if(GAMEENGINE_BUILD_LUA)
        target_link_libraries(gameengine_hardening_tests PRIVATE gameengine_scripting)
        target_compile_definitions(gameengine_hardening_tests PRIVATE GAMEENGINE_BUILD_LUA=1)
    endif()
    gameengine_configure_target(gameengine_hardening_tests)
    add_test(NAME gameengine_hardening COMMAND gameengine_hardening_tests)
    set_tests_properties(gameengine_hardening PROPERTIES LABELS "unit")

    add_executable(gameengine_asset_tests
        tests/asset_tests.cpp
    )
    target_link_libraries(gameengine_asset_tests PRIVATE gameengine_assets gameengine_core)
    gameengine_configure_target(gameengine_asset_tests)
    add_test(NAME gameengine_assets COMMAND gameengine_asset_tests)

    add_executable(gameengine_prepared_assets_tests
        tests/prepared_assets_tests.cpp
    )
    target_link_libraries(gameengine_prepared_assets_tests PRIVATE
        gameengine_renderer
        gameengine_scene
        gameengine_assets
        gameengine_core
    )
    gameengine_configure_target(gameengine_prepared_assets_tests)
    add_test(NAME gameengine_prepared_assets COMMAND gameengine_prepared_assets_tests)

    if(GAMEENGINE_BUILD_EDITOR)
        add_executable(gameengine_editor_tests
            tests/editor_tests.cpp
        )
        target_link_libraries(gameengine_editor_tests PRIVATE
            gameengine_editor_core
            gameengine_scene
            gameengine_assets
            gameengine_core
        )
        gameengine_configure_target(gameengine_editor_tests)
        add_test(NAME gameengine_editor COMMAND gameengine_editor_tests)

        add_executable(gameengine_editor_ui_tests
            tests/editor_ui_tests.cpp
        )
        target_link_libraries(gameengine_editor_ui_tests PRIVATE
            gameengine_editor_core
            gameengine_scene
            gameengine_assets
            gameengine_core
        )
        gameengine_configure_target(gameengine_editor_ui_tests)
        add_test(NAME gameengine_editor_ui COMMAND gameengine_editor_ui_tests)

        add_executable(gameengine_editor_tools_tests
            tests/editor_tools_tests.cpp
        )
        target_link_libraries(gameengine_editor_tools_tests PRIVATE
            gameengine_editor_core
            gameengine_scene
            gameengine_assets
            gameengine_renderer
            gameengine_core
        )
        gameengine_configure_target(gameengine_editor_tools_tests)
        add_test(NAME gameengine_editor_tools COMMAND gameengine_editor_tools_tests)
    endif()

    add_executable(gameengine_rhi_tests
        tests/rhi_tests.cpp
    )
    target_link_libraries(gameengine_rhi_tests PRIVATE gameengine_core gameengine_platform gameengine_renderer)
    if(GAMEENGINE_BUILD_VULKAN)
        target_include_directories(gameengine_rhi_tests PRIVATE ${Vulkan_INCLUDE_DIR})
    endif()
    gameengine_configure_target(gameengine_rhi_tests)
    add_test(NAME gameengine_rhi COMMAND gameengine_rhi_tests)

    add_executable(gameengine_shader_tests
        tests/shader_tests.cpp
    )
    target_include_directories(gameengine_shader_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    gameengine_configure_target(gameengine_shader_tests)
    add_test(NAME gameengine_shaders COMMAND gameengine_shader_tests)

    add_executable(gameengine_math_tests
        tests/math_tests.cpp
    )
    target_link_libraries(gameengine_math_tests PRIVATE gameengine_core)
    target_include_directories(gameengine_math_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    gameengine_configure_target(gameengine_math_tests)
    add_test(NAME gameengine_math COMMAND gameengine_math_tests)

    add_executable(gameengine_scene_tests
        tests/scene_tests.cpp
    )
    target_link_libraries(gameengine_scene_tests PRIVATE gameengine_scene gameengine_assets gameengine_core)
    target_include_directories(gameengine_scene_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    gameengine_configure_target(gameengine_scene_tests)
    add_test(NAME gameengine_scene COMMAND gameengine_scene_tests)

    if(GAMEENGINE_BUILD_ANIMATION)
        add_executable(gameengine_animation_tests
            tests/animation_tests.cpp
        )
        target_link_libraries(gameengine_animation_tests PRIVATE
            gameengine_animation
            gameengine_scene
            gameengine_core
        )
        target_include_directories(gameengine_animation_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
        )
        gameengine_configure_target(gameengine_animation_tests)
        add_test(NAME gameengine_animation COMMAND gameengine_animation_tests)
    endif()

    if(GAMEENGINE_BUILD_AUDIO)
        add_executable(gameengine_audio_tests
            tests/audio_tests.cpp
        )
        target_link_libraries(gameengine_audio_tests PRIVATE gameengine_audio gameengine_core)
        target_include_directories(gameengine_audio_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
        )
        gameengine_configure_target(gameengine_audio_tests)
        add_test(NAME gameengine_audio COMMAND gameengine_audio_tests)
    endif()

    if(GAMEENGINE_BUILD_PHYSICS)
        add_executable(gameengine_physics_tests
            tests/physics_tests.cpp
        )
        target_link_libraries(gameengine_physics_tests PRIVATE
            gameengine_physics
            gameengine_scene
            gameengine_core
        )
        target_include_directories(gameengine_physics_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
        )
        gameengine_configure_target(gameengine_physics_tests)
        add_test(NAME gameengine_physics COMMAND gameengine_physics_tests)
    endif()

    if(GAMEENGINE_BUILD_LUA)
        add_executable(gameengine_scripting_tests
            tests/scripting_tests.cpp
        )
        target_link_libraries(gameengine_scripting_tests PRIVATE
            gameengine_scripting
            gameengine_scene
            gameengine_core
        )
        target_include_directories(gameengine_scripting_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
        )
        gameengine_configure_target(gameengine_scripting_tests)
        add_test(NAME gameengine_scripting COMMAND gameengine_scripting_tests)
        add_test(NAME gameengine_runtime_lua_smoke COMMAND gameengine_runtime --lua-smoke-test)
    endif()

    add_executable(gameengine_render_graph_tests
        tests/render_graph_tests.cpp
    )
    target_link_libraries(gameengine_render_graph_tests PRIVATE gameengine_renderer gameengine_core)
    target_include_directories(gameengine_render_graph_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    gameengine_configure_target(gameengine_render_graph_tests)
    add_test(NAME gameengine_render_graph COMMAND gameengine_render_graph_tests)

    add_executable(gameengine_advanced_graphics_tests
        tests/advanced_graphics_tests.cpp
    )
    target_link_libraries(gameengine_advanced_graphics_tests PRIVATE gameengine_renderer gameengine_core)
    target_include_directories(gameengine_advanced_graphics_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    gameengine_configure_target(gameengine_advanced_graphics_tests)
    add_test(NAME gameengine_advanced_graphics COMMAND gameengine_advanced_graphics_tests)

    add_executable(gameengine_renderer_metrics_tests
        tests/renderer_metrics_tests.cpp
    )
    target_link_libraries(gameengine_renderer_metrics_tests PRIVATE gameengine_renderer gameengine_core)
    target_include_directories(gameengine_renderer_metrics_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    gameengine_configure_target(gameengine_renderer_metrics_tests)
    add_test(NAME gameengine_renderer_metrics COMMAND gameengine_renderer_metrics_tests)

    add_executable(gameengine_renderer_instances_tests
        tests/renderer_instances_tests.cpp
    )
    target_link_libraries(gameengine_renderer_instances_tests PRIVATE
        gameengine_renderer
        gameengine_core
    )
    target_include_directories(gameengine_renderer_instances_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    gameengine_configure_target(gameengine_renderer_instances_tests)
    add_test(NAME gameengine_renderer_instances COMMAND gameengine_renderer_instances_tests)

    add_executable(gameengine_renderer_gpu_culling_tests
        tests/renderer_gpu_culling_tests.cpp
    )
    target_link_libraries(gameengine_renderer_gpu_culling_tests PRIVATE
        gameengine_renderer
        gameengine_core
    )
    target_include_directories(gameengine_renderer_gpu_culling_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    gameengine_configure_target(gameengine_renderer_gpu_culling_tests)
    add_test(NAME gameengine_renderer_gpu_culling COMMAND gameengine_renderer_gpu_culling_tests)

    add_executable(gameengine_renderer_benchmark_tests
        tests/renderer_benchmark_tests.cpp
    )
    target_link_libraries(gameengine_renderer_benchmark_tests PRIVATE
        gameengine_renderer
        gameengine_core
    )
    target_include_directories(gameengine_renderer_benchmark_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    gameengine_configure_target(gameengine_renderer_benchmark_tests)
    add_test(NAME gameengine_renderer_benchmark COMMAND gameengine_renderer_benchmark_tests)

    add_executable(gameengine_renderer_quality_tests
        tests/renderer_quality_tests.cpp
    )
    target_link_libraries(gameengine_renderer_quality_tests PRIVATE
        gameengine_renderer
        gameengine_core
    )
    target_include_directories(gameengine_renderer_quality_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    gameengine_configure_target(gameengine_renderer_quality_tests)
    add_test(NAME gameengine_renderer_quality COMMAND gameengine_renderer_quality_tests)

    add_executable(gameengine_forward_plus_tests
        tests/forward_plus_tests.cpp
    )
    target_link_libraries(gameengine_forward_plus_tests PRIVATE
        gameengine_renderer
        gameengine_core
    )
    target_include_directories(gameengine_forward_plus_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    gameengine_configure_target(gameengine_forward_plus_tests)
    add_test(NAME gameengine_forward_plus COMMAND gameengine_forward_plus_tests)

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_program(GAMEENGINE_XVFB_RUN xvfb-run)

        add_executable(gameengine_platform_tests
            tests/platform_tests.cpp
        )
        target_link_libraries(gameengine_platform_tests PRIVATE gameengine_core gameengine_platform)
        gameengine_configure_target(gameengine_platform_tests)

        if(GAMEENGINE_BUILD_VULKAN)
            add_executable(gameengine_vulkan_tests
                tests/vulkan_tests.cpp
            )
            target_link_libraries(gameengine_vulkan_tests PRIVATE
                gameengine_core
                gameengine_platform
                gameengine_renderer
            )
            target_include_directories(gameengine_vulkan_tests PRIVATE ${Vulkan_INCLUDE_DIR})
            gameengine_configure_target(gameengine_vulkan_tests)

            add_executable(gameengine_shader_pipeline_tests
                tests/shader_pipeline_tests.cpp
            )
            target_link_libraries(gameengine_shader_pipeline_tests PRIVATE
                gameengine_core
                gameengine_renderer
            )
            target_include_directories(gameengine_shader_pipeline_tests PRIVATE ${Vulkan_INCLUDE_DIR})
            gameengine_configure_target(gameengine_shader_pipeline_tests)
            add_test(NAME gameengine_shader_pipeline COMMAND gameengine_shader_pipeline_tests)
        endif()

        if(GAMEENGINE_XVFB_RUN)
            add_test(
                NAME gameengine_runtime_smoke
                COMMAND ${GAMEENGINE_XVFB_RUN} --auto-servernum $<TARGET_FILE:gameengine_runtime> --smoke-test
            )
            add_test(
                NAME gameengine_runtime_metrics
                COMMAND ${GAMEENGINE_XVFB_RUN} --auto-servernum $<TARGET_FILE:gameengine_runtime> --metrics
            )
            add_test(
                NAME gameengine_runtime_gpu_metrics
                COMMAND ${GAMEENGINE_XVFB_RUN} --auto-servernum $<TARGET_FILE:gameengine_runtime> --metrics --gpu-culling
            )
            add_test(
                NAME gameengine_runtime_benchmark
                COMMAND ${GAMEENGINE_XVFB_RUN} --auto-servernum $<TARGET_FILE:gameengine_runtime> --renderer-benchmark
            )
            add_test(
                NAME gameengine_runtime_quality_low
                COMMAND ${GAMEENGINE_XVFB_RUN} --auto-servernum $<TARGET_FILE:gameengine_runtime> --smoke-test --renderer-quality low
            )
            add_test(
                NAME gameengine_runtime_quality_medium
                COMMAND ${GAMEENGINE_XVFB_RUN} --auto-servernum $<TARGET_FILE:gameengine_runtime> --smoke-test --renderer-quality medium
            )
            add_test(
                NAME gameengine_runtime_quality_high
                COMMAND ${GAMEENGINE_XVFB_RUN} --auto-servernum $<TARGET_FILE:gameengine_runtime> --smoke-test --renderer-quality high
            )
            if(GAMEENGINE_BUILD_PHYSICS)
                add_test(
                    NAME gameengine_runtime_physics_demo
                    COMMAND ${GAMEENGINE_XVFB_RUN} --auto-servernum $<TARGET_FILE:gameengine_runtime> --smoke-test --physics-demo
                )
            endif()
            if(GAMEENGINE_BUILD_LUA)
                add_test(
                    NAME gameengine_runtime_lua_demo
                    COMMAND ${GAMEENGINE_XVFB_RUN} --auto-servernum $<TARGET_FILE:gameengine_runtime> --smoke-test --lua-demo
                )
            endif()
            add_test(
                NAME gameengine_platform_x11
                COMMAND ${GAMEENGINE_XVFB_RUN} --auto-servernum $<TARGET_FILE:gameengine_platform_tests>
            )
            if(GAMEENGINE_BUILD_VULKAN)
                add_test(
                    NAME gameengine_vulkan_resize
                    COMMAND ${GAMEENGINE_XVFB_RUN} --auto-servernum $<TARGET_FILE:gameengine_vulkan_tests>
                )
            endif()
        else()
            add_test(NAME gameengine_runtime_smoke COMMAND gameengine_runtime --smoke-test)
            add_test(NAME gameengine_runtime_metrics COMMAND gameengine_runtime --metrics)
            add_test(NAME gameengine_runtime_gpu_metrics COMMAND gameengine_runtime --metrics --gpu-culling)
            add_test(NAME gameengine_runtime_benchmark COMMAND gameengine_runtime --renderer-benchmark)
            add_test(NAME gameengine_runtime_quality_low COMMAND gameengine_runtime --smoke-test --renderer-quality low)
            add_test(NAME gameengine_runtime_quality_medium COMMAND gameengine_runtime --smoke-test --renderer-quality medium)
            add_test(NAME gameengine_runtime_quality_high COMMAND gameengine_runtime --smoke-test --renderer-quality high)
            if(GAMEENGINE_BUILD_PHYSICS)
                add_test(NAME gameengine_runtime_physics_demo COMMAND gameengine_runtime --smoke-test --physics-demo)
            endif()
            if(GAMEENGINE_BUILD_LUA)
                add_test(NAME gameengine_runtime_lua_demo COMMAND gameengine_runtime --smoke-test --lua-demo)
            endif()
            add_test(NAME gameengine_platform_x11 COMMAND gameengine_platform_tests)
            if(GAMEENGINE_BUILD_VULKAN)
                add_test(NAME gameengine_vulkan_resize COMMAND gameengine_vulkan_tests)
            endif()
        endif()
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        add_executable(gameengine_platform_tests
            tests/platform_tests.cpp
        )
        target_link_libraries(gameengine_platform_tests PRIVATE gameengine_core gameengine_platform)
        gameengine_configure_target(gameengine_platform_tests)

        add_executable(gameengine_win32_tests
            tests/win32_tests.cpp
        )
        target_link_libraries(gameengine_win32_tests PRIVATE gameengine_core gameengine_platform)
        gameengine_configure_target(gameengine_win32_tests)

        if(GAMEENGINE_BUILD_VULKAN)
            add_executable(gameengine_vulkan_tests
                tests/vulkan_tests.cpp
            )
            target_link_libraries(gameengine_vulkan_tests PRIVATE
                gameengine_core
                gameengine_platform
                gameengine_renderer
            )
            target_include_directories(gameengine_vulkan_tests PRIVATE ${Vulkan_INCLUDE_DIR})
            gameengine_configure_target(gameengine_vulkan_tests)

            add_executable(gameengine_shader_pipeline_tests
                tests/shader_pipeline_tests.cpp
            )
            target_link_libraries(gameengine_shader_pipeline_tests PRIVATE
                gameengine_core
                gameengine_renderer
            )
            target_include_directories(gameengine_shader_pipeline_tests PRIVATE ${Vulkan_INCLUDE_DIR})
            gameengine_configure_target(gameengine_shader_pipeline_tests)
        endif()

        add_test(NAME gameengine_platform_win32 COMMAND gameengine_platform_tests)
        add_test(NAME gameengine_win32_unit COMMAND gameengine_win32_tests)
        if(GAMEENGINE_BUILD_VULKAN)
            add_test(NAME gameengine_vulkan_resize COMMAND gameengine_vulkan_tests)
            add_test(NAME gameengine_shader_pipeline COMMAND gameengine_shader_pipeline_tests)
        endif()
        add_test(NAME gameengine_runtime_smoke COMMAND gameengine_runtime --smoke-test)
        add_test(NAME gameengine_runtime_metrics COMMAND gameengine_runtime --metrics)
        add_test(NAME gameengine_runtime_gpu_metrics COMMAND gameengine_runtime --metrics --gpu-culling)
        add_test(NAME gameengine_runtime_benchmark COMMAND gameengine_runtime --renderer-benchmark)
        add_test(NAME gameengine_runtime_quality_low COMMAND gameengine_runtime --smoke-test --renderer-quality low)
        add_test(NAME gameengine_runtime_quality_medium COMMAND gameengine_runtime --smoke-test --renderer-quality medium)
        add_test(NAME gameengine_runtime_quality_high COMMAND gameengine_runtime --smoke-test --renderer-quality high)
        if(GAMEENGINE_BUILD_PHYSICS)
            add_test(NAME gameengine_runtime_physics_demo COMMAND gameengine_runtime --smoke-test --physics-demo)
        endif()
        if(GAMEENGINE_BUILD_LUA)
            add_test(NAME gameengine_runtime_lua_demo COMMAND gameengine_runtime --smoke-test --lua-demo)
        endif()
    else()
        add_test(NAME gameengine_runtime_smoke COMMAND gameengine_runtime --smoke-test)
    endif()

    set_tests_properties(gameengine_core PROPERTIES LABELS "unit")
    set_tests_properties(gameengine_assets PROPERTIES LABELS "unit")
    set_tests_properties(gameengine_prepared_assets PROPERTIES LABELS "unit")
    set_tests_properties(gameengine_rhi PROPERTIES LABELS "rhi")
    set_tests_properties(gameengine_shaders PROPERTIES LABELS "shader")
    set_tests_properties(gameengine_math PROPERTIES LABELS "unit")
    set_tests_properties(gameengine_scene PROPERTIES LABELS "unit")
    set_tests_properties(gameengine_render_graph PROPERTIES LABELS "unit")
    set_tests_properties(gameengine_renderer_metrics PROPERTIES LABELS "unit")
    set_tests_properties(gameengine_renderer_instances PROPERTIES LABELS "unit")
    set_tests_properties(gameengine_renderer_gpu_culling PROPERTIES LABELS "unit")
    set_tests_properties(gameengine_renderer_benchmark PROPERTIES LABELS "unit")
    set_tests_properties(gameengine_renderer_quality PROPERTIES LABELS "unit")
    set_tests_properties(gameengine_forward_plus PROPERTIES LABELS "unit")
    if(TEST gameengine_shader_pipeline)
        set_tests_properties(gameengine_shader_pipeline PROPERTIES LABELS "shader")
    endif()
    set_tests_properties(gameengine_runtime_smoke PROPERTIES LABELS "smoke")
    set_tests_properties(gameengine_runtime_metrics PROPERTIES LABELS "smoke")
    set_tests_properties(gameengine_runtime_gpu_metrics PROPERTIES LABELS "smoke")
    set_tests_properties(gameengine_runtime_benchmark PROPERTIES LABELS "smoke")
    set_tests_properties(gameengine_runtime_quality_low PROPERTIES LABELS "smoke")
    set_tests_properties(gameengine_runtime_quality_medium PROPERTIES LABELS "smoke")
    set_tests_properties(gameengine_runtime_quality_high PROPERTIES LABELS "smoke")
    if(TEST gameengine_platform_win32)
        set_tests_properties(gameengine_platform_win32 PROPERTIES LABELS "platform")
    endif()
    if(TEST gameengine_win32_unit)
        set_tests_properties(gameengine_win32_unit PROPERTIES LABELS "unit")
    endif()
    if(TEST gameengine_vulkan_resize)
        set_tests_properties(gameengine_vulkan_resize PROPERTIES LABELS "vulkan")
    endif()
    if(TEST gameengine_physics)
        set_tests_properties(gameengine_physics PROPERTIES LABELS "unit")
    endif()
    if(TEST gameengine_runtime_physics_demo)
        set_tests_properties(gameengine_runtime_physics_demo PROPERTIES LABELS "smoke")
    endif()
    if(TEST gameengine_scripting)
        set_tests_properties(gameengine_scripting PROPERTIES LABELS "unit")
    endif()
    if(TEST gameengine_runtime_lua_smoke)
        set_tests_properties(gameengine_runtime_lua_smoke PROPERTIES LABELS "smoke")
    endif()
    if(TEST gameengine_runtime_lua_demo)
        set_tests_properties(gameengine_runtime_lua_demo PROPERTIES LABELS "smoke")
    endif()
endif()
