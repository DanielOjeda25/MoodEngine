#include "engine/i18n/I18n.h"

#include "core/Log.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

namespace Mood::I18n {

namespace {

Language                                          s_current = Language::Spanish;
std::filesystem::path                             s_dir;
std::unordered_map<std::string, std::string>      s_active;
std::unordered_map<std::string, std::string>      s_fallback;  // siempre English
std::unordered_set<std::string>                   s_warnedKeys;

bool loadInto(std::unordered_map<std::string, std::string>& dict,
              const std::string& langCode) {
    const auto path = s_dir / (langCode + ".json");
    std::ifstream in(path);
    if (!in.is_open()) {
        Log::engine()->warn("[i18n] no se pudo abrir '{}'", path.generic_string());
        return false;
    }
    nlohmann::json j;
    try {
        in >> j;
    } catch (const std::exception& e) {
        Log::engine()->error("[i18n] parse error en '{}': {}",
                              path.generic_string(), e.what());
        return false;
    }
    if (!j.is_object()) {
        Log::engine()->error("[i18n] '{}' no es un objeto JSON",
                              path.generic_string());
        return false;
    }
    dict.clear();
    for (auto it = j.begin(); it != j.end(); ++it) {
        // Ignora keys que arrancan con `_` (convencion para metadata como
        // `_comment`); el resto debe ser string.
        if (!it.key().empty() && it.key()[0] == '_') continue;
        if (!it.value().is_string()) continue;
        dict.emplace(it.key(), it.value().get<std::string>());
    }
    Log::engine()->info("[i18n] cargado '{}' ({} keys)",
                         path.generic_string(), dict.size());
    return true;
}

} // namespace

void init(Language defaultLang, const std::string& dirFs) {
    s_dir = dirFs;
    s_warnedKeys.clear();
    // Fallback siempre en Ingles, cargado una sola vez en init.
    loadInto(s_fallback, "en");
    s_current = defaultLang;
    if (defaultLang == Language::English) {
        s_active = s_fallback;  // copia (no aliasing — el dict puede mutar)
    } else {
        if (!loadInto(s_active, languageCode(defaultLang))) {
            // Si falla la carga, quedamos solo con fallback (T() ira al
            // path 2 para todas las keys hasta que se llame setLanguage).
            s_active.clear();
        }
    }
}

void shutdown() {
    s_active.clear();
    s_fallback.clear();
    s_warnedKeys.clear();
}

bool setLanguage(Language lang) {
    if (lang == s_current && !s_active.empty()) return true;
    if (lang == Language::English) {
        s_active = s_fallback;
    } else {
        std::unordered_map<std::string, std::string> tmp;
        if (!loadInto(tmp, languageCode(lang))) return false;
        s_active = std::move(tmp);
    }
    s_current = lang;
    s_warnedKeys.clear();  // re-warn por si el idioma nuevo tiene huecos distintos
    Log::engine()->info("[i18n] idioma activo: {}", languageCode(lang));
    return true;
}

Language currentLanguage() { return s_current; }

const char* languageCode(Language lang) {
    switch (lang) {
        case Language::English: return "en";
        case Language::Spanish: return "es";
    }
    return "en";
}

Language languageFromCode(const std::string& code) {
    if (code == "es") return Language::Spanish;
    return Language::English;
}

std::string T(const std::string& key) {
    auto it = s_active.find(key);
    if (it != s_active.end()) return it->second;

    auto fit = s_fallback.find(key);
    if (fit != s_fallback.end()) {
        if (s_warnedKeys.insert(key).second) {
            Log::engine()->warn(
                "[i18n] key '{}' falta en idioma activo, usando fallback Ingles",
                key);
        }
        return fit->second;
    }

    if (s_warnedKeys.insert(key).second) {
        Log::engine()->warn(
            "[i18n] key '{}' falta en activo y fallback, devuelvo key cruda", key);
    }
    return key;
}

} // namespace Mood::I18n
