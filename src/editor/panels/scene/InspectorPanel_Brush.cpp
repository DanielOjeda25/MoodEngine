// F2H24: Inspector — BrushComponent (CSG) + UV editor (F2H15/F2H17).

#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"

#include "editor/commands/EditBrushUVCommand.h"  // BrushUVSnapshot
#include "editor/ui/EditorUI.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/Components.h"

#include <imgui.h>

#include <memory>
#include <string>
#include <utility>

namespace Mood {

// BrushComponent (F2H11)
// Read-only en F2H11. F2H13 agrega edicion de primitivas (cilindro,
// esfera, etc.); F2H14 agrega editor de UV per-cara con drag &
// drop de materiales; F2H15 agrega seleccion de cara individual.
void InspectorPanel::renderBrushSection(Entity e) {
    auto& bc = e.getComponent<BrushComponent>();
    ImGui::TextDisabled("Brush (CSG)");

    ImGui::Text("faces: %u", static_cast<u32>(bc.brush.faces.size()));

    const glm::vec3 size = bc.brush.localAabb.size();
    ImGui::TextDisabled("local AABB: %.2f x %.2f x %.2f",
                         static_cast<double>(size.x),
                         static_cast<double>(size.y),
                         static_cast<double>(size.z));

    if (m_assets != nullptr) {
        // F2H17: el brush tiene N slots de material (uno por
        // material distinto entre las caras). Mostrar todos.
        ImGui::Text("materiales: %u slots",
                     static_cast<u32>(bc.materials.size()));
        for (u32 i = 0; i < bc.materials.size(); ++i) {
            const MaterialAssetId mid = bc.materials[i];
            const std::string matPath = (mid == 0)
                ? std::string{"(blank look)"}
                : m_assets->materialPathOf(mid);
            ImGui::TextDisabled("  [%u] %s (id %u)", i,
                                   matPath.c_str(),
                                   static_cast<unsigned>(mid));
        }
    }

    ImGui::TextDisabled("mesh cache: %u submeshes",
                         static_cast<u32>(bc.meshCache.size()));
    ImGui::TextDisabled("dirty: %s", bc.dirty ? "si" : "no");

    // Recompute mesh: util para debug si la mesh quedo stale por
    // un cambio que no marcamos dirty (no deberia pasar pero es
    // breadcrumb util cuando F2H12+ agreguen booleanos).
    if (ImGui::Button("Recompute mesh")) {
        bc.dirty = true;
    }

    ImGui::Separator();

    // --- F2H15 + F2H17: UV editor ---
    // F2H17: si Face Mode + activeFaceIndex >= 0, los widgets
    // editan SOLO esa cara. Sino, fallback al modo global F2H15
    // (todas las caras). El header cambia entre "UV (Brush)" y
    // "UV (Cara N)" para feedback visual.
    const bool faceMode = (m_ui != nullptr) &&
        (m_ui->subMode() == EditorSubMode::Face) &&
        (m_ui->selectionSet().activeFaceIndex >= 0) &&
        (static_cast<u32>(m_ui->selectionSet().activeFaceIndex)
            < bc.brush.faces.size());
    const i32 faceIdx = faceMode
        ? m_ui->selectionSet().activeFaceIndex : -1;

    if (faceMode) {
        ImGui::TextDisabled("UV (Cara %d)", faceIdx);
    } else {
        ImGui::TextDisabled("UV (Brush)");
    }

    if (!bc.brush.faces.empty()) {
        // En faceMode, el "representante" es la cara activa;
        // en object mode, sigue siendo la cara 0.
        auto& faceRef = faceMode ? bc.brush.faces[faceIdx]
                                   : bc.brush.faces[0];
        const std::string entityTag = e.hasComponent<TagComponent>()
            ? e.getComponent<TagComponent>().name : std::string{};

        // Helper: aplicar mutacion a TODAS las caras (object mode)
        // o solo a la cara activa (face mode).
        auto applyToScope = [&](auto&& fn) {
            if (faceMode) {
                fn(bc.brush.faces[faceIdx]);
            } else {
                for (auto& f : bc.brush.faces) fn(f);
            }
        };

        auto captureSnapshotIfActivated = [&]() {
            if (ImGui::IsItemActivated()) {
                m_uvSnapshotPre = captureBrushUV(bc.brush);
                m_uvSnapshotValid = true;
                m_uvSnapshotEntityTag = entityTag;
            }
        };
        auto pushCommandIfChanged = [&](const char* label) {
            if (!ImGui::IsItemDeactivatedAfterEdit()) return;
            if (!m_uvSnapshotValid) return;
            if (m_ui == nullptr || m_ui->scene() == nullptr) return;
            const BrushUVSnapshot postSnap = captureBrushUV(bc.brush);
            if (snapshotsEqual(m_uvSnapshotPre, postSnap)) {
                m_uvSnapshotValid = false;
                return;
            }
            if (HistoryStack* h = m_ui->historyStack()) {
                h->push(std::make_unique<EditBrushUVCommand>(
                    m_ui->scene(), m_uvSnapshotEntityTag,
                    std::move(m_uvSnapshotPre), std::move(postSnap),
                    std::string{label}));
            }
            m_uvSnapshotValid = false;
        };

        glm::vec2 uvScale = faceRef.uvScale;
        if (ImGui::DragFloat2("uv scale##uvbrush", &uvScale.x,
                                0.05f, 0.01f, 100.0f)) {
            applyToScope([&](auto& f) { f.uvScale = uvScale; });
            bc.dirty = true;
            m_editedThisFrame = true;
        }
        captureSnapshotIfActivated();
        pushCommandIfChanged(faceMode ? "Editar UV scale (cara)"
                                        : "Editar UV scale");

        f32 uvRotDeg = glm::degrees(faceRef.uvRotation);
        if (ImGui::DragFloat("uv rotation (deg)##uvbrush",
                                &uvRotDeg, 1.0f, -360.0f, 360.0f)) {
            const f32 uvRotRad = glm::radians(uvRotDeg);
            applyToScope([&](auto& f) { f.uvRotation = uvRotRad; });
            bc.dirty = true;
            m_editedThisFrame = true;
        }
        captureSnapshotIfActivated();
        pushCommandIfChanged(faceMode ? "Editar UV rotation (cara)"
                                        : "Editar UV rotation");

        glm::vec2 uvOffset = faceRef.uvOffset;
        if (ImGui::DragFloat2("uv offset##uvbrush", &uvOffset.x,
                                0.05f)) {
            applyToScope([&](auto& f) { f.uvOffset = uvOffset; });
            bc.dirty = true;
            m_editedThisFrame = true;
        }
        captureSnapshotIfActivated();
        pushCommandIfChanged(faceMode ? "Editar UV offset (cara)"
                                        : "Editar UV offset");

        // Checkbox: instantaneo. Capturar pre + post al click + push.
        bool lockToWorld = faceRef.lockToWorld;
        BrushUVSnapshot lockPreSnap;
        const bool lockChanged = [&]() {
            if (ImGui::Checkbox("lock to world##uvbrush", &lockToWorld)) {
                lockPreSnap = captureBrushUV(bc.brush);
                applyToScope([&](auto& f) { f.lockToWorld = lockToWorld; });
                bc.dirty = true;
                // Recompute cache flag global (puede haber otras
                // caras todavia con lock-to-world).
                bc.anyFaceLockToWorld = false;
                for (const auto& f : bc.brush.faces) {
                    if (f.lockToWorld) {
                        bc.anyFaceLockToWorld = true;
                        break;
                    }
                }
                m_editedThisFrame = true;
                return true;
            }
            return false;
        }();
        if (lockChanged && m_ui != nullptr && m_ui->scene() != nullptr) {
            if (HistoryStack* h = m_ui->historyStack()) {
                BrushUVSnapshot postSnap = captureBrushUV(bc.brush);
                h->push(std::make_unique<EditBrushUVCommand>(
                    m_ui->scene(), entityTag,
                    std::move(lockPreSnap), std::move(postSnap),
                    faceMode ? std::string{"Toggle lock-to-world (cara)"}
                             : std::string{"Toggle lock-to-world"}));
            }
        }

        // Conteo informativo de caras con lock-to-world (util
        // cuando F2H17 permita per-cara y haya mezcla).
        u32 lockedCount = 0;
        for (const auto& f : bc.brush.faces) {
            if (f.lockToWorld) ++lockedCount;
        }
        ImGui::TextDisabled("%u/%u caras con lock-to-world",
                              lockedCount,
                              static_cast<u32>(bc.brush.faces.size()));
    }

    ImGui::Separator();
}

} // namespace Mood
