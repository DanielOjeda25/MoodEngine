// F2H24: Inspector — BrushComponent (CSG) + UV editor (F2H15/F2H17).
// F2H33: + texture alignment (Align/Fit/Justify L/R/T/B + Treat as one face).
//
// break-B6c (auditoria): split de la función gigante. El cuerpo era una
// secuencia de header info + UV editor + alignment buttons. Extraemos el
// header info (read-only) y el bloque de alignment (170 LOC) a helpers
// file-local. El UV editor queda inline porque sus lambdas (applyToScope +
// captureSnapshot + pushCommandIfChanged) están muy acopladas con state
// local; sacarlo agregaria mas param-passing que la mejora justifica.

#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"
#include "editor/panels/scene/InspectorPanel_Materials.h"  // F3H29: helpers PBR/Shader/Blending

#include "core/Log.h"
#include "editor/commands/EditBrushUVCommand.h"  // BrushUVSnapshot
#include "editor/ui/EditorUI.h"
#include "editor/ui/IconsFontAwesome6.h"  // F3H30: ICON_FA_ROTATE_LEFT reset
#include "engine/assets/manager/AssetManager.h"
#include "core/i18n/I18n.h"  // F2H43
#include "engine/render/resources/MaterialAsset.h"  // F3H29
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/Components.h"
#include "engine/world/csg/BrushOps.h"  // F2H33: alignment helpers

#include <glm/geometric.hpp>  // F2H33: glm::dot
#include <imgui.h>

#include <cfloat>  // F3H29: FLT_MIN para BeginListBox width=fill
#include <cmath>   // F2H33: std::fabs
#include <cstdio>  // F3H29
#include <memory>
#include <string>
#include <utility>

