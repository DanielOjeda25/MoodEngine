#include "editor/panels/narrative/DialogBrowserPanel.h"

#include "core/Log.h"
#include "editor/panels/narrative/DialogEditorPanel.h"
#include "editor/ui/EditorUI.h"
#include "engine/dialog/DialogAsset.h"
#include "engine/i18n/I18n.h"

#include <imgui.h>

#include <algorithm>
#include <cstring>
#include <system_error>

namespace Mood {

namespace {
// Convencion del editor: directorios de assets viven en
// `<cwd>/assets/<kind>/`. Igual que Material Editor F2H42 usa
// `assets/materials/`.
constexpr const char* k_dialogsDir = "assets/dialogs";
} // namespace

std::optional<std::filesystem::path> DialogBrowserPanel::dialogsDir() const {
    namespace fs = std::filesystem;
    const fs::path dir = fs::current_path() / k_dialogsDir;
    std::error_code ec;
    if (!fs::exists(dir, ec)) {
        // Crearlo si no existe — facilita el primer save sin pelearse
        // con missing directory.
        fs::create_directories(dir, ec);
        if (ec) return std::nullopt;
    }
    return dir;
}

void DialogBrowserPanel::refresh() {
    namespace fs = std::filesystem;
    m_files.clear();
    const auto dir = dialogsDir();
    if (!dir.has_value()) {
        m_refreshed = true;
        return;
    }
    std::error_code ec;
    fs::directory_iterator it(*dir, ec);
    if (ec) {
        m_refreshed = true;
        return;
    }
    for (const auto& entry : it) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() == Dialog::k_fileExtension) {
            m_files.push_back(entry.path());
        }
    }
    std::sort(m_files.begin(), m_files.end(),
                [](const fs::path& a, const fs::path& b) {
                    return a.filename() < b.filename();
                });
    m_refreshed = true;
}

void DialogBrowserPanel::onImGuiRender() {
    if (!visible) return;
    if (!m_refreshed) refresh();

    if (!ImGui::Begin(name(), &visible)) {
        ImGui::End();
        return;
    }

    if (ImGui::Button(I18n::T("editor.panel.dialog_browser.new").c_str())) {
        m_newPopupOpen = true;
        m_newName.clear();
    }
    ImGui::SameLine();
    if (ImGui::Button(I18n::T("editor.panel.dialog_browser.refresh").c_str())) {
        refresh();
    }

    ImGui::Separator();

    if (m_files.empty()) {
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.dialog_browser.empty").c_str());
    } else {
        for (const auto& f : m_files) {
            const std::string filename = f.filename().string();
            if (ImGui::Selectable(filename.c_str())) {
                if (m_ui) {
                    m_ui->dialogEditor().openFromFile(f);
                }
            }
        }
    }

    drawNewDialogPopup();

    ImGui::End();
}

void DialogBrowserPanel::drawNewDialogPopup() {
    if (!m_newPopupOpen) return;
    ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_Appearing);
    if (!ImGui::Begin(I18n::T("editor.panel.dialog_browser.new_title").c_str(),
                       &m_newPopupOpen,
                       ImGuiWindowFlags_NoDocking |
                       ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }
    ImGui::TextUnformatted(I18n::T("editor.panel.dialog_browser.new_prompt").c_str());
    char buf[128];
    std::strncpy(buf, m_newName.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    if (ImGui::InputText("##new_name", buf, sizeof(buf))) {
        m_newName = buf;
    }
    const bool nameOk = !m_newName.empty() &&
        m_newName.find_first_of("/\\:*?\"<>|") == std::string::npos;
    ImGui::BeginDisabled(!nameOk);
    if (ImGui::Button(I18n::T("editor.panel.dialog_browser.create").c_str())) {
        const auto dir = dialogsDir();
        if (dir.has_value()) {
            const auto path = *dir / (m_newName + std::string(Dialog::k_fileExtension));
            Dialog::Asset a;
            a.metadata().name = m_newName;
            a.addLine(glm::vec2(100.0f, 100.0f));
            if (a.saveToFile(path)) {
                Log::editor()->info("[DialogBrowser] creado '{}'",
                                      path.generic_string());
                refresh();
                if (m_ui) m_ui->dialogEditor().openFromFile(path);
                m_newPopupOpen = false;
            }
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(I18n::T("editor.panel.dialog_browser.cancel").c_str())) {
        m_newPopupOpen = false;
    }
    ImGui::End();
}

} // namespace Mood
