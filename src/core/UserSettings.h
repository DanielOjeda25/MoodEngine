#pragma once

// UserSettings (F2H43): preferencias del usuario persistidas a un JSON
// global por usuario (no por proyecto). Vive en
// `%APPDATA%\MoodEngine\settings.json` (Windows). Compartido entre
// MoodEditor y MoodPlayer — ambos llaman `init()` al arrancar y leen
// el idioma persistido para pasarselo a `I18n::init()`.
//
// Convencion: el setter marca dirty pero NO escribe al disco. El caller
// es responsable de llamar `save()` cuando corresponde (al cambiar
// idioma desde el menu, al salir del programa, etc). Esto evita writes
// accidentales por cambios transitorios.

#include "engine/i18n/I18n.h"

#include <filesystem>

namespace Mood::UserSettings {

/// @brief Lee `settings.json` del disco. Si no existe o tiene parse
///        error, usa defaults (idioma=Spanish) y NO crea el archivo
///        (se creara en el primer `save()`). Llamar UNA vez al arrancar
///        antes de `I18n::init()`.
void init();

/// @brief No-op por ahora; reservado para futuro flush al disco si se
///        agrega autosave-on-change.
void shutdown();

/// @brief Escribe el estado actual al disco. Crea el directorio
///        `%APPDATA%\MoodEngine\` si no existe. Loggea warn si falla.
bool save();

/// @brief Idioma cargado de `settings.json` (o default Spanish si no
///        habia archivo).
I18n::Language language();

/// @brief Marca el idioma nuevo en memoria. NO escribe al disco — el
///        caller llama `save()` cuando corresponde.
void setLanguage(I18n::Language lang);

/// @brief Path absoluto al `settings.json`. Util para debug/test.
std::filesystem::path settingsPath();

} // namespace Mood::UserSettings
