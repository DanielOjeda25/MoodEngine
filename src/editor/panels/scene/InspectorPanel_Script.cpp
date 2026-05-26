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
    if (!beginComponentSection<ScriptComponent>(e, ICON_FA_FILE_CODE " Script")) return;
    char buf[512];
    std::snprintf(buf, sizeof(buf), "%s", sc.path.c_str());
    const std::string pathLabel = I18n::T("editor.panel.inspector.script.path") + "##sc";
    if (ImGui::InputText(pathLabel.c_str(), buf, sizeof(buf))) {
        sc.path = buf;
        sc.loaded = false;
        sc.lastError.clear();
        m_editedThisFrame = true;
    }
    // F3H12: undo del path InputText. El setter aplica path + resetea
    // loaded/lastError para que el ScriptSystem recargue.
    detail::pushEditIfDone<std::string>(m_editTracker, m_ui, e, sc.path,
        [](Entity& en, const std::string& v) {
            auto& s = en.getComponent<ScriptComponent>();
            s.path = v;
            s.loaded = false;
            s.lastError.clear();
        },
        "Editar script path");
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
            // F3H12: cada tipo expone undo via pushEditIfDone<T>. Setter
            // captura prop.name por valor (el cmd puede vivir mas que el
            // sc.exposedProps vector si el script se recarga). Setter
            // tambien resetea loaded/lastError para que ScriptSystem
            // re-aplique el override.
            const std::string propName = prop.name;
            switch (prop.type) {
                case ExposedType::Number: {
                    f32 v = std::get<f32>(current);
                    if (ImGui::DragFloat(prop.name.c_str(), &v, 0.1f)) {
                        sc.overrides[prop.name] = v;
                        changed = true;
                    }
                    detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, v,
                        [propName](Entity& en, const f32& val) {
                            auto& s = en.getComponent<ScriptComponent>();
                            s.overrides[propName] = val;
                            s.loaded = false;
                            s.lastError.clear();
                        },
                        "Editar script prop (Number)");
                    break;
                }
                case ExposedType::Bool: {
                    bool v = std::get<bool>(current);
                    if (ImGui::Checkbox(prop.name.c_str(), &v)) {
                        // Checkbox YA toggled `v` localmente; pushAtomicEdit
                        // captura before=!v, after=v.
                        sc.overrides[prop.name] = v;
                        detail::pushAtomicEdit<bool>(m_ui, e, !v, v,
                            [propName](Entity& en, const bool& val) {
                                auto& s = en.getComponent<ScriptComponent>();
                                s.overrides[propName] = val;
                                s.loaded = false;
                                s.lastError.clear();
                            },
                            "Toggle script prop (Bool)");
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
                        v.assign(sbuf);
                        sc.overrides[prop.name] = v;
                        changed = true;
                    }
                    detail::pushEditIfDone<std::string>(m_editTracker, m_ui, e, v,
                        [propName](Entity& en, const std::string& val) {
                            auto& s = en.getComponent<ScriptComponent>();
                            s.overrides[propName] = val;
                            s.loaded = false;
                            s.lastError.clear();
                        },
                        "Editar script prop (String)");
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
                    detail::pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, v,
                        [propName](Entity& en, const glm::vec3& val) {
                            auto& s = en.getComponent<ScriptComponent>();
                            s.overrides[propName] = val;
                            s.loaded = false;
                            s.lastError.clear();
                        },
                        "Editar script prop (Vec3)");
                    break;
                }
            }

            ImGui::SameLine();
            if (ImGui::SmallButton(I18n::T("editor.panel.inspector.script.reset").c_str())) {
                // F3H12: el reset borra el override del map. Undo no
                // implementado para este boton — re-editar el slider
                // restaura el override (los exposed props default vienen
                // del script Lua, no hace falta snapshot). Follow-up: si
                // se vuelve molesto, agregar un EditScriptOverrideCommand
                // que snapshotee la entry pre-borrado.
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
