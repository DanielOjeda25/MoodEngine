#pragma once

// I18n (F2H43): sistema de internacionalizacion para Editor + Player + HUD.
// API namespaced (mismo patron que `Mood::Log::*`).
//
// Uso tipico:
//   #include "engine/i18n/I18n.h"
//   ImGui::Text("%s", Mood::I18n::T("editor.menu.file").c_str());
//   ImGui::Text("%s", Mood::I18n::T("test.count", n).c_str());  // interpolacion fmt
//
// Keys son flat con dot notation: `"editor.menu.file"`, `"hud.label.health"`.
// Diccionarios viven en `assets/i18n/{en,es}.json`. El JSON es plano (no
// nested) — la dot notation es solo convencion de naming, no estructura.
//
// Idioma default: Espanol (el dev trabaja en ES). Fallback siempre en
// Ingles. Si una key falta en activo y fallback, devuelve la key cruda
// y loggea warn UNA vez por key (no spam por frame).

#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <string>

namespace Mood::I18n {

enum class Language {
    English,
    Spanish,
};

/// @brief Inicializa el sistema cargando el JSON del idioma default + el
///        de fallback (siempre Ingles). Llamar una vez al arrancar (Editor
///        o Player). Si el JSON del idioma default no existe o falla
///        parse, queda activo solo el fallback.
/// @param defaultLang Idioma activo al primer arranque.
/// @param dirFs Carpeta del filesystem con los `*.json` (default
///        `"assets/i18n"`).
void init(Language defaultLang = Language::Spanish,
          const std::string& dirFs = "assets/i18n");

/// @brief Limpia los diccionarios. Llamar al apagar.
void shutdown();

/// @brief Cambia el idioma activo recargando el JSON correspondiente. Si
///        falla la carga, no cambia nada y devuelve false.
bool setLanguage(Language lang);

/// @brief Idioma actualmente activo.
Language currentLanguage();

/// @brief Codigo ISO ("en", "es") para un Language. Estable para
///        persistencia en `settings.json`.
const char* languageCode(Language lang);

/// @brief Inverso: codigo "es" -> Language::Spanish. Cae a English si no
///        reconoce el codigo (forward-compat con futuros idiomas).
Language languageFromCode(const std::string& code);

/// @brief Lookup de la traduccion. Resolucion en orden:
///        1) diccionario activo, 2) diccionario fallback (Ingles),
///        3) la key cruda. Casos 2 y 3 loggean warn UNA vez por key.
std::string T(const std::string& key);

/// @brief Lookup con interpolacion estilo `fmt::format`. La traduccion
///        debe contener placeholders `{}`. Ej:
///          "test.count": "Found {} items"
///          T("test.count", 5) -> "Found 5 items"
///        Los args se forwardean a `fmt::format`. Si falta la key, hace
///        fmt sobre la key cruda (suele tirar fmt::format_error porque
///        la key no tiene `{}`; sirve como senial de error visible).
template <typename... Args>
std::string T(const std::string& key, Args&&... args) {
    const std::string fmtStr = T(key);
    try {
        return fmt::format(fmt::runtime(fmtStr), std::forward<Args>(args)...);
    } catch (const fmt::format_error& e) {
        spdlog::warn("[i18n] format error en key '{}': {}", key, e.what());
        return fmtStr;
    }
}

} // namespace Mood::I18n
