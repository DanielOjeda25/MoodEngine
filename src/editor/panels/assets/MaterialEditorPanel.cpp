#include "editor/panels/assets/MaterialEditorPanel.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/i18n/I18n.h"  // F2H43
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
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.material.no_manager").c_str());
        ImGui::End();
        return;
    }

    const usize matCount = m_assets->materialCount();
    if (matCount == 0) {
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.material.no_materials").c_str());
        ImGui::End();
        return;
    }

    // --- Combo de materiales ---
    std::vector<std::string> labels;
    labels.reserve(matCount);
    for (MaterialAssetId i = 0; i < matCount; ++i) {
        labels.push_back(m_assets->materialPathOf(i));
        if (labels.back().empty()) {
            labels.back() = I18n::T("editor.panel.material.no_path");
        }
    }
    // F2H21: la primera vez que el panel se abre (m_selectedMatIdx=-1),
    // arrancamos en el primer material NO sentinel (slot 0 es el default
    // magenta deliberado; mostrarlo by default desconcierta al dev).
    // Si todos son sentinels (raro), caemos al slot 0.
    if (m_selectedMatIdx < 0 || m_selectedMatIdx >= static_cast<int>(matCount)) {
        m_selectedMatIdx = 0;
        for (int i = 0; i < static_cast<int>(matCount); ++i) {
            if (isPersistablePath(labels[i])) {
                m_selectedMatIdx = i;
                break;
            }
        }
    }

    if (ImGui::BeginCombo(I18n::T("editor.panel.material.material").c_str(),
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

    // F2H21 tracking: log al cambiar el material seleccionado.
    if (m_selectedMatIdx != m_lastLoggedMatIdx) {
        Log::editor()->info(
            "[material-editor] seleccionado material '{}'",
            labels[m_selectedMatIdx]);
        m_lastLoggedMatIdx = m_selectedMatIdx;
    }

    const MaterialAssetId matId = static_cast<MaterialAssetId>(m_selectedMatIdx);
    MaterialAsset* mat = m_assets->getMaterial(matId);
    if (mat == nullptr) {
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.material.no_instance", matId).c_str());
        ImGui::End();
        return;
    }

    ImGui::Separator();

    // --- F2H21: layout adaptativo segun ancho del panel ---
    // Modos:
    //   - 2 columnas (>= 540px): controles izq | preview der.
    //   - Vertical (panel angosto): preview ARRIBA, controles abajo.
    //   - Sin preview (m_preview == nullptr): solo controles.
    enum class LayoutMode { TwoColumns, Vertical, ControlsOnly };
    const f32 availWidth = ImGui::GetContentRegionAvail().x;
    LayoutMode layout = LayoutMode::ControlsOnly;
    if (m_preview != nullptr) {
        layout = (availWidth >= 540.0f) ? LayoutMode::TwoColumns
                                         : LayoutMode::Vertical;
    }

    // Helper interno: dibuja el preview con la esfera. Compartido entre
    // los dos layouts que muestran preview.
    auto drawPreviewBlock = [&]() {
        m_preview->renderPreview(*mat, *m_assets);
        const GLuint texId = m_preview->outputTextureId();
        if (texId != 0) {
            // ImTextureID = ImU64 en esta version de ImGui — cast directo
            // de GLuint a ImU64 (no reinterpret entre integers).
            // ImVec2 uv0 = (0, 1) y uv1 = (1, 0) para flippear vertical
            // (ImGui asume top-left origin pero OpenGL FBO tiene
            // bottom-left origin).
            const f32 size = static_cast<f32>(m_preview->width());
            // Centrar horizontalmente en modo vertical.
            const f32 avail = ImGui::GetContentRegionAvail().x;
            if (avail > size) {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                                       (avail - size) * 0.5f);
            }
            ImGui::Image(
                static_cast<ImTextureID>(texId),
                ImVec2(size, size),
                ImVec2(0.0f, 1.0f),
                ImVec2(1.0f, 0.0f));
        } else {
            ImGui::TextDisabled("%s",
                I18n::T("editor.panel.material.preview_unavailable").c_str());
        }
    };

    // En modo Vertical, el preview va PRIMERO (arriba).
    if (layout == LayoutMode::Vertical) {
        ImGui::TextUnformatted(I18n::T("editor.panel.material.preview").c_str());
        drawPreviewBlock();
        ImGui::Separator();
    }

    if (layout == LayoutMode::TwoColumns) {
        ImGui::Columns(2, "##material_editor_cols", false);
        ImGui::SetColumnWidth(0, ImGui::GetWindowContentRegionMax().x - 280.0f);
    }

    // ===== Controles (siempre se dibujan; en TwoColumns van en col izq) =====

    // --- Sliders escalares ---
    // Patron F2H21 tracking: capturamos el valor PRE al `IsItemActivated`
    // (primer frame del drag) y logueamos el delta al `IsItemDeactivatedAfterEdit`
    // (cuando el dev suelta). Sin esto el log spamearia con cada frame.
    if (ImGui::ColorEdit3("albedo tint", &mat->albedoTint.x,
                            ImGuiColorEditFlags_NoInputs)) {
        m_editedThisFrame = true;
    }
    if (ImGui::IsItemActivated()) m_tintPreDrag = mat->albedoTint;
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        Log::editor()->info(
            "[material-editor] albedo_tint: ({:.2f},{:.2f},{:.2f}) -> ({:.2f},{:.2f},{:.2f})",
            m_tintPreDrag.x, m_tintPreDrag.y, m_tintPreDrag.z,
            mat->albedoTint.x, mat->albedoTint.y, mat->albedoTint.z);
    }

    if (ImGui::SliderFloat("metallic",  &mat->metallicMult,  0.0f, 1.0f, "%.2f")) {
        m_editedThisFrame = true;
    }
    if (ImGui::IsItemActivated()) m_metallicPreDrag = mat->metallicMult;
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        Log::editor()->info(
            "[material-editor] metallic: {:.2f} -> {:.2f}",
            m_metallicPreDrag, mat->metallicMult);
    }

    if (ImGui::SliderFloat("roughness", &mat->roughnessMult, 0.04f, 1.0f, "%.2f")) {
        m_editedThisFrame = true;
    }
    if (ImGui::IsItemActivated()) m_roughnessPreDrag = mat->roughnessMult;
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        Log::editor()->info(
            "[material-editor] roughness: {:.2f} -> {:.2f}",
            m_roughnessPreDrag, mat->roughnessMult);
    }

    if (ImGui::SliderFloat("ao",        &mat->aoMult,        0.0f, 1.0f, "%.2f")) {
        m_editedThisFrame = true;
    }
    if (ImGui::IsItemActivated()) m_aoPreDrag = mat->aoMult;
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        Log::editor()->info(
            "[material-editor] ao: {:.2f} -> {:.2f}",
            m_aoPreDrag, mat->aoMult);
    }

    ImGui::Separator();

    // --- Texture slots con drop targets ---
    // Layout por slot: boton principal (drop target) que ocupa el ancho
    // del panel menos espacio para el "X" de clear a la derecha.
    auto textureSlot = [&](const char* label, TextureAssetId& slotRef) {
        // Slot vacio = id 0. `pathOf(0)` devuelve "textures/missing.png"
        // (el path del fallback) — NO un string vacio. Usamos el id
        // directo como criterio.
        const bool empty = (slotRef == 0);
        const std::string path = m_assets->pathOf(slotRef);
        const std::string btnLabel = empty
            ? std::string(label) + ": " + I18n::T("editor.panel.material.slot_empty")
            : std::string(label) + ": " + path;

        // Reservar 28 px a la derecha para el boton "X". -FLT_MIN tomaba
        // todo el ancho y el "X" se salia del panel — el dev no lo veia.
        const f32 clearBtnW = 28.0f;
        const f32 mainBtnW = ImGui::GetContentRegionAvail().x - clearBtnW
                              - ImGui::GetStyle().ItemSpacing.x;
        ImGui::Button(btnLabel.c_str(), ImVec2(mainBtnW, 0));
        if (ImGui::IsItemHovered()) {
            const std::string ttBody = empty
                ? I18n::T("editor.panel.material.slot_no_texture")
                : path;
            ImGui::SetTooltip("%s",
                I18n::T("editor.panel.material.slot_tooltip", ttBody).c_str());
        }
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
                    // F2H21 tracking.
                    Log::editor()->info(
                        "[material-editor] drop sobre slot '{}': '{}'",
                        label, m_assets->pathOf(slotRef));
                }
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::SameLine();
        std::string clearId = std::string("X##clear_") + label;
        // Boton "X" siempre visible. Deshabilitado cuando el slot ya esta
        // vacio (no tiene nada que limpiar).
        if (empty) ImGui::BeginDisabled();
        if (ImGui::Button(clearId.c_str(),
                            ImVec2(clearBtnW, 0))) {
            slotRef = 0;
            if (label == std::string("albedo")) {
                mat->useAlbedoMap = false;
            }
            m_editedThisFrame = true;
            // F2H21 tracking.
            Log::editor()->info(
                "[material-editor] slot '{}' limpiado (slot=0)", label);
        }
        if (empty) ImGui::EndDisabled();
        if (ImGui::IsItemHovered() && !empty) {
            ImGui::SetTooltip("%s",
                I18n::T("editor.panel.material.slot_remove_tooltip",
                        std::string(label)).c_str());
        }
    };

    ImGui::Spacing();
    ImGui::TextUnformatted(I18n::T("editor.panel.material.texture_slots").c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.panel.material.texture_slots_help").c_str());
    }

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
    if (ImGui::Button(I18n::T("editor.panel.material.save").c_str(), ImVec2(-FLT_MIN, 0))) {
        // F2H21 tracking: log de la accion + resultado.
        Log::editor()->info(
            "[material-editor] click Guardar: '{}'", matPath);
        const bool ok = m_assets->saveMaterial(matId);
        m_saveStatusOk = ok;
        m_saveStatusFrames = 120;  // ~2 segundos a 60 FPS
    }
    if (!canSave) {
        ImGui::EndDisabled();
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.material.runtime_sentinel").c_str());
    }
    if (m_saveStatusFrames > 0) {
        const ImVec4 color = m_saveStatusOk
            ? ImVec4(0.4f, 0.95f, 0.4f, 1.0f)
            : ImVec4(0.95f, 0.4f, 0.4f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(I18n::T(m_saveStatusOk
            ? "editor.panel.material.save_ok"
            : "editor.panel.material.save_fail").c_str());
        ImGui::PopStyleColor();
        --m_saveStatusFrames;
    }

    // ===== Columna derecha: preview (solo en TwoColumns) =====
    if (layout == LayoutMode::TwoColumns) {
        ImGui::NextColumn();

        ImGui::TextUnformatted(I18n::T("editor.panel.material.preview").c_str());
        ImGui::Separator();

        drawPreviewBlock();

        ImGui::Columns(1);
    }

    ImGui::End();
}

} // namespace Mood
