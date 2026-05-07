#include "editor/panels/assets/MaterialEditorPanel.h"

#include "engine/assets/manager/AssetManager.h"
#include "engine/render/preview/MaterialPreviewRenderer.h"
#include "engine/render/resources/MaterialAsset.h"

#include <imgui.h>

#include <string>
#include <vector>

namespace Mood {

namespace {

// Sentinels que NO se persisten via saveMaterial (mismos que asume
// AssetManager::saveMaterial). Si el path lógico cae en alguno, el
// botón Guardar queda deshabilitado.
bool isPersistablePath(const std::string& p) {
    if (p.empty()) return false;
    if (p == "__default_material") return false;
    if (p.rfind("__tex#", 0) == 0) return false;
    if (p.rfind("__runtime#", 0) == 0) return false;
    return true;
}

} // namespace

void MaterialEditorPanel::onImGuiRender() {
    if (!visible) return;

    if (!ImGui::Begin(name(), &visible)) {
        ImGui::End();
        return;
    }

    if (m_assets == nullptr) {
        ImGui::TextDisabled("AssetManager no inyectado");
        ImGui::End();
        return;
    }

    const usize matCount = m_assets->materialCount();
    if (matCount == 0) {
        ImGui::TextDisabled("No hay materiales registrados.");
        ImGui::End();
        return;
    }

    // --- Combo de materiales ---
    std::vector<std::string> labels;
    labels.reserve(matCount);
    for (MaterialAssetId i = 0; i < matCount; ++i) {
        labels.push_back(m_assets->materialPathOf(i));
        if (labels.back().empty()) {
            labels.back() = "<sin path>";
        }
    }
    if (m_selectedMatIdx < 0 || m_selectedMatIdx >= static_cast<int>(matCount)) {
        m_selectedMatIdx = 0;
    }

    if (ImGui::BeginCombo("Material",
                            labels[m_selectedMatIdx].c_str())) {
        for (int i = 0; i < static_cast<int>(matCount); ++i) {
            const bool selected = (m_selectedMatIdx == i);
            if (ImGui::Selectable(labels[i].c_str(), selected)) {
                m_selectedMatIdx = i;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    const MaterialAssetId matId = static_cast<MaterialAssetId>(m_selectedMatIdx);
    MaterialAsset* mat = m_assets->getMaterial(matId);
    if (mat == nullptr) {
        ImGui::TextDisabled("Material id %u sin instancia (raro).", matId);
        ImGui::End();
        return;
    }

    ImGui::Separator();

    // --- F2H21: layout 2 columnas (controles | preview) ---
    // Con preview renderer disponible y panel suficientemente ancho,
    // dividimos en 2 columnas. Si el panel es angosto o no hay preview,
    // caemos a una sola columna (legacy layout).
    const bool showPreview = (m_preview != nullptr) &&
                             (ImGui::GetContentRegionAvail().x >= 540.0f);

    if (showPreview) {
        ImGui::Columns(2, "##material_editor_cols", false);
        ImGui::SetColumnWidth(0, ImGui::GetWindowContentRegionMax().x - 280.0f);
    }

    // ===== Columna izquierda: controles =====

    // --- Sliders escalares ---
    if (ImGui::ColorEdit3("albedo tint", &mat->albedoTint.x,
                            ImGuiColorEditFlags_NoInputs)) {
        m_editedThisFrame = true;
    }
    if (ImGui::SliderFloat("metallic",  &mat->metallicMult,  0.0f, 1.0f, "%.2f")) {
        m_editedThisFrame = true;
    }
    if (ImGui::SliderFloat("roughness", &mat->roughnessMult, 0.04f, 1.0f, "%.2f")) {
        m_editedThisFrame = true;
    }
    if (ImGui::SliderFloat("ao",        &mat->aoMult,        0.0f, 1.0f, "%.2f")) {
        m_editedThisFrame = true;
    }

    ImGui::Separator();

    // --- Texture slots con drop targets ---
    auto textureSlot = [&](const char* label, TextureAssetId& slotRef) {
        const std::string current = m_assets->pathOf(slotRef);
        const std::string btnLabel = std::string(label) + ": " +
            (current.empty() ? "<vacio>" : current);
        ImGui::Button(btnLabel.c_str(), ImVec2(-FLT_MIN, 0));
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* p =
                    ImGui::AcceptDragDropPayload("MOOD_TEXTURE_ASSET")) {
                if (p->DataSize == sizeof(TextureAssetId)) {
                    slotRef = *static_cast<const TextureAssetId*>(p->Data);
                    m_editedThisFrame = true;
                    if (label == std::string("albedo")) {
                        // Drop sobre albedo activa el sample del shader
                        // automaticamente (consistente con el patron del
                        // Inspector y de loadMaterial).
                        mat->useAlbedoMap = true;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::SameLine();
        std::string clearId = std::string("X##clear_") + label;
        if (ImGui::SmallButton(clearId.c_str())) {
            slotRef = 0;
            if (label == std::string("albedo")) {
                mat->useAlbedoMap = false;
            }
            m_editedThisFrame = true;
        }
    };

    ImGui::TextUnformatted("Texture slots (drop desde Asset Browser)");
    textureSlot("albedo",             mat->albedo);
    textureSlot("metallic_roughness", mat->metallicRoughness);
    textureSlot("normal",             mat->normal);
    textureSlot("ao",                 mat->ao);

    ImGui::Separator();

    // --- F2H21: boton Guardar + feedback ---
    const std::string& matPath = labels[m_selectedMatIdx];
    const bool canSave = isPersistablePath(matPath);
    if (!canSave) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Guardar (.material)", ImVec2(-FLT_MIN, 0))) {
        const bool ok = m_assets->saveMaterial(matId);
        m_saveStatusOk = ok;
        m_saveStatusFrames = 120;  // ~2 segundos a 60 FPS
    }
    if (!canSave) {
        ImGui::EndDisabled();
        ImGui::TextDisabled("(material runtime / sentinel — no persistible)");
    }
    if (m_saveStatusFrames > 0) {
        const ImVec4 color = m_saveStatusOk
            ? ImVec4(0.4f, 0.95f, 0.4f, 1.0f)
            : ImVec4(0.95f, 0.4f, 0.4f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(m_saveStatusOk
            ? "Guardado OK"
            : "Guardar fallo (ver log)");
        ImGui::PopStyleColor();
        --m_saveStatusFrames;
    }

    // ===== Columna derecha: preview =====
    if (showPreview) {
        ImGui::NextColumn();

        ImGui::TextUnformatted("Preview");
        ImGui::Separator();

        m_preview->renderPreview(*mat, *m_assets);
        const GLuint texId = m_preview->outputTextureId();
        if (texId != 0) {
            // ImTextureID = ImU64 en esta version de ImGui — cast directo
            // de GLuint a ImU64 (no reinterpret entre integers).
            // ImVec2 uv0 = (0, 1) y uv1 = (1, 0) para flippear vertical
            // (ImGui asume top-left origin pero OpenGL FBO tiene
            // bottom-left origin).
            ImGui::Image(
                static_cast<ImTextureID>(texId),
                ImVec2(static_cast<f32>(m_preview->width()),
                       static_cast<f32>(m_preview->height())),
                ImVec2(0.0f, 1.0f),
                ImVec2(1.0f, 0.0f));
        } else {
            ImGui::TextDisabled("(preview no disponible)");
        }

        ImGui::Columns(1);
    }

    ImGui::End();
}

} // namespace Mood
