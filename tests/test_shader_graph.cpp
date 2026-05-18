// Tests del ShaderGraph (F2H62 Bloque G). Cubre:
//   - Asset::createNode con sockets default por NodeKind.
//   - Asset toJson/fromJson roundtrip preserva grafo + outputNodeId.
//   - Asset save/load disco roundtrip.
//   - Generador GLSL: caso minimo (OutputPBR sin conexiones) succeeded=true.
//   - Generador GLSL: caso conectado Color -> albedo emite expresion del Color.
//   - Generador: si NO hay OutputPBR -> succeeded=false.
//   - Generador: ciclo en el grafo -> succeeded=false con error.

#include <doctest/doctest.h>

#include "engine/nodegraph/Graph.h"
#include "engine/shader_graph/ShaderGraphAsset.h"
#include "engine/shader_graph/ShaderGraphGenerator.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>

using namespace Mood;
using namespace Mood::NodeGraph;
namespace SG = Mood::ShaderGraph;

// =====================================================================
// Asset CRUD
// =====================================================================

TEST_CASE("ShaderGraph::Asset: empty -> outputNodeId is invalid") {
    SG::Asset a;
    CHECK(a.outputNodeId() == k_invalidNodeId);
    CHECK(a.graph().nodeCount() == 0);
}

TEST_CASE("ShaderGraph::Asset: createNode(OutputPBR) tracks outputNodeId") {
    SG::Asset a;
    const NodeId out = a.createNode(SG::NodeKind::OutputPBR, {300.0f, 100.0f});
    CHECK(out != k_invalidNodeId);
    CHECK(a.outputNodeId() == out);
    CHECK(a.graph().nodeCount() == 1);

    // F2H63: OutputPBR tiene 6 inputs (albedo, metallic, roughness,
    // normal, emissive, opacity). Assets viejos cargan con 5 -- el
    // generator hace fallback. createNode siempre crea con la version
    // actual.
    const Node* n = a.graph().findNode(out);
    REQUIRE(n != nullptr);
    CHECK(n->inputs.size() == 6);
    CHECK(n->inputs[5].typeTag == SG::kSocketType_Float);
    CHECK(n->inputs[5].name    == "opacity");
    CHECK(n->outputs.empty());
    CHECK(n->title == "Output PBR");  // semantic label seteado por createNode
}

TEST_CASE("ShaderGraph::Asset: createNode(Color) produces vec3 output with default white") {
    SG::Asset a;
    const NodeId id = a.createNode(SG::NodeKind::Color, {0.0f, 0.0f});
    const Node* n = a.graph().findNode(id);
    REQUIRE(n != nullptr);
    CHECK(n->inputs.empty());
    REQUIRE(n->outputs.size() == 1);
    CHECK(n->outputs[0].typeTag == SG::kSocketType_Vec3);

    const glm::vec4 v = SG::getColorValue(*n);
    CHECK(v.x == doctest::Approx(1.0f));
    CHECK(v.y == doctest::Approx(1.0f));
    CHECK(v.z == doctest::Approx(1.0f));
}

TEST_CASE("ShaderGraph::Asset: initEmpty leaves exactly one OutputPBR") {
    SG::Asset a;
    a.initEmpty();
    CHECK(a.graph().nodeCount() == 1);
    CHECK(a.outputNodeId() != k_invalidNodeId);
}

TEST_CASE("ShaderGraph::Asset: removeNode(OutputPBR) clears outputNodeId") {
    SG::Asset a;
    a.initEmpty();
    const NodeId out = a.outputNodeId();
    REQUIRE(out != k_invalidNodeId);
    REQUIRE(a.removeNode(out));
    CHECK(a.outputNodeId() == k_invalidNodeId);
    CHECK(a.graph().nodeCount() == 0);
}

// =====================================================================
// JSON roundtrip
// =====================================================================

