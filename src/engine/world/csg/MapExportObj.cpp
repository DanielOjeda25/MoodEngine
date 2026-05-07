#include "engine/world/csg/MapExportObj.h"

#include <fstream>
#include <system_error>
#include <unordered_set>

namespace Mood::Csg {

namespace {

constexpr const char* kDefaultMaterialName = "_default";

/// @brief Sanitiza un material name para `.mtl`: el formato no tolera
///        espacios ni caracteres especiales. Reemplaza no-alfanumericos
///        (excepto `_`, `.`, `-`) por `_`. Empty path => `_default`.
std::string sanitizeMtlName(const std::string& path) {
    if (path.empty()) return kDefaultMaterialName;
    std::string out;
    out.reserve(path.size());
    for (char c : path) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
             c == '_' || c == '.' || c == '-' || c == '/') {
            out.push_back(c);
        } else {
            out.push_back('_');
        }
    }
    return out;
}

} // namespace

ObjExportResult writeObj(const CompiledMap& compiled,
                          const std::filesystem::path& objPathIn) {
    ObjExportResult result;

    // Asegurar extension `.obj`.
    auto objPath = objPathIn;
    if (objPath.extension() != ".obj") {
        objPath += ".obj";
    }
    auto mtlPath = objPath;
    mtlPath.replace_extension(".mtl");

    result.objPath = objPath;
    result.mtlPath = mtlPath;

    // Crear directorios padre.
    std::error_code ec;
    if (!objPath.parent_path().empty()) {
        std::filesystem::create_directories(objPath.parent_path(), ec);
        if (ec) {
            result.success = false;
            result.errorMessage = "No se pudo crear el directorio padre: " +
                                  ec.message();
            return result;
        }
    }

    std::ofstream obj(objPath);
    if (!obj.is_open()) {
        result.success = false;
        result.errorMessage = "No se pudo abrir " + objPath.generic_string() +
                              " para escritura";
        return result;
    }
    std::ofstream mtl(mtlPath);
    if (!mtl.is_open()) {
        result.success = false;
        result.errorMessage = "No se pudo abrir " + mtlPath.generic_string() +
                              " para escritura";
        return result;
    }

    // ---------- .mtl ----------
    // Un newmtl por submesh distinto. Si el material es vacio, usa
    // `_default`. Dedup: si dos submeshes apuntan al mismo path,
    // solo escribimos el newmtl una vez (defensivo).
    std::unordered_set<std::string> seenMtl;
    for (const auto& sub : compiled.submeshes) {
        const std::string mtlName = sanitizeMtlName(sub.materialPath);
        if (seenMtl.count(mtlName) != 0u) continue;
        seenMtl.insert(mtlName);

        mtl << "newmtl " << mtlName << "\n";
        if (sub.materialPath.empty()) {
            // Default: gris claro, no diffuse map.
            mtl << "Kd 0.8 0.8 0.8\n";
        } else {
            mtl << "Kd 1.0 1.0 1.0\n";
            mtl << "map_Kd " << sub.materialPath << "\n";
        }
        mtl << "\n";
    }

    // ---------- .obj ----------
    obj << "# MoodEngine F2H20 — compiled map\n";
    obj << "mtllib " << mtlPath.filename().generic_string() << "\n";
    obj << "o map_compiled\n\n";

    // OBJ usa indices 1-based **globales** para v/vn/vt — debemos
    // emitir todos los vertices de TODOS los submeshes en orden y
    // luego usar offsets por submesh para las `f`. Cada submesh tiene
    // sus propios indices que en el OBJ deben sumarse al offset
    // acumulado.
    u32 vertexOffset = 1;  // OBJ es 1-based
    for (const auto& sub : compiled.submeshes) {
        for (const auto& v : sub.vertices) {
            obj << "v " << v.position.x << " " << v.position.y
                << " " << v.position.z << "\n";
        }
    }
    obj << "\n";
    for (const auto& sub : compiled.submeshes) {
        for (const auto& v : sub.vertices) {
            obj << "vn " << v.normal.x << " " << v.normal.y
                << " " << v.normal.z << "\n";
        }
    }
    obj << "\n";
    for (const auto& sub : compiled.submeshes) {
        for (const auto& v : sub.vertices) {
            obj << "vt " << v.uv.x << " " << v.uv.y << "\n";
        }
    }
    obj << "\n";

    // Caras: bucket por submesh para emitir `usemtl` antes de cada
    // bloque. Indices f a/ta/na = global = local + offset.
    for (const auto& sub : compiled.submeshes) {
        const std::string mtlName = sanitizeMtlName(sub.materialPath);
        obj << "usemtl " << mtlName << "\n";
        // OBJ triangle face: cada index del triangle list es 0-based;
        // OBJ es 1-based, asi que sumar 1 + offset acumulado.
        for (usize k = 0; k + 2 < sub.indices.size(); k += 3) {
            const u32 i0 = sub.indices[k]     + vertexOffset;
            const u32 i1 = sub.indices[k + 1] + vertexOffset;
            const u32 i2 = sub.indices[k + 2] + vertexOffset;
            obj << "f "
                << i0 << "/" << i0 << "/" << i0 << " "
                << i1 << "/" << i1 << "/" << i1 << " "
                << i2 << "/" << i2 << "/" << i2 << "\n";
        }
        obj << "\n";
        vertexOffset += static_cast<u32>(sub.vertices.size());
    }

    obj.flush();
    mtl.flush();

    result.success = true;
    return result;
}

} // namespace Mood::Csg
