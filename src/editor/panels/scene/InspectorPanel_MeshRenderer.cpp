// F2H24: Inspector — MeshRendererComponent (mesh + materiales + drop).

#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"

#include "core/Log.h"  // F2H62 polish: warn al evitar overwrite de dirty shader graph
#include "editor/panels/assets/ShaderGraphEditorPanel.h"  // F2H62 Bloque D
#include "editor/ui/EditorUI.h"
#include "engine/assets/manager/AssetManager.h"
#include "core/i18n/I18n.h"  // F2H43
#include "engine/render/resources/MaterialAsset.h"
#include "engine/render/resources/MeshAsset.h"
#include "engine/scene/components/Components.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace Mood {

// MeshRendererComponent
// Con Hito 10, `mesh` es un MeshAssetId (no un IMesh* crudo) y `materials`
// es un vector de MaterialAssetId (1 por submesh). Muestra metadata del
// mesh resuelto y la lista de materiales.
void InspectorPanel::renderMeshRendererSection(Entity e) {
    auto& mr = e.getComponent<MeshRendererComponent>();
    ImGui::SeparatorText(ICON_FA_CUBE " MeshRenderer");
    if (m_assets != nullptr) {
        ImGui::Text("%s",
            I18n::T("editor.panel.inspector.mesh.mesh_id_path",
                    m_assets->meshPathOf(mr.mesh), mr.mesh).c_str());
        MeshAsset* asset = m_assets->getMesh(mr.mesh);
        if (asset != nullptr) {
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
    } else {
        ImGui::Text("%s",
            I18n::T("editor.panel.inspector.mesh.mesh_id", mr.mesh).c_str());
    }
    ImGui::Text("%s",
        I18n::T("editor.panel.inspector.mesh.materials",
                static_cast<u32>(mr.materials.size())).c_str());
    for (usize i = 0; i < mr.materials.size(); ++i) {
        const MaterialAssetId matId = mr.materials[i];
        const std::string matPath = m_assets->materialPathOf(matId);
        ImGui::PushID(static_cast<int>(i));
        // Header del slot: path del material (read-only).
        ImGui::SeparatorText(
            (I18n::T("editor.panel.inspector.mesh.material_slot") + " " +
             std::to_string(i)).c_str());
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
            ImGui::TextDisabled("%s",
                I18n::T("editor.panel.inspector.mesh.tex_status",
                        mat->albedo, mat->metallicRoughness,
                        mat->normal,  mat->ao).c_str());

            // F2H62 Bloque D + polish: shader (PBR estandar vs Shader Graph).
            // UX estilo Blender Principled BSDF: dropdown listando los
            // .moodshader del proyecto en vez de InputText manual.
            if (ImGui::CollapsingHeader("Shader")) {
                // 1) Escanear `assets/shaders/graphs/` para los .moodshader
                // disponibles. Resolvemos la primera vez por frame; baratisimo
                // (decenas de archivos como mucho).
                std::vector<std::string> graphPaths;
                if (m_assets != nullptr) {
                    const auto dir = m_assets->resolvePath("shaders/graphs");
                    if (!dir.empty()) {
                        std::error_code ec;
                        if (std::filesystem::exists(dir, ec) &&
                            std::filesystem::is_directory(dir, ec)) {
                            for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
                                if (!e.is_regular_file() ||
                                    e.path().extension() != ".moodshader") continue;
                                // Path logico relativo al root del proyecto.
                                graphPaths.push_back(
                                    "shaders/graphs/" +
                                    e.path().filename().generic_string());
                            }
                            std::sort(graphPaths.begin(), graphPaths.end());
                        }
                    }
                }

                // 2) Combo "Shader graph": "(PBR estandar)" + lista + indice
                //    actual segun mat->shaderGraphPath.
                std::vector<const char*> items;
                items.reserve(graphPaths.size() + 1);
                items.push_back("(PBR estandar)");
                for (const auto& p : graphPaths) items.push_back(p.c_str());
                int curIdx = 0;  // 0 = PBR estandar
                if (!mat->shaderGraphPath.empty()) {
                    for (usize idx = 0; idx < graphPaths.size(); ++idx) {
                        if (graphPaths[idx] == mat->shaderGraphPath) {
                            curIdx = static_cast<int>(idx + 1);
                            break;
                        }
                    }
                    // Path seteado que NO esta en la lista (huerfano:
                    // archivo borrado, path mal escrito). Insertamos un
                    // item especial al final para mostrarlo + permitir
                    // quitarlo via el combo.
                    if (curIdx == 0) {
                        items.push_back(mat->shaderGraphPath.c_str());
                        curIdx = static_cast<int>(items.size() - 1);
                    }
                }
                if (ImGui::Combo("Shader graph", &curIdx,
                                    items.data(),
                                    static_cast<int>(items.size()))) {
                    if (curIdx == 0) {
                        mat->shaderGraphPath.clear();
                    } else if (curIdx <= static_cast<int>(graphPaths.size())) {
                        mat->shaderGraphPath = graphPaths[curIdx - 1];
                    }
                    // (caso huerfano: el dev re-seleccionando el huerfano
                    // no cambia nada; no hace falta accion.)
                    m_editedThisFrame = true;
                }

                // 3) Botones de accion segun el estado actual.
                if (!mat->shaderGraphPath.empty()) {
                    if (ImGui::Button("Editar")) {
                        if (m_ui != nullptr && m_assets != nullptr) {
                            auto& panel = m_ui->shaderGraphEditor();
                            const auto fs = m_assets->resolvePath(mat->shaderGraphPath);
                            Log::editor()->info(
                                "[Inspector] Editar shader graph: path='{}' fs='{}' "
                                "panelHasAsset={} panelFilePath='{}' panelDirty={}",
                                mat->shaderGraphPath, fs.generic_string(),
                                panel.hasAsset(),
                                panel.filePath().has_value()
                                    ? panel.filePath()->generic_string()
                                    : std::string("<vacio>"),
                                panel.dirty());

                            // Garantia: el panel queda visible siempre que el
                            // dev haga click en Editar. Cualquier rama hace
                            // visible=true para que el dev SIEMPRE vea algo
                            // en respuesta al click.
                            panel.visible = true;

                            const bool samePathOpen =
                                panel.filePath().has_value() &&
                                (panel.filePath()->generic_string() ==
                                  fs.generic_string());
                            if (samePathOpen) {
                                // Mismo asset abierto, nada que cargar.
                            } else if (panel.hasAsset() && panel.dirty()) {
                                Log::editor()->warn(
                                    "[Inspector] ShaderGraphEditor con cambios "
                                    "sin guardar -- no se reemplaza. Guarda "
                                    "antes para abrir '{}'.",
                                    mat->shaderGraphPath);
                            } else if (!fs.empty() && std::filesystem::exists(fs)) {
                                panel.openFromFile(fs);
                            } else {
                                // El path en el material no existe en disco
                                // (archivo borrado, path mal escrito). Avisamos
                                // pero dejamos el panel visible.
                                Log::editor()->warn(
                                    "[Inspector] '{}' no existe en disco "
                                    "(fs='{}'); abrir un graph nuevo o corregir "
                                    "el path en el combo de arriba.",
                                    mat->shaderGraphPath, fs.generic_string());
                            }
                        }
                    }
                    ImGui::SameLine();
                }
                if (ImGui::Button("+ Nuevo shader graph")) {
                    // Abrir el panel con un asset nuevo + pedir Save As
                    // automaticamente. Cuando el dev guarda, el path se
                    // mete en el material via el combo de arriba (al
                    // siguiente frame el escaneo del dir lo detecta).
                    if (m_ui != nullptr) {
                        auto& panel = m_ui->shaderGraphEditor();
                        if (panel.hasAsset() && panel.dirty()) {
                            Log::editor()->warn(
                                "[Inspector] ShaderGraphEditor con cambios sin "
                                "guardar -- guarda antes de crear uno nuevo.");
                            panel.visible = true;
                        } else {
                            panel.newAsset();
                            panel.visible = true;
                        }
                    }
                }
                ImGui::TextDisabled(
                    "(graphs en assets/shaders/graphs/ aparecen aca)");
            }

            // F2H63: Blending (Opaque / Translucent / Additive).
            // Dropdown + sliders opacity / IOR / refractionStrength. Los
            // sliders se desactivan cuando no aplican (e.g. IOR no afecta
            // a Additive porque no hace refraccion).
            if (ImGui::CollapsingHeader(
                    I18n::T("editor.panel.inspector.mesh.blending").c_str())) {
                const char* modeKeys[3] = {
                    "editor.panel.inspector.mesh.blend_opaque",
                    "editor.panel.inspector.mesh.blend_translucent",
                    "editor.panel.inspector.mesh.blend_additive",
                };
                // I18n::T devuelve std::string -- guardamos las 3 strings
                // localizadas en variables locales para que los punteros
                // sobrevivan hasta la llamada al Combo.
                const std::string mOpaque      = I18n::T(modeKeys[0]);
                const std::string mTranslucent = I18n::T(modeKeys[1]);
                const std::string mAdditive    = I18n::T(modeKeys[2]);
                const char* modeLabels[3] = {
                    mOpaque.c_str(), mTranslucent.c_str(), mAdditive.c_str(),
                };
                int curBlend = static_cast<int>(mat->blendMode);
                if (ImGui::Combo(
                        I18n::T("editor.panel.inspector.mesh.blend_mode").c_str(),
                        &curBlend, modeLabels, 3)) {
                    mat->blendMode = static_cast<BlendMode>(curBlend);
                    m_editedThisFrame = true;
                }
                // Undo del Combo: trackeamos cambio del valor (cast a u32 --
                // el variant de InspectorEditTracker tiene u32 pero no int).
                // La conversion BlendMode->u32 va y vuelve sin perdida
                // (enum class : u8).
                {
                    u32 blendModeU32 = static_cast<u32>(mat->blendMode);
                    detail::pushEditIfDone<u32>(
                        m_editTracker, m_ui, e, blendModeU32,
                        [assetsCap, matIdCap](Entity&, const u32& v) {
                            if (auto* m = assetsCap->getMaterial(matIdCap))
                                m->blendMode = static_cast<BlendMode>(v);
                        },
                        "Editar blend mode");
                }

                const bool isOpaque  = (mat->blendMode == BlendMode::Opaque);
                const bool isAdditive = (mat->blendMode == BlendMode::Additive);

                ImGui::BeginDisabled(isOpaque);
                if (ImGui::SliderFloat(
                        I18n::T("editor.panel.inspector.mesh.opacity").c_str(),
                        &mat->opacity, 0.0f, 1.0f, "%.2f")) {
                    m_editedThisFrame = true;
                }
                detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, mat->opacity,
                    [assetsCap, matIdCap](Entity&, const f32& v) {
                        if (auto* m = assetsCap->getMaterial(matIdCap)) m->opacity = v;
                    },
                    "Editar opacity");
                ImGui::EndDisabled();

                // IOR + refractionStrength: solo aplican a Translucent.
                // Additive no hace refraccion (es emisivo puro).
                ImGui::BeginDisabled(isOpaque || isAdditive);
                if (ImGui::SliderFloat(
                        I18n::T("editor.panel.inspector.mesh.ior").c_str(),
                        &mat->ior, 1.0f, 2.5f, "%.2f")) {
                    m_editedThisFrame = true;
                }
                detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, mat->ior,
                    [assetsCap, matIdCap](Entity&, const f32& v) {
                        if (auto* m = assetsCap->getMaterial(matIdCap)) m->ior = v;
                    },
                    "Editar IOR");
                ImGui::TextDisabled("%s",
                    I18n::T("editor.panel.inspector.mesh.ior_presets").c_str());

                if (ImGui::SliderFloat(
                        I18n::T("editor.panel.inspector.mesh.refraction_strength").c_str(),
                        &mat->refractionStrength, 0.0f, 1.0f, "%.2f")) {
                    m_editedThisFrame = true;
                }
                detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, mat->refractionStrength,
                    [assetsCap, matIdCap](Entity&, const f32& v) {
                        if (auto* m = assetsCap->getMaterial(matIdCap)) m->refractionStrength = v;
                    },
                    "Editar refraction strength");
                ImGui::EndDisabled();
            }
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
        ImGui::Button(I18n::T("editor.panel.inspector.mesh.drop_replace").c_str(),
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
