#include "api/c/gameengine_c.h"
#include "engine/assets/asset_reader.hpp"
#include "engine/assets/prepared_assets.hpp"
#include "engine/audio/audio.hpp"
#include "engine/core/core.hpp"
#include "engine/scene/scene.hpp"
#include "engine/scene/scene_serialization.hpp"

#if GAMEENGINE_BUILD_PHYSICS
#include "engine/physics/physics.hpp"
#endif

#if GAMEENGINE_BUILD_LUA
#include "engine/scripting/scripting.hpp"
#endif

#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

using gameengine::core::u64;

[[nodiscard]] u64 splitmix64(u64& state) noexcept
{
    state += 0x9e3779b97f4a7c15ULL;
    u64 z = state;
    z = (z ^ (z >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27U)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31U);
}

bool test_repeated_shutdown_and_destruction() noexcept
{
    for (int cycle = 0; cycle < 16; ++cycle) {
        gameengine::scene::Scene scene;
        if (!gameengine::scene::create_bootstrap_scene(scene).ok()) {
            return false;
        }
        std::vector<std::byte> bytes;
        if (!gameengine::scene::serialize_scene(scene, bytes).ok()) {
            return false;
        }
        scene.clear();
        if (!gameengine::scene::deserialize_scene(bytes, scene).ok()) {
            return false;
        }
    }

    for (int cycle = 0; cycle < 12; ++cycle) {
        gameengine::audio::AudioSystem audio;
        if (!audio.initialize({.enable_output = false}).ok()) {
            return false;
        }
        gameengine::audio::VoiceHandle voice{};
        std::vector<float> mix_buffer(
            static_cast<std::size_t>(gameengine::audio::kFramesPerBuffer) *
                gameengine::audio::kChannelCount,
            0.0F);
        if (!audio.play_tone({}, voice).ok() || !audio.stop(voice).ok() ||
            !audio.render_for_testing(mix_buffer).ok()) {
            audio.shutdown();
            return false;
        }
        audio.shutdown();
        audio.shutdown();
        if (audio.stop(voice).ok()) {
            return false;
        }
    }

    for (int cycle = 0; cycle < 12; ++cycle) {
        GeEngineCreateInfo info{};
        info.struct_size = static_cast<uint32_t>(sizeof(GeEngineCreateInfo));
        info.api_version = GE_API_VERSION;
        info.headless_platform_only = true;

        GeEngine* engine = nullptr;
        GeScene* scene = nullptr;
        if (ge_engine_create(&info, &engine) != GE_OK || ge_scene_create(&scene) != GE_OK) {
            ge_scene_destroy(scene);
            ge_engine_destroy(engine);
            return false;
        }
        const GeEntity entity = ge_scene_entity_create(scene);
        bool cont = false;
        if (entity == GE_INVALID_ENTITY ||
            ge_scene_transform_set_position(scene, entity, {1.0F, 2.0F, 3.0F}) != GE_OK ||
            ge_engine_attach_scene(engine, scene) != GE_OK ||
            ge_engine_step_frame(engine, &cont) != GE_OK || !cont ||
            ge_engine_detach_scene(engine) != GE_OK) {
            ge_scene_destroy(scene);
            ge_engine_destroy(engine);
            return false;
        }
        ge_scene_destroy(scene);
        ge_engine_destroy(engine);
    }

#if GAMEENGINE_BUILD_PHYSICS
    for (int cycle = 0; cycle < 12; ++cycle) {
        gameengine::scene::Scene scene;
        const gameengine::scene::Entity e = scene.create_entity();
        if (!e.valid() || !scene.add_transform(e, {.local_position = {0.0F, 2.0F, 0.0F}}).ok()) {
            return false;
        }
        gameengine::physics::PhysicsWorld world;
        if (!world.initialize().ok()) {
            return false;
        }
        if (!world.add_body(scene, e, {}).ok() || !world.update(1.0F / 60.0F).ok() ||
            !world.write_transforms(scene).ok() || !world.remove_body(e).ok() ||
            world.remove_body(e).ok()) {
            world.shutdown();
            return false;
        }
        world.shutdown();
        world.shutdown();
    }
#endif

#if GAMEENGINE_BUILD_LUA
    for (int cycle = 0; cycle < 12; ++cycle) {
        gameengine::scene::Scene scene;
        gameengine::scripting::ScriptRuntime vm;
        if (!vm.initialize().ok()) {
            return false;
        }
        if (!vm.load_script("function update(dt) local x = dt * 2.0 end").ok() ||
            !vm.call_update(scene, 1.0F / 60.0F).ok()) {
            vm.shutdown();
            return false;
        }
        vm.shutdown();
        vm.shutdown();
    }
#endif

    return true;
}

