#pragma once

// F2H30 Bloque B: comando undoable para mutaciones geometricas del
// brush (vertex/edge edit). Captura snapshot completo de los `Plane`
// de cada `BrushFace` antes y despues — vertex move = mover 3 planos
// adyacentes; edge move = mover 2; ambos casos cubiertos con la
// misma estructura.
//
// Captura por TAG (no handle EnTT) — patron post-F2H16 (robusto a
// delete + recreate del HistoryStack handle remap).
//
// Tras execute/undo: setea `bc.dirty = true` (el brushPass del frame
// regenera la mesh) + recomputa `bc.brush.localAabb` con
// `computeBrushAabb`.

#include "core/Types.h"
#include "core/math/Plane.h"
#include "editor/commands/Command.h"

#include <glm/vec3.hpp>

#include <string>
#include <vector>

namespace Mood {

class Scene;

class EditBrushGeometryCommand : public ICommand {
public:
    EditBrushGeometryCommand(Scene* scene,
                              std::string entityTag,
                              std::vector<Plane> beforePlanes,
                              std::vector<Plane> afterPlanes,
                              glm::vec3 tfPositionBefore,
                              glm::vec3 tfPositionAfter,
                              std::string label = "Editar geometria de brush");

    void execute() override;
    void undo() override;
    std::string name() const override { return m_label; }

    /// True si los planos + transform pre/post son identicos.
    bool isNoOp() const;

private:
    Scene* m_scene;
    std::string m_entityTag;
    std::vector<Plane> m_before;
    std::vector<Plane> m_after;
    glm::vec3 m_tfPosBefore;
    glm::vec3 m_tfPosAfter;
    std::string m_label;

    void applyState(const std::vector<Plane>& planes,
                     const glm::vec3& tfPos);
};

} // namespace Mood
