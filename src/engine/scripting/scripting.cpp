#include "engine/scripting/scripting.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <vector>

#include "engine/core/diagnostics.hpp"
#include "engine/math/math.hpp"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace gameengine::scripting {
namespace {

constexpr const char* entity_metatable_name = "GameEngine.Entity";
constexpr core::usize max_script_source_bytes = 4U * 1024U * 1024U;
constexpr core::usize error_buffer_capacity = 512U;

struct NamedEntityRecord final {
    std::array<char, max_entity_name_length + 1U> name{};
    core::u8 length = 0;
    scene::Entity entity{};
};

struct ScriptRuntimeState {
    explicit ScriptRuntimeState(const ScriptConfig& runtime_config) noexcept : config(runtime_config) {}

    void set_error(std::string_view message) noexcept
    {
        const core::usize copy_length = std::min(message.size(), error_buffer.size() - 1U);
        if (copy_length > 0U) {
            std::memcpy(error_buffer.data(), message.data(), copy_length);
        }
        error_buffer[copy_length] = '\0';
        error_length = copy_length;
    }

    void clear_error() noexcept
    {
        error_buffer[0] = '\0';
        error_length = 0;
    }

    [[nodiscard]] const NamedEntityRecord* find_named_entity(
        std::string_view name) const noexcept
    {
        for (const NamedEntityRecord& record : named_entities) {
            if (record.length == name.size() &&
                std::memcmp(record.name.data(), name.data(), record.length) == 0) {
                return &record;
            }
        }
        return nullptr;
    }

    [[nodiscard]] NamedEntityRecord* find_named_entity_mut(std::string_view name) noexcept
    {
        for (NamedEntityRecord& record : named_entities) {
            if (record.length == name.size() &&
                std::memcmp(record.name.data(), name.data(), record.length) == 0) {
                return &record;
            }
        }
        return nullptr;
    }

    ScriptConfig config{};
    lua_State* state = nullptr;
    std::vector<NamedEntityRecord> named_entities;
    std::array<char, error_buffer_capacity> error_buffer{};
    core::usize error_length = 0;

    core::usize memory_bytes = 0;
    core::usize peak_memory_bytes = 0;
    core::u64 allocation_count = 0;
    core::u64 start_calls = 0;
    core::u64 update_calls = 0;
    bool script_loaded = false;

    bool memory_exceeded = false;
    bool instruction_limit_exceeded = false;
    scene::Scene* active_scene = nullptr;
    const input::InputState* active_input = nullptr;
    bool scene_modified = false;
};

struct ActiveCallScope final {
    ScriptRuntimeState* impl = nullptr;

    ActiveCallScope(ScriptRuntimeState* target,
                    scene::Scene* scene,
                    const input::InputState* input) noexcept
        : impl(target)
    {
        impl->active_scene = scene;
        impl->active_input = input;
        impl->scene_modified = false;
        impl->memory_exceeded = false;
        impl->instruction_limit_exceeded = false;
    }

    ~ActiveCallScope() noexcept
    {
        if (impl != nullptr) {
            if (impl->state != nullptr) {
                lua_sethook(impl->state, nullptr, 0, 0);
            }
            impl->active_scene = nullptr;
            impl->active_input = nullptr;
        }
    }

