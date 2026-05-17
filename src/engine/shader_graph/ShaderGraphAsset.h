#pragma once

// ShaderGraphAsset (F2H62 Bloque A): asset .moodshader.
//
// Disenado encima del modelo generico `Mood::NodeGraph::Graph` (F2H46),
// el mismo que usa el DialogEditor. Una unica fuente de verdad para
// grafos: reutilizamos CRUD, validacion de links, IDs monotonicos,
// serializacion + el wrapper visual NodeGraphEditor (que ya integra
// imgui-node-editor en produccion).
//
// Lo que agrega ShaderGraphAsset encima del Graph generico:
//   - Convenciones de typeTag para nodos (kNodeType_OutputPBR,
//     kNodeType_Color, etc.) que el generador GLSL reconoce.
//   - Convenciones de typeTag para sockets (kSocketType_Float,
//     kSocketType_Vec3, etc.). `Graph::canConnect` enforza strict
//     matching, asi que esto da type safety en las conexiones sin
//     extender el Graph.
//   - Factory `createNode(NodeKind, pos)` que arma el nodo con sus
//     sockets default + valores literales iniciales en customData.
//   - Helpers para leer/escribir literals del customData JSON.
//   - Tracking del nodo OutputPBR terminal (el shader debe tener
//     exactamente uno).
//   - I/O de disco con sufijo .moodshader (carga/guarda el Graph
//     toJson/fromJson dentro de un wrapper versionado).
//
// Filosofia engine-grade: el motor NO conoce "water" / "hologram"
// hardcoded. Provee primitivas matematicas + samples + un terminal
// PBR; el dev compone su look. Convencion Unity URP Shader Graph /
// Unreal Material Editor: el grafo solo define los slots PBR
// (albedo / metallic / roughness / normal / emissive), no reemplaza
// el shader entero -- el motor inyecta los slots en el pbr.frag
// existente y mantiene luces + CSM + SSAO + IBL + fog intactos.

#include "core/Types.h"
#include "engine/nodegraph/Graph.h"

#include <glm/vec4.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <optional>
#include <string>

namespace Mood::ShaderGraph {

// =============================================================
// Constantes de typeTag
// =============================================================

// typeTag de nodos. NodeKind se mapea a estos strings via
// nodeKindToTypeTag / nodeKindFromTypeTag. Convencion lowercase_snake.
inline constexpr const char* kNodeType_OutputPBR     = "output_pbr";
inline constexpr const char* kNodeType_Color         = "color";
inline constexpr const char* kNodeType_Float         = "float";
inline constexpr const char* kNodeType_UV            = "uv";
inline constexpr const char* kNodeType_Time          = "time";
inline constexpr const char* kNodeType_SampleTexture = "sample_texture";
inline constexpr const char* kNodeType_Add           = "add";
inline constexpr const char* kNodeType_Multiply      = "multiply";
inline constexpr const char* kNodeType_Lerp          = "lerp";
inline constexpr const char* kNodeType_Power         = "power";
inline constexpr const char* kNodeType_OneMinus      = "one_minus";
inline constexpr const char* kNodeType_Saturate      = "saturate";
inline constexpr const char* kNodeType_Clamp         = "clamp";
inline constexpr const char* kNodeType_Dot           = "dot";
inline constexpr const char* kNodeType_Cross         = "cross";
inline constexpr const char* kNodeType_Normalize     = "normalize";
inline constexpr const char* kNodeType_Length        = "length";
inline constexpr const char* kNodeType_Reflect       = "reflect";
inline constexpr const char* kNodeType_Fresnel       = "fresnel";
inline constexpr const char* kNodeType_NormalMap     = "normal_map";

// typeTag de sockets. `Graph::canConnect` exige strict matching, asi
// que conectar "float" -> "vec3" se rechaza. El dev necesita un nodo
// generator del tipo correcto. Esto evita casts implicitos sorpresa.
inline constexpr const char* kSocketType_Float     = "float";
inline constexpr const char* kSocketType_Vec2      = "vec2";
inline constexpr const char* kSocketType_Vec3      = "vec3";
inline constexpr const char* kSocketType_Vec4      = "vec4";

/// @brief Enum tipado para crear nodos. Se mapea a typeTag via
///        nodeKindToTypeTag. Existe para que el editor visual /
///        palette puedan iterarlo, y para que el generador GLSL
///        switchee sobre tipos C++ sin string comparison hot path.
enum class NodeKind : u32 {
    OutputPBR     = 0,
    Color         = 10,
    Float         = 11,
    UV            = 12,
    Time          = 13,
    SampleTexture = 14,
    Add           = 20,
    Multiply      = 21,
    Lerp          = 22,
    Power         = 23,
    OneMinus      = 24,
    Saturate      = 25,
    Clamp         = 26,
    Dot           = 30,
    Cross         = 31,
    Normalize     = 32,
    Length        = 33,
    Reflect       = 34,
    Fresnel       = 40,
    NormalMap     = 41,
};

/// @brief Lista completa de NodeKinds disponibles. Util para la
///        palette del editor visual + para el spawn de nodos.
const std::vector<NodeKind>& allNodeKinds();

/// @brief NodeKind -> typeTag string. Si el kind no esta mapeado,
///        retorna kNodeType_Color como fallback defensivo.
const char* nodeKindToTypeTag(NodeKind kind);

/// @brief typeTag -> NodeKind. Si el string no matchea ningun kind
///        conocido, retorna std::nullopt (no asumimos un fallback
///        porque el grafo podria contener un nodo con typeTag de
///        un asset de version futura).
std::optional<NodeKind> nodeKindFromTypeTag(const std::string& tag);

/// @brief Label legible para la palette del editor visual.
const char* nodeKindDisplayName(NodeKind kind);

// =============================================================
// Conventions de customData
// =============================================================

// Keys del customData JSON de un nodo. Cada nodo puede usar las que
// tengan sentido segun su NodeKind:
//   - Color:         "value": [r, g, b, a]   (vec4 literal)
//   - Float:         "value": [x, 0, 0, 0]   (float literal en x)
//   - SampleTexture: "texture_path": "..."
//   - Lerp/Add/Multiply: no usa customData (inputs literals viven
//     en el customData del socket via setSocketLiteralVec4).
//
// Para los LITERALS de un input socket desconectado (e.g. el "albedo"
// del OutputPBR sin link entrante), usamos customData.socket_literals
// del nodo, con clave = socketId stringified, valor = [x, y, z, w].
// Razon: el Graph::Socket no tiene campo de literal, pero customData
// del Node si. Pattern aceptable.

/// @brief Lee el literal vec4 de un input socket desconectado.
///        Retorna {0,0,0,0} si no esta seteado.
glm::vec4 getSocketLiteral(const NodeGraph::Node& node, NodeGraph::SocketId socketId);

/// @brief Setea el literal de un input socket. Si el value es {0,0,0,0}
///        remueve la entry (compactar JSON).
void setSocketLiteral(NodeGraph::Node& node, NodeGraph::SocketId socketId, glm::vec4 value);

/// @brief Lee/escribe el texturePath de un SampleTexture node.
std::string getTexturePath(const NodeGraph::Node& node);
void        setTexturePath(NodeGraph::Node& node, std::string path);

/// @brief Lee/escribe el value de un Color node (vec3 emite como vec4).
glm::vec4 getColorValue(const NodeGraph::Node& node);
void      setColorValue(NodeGraph::Node& node, glm::vec4 value);

/// @brief Lee/escribe el value de un Float node.
float getFloatValue(const NodeGraph::Node& node);
void  setFloatValue(NodeGraph::Node& node, float value);

// =============================================================
// Asset envoltorio del Graph
// =============================================================

inline constexpr const char* k_fileExtension = ".moodshader";

/// @brief Asset declarativo del shader graph. Envoltorio delgado sobre
///        Mood::NodeGraph::Graph + tracking del OutputPBR terminal +
///        I/O de disco con version del schema.
class Asset {
public:
    Asset() = default;