TEST_CASE("ShaderGraph::Asset: toJson/fromJson preserves nodes + outputId") {
    SG::Asset a;
    const NodeId outId = a.createNode(SG::NodeKind::OutputPBR, {300.0f, 100.0f});
    const NodeId colId = a.createNode(SG::NodeKind::Color, {50.0f, 50.0f});

    const auto j = a.toJson();
    SG::Asset roundTrip = SG::Asset::fromJson(j);

    CHECK(roundTrip.outputNodeId() == outId);
    CHECK(roundTrip.graph().nodeCount() == 2);
    const Node* col = roundTrip.graph().findNode(colId);
    REQUIRE(col != nullptr);
    CHECK(col->typeTag == SG::kNodeType_Color);
    // El title semantico que setea createNode tambien debe persistir.
    CHECK(col->title == "Color");
}

TEST_CASE("ShaderGraph::Asset: fromJson rejects bad schema version with empty asset") {
    nlohmann::json bad{
        {"_version", 999u},  // version invalida
        {"output_node_id", 1u},
        {"graph", nlohmann::json::object()},
    };
    SG::Asset a = SG::Asset::fromJson(bad);
    CHECK(a.graph().nodeCount() == 0);
    CHECK(a.outputNodeId() == k_invalidNodeId);
}

TEST_CASE("ShaderGraph::Asset: saveToFile + loadFromFile disco roundtrip") {
    SG::Asset a;
    a.initEmpty();
    a.createNode(SG::NodeKind::Color, {10.0f, 20.0f});

    const auto tmpDir = std::filesystem::temp_directory_path();
    const auto tmpPath = tmpDir / "moodengine_test_shader_graph.moodshader";
    REQUIRE(a.saveToFile(tmpPath));

    auto loaded = SG::Asset::loadFromFile(tmpPath);
    REQUIRE(loaded.has_value());
    CHECK(loaded->graph().nodeCount() == 2);
    CHECK(loaded->outputNodeId() == a.outputNodeId());

    std::filesystem::remove(tmpPath);
}

// =====================================================================
// Generator GLSL
// =====================================================================

namespace {

// Plantilla minima con TODOS los markers requeridos por el generador.
// Usar esta en tests evita depender de I/O de disco (la plantilla real
// vive en shaders/pbr_graph_template.frag).
constexpr const char* k_tplFixture = R"(#version 330 core
out vec4 FragColor;
in vec3 vWorldPos;
in vec3 vWorldNormal;
in vec2 vUv;
uniform vec3 uCameraPos;
uniform float uTime;
void main() {
__SHADERGRAPH_DECLS__
vec3 albedo = __SHADERGRAPH_ALBEDO__;
float metallic = __SHADERGRAPH_METALLIC__;
float roughness = __SHADERGRAPH_ROUGHNESS__;
vec3 normal = __SHADERGRAPH_NORMAL__;
vec3 emissive = __SHADERGRAPH_EMISSIVE__;
float opacity = __SHADERGRAPH_OPACITY__;
FragColor = vec4(albedo + emissive, opacity);
}
)";

}

TEST_CASE("Generator: graph minimal (OutputPBR solo) succeeded=true con literals fallback") {
    SG::Asset a;
    a.initEmpty();  // un OutputPBR con literals default
    auto res = SG::generateGlsl(a, k_tplFixture);
    CHECK(res.succeeded);
    CHECK(!res.glsl.empty());
    // El GLSL debe haber reemplazado todos los markers.
    CHECK(res.glsl.find("__SHADERGRAPH_") == std::string::npos);
}

TEST_CASE("Generator: no OutputPBR -> succeeded=false con error") {
    SG::Asset a;  // grafo vacio
    auto res = SG::generateGlsl(a, k_tplFixture);
    CHECK_FALSE(res.succeeded);
    const auto errs = res.messagesOf(SG::GenSeverity::Error);
    CHECK_FALSE(errs.empty());
}

