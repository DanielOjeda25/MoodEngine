#include "editor/ui/StatusBar.h"

#include "editor/ui/IconsFontAwesome6.h"  // F2H37: icons en mode/submode
#include "engine/i18n/I18n.h"  // F2H43

#include <imgui.h>
#include <imgui_internal.h>

#include <string>

namespace Mood {

void StatusBar::draw(EditorMode mode, EditorSubMode subMode) {
    // Usamos BeginViewportSideBar para que ImGui reserve la franja inferior
    // del viewport principal ANTES del dockspace. Asi el Asset Browser no se
    // superpone con la status bar. Requiere llamarse antes de Dockspace::begin
    // (ver EditorUI::draw).
    const float statusHeight = ImGui::GetFrameHeight();

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (ImGui::BeginViewportSideBar("##MoodStatusBar", nullptr, ImGuiDir_Down, statusHeight, flags)) {
        if (ImGui::BeginMenuBar()) {
            // F2H23: FPS con color por rango (verde >=60 / amarillo 30-60 /
            // rojo <30) — feedback de performance al ojo sin tener que abrir
            // el Performance HUD. Sub-modo del Face Mode tambien obtiene
            // este tratamiento de color desde F2H17.
            ImVec4 fpsColor(0.4f, 0.95f, 0.4f, 1.0f);  // verde
            if (m_fps < 30.0f) {
                fpsColor = ImVec4(1.0f, 0.35f, 0.35f, 1.0f);  // rojo
            } else if (m_fps < 60.0f) {
                fpsColor = ImVec4(1.0f, 0.85f, 0.25f, 1.0f);  // amarillo
            }
            const std::string fpsLabel = std::string(ICON_FA_GAUGE " ") +
                I18n::T("editor.statusbar.fps",
                        static_cast<double>(m_fps));
            ImGui::TextColored(fpsColor, "%s", fpsLabel.c_str());

            ImGui::Separator();

            // F2H23: modo con color diferenciador (rojo Play, azul Editor).
            // F2H37: icon Play/Stop al inicio. El icon refuerza el color
            // — para devs daltonicos el shape del icon distingue.
            const ImVec4 modeColor = (mode == EditorMode::Play)
                ? ImVec4(1.0f, 0.45f, 0.45f, 1.0f)   // rojo claro
                : ImVec4(0.55f, 0.80f, 1.0f, 1.0f);  // azul claro
            const std::string modeLabel = (mode == EditorMode::Play)
                ? std::string(ICON_FA_PLAY " ") + I18n::T("editor.statusbar.mode_play")
                : std::string(ICON_FA_PEN_TO_SQUARE " ") + I18n::T("editor.statusbar.mode_editor");
            ImGui::TextColored(modeColor, "%s", modeLabel.c_str());

            // F2H17 + F2H30: sub-modo (Vertex / Edge / Face / Object).
            // F2H37: icon FA por sub-modo (mismos que MapEditorTopBar).
            if (mode == EditorMode::Editor && subMode != EditorSubMode::Object) {
                ImGui::SameLine();
                std::string subLabel;
                switch (subMode) {
                    case EditorSubMode::Vertex:
                        subLabel = std::string(" | ") + ICON_FA_CIRCLE_DOT + " " +
                            I18n::T("editor.statusbar.submode.vertex"); break;
                    case EditorSubMode::Edge:
                        subLabel = std::string(" | ") + ICON_FA_MINUS + " " +
                            I18n::T("editor.statusbar.submode.edge");   break;
                    case EditorSubMode::Face:
                        subLabel = std::string(" | ") + ICON_FA_VECTOR_SQUARE + " " +
                            I18n::T("editor.statusbar.submode.face"); break;
                    case EditorSubMode::Object: break; // unreachable
                }
                ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.10f, 1.0f),
                                     "%s", subLabel.c_str());
            }
            ImGui::Separator();
            ImGui::TextUnformatted(m_message.c_str());
            // F2H16: "Ultimo comando: <name>" Blender-style. F2H23: castellano
            // explicito (antes "Ultimo: ..."). Solo se muestra si hay algo
            // en el undo stack.
            if (!m_lastCommand.empty()) {
                ImGui::Separator();
                ImGui::Text("%s",
                    I18n::T("editor.statusbar.last_command",
                            m_lastCommand).c_str());
            }
            ImGui::EndMenuBar();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

} // namespace Mood
