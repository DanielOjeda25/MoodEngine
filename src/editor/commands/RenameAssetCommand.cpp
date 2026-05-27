#include "editor/commands/RenameAssetCommand.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <utility>

namespace Mood {

RenameAssetCommand::RenameAssetCommand(Scene* scene,
                                          AssetManager* assets,
                                          std::filesystem::path oldDiskPath,
                                          std::filesystem::path newDiskPath,
                                          std::string oldLogical,
                                          std::string newLogical,
                                          std::vector<asset_refs::RefSite> refs)
    : m_scene(scene),
      m_assets(assets),
      m_oldDiskPath(std::move(oldDiskPath)),
      m_newDiskPath(std::move(newDiskPath)),
      m_oldLogical(std::move(oldLogical)),
      m_newLogical(std::move(newLogical)),
      m_refs(std::move(refs)) {
    m_name = "Renombrar '" + m_oldLogical + "' -> '" + m_newLogical + "'";
}

void RenameAssetCommand::execute() {
    applyRename(/*forward=*/true);
}

void RenameAssetCommand::undo() {
    applyRename(/*forward=*/false);
}

void RenameAssetCommand::applyRename(bool forward) {
    const auto& fromDisk = forward ? m_oldDiskPath : m_newDiskPath;
    const auto& toDisk   = forward ? m_newDiskPath : m_oldDiskPath;
    const std::string& fromLogical = forward ? m_oldLogical : m_newLogical;
    const std::string& toLogical   = forward ? m_newLogical : m_oldLogical;

    // 1. Mover el archivo en disco. Si falla, abortar: undo() requiere el
    // disk en su estado original.
    std::error_code ec;
    std::filesystem::rename(fromDisk, toDisk, ec);
    if (ec) {
        Log::editor()->error(
            "RenameAssetCommand: fs::rename '{}' -> '{}' fallo: {}",
            fromDisk.generic_string(), toDisk.generic_string(), ec.message());
        return;
    }

    // 2. Reescribir el path interno del AssetManager (si el asset esta
    // cacheado). No-op para extensiones sin cache (.lua, .png externo
    // nunca cargado, etc).
    if (m_assets != nullptr) {
        m_assets->renameLogicalPath(fromLogical, toLogical);
    }

    // 3. Reescribir las refs string-path en los componentes.
    rewriteStringPathRefs(fromLogical, toLogical);

    Log::editor()->info(
        "Rename '{}' -> '{}' aplicado ({} refs actualizadas)",
        fromLogical, toLogical, m_refs.size());
}

void RenameAssetCommand::rewriteStringPathRefs(const std::string& from,
                                                  const std::string& to) {
    if (m_scene == nullptr) return;
    using asset_refs::RefKind;

    for (const auto& site : m_refs) {
        if (!site.entity) continue;
        Entity e = site.entity;  // copy local

        switch (site.kind) {
        case RefKind::ScriptPath: {
            if (e.hasComponent<ScriptComponent>()) {
                auto& sc = e.getComponent<ScriptComponent>();
                if (sc.path == from) {
                    sc.path = to;
                    sc.loaded = false;  // forzar reload con el nuevo path
                }
            }
            break;
        }
        case RefKind::DialogPath: {
            if (e.hasComponent<DialogComponent>()) {
                auto& dc = e.getComponent<DialogComponent>();
                if (dc.dialogPath == from) dc.dialogPath = to;
            }
            break;
        }
        case RefKind::ItemPath: {
            if (e.hasComponent<ItemPickupComponent>()) {
                auto& ic = e.getComponent<ItemPickupComponent>();
                if (ic.itemPath == from) ic.itemPath = to;
            }
            break;
        }
        case RefKind::VehiclePath: {
            if (e.hasComponent<VehicleComponent>()) {
                auto& vc = e.getComponent<VehicleComponent>();
                if (vc.configPath == from) {
                    vc.configPath = to;
                    vc.dirty = true;  // forzar rematerializacion
                }
            }
            break;
        }
        case RefKind::SkyboxPath: {
            if (e.hasComponent<EnvironmentComponent>()) {
                auto& env = e.getComponent<EnvironmentComponent>();
                if (env.skyboxPath == from) env.skyboxPath = to;
            }
            break;
        }
        case RefKind::ColorGradingLutPath: {
            if (e.hasComponent<EnvironmentComponent>()) {
                auto& env = e.getComponent<EnvironmentComponent>();
                if (env.colorGradingLutPath == from) env.colorGradingLutPath = to;
            }
            break;
        }
        case RefKind::PrefabLinkPath: {
            if (e.hasComponent<PrefabLinkComponent>()) {
                auto& pl = e.getComponent<PrefabLinkComponent>();
                if (pl.path == from) pl.path = to;
            }
            break;
        }
        // Refs id-based: el AssetManager.renameLogicalPath ya actualizo el
        // path interno; los componentes siguen usando el mismo id y al
        // pedir pathOf(id) reciben el nuevo path automaticamente. No hay
        // nada que reescribir aca.
        case RefKind::AudioClipId:
        case RefKind::MeshId:
        case RefKind::MeshMaterialSlot:
        case RefKind::AnimatorExternalClip:
        case RefKind::ParticleTextureId:
        case RefKind::BrushMaterialSlot:
        case RefKind::MaterialAlbedo:
        case RefKind::MaterialMetallicRoughness:
        case RefKind::MaterialNormal:
        case RefKind::MaterialAO:
            break;
        }
    }
}

}  // namespace Mood
