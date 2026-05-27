#include "engine/assets/validation/AssetValidator.h"

#include "engine/assets/manager/AssetManager.h"
#include "engine/render/resources/MaterialAsset.h"
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <filesystem>
#include <string>
#include <utility>

namespace Mood::asset_validation {

namespace {

// Detalles como i18n keys (resueltos por el panel UI).
constexpr const char* kDetailMissingScript     = "editor.asset_validator.detail.script";
constexpr const char* kDetailMissingDialog     = "editor.asset_validator.detail.dialog";
constexpr const char* kDetailMissingItem       = "editor.asset_validator.detail.item";
constexpr const char* kDetailMissingVehicle    = "editor.asset_validator.detail.vehicle";
constexpr const char* kDetailMissingSkybox     = "editor.asset_validator.detail.skybox";
constexpr const char* kDetailMissingAudio      = "editor.asset_validator.detail.audio";
constexpr const char* kDetailMissingMesh       = "editor.asset_validator.detail.mesh";
constexpr const char* kDetailMissingMaterial   = "editor.asset_validator.detail.material";
constexpr const char* kDetailMissingAnimClip   = "editor.asset_validator.detail.anim_clip";
constexpr const char* kDetailMissingPartTex    = "editor.asset_validator.detail.particle_texture";
constexpr const char* kDetailMissingMatTexture = "editor.asset_validator.detail.material_texture";

// Sentineles del AssetManager: paths sinteticos para los slots 0 (fallback).
// Si un `pathOf(id)` arranca con "__", el path es interno y no apunta a un
// archivo real. NO reportamos esos como BrokenRef — son referencias
// legitimas al fallback (id=0 default).
bool isSentinelPath(const std::string& path) {
    return path.size() >= 2 && path[0] == '_' && path[1] == '_';
}

bool pathExistsInProject(const AssetManager& assets, const std::string& logicalPath) {
    if (logicalPath.empty() || isSentinelPath(logicalPath)) return true;
    std::filesystem::path fs = assets.resolvePath(logicalPath);
    if (fs.empty()) return false;
    std::error_code ec;
    return std::filesystem::exists(fs, ec);
}

// Skybox: el `skyboxPath` puede ser equirect (`<path>.png`) o cubemap dir
// (`<path>/px.png`). Si ninguno existe -> roto. Replica la heuristica de
// `SceneRenderer::loadSkyboxAndIblFromBase` para reportar igual al render.
bool skyboxExistsInProject(const AssetManager& assets, const std::string& base) {
    if (base.empty() || isSentinelPath(base)) return true;
    std::filesystem::path root = assets.resolvePath(base);
    if (root.empty()) return false;
    std::error_code ec;
    // Equirect: <base>.png
    if (std::filesystem::exists(root.string() + ".png", ec)) return true;
    // Cubemap dir: <base>/px.png (las otras 5 caras se asumen presentes).
    if (std::filesystem::exists(root / "px.png", ec)) return true;
    return false;
}

std::string entityTagOrFallback(Entity e) {
    if (!e || !e.hasComponent<TagComponent>()) return "(sin tag)";
    const auto& tag = e.getComponent<TagComponent>();
    return tag.name.empty() ? std::string{"(sin tag)"} : tag.name;
}

AssetIssue makeIssue(IssueKind kind, std::string assetPath,
                       const char* detailKey,
                       std::string usedBy, Entity e) {
    AssetIssue issue;
    issue.kind = kind;
    issue.assetPath = std::move(assetPath);
    issue.detail = detailKey;
    issue.usedBy = std::move(usedBy);
    issue.entity = e;
    return issue;
}

// Chequea un AssetId generico: si != 0 y `pathOf(id)` no es sentinel pero
// no resuelve en disco, reporta BrokenRef. El callback `getPath` evita
// tener que pasar punteros a metodos miembros del AssetManager.
template<typename GetPathFn>
void checkAssetId(u32 id, GetPathFn getPath,
                   const AssetManager& assets,
                   Entity e, const char* detailKey,
                   std::vector<AssetIssue>& out) {
    if (id == 0) return;  // slot 0 = fallback legitimo (sin asignar)
    std::string path = getPath(id);
    if (isSentinelPath(path)) return;  // path sintetico (createMaterialFromTexture etc)
    if (pathExistsInProject(assets, path)) return;
    out.push_back(makeIssue(IssueKind::BrokenRef, path, detailKey,
                             entityTagOrFallback(e), e));
}

}  // namespace

std::vector<AssetIssue> validateProject(Scene& scene,
                                          const AssetManager& assets) {
    std::vector<AssetIssue> out;
    out.reserve(8);

    // --- Componentes con campo `path` string ---
    scene.forEach<ScriptComponent>([&](Entity e, ScriptComponent& sc) {
        if (!sc.path.empty() && !pathExistsInProject(assets, sc.path)) {
            out.push_back(makeIssue(IssueKind::BrokenRef, sc.path,
                                      kDetailMissingScript,
                                      entityTagOrFallback(e), e));
        }
    });

    scene.forEach<DialogComponent>([&](Entity e, DialogComponent& dc) {
        if (!dc.dialogPath.empty()
            && !pathExistsInProject(assets, dc.dialogPath)) {
            out.push_back(makeIssue(IssueKind::BrokenRef, dc.dialogPath,
                                      kDetailMissingDialog,
                                      entityTagOrFallback(e), e));
        }
    });

    scene.forEach<ItemPickupComponent>([&](Entity e, ItemPickupComponent& ic) {
        if (!ic.itemPath.empty()
            && !pathExistsInProject(assets, ic.itemPath)) {
            out.push_back(makeIssue(IssueKind::BrokenRef, ic.itemPath,
                                      kDetailMissingItem,
                                      entityTagOrFallback(e), e));
        }
    });

    scene.forEach<VehicleComponent>([&](Entity e, VehicleComponent& vc) {
        if (!vc.configPath.empty()
            && !pathExistsInProject(assets, vc.configPath)) {
            out.push_back(makeIssue(IssueKind::BrokenRef, vc.configPath,
                                      kDetailMissingVehicle,
                                      entityTagOrFallback(e), e));
        }
    });

    scene.forEach<EnvironmentComponent>([&](Entity e, EnvironmentComponent& env) {
        if (!env.skyboxPath.empty()
            && !skyboxExistsInProject(assets, env.skyboxPath)) {
            out.push_back(makeIssue(IssueKind::BrokenRef, env.skyboxPath,
                                      kDetailMissingSkybox,
                                      entityTagOrFallback(e), e));
        }
    });

    // --- Componentes con AssetId resoluble a path via AssetManager ---
    scene.forEach<AudioSourceComponent>([&](Entity e, AudioSourceComponent& a) {
        checkAssetId(a.clip,
                      [&](u32 id) { return assets.audioPathOf(id); },
                      assets, e, kDetailMissingAudio, out);
    });

    scene.forEach<MeshRendererComponent>([&](Entity e, MeshRendererComponent& mr) {
        checkAssetId(mr.mesh,
                      [&](u32 id) { return assets.meshPathOf(id); },
                      assets, e, kDetailMissingMesh, out);
        for (MaterialAssetId matId : mr.materials) {
            checkAssetId(matId,
                          [&](u32 id) { return assets.materialPathOf(id); },
                          assets, e, kDetailMissingMaterial, out);
        }
    });

    scene.forEach<AnimatorComponent>([&](Entity e, AnimatorComponent& a) {
        for (const auto& [alias, clipId] : a.externalClips) {
            (void)alias;
            checkAssetId(clipId,
                          [&](u32 id) { return assets.animationClipPathOf(id); },
                          assets, e, kDetailMissingAnimClip, out);
        }
    });

    scene.forEach<ParticleEmitterComponent>([&](Entity e, ParticleEmitterComponent& p) {
        checkAssetId(p.texture,
                      [&](u32 id) { return assets.pathOf(id); },
                      assets, e, kDetailMissingPartTex, out);
    });

    scene.forEach<BrushComponent>([&](Entity e, BrushComponent& bc) {
        for (MaterialAssetId matId : bc.materials) {
            checkAssetId(matId,
                          [&](u32 id) { return assets.materialPathOf(id); },
                          assets, e, kDetailMissingMaterial, out);
        }
    });

    // --- Materials cargados: chequear sus texture refs ---
    // Slot 0 es __default_material (skip). Cada material > 0 puede referir
    // a 4 texturas (albedo / MR / normal / ao). Si la textura no es 0 y
    // su `pathOf` no resuelve en disco, reportamos con `usedBy = "Material: <path>"`.
    for (usize i = 1; i < assets.materialCount(); ++i) {
        const auto matId = static_cast<MaterialAssetId>(i);
        const MaterialAsset* mat = assets.getMaterial(matId);
        if (mat == nullptr) continue;
        const std::string matPath = assets.materialPathOf(matId);
        if (isSentinelPath(matPath)) continue;  // material runtime/sintetico

        auto checkMatTex = [&](TextureAssetId texId) {
            if (texId == 0) return;
            std::string texPath = assets.pathOf(texId);
            if (isSentinelPath(texPath)) return;
            if (pathExistsInProject(assets, texPath)) return;
            AssetIssue issue;
            issue.kind = IssueKind::BrokenRef;
            issue.assetPath = std::move(texPath);
            issue.detail = kDetailMissingMatTexture;
            issue.usedBy = "Material: " + matPath;
            // `entity` queda falsy — el panel maneja go-to "asset" en vez
            // de "entity" cuando entity es invalido.
            out.push_back(std::move(issue));
        };
        checkMatTex(mat->albedo);
        checkMatTex(mat->metallicRoughness);
        checkMatTex(mat->normal);
        checkMatTex(mat->ao);
    }

    return out;
}

}  // namespace Mood::asset_validation
