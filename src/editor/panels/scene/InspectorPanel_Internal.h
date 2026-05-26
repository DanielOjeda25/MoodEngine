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

// Detecta si las N entidades del set tienen el mismo valor para el field
// (via getter). Devuelve `true` si TODAS coinciden con `activeValue`
// dentro del eps. Skipea entidades invalidas (defensivo ante destruccion
// entre frames).
template <typename T, typename Getter, typename Eq>
bool allMatch(const std::vector<Entity>& set, const T& activeValue,
              Getter getter, Eq eq) {
    for (const Entity& en : set) {
        if (!static_cast<bool>(en)) continue;
        if (!eq(getter(en), activeValue)) return false;
    }
    return true;
}

// Multi-edit de glm::vec3 con ColorEdit3. Si selection.size() <= 1 cae
// al path single-entity (fieldColorEdit3). Si > 1 detecta mixed, hace
// live preview en peers, y pushea MultiEditPropertyCommand<glm::vec3>
// al soltar.
inline bool multiEditColor3(MultiEditTracker& mTracker,
        InspectorEditTracker& sTracker, EditorUI* ui,
        Entity activeEntity,
        const std::string& labelKey, const char* idSuffix,
        glm::vec3& currentValue,
        std::function<glm::vec3(Entity)> getter,
        typename EditPropertyCommand<glm::vec3>::Setter setter,
        const std::string& cmdLabel) {
    if (ui == nullptr) return false;
    const SelectionSet& sel = ui->selectionSet();

    // Fallback single-entity path (back-compat con todos los call-sites
    // que tienen 1 sola entidad seleccionada).
    if (sel.selected.size() <= 1u) {
        return fieldColorEdit3(sTracker, ui, activeEntity, labelKey,
                                idSuffix, currentValue, setter, cmdLabel);
    }

    // Multi-edit path.
    // Detectar mixed comparando active vs peers.
    const bool mixed = !allMatch<glm::vec3>(sel.selected, currentValue,
        getter,
        [](const glm::vec3& a, const glm::vec3& b) {
            return nearlyEqualVec3(a, b);
        });

    // Render widget. Prefix em-dash si mixed para feedback visual.
    std::string label = mixed
        ? std::string("\xE2\x80\x94 ") + I18n::T(labelKey)
        : I18n::T(labelKey);
    label += idSuffix;
    const bool edited = ImGui::ColorEdit3(label.c_str(), &currentValue.x);
    if (mixed && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.inspector.multi_edit.mixed_tooltip",
                    static_cast<int>(sel.selected.size())).c_str());
    }

    // Live preview: mientras el widget esta activo, propaga el valor del
    // active a los peers cada frame. Mismo "feel" que el gizmo multi-edit
    // de F2H23 iter 5 — el dev ve las N entidades cambiar en vivo.
    if (ImGui::IsItemActive() && setter) {
        for (const Entity& other : sel.selected) {
            if (!static_cast<bool>(other)) continue;
            if (other.handle() == activeEntity.handle()) continue;
            Entity mut = other;
            setter(mut, currentValue);
        }
    }

    // Tracking: snapshot al activate, push command al deactivate-after-edit.
    const ImGuiID itemId = ImGui::GetItemID();
    if (ImGui::IsItemActivated()) {
        mTracker.activeId = itemId;
        mTracker.entities = sel.selected;
        std::vector<glm::vec3> before;
        before.reserve(sel.selected.size());
        for (const Entity& en : sel.selected) {
            before.push_back(static_cast<bool>(en)
                ? getter(en) : glm::vec3{0.0f});
        }
        mTracker.before = std::move(before);
    }

    if (ImGui::IsItemDeactivatedAfterEdit() && mTracker.activeId == itemId) {
        const glm::vec3 after = currentValue;
        const auto* beforePtr =
            std::get_if<std::vector<glm::vec3>>(&mTracker.before);
        if (beforePtr != nullptr && setter) {
            // Construir entries (entity + before individual).
            std::vector<typename MultiEditPropertyCommand<glm::vec3>::Entry> entries;
            entries.reserve(mTracker.entities.size());
            for (usize i = 0; i < mTracker.entities.size()
                              && i < beforePtr->size(); ++i) {
                entries.push_back({mTracker.entities[i], (*beforePtr)[i]});
            }
            // Revertir cada entidad a su before — push.execute() re-aplica
            // el after homogeneo (mismo patron que el single-entity tracker).
            for (auto& en : entries) {
                if (!static_cast<bool>(en.entity)) continue;
                setter(en.entity, en.before);
            }
            auto cmd = std::make_unique<
                MultiEditPropertyCommand<glm::vec3>>(
                std::move(entries), after, setter,
                cmdLabel + " (" +
                std::to_string(mTracker.entities.size()) + ")");
            if (!cmd->isNoOp()) {
                HistoryStack* h = ui->historyStack();
                if (h != nullptr) h->push(std::move(cmd));
            }
        }
        mTracker.reset();
    }

    return edited;
}

