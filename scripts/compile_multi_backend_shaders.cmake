# Companion offline shader compiler for DXIL (D3D12) and Metal targets.
# Included by scripts/compile_bootstrap_shaders.cmake.

set(_d3d12_header "${_repository_dir}/src/engine/renderer/d3d12/d3d12_shaders.hpp")
set(_metal_header "${_repository_dir}/src/engine/renderer/metal/metal_shaders.hpp")
get_filename_component(_d3d12_header_dir "${_d3d12_header}" DIRECTORY)
get_filename_component(_metal_header_dir "${_metal_header}" DIRECTORY)
file(MAKE_DIRECTORY "${_d3d12_header_dir}")
file(MAKE_DIRECTORY "${_metal_header_dir}")

function(_validate_dxil_outputs _stage _entry _output _reflection)
    if(NOT EXISTS "${_output}")
        message(FATAL_ERROR "Slang did not produce the ${_stage} DXIL artifact: ${_output}")
    endif()
    if(NOT EXISTS "${_reflection}")
        message(FATAL_ERROR "Slang did not produce the ${_stage} DXIL reflection: ${_reflection}")
    endif()
    file(SIZE "${_output}" _output_size)
    if(_output_size LESS 32)
        message(FATAL_ERROR "DXIL artifact is too small: ${_output}")
    endif()
    # DXIL containers start with 'DXBC' (0x44584243)
    file(READ "${_output}" _dxil_magic HEX LIMIT 4)
    if(NOT _dxil_magic STREQUAL "44584243")
        message(FATAL_ERROR "DXIL artifact has invalid DXBC magic '${_dxil_magic}': ${_output}")
    endif()
    file(READ "${_reflection}" _reflection_content)
    string(FIND "${_reflection_content}" "\"name\": \"${_entry}\"" _entry_position)
    if(_entry_position EQUAL -1)
        message(FATAL_ERROR "DXIL reflection missing entry point '${_entry}': ${_reflection}")
    endif()
endfunction()

function(_validate_metal_outputs _stage _entry _output _reflection)
    if(NOT EXISTS "${_output}")
        message(FATAL_ERROR "Slang did not produce the ${_stage} Metal artifact: ${_output}")
    endif()
    if(NOT EXISTS "${_reflection}")
        message(FATAL_ERROR "Slang did not produce the ${_stage} Metal reflection: ${_reflection}")
    endif()
    file(SIZE "${_output}" _output_size)
    if(_output_size EQUAL 0)
        message(FATAL_ERROR "Metal artifact is empty: ${_output}")
    endif()
    file(READ "${_output}" _metal_source)
    string(FIND "${_metal_source}" "${_entry}" _metal_entry_pos)
    if(_metal_entry_pos EQUAL -1)
        message(FATAL_ERROR "Metal source missing entry point '${_entry}': ${_output}")
    endif()
    file(READ "${_reflection}" _reflection_content)
    string(FIND "${_reflection_content}" "\"name\": \"${_entry}\"" _entry_position)
    if(_entry_position EQUAL -1)
        message(FATAL_ERROR "Metal reflection missing entry point '${_entry}': ${_reflection}")
    endif()
endfunction()

