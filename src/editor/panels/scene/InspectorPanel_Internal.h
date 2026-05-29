#pragma once

// F2H24: helpers compartidos por todos los archivos parciales del
// Inspector (InspectorPanel.cpp + InspectorPanel_*.cpp). Header
// privado del modulo — no incluir desde otro modulo.

#include "core/i18n/I18n.h"  // F2H74: field-helpers arman el label traducido
#include "editor/commands/AddComponentCommand.h"  // F2H81: makeRemoveComponentCommand
#include "editor/commands/EditPropertyCommand.h"
#include "editor/commands/MultiEditPropertyCommand.h"  // F3H8
#include "editor/commands/PasteComponentCommand.h"  // F3H9
#include "editor/components/ComponentClipboard.h"  // F3H9
#include "editor/panels/scene/InspectorEditTracker.h"
#include "editor/panels/scene/InspectorPanel.h"  // F2H81: def. de beginComponentSection
#include "editor/panels/scene/MultiEditTracker.h"  // F3H8
#include "editor/selection/SelectionSet.h"  // F3H8: itera N entidades
#include "editor/panels/project/AssetIssuesPanel.h"  // F3H18: chequeo inline de refs rotas
#include "editor/ui/EditorUI.h"
#include "editor/ui/IconsFontAwesome6.h"  // F2H37: icons en headers de seccion
#include "engine/assets/manager/AssetManager.h"  // F3H9: serializeComponent
#include "engine/scene/components/BrushComponent.h"  // F3H9: dispatch type-check (no esta en Components.h)
#include "engine/scene/components/Components.h"  // F3H9: type check para componentKey
#include "engine/scene/core/Entity.h"
#include "engine/scene/entity_type/EntityTypeTable.h"  // F3H9: isBaseComponent

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <imgui.h>

#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Mood::detail {

// Hito 32 D: helper para empujar un EditPropertyCommand cuando el dev
// suelta un drag/edit en un widget del Inspector. Se llama
// INMEDIATAMENTE despues del widget para que `IsItem*` se refiera a el.
// Captura history desde el ui (puede ser null si todavia no inyectado).
template<typename T>
void pushEditIfDone(InspectorEditTracker& tracker, EditorUI* ui, Entity e,
                     const T& current,
                     typename EditPropertyCommand<T>::Setter setter,
                     const std::string& label) {
    HistoryStack* h = ui ? ui->historyStack() : nullptr;
    if (h == nullptr) return;
    trackPropertyEdit<T>(tracker, current, e, *h, std::move(setter), label);
}

// F3H12: helper atomico para cambios discretos (checkbox toggle, combo
// selection, button click). NO usa el InspectorEditTracker (no hay drag —
// el cambio es instantaneo en un frame). Si `before != after`, crea un
// EditPropertyCommand<T> + push (que internamente llama execute() — el
// caller NO debe haber aplicado el cambio antes).
//
// Uso: caller detecta el cambio (ej. ImGui::Checkbox devolvio true),
// captura el `before` (valor pre-cambio), pasa el `after` (valor post),
// el setter y label. El push se encarga de aplicar el after.
template<typename T>
void pushAtomicEdit(EditorUI* ui, Entity e,
                     const T& before, const T& after,
                     typename EditPropertyCommand<T>::Setter setter,
                     const std::string& label) {
    if (before == after) return;
    HistoryStack* h = ui ? ui->historyStack() : nullptr;
    if (h != nullptr && setter) {
        auto cmd = std::make_unique<EditPropertyCommand<T>>(
            e, before, after, std::move(setter), label);
        h->push(std::move(cmd));
    } else if (setter) {
        // Fallback sin history: aplicar directo. No deberia pasar en
        // sesion normal (el editor siempre tiene history).
        setter(e, after);
    }
}

