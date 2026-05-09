// F2H24: Inspector — MeshRendererComponent (mesh + materiales + drop).

#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"

#include "editor/ui/EditorUI.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/render/resources/MaterialAsset.h"
#include "engine/render/resources/MeshAsset.h"
#include "engine/scene/components/Components.h"

#include <imgui.h>

#include <string>

namespace Mood {

// MeshRendererComponent
// Con Hito 10, `mesh` es un MeshAssetId (no un IMesh* crudo) y `materials`
// es un vector de MaterialAssetId (1 por submesh). Muestra metadata del
// mesh resuelto y la lista de materiales.
void InspectorPanel::renderMeshRendererSection(Entity e) {
    auto& mr = e.getComponent<MeshRendererComponent>();
    ImGui::SeparatorText(ICON_FA_CUBE " MeshRenderer");
    if (m_assets != nullptr) {
        ImGui::Text("mesh: %s (id %u)",
                     m_assets->meshPathOf(mr.mesh).c_str(), mr.mesh);
        MeshAsset* asset = m_assets->getMesh(mr.mesh);
        if (asset != nullptr) {
            ImGui::Text("submeshes: %u, vertices: %u",
                         static_cast<u32>(asset->submeshes.size()),
                         asset->totalVertexCount());

            // F2H6: info de LODs (read-only en v1). Editar manualmente
            // o regenerar = hito futuro.
            const u32 lod0Tris = asset->totalVertexCount() / 3;
            u32 lod1Tris = 0, lod2Tris = 0;
            for (const auto& s : asset->lod1Submeshes) lod1Tris += s.vertexCount / 3;
            for (const auto& s : asset->lod2Submeshes) lod2Tris += s.vertexCount / 3;
            if (lod1Tris > 0 || lod2Tris > 0) {
                ImGui::TextDisabled("LOD 0: %u tris (full)", lod0Tris);
                ImGui::TextDisabled("LOD 1: %u tris", lod1Tris);
                ImGui::TextDisabled("LOD 2: %u tris", lod2Tris);
                ImGui::TextDisabled("LOD distances: %.0fm / %.0fm",
                                      static_cast<double>(asset->lodDistances.x),
                                      static_cast<double>(asset->lodDistances.y));
            } else if (asset->hasSkeleton()) {
                ImGui::TextDisabled("LODs: skinned (no generado)");
            } else {
                ImGui::TextDisabled("LODs: no aplicable (mesh chico)");
            }
        }
    } else {
        ImGui::Text("mesh id: %u", mr.mesh);
    }
    ImGui::Text("materials: %u", static_cast<u32>(mr.materials.size()));
    for (usize i = 0; i < mr.materials.size(); ++i) {
        const MaterialAssetId matId = mr.materials[i];
        const std::string matPath = m_assets->materialPathOf(matId);
        ImGui::PushID(static_cast<int>(i));
        // Header del slot: path del material (read-only).
        ImGui::SeparatorText(("Material slot " + std::to_string(i)).c_str());
        ImGui::TextDisabled("%s (id %u)", matPath.c_str(),
                              static_cast<unsigned>(matId));

        MaterialAsset* mat = m_assets->getMaterial(matId);
        if (mat != nullptr) {
            // Sliders de los multiplicadores escalares. Editar muta el
            // MaterialAsset in-place: el render del frame siguiente lo
            // recoge automaticamente (no hay copia por entidad).
            // Hito 32 D: cada slider/color empuja un EditPropertyCommand
            // al soltar el drag. Setters capturan matId + AssetManager
            // porque el material vive en AssetManager, no en la entidad.
            AssetManager* assetsCap = m_assets;
            const MaterialAssetId matIdCap = matId;
            if (ImGui::ColorEdit3("albedoTint",
                    &mat->albedoTint.x, ImGuiColorEditFlags_NoInputs)) {
                m_editedThisFrame = true;
            }
            detail::pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, mat->albedoTint,
                [assetsCap, matIdCap](Entity&, const glm::vec3& v) {
                    if (auto* m = assetsCap->getMaterial(matIdCap)) m->albedoTint = v;
                },
                "Editar albedoTint");

            if (ImGui::SliderFloat("metallic",
                    &mat->metallicMult, 0.0f, 1.0f, "%.2f")) {
                m_editedThisFrame = true;
            }
            detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, mat->metallicMult,
                [assetsCap, matIdCap](Entity&, const f32& v) {
                    if (auto* m = assetsCap->getMaterial(matIdCap)) m->metallicMult = v;
                },
                "Editar metallic");

            if (ImGui::SliderFloat("roughness",
                    &mat->roughnessMult, 0.04f, 1.0f, "%.2f")) {
                m_editedThisFrame = true;
            }
            detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, mat->roughnessMult,
                [assetsCap, matIdCap](Entity&, const f32& v) {
                    if (auto* m = assetsCap->getMaterial(matIdCap)) m->roughnessMult = v;
                },
                "Editar roughness");

            if (ImGui::SliderFloat("ao",
                    &mat->aoMult, 0.0f, 1.0f, "%.2f")) {
                m_editedThisFrame = true;
            }
            detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, mat->aoMult,
                [assetsCap, matIdCap](Entity&, const f32& v) {
                    if (auto* m = assetsCap->getMaterial(matIdCap)) m->aoMult = v;
                },
                "Editar ao");
            // Estado de las texturas — read-only por ahora salvo el
            // drop de albedo (Hito 35 A).
            ImGui::TextDisabled(
                "albedo: %u  MR: %u  normal: %u  ao: %u",
                mat->albedo, mat->metallicRoughness,
                mat->normal,  mat->ao);
        }

        // Hito 35 A: drop de textura del AssetBrowser sobre este slot
        // -> reemplaza el material entero por uno nuevo (instance
        // unico) con la textura como albedo. Anti-contagio: nunca
        // muta el material previo (otras entidades pueden compartirlo).
        // Hito 36 A: ahora undoable — push de EditPropertyCommand<u32>
        // (MaterialAssetId == u32) con setter que indexa el slot via
        // captura por valor. Si no hay history disponible, fallback
        // a asignacion directa.
        // F2H23: highlight visual del drop target cuando el dev
        // tiene un drag activo de textura — color verde claro para
        // que el ojo encuentre rapido los slots aceptables.
        const bool dragTex = detail::isDragActiveOfType("MOOD_TEXTURE_ASSET");
        if (dragTex) {
            ImGui::PushStyleColor(ImGuiCol_Button,
                                    ImVec4(0.20f, 0.55f, 0.25f, 1.0f));
        }
        ImGui::Button("Drop textura para reemplazar material",
                        ImVec2(-FLT_MIN, 0));
        if (dragTex) ImGui::PopStyleColor();
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* p =
                    ImGui::AcceptDragDropPayload("MOOD_TEXTURE_ASSET")) {
                if (m_assets != nullptr && p->DataSize == sizeof(TextureAssetId)) {
                    const TextureAssetId tex =
                        *static_cast<const TextureAssetId*>(p->Data);
                    const MaterialAssetId oldMatId = mr.materials[i];
                    const MaterialAssetId newMatId =
                        m_assets->createMaterialFromTexture(tex);
                    const usize slotIndex = i;
                    HistoryStack* h = m_ui ? m_ui->historyStack() : nullptr;
                    if (h != nullptr) {
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
                    } else {
                        mr.materials[i] = newMatId;
                    }
                    m_editedThisFrame = true;
                }
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::PopID();
    }
    ImGui::Separator();
}

} // namespace Mood
