#pragma once

// Exporter de CompiledMap -> Wavefront OBJ + MTL lateral (F2H20).
// El OBJ es el formato estandar interoperable (Blender, MeshLab,
// Substance, etc.). El MTL acompañante define los materiales por path
// logico — el visualizador externo no resolvera las texturas (los paths
// son relativos al proyecto de MoodEngine, no al directorio del OBJ),
// pero el grafo de submeshes queda preservado para que el dev pueda
// retexturizar despues.

#include "engine/world/csg/CompileMap.h"

#include <filesystem>
#include <string>

namespace Mood::Csg {

struct ObjExportResult {
    bool success = false;
    std::string errorMessage;     // si !success
    std::filesystem::path objPath;
    std::filesystem::path mtlPath;
};

/// @brief Escribe `compiled` como `.obj` + `.mtl` lateral (mismo
///        basename, extension cambiada). Crea los directorios padre
///        si no existen. Si `objPath` no termina en `.obj` el sufijo
///        se agrega.
///
///        Layout del .obj:
///          mtllib <basename>.mtl
///          o map_compiled
///          v <x> <y> <z>     (todos los vertices del compiled)
///          vn <x> <y> <z>    (todas las normales)
///          vt <u> <v>        (todas las UVs)
///          usemtl <materialPath>
///          f a/a/a b/b/b c/c/c
///          ...
///
///        Layout del .mtl (1 newmtl por submesh):
///          newmtl <materialPath>
///          map_Kd <materialPath>          ;; relative al proyecto Mood
///        Material vacio "" se escribe como `_default` con `Kd 0.8 0.8 0.8`.
ObjExportResult writeObj(const CompiledMap& compiled,
                          const std::filesystem::path& objPath);

} // namespace Mood::Csg
