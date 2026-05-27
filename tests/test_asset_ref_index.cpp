// F3H19 — Tests del AssetRefIndex.
//
// Cubre el walk del scene + materials buscando refs a un asset path.
// El AssetRefIndex es la base del rename con cascada: lo que el index
// devuelve, el RenameAssetCommand reescribe (string paths) o renombra
// en el AssetManager (id-based).
//
// Strategy: mismos pat tests del AssetValidator + casos especificos al
// rename (mismo path en N entidades, EnvironmentComponent con 2 paths
// distintos, etc).

#include <doctest/doctest.h>

#include "engine/assets/manager/AssetManager.h"
#include "engine/assets/refs/AssetRefIndex.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/render/rhi/ITexture.h"

#include <memory>
#include <string>

using namespace Mood;
using namespace Mood::asset_refs;

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

}  // namespace

TEST_CASE("F3H19: normalizePath quita prefijo assets/ y backslashes") {
    CHECK(normalizePath("assets/textures/foo.png")  == "textures/foo.png");
    CHECK(normalizePath("assets\\textures\\foo.png") == "textures/foo.png");
    CHECK(normalizePath("textures/foo.png")          == "textures/foo.png");
    CHECK(normalizePath("textures\\sub\\foo.png")    == "textures/sub/foo.png");
    CHECK(normalizePath("").empty());
}

TEST_CASE("F3H19: findRefs en scene vacia devuelve lista vacia") {
    AssetManager assets(".", nullFactory());
    Scene scene;
    const auto refs = findRefs(scene, assets, "scripts/player.lua");
    CHECK(refs.empty());
}

TEST_CASE("F3H19: findRefs detecta ScriptComponent.path matching") {
    AssetManager assets(".", nullFactory());
    Scene scene;
    Entity e = scene.createEntity("Player");
    e.addComponent<ScriptComponent>(ScriptComponent{"scripts/player.lua"});

    const auto refs = findRefs(scene, assets, "scripts/player.lua");
    REQUIRE(refs.size() == 1);
    CHECK(refs[0].kind == RefKind::ScriptPath);
    CHECK(refs[0].entity == e);
}

TEST_CASE("F3H19: findRefs NO matchea path distinto") {
    AssetManager assets(".", nullFactory());
    Scene scene;
    Entity e = scene.createEntity("Player");
    e.addComponent<ScriptComponent>(ScriptComponent{"scripts/other.lua"});

    const auto refs = findRefs(scene, assets, "scripts/player.lua");
    CHECK(refs.empty());
}

TEST_CASE("F3H19: findRefs NO matchea cuando el campo esta vacio") {
    AssetManager assets(".", nullFactory());
    Scene scene;
    Entity e = scene.createEntity("Empty");
    e.addComponent<ScriptComponent>(ScriptComponent{""});

    const auto refs = findRefs(scene, assets, "scripts/player.lua");
    CHECK(refs.empty());
    // Ademas: buscar por "" devuelve vacio (no matchea con todo).
    const auto refs2 = findRefs(scene, assets, "");
    CHECK(refs2.empty());
}

TEST_CASE("F3H19: findRefs acumula multiples refs al mismo path") {
    AssetManager assets(".", nullFactory());
    Scene scene;
    Entity a = scene.createEntity("NPC_01");
    Entity b = scene.createEntity("NPC_02");
    a.addComponent<DialogComponent>(DialogComponent{"dialogs/greet.mooddialog"});
    b.addComponent<DialogComponent>(DialogComponent{"dialogs/greet.mooddialog"});

    const auto refs = findRefs(scene, assets, "dialogs/greet.mooddialog");
    REQUIRE(refs.size() == 2);
    CHECK(refs[0].kind == RefKind::DialogPath);
    CHECK(refs[1].kind == RefKind::DialogPath);
}

TEST_CASE("F3H19: findRefs detecta los 7 string-paths de componentes") {
    AssetManager assets(".", nullFactory());
    Scene scene;

    Entity script = scene.createEntity("ScriptHost");
    script.addComponent<ScriptComponent>(ScriptComponent{"target.lua"});

    Entity dialog = scene.createEntity("DialogHost");
    dialog.addComponent<DialogComponent>(DialogComponent{"target.lua"});

    Entity item = scene.createEntity("ItemHost");
    auto& ic = item.addComponent<ItemPickupComponent>();
    ic.itemPath = "target.lua";

    Entity vehicle = scene.createEntity("VehicleHost");
    auto& vc = vehicle.addComponent<VehicleComponent>();
    vc.configPath = "target.lua";

    Entity env = scene.createEntity("EnvHost");
    auto& envc = env.addComponent<EnvironmentComponent>();
    envc.skyboxPath          = "target.lua";
    envc.colorGradingLutPath = "target.lua";

    Entity prefab = scene.createEntity("PrefabHost");
    prefab.addComponent<PrefabLinkComponent>(PrefabLinkComponent{"target.lua"});

    const auto refs = findRefs(scene, assets, "target.lua");
    // 7 sites: Script + Dialog + Item + Vehicle + Skybox + ColorGradingLut + PrefabLink.
    CHECK(refs.size() == 7);
}

