#pragma once

#include <string_view>

#include "engine/core/status.hpp"
#include "engine/core/types.hpp"

namespace gameengine::renderer::quality {

enum class RendererQuality : core::u8 {
    low = 0,
    medium,
    high,
};

struct Resolution final {
    RendererQuality requested = RendererQuality::medium;
    RendererQuality effective = RendererQuality::low;
    bool compute_fallback = false;
    bool shadow_fallback = false;
    bool environment_fallback = false;
};

[[nodiscard]] constexpr std::string_view name(RendererQuality quality) noexcept
{
    switch (quality) {
    case RendererQuality::low:
        return "low";
    case RendererQuality::medium:
        return "medium";
    case RendererQuality::high:
        return "high";
    }
    return "unknown";
}

[[nodiscard]] core::Status parse(std::string_view value, RendererQuality& quality) noexcept;

[[nodiscard]] constexpr Resolution resolve(RendererQuality requested,
                                            bool compute_available,
                                            bool shadow_available,
                                            bool environment_available) noexcept
{
    Resolution result{};
    result.requested = requested;
    result.effective = requested;
    if (requested != RendererQuality::low && !compute_available) {
        result.effective = RendererQuality::low;
        result.compute_fallback = true;
        return result;
    }
    if (requested == RendererQuality::high && !shadow_available) {
        result.effective = RendererQuality::medium;
        result.shadow_fallback = true;
    }
    if (requested == RendererQuality::high && !environment_available) {
        result.environment_fallback = true;
    }
    return result;
}

} // namespace gameengine::renderer::quality
