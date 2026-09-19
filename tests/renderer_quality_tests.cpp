#include "engine/renderer/renderer_quality.hpp"

#include <cassert>

int main()
{
    using gameengine::renderer::quality::RendererQuality;
    RendererQuality parsed = RendererQuality::low;
    if (!gameengine::renderer::quality::parse("medium", parsed) ||
        parsed != RendererQuality::medium ||
        gameengine::renderer::quality::parse("ultra", parsed)) {
        return 1;
    }

    const auto low = gameengine::renderer::quality::resolve(
        RendererQuality::high, false, true, true);
    if (low.effective != RendererQuality::low || !low.compute_fallback) {
        return 1;
    }

    const auto medium = gameengine::renderer::quality::resolve(
        RendererQuality::high, true, false, true);
    if (medium.effective != RendererQuality::medium || !medium.shadow_fallback) {
        return 1;
    }

    const auto analytic_environment = gameengine::renderer::quality::resolve(
        RendererQuality::high, true, true, false);
    if (analytic_environment.effective != RendererQuality::high ||
        !analytic_environment.environment_fallback ||
        gameengine::renderer::quality::name(RendererQuality::medium) != "medium") {
        return 1;
    }
    return 0;
}
