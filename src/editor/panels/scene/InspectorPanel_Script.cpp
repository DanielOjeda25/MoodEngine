// F2H24: Inspector — ScriptComponent (path + recargar + exposed
// properties con override por entidad).

#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"

#include "core/i18n/I18n.h"  // F2H43
#include "engine/scene/components/Components.h"

#include <imgui.h>

#include <cstdio>
#include <string>

namespace Mood {

// ScriptComponent
// Cambiar el path o pulsar Recargar pone `loaded=false`: el ScriptSystem
// crea un sol::state fresco la proxima vez que corra update(). Eso es
// mas fuerte que el hot-reload por mtime (que reutiliza el state);
// cuando el usuario recarga manualmente lo habitual es querer un reset
// limpio de globals.
void InspectorPanel::renderScriptSection(Entity e) {
    auto& sc = e.getComponent<ScriptComponent>();
    ImGui::SeparatorText(ICON_FA_FILE_CODE " Script");
    char buf[512];
    std::snprintf(buf, sizeof(buf), "%s", sc.path.c_str());
    const std::string pathLabel = I18n::T("editor.panel.inspector.script.path") + "##sc";
    if (ImGui::InputText(pathLabel.c_str(), buf, sizeof(buf))) {
        sc.path = buf;
        sc.loaded = false;
        sc.lastError.clear();
        m_editedThisFrame = true;
    }
    const std::string reloadLabel = I18n::T("editor.panel.inspector.script.reload") + "##sc";
    if (ImGui::Button(reloadLabel.c_str())) {
        sc.loaded = false;
        sc.lastError.clear();
    }
    if (!sc.lastError.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
        ImGui::TextWrapped("%s",
            I18n::T("editor.panel.inspector.script.error", sc.lastError).c_str());
        ImGui::PopStyleColor();
    }

    // --- Exposed properties (Hito 24) ---
    // Listadas en el orden que aparecieron al cargar el script —
    // estable mientras el script no edite las llamadas
    // engine.exposed. El widget depende del tipo inferido por el
    // binding; un boton "Reset" borra el override (vuelve al
    // default del script).
    if (!sc.exposedProps.empty()) {
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.inspector.script.exposed_props").c_str());
        for (const auto& prop : sc.exposedProps) {
            ImGui::PushID(prop.name.c_str());

            // Resolver el valor a mostrar: override si esta, sino
            // default. El editor escribe siempre al overrides map,
            // asi al primer edit se "materializa".
            ExposedValue current = prop.defaultValue;
            auto ovIt = sc.overrides.find(prop.name);
            if (ovIt != sc.overrides.end()) current = ovIt->second;

            bool changed = false;
            switch (prop.type) {
                case ExposedType::Number: {
                    f32 v = std::get<f32>(current);
                    if (ImGui::DragFloat(prop.name.c_str(), &v, 0.1f)) {
                        sc.overrides[prop.name] = v;
                        changed = true;
                    }
                    break;
                }
                case ExposedType::Bool: {
                    bool v = std::get<bool>(current);
                    if (ImGui::Checkbox(prop.name.c_str(), &v)) {
                        sc.overrides[prop.name] = v;
                        changed = true;
                    }
                    break;
                }
                case ExposedType::String: {
                    std::string v = std::get<std::string>(current);
                    char sbuf[256];
                    std::snprintf(sbuf, sizeof(sbuf), "%s", v.c_str());
                    if (ImGui::InputText(prop.name.c_str(),
                                         sbuf, sizeof(sbuf))) {
                        sc.overrides[prop.name] = std::string(sbuf);
                        changed = true;
                    }
                    break;
                }
                case ExposedType::Vec3: {
                    glm::vec3 v = std::get<glm::vec3>(current);
                    const bool isColor =
                        prop.name.find("color") != std::string::npos ||
                        prop.name.find("Color") != std::string::npos;
                    bool edited = false;
                    if (isColor) {
                        edited = ImGui::ColorEdit3(prop.name.c_str(), &v.x);
                    } else {
                        edited = ImGui::DragFloat3(prop.name.c_str(), &v.x, 0.1f);
                    }
                    if (edited) {
                        sc.overrides[prop.name] = v;
                        changed = true;
                    }
                    break;
                }
            }

            ImGui::SameLine();
            if (ImGui::SmallButton(I18n::T("editor.panel.inspector.script.reset").c_str())) {
                sc.overrides.erase(prop.name);
                changed = true;
            }

            // Cualquier cambio fuerza reload — el chunk top-level
            // se re-corre y engine.exposed devuelve el nuevo valor.
            if (changed) {
                sc.loaded = false;
                sc.lastError.clear();
                m_editedThisFrame = true;
            }

            ImGui::PopID();
        }
    }

    ImGui::Separator();
}

} // namespace Mood
