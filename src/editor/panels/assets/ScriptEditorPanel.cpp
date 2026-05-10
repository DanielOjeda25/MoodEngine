#include "editor/panels/assets/ScriptEditorPanel.h"

#include "core/Log.h"
#include "engine/i18n/I18n.h"  // F2H43
#include "engine/scene/components/Components.h"

#include <imgui.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace Mood {

namespace {

// Tamaño del buffer fijo para InputTextMultiline. ImGui requiere un buffer
// de chars contiguos; usar std::string + ImGui::InputTextMultiline con flag
// CallbackResize seria lo "correcto" pero complica el wire-up. Para scripts
// chicos (hasta 32 KB) un buffer fijo es suficiente — la API de Lua que
// tenemos hoy genera scripts de 100-2000 lineas como mucho.
constexpr usize k_bufferSize = 32 * 1024;

} // namespace

void ScriptEditorPanel::loadFromDisk(const std::string& path) {
    m_loadedPath = path;
    m_buffer.clear();
    m_dirty = false;
    m_loadFailed = false;

    std::ifstream in(path);
    if (!in.good()) {
        m_loadFailed = true;
        Log::editor()->warn("ScriptEditorPanel: no se pudo abrir '{}'", path);
        return;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    m_buffer = ss.str();
    // Reservar headroom para edits sin reallocar a cada tecla.
    m_buffer.reserve(k_bufferSize);
}

bool ScriptEditorPanel::saveToDisk() {
    if (m_loadedPath.empty()) return false;
    std::ofstream out(m_loadedPath, std::ios::trunc);
    if (!out.good()) {
        Log::editor()->warn("ScriptEditorPanel: no se pudo escribir '{}'",
                             m_loadedPath);
        return false;
    }
    out << m_buffer;
    m_dirty = false;
    Log::editor()->info("ScriptEditorPanel: guardado '{}'", m_loadedPath);
    return true;
}

void ScriptEditorPanel::onImGuiRender() {
    if (!visible) return;
    if (!ImGui::Begin(name(), &visible)) {
        ImGui::End();
        return;
    }

    // Resolver path de la entidad seleccionada.
    std::string targetPath;
    if (static_cast<bool>(m_selected)
        && m_selected.hasComponent<ScriptComponent>()) {
        targetPath = m_selected.getComponent<ScriptComponent>().path;
    }

    if (targetPath.empty()) {
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.script_editor.no_entity").c_str());
        // Si no hay path, descartamos cualquier estado previo.
        if (!m_loadedPath.empty()) {
            m_loadedPath.clear();
            m_buffer.clear();
            m_dirty = false;
        }
        ImGui::End();
        return;
    }

    // Reload si cambio de entidad o de path. Pierde cambios sin guardar
    // — aceptable v1; un confirm-dialog seria polish futuro.
    if (targetPath != m_loadedPath) {
        loadFromDisk(targetPath);
    }

    if (m_loadFailed) {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1),
                            "%s",
                            I18n::T("editor.panel.script_editor.open_failed",
                                    targetPath).c_str());
        ImGui::End();
        return;
    }

    // Header con path + dirty flag.
    ImGui::Text("%s%s", targetPath.c_str(), m_dirty ? " *" : "");
    ImGui::SameLine();
    const bool clickedSave = ImGui::Button(I18n::T(m_dirty
        ? "editor.panel.script_editor.save"
        : "editor.panel.script_editor.save_clean").c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("Ctrl+S");

    // Hotkey: Ctrl+S guarda mientras el panel tiene foco (es decir, el
    // dev esta editando). Se chequea via ImGui input — solo dispara si
    // la ventana del panel esta enfocada para no pisar el Ctrl+S del
    // proyecto cuando el usuario esta en otro panel.
    const bool hotkeySave = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
                          && ImGui::GetIO().KeyCtrl
                          && ImGui::IsKeyPressed(ImGuiKey_S, false);

    // Buffer de edicion: mantenemos m_buffer en string + lo pasamos a
    // InputTextMultiline. Si crece, un realloc transparente.
    if (m_buffer.capacity() < k_bufferSize) {
        m_buffer.reserve(k_bufferSize);
    }
    // Flag CallbackResize permite que ImGui re-aloje el buffer si el dev
    // escribe mas de la capacidad inicial — esencial para no truncar
    // scripts largos en mitad de una edicion.
    auto resizeCb = [](ImGuiInputTextCallbackData* data) -> int {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
            auto* str = static_cast<std::string*>(data->UserData);
            str->resize(static_cast<usize>(data->BufTextLen));
            data->Buf = str->data();
        }
        return 0;
    };

    const ImVec2 editorSize = ImGui::GetContentRegionAvail();
    const bool changed = ImGui::InputTextMultiline(
        "##script_buffer",
        m_buffer.data(),
        m_buffer.capacity() + 1,
        editorSize,
        ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_AllowTabInput,
        resizeCb,
        &m_buffer);

    if (changed) m_dirty = true;
    if ((clickedSave && m_dirty) || hotkeySave) saveToDisk();

    ImGui::End();
}

} // namespace Mood
