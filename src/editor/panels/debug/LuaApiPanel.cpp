#include "editor/panels/debug/LuaApiPanel.h"

#include "core/i18n/I18n.h"  // F2H43

#include <imgui.h>

namespace Mood {

namespace {

// Helper para una fila API: nombre + tipo + descripcion + snippet.
// Usa TextWrapped para que las descripciones largas no rompan el panel.
void drawApiEntry(const char* signature,
                  const char* description,
                  const char* snippet) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.85f, 1.0f, 1.0f));
    ImGui::TextUnformatted(signature);
    ImGui::PopStyleColor();
    ImGui::Indent();
    ImGui::TextWrapped("%s", description);
    if (snippet && *snippet) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
        ImGui::TextWrapped("ej. %s", snippet);
        ImGui::PopStyleColor();
    }
    ImGui::Unindent();
    ImGui::Spacing();
}

} // namespace

void LuaApiPanel::onImGuiRender() {
    if (!visible) return;
    if (!ImGui::Begin(name(), &visible)) {
        ImGui::End();
        return;
    }

    ImGui::TextWrapped("%s",
        I18n::T("editor.panel.lua_api.intro").c_str());
    ImGui::Spacing();

    if (ImGui::BeginTabBar("##LuaApiTabs")) {
        if (ImGui::BeginTabItem("self")) {
            ImGui::Spacing();
            drawApiEntry("self.tag : string (read-only)",
                "Tag de la entidad (mismo string que muestra el Hierarchy).",
                "log.info(\"hola desde \" .. self.tag)");
            drawApiEntry("self.transform : TransformComponent",
                "Acceso al Transform de la entidad. Mutable: cambios "
                "se reflejan en el render del mismo frame.",
                "self.transform.position.y = self.transform.position.y + dt");
            drawApiEntry("  .position : Vec3",
                "Posicion en mundo. Campos x / y / z mutables.",
                "self.transform.position.x = 5.0");
            drawApiEntry("  .rotationEuler : Vec3",
                "Rotacion Euler en grados (x/y/z).",
                "self.transform.rotationEuler.y = self.transform.rotationEuler.y + 45 * dt");
            drawApiEntry("  .scale : Vec3",
                "Escala uniforme o no. Default = (1,1,1).",
                "self.transform.scale.x = 2.0");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("hud")) {
            ImGui::Spacing();
            ImGui::TextWrapped("%s",
                I18n::T("editor.panel.lua_api.hud_intro").c_str());
            ImGui::Spacing();
            drawApiEntry("hud.setHp(int)",
                "Setea el HP visible en el bloque izquierdo del HUD.",
                "hud.setHp(75)");
            drawApiEntry("hud.setAmmo(int)",
                "Setea el AMMO visible en el bloque derecho del HUD.",
                "hud.setAmmo(12)");
            drawApiEntry("hud.setPaused(bool)",
                "Togglea el estado de pausa. Si pasa a true, el menu modal "
                "aparece y el cursor se libera. Sirve para forzar pausa "
                "desde script (ej. game-over).",
                "if hud.getHp() == 0 then hud.setPaused(true) end");
            drawApiEntry("hud.getHp() : int",
                "Lee el HP actual.",
                "local hp = hud.getHp()");
            drawApiEntry("hud.getAmmo() : int",
                "Lee el AMMO actual.",
                "local ammo = hud.getAmmo()");
            drawApiEntry("hud.getPaused() : bool",
                "Devuelve true si el juego esta pausado.",
                "if not hud.getPaused() then ... end");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("log")) {
            ImGui::Spacing();
            ImGui::TextWrapped("%s",
                I18n::T("editor.panel.lua_api.log_intro").c_str());
            ImGui::Spacing();
            drawApiEntry("log.info(string)",
                "Mensaje informativo. Color por defecto en la consola.",
                "log.info(\"Cargado el script\")");
            drawApiEntry("log.warn(string)",
                "Warning. Color amarillo en la consola.",
                "log.warn(\"HP critico: \" .. hud.getHp())");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Lifecycle")) {
            ImGui::Spacing();
            ImGui::TextWrapped("%s",
                I18n::T("editor.panel.lua_api.lifecycle_intro").c_str());
            ImGui::Spacing();
            drawApiEntry("function onUpdate(self, dt)",
                "Llamada cada frame mientras la entidad tiene "
                "ScriptComponent. `self` es la entidad, `dt` es delta "
                "time en segundos (float).",
                "function onUpdate(self, dt)\n"
                "    self.transform.rotationEuler.y =\n"
                "        self.transform.rotationEuler.y + 45 * dt\n"
                "end");
            ImGui::Spacing();
            ImGui::TextWrapped("%s",
                I18n::T("editor.panel.lua_api.hot_reload").c_str());
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

} // namespace Mood
