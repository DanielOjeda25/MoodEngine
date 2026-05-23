#pragma once

// F2H84: EditAssetPropertyCommand<T> — gemelo de EditPropertyCommand<T>
// (que está atado a `Entity`), pero para edits sobre *assets* del editor
// (Material / Item / Quest). El asset no es una entity — vive en el
// AssetManager (MaterialAsset) o en un buffer interno del panel (ItemAsset
// /QuestAsset cargados a memoria). Por eso el setter no recibe entity:
// el callsite captura el path al campo via lambda (`[this](float v) {
// m_loaded.max_stack = static_cast<int>(v); m_dirty = true; }`).
//
// Patrón de uso (mismo que en el Inspector):
//   1. ImGui::DragFloat muta el campo en vivo durante el drag.
//   2. `IsItemActivated()` → snapshot del before via AssetEditTracker.
//   3. `IsItemDeactivatedAfterEdit()` → capturar after, revertir a before,
//      push command (que re-aplica via execute()).
//
// Resiliencia: si el panel descarga el asset entre push y undo, el setter
// debe ser seguro (no-op o aplica a un asset que ya no se muestra). En
// la práctica los panels llaman `historyStack->clear()` al cambiar de
// asset cargado (mismo patrón de `NodeGraphSandboxPanel` y
// `ShaderGraphEditorPanel`) — el history queda atado al asset activo.

#include "editor/commands/Command.h"

#include <functional>
#include <string>
#include <utility>

namespace Mood {

template<typename T>
class EditAssetPropertyCommand : public ICommand {
public:
    /// Setter generico: recibe el nuevo valor y lo aplica. Cada callsite
    /// captura el campo via lambda (`[this](const glm::vec3& v) {
    /// m_loaded.color = v; m_dirty = true; }`).
    using Setter = std::function<void(const T&)>;

    EditAssetPropertyCommand(T before, T after,
                              Setter setter, std::string label)
        : m_before(std::move(before))
        , m_after(std::move(after))
        , m_setter(std::move(setter))
        , m_label(std::move(label)) {}

    void execute() override {
        if (m_setter) m_setter(m_after);
    }
    void undo() override {
        if (m_setter) m_setter(m_before);
    }
    std::string name() const override { return m_label; }
    void onEntityRemap(entt::entity, entt::entity) override {
        // No-op: este comando no apunta a entities.
    }

    /// True si before == after — push() se vuelve no-op para evitar
    /// entradas vacias en el history al hacer click sin drag.
    bool isNoOp() const { return m_before == m_after; }

private:
    T      m_before;
    T      m_after;
    Setter m_setter;
    std::string m_label;
};

} // namespace Mood