// Multi-edit de f32 con DragFloat. Mismo patron que multiEditColor3 —
// duplicado intencional vs templatizar para mantener call-sites legibles
// (cada tipo tiene su widget ImGui especifico).
inline bool multiEditDragFloat(MultiEditTracker& mTracker,
        InspectorEditTracker& sTracker, EditorUI* ui,
        Entity activeEntity,
        const std::string& labelKey, const char* idSuffix,
        f32& currentValue,
        std::function<f32(Entity)> getter,
        typename EditPropertyCommand<f32>::Setter setter,
        const std::string& cmdLabel,
        f32 speed = 0.1f, f32 vmin = 0.0f, f32 vmax = 0.0f) {
    if (ui == nullptr) return false;
    const SelectionSet& sel = ui->selectionSet();

    if (sel.selected.size() <= 1u) {
        return fieldDragFloat(sTracker, ui, activeEntity, labelKey,
                                idSuffix, currentValue, setter, cmdLabel,
                                speed, vmin, vmax);
    }

    const bool mixed = !allMatch<f32>(sel.selected, currentValue,
        getter,
        [](f32 a, f32 b) { return nearlyEqualF32(a, b); });

    std::string label = mixed
        ? std::string("\xE2\x80\x94 ") + I18n::T(labelKey)
        : I18n::T(labelKey);
    label += idSuffix;
    const bool edited = ImGui::DragFloat(label.c_str(), &currentValue,
                                            speed, vmin, vmax);
    if (mixed && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.inspector.multi_edit.mixed_tooltip",
                    static_cast<int>(sel.selected.size())).c_str());
    }

    if (ImGui::IsItemActive() && setter) {
        for (const Entity& other : sel.selected) {
            if (!static_cast<bool>(other)) continue;
            if (other.handle() == activeEntity.handle()) continue;
            Entity mut = other;
            setter(mut, currentValue);
        }
    }

    const ImGuiID itemId = ImGui::GetItemID();
    if (ImGui::IsItemActivated()) {
        mTracker.activeId = itemId;
        mTracker.entities = sel.selected;
        std::vector<f32> before;
        before.reserve(sel.selected.size());
        for (const Entity& en : sel.selected) {
            before.push_back(static_cast<bool>(en) ? getter(en) : 0.0f);
        }
        mTracker.before = std::move(before);
    }

    if (ImGui::IsItemDeactivatedAfterEdit() && mTracker.activeId == itemId) {
        const f32 after = currentValue;
        const auto* beforePtr = std::get_if<std::vector<f32>>(&mTracker.before);
        if (beforePtr != nullptr && setter) {
            std::vector<typename MultiEditPropertyCommand<f32>::Entry> entries;
            entries.reserve(mTracker.entities.size());
            for (usize i = 0; i < mTracker.entities.size()
                              && i < beforePtr->size(); ++i) {
                entries.push_back({mTracker.entities[i], (*beforePtr)[i]});
            }
            for (auto& en : entries) {
                if (!static_cast<bool>(en.entity)) continue;
                setter(en.entity, en.before);
            }
            auto cmd = std::make_unique<MultiEditPropertyCommand<f32>>(
                std::move(entries), after, setter,
                cmdLabel + " (" +
                std::to_string(mTracker.entities.size()) + ")");
            if (!cmd->isNoOp()) {
                HistoryStack* h = ui->historyStack();
                if (h != nullptr) h->push(std::move(cmd));
            }
        }
        mTracker.reset();
    }

    return edited;
}