    /// @brief Schema version del JSON serializado. Bumpear cuando se
    ///        cambia la estructura de forma incompatible.
    static constexpr u32 k_schemaVersion = 1;

    // ----- Acceso al grafo subyacente -----

    NodeGraph::Graph&       graph()       { return m_graph; }
    const NodeGraph::Graph& graph() const { return m_graph; }

    /// @brief ID del nodo OutputPBR terminal. k_invalidNodeId si no hay
    ///        ninguno. Solo deberia haber uno por grafo -- la factory
    ///        createNode lo updatea cuando se crea un OutputPBR.
    NodeGraph::NodeId outputNodeId() const { return m_outputNodeId; }

    // ----- Factory de nodos -----

    /// @brief Crea un nodo del kind dado con sus sockets default y
    ///        customData inicial. Si kind == OutputPBR, actualiza
    ///        m_outputNodeId al id resultante (reemplazando cualquier
    ///        previo -- aunque idealmente solo deberia haber un OutputPBR
    ///        por grafo, no enforzamos al nivel de CRUD).
    /// @return id del nodo creado.
    NodeGraph::NodeId createNode(NodeKind kind, glm::vec2 position);

    /// @brief Borra un nodo del grafo. Si era el OutputPBR terminal,
    ///        resetea m_outputNodeId a k_invalidNodeId.
    bool removeNode(NodeGraph::NodeId id);

    /// @brief Resetea el asset a "grafo vacio con un solo OutputPBR
    ///        en pos (300, 100)". Util para "New Shader" en el editor.
    void initEmpty();

    // ----- Serializacion -----

    /// @brief Construye JSON completo (version + output_node_id +
    ///        graph). El graph se serializa con `Graph::toJson`.
    nlohmann::json toJson() const;

    /// @brief Carga desde JSON. Si version incompatible o JSON invalido,
    ///        retorna asset vacio + loggea error.
    static Asset fromJson(const nlohmann::json& j);

    // ----- I/O de disco -----

    /// @brief Carga un asset desde un archivo `.moodshader`. nullopt si
    ///        el archivo no existe / JSON invalido / schema incompatible.
    static std::optional<Asset> loadFromFile(const std::filesystem::path& fsPath);

    /// @brief Persiste el asset al filesystem path dado. Crea directorios
    ///        intermedios si no existen. Sobreescribe si ya existe.
    /// @return true si la escritura fue exitosa, false + log si fallo.
    bool saveToFile(const std::filesystem::path& fsPath) const;

private:
    NodeGraph::Graph  m_graph;
    NodeGraph::NodeId m_outputNodeId = NodeGraph::k_invalidNodeId;
};

} // namespace Mood::ShaderGraph
