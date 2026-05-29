// F2H24: Inspector — MeshRendererComponent (mesh + materiales + drop).
//
// break-B6b (auditoria): split de la función gigante. El cuerpo era un loop
// por material slot con 4 sub-bloques inline (PBR multipliers / shader
// graph picker / blending / drop target). Cada uno pasa a un helper
// file-local. La función queda en ~25 LOC + loop que despacha. Cero cambio
// de comportamiento.

#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"
#include "editor/panels/scene/InspectorPanel_Materials.h"  // F3H29: helpers compartidos
#include "editor/ui/DragDropFeedback.h"  // F3H17: halo overlay

#include "core/Log.h"
#include "editor/ui/EditorUI.h"
#include "engine/assets/manager/AssetManager.h"
#include "core/i18n/I18n.h"  // F2H43
#include "engine/render/resources/MaterialAsset.h"
#include "engine/render/resources/MeshAsset.h"
#include "engine/scene/components/Components.h"

#include <imgui.h>

#include <cfloat>   // F3H9 Stage 9: FLT_MIN para BeginListBox width=fill
#include <cstdio>
#include <string>

namespace Mood {

namespace {

// F2H81: las stats read-only del mesh (submeshes, vertices, LODs,
// distancias) volcaban inline y mareaban. Las metemos en un foldout
// colapsado por defecto — Unity tampoco las muestra arriba; el path del
// mesh + los materiales editables quedan visibles, el detalle tecnico
// queda a un clic.
void drawMeshTechDetails(MeshAsset* asset) {
    if (asset == nullptr) return;
    if (!ImGui::CollapsingHeader(
            I18n::T("editor.panel.inspector.mesh.tech_details").c_str())) {
        return;
    }
    ImGui::Text("%s",
        I18n::T("editor.panel.inspector.mesh.submeshes_vertices",
                static_cast<u32>(asset->submeshes.size()),
                asset->totalVertexCount()).c_str());

    // F2H6: info de LODs (read-only en v1). Editar manualmente
    // o regenerar = hito futuro.
    const u32 lod0Tris = asset->totalVertexCount() / 3;
    u32 lod1Tris = 0, lod2Tris = 0;
    for (const auto& s : asset->lod1Submeshes) lod1Tris += s.vertexCount / 3;
    for (const auto& s : asset->lod2Submeshes) lod2Tris += s.vertexCount / 3;
    if (lod1Tris > 0 || lod2Tris > 0) {
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.inspector.mesh.lod0_tris", lod0Tris).c_str());
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.inspector.mesh.lod1_tris", lod1Tris).c_str());
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.inspector.mesh.lod2_tris", lod2Tris).c_str());
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.inspector.mesh.lod_distances",
                    static_cast<double>(asset->lodDistances.x),
                    static_cast<double>(asset->lodDistances.y)).c_str());
    } else if (asset->hasSkeleton()) {
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.inspector.mesh.lods_skinned").c_str());
    } else {
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.inspector.mesh.lods_na").c_str());
    }
}

// F3H29: implementaciones de PBR/Shader/Blending movidas a
// `InspectorPanel_Materials.cpp` (compartidas con el Brush). Lo que
// queda local en este archivo es lo específico de MeshRenderer:
// `drawMeshTechDetails` (LOD info del MeshAsset) y `drawMaterialDropTarget`
// (este último muta `MeshRendererComponent.materials`, no el
// `MaterialAsset`).