namespace Mood {

namespace {

// F3H29 polish: bloque "Info" eliminado (era un CollapsingHeader con
// caras/AABB/mesh_cache/dirty). El dev lo consideró innecesario:
// *"para mí en render, el colapsable de info, eliminalo eso me parece
// innecesario"*. Si emerge demanda de debug, el Debug panel ya muestra
// estas stats globalmente.

// F3H29: UI Blender-style para los slots de material del brush.
// Espejo del patrón F3H9 Stage 9 del MeshRenderer: ListBox compacto
// arriba (4 rows visibles) + panel del slot seleccionado abajo con los
// helpers compartidos PBR / Shader / Blending. No incluye drop target
// porque los materiales del brush se asignan vía drag-drop sobre la
// cara en el viewport (F2H17), no desde el inspector.
void drawBrushMaterialsList(BrushComponent& bc, AssetManager* assets,
                              int& selectedSlot,
                              InspectorEditTracker& tracker,
                              EditorUI* ui, Entity e, bool& editedFlag) {
    const int slotCount = static_cast<int>(bc.materials.size());
    ImGui::Separator();
    ImGui::Text("%s",
        I18n::T("editor.panel.inspector.materials.list_header").c_str());

    if (slotCount == 0) {
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.inspector.materials.no_slots").c_str());
        return;
    }

    if (selectedSlot >= slotCount) selectedSlot = 0;
    if (selectedSlot < 0)          selectedSlot = 0;

    // (1) Lista compacta. Alto: max 4 filas visibles.
    {
        const float lineH = ImGui::GetTextLineHeightWithSpacing();
        const int   visibleRows = (slotCount < 4) ? slotCount : 4;
        const float listH = lineH * static_cast<float>(visibleRows)
                          + ImGui::GetStyle().FramePadding.y * 2.0f;
        if (ImGui::BeginListBox("##brush_mat_slot_list",
                                 ImVec2(-FLT_MIN, listH))) {
            for (int i = 0; i < slotCount; ++i) {
                const MaterialAssetId matId = bc.materials[i];
                std::string matPath;
                if (matId == 0) {
                    matPath = I18n::T(
                        "editor.panel.inspector.brush.blank_look");
                } else if (assets != nullptr) {
                    matPath = assets->materialPathOf(matId);
                }
                const bool selected = (i == selectedSlot);
                ImGui::PushID(i);
                char label[256];
                std::snprintf(label, sizeof(label), "%d  %s", i,
                              matPath.empty()
                                ? I18n::T("editor.panel.inspector.materials.no_material").c_str()
                                : matPath.c_str());
                if (ImGui::Selectable(label, selected)) {
                    selectedSlot = i;
                }
                ImGui::PopID();
            }
            ImGui::EndListBox();
        }
    }

    // (2) Panel del slot seleccionado — Surface + Shader + Blending,
    // todos colapsables y plegados por default (Blender-style).
    const usize i = static_cast<usize>(selectedSlot);
    const MaterialAssetId matId = bc.materials[i];
    ImGui::PushID(static_cast<int>(i));

    MaterialAsset* mat = (assets != nullptr && matId != 0)
        ? assets->getMaterial(matId) : nullptr;
    if (mat != nullptr) {
        // Brush nunca es skinned/instanced — pasar false a ambos.
        InspectorMaterials::drawPbrMultipliers(
            mat, assets, matId, tracker, ui, e, editedFlag);
        InspectorMaterials::drawShaderGraph(
            mat, assets, matId, ui,
            /*isSkinned=*/false, /*isInstanced=*/false, editedFlag);
        InspectorMaterials::drawBlending(
            mat, assets, matId, tracker, ui, e, editedFlag);
    } else {
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.inspector.materials.no_material").c_str());
    }
    ImGui::PopID();
}

// F2H33 Bloque D: texture alignment. Solo aplica en Face Mode. Botones operan
// sobre la cara active (single) o todas las seleccionadas (multi). El checkbox
// "Treat as one face" solo es visible cuando hay >1 caras y computa el bounding
// rect UV COMPARTIDO usando los axisU/V de la primary — para que las N caras
// compartan un solo wrap de textura coherente en lugar de fitear cada una por
// separado.
void drawBrushTextureAlignment(BrushComponent& bc, Entity e, i32 faceIdx,
                                 bool multiFace, const SelectionSet* selSet,
                                 const std::string& entityTag,
                                 EditorUI* ui, bool& treatAsOneFlag,
                                 bool& editedFlag) {
    if (!e.hasComponent<TransformComponent>()) return;
    ImGui::Separator();
    ImGui::TextDisabled("%s",
        I18n::T("editor.panel.inspector.brush.alignment").c_str());

    const auto& tf = e.getComponent<TransformComponent>();
    const glm::mat4 worldMat = tf.worldMatrix();

    if (multiFace) {
        const std::string treatLabel = I18n::T("editor.panel.inspector.brush.treat_as_one") + "##align";
        ImGui::Checkbox(treatLabel.c_str(), &treatAsOneFlag);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s",
                I18n::T("editor.panel.inspector.brush.treat_as_one_tooltip").c_str());
        }
    } else {
        treatAsOneFlag = false;  // reset si bajamos a single
    }
    const bool treatAsOne = multiFace && treatAsOneFlag;

