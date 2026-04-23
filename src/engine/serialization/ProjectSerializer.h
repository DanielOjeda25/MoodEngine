#pragma once

// Proyecto del motor: una carpeta con un archivo `.moodproj` en la raiz,
// una subcarpeta `maps/` con uno o mas `.moodmap`, y (futuro) `textures/`
// para assets especificos de ese proyecto.
//
// Schema `.moodproj` (ver k_MoodprojFormatVersion):
//   {
//     "version": 1,
//     "name": "MiJuego",
//     "defaultMap": "maps/default.moodmap",
//     "maps": ["maps/default.moodmap", "maps/level1.moodmap"]
//   }
// Todos los paths son relativos a la carpeta del `.moodproj`.

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Mood {

struct Project {
    std::string name;
    /// Carpeta raiz del proyecto (contiene el .moodproj). No se serializa:
    /// se infiere del path del archivo al cargar.
    std::filesystem::path root;
    /// Rutas a los mapas, relativas a `root`.
    std::vector<std::filesystem::path> maps;
    /// Mapa que el editor abre al cargar el proyecto. Relativa a `root`.
    std::filesystem::path defaultMap;
};

class ProjectSerializer {
public:
    /// @brief Guarda el proyecto a `<project.root>/<name>.moodproj`.
    /// @throws std::runtime_error si no se puede abrir el archivo.
    static void save(const Project& project);

    /// @brief Carga un proyecto desde el path a su archivo `.moodproj`.
    ///        `Project::root` se setea al directorio padre del archivo.
    /// @return `nullopt` si el archivo no existe, esta mal formado o la
    ///         version declarada supera la soportada.
    static std::optional<Project> load(const std::filesystem::path& moodprojPath);

    /// @brief Crea la estructura de carpetas de un proyecto nuevo:
    ///        `<root>/maps/`, `<root>/textures/`, y `<root>/<name>.moodproj`
    ///        apuntando a un mapa default. El mapa en si se escribe aparte
    ///        via `SceneSerializer::save`.
    /// @return proyecto creado o `nullopt` si no se pudieron crear los
    ///         directorios (permisos, disco lleno, etc.).
    static std::optional<Project> createNewProject(
        const std::filesystem::path& root,
        const std::string& name,
        const std::string& defaultMapName = "default");
};

} // namespace Mood
