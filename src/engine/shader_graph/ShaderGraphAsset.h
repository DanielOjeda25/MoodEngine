#pragma once

// ShaderGraphAsset (F2H62 Bloque A): asset .moodshader -- definicion
// declarativa de un shader visual compuesto por nodos + edges. Lo consume
// el ShaderGraphEditor (autoria) y el generador GLSL (Bloque B) que lo
// convierte en un fragment shader linkeable. El motor lo trata como
// alternativa al pbr.frag estandar cuando un MaterialAsset apunta a un
// shaderGraphPath (Bloque D).
//
// Filosofia engine-grade (regla VISION): el motor NO conoce "water" /
// "hologram" / "dissolve" como tipos de shader hardcoded. El editor
// provee primitivas matematicas y de sample y el dev compone su look.
//
// Set de nodos v1 (Bloques A-G):
//   - Generators: Color, Float, UV, Time, SampleTexture
//   - Math: Add, Multiply, Lerp, Power, OneMinus, Saturate, Clamp
//   - Vector: Dot, Cross, Normalize, Length, Reflect
//   - PBR-aware: Fresnel, NormalMap (decode tangent space)
//   - Terminal: OutputPBR (5 inputs: albedo, metallic, roughness, normal, emissive)
//
// Schema JSON del archivo:
//   {
//     "_version": 1,
//     "next_node_id": 7,
//     "next_pin_id":  21,
//     "output_node_id": 1,
//     "nodes": [
//       { "id": 1, "type": "output_pbr", "pos": [400, 100], "inputs":[...], "outputs":[] },
//       { "id": 2, "type": "color",      "pos": [100, 50],  "inputs":[],
//         "outputs":[{"id":5, "type":"vec3", "name":"out"}], "color":[1,0.5,0.2] },
//       ...
//     ],
//     "edges": [
//       { "from_node": 2, "from_pin": 5, "to_node": 1, "to_pin": 10 }
//     ]
//   }
//
// Este modulo NO depende de ImGui ni OpenGL -- testeable en mood_tests.