// F3H12: helpers para tipos que cambian atomicamente (no drag).
// Patron diferente de multiEditColor3/multiEditDragFloat: combo/checkbox
// disparan IsItemActivated + IsItemDeactivatedAfterEdit en el mismo frame
// del click. No hay live preview entre frames — el snapshot del before y
// el push del command ocurren en una sola pasada cuando el widget edita.

// Multi-edit de bool con Checkbox. Si selection.size() <= 1 cae al
// path single-entity (Checkbox + pushEditIfDone<bool>).
inline bool multiEditCheckbox(MultiEditTracker& mTracker,
        InspectorEditTracker& sTracker, EditorUI* ui,
        Entity activeEntity,
        const std::string& labelKey, const char* idSuffix,
        bool& currentValue,
        std::function<bool(Entity)> getter,
        typename EditPropertyCommand<bool>::Setter setter,
        const std::string& cmdLabel) {
    if (ui == nullptr) return false;
    const SelectionSet& sel = ui->selectionSet();

    if (sel.selected.size() <= 1u) {
        // Single-entity path: Checkbox + pushEditIfDone (mismo patron que
        // el resto del Inspector para checkboxes single).
        const std::string label = I18n::T(labelKey) + idSuffix;
        const bool edited = ImGui::Checkbox(label.c_str(), &currentValue);
        pushEditIfDone<bool>(sTracker, ui, activeEntity, currentValue,
                              std::move(setter), cmdLabel);
        return edited;
    }

    // Multi-edit path. Mixed = peers tienen valores distintos al active.
    const bool mixed = !allMatch<bool>(sel.selected, currentValue,
        getter, [](bool a, bool b) { return a == b; });

    std::string label = mixed
        ? std::string("\xE2\x80\x94 ") + I18n::T(labelKey)
        : I18n::T(labelKey);
    label += idSuffix;
    // Snapshot del before del active ANTES de llamar al Checkbox —
    // `currentValue` es referencia al campo del active y se mutara in-place
    // si el dev toggle. Si no capturamos aca, beforeVec[active_idx] queda
    // con el `after`, no el before.
    const bool activeBefore = currentValue;
    const bool edited = ImGui::Checkbox(label.c_str(), &currentValue);
    if (mixed && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.inspector.multi_edit.mixed_tooltip",
                    static_cast<int>(sel.selected.size())).c_str());
    }

    if (!edited) return false;

    // Click frame: snapshot before per-entity, propagar after, push command.
    const bool after = currentValue;
    std::vector<bool> beforeVec;
    beforeVec.reserve(sel.selected.size());
    for (const Entity& en : sel.selected) {
        if (en.handle() == activeEntity.handle()) {
            // Active: usar el snapshot pre-Checkbox (getter ya devolveria after).
            beforeVec.push_back(activeBefore);
        } else {
            beforeVec.push_back(static_cast<bool>(en) ? getter(en) : false);
        }
    }
    if (setter) {
        std::vector<typename MultiEditPropertyCommand<bool>::Entry> entries;
        entries.reserve(sel.selected.size());
        for (usize i = 0; i < sel.selected.size() && i < beforeVec.size(); ++i) {
            entries.push_back({sel.selected[i], beforeVec[i]});
        }
        // Aplicar after a TODOS los peers (active ya fue toggled por el
        // Checkbox; setter sobre `activeEntity` es idempotente).
        for (auto& en : entries) {
            if (!static_cast<bool>(en.entity)) continue;
            setter(en.entity, after);
        }
        auto cmd = std::make_unique<MultiEditPropertyCommand<bool>>(
            std::move(entries), after, setter,
            cmdLabel + " (" +
            std::to_string(sel.selected.size()) + ")");
        if (!cmd->isNoOp()) {
            HistoryStack* h = ui->historyStack();
            if (h != nullptr) h->push(std::move(cmd));
        }
    }
    (void)mTracker;  // no usado en este patron (cambio atomico, no drag)
    (void)sTracker;  // solo usado en el fallback single-entity (lo consume pushEditIfDone)
    return edited;
}

