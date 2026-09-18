cmake_minimum_required(VERSION 3.25)

set(_repository_dir "${GAMEENGINE_SOURCE_DIR}")
if(NOT _repository_dir)
    get_filename_component(_repository_dir "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

set(_expected_slang_version "2026.13.1-1-g84792eb15")
set(_shader_source "${_repository_dir}/assets/shaders/bootstrap/triangle.slang")
set(_default_header "${_repository_dir}/src/engine/renderer/vulkan/triangle_shaders.hpp")
set(_default_reflection_dir "${_repository_dir}/build/shader-reflection")

if(GAMEENGINE_SHADER_HEADER)
    set(_shader_header "${GAMEENGINE_SHADER_HEADER}")
else()
    set(_shader_header "${_default_header}")
endif()

if(GAMEENGINE_SHADER_REFLECTION_DIR)
    set(_reflection_dir "${GAMEENGINE_SHADER_REFLECTION_DIR}")
else()
    set(_reflection_dir "${_default_reflection_dir}")
endif()

if(GAMEENGINE_SLANGC)
    set(_slangc "${GAMEENGINE_SLANGC}")
elseif(DEFINED ENV{GAMEENGINE_SLANGC} AND NOT "$ENV{GAMEENGINE_SLANGC}" STREQUAL "")
    set(_slangc "$ENV{GAMEENGINE_SLANGC}")
else()
    find_program(_slangc NAMES slangc REQUIRED)
endif()

if(NOT EXISTS "${_slangc}")
    message(FATAL_ERROR
        "GAMEENGINE_SLANGC does not point to an existing slangc executable: ${_slangc}")
endif()
if(NOT EXISTS "${_shader_source}")
    message(FATAL_ERROR "Shader source does not exist: ${_shader_source}")
endif()

execute_process(
    COMMAND "${_slangc}" -version
    RESULT_VARIABLE _version_result
    OUTPUT_VARIABLE _slang_version
    ERROR_VARIABLE _version_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
)
if(NOT _version_result EQUAL 0)
    message(FATAL_ERROR
        "Unable to query slangc version. Exit code: ${_version_result}\n${_version_error}")
endif()
if(_slang_version STREQUAL "")
    set(_slang_version "${_version_error}")
endif()
if(NOT _slang_version STREQUAL _expected_slang_version)
    message(FATAL_ERROR
        "Unsupported slangc version '${_slang_version}'. "
        "Expected '${_expected_slang_version}'.")
endif()

file(MAKE_DIRECTORY "${_reflection_dir}")
get_filename_component(_header_directory "${_shader_header}" DIRECTORY)
file(MAKE_DIRECTORY "${_header_directory}")

set(_vertex_spirv "${_reflection_dir}/triangle.vertex.spv")
set(_fragment_spirv "${_reflection_dir}/triangle.fragment.spv")
set(_vertex_reflection "${_reflection_dir}/triangle.vertex.reflection.json")
set(_fragment_reflection "${_reflection_dir}/triangle.fragment.reflection.json")

function(_compile_shader _stage _entry _output _reflection)
    execute_process(
        COMMAND "${_slangc}"
                "${_shader_source}"
                -target spirv
                -profile spirv_1_0
                -stage "${_stage}"
                -entry "${_entry}"
                -fvk-use-entrypoint-name
                -fspv-reflect
                -emit-spirv-via-glsl
                -warnings-as-errors all
                -o "${_output}"
                -reflection-json "${_reflection}"
        RESULT_VARIABLE _compile_result
        OUTPUT_VARIABLE _compile_output
        ERROR_VARIABLE _compile_error
    )
    if(NOT _compile_result EQUAL 0)
        message(FATAL_ERROR
            "Slang ${_stage} shader compilation failed with exit code ${_compile_result}.\n"
            "${_compile_output}\n${_compile_error}")
    endif()
    if(NOT EXISTS "${_output}")
        message(FATAL_ERROR "Slang did not produce the SPIR-V artifact: ${_output}")
    endif()
    if(NOT EXISTS "${_reflection}")
        message(FATAL_ERROR "Slang did not produce the reflection artifact: ${_reflection}")
    endif()
    file(SIZE "${_output}" _output_size)
    if(_output_size EQUAL 0 OR NOT _output_size GREATER 3)
        message(FATAL_ERROR "SPIR-V artifact is empty: ${_output}")
    endif()
    math(EXPR _word_remainder "${_output_size} % 4")
    if(NOT _word_remainder EQUAL 0)
        message(FATAL_ERROR "SPIR-V artifact is not aligned to 32-bit words: ${_output}")
    endif()
    file(SIZE "${_reflection}" _reflection_size)
    if(_reflection_size EQUAL 0)
        message(FATAL_ERROR "Reflection artifact is empty: ${_reflection}")
    endif()
    file(READ "${_reflection}" _reflection_content)
    string(FIND "${_reflection_content}" "\"name\": \"${_entry}\"" _entry_position)
    if(_entry_position EQUAL -1)
        message(FATAL_ERROR
            "Reflection artifact does not contain entry point '${_entry}': ${_reflection}")
    endif()
    string(FIND "${_reflection_content}" "\"stage\": \"${_stage}\"" _stage_position)
    if(_stage_position EQUAL -1)
        message(FATAL_ERROR
            "Reflection artifact does not contain stage '${_stage}': ${_reflection}")
    endif()
endfunction()

_compile_shader(vertex vertex_main "${_vertex_spirv}" "${_vertex_reflection}")
_compile_shader(fragment fragment_main "${_fragment_spirv}" "${_fragment_reflection}")

function(_read_spirv_words _path _symbol _result)
    file(READ "${_path}" _hex HEX)
    file(SIZE "${_path}" _size)
    math(EXPR _word_count "${_size} / 4")
    math(EXPR _last_word "${_word_count} - 1")
    set(_content "inline constexpr std::array<std::uint32_t, ${_word_count}> ${_symbol} = {\n")
    foreach(_word_index RANGE 0 ${_last_word})
        math(EXPR _offset "${_word_index} * 8")
        string(SUBSTRING "${_hex}" ${_offset} 8 _little_endian)
        string(SUBSTRING "${_little_endian}" 0 2 _byte0)
        string(SUBSTRING "${_little_endian}" 2 2 _byte1)
        string(SUBSTRING "${_little_endian}" 4 2 _byte2)
        string(SUBSTRING "${_little_endian}" 6 2 _byte3)
        string(APPEND _content "    0x${_byte3}${_byte2}${_byte1}${_byte0},\n")
    endforeach()
    string(APPEND _content "};\n")
    set(${_result} "${_content}" PARENT_SCOPE)
endfunction()

_read_spirv_words("${_vertex_spirv}" vertex_shader _vertex_array)
_read_spirv_words("${_fragment_spirv}" fragment_shader _fragment_array)

file(WRITE "${_shader_header}" "#pragma once\n\n")
file(APPEND "${_shader_header}" "#include <array>\n#include <cstdint>\n\n")
file(APPEND "${_shader_header}"
    "namespace gameengine::renderer::vulkan::bootstrap {\n\n"
    "// Generated by scripts/compile_bootstrap_shaders.cmake.\n"
    "// Do not edit this file manually.\n")
file(APPEND "${_shader_header}" "${_vertex_array}${_fragment_array}\n")
file(APPEND "${_shader_header}" "} // namespace gameengine::renderer::vulkan::bootstrap\n")

message(STATUS "Generated ${_shader_header}")
message(STATUS "Reflection artifacts: ${_reflection_dir}")
