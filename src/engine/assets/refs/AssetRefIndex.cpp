#include "engine/assets/refs/AssetRefIndex.h"

#include "engine/assets/manager/AssetManager.h"
#include "engine/render/resources/MaterialAsset.h"
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace Mood::asset_refs {

namespace {

bool isSentinelPath(const std::string& path) {
    return path.size() >= 2 && path[0] == '_' && path[1] == '_';
}

// Match exacto del path normalizado. Las paths en componentes y en el
// AssetManager se guardan tal cual el dev escribió — asumimos consistencia.
bool sameLogical(const std::string& a, const std::string& b) {
    return a == b;
}

// Para refs id-based: el id es != 0 (slot 0 = fallback), su `pathOf(id)`
// no es sentinel, y coincide con `target`.
template <typename GetPathFn>
bool idRefsTarget(u32 id, GetPathFn getPath, const std::string& target) {
    if (id == 0) return false;
    std::string p = getPath(id);
    if (isSentinelPath(p)) return false;
    return sameLogical(p, target);
}

}  // namespace

std::string normalizePath(const std::string& path) {
    std::string out;
    out.reserve(path.size());

    // Saltar prefijo "assets/" o "assets\\" si está al inicio.
    std::size_t start = 0;
    constexpr const char* kPrefix = "assets/";
    constexpr std::size_t kPrefixLen = 7;
    if (path.size() >= kPrefixLen &&
        (path.compare(0, kPrefixLen, kPrefix) == 0 ||
         path.compare(0, kPrefixLen, "assets\\") == 0)) {
        start = kPrefixLen;
    }

    for (std::size_t i = start; i < path.size(); ++i) {
        out.push_back(path[i] == '\\' ? '/' : path[i]);
    }
    return out;
}

std::vector<RefSite> findRefs(Scene& scene, const AssetManager& assets,
                                const std::string& assetLogicalPath) {
    std::vector<RefSite> out;
    out.reserve(8);
    if (assetLogicalPath.empty()) return out;

    auto addSite = [&](RefKind kind, Entity e,
                        usize slot = 0,
                        std::string animAlias = {}) {
        RefSite site;
        site.kind = kind;
        site.entity = e;
        site.slotIndex = slot;
        site.animAlias = std::move(animAlias);
        out.push_back(std::move(site));
    };

    // ----- String paths en componentes -----
    scene.forEach<ScriptComponent>([&](Entity e, ScriptComponent& sc) {
        if (sameLogical(sc.path, assetLogicalPath)) {
            addSite(RefKind::ScriptPath, e);
        }
    });

    scene.forEach<DialogComponent>([&](Entity e, DialogComponent& dc) {
        if (sameLogical(dc.dialogPath, assetLogicalPath)) {
            addSite(RefKind::DialogPath, e);
        }
    });

    scene.forEach<ItemPickupComponent>([&](Entity e, ItemPickupComponent& ic) {
        if (sameLogical(ic.itemPath, assetLogicalPath)) {
            addSite(RefKind::ItemPath, e);
        }
    });

    scene.forEach<VehicleComponent>([&](Entity e, VehicleComponent& vc) {
        if (sameLogical(vc.configPath, assetLogicalPath)) {
            addSite(RefKind::VehiclePath, e);
        }
    });

    scene.forEach<EnvironmentComponent>([&](Entity e, EnvironmentComponent& env) {
        if (sameLogical(env.skyboxPath, assetLogicalPath)) {
            addSite(RefKind::SkyboxPath, e);
        }
        if (sameLogical(env.colorGradingLutPath, assetLogicalPath)) {
            addSite(RefKind::ColorGradingLutPath, e);
        }
    });

    scene.forEach<PrefabLinkComponent>([&](Entity e, PrefabLinkComponent& pl) {
        if (sameLogical(pl.path, assetLogicalPath)) {
            addSite(RefKind::PrefabLinkPath, e);
        }
    });

    // ----- Refs id-based -----
    scene.forEach<AudioSourceComponent>([&](Entity e, AudioSourceComponent& a) {
        if (idRefsTarget(a.clip,
                          [&](u32 id) { return assets.audioPathOf(id); },
                          assetLogicalPath)) {
            addSite(RefKind::AudioClipId, e);
        }
    });

    scene.forEach<MeshRendererComponent>([&](Entity e, MeshRendererComponent& mr) {
        if (idRefsTarget(mr.mesh,
                          [&](u32 id) { return assets.meshPathOf(id); },
                          assetLogicalPath)) {
            addSite(RefKind::MeshId, e);
        }
        for (usize i = 0; i < mr.materials.size(); ++i) {
            if (idRefsTarget(mr.materials[i],
                              [&](u32 id) { return assets.materialPathOf(id); },
                              assetLogicalPath)) {
                addSite(RefKind::MeshMaterialSlot, e, i);
            }
        }
    });

    scene.forEach<AnimatorComponent>([&](Entity e, AnimatorComponent& a) {
        for (const auto& [alias, clipId] : a.externalClips) {
            if (idRefsTarget(clipId,
                              [&](u32 id) { return assets.animationClipPathOf(id); },
                              assetLogicalPath)) {
                addSite(RefKind::AnimatorExternalClip, e, 0, alias);
            }
        }
    });

    scene.forEach<ParticleEmitterComponent>([&](Entity e, ParticleEmitterComponent& p) {
        if (idRefsTarget(p.texture,
                          [&](u32 id) { return assets.pathOf(id); },
                          assetLogicalPath)) {
            addSite(RefKind::ParticleTextureId, e);
        }
    });

    scene.forEach<BrushComponent>([&](Entity e, BrushComponent& bc) {
        for (usize i = 0; i < bc.materials.size(); ++i) {
            if (idRefsTarget(bc.materials[i],
                              [&](u32 id) { return assets.materialPathOf(id); },
                              assetLogicalPath)) {
                addSite(RefKind::BrushMaterialSlot, e, i);
            }
        }
    });

    // ----- Texturas referenciadas por Material cargados -----
    // Slot 0 (__default_material) y materiales runtime (paths con __) saltados.
    for (usize i = 1; i < assets.materialCount(); ++i) {
        const auto matId = static_cast<MaterialAssetId>(i);
        const MaterialAsset* mat = assets.getMaterial(matId);
        if (mat == nullptr) continue;
        const std::string matPath = assets.materialPathOf(matId);
        if (isSentinelPath(matPath)) continue;

        auto checkMatTex = [&](TextureAssetId texId, RefKind kind) {
            if (texId == 0) return;
            std::string texPath = assets.pathOf(texId);
            if (isSentinelPath(texPath)) return;
            if (!sameLogical(texPath, assetLogicalPath)) return;
            RefSite site;
            site.kind = kind;
            site.materialPath = matPath;
            out.push_back(std::move(site));
        };
        checkMatTex(mat->albedo,             RefKind::MaterialAlbedo);
        checkMatTex(mat->metallicRoughness,  RefKind::MaterialMetallicRoughness);
        checkMatTex(mat->normal,             RefKind::MaterialNormal);
        checkMatTex(mat->ao,                 RefKind::MaterialAO);
    }

    return out;
}

}  // namespace Mood::asset_refs
