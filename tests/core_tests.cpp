#include "engine/core/core.hpp"

int main()
{
    gameengine::core::Core core;

    if (core.is_initialized()) {
        return 1;
    }

    if (!core.initialize()) {
        return 2;
    }

    if (!core.is_initialized()) {
        return 3;
    }

    if (core.initialize()) {
        return 4;
    }

    core.shutdown();

    if (core.is_initialized()) {
        return 5;
    }

    return 0;
}
