// Tests de ScriptSystem + LuaBindings (Hito 8 Bloque 7).
// Escriben scripts a archivos temporales y corren el sistema sin GL ni
// editor. Cubren:
//   - carga inicial y mutacion de TransformComponent desde Lua.
//   - hot-reload: un cambio de mtime dispara re-ejecucion del .lua.

#include <doctest/doctest.h>

#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "systems/scripting/ScriptSystem.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

using namespace Mood;

namespace {

std::filesystem::path writeTempScript(const std::string& content) {
    const auto dir = std::filesystem::temp_directory_path();
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = dir / ("mood_test_" + std::to_string(stamp) + ".lua");
    std::ofstream f(path, std::ios::binary);
    f << content;
    return path;
}

} // namespace

TEST_CASE("ScriptSystem: onUpdate muta TransformComponent de self") {
    const auto path = writeTempScript(R"(
        function onUpdate(self, dt)
            self.transform.position.x = 7.5
        end
    )");

    Scene scene;
    Entity e = scene.createEntity("victim");
    e.addComponent<ScriptComponent>(path.generic_string());

    ScriptSystem sys;
    sys.update(scene, 0.016f);

    CHECK(e.getComponent<TransformComponent>().position.x == doctest::Approx(7.5f));

    std::filesystem::remove(path);
}

TEST_CASE("ScriptSystem: hot-reload re-ejecuta al cambiar mtime") {
    const auto path = writeTempScript(R"(
        function onUpdate(self, dt)
            self.transform.position.x = 1.0
        end
    )");

    Scene scene;
    Entity e = scene.createEntity("victim");
    e.addComponent<ScriptComponent>(path.generic_string());

    ScriptSystem sys;
    sys.update(scene, 0.016f);
    CHECK(e.getComponent<TransformComponent>().position.x == doctest::Approx(1.0f));

    // Sleep para que la resolucion del filesystem reporte un mtime nuevo.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    {
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        f << R"(
            function onUpdate(self, dt)
                self.transform.position.x = 2.0
            end
        )";
    }

    // dt >= 0.5s dispara el throttle del check de mtime en este mismo update.
    sys.update(scene, 1.0f);
    CHECK(e.getComponent<TransformComponent>().position.x == doctest::Approx(2.0f));

    std::filesystem::remove(path);
}

TEST_CASE("ScriptSystem: script con error guarda lastError y no crashea") {
    const auto path = writeTempScript("esto no es lua valido @#$");

    Scene scene;
    Entity e = scene.createEntity("broken");
    e.addComponent<ScriptComponent>(path.generic_string());

    ScriptSystem sys;
    sys.update(scene, 0.016f);

    const auto& sc = e.getComponent<ScriptComponent>();
    CHECK_FALSE(sc.loaded);
    CHECK_FALSE(sc.lastError.empty());

    std::filesystem::remove(path);
}

// ============================================================
// F2H48: bindings de la tabla `dialog`
// ============================================================

#include "engine/dialog/DialogAsset.h"
#include "engine/dialog/DialogSystem.h"
#include "engine/game/state/GameState.h"

TEST_CASE("Lua dialog.set_var / get_var / has_var / clear_vars") {
    const auto path = writeTempScript(R"(
        function onUpdate(self, dt)
            dialog.set_var("met_guard", "true")
            dialog.set_var("gold", "42")
            self.transform.position.x = dialog.has_var("met_guard") and 1.0 or -1.0
            self.transform.position.y = tonumber(dialog.get_var("gold")) or 0
        end
    )");

    GameState::dialogVars().clear();
    Scene scene;
    Entity e = scene.createEntity("varuser");
    e.addComponent<ScriptComponent>(path.generic_string());

    ScriptSystem sys;
    sys.update(scene, 0.016f);

    CHECK(GameState::dialogVars()["met_guard"] == "true");
    CHECK(GameState::dialogVars()["gold"] == "42");
    CHECK(e.getComponent<TransformComponent>().position.x == doctest::Approx(1.0f));
    CHECK(e.getComponent<TransformComponent>().position.y == doctest::Approx(42.0f));

    std::filesystem::remove(path);
}

TEST_CASE("Lua dialog.isActive + advance + stop sobre asset en memoria") {
    // Construir asset minimo de 2 nodos.
    Dialog::Asset a;
    using namespace Mood::NodeGraph;
    const NodeId n1 = a.graph().addNode(Dialog::k_typeDialogLine, {0, 0});
    const NodeId n2 = a.graph().addNode(Dialog::k_typeDialogLine, {1, 0});
    a.graph().addSocket(n1, SocketKind::Input,  Dialog::k_socketFlow, "in");
    const SocketId n1Out = a.graph().addSocket(n1, SocketKind::Output, Dialog::k_socketFlow, "continue");
    const SocketId n2In  = a.graph().addSocket(n2, SocketKind::Input,  Dialog::k_socketFlow, "in");
    a.graph().addSocket(n2, SocketKind::Output, Dialog::k_socketFlow, "continue");
    a.graph().addLink(n1Out, n2In);
    Dialog::Line l; l.text_literal = "Hi"; a.writeLine(n1, l);
    Dialog::Line l2; l2.text_literal = "Bye"; a.writeLine(n2, l2);
    a.metadata().start_node_id = n1;

    Dialog::DialogSystem::reset();
    Dialog::DialogSystem::clearHooks();
    REQUIRE(Dialog::DialogSystem::start(&a));

    const auto path = writeTempScript(R"(
        was_active = false
        function onUpdate(self, dt)
            was_active = dialog.isActive()
            -- Avanzar al siguiente nodo
            dialog.continueNext()
            -- Tras el continue, el nodo n2 deberia seguir activo
            self.transform.position.x = dialog.isActive() and 1.0 or 0.0
            -- Stop manual
            dialog.stop()
            self.transform.position.y = dialog.isActive() and 1.0 or 0.0
        end
    )");

    Scene scene;
    Entity e = scene.createEntity("driver");
    e.addComponent<ScriptComponent>(path.generic_string());

    ScriptSystem sys;
    sys.update(scene, 0.016f);

    CHECK(e.getComponent<TransformComponent>().position.x == doctest::Approx(1.0f));  // continueNext OK
    CHECK(e.getComponent<TransformComponent>().position.y == doctest::Approx(0.0f));  // stop OK
    CHECK_FALSE(Dialog::DialogSystem::isActive());

    std::filesystem::remove(path);
}

TEST_CASE("Lua dialog.start sin AssetManager (test mode) retorna false + log warn") {
    Dialog::DialogSystem::reset();
    Dialog::DialogSystem::clearHooks();

    const auto path = writeTempScript(R"(
        function onUpdate(self, dt)
            local ok = dialog.start("dialogs/inexistente.mooddialog")
            self.transform.position.x = ok and 1.0 or -1.0
        end
    )");

    Scene scene;
    Entity e = scene.createEntity("starter");
    e.addComponent<ScriptComponent>(path.generic_string());

    ScriptSystem sys;
    sys.update(scene, 0.016f);  // sin AssetManager -> dialog.start retorna false

    CHECK(e.getComponent<TransformComponent>().position.x == doctest::Approx(-1.0f));
    CHECK_FALSE(Dialog::DialogSystem::isActive());

    std::filesystem::remove(path);
}