    ActiveCallScope(const ActiveCallScope&) = delete;
    ActiveCallScope& operator=(const ActiveCallScope&) = delete;
};

void* bounded_lua_alloc(void* user_data, void* pointer, size_t old_size, size_t new_size) noexcept
{
    auto* impl = static_cast<ScriptRuntimeState*>(user_data);
    if (impl == nullptr) {
        return nullptr;
    }

    if (new_size == 0U) {
        if (pointer != nullptr) {
            const core::usize released = static_cast<core::usize>(old_size);
            impl->memory_bytes = impl->memory_bytes >= released ? impl->memory_bytes - released : 0U;
            std::free(pointer);
        }
        return nullptr;
    }

    if (pointer == nullptr) {
        // Under Lua's lua_Alloc contract, old_size is a type tag when pointer == nullptr.
        const core::usize requested = static_cast<core::usize>(new_size);
        if (requested > impl->config.memory_limit_bytes ||
            impl->memory_bytes > impl->config.memory_limit_bytes - requested) {
            impl->memory_exceeded = true;
            return nullptr;
        }
        void* allocated = std::malloc(new_size);
        if (allocated == nullptr) {
            return nullptr;
        }
        impl->memory_bytes += requested;
        impl->peak_memory_bytes = std::max(impl->peak_memory_bytes, impl->memory_bytes);
        ++impl->allocation_count;
        return allocated;
    }

    const core::usize previous = static_cast<core::usize>(old_size);
    const core::usize requested = static_cast<core::usize>(new_size);
    if (requested > previous) {
        const core::usize growth = requested - previous;
        if (growth > impl->config.memory_limit_bytes ||
            impl->memory_bytes > impl->config.memory_limit_bytes - growth) {
            impl->memory_exceeded = true;
            return nullptr;
        }
    }

    void* reallocated = std::realloc(pointer, new_size);
    if (reallocated == nullptr) {
        return nullptr;
    }
    if (requested >= previous) {
        impl->memory_bytes += requested - previous;
    } else {
        const core::usize shrink = previous - requested;
        impl->memory_bytes = impl->memory_bytes >= shrink ? impl->memory_bytes - shrink : 0U;
    }
    impl->peak_memory_bytes = std::max(impl->peak_memory_bytes, impl->memory_bytes);
    ++impl->allocation_count;
    return reallocated;
}

ScriptRuntimeState* runtime_from_lua(lua_State* state) noexcept
{
    void* user_data = nullptr;
    static_cast<void>(lua_getallocf(state, &user_data));
    return static_cast<ScriptRuntimeState*>(user_data);
}

void instruction_limit_hook(lua_State* state, lua_Debug* debug)
{
    (void)debug;
    ScriptRuntimeState* impl = runtime_from_lua(state);
    if (impl != nullptr) {
        impl->instruction_limit_exceeded = true;
    }
    // Re-arm every single instruction in case user code wrapped execution in Lua pcall().
    lua_sethook(state, instruction_limit_hook, LUA_MASKCOUNT, 1);
    luaL_error(state, "script instruction budget exceeded");
}

int lua_panic_handler(lua_State* state)
{
    ScriptRuntimeState* impl = runtime_from_lua(state);
    const char* message = lua_tostring(state, -1);
    if (impl != nullptr) {
        impl->set_error(message != nullptr ? message : "unhandled Lua panic");
    }
    return 0;
}

[[nodiscard]] bool equals_ascii_case_insensitive(std::string_view left,
                                                 std::string_view right) noexcept
{
    if (left.size() != right.size()) {
        return false;
    }
    for (core::usize index = 0; index < left.size(); ++index) {
        const unsigned char a = static_cast<unsigned char>(left[index]);
        const unsigned char b = static_cast<unsigned char>(right[index]);
        if (std::tolower(a) != std::tolower(b)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] input::KeyCode parse_key_name(std::string_view name) noexcept
{
    if (name.size() == 1U) {
        const char ch = static_cast<char>(std::tolower(static_cast<unsigned char>(name[0])));
        if (ch >= 'a' && ch <= 'z') {
            return static_cast<input::KeyCode>(
                static_cast<core::u16>(input::KeyCode::a) + static_cast<core::u16>(ch - 'a'));
        }
        if (ch >= '0' && ch <= '9') {
            return static_cast<input::KeyCode>(
                static_cast<core::u16>(input::KeyCode::digit_0) + static_cast<core::u16>(ch - '0'));
        }
    }
    if (equals_ascii_case_insensitive(name, "Escape")) return input::KeyCode::escape;
    if (equals_ascii_case_insensitive(name, "Enter")) return input::KeyCode::enter;
    if (equals_ascii_case_insensitive(name, "Tab")) return input::KeyCode::tab;
    if (equals_ascii_case_insensitive(name, "Backspace")) return input::KeyCode::backspace;
    if (equals_ascii_case_insensitive(name, "Space")) return input::KeyCode::space;
    if (equals_ascii_case_insensitive(name, "Left")) return input::KeyCode::left;
    if (equals_ascii_case_insensitive(name, "Right")) return input::KeyCode::right;
    if (equals_ascii_case_insensitive(name, "Up")) return input::KeyCode::up;
    if (equals_ascii_case_insensitive(name, "Down")) return input::KeyCode::down;
    if (equals_ascii_case_insensitive(name, "PageUp")) return input::KeyCode::page_up;
    if (equals_ascii_case_insensitive(name, "PageDown")) return input::KeyCode::page_down;
    if (equals_ascii_case_insensitive(name, "Shift")) return input::KeyCode::shift;
    if (equals_ascii_case_insensitive(name, "Control") ||
        equals_ascii_case_insensitive(name, "Ctrl")) {
        return input::KeyCode::control;
    }
    if (equals_ascii_case_insensitive(name, "Alt")) return input::KeyCode::alt;
    return input::KeyCode::unknown;
}

scene::Entity* check_entity_userdata(lua_State* state, int stack_index)
{
    void* userdata = luaL_checkudata(state, stack_index, entity_metatable_name);
    return static_cast<scene::Entity*>(userdata);
}

scene::TransformComponent* require_live_transform(lua_State* state,
                                                  ScriptRuntimeState*& out_impl)
{
    out_impl = runtime_from_lua(state);
    if (out_impl == nullptr || out_impl->active_scene == nullptr) {
        luaL_error(state, "no active scene in current script context");
        return nullptr;
    }
    const scene::Entity* entity = check_entity_userdata(state, 1);
    if (entity == nullptr || !out_impl->active_scene->is_alive(*entity)) {
        luaL_error(state, "entity handle is invalid or destroyed");
        return nullptr;
    }
    scene::TransformComponent* transform = out_impl->active_scene->transform(*entity);
    if (transform == nullptr) {
        luaL_error(state, "entity does not have a TransformComponent");
        return nullptr;
    }
    return transform;
}

core::f32 check_finite_float(lua_State* state, int stack_index)
{
    const lua_Number number = luaL_checknumber(state, stack_index);
    const core::f32 value = static_cast<core::f32>(number);
    if (!std::isfinite(value)) {
        luaL_argerror(state, stack_index, "expected finite number");
    }
    return value;
}

int lua_entity_is_alive(lua_State* state)
{
    const ScriptRuntimeState* impl = runtime_from_lua(state);
    const scene::Entity* entity = check_entity_userdata(state, 1);
    const bool alive = impl != nullptr && impl->active_scene != nullptr &&
                       entity != nullptr && impl->active_scene->is_alive(*entity);
    lua_pushboolean(state, alive ? 1 : 0);
    return 1;
}

int lua_entity_get_position(lua_State* state)
{
    ScriptRuntimeState* impl = nullptr;
    const scene::TransformComponent* transform = require_live_transform(state, impl);
    if (transform == nullptr) {
        return 0;
    }
    lua_pushnumber(state, static_cast<lua_Number>(transform->local_position.x));
    lua_pushnumber(state, static_cast<lua_Number>(transform->local_position.y));
    lua_pushnumber(state, static_cast<lua_Number>(transform->local_position.z));
    return 3;
}

int lua_entity_set_position(lua_State* state)
{
    ScriptRuntimeState* impl = nullptr;
    scene::TransformComponent* transform = require_live_transform(state, impl);
    if (transform == nullptr) {
        return 0;
    }
    const core::f32 x = check_finite_float(state, 2);
    const core::f32 y = check_finite_float(state, 3);
    const core::f32 z = check_finite_float(state, 4);
    transform->local_position = {x, y, z};
    impl->scene_modified = true;
    return 0;
}

int lua_entity_move(lua_State* state)
{
    ScriptRuntimeState* impl = nullptr;
    scene::TransformComponent* transform = require_live_transform(state, impl);
    if (transform == nullptr) {
        return 0;
    }
    const core::f32 dx = check_finite_float(state, 2);
    const core::f32 dy = check_finite_float(state, 3);
    const core::f32 dz = check_finite_float(state, 4);
    const math::Vec3 next{
        transform->local_position.x + dx,
        transform->local_position.y + dy,
        transform->local_position.z + dz,
    };
    if (!math::is_finite(next)) {
        return luaL_error(state, "resulting position is non-finite");
    }
    transform->local_position = next;
    impl->scene_modified = true;
    return 0;
}

int lua_entity_get_scale(lua_State* state)
{
    ScriptRuntimeState* impl = nullptr;
    const scene::TransformComponent* transform = require_live_transform(state, impl);
    if (transform == nullptr) {
        return 0;
    }
    lua_pushnumber(state, static_cast<lua_Number>(transform->local_scale.x));
    lua_pushnumber(state, static_cast<lua_Number>(transform->local_scale.y));
    lua_pushnumber(state, static_cast<lua_Number>(transform->local_scale.z));
    return 3;
}

int lua_entity_set_scale(lua_State* state)
{
    ScriptRuntimeState* impl = nullptr;
    scene::TransformComponent* transform = require_live_transform(state, impl);
    if (transform == nullptr) {
        return 0;
    }
    const core::f32 x = check_finite_float(state, 2);
    const core::f32 y = check_finite_float(state, 3);
    const core::f32 z = check_finite_float(state, 4);
    transform->local_scale = {x, y, z};
    impl->scene_modified = true;
    return 0;
}

int lua_entity_rotate_y(lua_State* state)
{
    ScriptRuntimeState* impl = nullptr;
    scene::TransformComponent* transform = require_live_transform(state, impl);
    if (transform == nullptr) {
        return 0;
    }
    const core::f32 radians = check_finite_float(state, 2);
    const math::Quaternion delta = math::quaternion_from_axis_angle({0.0F, 1.0F, 0.0F}, radians);
    transform->local_rotation = math::normalize(math::multiply(transform->local_rotation, delta));
    impl->scene_modified = true;
    return 0;
}

int lua_entity_eq(lua_State* state)
{
    const scene::Entity* left = check_entity_userdata(state, 1);
    const scene::Entity* right = check_entity_userdata(state, 2);
    const bool equal = left != nullptr && right != nullptr && (*left == *right);
    lua_pushboolean(state, equal ? 1 : 0);
    return 1;
}

int lua_engine_find_entity(lua_State* state)
{
    size_t length = 0;
    const char* name_ptr = luaL_checklstring(state, 1, &length);
    const ScriptRuntimeState* impl = runtime_from_lua(state);
    if (impl == nullptr || impl->active_scene == nullptr || name_ptr == nullptr) {
        lua_pushnil(state);
        return 1;
    }
    const NamedEntityRecord* record =
        impl->find_named_entity(std::string_view{name_ptr, length});
    if (record == nullptr || !impl->active_scene->is_alive(record->entity)) {
        lua_pushnil(state);
        return 1;
    }
    void* storage = lua_newuserdatauv(state, sizeof(scene::Entity), 0);
    std::memcpy(storage, &record->entity, sizeof(scene::Entity));
    luaL_setmetatable(state, entity_metatable_name);
    return 1;
}

int lua_engine_is_alive(lua_State* state)
{
    return lua_entity_is_alive(state);
}

int lua_engine_log(lua_State* state)
{
    size_t length = 0;
    const char* text = luaL_checklstring(state, 1, &length);
    if (text != nullptr && length > 0U) {
        std::array<char, 256> message{};
        const core::usize copy_length = std::min<core::usize>(length, message.size() - 1U);
        std::memcpy(message.data(), text, copy_length);
        message[copy_length] = '\0';
        core::log(core::LogLevel::info, message.data());
    }
    return 0;
}

int lua_input_key_down(lua_State* state)
{
    size_t length = 0;
    const char* key_ptr = luaL_checklstring(state, 1, &length);
    const ScriptRuntimeState* impl = runtime_from_lua(state);
    if (impl == nullptr || impl->active_input == nullptr || key_ptr == nullptr) {
        lua_pushboolean(state, 0);
        return 1;
    }
    const input::KeyCode code = parse_key_name(std::string_view{key_ptr, length});
    const bool down =
        code != input::KeyCode::unknown && impl->active_input->is_key_down(code);
    lua_pushboolean(state, down ? 1 : 0);
    return 1;
}

void open_libraries(lua_State* state, bool sandbox) noexcept
{
    if (!sandbox) {
        luaL_openlibs(state);
        return;
    }

    static constexpr luaL_Reg safe_libs[] = {
        {LUA_GNAME, luaopen_base},
        {LUA_COLIBNAME, luaopen_coroutine},
        {LUA_TABLIBNAME, luaopen_table},
        {LUA_STRLIBNAME, luaopen_string},
        {LUA_MATHLIBNAME, luaopen_math},
        {LUA_UTF8LIBNAME, luaopen_utf8},
    };
    for (const luaL_Reg& lib : safe_libs) {
        luaL_requiref(state, lib.name, lib.func, 1);
        lua_pop(state, 1);
    }

    // Strip file-loading helpers from base in sandbox mode so scripts cannot touch the disk.
    lua_pushnil(state);
    lua_setglobal(state, "dofile");
    lua_pushnil(state);
    lua_setglobal(state, "loadfile");
}

void register_engine_api(lua_State* state) noexcept
{
    static constexpr luaL_Reg entity_methods[] = {
        {"isAlive", lua_entity_is_alive},
        {"getPosition", lua_entity_get_position},
        {"setPosition", lua_entity_set_position},
        {"move", lua_entity_move},
        {"getScale", lua_entity_get_scale},
        {"setScale", lua_entity_set_scale},
        {"rotateY", lua_entity_rotate_y},
        {nullptr, nullptr},
    };

    if (luaL_newmetatable(state, entity_metatable_name) != 0) {
        lua_pushcfunction(state, lua_entity_eq);
        lua_setfield(state, -2, "__eq");

        lua_newtable(state);
        luaL_setfuncs(state, entity_methods, 0);
        lua_setfield(state, -2, "__index");
    }
    lua_pop(state, 1);

    static constexpr luaL_Reg engine_functions[] = {
        {"findEntity", lua_engine_find_entity},
        {"isAlive", lua_engine_is_alive},
        {"log", lua_engine_log},
        {nullptr, nullptr},
    };
    luaL_newlib(state, engine_functions);
    lua_setglobal(state, "Engine");

    static constexpr luaL_Reg input_functions[] = {
        {"keyDown", lua_input_key_down},
        {nullptr, nullptr},
    };
    luaL_newlib(state, input_functions);
    lua_setglobal(state, "Input");
}

[[nodiscard]] core::Status map_lua_call_error(ScriptRuntimeState* impl,
                                                int lua_status,
                                                core::ErrorCode fallback) noexcept
{
    if (impl->instruction_limit_exceeded) {
        if (impl->error_length == 0U) {
            impl->set_error("script instruction budget exceeded");
        }
        lua_pop(impl->state, 1);
        return core::Status{core::ErrorCode::script_instruction_limit_exceeded};
    }
    if (impl->memory_exceeded || lua_status == LUA_ERRMEM) {
        if (impl->error_length == 0U) {
            impl->set_error("script memory limit exceeded");
        }
        if (lua_gettop(impl->state) > 0) {
            lua_pop(impl->state, 1);
        }
        return core::Status{core::ErrorCode::script_memory_exceeded};
    }
    const char* message = lua_tostring(impl->state, -1);
    impl->set_error(message != nullptr ? message : "unknown Lua error");
    lua_pop(impl->state, 1);
    return core::Status{fallback};
}

[[nodiscard]] core::Status run_protected_global(ScriptRuntimeState* impl,
                                                const char* function_name,
                                                scene::Scene& scene,
                                                const input::InputState* input,
                                                bool has_dt,
                                                core::f32 delta_seconds,
                                                bool& executed) noexcept
{
    executed = false;
    impl->clear_error();

    lua_getglobal(impl->state, function_name);
    if (lua_isnil(impl->state, -1)) {
        lua_pop(impl->state, 1);
        return {};
    }
    if (!lua_isfunction(impl->state, -1)) {
        lua_pop(impl->state, 1);
        impl->set_error("expected global script hook to be a function");
        return core::Status{core::ErrorCode::script_runtime_failed};
    }

    int argument_count = 0;
    if (has_dt) {
        lua_pushnumber(impl->state, static_cast<lua_Number>(delta_seconds));
        argument_count = 1;
    }

    const int budget = static_cast<int>(
        std::min<core::u32>(impl->config.max_instructions_per_call,
                            static_cast<core::u32>(std::numeric_limits<int>::max())));
    ActiveCallScope scope(impl, &scene, input);
    lua_sethook(impl->state, instruction_limit_hook, LUA_MASKCOUNT, budget);
    const int pcall_status = lua_pcall(impl->state, argument_count, 0, 0);
    if (pcall_status != LUA_OK) {
        return map_lua_call_error(impl, pcall_status, core::ErrorCode::script_runtime_failed);
    }
    if (impl->instruction_limit_exceeded) {
        impl->set_error("script instruction budget exceeded");
        return core::Status{core::ErrorCode::script_instruction_limit_exceeded};
    }
    if (impl->memory_exceeded) {
        impl->set_error("script memory limit exceeded");
        return core::Status{core::ErrorCode::script_memory_exceeded};
    }
    if (impl->scene_modified) {
        const core::Status transform_status = scene.update_transforms();
        if (!transform_status) {
            impl->set_error("scene transform update failed after script execution");
            return transform_status;
        }
    }
    executed = true;
    return {};
}

} // namespace

class ScriptRuntime::Impl final : public ScriptRuntimeState {
public:
    using ScriptRuntimeState::ScriptRuntimeState;
};

core::Status validate_config(const ScriptConfig& config) noexcept
{
    if (config.memory_limit_bytes < minimum_memory_limit_bytes ||
        config.max_instructions_per_call == 0U || config.max_named_entities == 0U) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    return {};
}

ScriptRuntime::~ScriptRuntime() noexcept
{
    shutdown();
}

core::Status ScriptRuntime::initialize(const ScriptConfig& config) noexcept
{
    if (impl_ != nullptr) {
        return core::Status{core::ErrorCode::already_initialized};
    }
    const core::Status config_status = validate_config(config);
    if (!config_status) {
        return config_status;
    }

    Impl* next_impl = new (std::nothrow) Impl(config);
    if (next_impl == nullptr) {
        return core::Status{core::ErrorCode::allocation_failed};
    }
    next_impl->named_entities.reserve(config.max_named_entities);

    lua_State* state = lua_newstate(bounded_lua_alloc, static_cast<ScriptRuntimeState*>(next_impl));
    if (state == nullptr) {
        const bool oom = next_impl->memory_exceeded;
        delete next_impl;
        return core::Status{oom ? core::ErrorCode::script_memory_exceeded
                               : core::ErrorCode::allocation_failed};
    }
    next_impl->state = state;
    lua_atpanic(state, lua_panic_handler);
    open_libraries(state, config.sandbox);
    register_engine_api(state);
    if (next_impl->memory_exceeded) {
        lua_close(state);
        delete next_impl;
        return core::Status{core::ErrorCode::script_memory_exceeded};
    }

    impl_ = next_impl;
    return {};
}

void ScriptRuntime::shutdown() noexcept
{
    if (impl_ == nullptr) {
        return;
    }
    if (impl_->state != nullptr) {
        lua_close(impl_->state);
        impl_->state = nullptr;
    }
    impl_->named_entities.clear();
    delete impl_;
    impl_ = nullptr;
}

core::Status ScriptRuntime::load_script(std::string_view source,
                                        std::string_view chunk_name) noexcept
{
    if (impl_ == nullptr || impl_->state == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    if (source.empty() || source.size() > max_script_source_bytes || chunk_name.empty()) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    impl_->clear_error();
    impl_->memory_exceeded = false;
    impl_->instruction_limit_exceeded = false;

    std::array<char, 128> chunk_label{};
    const core::usize label_len = std::min(chunk_name.size(), chunk_label.size() - 1U);
    std::memcpy(chunk_label.data(), chunk_name.data(), label_len);
    chunk_label[label_len] = '\0';

    // Mode "t" strictly forbids precompiled Lua bytecode as untrusted binary input.
    const int load_status =
        luaL_loadbufferx(impl_->state, source.data(), source.size(), chunk_label.data(), "t");
    if (load_status != LUA_OK) {
        return map_lua_call_error(impl_, load_status, core::ErrorCode::script_compile_failed);
    }

    const int budget = static_cast<int>(
        std::min<core::u32>(impl_->config.max_instructions_per_call,
                            static_cast<core::u32>(std::numeric_limits<int>::max())));
    ActiveCallScope scope(impl_, nullptr, nullptr);
    lua_sethook(impl_->state, instruction_limit_hook, LUA_MASKCOUNT, budget);
    const int exec_status = lua_pcall(impl_->state, 0, 0, 0);
    if (exec_status != LUA_OK) {
        return map_lua_call_error(impl_, exec_status, core::ErrorCode::script_runtime_failed);
    }
    if (impl_->instruction_limit_exceeded) {
        impl_->set_error("script instruction budget exceeded");
        return core::Status{core::ErrorCode::script_instruction_limit_exceeded};
    }
    impl_->script_loaded = true;
    return {};
}

core::Status ScriptRuntime::register_entity(std::string_view name, scene::Entity entity) noexcept
{
    if (impl_ == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    if (name.empty() || name.size() > max_entity_name_length || !entity.valid()) {
        return core::Status{core::ErrorCode::invalid_argument};
    }

    if (NamedEntityRecord* existing = impl_->find_named_entity_mut(name); existing != nullptr) {
        existing->entity = entity;
        return {};
    }
    if (impl_->named_entities.size() >= impl_->config.max_named_entities) {
        return core::Status{core::ErrorCode::invalid_argument};
    }

    NamedEntityRecord record{};
    std::memcpy(record.name.data(), name.data(), name.size());
    record.name[name.size()] = '\0';
    record.length = static_cast<core::u8>(name.size());
    record.entity = entity;
    impl_->named_entities.push_back(record);
    return {};
}

core::Status ScriptRuntime::unregister_entity(std::string_view name) noexcept
{
    if (impl_ == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    if (name.empty() || name.size() > max_entity_name_length) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    for (auto it = impl_->named_entities.begin(); it != impl_->named_entities.end(); ++it) {
        if (it->length == name.size() &&
            std::memcmp(it->name.data(), name.data(), it->length) == 0) {
            impl_->named_entities.erase(it);
            return {};
        }
    }
    return core::Status{core::ErrorCode::invalid_argument};
}

void ScriptRuntime::clear_entities() noexcept
{
    if (impl_ != nullptr) {
        impl_->named_entities.clear();
    }
}

core::Status ScriptRuntime::call_start(scene::Scene& scene,
                                       const input::InputState* input) noexcept
{
    if (impl_ == nullptr || impl_->state == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    if (!impl_->script_loaded) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    bool executed = false;
    const core::Status status =
        run_protected_global(impl_, "start", scene, input, false, 0.0F, executed);
    if (status && executed) {
        ++impl_->start_calls;
    }
    return status;
}

core::Status ScriptRuntime::call_update(scene::Scene& scene,
                                        core::f32 delta_seconds,
                                        const input::InputState* input) noexcept
{
    if (impl_ == nullptr || impl_->state == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    if (!impl_->script_loaded || !std::isfinite(delta_seconds) || delta_seconds < 0.0F) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    bool executed = false;
    const core::Status status =
        run_protected_global(impl_, "update", scene, input, true, delta_seconds, executed);
    if (status && executed) {
        ++impl_->update_calls;
    }
    return status;
}

std::string_view ScriptRuntime::last_error() const noexcept
{
    if (impl_ == nullptr || impl_->error_length == 0U) {
        return {};
    }
    return std::string_view{impl_->error_buffer.data(), impl_->error_length};
}

ScriptMetrics ScriptRuntime::metrics() const noexcept
{
    if (impl_ == nullptr) {
        return {};
    }
    return ScriptMetrics{
        .memory_bytes = impl_->memory_bytes,
        .peak_memory_bytes = impl_->peak_memory_bytes,
        .allocation_count = impl_->allocation_count,
        .start_calls = impl_->start_calls,
        .update_calls = impl_->update_calls,
        .named_entity_count = static_cast<core::u32>(impl_->named_entities.size()),
        .initialized = true,
        .script_loaded = impl_->script_loaded,
    };
}

} // namespace gameengine::scripting
