#include "engine/shader_graph/ShaderGraphAsset.h"

#include "core/Log.h"

#include <algorithm>
#include <fstream>

namespace Mood::ShaderGraph {

// =============================================================
// Helpers de enums <-> string
// =============================================================

const char* nodeTypeToString(NodeType t) {
    switch (t) {
        case NodeType::OutputPBR:     return "output_pbr";
        case NodeType::Color:         return "color";
        case NodeType::Float:         return "float";
        case NodeType::UV:            return "uv";
        case NodeType::Time:          return "time";
        case NodeType::SampleTexture: return "sample_texture";
        case NodeType::Add:           return "add";
        case NodeType::Multiply:      return "multiply";
        case NodeType::Lerp:          return "lerp";
        case NodeType::Power:         return "power";
        case NodeType::OneMinus:      return "one_minus";
        case NodeType::Saturate:      return "saturate";
        case NodeType::Clamp:         return "clamp";
        case NodeType::Dot:           return "dot";
        case NodeType::Cross:         return "cross";
        case NodeType::Normalize:     return "normalize";
        case NodeType::Length:        return "length";
        case NodeType::Reflect:       return "reflect";
        case NodeType::Fresnel:       return "fresnel";
        case NodeType::NormalMap:     return "normal_map";
    }
    return "color";
}

NodeType nodeTypeFromString(const std::string& s) {
    if (s == "output_pbr")     return NodeType::OutputPBR;
    if (s == "color")          return NodeType::Color;
    if (s == "float")          return NodeType::Float;
    if (s == "uv")             return NodeType::UV;
    if (s == "time")           return NodeType::Time;
    if (s == "sample_texture") return NodeType::SampleTexture;
    if (s == "add")            return NodeType::Add;
    if (s == "multiply")       return NodeType::Multiply;
    if (s == "lerp")           return NodeType::Lerp;
    if (s == "power")          return NodeType::Power;
    if (s == "one_minus")      return NodeType::OneMinus;
    if (s == "saturate")       return NodeType::Saturate;
    if (s == "clamp")          return NodeType::Clamp;
    if (s == "dot")            return NodeType::Dot;
    if (s == "cross")          return NodeType::Cross;
    if (s == "normalize")      return NodeType::Normalize;
    if (s == "length")         return NodeType::Length;
    if (s == "reflect")        return NodeType::Reflect;
    if (s == "fresnel")        return NodeType::Fresnel;
    if (s == "normal_map")     return NodeType::NormalMap;
    return NodeType::Color;
}

const char* pinTypeToString(PinType t) {
    switch (t) {
        case PinType::Float:     return "float";
        case PinType::Vec2:      return "vec2";
        case PinType::Vec3:      return "vec3";
        case PinType::Vec4:      return "vec4";
        case PinType::Texture2D: return "texture2d";
    }
    return "float";
}

PinType pinTypeFromString(const std::string& s) {
    if (s == "vec2")      return PinType::Vec2;
    if (s == "vec3")      return PinType::Vec3;
    if (s == "vec4")      return PinType::Vec4;
    if (s == "texture2d") return PinType::Texture2D;
    return PinType::Float;
}

// =============================================================
// NodeFactory: pins por default segun NodeType
// =============================================================

namespace {

Pin makePin(u32& nextPinId, PinType type, std::string name,
             glm::vec4 literal = {0.0f, 0.0f, 0.0f, 0.0f}) {
    Pin p;
    p.id = nextPinId++;
    p.type = type;
    p.name = std::move(name);
    p.literalValue = literal;
    return p;
}

void populateDefaultPins(Node& n, u32& nextPinId) {
    switch (n.type) {
        case NodeType::OutputPBR:
            // 5 inputs PBR. Sin outputs (terminal).
            n.inputs.push_back(makePin(nextPinId, PinType::Vec3,  "albedo",    {1.0f, 1.0f, 1.0f, 0.0f}));
            n.inputs.push_back(makePin(nextPinId, PinType::Float, "metallic",  {0.0f, 0.0f, 0.0f, 0.0f}));
            n.inputs.push_back(makePin(nextPinId, PinType::Float, "roughness", {0.5f, 0.0f, 0.0f, 0.0f}));
            n.inputs.push_back(makePin(nextPinId, PinType::Vec3,  "normal",    {0.0f, 0.0f, 1.0f, 0.0f}));
            n.inputs.push_back(makePin(nextPinId, PinType::Vec3,  "emissive",  {0.0f, 0.0f, 0.0f, 0.0f}));
            break;
        case NodeType::Color:
            n.outputs.push_back(makePin(nextPinId, PinType::Vec3, "out", {1.0f, 1.0f, 1.0f, 0.0f}));
            break;
        case NodeType::Float:
            n.outputs.push_back(makePin(nextPinId, PinType::Float, "out"));
            break;
        case NodeType::UV:
            n.outputs.push_back(makePin(nextPinId, PinType::Vec2, "uv"));
            break;
        case NodeType::Time:
            n.outputs.push_back(makePin(nextPinId, PinType::Float, "time"));
            break;
        case NodeType::SampleTexture:
            n.inputs.push_back(makePin(nextPinId, PinType::Vec2, "uv"));
            n.outputs.push_back(makePin(nextPinId, PinType::Vec4, "rgba"));
            n.outputs.push_back(makePin(nextPinId, PinType::Vec3, "rgb"));
            n.outputs.push_back(makePin(nextPinId, PinType::Float, "a"));
            break;
        case NodeType::Add:
        case NodeType::Multiply:
            n.inputs.push_back(makePin(nextPinId, PinType::Vec3, "a", {1.0f, 1.0f, 1.0f, 0.0f}));
            n.inputs.push_back(makePin(nextPinId, PinType::Vec3, "b", {1.0f, 1.0f, 1.0f, 0.0f}));
            n.outputs.push_back(makePin(nextPinId, PinType::Vec3, "out"));
            break;
        case NodeType::Lerp:
            n.inputs.push_back(makePin(nextPinId, PinType::Vec3,  "a", {0.0f, 0.0f, 0.0f, 0.0f}));
            n.inputs.push_back(makePin(nextPinId, PinType::Vec3,  "b", {1.0f, 1.0f, 1.0f, 0.0f}));
            n.inputs.push_back(makePin(nextPinId, PinType::Float, "t", {0.5f, 0.0f, 0.0f, 0.0f}));
            n.outputs.push_back(makePin(nextPinId, PinType::Vec3, "out"));
            break;
        case NodeType::Power:
            n.inputs.push_back(makePin(nextPinId, PinType::Float, "base", {1.0f, 0.0f, 0.0f, 0.0f}));
            n.inputs.push_back(makePin(nextPinId, PinType::Float, "exp",  {2.0f, 0.0f, 0.0f, 0.0f}));
            n.outputs.push_back(makePin(nextPinId, PinType::Float, "out"));
            break;
        case NodeType::OneMinus:
            n.inputs.push_back(makePin(nextPinId, PinType::Float, "in"));
            n.outputs.push_back(makePin(nextPinId, PinType::Float, "out"));
            break;
        case NodeType::Saturate:
            n.inputs.push_back(makePin(nextPinId, PinType::Float, "in"));
            n.outputs.push_back(makePin(nextPinId, PinType::Float, "out"));
            break;
        case NodeType::Clamp:
            n.inputs.push_back(makePin(nextPinId, PinType::Float, "in"));
            n.inputs.push_back(makePin(nextPinId, PinType::Float, "min", {0.0f, 0.0f, 0.0f, 0.0f}));
            n.inputs.push_back(makePin(nextPinId, PinType::Float, "max", {1.0f, 0.0f, 0.0f, 0.0f}));
            n.outputs.push_back(makePin(nextPinId, PinType::Float, "out"));
            break;
        case NodeType::Dot:
            n.inputs.push_back(makePin(nextPinId, PinType::Vec3, "a"));
            n.inputs.push_back(makePin(nextPinId, PinType::Vec3, "b"));
            n.outputs.push_back(makePin(nextPinId, PinType::Float, "out"));
            break;
        case NodeType::Cross:
            n.inputs.push_back(makePin(nextPinId, PinType::Vec3, "a"));
            n.inputs.push_back(makePin(nextPinId, PinType::Vec3, "b"));
            n.outputs.push_back(makePin(nextPinId, PinType::Vec3, "out"));
            break;
        case NodeType::Normalize:
            n.inputs.push_back(makePin(nextPinId, PinType::Vec3, "in"));
            n.outputs.push_back(makePin(nextPinId, PinType::Vec3, "out"));
            break;
        case NodeType::Length:
            n.inputs.push_back(makePin(nextPinId, PinType::Vec3, "in"));
            n.outputs.push_back(makePin(nextPinId, PinType::Float, "out"));
            break;
        case NodeType::Reflect:
            n.inputs.push_back(makePin(nextPinId, PinType::Vec3, "i"));
            n.inputs.push_back(makePin(nextPinId, PinType::Vec3, "n"));
            n.outputs.push_back(makePin(nextPinId, PinType::Vec3, "out"));
            break;
        case NodeType::Fresnel:
            n.inputs.push_back(makePin(nextPinId, PinType::Float, "power", {5.0f, 0.0f, 0.0f, 0.0f}));
            n.outputs.push_back(makePin(nextPinId, PinType::Float, "out"));
            break;
        case NodeType::NormalMap:
            n.inputs.push_back(makePin(nextPinId, PinType::Vec2, "uv"));
            n.outputs.push_back(makePin(nextPinId, PinType::Vec3, "normal"));
            break;
    }
}

} // namespace

Node makeNodeWithDefaultPins(NodeType type, u32& nextNodeId, u32& nextPinId) {
    Node n;
    n.id = nextNodeId++;
    n.type = type;
    populateDefaultPins(n, nextPinId);
    return n;
}

// =============================================================
// Asset: helpers de construccion
// =============================================================

Node& Asset::addNode(NodeType type, glm::vec2 editorPos) {
    Node n = makeNodeWithDefaultPins(type, nextNodeId, nextPinId);
    n.editorPos = editorPos;
    if (type == NodeType::OutputPBR) {
        outputNodeId = n.id;
    }
    nodes.push_back(std::move(n));
    return nodes.back();
}

void Asset::connect(u32 fromNodeId, u32 fromPinId, u32 toNodeId, u32 toPinId) {
    // Un input solo acepta una fuente -- remover edge previa al mismo toPin.
    disconnect(toPinId);
    edges.push_back({fromNodeId, fromPinId, toNodeId, toPinId});
}

void Asset::disconnect(u32 toPinId) {
    edges.erase(
        std::remove_if(edges.begin(), edges.end(),
            [toPinId](const Edge& e) { return e.toPinId == toPinId; }),
        edges.end());
}

Node* Asset::findNode(u32 nodeId) {
    for (auto& n : nodes) {
        if (n.id == nodeId) return &n;
    }
    return nullptr;
}

const Node* Asset::findNode(u32 nodeId) const {
    for (const auto& n : nodes) {
        if (n.id == nodeId) return &n;
    }
    return nullptr;
}

std::pair<Node*, Pin*> Asset::findPin(u32 pinId) {
    for (auto& n : nodes) {
        for (auto& p : n.inputs)  { if (p.id == pinId) return {&n, &p}; }
        for (auto& p : n.outputs) { if (p.id == pinId) return {&n, &p}; }
    }
    return {nullptr, nullptr};
}

std::pair<const Node*, const Pin*> Asset::findPin(u32 pinId) const {
    for (const auto& n : nodes) {
        for (const auto& p : n.inputs)  { if (p.id == pinId) return {&n, &p}; }
        for (const auto& p : n.outputs) { if (p.id == pinId) return {&n, &p}; }
    }
    return {nullptr, nullptr};
}

// =============================================================
// Serializacion JSON
// =============================================================

namespace {

nlohmann::json pinToJson(const Pin& p) {
    return nlohmann::json{
        {"id",   p.id},
        {"type", pinTypeToString(p.type)},
        {"name", p.name},
        {"literal", {p.literalValue.x, p.literalValue.y,
                     p.literalValue.z, p.literalValue.w}},
    };
}

Pin pinFromJson(const nlohmann::json& j) {
    Pin p;
    if (!j.is_object()) return p;
    p.id   = j.value("id", 0u);
    p.type = pinTypeFromString(j.value("type", std::string{}));
    p.name = j.value("name", std::string{});
    if (j.contains("literal") && j.at("literal").is_array() &&
        j.at("literal").size() == 4) {
        const auto& a = j.at("literal");
        p.literalValue = {a[0].get<float>(), a[1].get<float>(),
                          a[2].get<float>(), a[3].get<float>()};
    }
    return p;
}

nlohmann::json nodeToJson(const Node& n) {
    nlohmann::json inputsJ = nlohmann::json::array();
    for (const auto& p : n.inputs) inputsJ.push_back(pinToJson(p));
    nlohmann::json outputsJ = nlohmann::json::array();
    for (const auto& p : n.outputs) outputsJ.push_back(pinToJson(p));

    nlohmann::json j{
        {"id",      n.id},
        {"type",    nodeTypeToString(n.type)},
        {"pos",     {n.editorPos.x, n.editorPos.y}},
        {"inputs",  inputsJ},
        {"outputs", outputsJ},
    };
    if (!n.texturePath.empty()) j["texture_path"] = n.texturePath;
    return j;
}

Node nodeFromJson(const nlohmann::json& j) {
    Node n;
    if (!j.is_object()) return n;
    n.id = j.value("id", 0u);
    n.type = nodeTypeFromString(j.value("type", std::string{}));
    if (j.contains("pos") && j.at("pos").is_array() && j.at("pos").size() == 2) {
        n.editorPos = {j.at("pos")[0].get<float>(),
                       j.at("pos")[1].get<float>()};
    }
    if (j.contains("inputs") && j.at("inputs").is_array()) {
        for (const auto& pj : j.at("inputs")) n.inputs.push_back(pinFromJson(pj));
    }
    if (j.contains("outputs") && j.at("outputs").is_array()) {
        for (const auto& pj : j.at("outputs")) n.outputs.push_back(pinFromJson(pj));
    }
    n.texturePath = j.value("texture_path", std::string{});
    return n;
}

} // namespace

nlohmann::json Asset::toJson() const {
    nlohmann::json nodesJ = nlohmann::json::array();
    for (const auto& n : nodes) nodesJ.push_back(nodeToJson(n));

    nlohmann::json edgesJ = nlohmann::json::array();
    for (const auto& e : edges) {
        edgesJ.push_back({
            {"from_node", e.fromNodeId},
            {"from_pin",  e.fromPinId},
            {"to_node",   e.toNodeId},
            {"to_pin",    e.toPinId},
        });
    }

    return nlohmann::json{
        {"_version",       k_schemaVersion},
        {"next_node_id",   nextNodeId},
        {"next_pin_id",    nextPinId},
        {"output_node_id", outputNodeId},
        {"nodes",          nodesJ},
        {"edges",          edgesJ},
    };
}

Asset Asset::fromJson(const nlohmann::json& j) {
    Asset a;
    if (!j.is_object()) {
        Log::engine()->warn("[ShaderGraph] fromJson: JSON no es objeto -- asset vacio");
        return a;
    }
    const u32 ver = j.value("_version", 0u);
    if (ver != k_schemaVersion) {
        Log::engine()->warn("[ShaderGraph] fromJson: version incompatible (esperado {}, recibido {}) -- asset vacio",
                              k_schemaVersion, ver);
        return a;
    }
    a.nextNodeId   = j.value("next_node_id",   1u);
    a.nextPinId    = j.value("next_pin_id",    1u);
    a.outputNodeId = j.value("output_node_id", 0u);

    if (j.contains("nodes") && j.at("nodes").is_array()) {
        for (const auto& nj : j.at("nodes")) a.nodes.push_back(nodeFromJson(nj));
    }
    if (j.contains("edges") && j.at("edges").is_array()) {
        for (const auto& ej : j.at("edges")) {
            if (!ej.is_object()) continue;
            Edge e;
            e.fromNodeId = ej.value("from_node", 0u);
            e.fromPinId  = ej.value("from_pin",  0u);
            e.toNodeId   = ej.value("to_node",   0u);
            e.toPinId    = ej.value("to_pin",    0u);
            a.edges.push_back(e);
        }
    }
    return a;
}

std::optional<Asset> Asset::loadFromFile(const std::filesystem::path& fsPath) {
    std::ifstream in(fsPath);
    if (!in) {
        Log::engine()->warn("[ShaderGraph] loadFromFile: no se pudo abrir '{}'",
                              fsPath.generic_string());
        return std::nullopt;
    }
    nlohmann::json j;
    try {
        in >> j;
    } catch (const std::exception& e) {
        Log::engine()->warn("[ShaderGraph] loadFromFile: JSON invalido en '{}': {}",
                              fsPath.generic_string(), e.what());
        return std::nullopt;
    }
    const u32 ver = j.value("_version", 0u);
    if (ver != k_schemaVersion) {
        Log::engine()->warn("[ShaderGraph] loadFromFile: schema version {} != {} en '{}'",
                              ver, k_schemaVersion, fsPath.generic_string());
        return std::nullopt;
    }
    return Asset::fromJson(j);
}

bool Asset::saveToFile(const std::filesystem::path& fsPath) const {
    std::error_code ec;
    std::filesystem::create_directories(fsPath.parent_path(), ec);

    std::ofstream out(fsPath);
    if (!out) {
        Log::engine()->warn("[ShaderGraph] saveToFile: no se pudo abrir '{}'",
                              fsPath.generic_string());
        return false;
    }
    out << toJson().dump(2);
    if (!out) {
        Log::engine()->warn("[ShaderGraph] saveToFile: error al escribir '{}'",
                              fsPath.generic_string());
        return false;
    }
    return true;
}

} // namespace Mood::ShaderGraph