#include "core/Types.h"

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Mood::ShaderGraph {

/// @brief Tipo de un nodo. La identidad determina como el generador GLSL
///        emite el codigo + cuantos input/output pins tiene.
enum class NodeType : u32 {
    // Terminal (uno por grafo). Sus inputs alimentan los outputs del
    // fragment shader generado.
    OutputPBR     = 0,

    // Generators (sin inputs).
    Color         = 10,
    Float         = 11,
    UV            = 12,   // emite vUv del vert
    Time          = 13,   // emite uTime uniform
    SampleTexture = 14,   // 1 input UV, 1 output vec4. texturePath en parametros.

    // Math escalar/vec.
    Add           = 20,
    Multiply      = 21,
    Lerp          = 22,
    Power         = 23,
    OneMinus      = 24,
    Saturate      = 25,
    Clamp         = 26,

    // Vector ops.
    Dot           = 30,
    Cross         = 31,
    Normalize     = 32,
    Length        = 33,
    Reflect       = 34,

    // PBR-aware.
    Fresnel       = 40,   // emite pow(1 - NdotV, power)
    NormalMap     = 41,   // sample + decode tangent-space normal
};

/// @brief Tipo del valor que circula por un pin. El generador GLSL inserta
///        casts implicitos donde aplica (float -> vec3 = vec3(x), etc.).
enum class PinType : u8 {
    Float    = 0,
    Vec2     = 1,
    Vec3     = 2,
    Vec4     = 3,
    Texture2D = 4,  // solo usado en SampleTexture node (no se conecta a otros)
};

/// @brief Un pin de input o output. Los IDs son unicos dentro del grafo
///        (no per-nodo) -- simplifica las edges.
struct Pin {
    u32 id = 0;
    PinType type = PinType::Float;
    std::string name;
    /// Valor literal cuando el pin de input no esta conectado a nada.
    /// Solo float/vec2/vec3/vec4 lo usan; Texture2D no tiene literal.
    glm::vec4 literalValue{0.0f};
};

/// @brief Un nodo del grafo. Los inputs vienen del orden declarado en
///        el .cpp (NodeFactory) y matchean por indice con los outputs
///        de los nodos conectados.
struct Node {
    u32 id = 0;
    NodeType type = NodeType::Color;
    /// Posicion en el editor visual (UX, no afecta el shader generado).
    glm::vec2 editorPos{0.0f};

    std::vector<Pin> inputs;
    std::vector<Pin> outputs;

    // Parametros internos type-specific (no relevantes se ignoran):
    std::string texturePath;   // SampleTexture: path logico al asset.
};

/// @brief Conexion entre el output de un nodo y el input de otro.
struct Edge {
    u32 fromNodeId = 0;
    u32 fromPinId  = 0;
    u32 toNodeId   = 0;
    u32 toPinId    = 0;
};

/// @brief Extension del filesystem para archivos de shader graph.
inline constexpr const char* k_fileExtension = ".moodshader";

/// @brief Asset declarativo del shader graph -- datos puros, sin runtime.
class Asset {
public:
    Asset() = default;

    /// @brief Schema version del JSON. Bumpear cuando cambia la estructura
    ///        de forma incompatible.
    static constexpr u32 k_schemaVersion = 1;

    // ----- Campos (acceso directo -- struct-like) -----

    std::vector<Node> nodes;
    std::vector<Edge> edges;
    /// ID del nodo OutputPBR terminal (uno por grafo).
    u32 outputNodeId = 0;
    /// Monotonic counters para asignar nuevos ids sin colision.
    u32 nextNodeId = 1;
    u32 nextPinId  = 1;

    // ----- Helpers de construccion -----

    /// @brief Crea un nodo de tipo `type` con los pins por default segun
    ///        la NodeFactory (declarada en .cpp). Asigna ids monotonicos.
    ///        Retorna referencia al nodo recien creado.
    Node& addNode(NodeType type, glm::vec2 editorPos = {0.0f, 0.0f});

    /// @brief Conecta un output pin con un input pin. NO valida tipos
    ///        (eso queda para el generador GLSL). Si ya existe edge al
    ///        mismo toPin, lo reemplaza (un input solo acepta una fuente).
    void connect(u32 fromNodeId, u32 fromPinId, u32 toNodeId, u32 toPinId);

    /// @brief Remueve la edge que termina en `toPinId` (si existe).
    void disconnect(u32 toPinId);

    /// @brief Busca un nodo por id. nullptr si no existe.
    Node* findNode(u32 nodeId);
    const Node* findNode(u32 nodeId) const;

    /// @brief Busca un pin por id (busca en todos los nodos). Retorna
    ///        tupla (nodo, pin) o (nullptr, nullptr).
    std::pair<Node*, Pin*> findPin(u32 pinId);
    std::pair<const Node*, const Pin*> findPin(u32 pinId) const;

    // ----- Serializacion -----

    /// @brief Construye JSON completo (version + nodos + edges).
    nlohmann::json toJson() const;

    /// @brief Carga desde JSON. Si version incompatible o JSON invalido,
    ///        retorna asset vacio + loggea error.
    static Asset fromJson(const nlohmann::json& j);

    // ----- I/O de disco -----

    /// @brief Carga un asset desde un archivo `.moodshader`. Retorna nullopt
    ///        si el archivo no existe / no parsea / schema version
    ///        incompatible.
    static std::optional<Asset> loadFromFile(const std::filesystem::path& fsPath);

    /// @brief Persiste el asset al filesystem path dado. Crea directorios
    ///        intermedios si no existen. Sobreescribe si ya existe.
    /// @return true si la escritura fue exitosa, false + log si fallo.
    bool saveToFile(const std::filesystem::path& fsPath) const;
};

// ----- Helpers de serializacion de enums -----

/// @brief NodeType -> string para JSON (legible y estable a reorden).
const char* nodeTypeToString(NodeType t);

/// @brief string -> NodeType. Unknown / vacio -> Color (safe default
///        -- emite vec3(0) y no rompe el grafo).
NodeType nodeTypeFromString(const std::string& s);

/// @brief PinType -> string.
const char* pinTypeToString(PinType t);

/// @brief string -> PinType. Default + unknown -> Float.
PinType pinTypeFromString(const std::string& s);

/// @brief Crea un nodo de tipo `type` poblando los pins default segun
///        la factory interna. Asigna ids monotonicos usando el counter
///        del asset. Funcion estandalone para que tests + serializer la
///        usen sin tener que instanciar un Asset.
Node makeNodeWithDefaultPins(NodeType type, u32& nextNodeId, u32& nextPinId);

} // namespace Mood::ShaderGraph
