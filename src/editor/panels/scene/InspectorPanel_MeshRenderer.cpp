// F2H24: Inspector — MeshRendererComponent (mesh + materiales + drop).
//
// break-B6b (auditoria): split de la función gigante. El cuerpo era un loop
// por material slot con 4 sub-bloques inline (PBR multipliers / shader
// graph picker / blending / drop target). Cada uno pasa a un helper
// file-local. La función queda en ~25 LOC + loop que despacha. Cero cambio
// de comportamiento.

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

// Sliders de los multiplicadores PBR + textura status. Editar muta el
// MaterialAsset in-place: el render del frame siguiente lo recoge
// automaticamente. Hito 32 D: cada slider/color empuja un
// EditPropertyCommand al soltar el drag. Setters capturan matId +
// AssetManager porque el material vive en AssetManager, no en la entidad.
void drawMaterialPbrMultipliers(MaterialAsset* mat,
                                  AssetManager* assetsCap,
                                  MaterialAssetId matIdCap,
                                  InspectorEditTracker& tracker,
                                  EditorUI* ui, Entity e,
                                  bool& editedFlag) {
    if (ImGui::ColorEdit3("albedoTint",
            &mat->albedoTint.x, ImGuiColorEditFlags_NoInputs)) {
        editedFlag = true;
    }
    detail::pushEditIfDone<glm::vec3>(tracker, ui, e, mat->albedoTint,
        [assetsCap, matIdCap](Entity&, const glm::vec3& v) {
            if (auto* m = assetsCap->getMaterial(matIdCap)) m->albedoTint = v;
        },
        "Editar albedoTint");

    if (ImGui::SliderFloat("metallic",
            &mat->metallicMult, 0.0f, 1.0f, "%.2f")) {
        editedFlag = true;
    }
    detail::pushEditIfDone<f32>(tracker, ui, e, mat->metallicMult,
        [assetsCap, matIdCap](Entity&, const f32& v) {
            if (auto* m = assetsCap->getMaterial(matIdCap)) m->metallicMult = v;
        },
        "Editar metallic");

    if (ImGui::SliderFloat("roughness",
            &mat->roughnessMult, 0.04f, 1.0f, "%.2f")) {
        editedFlag = true;
    }
    detail::pushEditIfDone<f32>(tracker, ui, e, mat->roughnessMult,
        [assetsCap, matIdCap](Entity&, const f32& v) {
            if (auto* m = assetsCap->getMaterial(matIdCap)) m->roughnessMult = v;
        },
        "Editar roughness");

    if (ImGui::SliderFloat("ao",
            &mat->aoMult, 0.0f, 1.0f, "%.2f")) {
        editedFlag = true;
    }
    detail::pushEditIfDone<f32>(tracker, ui, e, mat->aoMult,
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
}

// F2H62 Bloque D + polish: shader (PBR estandar vs Shader Graph).
// UX estilo Blender Principled BSDF: dropdown listando los .moodshader
// del proyecto en vez de InputText manual.
//
// break-A6 (auditoria): si el material tiene un shaderGraphPath asignado
// pero el mesh es skinned o instanced (caminos de render fallback al PBR
// estandar — el cache solo conoce `pbr.vert`), mostramos un warning visible
// para que el dev no piense que el grafo esta activo cuando no lo esta.
void drawMaterialShaderGraph(MaterialAsset* mat, AssetManager* assets,
                              EditorUI* ui, bool isSkinned, bool isInstanced,
                              bool& editedFlag) {
    if (!ImGui::CollapsingHeader("Shader")) return;

    // Warning de mesh no soportado. Visible solo cuando hay un graph
    // asignado y el mesh cae al fallback PBR (skinned/instanced). Sin
    // path asignado no hay nada que avisar.
    if (!mat->shaderGraphPath.empty() && (isSkinned || isInstanced)) {
        const std::string kind = I18n::T(isSkinned
            ? "editor.panel.inspector.mesh.shader_graph_unsupported_skinned"
            : "editor.panel.inspector.mesh.shader_graph_unsupported_instanced");
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.20f, 1.0f));
        ImGui::TextWrapped("%s",
            I18n::T("editor.panel.inspector.mesh.shader_graph_unsupported",
                    kind).c_str());
        ImGui::PopStyleColor();
    }

    // 1) Escanear `assets/shaders/graphs/` para los .moodshader
    // disponibles. Resolvemos la primera vez por frame; baratisimo
    // (decenas de archivos como mucho).
    std::vector<std::string> graphPaths;
    if (assets != nullptr) {
        const auto dir = assets->resolvePath("shaders/graphs");
        if (!dir.empty()) {
            std::error_code ec;
            if (std::filesystem::exists(dir, ec) &&
                std::filesystem::is_directory(dir, ec)) {
                for (const auto& en : std::filesystem::directory_iterator(dir, ec)) {
                    if (!en.is_regular_file() ||
                        en.path().extension() != ".moodshader") continue;
                    // Path logico relativo al root del proyecto.
                    graphPaths.push_back(
                        "shaders/graphs/" +
                        en.path().filename().generic_string());
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
        // Path seteado que NO esta en la lista (huerfano: archivo borrado,
        // path mal escrito). Insertamos un item especial al final para
        // mostrarlo + permitir quitarlo via el combo.
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
        // (caso huerfano: el dev re-seleccionando el huerfano no cambia
        // nada; no hace falta accion.)
        editedFlag = true;
    }

    // 3) Botones de accion segun el estado actual.
    if (!mat->shaderGraphPath.empty()) {
        if (ImGui::Button("Editar")) {
            if (ui != nullptr && assets != nullptr) {
                auto& panel = ui->shaderGraphEditor();
                const auto fs = assets->resolvePath(mat->shaderGraphPath);
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
        if (ui != nullptr) {
            auto& panel = ui->shaderGraphEditor();
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

// F2H63: Blending (Opaque / Translucent / Additive). Dropdown +
// sliders opacity / IOR / refractionStrength. Los sliders se desactivan
// cuando no aplican (e.g. IOR no afecta a Additive porque no hace
// refraccion). F2H64: checkbox "Proyectar sombra tintada".
void drawMaterialBlending(MaterialAsset* mat, AssetManager* assetsCap,
                           MaterialAssetId matIdCap,
                           InspectorEditTracker& tracker,
                           EditorUI* ui, Entity e, bool& editedFlag) {
    if (!ImGui::CollapsingHeader(
            I18n::T("editor.panel.inspector.mesh.blending").c_str())) {
        return;
    }
    const char* modeKeys[3] = {
        "editor.panel.inspector.mesh.blend_opaque",
        "editor.panel.inspector.mesh.blend_translucent",
        "editor.panel.inspector.mesh.blend_additive",
    };
    // I18n::T devuelve std::string -- guardamos las 3 strings localizadas
    // en variables locales para que los punteros sobrevivan hasta la
    // llamada al Combo.
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
        editedFlag = true;
    }
    // Undo del Combo: trackeamos cambio del valor (cast a u32 — el
    // variant de InspectorEditTracker tiene u32 pero no int). La
    // conversion BlendMode->u32 va y vuelve sin perdida (enum class : u8).
    {
        u32 blendModeU32 = static_cast<u32>(mat->blendMode);
        detail::pushEditIfDone<u32>(
            tracker, ui, e, blendModeU32,
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
        editedFlag = true;
    }
    detail::pushEditIfDone<f32>(tracker, ui, e, mat->opacity,
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
        editedFlag = true;
    }
    detail::pushEditIfDone<f32>(tracker, ui, e, mat->ior,
        [assetsCap, matIdCap](Entity&, const f32& v) {
            if (auto* m = assetsCap->getMaterial(matIdCap)) m->ior = v;
        },
        "Editar IOR");
    ImGui::TextDisabled("%s",
        I18n::T("editor.panel.inspector.mesh.ior_presets").c_str());

    if (ImGui::SliderFloat(
            I18n::T("editor.panel.inspector.mesh.refraction_strength").c_str(),
            &mat->refractionStrength, 0.0f, 1.0f, "%.2f")) {
        editedFlag = true;
    }
    detail::pushEditIfDone<f32>(tracker, ui, e, mat->refractionStrength,
        [assetsCap, matIdCap](Entity&, const f32& v) {
            if (auto* m = assetsCap->getMaterial(matIdCap)) m->refractionStrength = v;
        },
        "Editar refraction strength");
    ImGui::EndDisabled();

    // F2H64: checkbox "Proyectar sombra tintada". Solo aplica a
    // Translucent (Additive no tiene sentido como shadow caster).
    ImGui::BeginDisabled(mat->blendMode != BlendMode::Translucent);
    if (ImGui::Checkbox(
            I18n::T("editor.panel.inspector.mesh.cast_translucent_shadow").c_str(),
            &mat->castTranslucentShadow)) {
        editedFlag = true;
    }
    detail::pushEditIfDone<u32>(tracker, ui, e,
        static_cast<u32>(mat->castTranslucentShadow ? 1u : 0u),
        [assetsCap, matIdCap](Entity&, const u32& v) {
            if (auto* m = assetsCap->getMaterial(matIdCap))
                m->castTranslucentShadow = (v != 0u);
        },
        "Editar cast translucent shadow");
    ImGui::EndDisabled();
}

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
            drawMaterialPbrMultipliers(mat, m_assets, matId,
                                          m_editTracker, m_ui, e, m_editedThisFrame);
            drawMaterialShaderGraph(mat, m_assets, m_ui, isSkinned, isInstanced,
                                       m_editedThisFrame);
            drawMaterialBlending(mat, m_assets, matId,
                                   m_editTracker, m_ui, e, m_editedThisFrame);
        }
        drawMaterialDropTarget(e, mr, i, m_assets, m_ui, m_editedThisFrame);
        ImGui::PopID();
    }
    ImGui::Separator();
}

} // namespace Mood
