// Tests del exporter Wavefront OBJ + MTL (F2H20). Cubren:
//   - writeObj produce los archivos .obj y .mtl en el destino.
//   - El .obj contiene mtllib, o, v, vn, vt, usemtl, f en cantidades
//     coherentes con la CompiledMap.
//   - El .mtl contiene newmtl por cada material distinto del compiled.
//   - Material vacio "" se manifiesta como `_default` con Kd gris.
//   - Path destino sin extension agrega `.obj` automaticamente.

#include <doctest/doctest.h>

#include "engine/world/csg/Brush.h"
#include "engine/world/csg/CompileMap.h"
#include "engine/world/csg/MapExportObj.h"

#include <glm/gtc/matrix_transform.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace Mood;
using namespace Mood::Csg;

namespace {

constexpr f32 k_eps = 1e-3f;

BrushSource boxSourceAt(const glm::vec3& center, f32 size = 1.0f,
                          const std::string& matPath = "") {
    BrushSource src;
    glm::mat4 m = glm::translate(glm::mat4(1.0f), center);
    m = glm::scale(m, glm::vec3(size));
    src.brush = makeBoxBrush(glm::mat4(1.0f));
    src.worldMatrix = m;
    src.materialPaths.push_back(matPath);
    return src;
}

std::string readFile(const std::filesystem::path& p) {
    std::ifstream in(p);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

usize countLinesStartingWith(const std::string& content,
                                const std::string& prefix) {
    usize count = 0;
    usize pos = 0;
    while (pos < content.size()) {
        const usize lineEnd = content.find('\n', pos);
        const std::string line = content.substr(
            pos, (lineEnd == std::string::npos ? content.size() : lineEnd) - pos);
        if (line.size() >= prefix.size() &&
            line.compare(0, prefix.size(), prefix) == 0) {
            ++count;
        }
        if (lineEnd == std::string::npos) break;
        pos = lineEnd + 1;
    }
    return count;
}

std::filesystem::path tempObjPath(const std::string& nameStem) {
    auto base = std::filesystem::temp_directory_path() / "mood_f2h20_tests";
    std::error_code ec;
    std::filesystem::create_directories(base, ec);
    return base / (nameStem + ".obj");
}

}  // namespace

TEST_CASE("writeObj: 1 box brush => archivos creados, contenido coherente") {
    auto compiled = compileMap({boxSourceAt(glm::vec3(0.0f), 1.0f, "tex/grid")});
    const auto path = tempObjPath("single_box");

    auto result = writeObj(compiled, path);
    REQUIRE(result.success);
    CHECK(std::filesystem::exists(result.objPath));
    CHECK(std::filesystem::exists(result.mtlPath));
    CHECK(result.objPath.extension() == ".obj");
    CHECK(result.mtlPath.extension() == ".mtl");

    const std::string obj = readFile(result.objPath);
    CHECK(obj.find("mtllib ") != std::string::npos);
    CHECK(obj.find("o map_compiled") != std::string::npos);
    CHECK(obj.find("usemtl tex/grid") != std::string::npos);
    // 24 vertices del compiled (box con normal split) => 24 v / 24 vn / 24 vt.
    CHECK(countLinesStartingWith(obj, "v ") == 24u);
    CHECK(countLinesStartingWith(obj, "vn ") == 24u);
    CHECK(countLinesStartingWith(obj, "vt ") == 24u);
    // 12 triangulos.
    CHECK(countLinesStartingWith(obj, "f ") == 12u);

    const std::string mtl = readFile(result.mtlPath);
    CHECK(mtl.find("newmtl tex/grid") != std::string::npos);
    CHECK(mtl.find("map_Kd tex/grid") != std::string::npos);
}

TEST_CASE("writeObj: 2 boxes con materiales distintos => 2 newmtl + 2 usemtl") {
    std::vector<BrushSource> sources = {
        boxSourceAt(glm::vec3(-3.0f, 0.0f, 0.0f), 1.0f, "tex/red"),
        boxSourceAt(glm::vec3( 3.0f, 0.0f, 0.0f), 1.0f, "tex/blue"),
    };
    auto compiled = compileMap(sources);
    const auto path = tempObjPath("two_boxes_distinct");

    auto result = writeObj(compiled, path);
    REQUIRE(result.success);

    const std::string obj = readFile(result.objPath);
    CHECK(countLinesStartingWith(obj, "usemtl ") == 2u);
    CHECK(countLinesStartingWith(obj, "f ") == 24u);  // 12 + 12

    const std::string mtl = readFile(result.mtlPath);
    CHECK(countLinesStartingWith(mtl, "newmtl ") == 2u);
    CHECK(mtl.find("newmtl tex/red") != std::string::npos);
    CHECK(mtl.find("newmtl tex/blue") != std::string::npos);
}

TEST_CASE("writeObj: material vacio se mapea a _default con Kd gris") {
    auto compiled = compileMap({boxSourceAt(glm::vec3(0.0f), 1.0f, "")});
    const auto path = tempObjPath("default_material");

    auto result = writeObj(compiled, path);
    REQUIRE(result.success);

    const std::string obj = readFile(result.objPath);
    CHECK(obj.find("usemtl _default") != std::string::npos);

    const std::string mtl = readFile(result.mtlPath);
    CHECK(mtl.find("newmtl _default") != std::string::npos);
    CHECK(mtl.find("Kd 0.8 0.8 0.8") != std::string::npos);
    // El default NO tiene map_Kd.
    CHECK(mtl.find("map_Kd _default") == std::string::npos);
}

TEST_CASE("writeObj: path sin extension agrega .obj automaticamente") {
    auto compiled = compileMap({boxSourceAt(glm::vec3(0.0f))});
    const auto basePath =
        std::filesystem::temp_directory_path() / "mood_f2h20_tests" / "noext";

    auto result = writeObj(compiled, basePath);
    REQUIRE(result.success);
    CHECK(result.objPath.extension() == ".obj");
    CHECK(result.mtlPath.extension() == ".mtl");
}

TEST_CASE("writeObj: 0 brushes => archivos vacios pero validos") {
    CompiledMap empty;  // sin brushes
    const auto path = tempObjPath("empty");

    auto result = writeObj(empty, path);
    REQUIRE(result.success);
    CHECK(std::filesystem::exists(result.objPath));

    const std::string obj = readFile(result.objPath);
    CHECK(obj.find("mtllib ") != std::string::npos);
    CHECK(countLinesStartingWith(obj, "v ") == 0u);
    CHECK(countLinesStartingWith(obj, "f ") == 0u);
}

TEST_CASE("writeObj: indices de cara son 1-based y respetan offset entre submeshes") {
    // 2 boxes con materiales distintos => 2 submeshes con 24 verts cada uno.
    // El segundo submesh debe usar indices 25..48.
    std::vector<BrushSource> sources = {
        boxSourceAt(glm::vec3(-3.0f, 0.0f, 0.0f), 1.0f, "tex/a"),
        boxSourceAt(glm::vec3( 3.0f, 0.0f, 0.0f), 1.0f, "tex/b"),
    };
    auto compiled = compileMap(sources);
    const auto path = tempObjPath("offsets");

    auto result = writeObj(compiled, path);
    REQUIRE(result.success);

    const std::string obj = readFile(result.objPath);
    // Los indices del primer submesh estan en [1..24]. Buscamos al menos
    // un "f 25/" para verificar que el offset global se aplica al segundo
    // submesh.
    CHECK(obj.find("f 25/") != std::string::npos);
}
