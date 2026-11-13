#include "engine/scripting/scripting.hpp"

#include <cmath>
#include <cstdio>
#include <limits>
#include <string>

namespace {

using namespace gameengine;

bool expect(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
    return condition;
}

bool near_equal(core::f32 left, core::f32 right, core::f32 epsilon = 0.0005F)
{
    return std::fabs(left - right) <= epsilon;
}

} // namespace

int main()
{
    bool passed = true;

    // 1. Config validation
    {
        scripting::ScriptConfig invalid{};
        invalid.memory_limit_bytes = 1024U;
        passed = expect(!scripting::validate_config(invalid).ok(), "reject tiny memory limit") && passed;

        invalid = {};
        invalid.max_instructions_per_call = 0U;
        passed = expect(!scripting::validate_config(invalid).ok(), "reject zero instruction budget") && passed;

        invalid = {};
        invalid.max_named_entities = 0U;
        passed = expect(!scripting::validate_config(invalid).ok(), "reject zero entity capacity") && passed;

        passed = expect(scripting::validate_config({}).ok(), "default config is valid") && passed;
    }

    // 2. Lifecycle: initialize / double initialize / shutdown / reinitialize
    {
        scripting::ScriptRuntime runtime;
        passed = expect(!runtime.initialized(), "initially uninitialized") && passed;
        passed = expect(!runtime.load_script("x = 1").ok(), "load_script before init fails") && passed;
        passed = expect(runtime.initialize().ok(), "first initialize succeeds") && passed;
        passed = expect(runtime.initialized(), "initialized flag true") && passed;
        passed = expect(runtime.initialize().code == core::ErrorCode::already_initialized,
                        "double initialize rejected") && passed;

        const scripting::ScriptMetrics m = runtime.metrics();
        passed = expect(m.initialized && !m.script_loaded && m.memory_bytes > 0U,
                        "metrics reflect initialized state") && passed;

        runtime.shutdown();
        runtime.shutdown();
        passed = expect(!runtime.initialized(), "uninitialized after repeated shutdown") && passed;
        passed = expect(runtime.initialize().ok(), "reinitialize succeeds") && passed;
        runtime.shutdown();
    }

    // 3. Entity registration, start(), update(dt), Input.keyDown(), transform & scale & rotateY
    {
        scene::Scene scene;
        const scene::Entity player = scene.create_entity();
        passed = expect(player.valid(), "create player entity") && passed;
        passed = expect(scene.add_transform(player, {.local_position = {1.0F, 0.0F, 0.0F}}).ok(),
                        "add player transform") && passed;
        passed = expect(scene.update_transforms().ok(), "update initial transforms") && passed;

        scripting::ScriptRuntime runtime;
        passed = expect(runtime.initialize().ok(), "init runtime for gameplay test") && passed;
        passed = expect(runtime.register_entity("Player", player).ok(), "register Player") && passed;
        passed = expect(!runtime.register_entity("", player).ok(), "reject empty name") && passed;
        passed = expect(!runtime.register_entity("Bad", scene::invalid_entity).ok(),
                        "reject invalid entity") && passed;

        constexpr std::string_view gameplay_script = R"lua(
            local player = nil
            function start()
                player = Engine.findEntity("Player")
                assert(player ~= nil and player:isAlive())
                assert(Engine.findEntity("Missing") == nil)
                local x, y, z = player:getPosition()
                player:setPosition(x + 1.0, y, z)
                player:setScale(2.0, 2.0, 2.0)
            end

            function update(dt)
                if Input.keyDown("W") then
                    player:move(0.0, 0.0, 5.0 * dt)
                end
                player:rotateY(0.5 * dt)
            end
        )lua";

        passed = expect(runtime.load_script(gameplay_script).ok(), "load gameplay script") && passed;
        input::InputState input;
        passed = expect(runtime.call_start(scene, &input).ok(), "call_start succeeds") && passed;

        const scene::TransformComponent* transform = scene.transform(player);
        passed = expect(transform != nullptr, "player transform valid") && passed;
        if (transform != nullptr) {
            passed = expect(near_equal(transform->local_position.x, 2.0F),
                            "start moved position.x to 2.0") && passed;
            passed = expect(near_equal(transform->local_scale.x, 2.0F) &&
                            near_equal(transform->local_scale.y, 2.0F),
                            "start set scale to 2.0") && passed;
        }

        input::Event w_down{};
        w_down.type = input::EventType::key_pressed;
        w_down.key = input::KeyCode::w;
        input.apply(w_down);

        passed = expect(runtime.call_update(scene, 0.5F, &input).ok(),
                        "call_update with W pressed") && passed;
        if (transform != nullptr) {
            passed = expect(near_equal(transform->local_position.z, 2.5F),
                            "update moved z by 5 * 0.5") && passed;
            passed = expect(near_equal(transform->world_matrix.at(2, 3), 2.5F),
                            "scene world matrix synced after script update") && passed;
        }

        passed = expect(!runtime.call_update(scene, -0.1F, &input).ok(),
                        "reject negative dt") && passed;
        passed = expect(!runtime.call_update(scene, std::numeric_limits<core::f32>::quiet_NaN(), &input).ok(),
                        "reject NaN dt") && passed;

        const scripting::ScriptMetrics metrics = runtime.metrics();
        passed = expect(metrics.script_loaded && metrics.start_calls == 1U &&
                        metrics.update_calls == 1U && metrics.named_entity_count == 1U,
                        "metrics count calls and entities") && passed;

        // Destroyed entity is detected safely via generation check
        passed = expect(scene.destroy_entity(player).ok(), "destroy player entity") && passed;
        passed = expect(!runtime.call_update(scene, 0.1F, &input).ok(),
                        "update fails cleanly when script touches destroyed entity") && passed;
        passed = expect(!runtime.last_error().empty(), "last_error populated on stale entity") && passed;

        passed = expect(runtime.unregister_entity("Player").ok(), "unregister Player") && passed;
        passed = expect(!runtime.unregister_entity("Player").ok(), "double unregister fails") && passed;
        runtime.clear_entities();
        runtime.shutdown();
    }

    // 4. Security & untrusted input: syntax error, bytecode rejection, sandbox restrictions
    {
        scripting::ScriptRuntime runtime;
        passed = expect(runtime.initialize().ok(), "init runtime for security tests") && passed;

        // Syntax error
        const core::Status syntax_status = runtime.load_script("function broken(");
        passed = expect(syntax_status.code == core::ErrorCode::script_compile_failed,
                        "syntax error returns script_compile_failed") && passed;
        passed = expect(!runtime.last_error().empty(), "syntax error populates last_error") && passed;

        // Precompiled Lua bytecode header (\x1bLua) rejected in text-only mode
        const std::string fake_bytecode = "\x1bLuaQ\x00";
        const core::Status bytecode_status = runtime.load_script(fake_bytecode);
        passed = expect(bytecode_status.code == core::ErrorCode::script_compile_failed,
                        "binary bytecode rejected as untrusted input") && passed;

        // Sandbox removes os/io/debug/package/dofile/loadfile
        constexpr std::string_view sandbox_check = R"lua(
            assert(os == nil, "os must be nil in sandbox")
            assert(io == nil, "io must be nil in sandbox")
            assert(debug == nil, "debug must be nil in sandbox")
            assert(package == nil, "package must be nil in sandbox")
            assert(dofile == nil, "dofile must be nil in sandbox")
            assert(loadfile == nil, "loadfile must be nil in sandbox")
        )lua";
        passed = expect(runtime.load_script(sandbox_check).ok(),
                        "sandbox strips dangerous standard libraries") && passed;

        runtime.shutdown();
    }

    // 5. Instruction budget timeout protection (even wrapped inside pcall)
    {
        scripting::ScriptRuntime runtime;
        passed = expect(runtime.initialize({.memory_limit_bytes = 256U * 1024U,
                                            .max_instructions_per_call = 5'000U})
                            .ok(),
                        "init runtime with instruction budget") && passed;
        passed = expect(runtime.load_script(R"lua(
            function update(dt)
                pcall(function()
                    while true do end
                end)
            end
        )lua").ok(),
                        "load infinite loop script") && passed;

        scene::Scene scene;
        const core::Status loop_status = runtime.call_update(scene, 0.016F);
        passed = expect(loop_status.code == core::ErrorCode::script_instruction_limit_exceeded,
                        "infinite loop aborted by instruction budget") && passed;
        runtime.shutdown();
    }

    // 6. Memory limit enforcement
    {
        scripting::ScriptRuntime runtime;
        passed = expect(runtime.initialize({.memory_limit_bytes = 48U * 1024U,
                                            .max_instructions_per_call = 200'000U})
                            .ok(),
                        "init runtime with tight memory limit") && passed;
        passed = expect(runtime.load_script(R"lua(
            function update(dt)
                local t = {}
                for i = 1, 10000 do
                    t[i] = string.rep("x", 128)
                end
            end
        )lua").ok(),
                        "load allocation bomb script") && passed;

        scene::Scene scene;
        const core::Status mem_status = runtime.call_update(scene, 0.016F);
        passed = expect(mem_status.code == core::ErrorCode::script_memory_exceeded,
                        "excessive allocation stopped by bounded allocator") && passed;
        passed = expect(runtime.metrics().peak_memory_bytes <= 48U * 1024U,
                        "peak memory stayed within limit") && passed;
        runtime.shutdown();
    }

    // 7. Capacity enforcement for named entities
    {
        scripting::ScriptRuntime runtime;
        passed = expect(runtime.initialize({.max_named_entities = 2U}).ok(),
                        "init runtime with capacity 2") && passed;
        const scene::Entity e1 = scene::Entity::make(0U, 1U);
        const scene::Entity e2 = scene::Entity::make(1U, 1U);
        const scene::Entity e3 = scene::Entity::make(2U, 1U);
        passed = expect(runtime.register_entity("A", e1).ok(), "register A") && passed;
        passed = expect(runtime.register_entity("B", e2).ok(), "register B") && passed;
        passed = expect(runtime.register_entity("A", e3).ok(), "overwrite existing A") && passed;
        passed = expect(!runtime.register_entity("C", e3).ok(), "reject C beyond capacity") && passed;
        runtime.shutdown();
    }

    return passed ? 0 : 1;
}