    // Helper local: aplica `op(face, rect)` a las caras del scope
    // segun multiFace + treatAsOne, snapshotea y pushea command.
    auto runAlignmentOp = [&](const char* commandLabel, auto&& op) {
        if (ui == nullptr || ui->scene() == nullptr) return;
        BrushUVSnapshot pre = captureBrushUV(bc.brush);

        if (treatAsOne) {
            // Rect compartido en sistema de la primary.
            const u32 primaryIdx = static_cast<u32>(faceIdx);
            const Csg::FaceUvRect sharedRect =
                Csg::computeFaceUvRect(bc.brush, primaryIdx, worldMat);
            if (!sharedRect.isValid()) {
                Log::editor()->warn(
                    "Alignment: rect de la cara primary "
                    "degenerado, skipeando op");
                return;
            }
            // Heuristica: si las caras secundarias tienen axis muy distintos
            // a la primary, log warn (el resultado visual puede salir raro).
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
        editedFlag = true;
        BrushUVSnapshot post = captureBrushUV(bc.brush);
        if (snapshotsEqual(pre, post)) return;
        if (HistoryStack* h = ui->historyStack()) {
            h->push(std::make_unique<EditBrushUVCommand>(
                ui->scene(), entityTag,
                std::move(pre), std::move(post),
                std::string{commandLabel}));
        }
    };

    // Layout: Align (full width) + grid 2x2 Fit/Justify.
    const f32 buttonW = (ImGui::GetContentRegionAvail().x - 8.0f) * 0.5f;
    const ImVec2 buttonSize(buttonW, 0.0f);

    const std::string alignBtn = I18n::T("editor.panel.inspector.brush.align_to_face") + "##align";
    if (ImGui::Button(alignBtn.c_str(), ImVec2(-1, 0))) {
        // Align ignora el rect — resetea el axis sin medir el poligono.
        // Lo wrappeamos en una op compatible con runAlignmentOp pasando un
        // rect dummy.
        runAlignmentOp("Align to face",
            [](Csg::BrushFace& f, const Csg::FaceUvRect&) {
                Csg::alignFaceToFace(f);
            });
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.panel.inspector.brush.align_tooltip").c_str());
    }

    const std::string fitBtn = I18n::T("editor.panel.inspector.brush.fit") + "##align";
    if (ImGui::Button(fitBtn.c_str(), buttonSize)) {
        runAlignmentOp("Fit",
            [](Csg::BrushFace& f, const Csg::FaceUvRect& r) {
                Csg::fitFaceToRect(f, r);
            });
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.panel.inspector.brush.fit_tooltip").c_str());
    }
    ImGui::SameLine();
    const std::string justLBtn = I18n::T("editor.panel.inspector.brush.justify_l") + "##align";
    if (ImGui::Button(justLBtn.c_str(), buttonSize)) {
        runAlignmentOp("Justify L",
            [](Csg::BrushFace& f, const Csg::FaceUvRect& r) {
                Csg::justifyFaceToRect(f, r, Csg::JustifySide::Left);
            });
    }
    const std::string justRBtn = I18n::T("editor.panel.inspector.brush.justify_r") + "##align";
    if (ImGui::Button(justRBtn.c_str(), buttonSize)) {
        runAlignmentOp("Justify R",
            [](Csg::BrushFace& f, const Csg::FaceUvRect& r) {
                Csg::justifyFaceToRect(f, r, Csg::JustifySide::Right);
            });
    }
    ImGui::SameLine();
    const std::string justTBtn = I18n::T("editor.panel.inspector.brush.justify_t") + "##align";
    if (ImGui::Button(justTBtn.c_str(), buttonSize)) {
        runAlignmentOp("Justify T",
            [](Csg::BrushFace& f, const Csg::FaceUvRect& r) {
                Csg::justifyFaceToRect(f, r, Csg::JustifySide::Top);
            });
    }
    const std::string justBBtn = I18n::T("editor.panel.inspector.brush.justify_b") + "##align";
    if (ImGui::Button(justBBtn.c_str(), buttonSize)) {
        runAlignmentOp("Justify B",
            [](Csg::BrushFace& f, const Csg::FaceUvRect& r) {
                Csg::justifyFaceToRect(f, r, Csg::JustifySide::Bottom);
            });
    }
}

} // namespace

