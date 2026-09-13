#include "engine/core/core.hpp"

int main()
{
    gameengine::core::Core core;

    if (!core.initialize()) {
        return 1;
    }

    core.shutdown();
    return 0;
}
