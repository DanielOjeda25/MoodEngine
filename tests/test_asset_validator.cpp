// F3H18 — Tests del AssetValidator.
//
// Cubre el flow Tier 1 del validator (refs muertas detectadas via
// string-paths de los componentes). Los AssetIds que apuntan al
// fallback no se cubren acá — eso requiere mocks completos del
// AssetManager (textura/material/etc), fuera del scope del headless.
//
// Convencion de paths: el AssetManager resuelve `logicalPath` contra
// su VFS root (constructor arg). El validator chequea
// `assets.resolvePath(path)` + `std::filesystem::exists`. Creamos un
// dir temporal y plantamos archivos placeholder para los "validos".

#include <doctest/doctest.h>

#include "engine/assets/manager/AssetManager.h"
#include "engine/assets/validation/AssetValidator.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/render/rhi/ITexture.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

using namespace Mood;
using namespace Mood::asset_validation;

namespace {

class NullTex : public ITexture {
public:
    explicit NullTex(std::string p) : m_p(std::move(p)) {}
    void bind(u32 = 0) const override {}
    void unbind() const override {}
    u32 width() const override { return 1; }
    u32 height() const override { return 1; }
    TextureHandle handle() const override { return nullptr; }
private:
    std::string m_p;
};

AssetManager::TextureFactory nullFactory() {
    return [](const std::string& p) { return std::make_unique<NullTex>(p); };
}

// Crea un dir temporal unico por test + opcionalmente planta archivos.
struct TempRoot {
    std::filesystem::path path;

    explicit TempRoot(const char* tag) {
        path = std::filesystem::temp_directory_path() /
               (std::string("moodengine_validator_") + tag);
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
        std::filesystem::create_directories(path);
    }
    ~TempRoot() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    void plant(const std::string& relative) {
        const auto full = path / relative;
        std::filesystem::create_directories(full.parent_path());
        std::ofstream(full).put('x');
    }
};

} // namespace

TEST_CASE("F3H18: validator no reporta issues en proyecto sin refs") {
    TempRoot root("clean");
    AssetManager assets(root.path.string(), nullFactory());
    Scene scene;

    auto issues = validateProject(scene, assets);
    CHECK(issues.empty());
}

TEST_CASE("F3H18: validator detecta ScriptComponent.path inexistente") {
    TempRoot root("script_missing");
    AssetManager assets(root.path.string(), nullFactory());
    Scene scene;

    Entity e = scene.createEntity("Player");
    e.addComponent<ScriptComponent>(ScriptComponent{"scripts/no_existe.lua"});

    auto issues = validateProject(scene, assets);
    REQUIRE(issues.size() == 1);
    CHECK(issues[0].kind == IssueKind::BrokenRef);
    CHECK(issues[0].assetPath == "scripts/no_existe.lua");
    CHECK(issues[0].usedBy == "Player");
    CHECK(issues[0].entity == e);
}

TEST_CASE("F3H18: validator NO reporta ScriptComponent.path que existe en disco") {
    TempRoot root("script_ok");
    root.plant("scripts/player.lua");
    AssetManager assets(root.path.string(), nullFactory());
    Scene scene;

    Entity e = scene.createEntity("Player");
    e.addComponent<ScriptComponent>(ScriptComponent{"scripts/player.lua"});

    auto issues = validateProject(scene, assets);
    CHECK(issues.empty());
}

TEST_CASE("F3H18: validator NO reporta ScriptComponent.path vacio") {
    // Path vacio = sin asignar — no es ref muerta, es no-ref.
    TempRoot root("script_empty");
    AssetManager assets(root.path.string(), nullFactory());
    Scene scene;

    Entity e = scene.createEntity("Player");
    e.addComponent<ScriptComponent>(ScriptComponent{""});

    auto issues = validateProject(scene, assets);
    CHECK(issues.empty());
}

TEST_CASE("F3H18: validator detecta DialogComponent + ItemPickupComponent rotos") {
    TempRoot root("dialog_item");
    AssetManager assets(root.path.string(), nullFactory());
    Scene scene;

    Entity npc = scene.createEntity("NPC_01");
    DialogComponent dc;
    dc.dialogPath = "dialogs/no_existe.mooddialog";
    npc.addComponent<DialogComponent>(dc);

    Entity pickup = scene.createEntity("CrateItem");
    ItemPickupComponent ic;
    ic.itemPath = "items/desaparecido.mooditem";
    pickup.addComponent<ItemPickupComponent>(ic);

    auto issues = validateProject(scene, assets);
    REQUIRE(issues.size() == 2);
    // Orden estable: scripts -> dialogs -> items, asi que dialog antes
    // del item segun el orden de iteracion del validator.
    CHECK(issues[0].assetPath == "dialogs/no_existe.mooddialog");
    CHECK(issues[1].assetPath == "items/desaparecido.mooditem");
}

TEST_CASE("F3H18: AssetIssue.detail es i18n key estable") {
    // El detail es la key (no el string traducido) — el panel UI resuelve.
    // Cambiar la key rompe el panel, asi que freezeamos el contrato.
    TempRoot root("detail_key");
    AssetManager assets(root.path.string(), nullFactory());
    Scene scene;

    Entity e = scene.createEntity("X");
    e.addComponent<ScriptComponent>(ScriptComponent{"scripts/x.lua"});

    auto issues = validateProject(scene, assets);
    REQUIRE(issues.size() == 1);
    CHECK(issues[0].detail == "editor.asset_validator.detail.script");
}

TEST_CASE("F3H18: validator detecta VehicleComponent.configPath roto") {
    TempRoot root("vehicle_missing");
    AssetManager assets(root.path.string(), nullFactory());
    Scene scene;

    Entity e = scene.createEntity("Car");
    VehicleComponent vc;
    vc.configPath = "vehicles/no_existe.moodvehicle";
    e.addComponent<VehicleComponent>(vc);

    auto issues = validateProject(scene, assets);
    REQUIRE(issues.size() == 1);
    CHECK(issues[0].kind == IssueKind::BrokenRef);
    CHECK(issues[0].assetPath == "vehicles/no_existe.moodvehicle");
    CHECK(issues[0].detail == "editor.asset_validator.detail.vehicle");
}