function(_compile_target_shader _target _profile _ext _label _source _source_hash _stage _entry _output_result)
    set(_manifest
        "schema=2\nslang_version=${_slang_version}\ntarget=${_target}\nprofile=${_profile}\n"
        "configuration=${_configuration}\nsource_sha256=${_source_hash}\nstage=${_stage}\nentry=${_entry}\n")
    string(SHA256 _id "${_manifest}")
    set(_artifact_dir "${_cache_dir}/${_configuration}/${_target}_${_id}")
    set(_cached_output "${_artifact_dir}/shader.${_ext}")
    set(_cached_reflection "${_artifact_dir}/reflection.json")
    set(_cached_manifest "${_artifact_dir}/manifest.txt")
    file(MAKE_DIRECTORY "${_artifact_dir}")

    set(_cache_hit FALSE)
    if(EXISTS "${_cached_output}" AND EXISTS "${_cached_reflection}" AND EXISTS "${_cached_manifest}")
        file(READ "${_cached_manifest}" _stored_manifest)
        if(_stored_manifest STREQUAL _manifest)
            set(_cache_hit TRUE)
        endif()
    endif()

    if(NOT _cache_hit)
        message(STATUS "Compiling ${_target} shader: ${_stage} ${_entry} (${_id})")
        set(_temporary_output "${_cached_output}.tmp")
        set(_temporary_reflection "${_cached_reflection}.tmp")
        file(REMOVE "${_temporary_output}" "${_temporary_reflection}")
        if(_configuration STREQUAL "Debug")
            set(_optimization_args -O0)
        else()
            set(_optimization_args -O2)
        endif()
        set(_profile_args)
        if(NOT _profile STREQUAL "")
            set(_profile_args -profile "${_profile}")
        endif()
        execute_process(
            COMMAND "${_slangc}"
                    "${_source}"
                    -target "${_target}"
                    ${_profile_args}
                    -stage "${_stage}"
                    -entry "${_entry}"
                    -line-directive-mode none
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
                "Slang ${_target} ${_stage} compilation failed (${_compile_result}):\n${_compile_output}\n${_compile_error}")
        endif()
        if(_target STREQUAL "dxil")
            _validate_dxil_outputs("${_stage}" "${_entry}" "${_temporary_output}" "${_temporary_reflection}")
        else()
            _validate_metal_outputs("${_stage}" "${_entry}" "${_temporary_output}" "${_temporary_reflection}")
        endif()
        file(RENAME "${_temporary_output}" "${_cached_output}")
        file(RENAME "${_temporary_reflection}" "${_cached_reflection}")
        file(WRITE "${_cached_manifest}" "${_manifest}")
    endif()

    if(_target STREQUAL "dxil")
        _validate_dxil_outputs("${_stage}" "${_entry}" "${_cached_output}" "${_cached_reflection}")
    else()
        _validate_metal_outputs("${_stage}" "${_entry}" "${_cached_output}" "${_cached_reflection}")
    endif()
    configure_file("${_cached_output}" "${_reflection_dir}/${_label}.${_stage}.${_ext}" COPYONLY)
    configure_file("${_cached_reflection}" "${_reflection_dir}/${_label}.${_stage}.${_target}.reflection.json" COPYONLY)
    set(${_output_result} "${_cached_output}" PARENT_SCOPE)
endfunction()

function(_bytes_to_u8_array _input _symbol _result)
    file(READ "${_input}" _hex_content HEX)
    string(LENGTH "${_hex_content}" _hex_length)
    math(EXPR _byte_count "${_hex_length} / 2")
    string(REGEX REPLACE "([0-9a-fA-F][0-9a-fA-F])" "    0x\\1,\n" _array_body "${_hex_content}")
    set(${_result}
        "inline constexpr std::array<std::uint8_t, ${_byte_count}> ${_symbol} = {\n${_array_body}};\n"
        PARENT_SCOPE)
endfunction()

# Compile DXIL shaders (D3D12)
_compile_target_shader(dxil sm_6_0 dxil "triangle" "${_shader_source}" "${_source_sha256}" vertex vertex_main _dxil_tri_vs)
_compile_target_shader(dxil sm_6_0 dxil "triangle" "${_shader_source}" "${_source_sha256}" fragment fragment_main _dxil_tri_fs)
_compile_target_shader(dxil sm_6_0 dxil "editor_ui" "${_editor_ui_shader_source}" "${_editor_ui_source_sha256}" vertex editor_ui_vertex_main _dxil_ui_vs)
_compile_target_shader(dxil sm_6_0 dxil "editor_ui" "${_editor_ui_shader_source}" "${_editor_ui_source_sha256}" fragment editor_ui_fragment_main _dxil_ui_fs)
_compile_target_shader(dxil sm_6_0 dxil "cull" "${_compute_shader_source}" "${_compute_source_sha256}" compute compute_main _dxil_cull_cs)
_compile_target_shader(dxil sm_6_0 dxil "shadow" "${_shadow_shader_source}" "${_shadow_source_sha256}" vertex shadow_vertex_main _dxil_shadow_vs)