// Hito 35 A: drop de textura del AssetBrowser sobre este slot -> reemplaza
// el material entero por uno nuevo (instance unico) con la textura como
// albedo. Anti-contagio: nunca muta el material previo (otras entidades
// pueden compartirlo). Hito 36 A: ahora undoable — push de
// EditPropertyCommand<u32> (MaterialAssetId == u32) con setter que indexa
// el slot via captura por valor. Si no hay history disponible, fallback a
// asignacion directa. F2H23: highlight visual del drop target cuando el
// dev tiene un drag activo de textura — color verde claro para que el
// ojo encuentre rapido los slots aceptables.
void drawMaterialDropTarget(Entity e, MeshRendererComponent& mr, usize i,
                              AssetManager* assets, EditorUI* ui,
                              bool& editedFlag) {
    const bool dragTex = detail::isDragActiveOfType("MOOD_TEXTURE_ASSET");
    if (dragTex) {
        ImGui::PushStyleColor(ImGuiCol_Button,
                                ImVec4(0.20f, 0.55f, 0.25f, 1.0f));
    }
    ImGui::Button(I18n::T("editor.panel.inspector.mesh.drop_replace").c_str(),
                    ImVec2(-FLT_MIN, 0));
    if (dragTex) ImGui::PopStyleColor();
    // F3H17: halo overlay durante drag activo de textura. Verde si el
    // cursor esta sobre este boton (drop OK al soltar), cyan si esta
    // en otro lado.
    if (dragTex) {
        DragDropFeedback::drawItemDropHalo(ImGui::IsItemHovered());
    }
    if (!ImGui::BeginDragDropTarget()) return;
    if (const ImGuiPayload* p =
            ImGui::AcceptDragDropPayload("MOOD_TEXTURE_ASSET")) {
        if (assets != nullptr && p->DataSize == sizeof(TextureAssetId)) {
            const TextureAssetId tex =
                *static_cast<const TextureAssetId*>(p->Data);
            const MaterialAssetId oldMatId = mr.materials[i];
            const MaterialAssetId newMatId =
                assets->createMaterialFromTexture(tex);
            const usize slotIndex = i;
            HistoryStack* h = ui ? ui->historyStack() : nullptr;
            if (h == nullptr) {
                // Sin HistoryStack no mutamos: una asignacion directa
                // se saltearia el undo/redo (deuda historica). No deberia
                // pasar en una sesion normal — el stack siempre existe.
                Log::editor()->warn(
                    "Drop de textura sobre material ignorado: sin HistoryStack");
            } else {
                auto cmd = std::make_unique<EditPropertyCommand<u32>>(
                    e, oldMatId, newMatId,
                    [slotIndex](Entity& en, const u32& v) {
                        auto& mrc = en.getComponent<MeshRendererComponent>();
                        if (slotIndex < mrc.materials.size()) {
                            mrc.materials[slotIndex] = v;
                        }
                    },
                    "Reemplazar textura material");
                h->push(std::move(cmd));  // execute() asigna newMatId
                editedFlag = true;
            }
        }
    }
    ImGui::EndDragDropTarget();
}

} // namespace