TEST_CASE("Generator: template invalida (sin markers) -> succeeded=false") {
    SG::Asset a;
    a.initEmpty();
    auto res = SG::generateGlsl(a, "void main() {}");  // sin markers
    CHECK_FALSE(res.succeeded);
}

TEST_CASE("Generator: Color conectado a albedo emite literal del color en GLSL") {
    SG::Asset a;
    const NodeId outId = a.createNode(SG::NodeKind::OutputPBR, {300.0f, 100.0f});
    const NodeId colId = a.createNode(SG::NodeKind::Color, {50.0f, 50.0f});

    // Setear color rojo (1, 0, 0).
    Node* col = a.graph().findNode(colId);
    REQUIRE(col != nullptr);
    SG::setColorValue(*col, {1.0f, 0.0f, 0.0f, 1.0f});

    // Conectar Color.out -> OutputPBR.albedo (primer input).
    const Node* out = a.graph().findNode(outId);
    REQUIRE(out != nullptr);
    REQUIRE(!out->inputs.empty());
    REQUIRE(!col->outputs.empty());
    const SocketId fromSocket = col->outputs[0].id;
    const SocketId toAlbedo   = out->inputs[0].id;
    const LinkId link = a.graph().addLink(fromSocket, toAlbedo);
    CHECK(link != k_invalidLinkId);

    auto res = SG::generateGlsl(a, k_tplFixture);
    CHECK(res.succeeded);
    // El GLSL debe contener la literal del color (1.0, 0.0, 0.0) en algun lado.
    // Buscamos "vec3(1.0" o "vec3(1," como heuristica simple.
    const bool hasRed = res.glsl.find("vec3(1.") != std::string::npos
                      || res.glsl.find("vec3(1,") != std::string::npos;
    CHECK(hasRed);
}

TEST_CASE("Generator: ciclo en el grafo -> succeeded=false") {
    // Construimos un ciclo manualmente bypaseando canConnect (no
    // permitiria outputs->inputs del mismo nodo). El generador debe
    // detectarlo via DFS visited+inStack.
    SG::Asset a;
    const NodeId outId = a.createNode(SG::NodeKind::OutputPBR, {300.0f, 100.0f});
    const NodeId mulA  = a.createNode(SG::NodeKind::Multiply, {100.0f, 100.0f});
    const NodeId mulB  = a.createNode(SG::NodeKind::Multiply, {200.0f, 100.0f});

    const Node* a1 = a.graph().findNode(mulA);
    const Node* a2 = a.graph().findNode(mulB);
    const Node* o  = a.graph().findNode(outId);
    REQUIRE(a1 != nullptr); REQUIRE(a2 != nullptr); REQUIRE(o != nullptr);

    // mulA.out -> mulB.a
    a.graph().addLink(a1->outputs[0].id, a2->inputs[0].id);
    // mulB.out -> mulA.a (ciclo!)
    a.graph().addLink(a2->outputs[0].id, a1->inputs[0].id);
    // mulB.out -> OutputPBR.albedo (necesario para que el walk visite el ciclo)
    a.graph().addLink(a2->outputs[0].id, o->inputs[0].id);

    auto res = SG::generateGlsl(a, k_tplFixture);
    CHECK_FALSE(res.succeeded);
    const auto errs = res.messagesOf(SG::GenSeverity::Error);
    CHECK_FALSE(errs.empty());
}

