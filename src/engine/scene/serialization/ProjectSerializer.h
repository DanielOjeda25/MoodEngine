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

#include "core/Types.h"  // f32 (Hito 40 G)
#include "engine/project/Workspace.h"  // F2H7 (movido a engine/project en AUDIT-3)

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
    /// Hito 40 G: configuracion del feel del char controller, editable
    /// per-proyecto. Defaults coinciden con los hardcoded del Hito 34 C.
    /// Se persisten al `.moodproj` solo si difieren del default (para no
    /// ensuciar mapas viejos con campos nuevos).
    f32 coyoteWindowSec     = 0.10f;  // segundos
    f32 jumpBufferWindowSec = 0.15f;
    /// F2H7: workspaces del editor (Layout/Scripting/Profile/Materials).
    /// Vacio en proyecto nuevo: el WorkspaceManager los inicializa con
    /// defaults la primera vez que se abre. Al guardar, el editor captura
    /// el iniLayout actual y los persiste todos.
    std::vector<Workspace> workspaces;
    /// F2H35 Bloque E: toggle "Nombres" del MapEditorTopBar — muestra/
    /// oculta los labels arriba de los iconos de point entities en
    /// perspective + ortos. Default true. Persistido por proyecto solo
    /// si difiere del default (mismo patron que coyoteWindowSec).
    bool showEntityLabels = true;
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
    ///        apuntando a un mapa default `maps/<defaultMapName>.moodmap`.
    ///        El mapa en si se escribe aparte via `SceneSerializer::save`.
    ///
    ///        Si `defaultMapName` queda vacio, se usa `name` (el nombre del
    ///        proyecto). Esto evita colisiones cuando varios proyectos
    ///        comparten `root` (dos proyectos en la misma carpeta con mapas
    ///        llamados "default" se pisaban mutuamente).
    /// @return proyecto creado o `nullopt` si no se pudieron crear los
    ///         directorios (permisos, disco lleno, etc.).
    static std::optional<Project> createNewProject(
        const std::filesystem::path& root,
        const std::string& name,
        const std::string& defaultMapName = {});
};

} // namespace Mood