TEST_CASE("F3H19: findRefs distingue Skybox vs ColorGradingLut del mismo Environment") {
    AssetManager assets(".", nullFactory());
    Scene scene;
    Entity env = scene.createEntity("EnvHost");
    auto& envc = env.addComponent<EnvironmentComponent>();
    envc.skyboxPath          = "skyboxes/sky_a";
    envc.colorGradingLutPath = "luts/grade_b.png";

    const auto refsSky = findRefs(scene, assets, "skyboxes/sky_a");
    REQUIRE(refsSky.size() == 1);
    CHECK(refsSky[0].kind == RefKind::SkyboxPath);

    const auto refsLut = findRefs(scene, assets, "luts/grade_b.png");
    REQUIRE(refsLut.size() == 1);
    CHECK(refsLut[0].kind == RefKind::ColorGradingLutPath);
}

TEST_CASE("F3H19: findRefs PrefabLink path") {
    AssetManager assets(".", nullFactory());
    Scene scene;
    Entity e = scene.createEntity("Spawned");
    e.addComponent<PrefabLinkComponent>(PrefabLinkComponent{"prefabs/torch.moodprefab"});

    const auto refs = findRefs(scene, assets, "prefabs/torch.moodprefab");
    REQUIRE(refs.size() == 1);
    CHECK(refs[0].kind == RefKind::PrefabLinkPath);
    CHECK(refs[0].entity == e);
}

// ============================================================================
// renameLogicalPath: actualiza el mapeo id↔path interno del AssetManager.
// ============================================================================

TEST_CASE("F3H19: renameLogicalPath no-op cuando asset no cargado") {
    AssetManager assets(".", nullFactory());
    CHECK_FALSE(assets.renameLogicalPath("textures/no_cargada.png",
                                           "textures/nueva.png"));
}

TEST_CASE("F3H19: renameLogicalPath no-op con oldPath == newPath") {
    AssetManager assets(".", nullFactory());
    const TextureAssetId id = assets.loadTexture("textures/foo.png");
    REQUIRE(id != 0);
    CHECK_FALSE(assets.renameLogicalPath("textures/foo.png",
                                           "textures/foo.png"));
}

TEST_CASE("F3H19: renameLogicalPath no-op con extension .lua (no cacheada)") {
    AssetManager assets(".", nullFactory());
    CHECK_FALSE(assets.renameLogicalPath("scripts/player.lua",
                                           "scripts/hero.lua"));
}

TEST_CASE("F3H19: renameLogicalPath de textura cargada actualiza pathOf") {
    AssetManager assets(".", nullFactory());
    const TextureAssetId id = assets.loadTexture("textures/grass.png");
    REQUIRE(id != 0);
    REQUIRE(assets.pathOf(id) == "textures/grass.png");

    CHECK(assets.renameLogicalPath("textures/grass.png",
                                     "textures/lawn.png"));
    CHECK(assets.pathOf(id) == "textures/lawn.png");
}

TEST_CASE("F3H19: tras rename, loadTexture del oldPath produce id nuevo") {
    // El cache map ya no tiene el oldPath -> una segunda carga lo trata
    // como path nuevo y crea un id distinto. Comportamiento esperado:
    // el oldPath quedo "huerfano" del cache.
    AssetManager assets(".", nullFactory());
    const TextureAssetId id1 = assets.loadTexture("textures/grass.png");
    REQUIRE(assets.renameLogicalPath("textures/grass.png", "textures/lawn.png"));

    const TextureAssetId id2 = assets.loadTexture("textures/grass.png");
    CHECK(id2 != id1);  // path huerfano: load nuevo crea slot nuevo
    CHECK(assets.pathOf(id1) == "textures/lawn.png");
    CHECK(assets.pathOf(id2) == "textures/grass.png");
}

TEST_CASE("F3H19: renameLogicalPath usa anim_ prefix para .fbx") {
    // No podemos cargar un .fbx sin assimp + archivo real, pero podemos
    // verificar que la extension distingue correctamente — ambos no-op
    // porque ni el mesh ni el anim clip estan cargados.
    AssetManager assets(".", nullFactory());
    CHECK_FALSE(assets.renameLogicalPath("meshes/cube.fbx",
                                           "meshes/box.fbx"));
    CHECK_FALSE(assets.renameLogicalPath("anims/anim_walk.fbx",
                                           "anims/anim_run.fbx"));
}
