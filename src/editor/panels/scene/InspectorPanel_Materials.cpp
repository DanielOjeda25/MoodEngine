// F3H29: implementación de los helpers compartidos de materiales. Movidos
// desde `InspectorPanel_MeshRenderer.cpp` (donde vivían inline en el
// namespace anónimo) — sin cambios de comportamiento. Ahora `Brush` los
// reusa para mostrar la misma UI Blender-style por slot de material.

#include "editor/panels/scene/InspectorPanel_Materials.h"

#include "core/Log.h"
#include "core/i18n/I18n.h"
#include "editor/commands/EditPropertyCommand.h"
#include "editor/commands/HistoryStack.h"
#include "editor/panels/assets/ShaderGraphEditorPanel.h"
#include "editor/panels/scene/InspectorEditTracker.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"  // detail::pushEditIfDone
#include "editor/ui/EditorUI.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/render/resources/MaterialAsset.h"
#include "engine/scene/core/Entity.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

namespace Mood {
namespace InspectorMaterials {

void drawPbrMultipliers(MaterialAsset* mat,
                          AssetManager* assetsCap,
                          MaterialAssetId matIdCap,
                          InspectorEditTracker& tracker,
                          EditorUI* ui, Entity e,
                          bool& editedFlag) {
    if (mat == nullptr) return;
    // F3H29 polish: Blender-style — los PBR multipliers viven dentro de
    // un CollapsingHeader "Surface" plegado por default. Antes ocupaban
    // 5 líneas fijas al elegir un slot; ahora si el dev no las quiere
    // tocar ni siquiera las ve.
    if (!ImGui::CollapsingHeader(
            I18n::T("editor.panel.inspector.materials.surface").c_str())) {
        return;
    }
    // F3H29 polish: reset button (↺) per-field, visible solo cuando
    // current != default. Mismo helper que ProjectSettings/Inspector
    // usa en otros lados. Defaults canónicos: albedoTint blanco,
    // metallic 0, roughness 1, ao 1 (MaterialAsset constructor).
    auto setAlbedoTint = [assetsCap, matIdCap](Entity&, const glm::vec3& v) {
        if (auto* m = assetsCap->getMaterial(matIdCap)) m->albedoTint = v;
    };
    auto setMetallic = [assetsCap, matIdCap](Entity&, const f32& v) {
        if (auto* m = assetsCap->getMaterial(matIdCap)) m->metallicMult = v;
    };
    auto setRoughness = [assetsCap, matIdCap](Entity&, const f32& v) {
        if (auto* m = assetsCap->getMaterial(matIdCap)) m->roughnessMult = v;
    };
    auto setAo = [assetsCap, matIdCap](Entity&, const f32& v) {
        if (auto* m = assetsCap->getMaterial(matIdCap)) m->aoMult = v;
    };

    if (ImGui::ColorEdit3("albedoTint",
            &mat->albedoTint.x, ImGuiColorEditFlags_NoInputs)) {
        editedFlag = true;
    }
    detail::pushEditIfDone<glm::vec3>(tracker, ui, e, mat->albedoTint,
        setAlbedoTint, "Editar albedoTint");
    if (detail::inspectorResetButton<glm::vec3>(ui, e, "albedoTint",
            mat->albedoTint, glm::vec3(1.0f), setAlbedoTint,
            "Reset albedoTint")) {
        editedFlag = true;
    }

    if (ImGui::SliderFloat("metallic",
            &mat->metallicMult, 0.0f, 1.0f, "%.2f")) {
        editedFlag = true;
    }
    detail::pushEditIfDone<f32>(tracker, ui, e, mat->metallicMult,
        setMetallic, "Editar metallic");
    if (detail::inspectorResetButton<f32>(ui, e, "metallic",
            mat->metallicMult, 0.0f, setMetallic, "Reset metallic")) {
        editedFlag = true;
    }

    if (ImGui::SliderFloat("roughness",
            &mat->roughnessMult, 0.04f, 1.0f, "%.2f")) {
        editedFlag = true;
    }
    detail::pushEditIfDone<f32>(tracker, ui, e, mat->roughnessMult,
        setRoughness, "Editar roughness");
    if (detail::inspectorResetButton<f32>(ui, e, "roughness",
            mat->roughnessMult, 1.0f, setRoughness, "Reset roughness")) {
        editedFlag = true;
    }

    if (ImGui::SliderFloat("ao",
            &mat->aoMult, 0.0f, 1.0f, "%.2f")) {
        editedFlag = true;
    }
    detail::pushEditIfDone<f32>(tracker, ui, e, mat->aoMult,
        setAo, "Editar ao");
    if (detail::inspectorResetButton<f32>(ui, e, "ao",
            mat->aoMult, 1.0f, setAo, "Reset ao")) {
        editedFlag = true;
    }
    // F3H29 polish: textura status `albedo: 0 MR: 0 normal: 0 ao: 0`
    // eliminado del Surface — texto colgado innecesario. Si el dev
    // quiere ver IDs de texturas, el AssetBrowser ya las lista.
}

void drawShaderGraph(MaterialAsset* mat, AssetManager* assets,
                       MaterialAssetId matId,
                       EditorUI* ui, bool isSkinned, bool isInstanced,
                       bool& editedFlag) {
    if (mat == nullptr) return;
    if (!ImGui::CollapsingHeader("Shader")) return;

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
                    graphPaths.push_back(
                        "shaders/graphs/" +
                        en.path().filename().generic_string());
                }
                std::sort(graphPaths.begin(), graphPaths.end());
            }
        }
    }

    std::vector<const char*> items;
    items.reserve(graphPaths.size() + 1);
    items.push_back("(PBR estandar)");
    for (const auto& p : graphPaths) items.push_back(p.c_str());
    int curIdx = 0;
    if (!mat->shaderGraphPath.empty()) {
        for (usize idx = 0; idx < graphPaths.size(); ++idx) {
            if (graphPaths[idx] == mat->shaderGraphPath) {
                curIdx = static_cast<int>(idx + 1);
                break;
            }
        }
        if (curIdx == 0) {
            items.push_back(mat->shaderGraphPath.c_str());
            curIdx = static_cast<int>(items.size() - 1);
        }
    }
    if (ImGui::Combo("Shader graph", &curIdx,
                        items.data(),
                        static_cast<int>(items.size()))) {
        std::string newPath;
        if (curIdx == 0) {
            newPath.clear();
        } else if (curIdx <= static_cast<int>(graphPaths.size())) {
            newPath = graphPaths[curIdx - 1];
        } else {
            newPath = mat->shaderGraphPath;
        }
        const std::string oldPath = mat->shaderGraphPath;
        if (oldPath != newPath) {
            HistoryStack* h = ui ? ui->historyStack() : nullptr;
            if (h != nullptr) {
                auto cmd = std::make_unique<EditPropertyCommand<std::string>>(
                    Entity{}, oldPath, newPath,
                    [assets, matId](Entity&, const std::string& v) {
                        if (auto* m = assets->getMaterial(matId)) {
                            m->shaderGraphPath = v;
                        }
                    },
                    "Cambiar shader graph");
                h->push(std::move(cmd));
            } else {
                mat->shaderGraphPath = newPath;
            }
            editedFlag = true;
        }
    }

    if (!mat->shaderGraphPath.empty()) {
        if (ImGui::Button(I18n::T("editor.modal.common.edit").c_str())) {
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
    // F3H29 polish: hint "(graphs en assets/shaders/graphs/ aparecen aca)"
    // eliminado — texto colgado de F2H62 cuando el sistema era nuevo.
}

void drawBlending(MaterialAsset* mat, AssetManager* assetsCap,
                    MaterialAssetId matIdCap,
                    InspectorEditTracker& tracker,
                    EditorUI* ui, Entity e, bool& editedFlag) {
    if (mat == nullptr) return;
    if (!ImGui::CollapsingHeader(
            I18n::T("editor.panel.inspector.mesh.blending").c_str())) {
        return;
    }
    const char* modeKeys[3] = {
        "editor.panel.inspector.mesh.blend_opaque",
        "editor.panel.inspector.mesh.blend_translucent",
        "editor.panel.inspector.mesh.blend_additive",
    };
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
    // F3H29 polish: hint "Agua=1.33 Vidrio=1.50 Diamante=2.42"
    // eliminado — info teórica que el dev memoriza o googlea.

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

} // namespace InspectorMaterials
} // namespace Mood
