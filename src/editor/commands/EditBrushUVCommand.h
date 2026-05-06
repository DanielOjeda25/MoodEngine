#pragma once

// F2H16: comando undoable para cambios de UV params del brush
// (uvScale, uvRotation, uvOffset, lockToWorld, uAxis, vAxis).
// Captura snapshot completo de los 6 params de TODAS las caras
// pre y post — granularidad por "intencion del user" (un drag
// de slider completo = 1 command).
//
// Uso: el Inspector "UV (Brush)" captura snapshot al ImGui::IsItemActivated
// y push del comando al ImGui::IsItemDeactivatedAfterEdit.
//
// F2H17 (face mode) puede agregar variante per-cara
// EditBrushFaceUVCommand. En F2H16 todas las ediciones son
// "global por brush".

#include "editor/commands/Command.h"

#include "core/Types.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <string>
#include <vector>

namespace Mood {

class Scene;

namespace Csg { struct Brush; }

/// @brief Snapshot de los UV params de TODAS las caras del brush.
///        Serializable por valor (sin handles), reusable como
///        captura pre/post del EditBrushUVCommand.
struct BrushUVSnapshot {
    std::vector<glm::vec3> uAxes;
    std::vector<glm::vec3> vAxes;
    std::vector<glm::vec2> uvOffsets;
    std::vector<glm::vec2> uvScales;
    std::vector<f32>       uvRotations;
    // std::vector<bool> es bitset; usamos u8 para evitar
    // especializacion templated de vector<bool>.
    std::vector<u8>        lockToWorlds;
};

/// @brief Captura los UV params actuales de cada cara del brush.
BrushUVSnapshot captureBrushUV(const Csg::Brush& brush);

/// @brief Aplica un snapshot a las caras del brush. Si la cantidad
///        de caras del snapshot no coincide con la del brush actual,
///        no hace nada (defensivo: protege contra comandos del stack
///        ejecutados sobre un brush que cambio de topologia).
void applyBrushUV(Csg::Brush& brush, const BrushUVSnapshot& snap);

/// @brief True si dos snapshots son estructuralmente identicos
///        (mismo numero de caras y mismos params en cada una con
///        tolerancia kPlaneEpsilon). Util para evitar push spurio
///        de comandos cuando el user clickea un slider sin moverlo.
bool snapshotsEqual(const BrushUVSnapshot& a, const BrushUVSnapshot& b);

class EditBrushUVCommand : public ICommand {
public:
    EditBrushUVCommand(Scene* scene,
                        std::string entityTag,
                        BrushUVSnapshot oldSnap,
                        BrushUVSnapshot newSnap,
                        std::string label = "Editar UV de brush");

    void execute() override;
    void undo() override;
    std::string name() const override { return m_label; }

private:
    Scene* m_scene;
    std::string m_entityTag;
    BrushUVSnapshot m_oldSnap;
    BrushUVSnapshot m_newSnap;
    std::string m_label;

    void applySnapshot(const BrushUVSnapshot& snap);
};

} // namespace Mood
