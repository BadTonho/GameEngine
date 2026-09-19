cmake_minimum_required(VERSION 3.25)

set(_repository_dir "${GAMEENGINE_SOURCE_DIR}")
if(NOT _repository_dir)
    get_filename_component(_repository_dir "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

set(_expected_slang_version "2026.13.1-1-g84792eb15")
set(_shader_source "${_repository_dir}/assets/shaders/bootstrap/triangle.slang")
set(_compute_shader_source "${_repository_dir}/assets/shaders/bootstrap/cull.slang")
set(_benchmark_shader_source "${_repository_dir}/assets/shaders/bootstrap/benchmark.slang")
set(_forward_plus_shader_source "${_repository_dir}/assets/shaders/bootstrap/forward_plus.slang")
set(_forward_plus_high_shader_source "${_repository_dir}/assets/shaders/bootstrap/forward_plus_high.slang")
set(_forward_plus_compute_shader_source "${_repository_dir}/assets/shaders/bootstrap/forward_plus_compute.slang")
set(_shadow_shader_source "${_repository_dir}/assets/shaders/bootstrap/shadow.slang")
set(_environment_shader_source "${_repository_dir}/assets/shaders/bootstrap/environment.slang")
set(_editor_ui_shader_source "${_repository_dir}/assets/shaders/bootstrap/editor_ui.slang")
set(_default_header "${_repository_dir}/src/engine/renderer/vulkan/triangle_shaders.hpp")
set(_default_reflection_dir "${_repository_dir}/build/shader-reflection")
set(_default_cache_dir "${_repository_dir}/build/shader-cache")

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

if(GAMEENGINE_SHADER_CACHE_DIR)
    set(_cache_dir "${GAMEENGINE_SHADER_CACHE_DIR}")
else()
    set(_cache_dir "${_default_cache_dir}")
endif()

if(GAMEENGINE_SHADER_CONFIGURATION)
    set(_configuration "${GAMEENGINE_SHADER_CONFIGURATION}")
else()
    set(_configuration "Release")
endif()
if(NOT _configuration STREQUAL "Debug" AND NOT _configuration STREQUAL "Release")
    message(FATAL_ERROR
        "GAMEENGINE_SHADER_CONFIGURATION must be Debug or Release, got '${_configuration}'.")
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
if(NOT EXISTS "${_compute_shader_source}")
    message(FATAL_ERROR "Compute shader source does not exist: ${_compute_shader_source}")
endif()
if(NOT EXISTS "${_benchmark_shader_source}")
    message(FATAL_ERROR "Benchmark shader source does not exist: ${_benchmark_shader_source}")
endif()
if(NOT EXISTS "${_forward_plus_shader_source}")
    message(FATAL_ERROR "Forward+ shader source does not exist: ${_forward_plus_shader_source}")
endif()
if(NOT EXISTS "${_forward_plus_high_shader_source}")
    message(FATAL_ERROR "Forward+ High shader source does not exist: ${_forward_plus_high_shader_source}")
endif()
if(NOT EXISTS "${_forward_plus_compute_shader_source}")
    message(FATAL_ERROR "Forward+ compute shader source does not exist: ${_forward_plus_compute_shader_source}")
endif()
if(NOT EXISTS "${_shadow_shader_source}")
    message(FATAL_ERROR "Shadow shader source does not exist: ${_shadow_shader_source}")
endif()
if(NOT EXISTS "${_environment_shader_source}")
    message(FATAL_ERROR "Environment shader source does not exist: ${_environment_shader_source}")
endif()
if(NOT EXISTS "${_editor_ui_shader_source}")
    message(FATAL_ERROR "Editor UI shader source does not exist: ${_editor_ui_shader_source}")
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
file(MAKE_DIRECTORY "${_cache_dir}/${_configuration}")
get_filename_component(_header_directory "${_shader_header}" DIRECTORY)
file(MAKE_DIRECTORY "${_header_directory}")
file(SHA256 "${_shader_source}" _source_sha256)
file(SHA256 "${_compute_shader_source}" _compute_source_sha256)
file(SHA256 "${_benchmark_shader_source}" _benchmark_source_sha256)
file(SHA256 "${_forward_plus_shader_source}" _forward_plus_source_sha256)
file(SHA256 "${_forward_plus_high_shader_source}" _forward_plus_high_source_sha256)
file(SHA256 "${_forward_plus_compute_shader_source}" _forward_plus_compute_source_sha256)
file(SHA256 "${_shadow_shader_source}" _shadow_source_sha256)
file(SHA256 "${_environment_shader_source}" _environment_source_sha256)
file(SHA256 "${_editor_ui_shader_source}" _editor_ui_source_sha256)

set(_base_manifest
    "schema=1\n"
    "slang_version=${_slang_version}\n"
    "target=spirv\n"
    "profile=spirv_1_0+GLSL_450\n"
    "capabilities=vulkan_1_0\n"
    "configuration=${_configuration}\n")

function(_make_shader_identity _source_hash _stage _entry _id_result _manifest_result)
    set(_manifest "${_base_manifest}source_sha256=${_source_hash}\nstage=${_stage}\nentry=${_entry}\n")
    string(SHA256 _id "${_manifest}")
    set(${_id_result} "${_id}" PARENT_SCOPE)
    set(${_manifest_result} "${_manifest}" PARENT_SCOPE)
endfunction()

function(_validate_shader_outputs _stage _entry _output _reflection)
    if(NOT EXISTS "${_output}")
        message(FATAL_ERROR "Slang did not produce the ${_stage} SPIR-V artifact: ${_output}")
    endif()
    if(NOT EXISTS "${_reflection}")
        message(FATAL_ERROR
            "Slang did not produce the ${_stage} reflection artifact: ${_reflection}")
    endif()
    file(SIZE "${_output}" _output_size)
    if(_output_size EQUAL 0 OR NOT _output_size GREATER 3)
        message(FATAL_ERROR "SPIR-V artifact is empty: ${_output}")
    endif()
    file(READ "${_output}" _spirv_magic HEX LIMIT 4)
    if(NOT _spirv_magic STREQUAL "03022307")
        message(FATAL_ERROR "SPIR-V artifact has an invalid magic number: ${_output}")
    endif()
    math(EXPR _word_remainder "${_output_size} % 4")
    if(NOT _word_remainder EQUAL 0)
        message(FATAL_ERROR "SPIR-V artifact is not aligned to 32-bit words: ${_output}")
    endif()
    file(READ "${_reflection}" _reflection_content)
    file(SIZE "${_reflection}" _reflection_size)
    if(_reflection_size EQUAL 0)
        message(FATAL_ERROR "Reflection artifact is empty: ${_reflection}")
    endif()
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
    if(_stage STREQUAL "fragment" AND NOT _entry STREQUAL "editor_ui_fragment_main")
        foreach(_resource_name IN ITEMS material_constants albedo_texture albedo_sampler)
            string(FIND "${_reflection_content}" "\"name\": \"${_resource_name}\"" _resource_position)
            if(_resource_position EQUAL -1)
                message(FATAL_ERROR
                    "Fragment reflection is missing resource '${_resource_name}': ${_reflection}")
            endif()
        endforeach()
    elseif(_stage STREQUAL "vertex" AND NOT _entry STREQUAL "editor_ui_vertex_main")
        foreach(_input_name IN ITEMS model_column0 model_column1 model_column2 model_column3)
            string(FIND "${_reflection_content}" "\"name\": \"${_input_name}\"" _input_position)
            if(_input_position EQUAL -1)
                message(FATAL_ERROR
                    "Vertex reflection is missing instance input '${_input_name}': ${_reflection}")
            endif()
        endforeach()
    elseif(_stage STREQUAL "compute" AND _entry STREQUAL "compute_main")
        foreach(_resource_name IN ITEMS source_instances visible_instances indirect_commands)
            string(FIND "${_reflection_content}" "\"name\": \"${_resource_name}\"" _resource_position)
            if(_resource_position EQUAL -1)
                message(FATAL_ERROR
                    "Compute reflection is missing resource '${_resource_name}': ${_reflection}")
            endif()
        endforeach()
    elseif(_stage STREQUAL "compute" AND _entry STREQUAL "benchmark_compute_main")
        foreach(_resource_name IN ITEMS benchmark_input benchmark_output benchmark_lights)
            string(FIND "${_reflection_content}" "\"name\": \"${_resource_name}\"" _resource_position)
            if(_resource_position EQUAL -1)
                message(FATAL_ERROR
                    "Benchmark reflection is missing resource '${_resource_name}': ${_reflection}")
            endif()
        endforeach()
    elseif(_stage STREQUAL "compute" AND _entry STREQUAL "forward_plus_light_list_main")
        foreach(_resource_name IN ITEMS point_lights tile_headers tile_light_indices)
            string(FIND "${_reflection_content}" "\"name\": \"${_resource_name}\"" _resource_position)
            if(_resource_position EQUAL -1)
                message(FATAL_ERROR
                    "Forward+ compute reflection is missing resource '${_resource_name}': ${_reflection}")
            endif()
        endforeach()
    elseif(_entry STREQUAL "forward_plus_vertex_main")
        foreach(_input_name IN ITEMS model_column0 model_column1 model_column2 model_column3)
            string(FIND "${_reflection_content}" "\"name\": \"${_input_name}\"" _input_position)
            if(_input_position EQUAL -1)
                message(FATAL_ERROR
                    "Forward+ vertex reflection is missing instance input '${_input_name}': ${_reflection}")
            endif()
        endforeach()
    elseif(_entry STREQUAL "forward_plus_fragment_main")
        foreach(_resource_name IN ITEMS point_lights tile_headers tile_light_indices)
            string(FIND "${_reflection_content}" "\"name\": \"${_resource_name}\"" _resource_position)
            if(_resource_position EQUAL -1)
                message(FATAL_ERROR
                    "Forward+ reflection is missing resource '${_resource_name}': ${_reflection}")
            endif()
        endforeach()
    elseif(_entry STREQUAL "forward_plus_high_fragment_main")
        foreach(_resource_name IN ITEMS point_lights tile_headers tile_light_indices shadow_map environment_map)
            string(FIND "${_reflection_content}" "\"name\": \"${_resource_name}\"" _resource_position)
            if(_resource_position EQUAL -1)
                message(FATAL_ERROR
                    "Forward+ High reflection is missing resource '${_resource_name}': ${_reflection}")
            endif()
        endforeach()
    elseif(_entry STREQUAL "forward_plus_high_vertex_main")
        foreach(_input_name IN ITEMS model_column0 model_column1 model_column2 model_column3)
            string(FIND "${_reflection_content}" "\"name\": \"${_input_name}\"" _input_position)
            if(_input_position EQUAL -1)
                message(FATAL_ERROR
                    "Forward+ High vertex reflection is missing instance input '${_input_name}': ${_reflection}")
            endif()
        endforeach()
    elseif(_entry STREQUAL "shadow_vertex_main")
        string(FIND "${_reflection_content}" "\"name\": \"model_column0\"" _input_position)
        if(_input_position EQUAL -1)
            message(FATAL_ERROR "Shadow reflection is missing model input: ${_reflection}")
        endif()
    elseif(_entry STREQUAL "editor_ui_vertex_main")
        foreach(_input_name IN ITEMS position uv color)
            string(FIND "${_reflection_content}" "\"name\": \"${_input_name}\"" _input_position)
            if(_input_position EQUAL -1)
                message(FATAL_ERROR "Editor UI vertex reflection is missing input '${_input_name}': ${_reflection}")
            endif()
        endforeach()
    elseif(_entry STREQUAL "editor_ui_fragment_main")
        string(FIND "${_reflection_content}" "\"name\": \"color\"" _input_position)
        if(_input_position EQUAL -1)
            message(FATAL_ERROR "Editor UI fragment reflection is missing color input: ${_reflection}")
        endif()
    endif()
endfunction()

function(_compile_shader _label _source _stage _entry _id _manifest _output_result _reflection_result)
    set(_artifact_dir "${_cache_dir}/${_configuration}/${_id}")
    set(_cached_output "${_artifact_dir}/shader.spv")
    set(_cached_reflection "${_artifact_dir}/reflection.json")
    set(_cached_manifest "${_artifact_dir}/manifest.txt")
    file(MAKE_DIRECTORY "${_artifact_dir}")

    set(_cache_hit FALSE)
    if(EXISTS "${_cached_output}" AND EXISTS "${_cached_reflection}" AND
       EXISTS "${_cached_manifest}")
        file(READ "${_cached_manifest}" _stored_manifest)
        if(_stored_manifest STREQUAL _manifest)
            set(_cache_hit TRUE)
        endif()
    endif()

    if(_cache_hit)
        message(STATUS "Shader cache hit: ${_stage} ${_entry} (${_id})")
    else()
        message(STATUS "Compiling shader: ${_stage} ${_entry} (${_id})")
        set(_temporary_output "${_cached_output}.tmp")
        set(_temporary_reflection "${_cached_reflection}.tmp")
        file(REMOVE "${_temporary_output}" "${_temporary_reflection}")
        if(_configuration STREQUAL "Debug")
            set(_optimization_args -O0 -g3)
        else()
            set(_optimization_args -O2 -g0)
        endif()
        execute_process(
            COMMAND "${_slangc}"
                    "${_source}"
                    -target spirv
                    -profile spirv_1_0+GLSL_450
                    -stage "${_stage}"
                    -entry "${_entry}"
                    -fvk-use-entrypoint-name
                    -fspv-reflect
                    -emit-spirv-via-glsl
                    -warnings-as-errors all
                    ${_optimization_args}
                    -o "${_temporary_output}"
                    -reflection-json "${_temporary_reflection}"
            RESULT_VARIABLE _compile_result
            OUTPUT_VARIABLE _compile_output
            ERROR_VARIABLE _compile_error
        )
        if(NOT _compile_result EQUAL 0)
            file(REMOVE "${_temporary_output}" "${_temporary_reflection}")
            message(FATAL_ERROR
                "Slang ${_stage} shader compilation failed with exit code ${_compile_result}.\n"
                "${_compile_output}\n${_compile_error}")
        endif()
        _validate_shader_outputs(
            "${_stage}" "${_entry}" "${_temporary_output}" "${_temporary_reflection}")
        file(RENAME "${_temporary_output}" "${_cached_output}")
        file(RENAME "${_temporary_reflection}" "${_cached_reflection}")
        file(WRITE "${_cached_manifest}" "${_manifest}")
    endif()

    _validate_shader_outputs("${_stage}" "${_entry}" "${_cached_output}" "${_cached_reflection}")
    set(_staged_output "${_reflection_dir}/${_label}.${_stage}.spv")
    set(_staged_reflection "${_reflection_dir}/${_label}.${_stage}.reflection.json")
    configure_file("${_cached_output}" "${_staged_output}" COPYONLY)
    configure_file("${_cached_reflection}" "${_staged_reflection}" COPYONLY)
    set(${_output_result} "${_cached_output}" PARENT_SCOPE)
    set(${_reflection_result} "${_cached_reflection}" PARENT_SCOPE)
endfunction()

_make_shader_identity("${_source_sha256}" vertex vertex_main _vertex_id _vertex_manifest)
_make_shader_identity("${_source_sha256}" fragment fragment_main _fragment_id _fragment_manifest)
_make_shader_identity("${_compute_source_sha256}" compute compute_main _compute_id _compute_manifest)
_make_shader_identity("${_benchmark_source_sha256}" compute benchmark_compute_main _benchmark_compute_id _benchmark_compute_manifest)
_make_shader_identity("${_forward_plus_source_sha256}" vertex forward_plus_vertex_main _forward_plus_vertex_id _forward_plus_vertex_manifest)
_make_shader_identity("${_forward_plus_source_sha256}" fragment forward_plus_fragment_main _forward_plus_fragment_id _forward_plus_fragment_manifest)
_make_shader_identity("${_forward_plus_high_source_sha256}" vertex forward_plus_high_vertex_main _forward_plus_high_vertex_id _forward_plus_high_vertex_manifest)
_make_shader_identity("${_forward_plus_high_source_sha256}" fragment forward_plus_high_fragment_main _forward_plus_high_fragment_id _forward_plus_high_fragment_manifest)
_make_shader_identity("${_forward_plus_compute_source_sha256}" compute forward_plus_light_list_main _forward_plus_compute_id _forward_plus_compute_manifest)
_make_shader_identity("${_shadow_source_sha256}" vertex shadow_vertex_main _shadow_vertex_id _shadow_vertex_manifest)
_make_shader_identity("${_environment_source_sha256}" compute environment_compute_main _environment_compute_id _environment_compute_manifest)
_make_shader_identity("${_editor_ui_source_sha256}" vertex editor_ui_vertex_main _editor_ui_vertex_id _editor_ui_vertex_manifest)
_make_shader_identity("${_editor_ui_source_sha256}" fragment editor_ui_fragment_main _editor_ui_fragment_id _editor_ui_fragment_manifest)
_compile_shader("triangle" "${_shader_source}" vertex vertex_main "${_vertex_id}" "${_vertex_manifest}" _vertex_spirv _vertex_reflection)
_compile_shader("triangle" "${_shader_source}" fragment fragment_main "${_fragment_id}" "${_fragment_manifest}" _fragment_spirv _fragment_reflection)
_compile_shader("cull" "${_compute_shader_source}" compute compute_main "${_compute_id}" "${_compute_manifest}" _compute_spirv _compute_reflection)
_compile_shader("benchmark" "${_benchmark_shader_source}" compute benchmark_compute_main "${_benchmark_compute_id}" "${_benchmark_compute_manifest}" _benchmark_compute_spirv _benchmark_compute_reflection)
_compile_shader("forward_plus" "${_forward_plus_shader_source}" vertex forward_plus_vertex_main "${_forward_plus_vertex_id}" "${_forward_plus_vertex_manifest}" _forward_plus_vertex_spirv _forward_plus_vertex_reflection)
_compile_shader("forward_plus" "${_forward_plus_shader_source}" fragment forward_plus_fragment_main "${_forward_plus_fragment_id}" "${_forward_plus_fragment_manifest}" _forward_plus_fragment_spirv _forward_plus_fragment_reflection)
_compile_shader("forward_plus_high" "${_forward_plus_high_shader_source}" vertex forward_plus_high_vertex_main "${_forward_plus_high_vertex_id}" "${_forward_plus_high_vertex_manifest}" _forward_plus_high_vertex_spirv _forward_plus_high_vertex_reflection)
_compile_shader("forward_plus_high" "${_forward_plus_high_shader_source}" fragment forward_plus_high_fragment_main "${_forward_plus_high_fragment_id}" "${_forward_plus_high_fragment_manifest}" _forward_plus_high_fragment_spirv _forward_plus_high_fragment_reflection)
_compile_shader("forward_plus_compute" "${_forward_plus_compute_shader_source}" compute forward_plus_light_list_main "${_forward_plus_compute_id}" "${_forward_plus_compute_manifest}" _forward_plus_compute_spirv _forward_plus_compute_reflection)
_compile_shader("shadow" "${_shadow_shader_source}" vertex shadow_vertex_main "${_shadow_vertex_id}" "${_shadow_vertex_manifest}" _shadow_vertex_spirv _shadow_vertex_reflection)
_compile_shader("environment" "${_environment_shader_source}" compute environment_compute_main "${_environment_compute_id}" "${_environment_compute_manifest}" _environment_compute_spirv _environment_compute_reflection)
_compile_shader("editor_ui" "${_editor_ui_shader_source}" vertex editor_ui_vertex_main "${_editor_ui_vertex_id}" "${_editor_ui_vertex_manifest}" _editor_ui_vertex_spirv _editor_ui_vertex_reflection)
_compile_shader("editor_ui" "${_editor_ui_shader_source}" fragment editor_ui_fragment_main "${_editor_ui_fragment_id}" "${_editor_ui_fragment_manifest}" _editor_ui_fragment_spirv _editor_ui_fragment_reflection)

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
_read_spirv_words("${_compute_spirv}" compute_shader _compute_array)
_read_spirv_words("${_benchmark_compute_spirv}" benchmark_compute_shader _benchmark_compute_array)
_read_spirv_words("${_forward_plus_vertex_spirv}" forward_plus_vertex_shader _forward_plus_vertex_array)
_read_spirv_words("${_forward_plus_fragment_spirv}" forward_plus_fragment_shader _forward_plus_fragment_array)
_read_spirv_words("${_forward_plus_high_vertex_spirv}" forward_plus_high_vertex_shader _forward_plus_high_vertex_array)
_read_spirv_words("${_forward_plus_high_fragment_spirv}" forward_plus_high_fragment_shader _forward_plus_high_fragment_array)
_read_spirv_words("${_forward_plus_compute_spirv}" forward_plus_compute_shader _forward_plus_compute_array)
_read_spirv_words("${_shadow_vertex_spirv}" shadow_vertex_shader _shadow_vertex_array)
_read_spirv_words("${_environment_compute_spirv}" environment_compute_shader _environment_compute_array)
_read_spirv_words("${_editor_ui_vertex_spirv}" editor_ui_vertex_shader _editor_ui_vertex_array)
_read_spirv_words("${_editor_ui_fragment_spirv}" editor_ui_fragment_shader _editor_ui_fragment_array)

file(WRITE "${_shader_header}" "#pragma once\n\n")
file(APPEND "${_shader_header}"
    "#include <array>\n"
    "#include <cstdint>\n"
    "#include <string_view>\n\n"
    "#include \"engine/renderer/vulkan/shader_pipeline.hpp\"\n\n"
    "namespace gameengine::renderer::vulkan::bootstrap {\n\n"
    "// Generated by scripts/compile_bootstrap_shaders.cmake.\n"
    "// Do not edit this file manually.\n"
    "inline constexpr std::string_view shader_configuration = \"${_configuration}\";\n"
    "inline constexpr std::string_view shader_source_sha256 = \"${_source_sha256}\";\n"
    "inline constexpr std::string_view compute_shader_source_sha256 = \"${_compute_source_sha256}\";\n"
    "inline constexpr std::string_view benchmark_compute_shader_source_sha256 = \"${_benchmark_source_sha256}\";\n"
    "inline constexpr std::string_view shader_vertex_layout = \"position3_normal3_uv2+instance_model4\";\n"
    "inline constexpr std::uint32_t shader_push_constant_size = 64U;\n"
    "inline constexpr std::uint32_t forward_plus_push_constant_size = 72U;\n"
    "inline constexpr std::string_view shader_vertex_inputs =\n"
    "    \"location0:position3,location1:normal3,location2:uv2,location3:model_column0,\"\n"
    "    \"location4:model_column1,location5:model_column2,location6:model_column3\";\n"
    "inline constexpr std::string_view shader_resource_layout =\n"
    "    \"set0:uniform_buffer+sampled_image+sampler\";\n"
    "inline constexpr std::string_view compute_shader_resource_layout =\n"
    "    \"set0:storage_buffer+storage_buffer+storage_buffer\";\n"
    "inline constexpr std::uint32_t compute_shader_workgroup_size = 64U;\n")
file(APPEND "${_shader_header}"
    "inline constexpr std::string_view benchmark_compute_resource_layout =\n"
    "    \"set0:storage_buffer+storage_buffer+storage_buffer\";\n"
    "inline constexpr std::uint32_t benchmark_compute_workgroup_size = 64U;\n"
    "inline constexpr std::string_view forward_plus_resource_layout =\n"
    "    \"set0:uniform_buffer+sampled_image+sampler+storage_buffer+storage_buffer+storage_buffer\";\n"
    "inline constexpr std::uint32_t forward_plus_workgroup_size = 16U;\n"
    "inline constexpr std::string_view forward_plus_high_resource_layout =\n"
    "    \"set0:uniform_buffer+sampled_image+sampler+storage_buffer+storage_buffer+storage_buffer+sampled_image+sampled_image\";\n"
    "inline constexpr std::string_view forward_plus_compute_resource_layout =\n"
    "    \"set0:storage_buffer+storage_buffer+storage_buffer\";\n"
    "inline constexpr std::uint32_t forward_plus_compute_workgroup_size = 16U;\n"
    "inline constexpr std::string_view shadow_resource_layout = \"push_constant_only\";\n")
file(APPEND "${_shader_header}"
    "inline constexpr std::string_view editor_ui_vertex_layout = \"position2_uv2_color4\";\n"
    "inline constexpr std::string_view editor_ui_resource_layout = \"none\";\n")
file(APPEND "${_shader_header}"
    "${_vertex_array}${_fragment_array}${_compute_array}${_benchmark_compute_array}"
    "${_forward_plus_vertex_array}${_forward_plus_fragment_array}"
    "${_forward_plus_high_vertex_array}${_forward_plus_high_fragment_array}"
    "${_forward_plus_compute_array}"
    "${_shadow_vertex_array}${_environment_compute_array}"
    "${_editor_ui_vertex_array}${_editor_ui_fragment_array}\n")
file(APPEND "${_shader_header}"
    "inline constexpr std::string_view vertex_shader_id = \"${_vertex_id}\";\n"
    "inline constexpr std::string_view fragment_shader_id = \"${_fragment_id}\";\n"
    "inline constexpr std::string_view compute_shader_id = \"${_compute_id}\";\n"
    "inline constexpr ShaderArtifact vertex_shader_artifact{\n"
    "    vertex_shader_id, ShaderStage::vertex, \"vertex_main\",\n"
    "    shader_capability_vulkan_1_0, 0, vertex_shader.data(), vertex_shader.size()\n"
    "};\n"
    "inline constexpr ShaderArtifact fragment_shader_artifact{\n"
    "    fragment_shader_id, ShaderStage::fragment, \"fragment_main\",\n"
    "    shader_capability_vulkan_1_0, 0, fragment_shader.data(), fragment_shader.size()\n"
    "};\n"
    "inline constexpr std::array<ShaderArtifact, 1> vertex_shader_variants = {\n"
    "    vertex_shader_artifact,\n"
    "};\n"
    "inline constexpr std::array<ShaderArtifact, 1> fragment_shader_variants = {\n"
    "    fragment_shader_artifact,\n"
    "};\n"
    "inline constexpr ShaderArtifact compute_shader_artifact{\n"
    "    compute_shader_id, ShaderStage::compute, \"compute_main\",\n"
    "    shader_capability_vulkan_1_0, 0, compute_shader.data(), compute_shader.size()\n"
    "};\n"
    "inline constexpr std::array<ShaderArtifact, 1> compute_shader_variants = {\n"
    "    compute_shader_artifact,\n"
    "};\n"
    "inline constexpr std::string_view benchmark_compute_shader_id = \"${_benchmark_compute_id}\";\n"
    "inline constexpr ShaderArtifact benchmark_compute_shader_artifact{\n"
    "    benchmark_compute_shader_id, ShaderStage::compute, \"benchmark_compute_main\",\n"
    "    shader_capability_vulkan_1_0, 0, benchmark_compute_shader.data(), benchmark_compute_shader.size()\n"
    "};\n"
    "inline constexpr std::array<ShaderArtifact, 1> benchmark_compute_shader_variants = {\n"
    "    benchmark_compute_shader_artifact,\n"
    "};\n"
    "inline constexpr std::string_view forward_plus_vertex_shader_id = \"${_forward_plus_vertex_id}\";\n"
    "inline constexpr std::string_view forward_plus_fragment_shader_id = \"${_forward_plus_fragment_id}\";\n"
    "inline constexpr ShaderArtifact forward_plus_vertex_shader_artifact{\n"
    "    forward_plus_vertex_shader_id, ShaderStage::vertex, \"forward_plus_vertex_main\",\n"
    "    shader_capability_vulkan_1_0, 0, forward_plus_vertex_shader.data(), forward_plus_vertex_shader.size()\n"
    "};\n"
    "inline constexpr ShaderArtifact forward_plus_fragment_shader_artifact{\n"
    "    forward_plus_fragment_shader_id, ShaderStage::fragment, \"forward_plus_fragment_main\",\n"
    "    shader_capability_vulkan_1_0, 0, forward_plus_fragment_shader.data(), forward_plus_fragment_shader.size()\n"
    "};\n"
    "inline constexpr std::array<ShaderArtifact, 1> forward_plus_vertex_shader_variants = {\n"
    "    forward_plus_vertex_shader_artifact,\n"
    "};\n"
    "inline constexpr std::array<ShaderArtifact, 1> forward_plus_fragment_shader_variants = {\n"
    "    forward_plus_fragment_shader_artifact,\n"
    "};\n"
    "inline constexpr std::string_view forward_plus_high_vertex_shader_id = \"${_forward_plus_high_vertex_id}\";\n"
    "inline constexpr std::string_view forward_plus_high_fragment_shader_id = \"${_forward_plus_high_fragment_id}\";\n"
    "inline constexpr ShaderArtifact forward_plus_high_vertex_shader_artifact{\n"
    "    forward_plus_high_vertex_shader_id, ShaderStage::vertex, \"forward_plus_high_vertex_main\",\n"
    "    shader_capability_vulkan_1_0, 0, forward_plus_high_vertex_shader.data(), forward_plus_high_vertex_shader.size()\n"
    "};\n"
    "inline constexpr ShaderArtifact forward_plus_high_fragment_shader_artifact{\n"
    "    forward_plus_high_fragment_shader_id, ShaderStage::fragment, \"forward_plus_high_fragment_main\",\n"
    "    shader_capability_vulkan_1_0, 0, forward_plus_high_fragment_shader.data(), forward_plus_high_fragment_shader.size()\n"
    "};\n"
    "inline constexpr std::array<ShaderArtifact, 1> forward_plus_high_vertex_shader_variants = {\n"
    "    forward_plus_high_vertex_shader_artifact,\n"
    "};\n"
    "inline constexpr std::array<ShaderArtifact, 1> forward_plus_high_fragment_shader_variants = {\n"
    "    forward_plus_high_fragment_shader_artifact,\n"
    "};\n"
    "inline constexpr std::string_view forward_plus_compute_shader_id = \"${_forward_plus_compute_id}\";\n"
    "inline constexpr ShaderArtifact forward_plus_compute_shader_artifact{\n"
    "    forward_plus_compute_shader_id, ShaderStage::compute, \"forward_plus_light_list_main\",\n"
    "    shader_capability_vulkan_1_0, 0, forward_plus_compute_shader.data(), forward_plus_compute_shader.size()\n"
    "};\n"
    "inline constexpr std::array<ShaderArtifact, 1> forward_plus_compute_shader_variants = {\n"
    "    forward_plus_compute_shader_artifact,\n"
    "};\n"
    "inline constexpr std::string_view shadow_vertex_shader_id = \"${_shadow_vertex_id}\";\n"
    "inline constexpr ShaderArtifact shadow_vertex_shader_artifact{\n"
    "    shadow_vertex_shader_id, ShaderStage::vertex, \"shadow_vertex_main\",\n"
    "    shader_capability_vulkan_1_0, 0, shadow_vertex_shader.data(), shadow_vertex_shader.size()\n"
    "};\n"
    "inline constexpr std::array<ShaderArtifact, 1> shadow_vertex_shader_variants = {\n"
    "    shadow_vertex_shader_artifact,\n"
    "};\n"
    "inline constexpr std::string_view environment_compute_shader_id = \"${_environment_compute_id}\";\n"
    "inline constexpr ShaderArtifact environment_compute_shader_artifact{\n"
    "    environment_compute_shader_id, ShaderStage::compute, \"environment_compute_main\",\n"
    "    shader_capability_vulkan_1_0, 0, environment_compute_shader.data(), environment_compute_shader.size()\n"
    "};\n"
    "inline constexpr std::array<ShaderArtifact, 1> environment_compute_shader_variants = {\n"
    "    environment_compute_shader_artifact,\n"
    "};\n"
    "inline constexpr std::string_view editor_ui_vertex_shader_id = \"${_editor_ui_vertex_id}\";\n"
    "inline constexpr std::string_view editor_ui_fragment_shader_id = \"${_editor_ui_fragment_id}\";\n"
    "inline constexpr ShaderArtifact editor_ui_vertex_shader_artifact{\n"
    "    editor_ui_vertex_shader_id, ShaderStage::vertex, \"editor_ui_vertex_main\",\n"
    "    shader_capability_vulkan_1_0, 0, editor_ui_vertex_shader.data(), editor_ui_vertex_shader.size()\n"
    "};\n"
    "inline constexpr ShaderArtifact editor_ui_fragment_shader_artifact{\n"
    "    editor_ui_fragment_shader_id, ShaderStage::fragment, \"editor_ui_fragment_main\",\n"
    "    shader_capability_vulkan_1_0, 0, editor_ui_fragment_shader.data(), editor_ui_fragment_shader.size()\n"
    "};\n"
    "inline constexpr std::array<ShaderArtifact, 1> editor_ui_vertex_shader_variants = {\n"
    "    editor_ui_vertex_shader_artifact,\n"
    "};\n"
    "inline constexpr std::array<ShaderArtifact, 1> editor_ui_fragment_shader_variants = {\n"
    "    editor_ui_fragment_shader_artifact,\n"
    "};\n\n"
    "} // namespace gameengine::renderer::vulkan::bootstrap\n")

message(STATUS "Generated ${_shader_header}")
message(STATUS "Shader configuration: ${_configuration}")
message(STATUS "Shader cache: ${_cache_dir}/${_configuration}")
message(STATUS "Reflection artifacts: ${_reflection_dir}")
