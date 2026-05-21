#pragma once

// F2H76: temas visuales del editor. Cada tema es una paleta ImGui aplicada
// al `ImGui::GetStyle()`. Set built-in (dark/light/midnight/sepia); el id se
// persiste en `UserSettings`. Themes con colores 100% custom editables por el
// usuario quedan para un hito futuro de la Sub-fase 2.7.

#include <string>
#include <vector>

namespace Mood::EditorThemes {

/// @brief Metadata de un tema para poblar el combo de Preferencias.
struct ThemeInfo {
    std::string id;       ///< id persistido en settings.json (ej. "dark").
    std::string i18nKey;  ///< clave i18n del nombre legible (ej. "theme.dark").
};

/// @brief Lista de temas disponibles (orden estable para el combo).
const std::vector<ThemeInfo>& available();

/// @brief Aplica el tema al `ImGui::GetStyle()` actual. Un `id` desconocido
///        cae a "dark" (defensivo). Llamable en cualquier momento (al startup
///        desde el settings, o live al cambiar el combo).
void apply(const std::string& id);

} // namespace Mood::EditorThemes
