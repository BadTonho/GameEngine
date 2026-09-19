#include "engine/renderer/renderer_quality.hpp"

namespace gameengine::renderer::quality {

core::Status parse(std::string_view value, RendererQuality& quality) noexcept
{
    if (value == "low") {
        quality = RendererQuality::low;
        return core::Status{};
    }
    if (value == "medium") {
        quality = RendererQuality::medium;
        return core::Status{};
    }
    if (value == "high") {
        quality = RendererQuality::high;
        return core::Status{};
    }
    return core::Status{core::ErrorCode::invalid_argument};
}

} // namespace gameengine::renderer::quality
