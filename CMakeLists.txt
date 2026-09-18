cmake_minimum_required(VERSION 3.25)

project(GameEngine VERSION 0.0.1 LANGUAGES CXX)

option(GAMEENGINE_BUILD_TESTS "Build the GameEngine tests" ON)
option(GAMEENGINE_BUILD_VULKAN "Build the Vulkan renderer" ON)

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
)
target_link_libraries(gameengine_renderer PUBLIC gameengine_rhi gameengine_platform)
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

gameengine_configure_target(gameengine_renderer)

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

add_executable(gameengine_runtime
    src/runtime/main.cpp
)

target_link_libraries(gameengine_runtime PRIVATE gameengine_core gameengine_platform gameengine_renderer)
gameengine_configure_target(gameengine_runtime)

if(GAMEENGINE_BUILD_TESTS)
    include(CTest)

    add_executable(gameengine_tests
        tests/core_tests.cpp
    )

    target_link_libraries(gameengine_tests PRIVATE gameengine_core)
    gameengine_configure_target(gameengine_tests)

    add_test(NAME gameengine_core COMMAND gameengine_tests)

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
    else()
        add_test(NAME gameengine_runtime_smoke COMMAND gameengine_runtime --smoke-test)
    endif()

    set_tests_properties(gameengine_core PROPERTIES LABELS "unit")
    set_tests_properties(gameengine_rhi PROPERTIES LABELS "rhi")
    set_tests_properties(gameengine_shaders PROPERTIES LABELS "shader")
    set_tests_properties(gameengine_math PROPERTIES LABELS "unit")
    if(TEST gameengine_shader_pipeline)
        set_tests_properties(gameengine_shader_pipeline PROPERTIES LABELS "shader")
    endif()
    set_tests_properties(gameengine_runtime_smoke PROPERTIES LABELS "smoke")
    if(TEST gameengine_platform_win32)
        set_tests_properties(gameengine_platform_win32 PROPERTIES LABELS "platform")
    endif()
    if(TEST gameengine_win32_unit)
        set_tests_properties(gameengine_win32_unit PROPERTIES LABELS "unit")
    endif()
    if(TEST gameengine_vulkan_resize)
        set_tests_properties(gameengine_vulkan_resize PROPERTIES LABELS "vulkan")
    endif()
endif()
