// Tests del helper `SceneLoader::applyEntitiesToScene` (Hito 22).
// El foco son los upgrades automaticos al cargar un map persistido:
//   - meshes con esqueleto + clips reciben AnimatorComponent +
//     SkeletonComponent automaticamente (fix arrastrado del Hito 19).
//   - back-compat de paths v6 (texturas en lugar de materials).
//
// Headless: usamos un AssetManager con factories null y un SavedMap
// armado a mano sin pasar por SceneSerializer.

#include <doctest/doctest.h>

#include "engine/assets/manager/AssetManager.h"
#include "engine/render/rhi/ITexture.h"
#include "engine/render/resources/MeshAsset.h"
#include "engine/world/GridMap.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/serialization/SceneLoader.h"
#include "engine/scene/serialization/SceneSerializer.h"

#include <filesystem>
#include <memory>
#include <string>

using namespace Mood;

namespace {

// AssetManager configurado para los tests: factories minimas que
// devuelven nullptr — el AssetManager las llama pero los tests no
// dependen del retorno (solo nos importan los componentes ECS).
std::unique_ptr<AssetManager> makeAssets() {
    auto texFactory = [](const std::string&) -> std::unique_ptr<ITexture> {
        return nullptr;
    };
    return std::make_unique<AssetManager>(
        "assets",
        AssetManager::TextureFactory(texFactory),
        AssetManager::AudioClipFactory{},
        AssetManager::MeshFactory{});
}

} // namespace

TEST_CASE("SceneLoader: mesh con skeleton + clips gana Animator + Skeleton") {
    auto assets = makeAssets();
    Scene scene;

    SavedMap saved{"test", GridMap(1u, 1u), {}};
    SavedEntity se{};
    se.tag      = "fox_persistido";
    se.position = glm::vec3(0.0f);
    se.scale    = glm::vec3(0.01f);
    SavedMeshRenderer mr{};
    mr.meshPath = "meshes/Fox.glb"; // existe en repo, trae 24 huesos + 3 clips
    se.meshRenderer = mr;
    saved.entities.push_back(se);

    SceneLoader::applyEntitiesToScene(saved, scene, *assets);

    // La entidad existe.
    Entity fox{};
    scene.forEach<TagComponent>([&](Entity e, TagComponent& t) {
        if (t.name == "fox_persistido") fox = e;
    });
    CHECK(static_cast<bool>(fox));

    // Auto-add: AnimatorComponent + SkeletonComponent.
    CHECK(fox.hasComponent<AnimatorComponent>());
    CHECK(fox.hasComponent<SkeletonComponent>());

    // Defaults razonables para que el AnimationSystem la reproduzca.
    if (fox.hasComponent<AnimatorComponent>()) {
        const auto& anim = fox.getComponent<AnimatorComponent>();
        CHECK(anim.playing);
        CHECK(anim.loop);
        CHECK(anim.clipName.empty()); // primer clip
    }
}

TEST_CASE("SceneLoader: mesh sin skeleton NO gana Animator/Skeleton") {
    auto assets = makeAssets();
    Scene scene;

    SavedMap saved{"test", GridMap(1u, 1u), {}};
    SavedEntity se{};
    se.tag = "pyramid_persistida";
    SavedMeshRenderer mr{};
    mr.meshPath = "meshes/pyramid.obj"; // existe en repo, no tiene skeleton
    se.meshRenderer = mr;
    saved.entities.push_back(se);

    SceneLoader::applyEntitiesToScene(saved, scene, *assets);

    Entity pyr{};
    scene.forEach<TagComponent>([&](Entity e, TagComponent& t) {
        if (t.name == "pyramid_persistida") pyr = e;
    });
    REQUIRE(static_cast<bool>(pyr));
    CHECK_FALSE(pyr.hasComponent<AnimatorComponent>());
    CHECK_FALSE(pyr.hasComponent<SkeletonComponent>());
}

TEST_CASE("SceneLoader: paths v6 (texturas en materials[]) se envuelven OK") {
    // Back-compat del Hito 17: si el SavedMeshRenderer trae paths que
    // no terminan en .material, se asume que son texturas y se envuelven
    // en un material wrapper via loadMaterialFromTexture.
    auto assets = makeAssets();
    Scene scene;

    SavedMap saved{"test", GridMap(1u, 1u), {}};
    SavedEntity se{};
    se.tag = "tile_v6";
    SavedMeshRenderer mr{};
    mr.meshPath = "meshes/pyramid.obj";
    mr.materials = {"textures/grid.png"}; // path v6 estilo
    se.meshRenderer = mr;
    saved.entities.push_back(se);

    SceneLoader::applyEntitiesToScene(saved, scene, *assets);

    Entity tile{};
    scene.forEach<TagComponent>([&](Entity e, TagComponent& t) {
        if (t.name == "tile_v6") tile = e;
    });
    REQUIRE(static_cast<bool>(tile));
    REQUIRE(tile.hasComponent<MeshRendererComponent>());
    const auto& meshR = tile.getComponent<MeshRendererComponent>();
    CHECK(meshR.materials.size() == 1);
    // El id no es 0 (missing) — fue creado via loadMaterialFromTexture.
    CHECK(meshR.materials[0] != assets->missingMaterialId());
}
