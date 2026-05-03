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
// V1 (Hito 21): copia las carpetas `assets/` y `shaders/` enteras —
// simple pero infla el paquete con assets no usados.
// V2 (Hito 37 A — smart-pack): escanea los `.moodmap` del proyecto +
// los `.material` referenciados, y copia solo los assets realmente
// usados. Una whitelist defensiva (missing.png/missing.wav, skyboxes
// completo) asegura que los fallbacks del runtime siempre estan
// presentes. Default: smart-pack ON.

#include "engine/scene/serialization/ProjectSerializer.h"

#include <filesystem>
#include <string>
#include <unordered_set>

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
    /// @brief Hito 37 A. Si true (default), escanea `.moodmap` + `.material`
    ///        y solo copia assets referenciados + whitelist defensiva. Si
    ///        false, fallback al modo V1 (copy `assets/` entero).
    bool smartPack = true;
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

/// @brief Hito 37 A. Escanea cada `.moodmap` del proyecto + los
///        `.material` referenciados, y devuelve el set de paths
///        logicos (relativos a `assets/`) que el runtime va a leer.
///        Util para smart-pack y para tests.
///
///        El set NO incluye la whitelist defensiva (missing.*,
///        skyboxes/*) — esa la maneja `build()` aparte.
///        `engineAssetsDir` se usa solo para abrir `.material`
///        referenciados; si no se pasa, los `.material` quedan sin
///        expandir (sus texturas no se incluyen).
std::unordered_set<std::string> collectReferencedAssetPaths(
    const Project& project,
    const std::filesystem::path& engineAssetsDir = {});

} // namespace PackageBuilder

} // namespace Mood
