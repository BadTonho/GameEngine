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

} // namespace gameengine::scene
