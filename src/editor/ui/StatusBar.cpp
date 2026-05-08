#include "editor/ui/StatusBar.h"

#include <imgui.h>
#include <imgui_internal.h>

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
            ImGui::TextColored(fpsColor, "FPS: %.1f",
                                 static_cast<double>(m_fps));

            ImGui::Separator();

            // F2H23: modo con color diferenciador (rojo Play, azul Editor).
            // Antes era TextUnformatted blanco — el dev tenia que leer la
            // palabra para saber si estaba en Play o Editor mode.
            const ImVec4 modeColor = (mode == EditorMode::Play)
                ? ImVec4(1.0f, 0.45f, 0.45f, 1.0f)   // rojo claro
                : ImVec4(0.55f, 0.80f, 1.0f, 1.0f);  // azul claro
            ImGui::TextColored(modeColor, "%s",
                mode == EditorMode::Play ? "Play Mode" : "Editor Mode");

            // F2H17: sub-modo (Face / Object) en color distinto.
            if (mode == EditorMode::Editor && subMode != EditorSubMode::Object) {
                ImGui::SameLine();
                const char* subLabel = (subMode == EditorSubMode::Face)
                    ? " | Face Mode (3)" : " | Sub-mode";
                ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.10f, 1.0f),
                                     "%s", subLabel);
            }
            ImGui::Separator();
            ImGui::TextUnformatted(m_message.c_str());
            // F2H16: "Ultimo comando: <name>" Blender-style. F2H23: castellano
            // explicito (antes "Ultimo: ..."). Solo se muestra si hay algo
            // en el undo stack.
            if (!m_lastCommand.empty()) {
                ImGui::Separator();
                ImGui::Text("Ultimo comando: %s", m_lastCommand.c_str());
            }
            ImGui::EndMenuBar();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

} // namespace Mood
