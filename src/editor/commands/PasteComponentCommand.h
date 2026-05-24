#pragma once

// PasteComponentCommand (F3H9): comando undo/redo para "Pegar valores"
// y "Pegar como nuevo componente" del clipboard de componentes del
// Inspector.
//
// Snapshot semantics: captura el estado pre-paste (componente existente
// serializado a JSON + flag hadComponentBefore) Y el payload a aplicar.
// Execute = aplicar payload (addComponent si falta + setea fields).
// Undo:
//   - hadComponentBefore == true  -> aplicar payload previo (restaura
//                                     los fields al estado pre-paste).
//   - hadComponentBefore == false -> removeComponent (paste como nuevo
//                                     -> undo lo quita).
//
// Resiliencia: si la entidad fue destruida tras el push, no-op silencioso.

#include "core/Types.h"
#include "editor/commands/Command.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <nlohmann/json.hpp>

#include <string>
#include <utility>

namespace Mood {

class AssetManager;

class PasteComponentCommand : public ICommand {
public:
    PasteComponentCommand(Entity target,
                            std::string componentKey,
                            nlohmann::json before,
                            nlohmann::json after,
                            bool hadComponentBefore,
                            AssetManager* assets,
                            std::string label)
        : m_target(target)
        , m_componentKey(std::move(componentKey))
        , m_before(std::move(before))
        , m_after(std::move(after))
        , m_hadComponentBefore(hadComponentBefore)
        , m_assets(assets)
        , m_label(std::move(label)) {}

    void execute() override;
    void undo() override;
    std::string name() const override { return m_label; }
    void onEntityRemap(entt::entity oldH, entt::entity newH) override {
        if (m_target.handle() == oldH) {
            m_target = Entity(newH, m_target.scene());
        }
    }

    /// True si before == after Y hadComponentBefore == true (paste valores
    /// sin cambio real). Usado por el caller para evitar push en clicks
    /// que no producen efecto.
    bool isNoOp() const {
        return m_hadComponentBefore && m_before == m_after;
    }

    bool hadComponentBefore() const { return m_hadComponentBefore; }
    const std::string& componentKey() const { return m_componentKey; }

private:
    bool isEntityValid() const {
        return m_target.scene() != nullptr
            && m_target.scene()->registry().valid(m_target.handle());
    }

    Entity         m_target;
    std::string    m_componentKey;
    nlohmann::json m_before;             // valor pre-paste (vacio si no habia componente)
    nlohmann::json m_after;              // valor a aplicar
    bool           m_hadComponentBefore; // true = paste valores; false = paste como nuevo
    AssetManager*  m_assets;
    std::string    m_label;
};

} // namespace Mood
