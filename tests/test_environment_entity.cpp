// F2H86 — Tests del Environment como entidad de primera clase.
//
// Cubre:
//  - Default del skyboxPath (cambio F2H86: era sky_day, ahora sky_kloofendal).
//  - Icono dedicado en IconHelpers (ICON_FA_GLOBE).
//  - Roundtrip JSON de skyboxPath preservando paths del catalogo.
//
// El swap real del SkyboxRenderer + IBL bake en runtime requiere GL
// context (creacion de cubemaps / texturas), por lo que NO se cubre
// aca — la unica garantia testeable headless es que el dato persiste y
// llega al renderer. La validacion visual del swap es responsabilidad
// del dev en la sesion de cierre (Inspector -> dropdown -> ver cielo
// cambiar).

#include <doctest/doctest.h>

#include "editor/ui/IconHelpers.h"
#include "editor/ui/IconsFontAwesome6.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/serialization/SceneSerializer.h"
#include "engine/world/grid/GridMap.h"
#include "test_scene_serializer_helpers.h"

#include <cstring>
#include <filesystem>

using namespace Mood;
using Mood::SceneSerializerTests::nullFactory;
using Mood::SceneSerializerTests::tempPath;

TEST_CASE("F2H86: EnvironmentComponent default skyboxPath es sky_kloofendal") {
    // Regresion: pre-F2H86 era 'skyboxes/sky_day'. El SceneRenderer
    // hardcodeaba kloofendal pero el componente decia day -> mismatch
    // silencioso. Hoy ambos estan alineados con el cargado en init.
    EnvironmentComponent env{};
    CHECK(env.skyboxPath == "skyboxes/sky_kloofendal");
}

TEST_CASE("F2H86: iconForEntity de un Environment es ICON_FA_GLOBE") {
    Scene scene;
    Entity e = scene.createEntity("Environment");
    e.addComponent<EnvironmentComponent>(EnvironmentComponent{});

    const char* icon = iconForEntity(e);
    REQUIRE(icon != nullptr);
    CHECK(std::strcmp(icon, ICON_FA_GLOBE) == 0);
}

TEST_CASE("F2H86: iconForEntity prioriza Environment sobre MeshRenderer") {
    // Defensivo: si una entidad mantiene ambos (caso raro pero posible
    // al migrar una entidad vieja con placeholder cubo + Environment),
    // el rol es 'Environment global', no 'geometria'.
    Scene scene;
    Entity e = scene.createEntity("MixedEnv");
    e.addComponent<MeshRendererComponent>(0u, 0u);
    e.addComponent<EnvironmentComponent>(EnvironmentComponent{});

    CHECK(std::strcmp(iconForEntity(e), ICON_FA_GLOBE) == 0);
}

TEST_CASE("F2H86: roundtrip JSON preserva skyboxPath de los presets") {
    AssetManager assets("assets", nullFactory());
    GridMap empty(1u, 1u, 1.0f);

    auto roundtrip = [&](const std::string& path) {
        Scene scene;
        Entity env = scene.createEntity("Environment");
        EnvironmentComponent data{};
        data.skyboxPath = path;
        env.addComponent<EnvironmentComponent>(data);

        const auto filePath = tempPath("env_skybox_roundtrip.moodmap");
        SceneSerializer::save(empty, "demo", &scene, assets, filePath);
        const auto loaded = SceneSerializer::load(filePath, assets);
        std::filesystem::remove(filePath);
        REQUIRE(loaded.has_value());
        REQUIRE(loaded->entities.size() == 1);
        REQUIRE(loaded->entities[0].environment.has_value());
        return loaded->entities[0].environment->skyboxPath;
    };

    CHECK(roundtrip("skyboxes/sky_kloofendal") == "skyboxes/sky_kloofendal");
    CHECK(roundtrip("skyboxes/sky_day")        == "skyboxes/sky_day");
    CHECK(roundtrip("skyboxes/custom_hdri")    == "skyboxes/custom_hdri");
}
