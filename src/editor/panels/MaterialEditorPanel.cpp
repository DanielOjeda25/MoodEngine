#include "editor/panels/MaterialEditorPanel.h"

#include "engine/assets/AssetManager.h"
#include "engine/render/resources/MaterialAsset.h"

#include <imgui.h>

#include <string>
#include <vector>

namespace Mood {

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
    // Construimos labels al vuelo. La lista cambia rara vez (solo al
    // cargar un mapa o al spawnear una entidad nueva), no vale la pena
    // cachear.
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

    // --- Sliders escalares ---
    // Mismo set que el Inspector. Sin pushEditIfDone (sin undo en este
    // panel — Fase 2). El dev espera ver el cambio en vivo.
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
    // Reusa el payload `MOOD_TEXTURE_ASSET` del AssetBrowser. Cada slot
    // es un boton "Drop aqui" que muestra el path actual.
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
                }
            }
            ImGui::EndDragDropTarget();
        }
        // Boton "limpiar" a la derecha en la misma linea.
        ImGui::SameLine();
        std::string clearId = std::string("X##clear_") + label;
        if (ImGui::SmallButton(clearId.c_str())) {
            slotRef = 0;
            m_editedThisFrame = true;
        }
    };

    ImGui::TextUnformatted("Texture slots (drop desde Asset Browser)");
    textureSlot("albedo",             mat->albedo);
    textureSlot("metallic_roughness", mat->metallicRoughness);
    textureSlot("normal",             mat->normal);
    textureSlot("ao",                 mat->ao);

    ImGui::Separator();
    ImGui::TextDisabled(
        "Para preview: asigna este material a una entidad del viewport.\n"
        "Preview esferico dedicado: pendiente Fase 2.");

    ImGui::End();
}

} // namespace Mood
