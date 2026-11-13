# Public C ABI (v1)

`src/api/c/gameengine_c.h` defines the versioned C11 public boundary (`gameengine_c_api`),
keeping internal C++ types, containers, templates, and exceptions behind opaque handles.

## Versioning and compatibility

- Header macros: `GE_API_VERSION_MAJOR` (`1`), `GE_API_VERSION_MINOR` (`0`),
  `GE_API_VERSION_PATCH` (`0`), and `GE_API_VERSION`.
- `ge_get_api_info(GeApiInfo* out_info)` verifies `out_info->struct_size` and returns the
  compiled ABI version.
- `ge_engine_create` checks `create_info->struct_size` and validates that the requested major
  version matches `GE_API_VERSION_MAJOR`, returning `GE_ERROR_INCOMPATIBLE_ABI` otherwise.

## Ownership and lifecycle

- `GeEngine*`: created by `ge_engine_create` and released by `ge_engine_destroy`. Passing
  `headless_platform_only = true` initializes the engine wrapper without opening a native window
  or Vulkan device.
- `GeScene*`: created by `ge_scene_create` and released by `ge_scene_destroy`. Scenes outlive
  attachment; callers must detach or destroy in reverse dependency order before destroying the
  engine if attached.
- `GeEntity`: `uint64_t` generational entity handle (`GE_INVALID_ENTITY == 0`), owned by its
  `GeScene`.

## Verification

`tests/c_api_tests.c` compiles strictly as C11 (`c_std_11`, `/W4 /WX` on MSVC, `-Wall -Wextra
-Wpedantic -Werror` on GCC/Clang) and runs under CTest as `gameengine_c_api`.
