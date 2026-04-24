#include "engine/scripting/LuaBindings.h"

#include "core/Log.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"

#include <glm/vec3.hpp>

#include <string>

namespace Mood {

void setupLuaBindings(sol::state& lua, Entity self) {
    // Libs basicas. `io`/`os` quedan fuera: los scripts no deberian hacer
    // I/O al FS ni ejecutar procesos. `package` tampoco (sin require).
    lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string);

    // --- glm::vec3 ---
    lua.new_usertype<glm::vec3>("Vec3",
        "x", &glm::vec3::x,
        "y", &glm::vec3::y,
        "z", &glm::vec3::z);

    // --- TransformComponent ---
    lua.new_usertype<TransformComponent>("TransformComponent",
        "position",      &TransformComponent::position,
        "rotationEuler", &TransformComponent::rotationEuler,
        "scale",         &TransformComponent::scale);

    // --- Entity ---
    // `tag`: string inmutable derivado de TagComponent.
    // `transform`: referencia al TransformComponent para mutacion en-place.
    lua.new_usertype<Entity>("Entity",
        "tag", sol::property([](Entity& e) -> std::string {
            return e.hasComponent<TagComponent>()
                ? e.getComponent<TagComponent>().name
                : std::string{};
        }),
        "transform", sol::property([](Entity& e) -> TransformComponent& {
            return e.getComponent<TransformComponent>();
        }));

    // --- log.info / log.warn ---
    sol::table logTable = lua.create_named_table("log");
    logTable.set_function("info", [](const std::string& msg) {
        Log::script()->info("{}", msg);
    });
    logTable.set_function("warn", [](const std::string& msg) {
        Log::script()->warn("{}", msg);
    });

    // self como global.
    lua["self"] = self;
}

} // namespace Mood
