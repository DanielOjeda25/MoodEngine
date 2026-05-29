#pragma once

// F4H1.5 split — multi-edit helpers extraidos de InspectorPanel_Internal.h
// (885 LOC sobre hard cap 800). 5 templates de 60-150 LOC cada uno para
// edits sincronizados sobre N entities selected. Single-edit helpers se
// quedan en `_Internal.h`.
//
// Convencion: cada multi-edit detecta si la selection es 1 entity
// (fallback al path single-edit) o N (drag/atomic pattern segun el
// widget). Mixed-detection compara active vs peers via getter; un
// em-dash prefix indica "mixed" en el widget label.
//
// 2 patterns:
// - drag pattern (ColorEdit3/4, DragFloat): snapshot al activate, live
//   preview en peers durante drag, push command al deactivate-after-edit.
// - atomic pattern (Checkbox, Combo): snapshot del before + apply after +
//   push command todo en el frame del click.
//
// NO incluir desde otro modulo — header privado de Inspector*.
//
// Incluir SOLO desde InspectorPanel_Internal.h (que ya tiene los
// includes necesarios). Si emerge demanda de incluir directamente,
// duplicar los includes de _Internal.h aqui.

namespace Mood::detail {

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
