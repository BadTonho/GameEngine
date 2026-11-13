# Companion offline shader compiler for Phase 12 Advanced Graphics features.
# Included by scripts/compile_bootstrap_shaders.cmake.

set(_advanced_header "${_repository_dir}/src/engine/renderer/advanced_graphics/advanced_shaders.hpp")
get_filename_component(_advanced_header_dir "${_advanced_header}" DIRECTORY)
file(MAKE_DIRECTORY "${_advanced_header_dir}")

function(_compile_advanced_spirv _name _macro _source _entry _out_id _out_bytes)
    if(NOT EXISTS "${_source}")
        message(FATAL_ERROR "Advanced shader source does not exist: ${_source}")
    endif()
    file(SHA256 "${_source}" _source_hash)
    set(_manifest
        "schema=1\nslang_version=${_slang_version}\ntarget=spirv\n"
        "configuration=${_configuration}\nsource_sha256=${_source_hash}\nstage=compute\nentry=${_entry}\n")
    string(SHA256 _id "${_manifest}")
    set(_artifact_dir "${_cache_dir}/${_configuration}/adv_spirv_${_id}")
    set(_cached_output "${_artifact_dir}/shader.spv")
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
        message(STATUS "Compiling advanced SPIR-V shader: ${_name} (${_id})")
        set(_tmp_out "${_cached_output}.tmp")
        set(_tmp_ref "${_cached_reflection}.tmp")
        file(REMOVE "${_tmp_out}" "${_tmp_ref}")
        if(_configuration STREQUAL "Debug")
            set(_opt -O0)
        else()
            set(_opt -O2)
        endif()
        execute_process(
            COMMAND "${_slangc}"
                    "${_source}"
                    -target spirv
                    -stage compute
                    -entry "${_entry}"
                    -line-directive-mode none
                    ${_opt}
                    -o "${_tmp_out}"
                    -reflection-json "${_tmp_ref}"
            RESULT_VARIABLE _res
            OUTPUT_VARIABLE _out
            ERROR_VARIABLE _err
        )
        if(NOT _res EQUAL 0)
            file(REMOVE "${_tmp_out}" "${_tmp_ref}")
            message(FATAL_ERROR "Advanced shader SPIR-V failed (${_name}):\n${_out}\n${_err}")
        endif()
        file(READ "${_tmp_out}" _spv_magic HEX LIMIT 4)
        if(NOT _spv_magic STREQUAL "03022307")
            message(FATAL_ERROR "Invalid SPIR-V magic for ${_name}: ${_spv_magic}")
        endif()
        file(RENAME "${_tmp_out}" "${_cached_output}")
        file(RENAME "${_tmp_ref}" "${_cached_reflection}")
        file(WRITE "${_cached_manifest}" "${_manifest}")
    endif()

    file(READ "${_cached_output}" _hex HEX)
    string(LENGTH "${_hex}" _hex_len)
    set(_words "")
    math(EXPR _last "${_hex_len} - 8")
    foreach(_off RANGE 0 ${_last} 8)
        string(SUBSTRING "${_hex}" ${_off} 8 _w)
        string(SUBSTRING "${_w}" 0 2 _b0)
        string(SUBSTRING "${_w}" 2 2 _b1)
        string(SUBSTRING "${_w}" 4 2 _b2)
        string(SUBSTRING "${_w}" 6 2 _b3)
        if(_words STREQUAL "")
            set(_words "    0x${_b3}${_b2}${_b1}${_b0}U")
        else()
            string(APPEND _words ",\n    0x${_b3}${_b2}${_b1}${_b0}U")
        endif()
    endforeach()

    set(${_out_id} "${_id}" PARENT_SCOPE)
    set(${_out_bytes} "${_words}" PARENT_SCOPE)
endfunction()

set(_adv_specs
    "taa|GAMEENGINE_HAS_TAA|taa_resolve.slang|taa_resolve_main"
    "upscaling|GAMEENGINE_HAS_UPSCALING|upscale.slang|upscale_main"
    "ssao|GAMEENGINE_HAS_SSAO|ssao.slang|ssao_main"
    "ssr|GAMEENGINE_HAS_SSR|ssr.slang|ssr_main"
    "contact_shadows|GAMEENGINE_HAS_CONTACT_SHADOWS|contact_shadows.slang|contact_shadows_main"
    "volumetric_fog|GAMEENGINE_HAS_VOLUMETRIC_FOG|volumetric_fog.slang|volumetric_fog_main"
    "gpu_particles|GAMEENGINE_HAS_GPU_PARTICLES|gpu_particles.slang|gpu_particles_main"
    "realtime_gi|GAMEENGINE_HAS_REALTIME_GI|realtime_gi.slang|realtime_gi_main"
    "ray_tracing|GAMEENGINE_HAS_RAY_TRACING|ray_tracing.slang|ray_tracing_main"
    "virtualized_geometry|GAMEENGINE_HAS_VIRTUALIZED_GEOMETRY|virtualized_geometry.slang|virtualized_geometry_main"
)

string(CONCAT _adv_header_body
    "// Generated offline by scripts/compile_advanced_shaders.cmake. Do not edit manually.\n"
    "#pragma once\n\n"
    "#include <array>\n"
    "#include <cstdint>\n"
    "#include <string_view>\n\n"
    "namespace gameengine::renderer::advanced::shaders {\n\n")

foreach(_spec IN LISTS _adv_specs)
    string(REPLACE "|" ";" _parts "${_spec}")
    list(GET _parts 0 _name)
    list(GET _parts 1 _macro)
    list(GET _parts 2 _file)
    list(GET _parts 3 _entry)

    _compile_advanced_spirv(
        "${_name}"
        "${_macro}"
        "${_repository_dir}/assets/shaders/advanced/${_file}"
        "${_entry}"
        _id
        _words)

    string(APPEND _adv_header_body
        "#if ${_macro}\n"
        "inline constexpr std::string_view ${_name}_shader_id = \"${_id}\";\n"
        "inline constexpr std::array<std::uint32_t, 0> ${_name}_unused_guard = {};\n"
        "inline constexpr std::uint32_t ${_name}_spirv[] = {\n${_words}\n};\n"
        "inline constexpr std::size_t ${_name}_spirv_word_count = sizeof(${_name}_spirv) / sizeof(std::uint32_t);\n"
        "#endif\n\n")
endforeach()

string(APPEND _adv_header_body "} // namespace gameengine::renderer::advanced::shaders\n")

file(WRITE "${_advanced_header}" "${_adv_header_body}")
message(STATUS "Generated ${_advanced_header}")