TEST_CASE("Generator: marker duplicado en plantilla -> succeeded=false (regression F2H62)") {
    // Bug F2H62 polish: si la plantilla tiene el marcador en un comentario
    // Y en codigo, replaceOnce pisa el comentario y deja el codigo intacto,
    // mandando GLSL invalido al driver. El generador ahora detecta marcadores
    // residuales post-substitucion y aborta limpio.
    constexpr const char* k_tplDoubleMarker = R"(#version 330 core
// Doc: el primer __SHADERGRAPH_ALBEDO__ esta en un comentario.
out vec4 FragColor;
void main() {
__SHADERGRAPH_DECLS__
vec3 albedo = __SHADERGRAPH_ALBEDO__;
float metallic = __SHADERGRAPH_METALLIC__;
float roughness = __SHADERGRAPH_ROUGHNESS__;
vec3 normal = __SHADERGRAPH_NORMAL__;
vec3 emissive = __SHADERGRAPH_EMISSIVE__;
float opacity = __SHADERGRAPH_OPACITY__;
FragColor = vec4(albedo + emissive, opacity);
}
)";
    SG::Asset a;
    a.initEmpty();
    auto res = SG::generateGlsl(a, k_tplDoubleMarker);
    CHECK_FALSE(res.succeeded);
    const auto errs = res.messagesOf(SG::GenSeverity::Error);
    CHECK_FALSE(errs.empty());
}

TEST_CASE("Generator: template real del repo se substituye sin marcadores residuales") {
    // El generador corrio sobre el pbr_graph_template.frag real (lo cargamos
    // como archivo) no debe dejar ningun __SHADERGRAPH_ residual. Si falla,
    // el template tiene los marcadores en mas de un lugar -- la causa raiz
    // del bug F2H62 polish (markers en comentarios de cabecera).
    //
    // Para no depender de cwd del test runner, intentamos abrir desde varias
    // rutas relativas comunes. Si ninguna existe, skipear (CTest puede
    // ejecutar desde paths distintos).
    const std::filesystem::path candidates[] = {
        "shaders/pbr_graph_template.frag",
        "../shaders/pbr_graph_template.frag",
        "../../shaders/pbr_graph_template.frag",
        "../../../shaders/pbr_graph_template.frag",
    };
    std::string tpl;
    for (const auto& p : candidates) {
        std::ifstream f(p);
        if (f) {
            std::stringstream ss;
            ss << f.rdbuf();
            tpl = ss.str();
            break;
        }
    }
    if (tpl.empty()) {
        WARN("No se encontro pbr_graph_template.frag relativo al test runner -- skipping");
        return;
    }
    SG::Asset a;
    a.initEmpty();
    auto res = SG::generateGlsl(a, tpl);
    CHECK(res.succeeded);
    CHECK(res.glsl.find("__SHADERGRAPH_") == std::string::npos);
}

// =====================================================================
// Bloque F: samples shipados — cada sample debe loadear desde disco y
// generar GLSL valido (succeeded + sin marcadores residuales). Si
// alguien rompe el schema o renombra typeTag, estos tests lo cazan.
// =====================================================================

namespace {

// Carga el template real desde varias rutas relativas posibles (el cwd
// del test varia segun como se invoque CTest).
std::string loadRealTemplate() {
    const std::filesystem::path candidates[] = {
        "shaders/pbr_graph_template.frag",
        "../shaders/pbr_graph_template.frag",
        "../../shaders/pbr_graph_template.frag",
        "../../../shaders/pbr_graph_template.frag",
    };
    for (const auto& p : candidates) {
        std::ifstream f(p);
        if (f) {
            std::stringstream ss;
            ss << f.rdbuf();
            return ss.str();
        }
    }
    return {};
}

// Carga un .moodshader desde varias rutas relativas posibles.
std::optional<SG::Asset> loadSample(const std::string& filename) {
    const std::filesystem::path candidates[] = {
        std::filesystem::path("assets/shaders/graphs") / filename,
        std::filesystem::path("../assets/shaders/graphs") / filename,
        std::filesystem::path("../../assets/shaders/graphs") / filename,
        std::filesystem::path("../../../assets/shaders/graphs") / filename,
    };
    for (const auto& p : candidates) {
        if (std::filesystem::exists(p)) {
            return SG::Asset::loadFromFile(p);
        }
    }
    return std::nullopt;
}

void checkSampleCompiles(const std::string& filename) {
    const std::string tpl = loadRealTemplate();
    if (tpl.empty()) {
        WARN(("Template real no encontrada -- skipping sample " + filename).c_str());
        return;
    }
    auto assetOpt = loadSample(filename);
    if (!assetOpt.has_value()) {
        WARN(("Sample no encontrado: " + filename).c_str());
        return;
    }
    auto res = SG::generateGlsl(*assetOpt, tpl);
    INFO("Sample: ", filename);
    if (!res.succeeded) {
        for (const auto& m : res.messages) {
            INFO("  msg: ", m.text);
        }
    }
    CHECK(res.succeeded);
    CHECK(res.glsl.find("__SHADERGRAPH_") == std::string::npos);
}

} // namespace

