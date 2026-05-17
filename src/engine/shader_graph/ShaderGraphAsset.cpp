#include "engine/shader_graph/ShaderGraphAsset.h"

#include "core/Log.h"

#include <fstream>
#include <string>

namespace Mood::ShaderGraph {

// =============================================================
// NodeKind <-> typeTag
// =============================================================

const std::vector<NodeKind>& allNodeKinds() {
    static const std::vector<NodeKind> kAll = {
        NodeKind::OutputPBR,
        NodeKind::Color,
        NodeKind::Float,
        NodeKind::UV,
        NodeKind::Time,
        NodeKind::SampleTexture,
        NodeKind::Add,
        NodeKind::Multiply,
        NodeKind::Lerp,
        NodeKind::Power,
        NodeKind::OneMinus,
        NodeKind::Saturate,
        NodeKind::Clamp,
        NodeKind::Dot,
        NodeKind::Cross,
        NodeKind::Normalize,
        NodeKind::Length,
        NodeKind::Reflect,
        NodeKind::Fresnel,
        NodeKind::NormalMap,
    };
    return kAll;
}

const char* nodeKindToTypeTag(NodeKind kind) {
    switch (kind) {
        case NodeKind::OutputPBR:     return kNodeType_OutputPBR;
        case NodeKind::Color:         return kNodeType_Color;
        case NodeKind::Float:         return kNodeType_Float;
        case NodeKind::UV:            return kNodeType_UV;
        case NodeKind::Time:          return kNodeType_Time;
        case NodeKind::SampleTexture: return kNodeType_SampleTexture;
        case NodeKind::Add:           return kNodeType_Add;
        case NodeKind::Multiply:      return kNodeType_Multiply;
        case NodeKind::Lerp:          return kNodeType_Lerp;
        case NodeKind::Power:         return kNodeType_Power;
        case NodeKind::OneMinus:      return kNodeType_OneMinus;
        case NodeKind::Saturate:      return kNodeType_Saturate;
        case NodeKind::Clamp:         return kNodeType_Clamp;
        case NodeKind::Dot:           return kNodeType_Dot;
        case NodeKind::Cross:         return kNodeType_Cross;
        case NodeKind::Normalize:     return kNodeType_Normalize;
        case NodeKind::Length:        return kNodeType_Length;
        case NodeKind::Reflect:       return kNodeType_Reflect;
        case NodeKind::Fresnel:       return kNodeType_Fresnel;
        case NodeKind::NormalMap:     return kNodeType_NormalMap;
    }
    return kNodeType_Color;
}

std::optional<NodeKind> nodeKindFromTypeTag(const std::string& tag) {
    if (tag == kNodeType_OutputPBR)     return NodeKind::OutputPBR;
    if (tag == kNodeType_Color)         return NodeKind::Color;
    if (tag == kNodeType_Float)         return NodeKind::Float;
    if (tag == kNodeType_UV)            return NodeKind::UV;
    if (tag == kNodeType_Time)          return NodeKind::Time;
    if (tag == kNodeType_SampleTexture) return NodeKind::SampleTexture;
    if (tag == kNodeType_Add)           return NodeKind::Add;
    if (tag == kNodeType_Multiply)      return NodeKind::Multiply;
    if (tag == kNodeType_Lerp)          return NodeKind::Lerp;
    if (tag == kNodeType_Power)         return NodeKind::Power;
    if (tag == kNodeType_OneMinus)      return NodeKind::OneMinus;
    if (tag == kNodeType_Saturate)      return NodeKind::Saturate;
    if (tag == kNodeType_Clamp)         return NodeKind::Clamp;
    if (tag == kNodeType_Dot)           return NodeKind::Dot;
    if (tag == kNodeType_Cross)         return NodeKind::Cross;
    if (tag == kNodeType_Normalize)     return NodeKind::Normalize;
    if (tag == kNodeType_Length)        return NodeKind::Length;
    if (tag == kNodeType_Reflect)       return NodeKind::Reflect;
    if (tag == kNodeType_Fresnel)       return NodeKind::Fresnel;
    if (tag == kNodeType_NormalMap)     return NodeKind::NormalMap;
    return std::nullopt;
}

const char* nodeKindDisplayName(NodeKind kind) {
    switch (kind) {
        case NodeKind::OutputPBR:     return "Output PBR";
        case NodeKind::Color:         return "Color";
        case NodeKind::Float:         return "Float";
        case NodeKind::UV:            return "UV";
        case NodeKind::Time:          return "Time";
        case NodeKind::SampleTexture: return "Sample Texture";
        case NodeKind::Add:           return "Add";
        case NodeKind::Multiply:      return "Multiply";
        case NodeKind::Lerp:          return "Lerp";
        case NodeKind::Power:         return "Power";
        case NodeKind::OneMinus:      return "One Minus";
        case NodeKind::Saturate:      return "Saturate";
        case NodeKind::Clamp:         return "Clamp";
        case NodeKind::Dot:           return "Dot";
        case NodeKind::Cross:         return "Cross";
        case NodeKind::Normalize:     return "Normalize";
        case NodeKind::Length:        return "Length";
        case NodeKind::Reflect:       return "Reflect";
        case NodeKind::Fresnel:       return "Fresnel";
        case NodeKind::NormalMap:     return "Normal Map";
    }
    return "Unknown";
}

// =============================================================
// customData helpers
// =============================================================

namespace {

// Convenciones de keys en customData:
//   "value":            [x, y, z, w]  (vec4, usado por Color/Float)
//   "texture_path":     "..."         (SampleTexture)
//   "socket_literals":  { "<socketId>": [x, y, z, w] }  (input pin literals)
constexpr const char* kKey_Value          = "value";
constexpr const char* kKey_TexturePath    = "texture_path";
constexpr const char* kKey_SocketLiterals = "socket_literals";

glm::vec4 vec4FromJsonArray(const nlohmann::json& j) {
    if (!j.is_array() || j.size() != 4) return {0.0f, 0.0f, 0.0f, 0.0f};
    return {j[0].get<float>(), j[1].get<float>(),
            j[2].get<float>(), j[3].get<float>()};
}

nlohmann::json vec4ToJsonArray(const glm::vec4& v) {
    return nlohmann::json{v.x, v.y, v.z, v.w};
}

bool isZeroVec4(const glm::vec4& v) {
    return v.x == 0.0f && v.y == 0.0f && v.z == 0.0f && v.w == 0.0f;
}

} // namespace

glm::vec4 getSocketLiteral(const NodeGraph::Node& node, NodeGraph::SocketId socketId) {
    if (!node.customData.contains(kKey_SocketLiterals)) return {0.0f, 0.0f, 0.0f, 0.0f};
    const auto& lits = node.customData.at(kKey_SocketLiterals);
    if (!lits.is_object()) return {0.0f, 0.0f, 0.0f, 0.0f};
    const std::string key = std::to_string(socketId);
    if (!lits.contains(key)) return {0.0f, 0.0f, 0.0f, 0.0f};
    return vec4FromJsonArray(lits.at(key));
}

void setSocketLiteral(NodeGraph::Node& node, NodeGraph::SocketId socketId, glm::vec4 value) {
    const std::string key = std::to_string(socketId);
    if (isZeroVec4(value)) {
        // Compactar: remover entry y, si quedo objeto vacio, remover el container.
        if (node.customData.contains(kKey_SocketLiterals)) {
            auto& lits = node.customData.at(kKey_SocketLiterals);
            if (lits.is_object() && lits.contains(key)) {
                lits.erase(key);
            }
            if (lits.is_object() && lits.empty()) {
                node.customData.erase(kKey_SocketLiterals);
            }
        }
        return;
    }
    if (!node.customData.contains(kKey_SocketLiterals) ||
        !node.customData.at(kKey_SocketLiterals).is_object()) {
        node.customData[kKey_SocketLiterals] = nlohmann::json::object();
    }
    node.customData[kKey_SocketLiterals][key] = vec4ToJsonArray(value);
}

std::string getTexturePath(const NodeGraph::Node& node) {
    if (!node.customData.contains(kKey_TexturePath)) return {};
    const auto& v = node.customData.at(kKey_TexturePath);
    if (!v.is_string()) return {};
    return v.get<std::string>();
}

void setTexturePath(NodeGraph::Node& node, std::string path) {
    if (path.empty()) {
        if (node.customData.contains(kKey_TexturePath)) {
            node.customData.erase(kKey_TexturePath);
        }
        return;
    }
    node.customData[kKey_TexturePath] = std::move(path);
}

glm::vec4 getColorValue(const NodeGraph::Node& node) {
    if (!node.customData.contains(kKey_Value)) return {1.0f, 1.0f, 1.0f, 1.0f};
    return vec4FromJsonArray(node.customData.at(kKey_Value));
}

void setColorValue(NodeGraph::Node& node, glm::vec4 value) {
    node.customData[kKey_Value] = vec4ToJsonArray(value);
}

float getFloatValue(const NodeGraph::Node& node) {
    if (!node.customData.contains(kKey_Value)) return 0.0f;
    const auto& v = node.customData.at(kKey_Value);
    if (!v.is_array() || v.empty()) return 0.0f;
    return v[0].get<float>();
}

void setFloatValue(NodeGraph::Node& node, float value) {
    node.customData[kKey_Value] = nlohmann::json{value, 0.0f, 0.0f, 0.0f};
}

// =============================================================
// Node factory: sockets default por kind
// =============================================================

namespace {

// Helpers compactos para no repetir 5 lineas por socket.
void addInput(NodeGraph::Graph& g, NodeGraph::NodeId nodeId,
              const char* tag, const char* name) {
    g.addSocket(nodeId, NodeGraph::SocketKind::Input, tag, name);
}

void addOutput(NodeGraph::Graph& g, NodeGraph::NodeId nodeId,
               const char* tag, const char* name) {
    g.addSocket(nodeId, NodeGraph::SocketKind::Output, tag, name);
}

void populateDefaultSockets(NodeGraph::Graph& g, NodeGraph::NodeId nodeId, NodeKind kind) {
    switch (kind) {
        case NodeKind::OutputPBR:
            addInput(g, nodeId, kSocketType_Vec3,  "albedo");
            addInput(g, nodeId, kSocketType_Float, "metallic");
            addInput(g, nodeId, kSocketType_Float, "roughness");
            addInput(g, nodeId, kSocketType_Vec3,  "normal");
            addInput(g, nodeId, kSocketType_Vec3,  "emissive");
            break;
        case NodeKind::Color:
            addOutput(g, nodeId, kSocketType_Vec3, "out");
            break;
        case NodeKind::Float:
            addOutput(g, nodeId, kSocketType_Float, "out");
            break;
        case NodeKind::UV:
            addOutput(g, nodeId, kSocketType_Vec2, "uv");
            break;
        case NodeKind::Time:
            addOutput(g, nodeId, kSocketType_Float, "time");
            break;
        case NodeKind::SampleTexture:
            addInput (g, nodeId, kSocketType_Vec2, "uv");
            addOutput(g, nodeId, kSocketType_Vec4, "rgba");
            addOutput(g, nodeId, kSocketType_Vec3, "rgb");
            addOutput(g, nodeId, kSocketType_Float, "a");
            break;
        case NodeKind::Add:
        case NodeKind::Multiply:
            addInput (g, nodeId, kSocketType_Vec3, "a");
            addInput (g, nodeId, kSocketType_Vec3, "b");
            addOutput(g, nodeId, kSocketType_Vec3, "out");
            break;
        case NodeKind::Lerp:
            addInput (g, nodeId, kSocketType_Vec3,  "a");
            addInput (g, nodeId, kSocketType_Vec3,  "b");
            addInput (g, nodeId, kSocketType_Float, "t");
            addOutput(g, nodeId, kSocketType_Vec3,  "out");
            break;
        case NodeKind::Power:
            addInput (g, nodeId, kSocketType_Float, "base");
            addInput (g, nodeId, kSocketType_Float, "exp");
            addOutput(g, nodeId, kSocketType_Float, "out");
            break;
        case NodeKind::OneMinus:
        case NodeKind::Saturate:
            addInput (g, nodeId, kSocketType_Float, "in");
            addOutput(g, nodeId, kSocketType_Float, "out");
            break;
        case NodeKind::Clamp:
            addInput (g, nodeId, kSocketType_Float, "in");
            addInput (g, nodeId, kSocketType_Float, "min");
            addInput (g, nodeId, kSocketType_Float, "max");
            addOutput(g, nodeId, kSocketType_Float, "out");
            break;
        case NodeKind::Dot:
            addInput (g, nodeId, kSocketType_Vec3,  "a");
            addInput (g, nodeId, kSocketType_Vec3,  "b");
            addOutput(g, nodeId, kSocketType_Float, "out");
            break;
        case NodeKind::Cross:
            addInput (g, nodeId, kSocketType_Vec3, "a");
            addInput (g, nodeId, kSocketType_Vec3, "b");
            addOutput(g, nodeId, kSocketType_Vec3, "out");
            break;
        case NodeKind::Normalize:
            addInput (g, nodeId, kSocketType_Vec3, "in");
            addOutput(g, nodeId, kSocketType_Vec3, "out");
            break;
        case NodeKind::Length:
            addInput (g, nodeId, kSocketType_Vec3,  "in");
            addOutput(g, nodeId, kSocketType_Float, "out");
            break;
        case NodeKind::Reflect:
            addInput (g, nodeId, kSocketType_Vec3, "i");
            addInput (g, nodeId, kSocketType_Vec3, "n");
            addOutput(g, nodeId, kSocketType_Vec3, "out");
            break;
        case NodeKind::Fresnel:
            addInput (g, nodeId, kSocketType_Float, "power");
            addOutput(g, nodeId, kSocketType_Float, "out");
            break;
        case NodeKind::NormalMap:
            addInput (g, nodeId, kSocketType_Vec2, "uv");
            addOutput(g, nodeId, kSocketType_Vec3, "normal");
            break;
    }
}

// Setea los literals default en customData para los sockets que tienen
// un valor inicial sensato. Se llama DESPUES de populateDefaultSockets.
void populateDefaultLiterals(NodeGraph::Node& node, NodeKind kind) {
    switch (kind) {
        case NodeKind::OutputPBR: {
            // sockets en orden: albedo, metallic, roughness, normal, emissive
            if (node.inputs.size() == 5) {
                setSocketLiteral(node, node.inputs[0].id, {1.0f, 1.0f, 1.0f, 0.0f}); // albedo blanco
                setSocketLiteral(node, node.inputs[1].id, {0.0f, 0.0f, 0.0f, 0.0f}); // metallic 0
                setSocketLiteral(node, node.inputs[2].id, {0.5f, 0.0f, 0.0f, 0.0f}); // roughness 0.5
                setSocketLiteral(node, node.inputs[3].id, {0.0f, 0.0f, 1.0f, 0.0f}); // normal Z+
                setSocketLiteral(node, node.inputs[4].id, {0.0f, 0.0f, 0.0f, 0.0f}); // emissive 0
            }
            break;
        }
        case NodeKind::Color:
            setColorValue(node, {1.0f, 1.0f, 1.0f, 1.0f});
            break;
        case NodeKind::Float:
            setFloatValue(node, 0.0f);
            break;
        case NodeKind::Power: {
            // exp = 2.0 default
            if (node.inputs.size() == 2) {
                setSocketLiteral(node, node.inputs[1].id, {2.0f, 0.0f, 0.0f, 0.0f});
            }
            break;
        }
        case NodeKind::Lerp: {
            // a=0, b=1, t=0.5
            if (node.inputs.size() == 3) {
                setSocketLiteral(node, node.inputs[1].id, {1.0f, 1.0f, 1.0f, 0.0f});
                setSocketLiteral(node, node.inputs[2].id, {0.5f, 0.0f, 0.0f, 0.0f});
            }
            break;
        }
        case NodeKind::Clamp: {
            // min=0, max=1
            if (node.inputs.size() == 3) {
                setSocketLiteral(node, node.inputs[1].id, {0.0f, 0.0f, 0.0f, 0.0f});
                setSocketLiteral(node, node.inputs[2].id, {1.0f, 0.0f, 0.0f, 0.0f});
            }
            break;
        }
        case NodeKind::Fresnel: {
            // power = 5.0 default (clasico Schlick)
            if (node.inputs.size() == 1) {
                setSocketLiteral(node, node.inputs[0].id, {5.0f, 0.0f, 0.0f, 0.0f});
            }
            break;
        }
        default:
            break;
    }
}

} // namespace

// =============================================================
// Asset CRUD
// =============================================================

NodeGraph::NodeId Asset::createNode(NodeKind kind, glm::vec2 position) {
    const NodeGraph::NodeId nodeId =
        m_graph.addNode(nodeKindToTypeTag(kind), position);

    populateDefaultSockets(m_graph, nodeId, kind);

    NodeGraph::Node* n = m_graph.findNode(nodeId);
    if (n) {
        // El title bar del editor visual lee n->title; si esta vacio
        // el wrapper cae a "Node #ID". Seteamos el display name del
        // kind ("Color", "Output PBR", "Lerp", ...) para que el dev
        // vea labels semanticos en lugar de IDs anonimos.
        n->title = nodeKindDisplayName(kind);
        populateDefaultLiterals(*n, kind);
    }

    if (kind == NodeKind::OutputPBR) {
        m_outputNodeId = nodeId;
    }
    return nodeId;
}

bool Asset::removeNode(NodeGraph::NodeId id) {
    const bool wasOutput = (id == m_outputNodeId);
    const bool ok = m_graph.removeNode(id);
    if (ok && wasOutput) {
        m_outputNodeId = NodeGraph::k_invalidNodeId;
    }
    return ok;
}

void Asset::initEmpty() {
    m_graph.clear();
    m_outputNodeId = NodeGraph::k_invalidNodeId;
    // Arrancar con un OutputPBR ya colocado a la derecha del canvas --
    // sin esto un grafo "nuevo" es funcionalmente inutilizable hasta
    // que el dev cree el terminal manualmente.
    createNode(NodeKind::OutputPBR, {300.0f, 100.0f});
}

// =============================================================
// Serializacion JSON
// =============================================================

nlohmann::json Asset::toJson() const {
    return nlohmann::json{
        {"_version",       k_schemaVersion},
        {"output_node_id", m_outputNodeId},
        {"graph",          m_graph.toJson()},
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
        Log::engine()->warn("[ShaderGraph] fromJson: version incompatible "
                              "(esperado {}, recibido {}) -- asset vacio",
                              k_schemaVersion, ver);
        return a;
    }
    a.m_outputNodeId = j.value("output_node_id", NodeGraph::k_invalidNodeId);
    if (j.contains("graph")) {
        a.m_graph = NodeGraph::Graph::fromJson(j.at("graph"));
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
    return Asset::fromJson(j);
}

bool Asset::saveToFile(const std::filesystem::path& fsPath) const {
    std::error_code ec;
    if (fsPath.has_parent_path()) {
        std::filesystem::create_directories(fsPath.parent_path(), ec);
    }
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
