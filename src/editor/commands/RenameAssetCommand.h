#pragma once

// F3H19: comando undoable de "renombrar un asset con cascada a las refs".
// Es la atomic unit que une 3 mutaciones:
//   1. std::filesystem::rename(oldDiskPath, newDiskPath).
//   2. AssetManager::renameLogicalPath(oldLogical, newLogical) — reescribe
//      el cache id↔path interno.
//   3. Por cada RefSite con kind string-path (Script/Dialog/Item/Vehicle/
//      Skybox/ColorGradingLut/PrefabLink), reescribir el campo del
//      componente con el nuevo path.
//
// Las refs id-based (AudioClipId, MeshId, MaterialSlot, AnimatorClip,
// ParticleTexture, BrushMaterialSlot, Material*Texture) NO se tocan
// individualmente — siguen apuntando al mismo id, y el cache del
// AssetManager ya devuelve el nuevo path en `pathOf(id)`. Por eso el
// rename se ve reflejado al instante sin tocar cada componente.
//
// Precondiciones (responsabilidad del caller):
//   - oldDiskPath existe en disco.
//   - newDiskPath NO existe en disco (chequear antes — abort si existe,
//     decisión D2 del PLAN_HITO_F3H19).
//   - oldLogical y newLogical son paths lógicos normalizados (forward
//     slashes, sin prefijo "assets/").
//   - refs es un snapshot fresco de asset_refs::findRefs(scene, assets,
//     oldLogical) tomado JUSTO antes de construir el comando — si la
//     scene cambió entre el snapshot y el execute(), el comando puede
//     dejar refs sin actualizar.

#include "editor/commands/Command.h"
#include "engine/assets/refs/AssetRefIndex.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Mood {

class Scene;
class AssetManager;

class RenameAssetCommand : public ICommand {
public:
    RenameAssetCommand(Scene* scene,
                        AssetManager* assets,
                        std::filesystem::path oldDiskPath,
                        std::filesystem::path newDiskPath,
                        std::string oldLogical,
                        std::string newLogical,
                        std::vector<asset_refs::RefSite> refs);

    void execute() override;
    void undo() override;
    std::string name() const override { return m_name; }

private:
    Scene* m_scene = nullptr;
    AssetManager* m_assets = nullptr;
    std::filesystem::path m_oldDiskPath;
    std::filesystem::path m_newDiskPath;
    std::string m_oldLogical;
    std::string m_newLogical;
    std::vector<asset_refs::RefSite> m_refs;
    std::string m_name;

    // forward=true → execute (oldLogical → newLogical en todos los sites).
    // forward=false → undo (newLogical → oldLogical).
    void applyRename(bool forward);

    // Reescribe los string-path refs en los componentes. La direccion la
    // decide el caller (execute/undo).
    void rewriteStringPathRefs(const std::string& from, const std::string& to);
};

}  // namespace Mood