bool test_seeded_parser_fuzzing() noexcept
{
    gameengine::scene::Scene bootstrap;
    if (!gameengine::scene::create_bootstrap_scene(bootstrap).ok()) {
        return false;
    }
    std::vector<std::byte> valid_scene;
    if (!gameengine::scene::serialize_scene(bootstrap, valid_scene).ok() || valid_scene.empty()) {
        return false;
    }

    u64 seed = 0x123456789abcdef0ULL;
    for (int iteration = 0; iteration < 512; ++iteration) {
        std::vector<std::byte> mutated = valid_scene;
        const u64 op = splitmix64(seed) % 4U;
        if (op == 0U && !mutated.empty()) {
            mutated.resize(static_cast<std::size_t>(splitmix64(seed) % mutated.size()));
        } else if (op == 1U && mutated.size() >= 4U) {
            mutated[0] = std::byte{0xFF};
            mutated[1] = std::byte{0x00};
        } else if (!mutated.empty()) {
            const std::size_t flips = 1U + static_cast<std::size_t>(splitmix64(seed) % 4U);
            for (std::size_t f = 0; f < flips; ++f) {
                const std::size_t idx = static_cast<std::size_t>(splitmix64(seed) % mutated.size());
                mutated[idx] ^= static_cast<std::byte>(splitmix64(seed) & 0xFFU);
            }
        }

        gameengine::assets::AssetView asset_view{};
        static_cast<void>(gameengine::assets::validate_container(
            mutated, gameengine::assets::AssetKind::scene, asset_view));
        gameengine::assets::MeshView mesh_view{};
        static_cast<void>(gameengine::assets::read_mesh(mutated, mesh_view));
        gameengine::assets::TextureView tex_view{};
        static_cast<void>(gameengine::assets::read_texture(mutated, tex_view));
        gameengine::assets::MaterialView mat_view{};
        static_cast<void>(gameengine::assets::read_material(mutated, mat_view));
        gameengine::assets::SceneView scene_view{};
        static_cast<void>(gameengine::assets::read_scene(mutated, scene_view));

        gameengine::scene::Scene parsed_scene;
        static_cast<void>(gameengine::scene::deserialize_scene(mutated, parsed_scene));
    }

    return true;
}

bool test_handles_and_references_after_destruction() noexcept
{
    gameengine::scene::Scene scene;
    const gameengine::scene::Entity e1 = scene.create_entity();
    const gameengine::scene::Entity child = scene.create_entity();
    if (!e1.valid() || !child.valid() || !scene.add_transform(e1).ok() ||
        !scene.add_transform(child).ok() || !scene.set_parent(child, e1).ok()) {
        return false;
    }

    if (!scene.destroy_entity(e1).ok() || scene.is_alive(e1) ||
        scene.destroy_entity(e1).ok() || scene.transform(e1) != nullptr ||
        scene.mesh_renderer(e1) != nullptr || scene.camera(e1) != nullptr ||
        scene.directional_light(e1) != nullptr || scene.add_transform(e1).ok() ||
        scene.set_parent(child, e1).ok()) {
        return false;
    }

    // Reusing the freed slot must bump generation and keep the old handle stale.
    const gameengine::scene::Entity e2 = scene.create_entity();
    if (!e2.valid() || e2 == e1 || scene.is_alive(e1) || !scene.is_alive(e2)) {
        return false;
    }

    // C ABI stale handle check across slot reuse.
    GeScene* c_scene = nullptr;
    if (ge_scene_create(&c_scene) != GE_OK) {
        return false;
    }
    const GeEntity ce1 = ge_scene_entity_create(c_scene);
    if (ce1 == GE_INVALID_ENTITY || ge_scene_entity_destroy(c_scene, ce1) != GE_OK ||
        ge_scene_entity_is_alive(c_scene, ce1) ||
        ge_scene_transform_set_position(c_scene, ce1, {0.0F, 0.0F, 0.0F}) !=
            GE_ERROR_INVALID_ARGUMENT) {
        ge_scene_destroy(c_scene);
        return false;
    }
    const GeEntity ce2 = ge_scene_entity_create(c_scene);
    const bool ok = ce2 != GE_INVALID_ENTITY && ce2 != ce1 && !ge_scene_entity_is_alive(c_scene, ce1);
    ge_scene_destroy(c_scene);
    return ok;
}

} // namespace

int main()
{
    if (!test_repeated_shutdown_and_destruction()) {
        return 1;
    }
    if (!test_seeded_parser_fuzzing()) {
        return 2;
    }
    if (!test_handles_and_references_after_destruction()) {
        return 3;
    }
    return 0;
}
