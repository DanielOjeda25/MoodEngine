#include "editor/ui/StatusBar.h"

#include "editor/ui/IconsFontAwesome6.h"  // F2H37: icons en mode/submode
#include "core/UserSettings.h"  // F3H23: statsOverlay toggles
#include "core/i18n/I18n.h"  // F2H43

#include <cstdio>
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
            // F3H23: cada chip de métricas es opcional — controlado por
            // `UserSettings.editor.statsOverlay.show<X>`. Default: FPS +
            // Draws + Tris ON; resto OFF. El dev toggleta desde
            // Preferences > Editor > Stats overlay. Sin chips activos,
            // la status bar arranca directo con el modo (Editor/Play).
            const auto& cfg = UserSettings::editor().statsOverlay;
            bool anyChipDrawn = false;

            if (cfg.showFps) {
                // F2H23: FPS con color por rango (verde >=60 / amarillo 30-60 /
                // rojo <30) — feedback de performance al ojo.
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
                anyChipDrawn = true;
            }

            // F3H23: chips secundarios. Helper para formatear miles
            // ("8200" → "8.2k") en el chip de triangles. El resto van
            // como int plano.
            auto formatThousands = [](u32 v) -> std::string {
                if (v < 1000) return std::to_string(v);
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.1fk",
                              static_cast<double>(v) / 1000.0);
                return std::string(buf);
            };
            auto chipSep = [&]() {
                if (anyChipDrawn) ImGui::Separator();
            };
            const ImVec4 kStatsColor(0.78f, 0.78f, 0.82f, 1.0f);  // gris suave

            if (cfg.showDrawcalls) {
                chipSep();
                ImGui::TextColored(kStatsColor, "Draws %u", m_drawCalls);
                anyChipDrawn = true;
            }
            if (cfg.showTris) {
                chipSep();
                ImGui::TextColored(kStatsColor, "Tris %s",
                                     formatThousands(m_triangles).c_str());
                anyChipDrawn = true;
            }
            // F3H23: helper para formatear bytes a MB (display compacto).
            auto formatMB = [](u64 bytes) -> std::string {
                if (bytes == 0) return std::string("\xe2\x80\x94");  // "—"
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.0f MB",
                              static_cast<double>(bytes) / (1024.0 * 1024.0));
                return std::string(buf);
            };

            if (cfg.showMemGpu) {
                chipSep();
                ImGui::TextColored(kStatsColor, "VRAM %s",
                                     formatMB(m_vramUsedBytes).c_str());
                anyChipDrawn = true;
            }
            if (cfg.showMemCpu) {
                chipSep();
                ImGui::TextColored(kStatsColor, "RSS %s",
                                     formatMB(m_rssBytes).c_str());
                anyChipDrawn = true;
            }
            if (cfg.showLights) {
                chipSep();
                ImGui::TextColored(kStatsColor, "Lights %u", m_activeLights);
                anyChipDrawn = true;
            }
            if (cfg.showEntities) {
                chipSep();
                ImGui::TextColored(kStatsColor, "Ents %u", m_entityCount);
                anyChipDrawn = true;
            }

            if (anyChipDrawn) ImGui::Separator();

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
            // F2H77: badge "sin guardar". El " *" del titulo del SO es facil
            // de no ver; un punto ambar en la status bar lo hace evidente
            // dentro del editor (estilo VS Code). Solo aparece con cambios
            // pendientes (sin proyecto, m_projectDirty queda false).
            if (m_projectDirty) {
                ImGui::Separator();
                const std::string dirtyLabel = std::string(ICON_FA_CIRCLE " ") +
                    I18n::T("editor.statusbar.unsaved");
                ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.28f, 1.0f),
                                     "%s", dirtyLabel.c_str());
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
