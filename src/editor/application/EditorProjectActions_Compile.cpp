// F2H24: handleCompileMap / handleExportObj (F2H20). Compilacion
// brush -> mesh estatica unificada + export OBJ. La compilacion es
// on-demand (no se persiste en .moodmap, no se usa en el runtime).
// Sirve para que el dev vea las stats post-cull/post-weld + exporte
// el mapa para herramientas externas (Blender, MeshLab).

#include "editor/application/EditorApplication.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/world/csg/CompileMap.h"
#include "engine/world/csg/MapExportObj.h"

#include <portable-file-dialogs.h>

#include <cstdio>
#include <string>

namespace Mood {

namespace {

/// @brief Construye el mensaje de stats que se muestra en el dialog
///        de "Compilar mapa" + en el OK de "Exportar OBJ".
std::string formatCompileStats(const Csg::CompileStats& s) {
    char buf[512];
    std::snprintf(buf, sizeof(buf),
        "Brushes:               %u  (omitidos: %u)\n"
        "Caras totales:         %u\n"
        "Caras internas culled: %u\n"
        "Triangulos finales:    %u\n"
        "Vertices pre-weld:     %u\n"
        "Vertices unicos:       %u\n"
        "Submeshes (materiales): %u",
        s.sourceBrushes, s.brushesSkipped,
        s.sourceFaces, s.culledInternalFaces,
        s.totalTriangles,
        s.totalVerticesPreWeld, s.totalVerticesUnique,
        s.submeshCount);
    return std::string(buf);
}

} // namespace

void EditorApplication::handleCompileMap() {
    if (!m_scene || !m_assetManager) {
        Log::editor()->warn("Compilar mapa: scene o assets no disponibles");
        return;
    }

    auto sources = Csg::collectBrushSourcesFromScene(*m_scene, *m_assetManager);
    auto compiled = Csg::compileMap(sources);

    const auto stats = formatCompileStats(compiled.stats);
    Log::editor()->info("[F2H20 compile]\n{}", stats);

    // Dialog informativo. pfd::message no soporta un layout multi-linea
    // sofisticado pero respeta los `\n` en el body.
    pfd::message("Compilacion de mapa", stats,
                 pfd::choice::ok, pfd::icon::info);
}

void EditorApplication::handleExportObj() {
    if (!m_scene || !m_assetManager) {
        Log::editor()->warn("Exportar OBJ: scene o assets no disponibles");
        return;
    }

    // Default filename: <currentMapName>.obj.
    const std::string defaultName =
        m_currentMapPath.stem().generic_string() + ".obj";
    std::filesystem::path startDir =
        m_project.has_value() ? m_project->root : std::filesystem::current_path();

    const auto sel = pfd::save_file(
        "Exportar mapa como OBJ",
        (startDir / defaultName).string(),
        {"Wavefront OBJ (*.obj)", "*.obj"},
        pfd::opt::none).result();
    if (sel.empty()) {
        Log::editor()->info("Exportar OBJ: cancelado");
        return;
    }

    auto sources = Csg::collectBrushSourcesFromScene(*m_scene, *m_assetManager);
    auto compiled = Csg::compileMap(sources);
    const auto result = Csg::writeObj(compiled, std::filesystem::path(sel));

    if (!result.success) {
        Log::editor()->warn("Exportar OBJ fallo: {}", result.errorMessage);
        pfd::message("Exportar OBJ",
                     std::string("Error al escribir el archivo:\n") +
                     result.errorMessage,
                     pfd::choice::ok, pfd::icon::error);
        return;
    }

    Log::editor()->info("OBJ exportado: {} ({} submeshes, {} tris)",
        result.objPath.generic_string(),
        compiled.stats.submeshCount, compiled.stats.totalTriangles);

    pfd::message("Exportar OBJ",
                 "OBJ exportado a:\n" + result.objPath.generic_string() +
                 "\n\n" + formatCompileStats(compiled.stats),
                 pfd::choice::ok, pfd::icon::info);
}

} // namespace Mood