_bytes_to_u8_array("${_dxil_tri_vs}" "triangle_vertex_dxil" _dxil_tri_vs_arr)
_bytes_to_u8_array("${_dxil_tri_fs}" "triangle_fragment_dxil" _dxil_tri_fs_arr)
_bytes_to_u8_array("${_dxil_ui_vs}" "editor_ui_vertex_dxil" _dxil_ui_vs_arr)
_bytes_to_u8_array("${_dxil_ui_fs}" "editor_ui_fragment_dxil" _dxil_ui_fs_arr)
_bytes_to_u8_array("${_dxil_cull_cs}" "cull_compute_dxil" _dxil_cull_cs_arr)
_bytes_to_u8_array("${_dxil_shadow_vs}" "shadow_vertex_dxil" _dxil_shadow_vs_arr)

file(WRITE "${_d3d12_header}"
    "#pragma once\n\n"
    "#include <array>\n"
    "#include <cstdint>\n\n"
    "namespace gameengine::renderer::d3d12::bootstrap {\n\n"
    "// Generated by scripts/compile_multi_backend_shaders.cmake.\n"
    "// Do not edit this file manually.\n"
    "${_dxil_tri_vs_arr}${_dxil_tri_fs_arr}"
    "${_dxil_ui_vs_arr}${_dxil_ui_fs_arr}"
    "${_dxil_cull_cs_arr}${_dxil_shadow_vs_arr}"
    "\n} // namespace gameengine::renderer::d3d12::bootstrap\n")

# Compile Metal shaders
_compile_target_shader(metal "" metal "triangle" "${_shader_source}" "${_source_sha256}" vertex vertex_main _metal_tri_vs)
_compile_target_shader(metal "" metal "triangle" "${_shader_source}" "${_source_sha256}" fragment fragment_main _metal_tri_fs)
_compile_target_shader(metal "" metal "editor_ui" "${_editor_ui_shader_source}" "${_editor_ui_source_sha256}" vertex editor_ui_vertex_main _metal_ui_vs)
_compile_target_shader(metal "" metal "editor_ui" "${_editor_ui_shader_source}" "${_editor_ui_source_sha256}" fragment editor_ui_fragment_main _metal_ui_fs)
_compile_target_shader(metal "" metal "shadow" "${_shadow_shader_source}" "${_shadow_source_sha256}" vertex shadow_vertex_main _metal_shadow_vs)

file(READ "${_metal_tri_vs}" _msl_tri_vs)
file(READ "${_metal_tri_fs}" _msl_tri_fs)
file(READ "${_metal_ui_vs}" _msl_ui_vs)
file(READ "${_metal_ui_fs}" _msl_ui_fs)
file(READ "${_metal_shadow_vs}" _msl_shadow_vs)

file(WRITE "${_metal_header}"
    "#pragma once\n\n"
    "#include <string_view>\n\n"
    "namespace gameengine::renderer::metal::bootstrap {\n\n"
    "// Generated by scripts/compile_multi_backend_shaders.cmake.\n"
    "// Do not edit this file manually.\n"
    "inline constexpr std::string_view triangle_vertex_msl = R\"GE_MSL(${_msl_tri_vs})GE_MSL\";\n\n"
    "inline constexpr std::string_view triangle_fragment_msl = R\"GE_MSL(${_msl_tri_fs})GE_MSL\";\n\n"
    "inline constexpr std::string_view editor_ui_vertex_msl = R\"GE_MSL(${_msl_ui_vs})GE_MSL\";\n\n"
    "inline constexpr std::string_view editor_ui_fragment_msl = R\"GE_MSL(${_msl_ui_fs})GE_MSL\";\n\n"
    "inline constexpr std::string_view shadow_vertex_msl = R\"GE_MSL(${_msl_shadow_vs})GE_MSL\";\n\n"
    "} // namespace gameengine::renderer::metal::bootstrap\n")

message(STATUS "Generated ${_d3d12_header}")
message(STATUS "Generated ${_metal_header}")
