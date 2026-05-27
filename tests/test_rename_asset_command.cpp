// F3H19 — Tests del RenameAssetCommand.
//
// Cubre el flow end-to-end del rename con cascada: snapshot de refs +
// fs::rename + AssetManager.renameLogicalPath + reescritura de refs en
// componentes. Undo debe revertir las 3 mutaciones.

#include <doctest/doctest.h>

#include "editor/commands/RenameAssetCommand.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/assets/refs/AssetRefIndex.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/render/rhi/ITexture.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

using namespace Mood;

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

struct TempRoot {
    std::filesystem::path path;

    explicit TempRoot(const char* tag) {
        path = std::filesystem::temp_directory_path() /
               (std::string("moodengine_rename_") + tag);
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
        std::filesystem::create_directories(path);
    }
    ~TempRoot() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    std::filesystem::path plant(const std::string& relative) {
        const auto full = path / relative;
        std::filesystem::create_directories(full.parent_path());
        std::ofstream(full).put('x');
        return full;
    }
};

}  // namespace

TEST_CASE("F3H19: RenameAssetCommand renombra archivo + ScriptComponent.path") {
    TempRoot root("script");
    const auto oldDisk = root.plant("scripts/player.lua");
    const auto newDisk = root.path / "scripts/hero.lua";

    AssetManager assets(root.path.string(), nullFactory());
    Scene scene;
    Entity e = scene.createEntity("Player");
    e.addComponent<ScriptComponent>(ScriptComponent{"scripts/player.lua"});

    auto refs = asset_refs::findRefs(scene, assets, "scripts/player.lua");
    REQUIRE(refs.size() == 1);

    RenameAssetCommand cmd(&scene, &assets, oldDisk, newDisk,
                            "scripts/player.lua", "scripts/hero.lua",
                            std::move(refs));
    cmd.execute();

    CHECK_FALSE(std::filesystem::exists(oldDisk));
    CHECK(std::filesystem::exists(newDisk));
    CHECK(e.getComponent<ScriptComponent>().path == "scripts/hero.lua");

    cmd.undo();
    CHECK(std::filesystem::exists(oldDisk));
    CHECK_FALSE(std::filesystem::exists(newDisk));
    CHECK(e.getComponent<ScriptComponent>().path == "scripts/player.lua");
}

TEST_CASE("F3H19: RenameAssetCommand cascada a multiples refs") {
    TempRoot root("multi");
    const auto oldDisk = root.plant("dialogs/greet.mooddialog");
    const auto newDisk = root.path / "dialogs/hello.mooddialog";

    AssetManager assets(root.path.string(), nullFactory());
    Scene scene;
    Entity a = scene.createEntity("NPC_01");
    Entity b = scene.createEntity("NPC_02");
    Entity c = scene.createEntity("NPC_03");
    a.addComponent<DialogComponent>(DialogComponent{"dialogs/greet.mooddialog"});
    b.addComponent<DialogComponent>(DialogComponent{"dialogs/greet.mooddialog"});
    c.addComponent<DialogComponent>(DialogComponent{"dialogs/greet.mooddialog"});

    auto refs = asset_refs::findRefs(scene, assets, "dialogs/greet.mooddialog");
    REQUIRE(refs.size() == 3);

    RenameAssetCommand cmd(&scene, &assets, oldDisk, newDisk,
                            "dialogs/greet.mooddialog",
                            "dialogs/hello.mooddialog",
                            std::move(refs));
    cmd.execute();

    CHECK(a.getComponent<DialogComponent>().dialogPath == "dialogs/hello.mooddialog");
    CHECK(b.getComponent<DialogComponent>().dialogPath == "dialogs/hello.mooddialog");
    CHECK(c.getComponent<DialogComponent>().dialogPath == "dialogs/hello.mooddialog");

    cmd.undo();
    CHECK(a.getComponent<DialogComponent>().dialogPath == "dialogs/greet.mooddialog");
    CHECK(b.getComponent<DialogComponent>().dialogPath == "dialogs/greet.mooddialog");
    CHECK(c.getComponent<DialogComponent>().dialogPath == "dialogs/greet.mooddialog");
}

TEST_CASE("F3H19: RenameAssetCommand actualiza AssetManager pathOf para textura cargada") {
    TempRoot root("tex_path");
    const auto oldDisk = root.plant("textures/grass.png");
    const auto newDisk = root.path / "textures/lawn.png";

    AssetManager assets(root.path.string(), nullFactory());
    Scene scene;
    const TextureAssetId texId = assets.loadTexture("textures/grass.png");
    REQUIRE(texId != 0);
    REQUIRE(assets.pathOf(texId) == "textures/grass.png");

    // Sin refs en scene — solo el AssetManager cache + disk.
    std::vector<asset_refs::RefSite> refs;
    RenameAssetCommand cmd(&scene, &assets, oldDisk, newDisk,
                            "textures/grass.png", "textures/lawn.png",
                            std::move(refs));
    cmd.execute();
    CHECK(assets.pathOf(texId) == "textures/lawn.png");

    cmd.undo();
    CHECK(assets.pathOf(texId) == "textures/grass.png");
}

TEST_CASE("F3H19: RenameAssetCommand con SkyboxPath + ColorGradingLutPath") {
    TempRoot root("env");
    const auto oldDisk = root.plant("luts/grade_a.png");
    const auto newDisk = root.path / "luts/grade_b.png";

    AssetManager assets(root.path.string(), nullFactory());
    Scene scene;
    Entity env = scene.createEntity("Env");
    auto& envc = env.addComponent<EnvironmentComponent>();
    envc.skyboxPath          = "luts/grade_a.png";  // mismo path en ambos campos
    envc.colorGradingLutPath = "luts/grade_a.png";

    auto refs = asset_refs::findRefs(scene, assets, "luts/grade_a.png");
    REQUIRE(refs.size() == 2);  // skybox + LUT en la misma entidad

    RenameAssetCommand cmd(&scene, &assets, oldDisk, newDisk,
                            "luts/grade_a.png", "luts/grade_b.png",
                            std::move(refs));
    cmd.execute();
    CHECK(env.getComponent<EnvironmentComponent>().skyboxPath == "luts/grade_b.png");
    CHECK(env.getComponent<EnvironmentComponent>().colorGradingLutPath == "luts/grade_b.png");

    cmd.undo();
    CHECK(env.getComponent<EnvironmentComponent>().skyboxPath == "luts/grade_a.png");
    CHECK(env.getComponent<EnvironmentComponent>().colorGradingLutPath == "luts/grade_a.png");
}

// Nota: la validación "newDiskPath no debe existir" es responsabilidad del
// caller (Stage D / UI del Asset Browser), no del Command — `fs::rename` en
// Windows sobreescribe el destino, así que si el caller no chequea antes el
// dev pierde el archivo destino sin darse cuenta. El modal de rename del
// Asset Browser hace el check antes de construir el comando.

TEST_CASE("F3H19: RenameAssetCommand.name() incluye old y new") {
    AssetManager assets(".", nullFactory());
    Scene scene;
    RenameAssetCommand cmd(&scene, &assets, {}, {},
                            "a.lua", "b.lua", {});
    const std::string n = cmd.name();
    CHECK(n.find("a.lua") != std::string::npos);
    CHECK(n.find("b.lua") != std::string::npos);
}
