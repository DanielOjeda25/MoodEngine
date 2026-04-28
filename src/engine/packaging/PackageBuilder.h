#pragma once

// Empaquetado standalone de un proyecto MoodEngine — Hito 21 Bloque 5.
//
// Toma un Project + un destino + el directorio donde vive el .exe del
// editor (de ahi salen MoodPlayer.exe + DLLs + assets + shaders) y
// produce un paquete autocontenido:
//
//   <destDir>/<projectName>/
//     MoodPlayer.exe
//     SDL2d.dll  (o SDL2.dll en Release)
//     game.json
//     assets/      <- copia del engine
//     shaders/     <- copia del engine
//     project/<name>.moodproj
//     project/maps/*.moodmap
//
// El usuario puede zippear esa carpeta y mandarla a alguien que la
// abra con doble-click sin tener Visual Studio instalado.
//
// V1 (este hito): copia las carpetas `assets/` y `shaders/` enteras —
// mas grande de lo necesario, pero simple. Filtrar por refs reales en
// scripts/serializadores queda como polish reactivo si el tamano de
// los paquetes molesta.

#include "engine/serialization/ProjectSerializer.h"

#include <filesystem>
#include <string>

namespace Mood {

namespace PackageBuilder {

struct BuildConfig {
    /// @brief Proyecto activo del editor (ya guardado en disco).
    const Project& project;
    /// @brief Path absoluto al `.moodproj` de `project`. Se necesita
    ///        ademas de `project.root` para conocer el nombre del
    ///        archivo y armar el `game.json` con el path correcto.
    std::filesystem::path projectFilePath;
    /// @brief Carpeta destino donde se creara `<destDir>/<projectName>/`.
    std::filesystem::path destDir;
    /// @brief Carpeta donde vive `MoodPlayer.exe` + `SDL2*.dll` (junto al
    ///        editor, post-build de CMake). Tipicamente la misma donde
    ///        vive `MoodEditor.exe` — el caller la obtiene con
    ///        `SDL_GetBasePath`.
    std::filesystem::path engineExeDir;
    /// @brief True cuando el editor se compilo en Debug — el packager
    ///        copia `SDL2d.dll` (variante debug). En Release: `SDL2.dll`.
    bool isDebugBuild = true;
};

struct BuildResult {
    bool ok = false;
    /// @brief Path absoluto al `<destDir>/<projectName>/` resultante.
    std::filesystem::path outputDir;
    /// @brief Cantidad de archivos copiados (para feedback al usuario).
    int filesCopied = 0;
    /// @brief Mensaje de error legible si `!ok`.
    std::string error;
};

/// @brief Ejecuta el empaquetado. Side-effects: crea directorios, copia
///        archivos, escribe `game.json`. No requiere GL ni el motor
///        corriendo — pura I/O + JSON.
BuildResult build(const BuildConfig& cfg);

} // namespace PackageBuilder

} // namespace Mood