// Multi-edit de u32 con Combo (enums, asset ids). Cambio atomico — mismo
// patron que multiEditCheckbox.
inline bool multiEditCombo(MultiEditTracker& mTracker,
        InspectorEditTracker& sTracker, EditorUI* ui,
        Entity activeEntity,
        const std::string& labelKey, const char* idSuffix,
        u32& currentValue,
        const char* const* items, int itemCount,
        std::function<u32(Entity)> getter,
        typename EditPropertyCommand<u32>::Setter setter,
        const std::string& cmdLabel) {
    if (ui == nullptr) return false;
    const SelectionSet& sel = ui->selectionSet();

    if (sel.selected.size() <= 1u) {
        const std::string label = I18n::T(labelKey) + idSuffix;
        int curIdx = static_cast<int>(currentValue);
        const bool edited = ImGui::Combo(label.c_str(), &curIdx, items, itemCount);
        if (edited) {
            const u32 oldVal = currentValue;
            const u32 newVal = static_cast<u32>(curIdx);
            if (oldVal != newVal && setter) {
                currentValue = newVal;
                HistoryStack* h = ui->historyStack();
                if (h != nullptr) {
                    auto cmd = std::make_unique<EditPropertyCommand<u32>>(
                        activeEntity, oldVal, newVal, setter, cmdLabel);
                    h->push(std::move(cmd));
                }
            }
        }
        return edited;
    }

    const bool mixed = !allMatch<u32>(sel.selected, currentValue,
        getter, [](u32 a, u32 b) { return a == b; });

    std::string label = mixed
        ? std::string("\xE2\x80\x94 ") + I18n::T(labelKey)
        : I18n::T(labelKey);
    label += idSuffix;
    int curIdx = static_cast<int>(currentValue);
    const bool edited = ImGui::Combo(label.c_str(), &curIdx, items, itemCount);
    if (mixed && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.inspector.multi_edit.mixed_tooltip",
                    static_cast<int>(sel.selected.size())).c_str());
    }

    if (!edited) return false;

    // Snapshot del before del active ANTES de mutar `currentValue` —
    // getter(active) tras la asignacion devolveria el after.
    const u32 activeBefore = currentValue;
    const u32 after = static_cast<u32>(curIdx);
    if (activeBefore == after) return edited;  // no-op click
    currentValue = after;
    if (setter) {
        std::vector<typename MultiEditPropertyCommand<u32>::Entry> entries;
        entries.reserve(sel.selected.size());
        for (const Entity& en : sel.selected) {
            u32 b;
            if (en.handle() == activeEntity.handle()) {
                b = activeBefore;
            } else {
                b = static_cast<bool>(en) ? getter(en) : 0u;
            }
            entries.push_back({en, b});
        }
        // Aplicar after a peers (active ya tiene `currentValue = after`).
        for (auto& en : entries) {
            if (!static_cast<bool>(en.entity)) continue;
            if (en.entity.handle() == activeEntity.handle()) continue;
            setter(en.entity, after);
        }
        auto cmd = std::make_unique<MultiEditPropertyCommand<u32>>(
            std::move(entries), after, setter,
            cmdLabel + " (" +
            std::to_string(sel.selected.size()) + ")");
        if (!cmd->isNoOp()) {
            HistoryStack* h = ui->historyStack();
            if (h != nullptr) h->push(std::move(cmd));
        }
    }
    (void)mTracker;
    (void)sTracker;
    return edited;
}