// F3H13: boton "↺" reset-to-default per-field. Convencion Unity/Unreal:
// aparece SOLO si current != default (sin visual noise para fields
// default; surface override cuando el dev cambio algo, para que sea facil
// volver al estado canonico).
//
// Diferencia con el `resetButton` de ProjectSettingsPanel: este integra
// undo via `pushAtomicEdit` — el reset queda en el HistoryStack como un
// EditPropertyCommand<T> regular (Ctrl+Z deshace el reset, devuelve al
// valor que el dev tenia antes).
//
// Llamar INMEDIATAMENTE despues del widget editable que se quiere
// resetear (SameLine + SmallButton). Devuelve true si se hizo reset
// este frame (para que el caller setee `m_editedThisFrame`). Si current
// == default, no renderea nada y devuelve false.
template <typename T>
inline bool inspectorResetButton(EditorUI* ui, Entity e,
        const char* idSuffix,
        const T& current,
        const T& defaultValue,
        typename EditPropertyCommand<T>::Setter setter,
        const std::string& cmdLabel) {
    if (current == defaultValue) return false;  // no visual noise

    // F3H13 polish: padding fino para el boton de reset.
    //   - Spacing externo = 3 px (un pelin de aire entre el label y el icono,
    //     sin que se vea flotando).
    //   - Padding interno = (4, 2) — apenas margen para que el icono ↺
    //     respire dentro del boton, sin inflarlo.
    ImGui::SameLine(0.0f, 3.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 2.0f));
    const std::string btnLabel =
        std::string(ICON_FA_ROTATE_LEFT) + "##reset_" + idSuffix;
    const bool clicked = ImGui::SmallButton(btnLabel.c_str());
    ImGui::PopStyleVar();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.panel.inspector.reset_default").c_str());
    }
    if (clicked) {
        pushAtomicEdit<T>(ui, e, current, defaultValue, std::move(setter),
                           cmdLabel);
        return true;
    }
    return false;
}

// F3H18: chequea si el ultimo widget (InputText / Drop slot) refiere a un
// asset rotos segun el AssetIssuesPanel y, si lo esta, le pinta un borde
// rojo + tooltip explicando el problema. Llamar INMEDIATAMENTE despues
// del widget (lee `GetItemRectMin/Max`). No-op si `ui == nullptr`, si
// el path esta vacio, o si el path resuelve OK.
inline void inspectorBrokenRefBorder(EditorUI* ui, Entity e,
                                       const std::string& path) {
    if (ui == nullptr || path.empty() || !e) return;
    const AssetIssuesPanel& issues = ui->assetIssues();
    if (!issues.isFieldBroken(e, path)) return;
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    // Border rojo 2 px sobre el ultimo item.
    ImGui::GetWindowDrawList()->AddRect(min, max,
        IM_COL32(232, 92, 92, 230), 0.0f, 0, 2.0f);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.panel.inspector.broken_ref_tooltip").c_str());
    }
}

// F2H23: helper estandar de ImGui samples — texto gris "(?)" con tooltip
// al hover. Sirve para descubribilidad sin inflar el panel con texto.
// Llamar INMEDIATAMENTE despues del widget que se quiere documentar.
inline void helpMarker(const char* desc) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", desc);
    }
}

// F2H23: detecta si el dev tiene un drag activo de tipo `type` (ej.
// "MOOD_TEXTURE_ASSET"). Sirve para cambiar el color de los botones
// drop-target (que el dev sepa "este boton acepta lo que arrastras").
inline bool isDragActiveOfType(const char* type) {
    const ImGuiPayload* p = ImGui::GetDragDropPayload();
    return p != nullptr && p->IsDataType(type);
}

// === F2H74: field-helpers del Inspector ===========================
// Colapsan el triplete que se repetia ~76 veces en los partials del
// Inspector: (1) armar label i18n + "##suffix", (2) widget, (3)
// pushEditIfDone para undo. Estilo property-drawer de Unity
// (EditorGUILayout) / Unreal (DetailsView). Devuelven `true` si el
// widget se edito este frame (para que el caller setee m_editedThisFrame).
// El `idSuffix` (ej "##trig") evita colisiones de ID entre secciones que
// reusan el mismo label key.