// MeshRendererComponent
// Con Hito 10, `mesh` es un MeshAssetId (no un IMesh* crudo) y `materials`
// es un vector de MaterialAssetId (1 por submesh). Muestra metadata del
// mesh resuelto y la lista de materiales.
void InspectorPanel::renderMeshRendererSection(Entity e) {
    auto& mr = e.getComponent<MeshRendererComponent>();
    if (!beginComponentSection<MeshRendererComponent>(e, ICON_FA_CUBE " MeshRenderer")) return;
    // break-A6: detectar si la malla cae al fallback PBR (skinned o
    // batcheable como instanced) — el shader graph queda silenciado en
    // esos casos y queremos avisar al dev.
    bool isSkinned = false;
    bool isInstanced = false;
    if (m_assets != nullptr) {
        ImGui::Text("%s",
            I18n::T("editor.panel.inspector.mesh.mesh_id_path",
                    m_assets->meshPathOf(mr.mesh), mr.mesh).c_str());
        MeshAsset* asset = m_assets->getMesh(mr.mesh);
        drawMeshTechDetails(asset);
        if (asset != nullptr) {
            isSkinned = asset->hasSkeleton();
            // Misma logica que RenderBatching: 1 submesh, <=1 material,
            // sin filtro de subMeshName => entra al instanced path con
            // el shader pbr_instanced.vert (que ignora el graph).
            isInstanced = (asset->submeshes.size() == 1u)
                          && (mr.materials.size() <= 1u)
                          && mr.subMeshName.empty();
        }
    } else {
        ImGui::Text("%s",
            I18n::T("editor.panel.inspector.mesh.mesh_id", mr.mesh).c_str());
    }
    ImGui::Text("%s",
        I18n::T("editor.panel.inspector.mesh.materials",
                static_cast<u32>(mr.materials.size())).c_str());

    // F3H9 Stage 9: UI Blender-style. Antes era un loop vertical que
    // dibujaba TODOS los slots uno debajo del otro — con N >= 3 el
    // Inspector se hacia kilometrico. Ahora:
    //   1) lista compacta arriba (ListBox de alto fijo) con un item
    //      por slot — el dev selecciona cual editar;
    //   2) abajo, SOLO el slot seleccionado se renderiza completo
    //      (drop target + PBR + Shader + Blending).
    //
    // Clampear el index sticky contra el size actual (la entidad pudo
    // cambiar, o el dev movio submeshes en el mesh asset).
    const int slotCount = static_cast<int>(mr.materials.size());
    if (m_selectedMaterialSlot >= slotCount) m_selectedMaterialSlot = 0;
    if (m_selectedMaterialSlot < 0)          m_selectedMaterialSlot = 0;

    if (slotCount == 0) {
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.inspector.mesh.no_materials").c_str());
        ImGui::Separator();
        return;
    }

    // (1) Lista compacta. Alto: max 4 lineas visibles (igual que Blender
    // — cualquier cantidad mayor scrollea). Single-line por slot:
    // "[i] <path>"
    {
        const float lineH = ImGui::GetTextLineHeightWithSpacing();
        const int   visibleRows = (slotCount < 4) ? slotCount : 4;
        const float listH = lineH * static_cast<float>(visibleRows)
                          + ImGui::GetStyle().FramePadding.y * 2.0f;
        if (ImGui::BeginListBox("##mat_slot_list",
                                 ImVec2(-FLT_MIN, listH))) {
            for (int i = 0; i < slotCount; ++i) {
                const MaterialAssetId matId = mr.materials[i];
                const std::string matPath = m_assets
                    ? m_assets->materialPathOf(matId)
                    : std::string{};
                const bool selected = (i == m_selectedMaterialSlot);
                ImGui::PushID(i);
                char label[256];
                std::snprintf(label, sizeof(label), "%d  %s", i,
                              matPath.empty() ? "(no material)" : matPath.c_str());
                if (ImGui::Selectable(label, selected)) {
                    m_selectedMaterialSlot = i;
                }
                ImGui::PopID();
            }
            ImGui::EndListBox();
        }
    }

    // (2) Panel del slot seleccionado.
    {
        const usize i = static_cast<usize>(m_selectedMaterialSlot);
        const MaterialAssetId matId = mr.materials[i];
        const std::string matPath = m_assets
            ? m_assets->materialPathOf(matId)
            : std::string{};
        ImGui::PushID(static_cast<int>(i));
        ImGui::SeparatorText(
            (I18n::T("editor.panel.inspector.mesh.material_slot") + " " +
             std::to_string(i)).c_str());
        ImGui::TextDisabled("%s (id %u)", matPath.c_str(),
                              static_cast<unsigned>(matId));

        MaterialAsset* mat = m_assets ? m_assets->getMaterial(matId) : nullptr;
        if (mat != nullptr) {
            InspectorMaterials::drawPbrMultipliers(
                mat, m_assets, matId,
                m_editTracker, m_ui, e, m_editedThisFrame);
            InspectorMaterials::drawShaderGraph(
                mat, m_assets, matId, m_ui,
                isSkinned, isInstanced, m_editedThisFrame);
            InspectorMaterials::drawBlending(
                mat, m_assets, matId,
                m_editTracker, m_ui, e, m_editedThisFrame);
        }
        drawMaterialDropTarget(e, mr, i, m_assets, m_ui, m_editedThisFrame);
        ImGui::PopID();
    }
    ImGui::Separator();
}

} // namespace Mood
