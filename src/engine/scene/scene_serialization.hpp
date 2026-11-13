#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "engine/core/status.hpp"
#include "engine/scene/scene.hpp"

namespace gameengine::scene {

[[nodiscard]] core::Status serialize_scene(
    const Scene& scene, std::vector<std::byte>& output) noexcept;

[[nodiscard]] core::Status deserialize_scene(
    std::span<const std::byte> bytes, Scene& output) noexcept;

[[nodiscard]] core::Status ensure_default_camera_and_light(Scene& scene) noexcept;

} // namespace gameengine::scene