inline bool fieldDragFloat3(InspectorEditTracker& tracker, EditorUI* ui,
        Entity e, const std::string& labelKey, const char* idSuffix,
        glm::vec3& value,
        typename EditPropertyCommand<glm::vec3>::Setter setter,
        const std::string& cmdLabel,
        float speed = 0.1f, float vmin = 0.0f, float vmax = 0.0f) {
    const std::string label = I18n::T(labelKey) + idSuffix;
    const bool edited = ImGui::DragFloat3(label.c_str(), &value.x, speed, vmin, vmax);
    pushEditIfDone<glm::vec3>(tracker, ui, e, value, std::move(setter), cmdLabel);
    return edited;
}

inline bool fieldDragFloat(InspectorEditTracker& tracker, EditorUI* ui,
        Entity e, const std::string& labelKey, const char* idSuffix,
        f32& value,
        typename EditPropertyCommand<f32>::Setter setter,
        const std::string& cmdLabel,
        float speed = 0.1f, float vmin = 0.0f, float vmax = 0.0f) {
    const std::string label = I18n::T(labelKey) + idSuffix;
    const bool edited = ImGui::DragFloat(label.c_str(), &value, speed, vmin, vmax);
    pushEditIfDone<f32>(tracker, ui, e, value, std::move(setter), cmdLabel);
    return edited;
}

// Color RGB (sin speed/min/max). Usa el mismo camino de undo que los drags.
inline bool fieldColorEdit3(InspectorEditTracker& tracker, EditorUI* ui,
        Entity e, const std::string& labelKey, const char* idSuffix,
        glm::vec3& value,
        typename EditPropertyCommand<glm::vec3>::Setter setter,
        const std::string& cmdLabel) {
    const std::string label = I18n::T(labelKey) + idSuffix;
    const bool edited = ImGui::ColorEdit3(label.c_str(), &value.x);
    pushEditIfDone<glm::vec3>(tracker, ui, e, value, std::move(setter), cmdLabel);
    return edited;
}

// === F3H8: multi-edit helpers =====================================
// Variantes selection-aware de los field-helpers. Detectan tamano del
// SelectionSet:
//   - size <= 1 -> fall through al field*Single() (back-compat).
//   - size  > 1 -> path multi-edit con detector valor comun + mixed
//                  marker + live preview + MultiEditPropertyCommand al
//                  soltar el widget.
//
// Snapshot semantics (D3 del plan F3H8): cada entity guarda su before
// individual, TODAS se homogenizan al active's after al commit. Delta
// semantics (cada entity gets before+delta) la usa Transform en F2H23
// iter 5 — no la replicamos aca.

inline bool nearlyEqualVec3(const glm::vec3& a, const glm::vec3& b,
                              f32 eps = 1.0f / 255.0f) {
    // Epsilon = 1 LSB en 8-bit color por defecto. Para positions /
    // intensities el caller puede pasar otro eps.
    return std::abs(a.x - b.x) < eps
        && std::abs(a.y - b.y) < eps
        && std::abs(a.z - b.z) < eps;
}

inline bool nearlyEqualF32(f32 a, f32 b, f32 eps = 1e-4f) {
    return std::abs(a - b) < eps;
}

} // namespace Mood::detail

// F4H1.5 split: multi-edit helpers (allMatch + multiEditColor3 / DragFloat /
// Checkbox / Combo / Color4) movidos a _Internal_MultiEdit.h para mantener
// _Internal.h bajo el hard cap 800 LOC. El header sigue siendo privado del
// modulo (incluir desde InspectorPanel_*.cpp via _Internal.h).
#include "editor/panels/scene/InspectorPanel_Internal_MultiEdit.h"