TEST_CASE("Sample shipado: sample_water.moodshader carga y compila") {
    checkSampleCompiles("sample_water.moodshader");
}

TEST_CASE("Sample shipado: sample_gold.moodshader carga y compila") {
    checkSampleCompiles("sample_gold.moodshader");
}

TEST_CASE("Sample shipado: sample_hologram.moodshader carga y compila") {
    checkSampleCompiles("sample_hologram.moodshader");
}

TEST_CASE("Sample shipado F2H63: sample_glass.moodshader carga y compila") {
    checkSampleCompiles("sample_glass.moodshader");
}

TEST_CASE("Generator F2H63: OutputPBR con 5 inputs (asset viejo) compila con opacity=1.0 fallback") {
    // Back-compat: assets pre-F2H63 tienen 5 inputs (sin opacity). El
    // generador debe emitir "1.0" como literal en lugar de fallar y
    // dejar el marcador __SHADERGRAPH_OPACITY__ residual.
    //
    // Simulamos un asset viejo construyendo el JSON manualmente con 5
    // sockets en OutputPBR (no podemos usar createNode porque crea 6).
    const nlohmann::json j = nlohmann::json::parse(R"({
        "_version": 1,
        "graph": {
            "_version": 1,
            "links": [],
            "next_link_id": 1,
            "next_node_id": 2,
            "next_socket_id": 6,
            "nodes": [{
                "custom_data": {
                    "socket_literals": {
                        "1": [1.0, 0.5, 0.2, 0.0],
                        "2": [0.0, 0.0, 0.0, 0.0],
                        "3": [0.5, 0.0, 0.0, 0.0],
                        "4": [0.0, 0.0, 1.0, 0.0],
                        "5": [0.0, 0.0, 0.0, 0.0]
                    }
                },
                "id": 1,
                "inputs": [
                    {"id": 1, "kind": 0, "name": "albedo",    "owner_node": 1, "type_tag": "vec3"},
                    {"id": 2, "kind": 0, "name": "metallic",  "owner_node": 1, "type_tag": "float"},
                    {"id": 3, "kind": 0, "name": "roughness", "owner_node": 1, "type_tag": "float"},
                    {"id": 4, "kind": 0, "name": "normal",    "owner_node": 1, "type_tag": "vec3"},
                    {"id": 5, "kind": 0, "name": "emissive",  "owner_node": 1, "type_tag": "vec3"}
                ],
                "outputs": [],
                "position": [0.0, 0.0],
                "title": "Output PBR",
                "type_tag": "output_pbr"
            }]
        },
        "output_node_id": 1
    })");
    SG::Asset legacy = SG::Asset::fromJson(j);
    // Confirmar que el load preservo los 5 sockets.
    const Node* outNode = legacy.graph().findNode(legacy.outputNodeId());
    REQUIRE(outNode != nullptr);
    CHECK(outNode->inputs.size() == 5);

    // Generar GLSL: el opacity expr debe ser literal "1.0".
    auto res = SG::generateGlsl(legacy, k_tplFixture);
    CHECK(res.succeeded);
    CHECK(res.glsl.find("__SHADERGRAPH_") == std::string::npos);
    // Verificar que el opacity literal "1.0" aparece en el GLSL emitido
    // (en la linea `float opacity = ...;` del fixture).
    CHECK(res.glsl.find("float opacity = 1.0;") != std::string::npos);
}
