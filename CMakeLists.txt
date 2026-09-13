cmake_minimum_required(VERSION 3.25)

project(GameEngine VERSION 0.0.1 LANGUAGES CXX)

option(GAMEENGINE_BUILD_TESTS "Build the GameEngine tests" ON)

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
)

target_include_directories(gameengine_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

gameengine_configure_target(gameengine_core)

add_executable(gameengine_runtime
    src/runtime/main.cpp
)

target_link_libraries(gameengine_runtime PRIVATE gameengine_core)
gameengine_configure_target(gameengine_runtime)

if(GAMEENGINE_BUILD_TESTS)
    include(CTest)

    add_executable(gameengine_tests
        tests/core_tests.cpp
    )

    target_link_libraries(gameengine_tests PRIVATE gameengine_core)
    gameengine_configure_target(gameengine_tests)

    add_test(NAME gameengine_core COMMAND gameengine_tests)
    add_test(NAME gameengine_runtime_smoke COMMAND gameengine_runtime)
endif()
