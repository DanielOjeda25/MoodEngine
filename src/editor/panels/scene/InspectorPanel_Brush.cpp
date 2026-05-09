// F2H24: Inspector — BrushComponent (CSG) + UV editor (F2H15/F2H17).
// F2H33: + texture alignment (Align/Fit/Justify L/R/T/B + Treat as one face).

#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"

#include "core/Log.h"
#include "editor/commands/EditBrushUVCommand.h"  // BrushUVSnapshot
#include "editor/ui/EditorUI.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/Components.h"
#include "engine/world/csg/BrushOps.h"  // F2H33: alignment helpers

#include <glm/geometric.hpp>  // F2H33: glm::dot
#include <imgui.h>

#include <cmath>  // F2H33: std::fabs
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
    ImGui::SeparatorText(ICON_FA_CUBES_STACKED " Brush (CSG)");

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

    // --- F2H15 + F2H17 + F2H33: UV editor ---
    // F2H17: si Face Mode + activeFaceIndex >= 0, los widgets editan
    // SOLO esa cara. Sino, fallback al modo global F2H15 (todas las
    // caras del brush).
    // F2H33: si hay > 1 cara en selectedFaceIndices (Shift+click multi-
    // select), los widgets editan TODAS las seleccionadas a la vez. Los
    // valores mostrados en los sliders son los de la "active" (=
    // selectedFaceIndices.back(), la mas recientemente clickeada).
    const SelectionSet* selSet = (m_ui != nullptr)
        ? &m_ui->selectionSet() : nullptr;
    const bool faceMode = (selSet != nullptr) &&
        (m_ui->subMode() == EditorSubMode::Face) &&
        (selSet->activeFaceIndex() >= 0) &&
        (static_cast<u32>(selSet->activeFaceIndex())
            < bc.brush.faces.size());
    const i32 faceIdx = faceMode ? selSet->activeFaceIndex() : -1;
    const usize selectedFaceCount = faceMode
        ? selSet->selectedFaceIndices.size() : 0;
    const bool multiFace = (selectedFaceCount > 1);

    if (multiFace) {
        ImGui::TextDisabled("UV (%zu caras, primary=#%d)",
                             selectedFaceCount, faceIdx);
    } else if (faceMode) {
        ImGui::TextDisabled("UV (Cara %d)", faceIdx);
    } else {
        ImGui::TextDisabled("UV (Brush)");
    }

    if (!bc.brush.faces.empty()) {
        // En faceMode, el "representante" es la cara activa (la primary);
        // en object mode, sigue siendo la cara 0.
        auto& faceRef = faceMode ? bc.brush.faces[faceIdx]
                                   : bc.brush.faces[0];
        const std::string entityTag = e.hasComponent<TagComponent>()
            ? e.getComponent<TagComponent>().name : std::string{};

        // Helper: aplicar mutacion segun scope.
        //   - Object mode: TODAS las caras del brush (F2H15 global).
        //   - Face mode + multi-select: las N caras del set (F2H33).
        //   - Face mode + single: solo la activa (F2H17).
        auto applyToScope = [&](auto&& fn) {
            if (!faceMode) {
                for (auto& f : bc.brush.faces) fn(f);
            } else if (multiFace) {
                for (i32 idxSigned : selSet->selectedFaceIndices) {
                    if (idxSigned < 0) continue;
                    const u32 idx = static_cast<u32>(idxSigned);
                    if (idx < bc.brush.faces.size()) fn(bc.brush.faces[idx]);
                }
            } else {
                fn(bc.brush.faces[faceIdx]);
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
        pushCommandIfChanged(
            multiFace ? "Editar UV scale (N caras)"
                      : (faceMode ? "Editar UV scale (cara)"
                                   : "Editar UV scale"));

        f32 uvRotDeg = glm::degrees(faceRef.uvRotation);
        if (ImGui::DragFloat("uv rotation (deg)##uvbrush",
                                &uvRotDeg, 1.0f, -360.0f, 360.0f)) {
            const f32 uvRotRad = glm::radians(uvRotDeg);
            applyToScope([&](auto& f) { f.uvRotation = uvRotRad; });
            bc.dirty = true;
            m_editedThisFrame = true;
        }
        captureSnapshotIfActivated();
        pushCommandIfChanged(
            multiFace ? "Editar UV rotation (N caras)"
                      : (faceMode ? "Editar UV rotation (cara)"
                                   : "Editar UV rotation"));

        glm::vec2 uvOffset = faceRef.uvOffset;
        if (ImGui::DragFloat2("uv offset##uvbrush", &uvOffset.x,
                                0.05f)) {
            applyToScope([&](auto& f) { f.uvOffset = uvOffset; });
            bc.dirty = true;
            m_editedThisFrame = true;
        }
        captureSnapshotIfActivated();
        pushCommandIfChanged(
            multiFace ? "Editar UV offset (N caras)"
                      : (faceMode ? "Editar UV offset (cara)"
                                   : "Editar UV offset"));

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
                    multiFace ? std::string{"Toggle lock-to-world (N caras)"}
                              : (faceMode
                                  ? std::string{"Toggle lock-to-world (cara)"}
                                  : std::string{"Toggle lock-to-world"})));
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

        // --- F2H33 Bloque D: texture alignment ---
        // Solo aplica en Face Mode. Botones operan sobre la cara active
        // (single) o todas las seleccionadas (multi). El checkbox
        // "Treat as one face" solo es visible cuando hay >1 caras y
        // computa el bounding rect UV COMPARTIDO usando los axisU/V
        // de la primary — para que las N caras compartan un solo wrap
        // de textura coherente en lugar de fitear cada una por separado.
        if (faceMode && e.hasComponent<TransformComponent>()) {
            ImGui::Separator();
            ImGui::TextDisabled("Alineacion");

            const auto& tf = e.getComponent<TransformComponent>();
            const glm::mat4 worldMat = tf.worldMatrix();

            if (multiFace) {
                ImGui::Checkbox("Treat as one face##align",
                                 &m_treatAsOneFace);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(
                        "Computa un solo bounding rect UV compartido\n"
                        "(usando el sistema de la cara primary) y aplica\n"
                        "la op a las N caras con ese rect — la textura\n"
                        "queda coherente entre las caras seleccionadas.\n"
                        "Off = cada cara se procesa con su propio rect.");
                }
            } else {
                m_treatAsOneFace = false;  // reset si bajamos a single
            }
            const bool treatAsOne = multiFace && m_treatAsOneFace;

            // Helper local: aplica `op(face, rect)` a las caras del scope
            // segun multiFace + treatAsOne, snapshotea y pushea command.
            auto runAlignmentOp = [&](const char* commandLabel,
                                       auto&& op) {
                if (m_ui == nullptr || m_ui->scene() == nullptr) return;
                BrushUVSnapshot pre = captureBrushUV(bc.brush);

                if (treatAsOne) {
                    // Rect compartido en sistema de la primary.
                    const u32 primaryIdx = static_cast<u32>(faceIdx);
                    const Csg::FaceUvRect sharedRect =
                        Csg::computeFaceUvRect(bc.brush, primaryIdx,
                                                worldMat);
                    if (!sharedRect.isValid()) {
                        Log::editor()->warn(
                            "Alignment: rect de la cara primary "
                            "degenerado, skipeando op");
                        return;
                    }
                    // Heuristica: si las caras secundarias tienen axis
                    // muy distintos a la primary, log warn (el resultado
                    // visual puede salir raro).
                    const auto& primary = bc.brush.faces[primaryIdx];
                    bool axisMismatch = false;
                    for (i32 idxSigned : selSet->selectedFaceIndices) {
                        if (idxSigned < 0) continue;
                        const u32 idx = static_cast<u32>(idxSigned);
                        if (idx >= bc.brush.faces.size()) continue;
                        if (idx == primaryIdx) continue;
                        const auto& f = bc.brush.faces[idx];
                        const f32 dotU = std::fabs(glm::dot(f.uAxis, primary.uAxis));
                        const f32 dotV = std::fabs(glm::dot(f.vAxis, primary.vAxis));
                        if (dotU < 0.99f || dotV < 0.99f) {
                            axisMismatch = true;
                            break;
                        }
                    }
                    if (axisMismatch) {
                        Log::editor()->warn(
                            "Alignment 'Treat as one': caras con axisU/V "
                            "distintos a la primary — resultado visual "
                            "puede ser raro");
                    }
                    for (i32 idxSigned : selSet->selectedFaceIndices) {
                        if (idxSigned < 0) continue;
                        const u32 idx = static_cast<u32>(idxSigned);
                        if (idx < bc.brush.faces.size()) {
                            op(bc.brush.faces[idx], sharedRect);
                        }
                    }
                } else if (multiFace) {
                    // Cada cara con su propio rect.
                    for (i32 idxSigned : selSet->selectedFaceIndices) {
                        if (idxSigned < 0) continue;
                        const u32 idx = static_cast<u32>(idxSigned);
                        if (idx >= bc.brush.faces.size()) continue;
                        const Csg::FaceUvRect rect =
                            Csg::computeFaceUvRect(bc.brush, idx, worldMat);
                        op(bc.brush.faces[idx], rect);
                    }
                } else {
                    // Single face.
                    const u32 idx = static_cast<u32>(faceIdx);
                    const Csg::FaceUvRect rect =
                        Csg::computeFaceUvRect(bc.brush, idx, worldMat);
                    op(bc.brush.faces[idx], rect);
                }

                bc.dirty = true;
                m_editedThisFrame = true;
                BrushUVSnapshot post = captureBrushUV(bc.brush);
                if (snapshotsEqual(pre, post)) return;
                if (HistoryStack* h = m_ui->historyStack()) {
                    h->push(std::make_unique<EditBrushUVCommand>(
                        m_ui->scene(), entityTag,
                        std::move(pre), std::move(post),
                        std::string{commandLabel}));
                }
            };

            // Layout: Align (full width) + grid 2x2 Fit/Justify.
            const f32 buttonW = (ImGui::GetContentRegionAvail().x - 8.0f) * 0.5f;
            const ImVec2 buttonSize(buttonW, 0.0f);

            if (ImGui::Button("Align to face##align", ImVec2(-1, 0))) {
                // Align ignora el rect — resetea el axis sin medir el
                // poligono. Lo wrappeamos en una op compatible con
                // runAlignmentOp pasando un rect dummy.
                runAlignmentOp("Align to face",
                    [](Csg::BrushFace& f, const Csg::FaceUvRect&) {
                        Csg::alignFaceToFace(f);
                    });
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "Resetea axisU/V derivados de la normal (default Quake)\n"
                    "+ scale=1, offset=0, rotation=0. Preserva lockToWorld.");
            }

            if (ImGui::Button("Fit##align", buttonSize)) {
                runAlignmentOp("Fit",
                    [](Csg::BrushFace& f, const Csg::FaceUvRect& r) {
                        Csg::fitFaceToRect(f, r);
                    });
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "Setea scale + offset para que la textura cubra\n"
                    "exactamente el rect 2D de la cara (UV ∈ [0,1]).");
            }
            ImGui::SameLine();
            if (ImGui::Button("Justify L##align", buttonSize)) {
                runAlignmentOp("Justify L",
                    [](Csg::BrushFace& f, const Csg::FaceUvRect& r) {
                        Csg::justifyFaceToRect(f, r, Csg::JustifySide::Left);
                    });
            }
            if (ImGui::Button("Justify R##align", buttonSize)) {
                runAlignmentOp("Justify R",
                    [](Csg::BrushFace& f, const Csg::FaceUvRect& r) {
                        Csg::justifyFaceToRect(f, r, Csg::JustifySide::Right);
                    });
            }
            ImGui::SameLine();
            if (ImGui::Button("Justify T##align", buttonSize)) {
                runAlignmentOp("Justify T",
                    [](Csg::BrushFace& f, const Csg::FaceUvRect& r) {
                        Csg::justifyFaceToRect(f, r, Csg::JustifySide::Top);
                    });
            }
            if (ImGui::Button("Justify B##align", buttonSize)) {
                runAlignmentOp("Justify B",
                    [](Csg::BrushFace& f, const Csg::FaceUvRect& r) {
                        Csg::justifyFaceToRect(f, r, Csg::JustifySide::Bottom);
                    });
            }
        }
    }

    ImGui::Separator();
}

} // namespace Mood