// Multi-edit de glm::vec4 con ColorEdit4. Mismo drag-pattern que
// multiEditColor3 — alpha agregado para particle colorStart/End.
inline bool multiEditColor4(MultiEditTracker& mTracker,
        InspectorEditTracker& sTracker, EditorUI* ui,
        Entity activeEntity,
        const std::string& labelKey, const char* idSuffix,
        glm::vec4& currentValue,
        std::function<glm::vec4(Entity)> getter,
        typename EditPropertyCommand<glm::vec4>::Setter setter,
        const std::string& cmdLabel) {
    if (ui == nullptr) return false;
    const SelectionSet& sel = ui->selectionSet();

    if (sel.selected.size() <= 1u) {
        // Single-entity: ColorEdit4 + pushEditIfDone<vec4>.
        const std::string label = I18n::T(labelKey) + idSuffix;
        const bool edited = ImGui::ColorEdit4(label.c_str(), &currentValue.x);
        pushEditIfDone<glm::vec4>(sTracker, ui, activeEntity, currentValue,
                                    std::move(setter), cmdLabel);
        return edited;
    }

    // Multi-edit con drag pattern (igual que multiEditColor3 pero con
    // 4 componentes — alpha incluido en el epsilon-equal).
    const bool mixed = !allMatch<glm::vec4>(sel.selected, currentValue,
        getter,
        [](const glm::vec4& a, const glm::vec4& b) {
            constexpr f32 eps = 1.0f / 255.0f;
            return std::abs(a.x - b.x) < eps
                && std::abs(a.y - b.y) < eps
                && std::abs(a.z - b.z) < eps
                && std::abs(a.w - b.w) < eps;
        });

    std::string label = mixed
        ? std::string("\xE2\x80\x94 ") + I18n::T(labelKey)
        : I18n::T(labelKey);
    label += idSuffix;
    const bool edited = ImGui::ColorEdit4(label.c_str(), &currentValue.x);
    if (mixed && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.inspector.multi_edit.mixed_tooltip",
                    static_cast<int>(sel.selected.size())).c_str());
    }

    if (ImGui::IsItemActive() && setter) {
        for (const Entity& other : sel.selected) {
            if (!static_cast<bool>(other)) continue;
            if (other.handle() == activeEntity.handle()) continue;
            Entity mut = other;
            setter(mut, currentValue);
        }
    }

    const ImGuiID itemId = ImGui::GetItemID();
    if (ImGui::IsItemActivated()) {
        mTracker.activeId = itemId;
        mTracker.entities = sel.selected;
        std::vector<glm::vec4> before;
        before.reserve(sel.selected.size());
        for (const Entity& en : sel.selected) {
            before.push_back(static_cast<bool>(en)
                ? getter(en) : glm::vec4{0.0f});
        }
        mTracker.before = std::move(before);
    }

    if (ImGui::IsItemDeactivatedAfterEdit() && mTracker.activeId == itemId) {
        const glm::vec4 after = currentValue;
        const auto* beforePtr =
            std::get_if<std::vector<glm::vec4>>(&mTracker.before);
        if (beforePtr != nullptr && setter) {
            std::vector<typename MultiEditPropertyCommand<glm::vec4>::Entry> entries;
            entries.reserve(mTracker.entities.size());
            for (usize i = 0; i < mTracker.entities.size()
                              && i < beforePtr->size(); ++i) {
                entries.push_back({mTracker.entities[i], (*beforePtr)[i]});
            }
            for (auto& en : entries) {
                if (!static_cast<bool>(en.entity)) continue;
                setter(en.entity, en.before);
            }
            auto cmd = std::make_unique<
                MultiEditPropertyCommand<glm::vec4>>(
                std::move(entries), after, setter,
                cmdLabel + " (" +
                std::to_string(mTracker.entities.size()) + ")");
            if (!cmd->isNoOp()) {
                HistoryStack* h = ui->historyStack();
                if (h != nullptr) h->push(std::move(cmd));
            }
        }
        mTracker.reset();
    }

    return edited;
}

} // namespace Mood::detail

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
    // Orden de "plegar/expandir todo" de este frame (botones del toolbar).
    if (m_forceSectionState > 0) {
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    } else if (m_forceSectionState < 0) {
        ImGui::SetNextItemOpen(false, ImGuiCond_Always);
    }

    const bool open = ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen);

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