// BrushComponent (F2H11)
// Read-only en F2H11. F2H13 agrega edicion de primitivas (cilindro,
// esfera, etc.); F2H14 agrega editor de UV per-cara con drag &
// drop de materiales; F2H15 agrega seleccion de cara individual.
void InspectorPanel::renderBrushSection(Entity e) {
    auto& bc = e.getComponent<BrushComponent>();
    if (!beginComponentSection<BrushComponent>(e, ICON_FA_CUBES_STACKED " Brush (CSG)")) return;

    // F3H29: lista Blender-style de materiales del brush (slot
    // seleccionable + panel PBR/Shader/Blending). Antes era una lista
    // TextDisabled read-only sin edición posible — la UI quedaba muy
    // asimétrica vs MeshRenderer.
    drawBrushMaterialsList(bc, m_assets, m_selectedMaterialSlot,
                            m_editTracker, m_ui, e, m_editedThisFrame);

    // F3H29 polish: UV editor envuelto en CollapsingHeader colapsado por
    // default. Antes los 4 widgets de UV (scale/rotation/offset/lock)
    // ocupaban espacio fijo al fondo del Inspector aunque el dev no
    // estuviera tocando UVs. Ahora el dev opt-in.
    if (!ImGui::CollapsingHeader(
            I18n::T("editor.panel.inspector.brush.uv").c_str())) {
        return;
    }

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

    // F3H29 polish: el hint "UV (Brush)" redundante fue eliminado del
    // modo object. En face mode sí se muestra cuántas caras están en
    // edición (info funcional, no decorativa).
    if (multiFace) {
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.inspector.brush.uv_multi",
                    selectedFaceCount, faceIdx).c_str());
    } else if (faceMode) {
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.inspector.brush.uv_face", faceIdx).c_str());
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

        // F3H30 polish: reset buttons inline (rotate-ccw) para cada UV
        // widget. Captura snapshot pre-reset, aplica default, pushea
        // command — undoable como cualquier edit manual.
        auto uvResetButton = [&](const char* widgetId,
                                  const std::string& undoLabel,
                                  auto&& applyDefault) {
            ImGui::SameLine();
            const std::string btn = std::string(ICON_FA_ROTATE_LEFT) + "##" + widgetId;
            if (ImGui::SmallButton(btn.c_str())) {
                BrushUVSnapshot pre = captureBrushUV(bc.brush);
                applyToScope(std::forward<decltype(applyDefault)>(applyDefault));
                bc.dirty = true;
                if (m_ui != nullptr && m_ui->scene() != nullptr) {
                    if (HistoryStack* h = m_ui->historyStack()) {
                        BrushUVSnapshot post = captureBrushUV(bc.brush);
                        if (!snapshotsEqual(pre, post)) {
                            h->push(std::make_unique<EditBrushUVCommand>(
                                m_ui->scene(), entityTag,
                                std::move(pre), std::move(post),
                                undoLabel));
                        }
                    }
                }
                m_editedThisFrame = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s",
                    I18n::T("editor.common.reset_default").c_str());
            }
        };

        glm::vec2 uvScale = faceRef.uvScale;
        const std::string uvScaleLabel = I18n::T("editor.panel.inspector.brush.uv_scale") + "##uvbrush";
        if (ImGui::DragFloat2(uvScaleLabel.c_str(), &uvScale.x,
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
        uvResetButton("reset_uv_scale", "Reset UV scale",
            [](auto& f) { f.uvScale = glm::vec2(1.0f); });

        f32 uvRotDeg = glm::degrees(faceRef.uvRotation);
        const std::string uvRotLabel = I18n::T("editor.panel.inspector.brush.uv_rotation") + "##uvbrush";
        if (ImGui::DragFloat(uvRotLabel.c_str(),
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
        uvResetButton("reset_uv_rot", "Reset UV rotation",
            [](auto& f) { f.uvRotation = 0.0f; });

        glm::vec2 uvOffset = faceRef.uvOffset;
        const std::string uvOffsetLabel = I18n::T("editor.panel.inspector.brush.uv_offset") + "##uvbrush";
        if (ImGui::DragFloat2(uvOffsetLabel.c_str(), &uvOffset.x,
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
        uvResetButton("reset_uv_offset", "Reset UV offset",
            [](auto& f) { f.uvOffset = glm::vec2(0.0f); });

        // Checkbox: instantaneo. Capturar pre + post al click + push.
        bool lockToWorld = faceRef.lockToWorld;
        BrushUVSnapshot lockPreSnap;
        const std::string lockLabel = I18n::T("editor.panel.inspector.brush.lock_to_world") + "##uvbrush";
        const bool lockChanged = [&]() {
            if (ImGui::Checkbox(lockLabel.c_str(), &lockToWorld)) {
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
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.inspector.brush.locked_count",
                    lockedCount,
                    static_cast<u32>(bc.brush.faces.size())).c_str());

        // --- F2H33 Bloque D: texture alignment ---
        if (faceMode) {
            drawBrushTextureAlignment(bc, e, faceIdx, multiFace, selSet,
                                         entityTag, m_ui, m_treatAsOneFace,
                                         m_editedThisFrame);
        }
    }

    ImGui::Separator();
}

} // namespace Mood