namespace Mood {

// F2H81: definicion del header plegable (declarado en InspectorPanel.h).
// Templado en T para que el menu "Quitar componente" arme un
// makeRemoveComponentCommand<T> tipado. Reemplaza el SeparatorText
// siempre-abierto: ahora cada componente es una tarjeta que se pliega.
//
// F3H9: dispatch T -> componentKey string (vocabulario de
// EntitySerializer). Cubre todos los componentes que el Inspector
// renderea — usado para (a) base-component lock del modelo EntityType,
// (b) copy/paste del clipboard Tier 1. `null` = T no tiene componentKey
// conocido (no se dibuja menu de paste ni lock-check).
namespace detail {
template <typename T>
inline const char* componentKeyForT() {
    if      constexpr (std::is_same_v<T, LightComponent>)           return "light";
    else if constexpr (std::is_same_v<T, MeshRendererComponent>)    return "mesh_renderer";
    else if constexpr (std::is_same_v<T, CameraComponent>)          return "camera";
    else if constexpr (std::is_same_v<T, AudioSourceComponent>)     return "audio_source";
    else if constexpr (std::is_same_v<T, TriggerComponent>)         return "trigger";
    else if constexpr (std::is_same_v<T, ForceFieldComponent>)      return "force_field";
    else if constexpr (std::is_same_v<T, ParticleEmitterComponent>) return "particle_emitter";
    else if constexpr (std::is_same_v<T, EnvironmentComponent>)     return "environment";
    else if constexpr (std::is_same_v<T, DialogComponent>)          return "dialog";
    else if constexpr (std::is_same_v<T, ItemPickupComponent>)      return "item_pickup";
    else if constexpr (std::is_same_v<T, BrushComponent>)           return "brush";
    else if constexpr (std::is_same_v<T, RigidBodyComponent>)       return "rigid_body";
    else if constexpr (std::is_same_v<T, ScriptComponent>)          return "script";
    else if constexpr (std::is_same_v<T, AnimatorComponent>)        return "animator";
    else if constexpr (std::is_same_v<T, InventoryComponent>)       return "inventory";
    else if constexpr (std::is_same_v<T, JointComponent>)           return "joint";
    else if constexpr (std::is_same_v<T, RagdollComponent>)         return "ragdoll";
    else if constexpr (std::is_same_v<T, VehicleComponent>)         return "vehicle";
    else if constexpr (std::is_same_v<T, ClothComponent>)           return "cloth";
    else                                                              return nullptr;
}
} // namespace detail

template<typename T>
bool InspectorPanel::beginComponentSection(Entity e, const char* label,
                                            bool removable) {
    // F3H22 polish: default colapsado (sin ImGuiTreeNodeFlags_DefaultOpen).
    // El dev expande las secciones que quiere ver — entities con muchos
    // componentes ya no muestran todo abierto al seleccionar.
    // Toolbar Plegar/Expandir todo eliminado (F2H81 ya no aplica con
    // categorías F3H22 — cada categoría tiene pocos componentes visibles).
    const bool open = ImGui::CollapsingHeader(label);

    // Menu contextual (clic derecho sobre el header): quitar componente
    // + F3H9 copy/paste de valores + lock de base-component segun
    // EntityType del owner.
    if (removable && ImGui::BeginPopupContextItem()) {
        // F3H9: dispatch T -> componentKey string (vocabulario de
        // EntitySerializer + EntityTypeTable). null = T no soportado
        // (no se dibuja copy/paste ni lock-check — solo Remove generic).
        const char* componentKey = detail::componentKeyForT<T>();

        // F3H9: chequear si este componente es BASE del type de la
        // entity. Si lo es, "Remove component" queda gris — la entity
        // ES de ese type (Light entity tiene LightComponent como nucleo
        // de identidad), borrarlo se hace borrando la entity.
        const EntityType entType = e.hasComponent<TagComponent>()
            ? e.getComponent<TagComponent>().entityType
            : EntityType::Generic;
        const bool isBase = (componentKey != nullptr) &&
            EntityTypeTable::isBaseComponent(entType, componentKey);

        // Tile entities son auto-gen — disabled todo el menu de
        // edicion (read-only del Inspector).
        const bool isAutoGen = (entType == EntityType::Tile);

        // Copy/Paste items — solo para los Tier 1 supported keys
        // (light/trigger/force_field/particle_emitter), por ahora.
        const bool tier1Paste = (componentKey != nullptr) &&
            ComponentClipboard::isSupported(componentKey);

        if (tier1Paste && !isAutoGen && m_ui != nullptr && m_assets != nullptr) {
            // Copiar valores: serializa el componente al clipboard de EditorUI.
            const std::string copyLabel =
                std::string(ICON_FA_COPY " ") +
                I18n::T("editor.panel.inspector.context.copy_values");
            if (ImGui::Selectable(copyLabel.c_str())) {
                auto payload = ComponentClipboard::serializeComponent(
                    componentKey, e, *m_assets);
                if (!payload.is_null()) {
                    m_ui->setClipboardComponent(
                        std::string(componentKey), std::move(payload));
                }
                ImGui::EndPopup();
                return open;
            }

            // Pegar valores: gris si el clipboard esta vacio o el componentKey
            // del clipboard no coincide con T.
            const auto& clip = m_ui->clipboardComponent();
            const bool canPaste = clip.has_value() &&
                                    clip->componentKey == componentKey;
            const std::string pasteLabel =
                std::string(ICON_FA_PASTE " ") +
                I18n::T("editor.panel.inspector.context.paste_values");
            if (!canPaste) ImGui::BeginDisabled();
            if (ImGui::Selectable(pasteLabel.c_str()) && canPaste) {
                // Build PasteComponentCommand: snapshot before, after = clipboard.
                auto before = ComponentClipboard::serializeComponent(
                    componentKey, e, *m_assets);
                auto cmd = std::make_unique<PasteComponentCommand>(
                    e, std::string(componentKey),
                    std::move(before),
                    clip->payload,  // copy del payload
                    /*hadComponentBefore=*/true,
                    m_assets,
                    I18n::T("editor.panel.inspector.context.cmd_paste_values"));
                if (!cmd->isNoOp()) {
                    HistoryStack* h = m_ui->historyStack();
                    if (h != nullptr) {
                        h->push(std::move(cmd));
                    } else {
                        cmd->execute();
                    }
                    m_editedThisFrame = true;
                }
                ImGui::EndPopup();
                return open;
            }
            if (!canPaste) ImGui::EndDisabled();

            ImGui::Separator();
        }

        // F3H9: "Remove component" disabled si es base del type
        // (no podes quitar el LightComponent de una entity Light;
        // borrala entera). isAutoGen tambien lo deshabilita
        // (entities Tile son read-only). Tooltip explica al hover.
        const std::string item =
            ICON_FA_TRASH_CAN " " + I18n::T("editor.panel.inspector.remove_component");
        const bool removeDisabled = isBase || isAutoGen;
        if (removeDisabled) ImGui::BeginDisabled();
        const bool removeClicked = ImGui::Selectable(item.c_str());
        if (removeDisabled) ImGui::EndDisabled();
        if (removeDisabled && ImGui::IsItemHovered(
                ImGuiHoveredFlags_AllowWhenDisabled)) {
            const char* reasonKey = isAutoGen
                ? "editor.panel.inspector.remove_disabled_autogen"
                : "editor.panel.inspector.remove_disabled_base";
            ImGui::SetTooltip("%s", I18n::T(reasonKey).c_str());
        }
        if (removeClicked && !removeDisabled) {
            HistoryStack* h = m_ui ? m_ui->historyStack() : nullptr;
            auto cmd = makeRemoveComponentCommand<T>(
                e, I18n::T("editor.panel.inspector.remove_component"));
            if (h != nullptr) {
                h->push(std::move(cmd));  // ejecuta + apila para undo
            } else {
                cmd->execute();  // fallback defensivo sin history
            }
            m_editedThisFrame = true;
            ImGui::EndPopup();
            // El componente ya no existe — el caller NO debe dibujar el
            // cuerpo (su referencia al componente quedaria colgada).
            return false;
        }
        ImGui::EndPopup();
    }

    return open;
}

} // namespace Mood
