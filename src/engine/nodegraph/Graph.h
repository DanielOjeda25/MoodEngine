#pragma once

// NodeGraph (F2H46): modelo de datos puro de un grafo de nodos
// editable. Base reutilizable de Dialog Editor (F2H47+), Quest Editor
// (F2H4X+) y eventualmente Material Editor pro version.
//
// Este header NO depende de ImGui ni de `imgui-node-editor`. La capa
// de rendering vive en `NodeGraphEditor.h/cpp`. Razon: serializacion,
// CRUD y tests viven en el data model; ImGui solo es vista. Mismo
// patron que F2H39 `GameState` ↔ `GameOverlay`.
//
// Convenciones:
// - IDs (NodeId / SocketId / LinkId) son `u32` monotonos por grafo.
//   No reusar tras delete. 0 = invalido / sentinel.
// - `typeTag` en Node y Socket es free-form string que los editores
//   especificos (Dialog/Quest) interpretan. v1 connection rule:
//   sockets con MISMO `typeTag` conectan; tipos distintos rechazan
//   el link.
// - `customData` en Node es payload JSON que el editor especifico
//   serializa/deserializa (ej. el texto de un dialog line, el min_count
//   de un quest objective). Mantener el grafo agnostico al gameplay.
// - Serializacion: `Graph::toJson()` / `Graph::fromJson()` con campo
//   `_version` para migrations futuras.

#include "core/Types.h"

#include <glm/vec2.hpp>
#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <vector>

namespace Mood::NodeGraph {

using NodeId   = u32;
using SocketId = u32;
using LinkId   = u32;

constexpr NodeId   k_invalidNodeId   = 0;
constexpr SocketId k_invalidSocketId = 0;
constexpr LinkId   k_invalidLinkId   = 0;

enum class SocketKind : u8 {
    Input  = 0,
    Output = 1,
};

struct Socket {
    SocketId    id        = k_invalidSocketId;
    NodeId      ownerNode = k_invalidNodeId;
    SocketKind  kind      = SocketKind::Input;
    std::string typeTag;   // taxonomia definida por el editor especifico
    std::string name;      // i18n key opcional (vacio = no label visible)
};

struct Node {
    NodeId             id = k_invalidNodeId;
    std::string        typeTag;   // ej. "dialog_line", "quest_objective_item_count"
    std::string        title;     // i18n key opcional
    glm::vec2          position{0.0f, 0.0f};
    std::vector<Socket> inputs;
    std::vector<Socket> outputs;
    nlohmann::json     customData = nlohmann::json::object();
};

struct Link {
    LinkId   id   = k_invalidLinkId;
    SocketId from = k_invalidSocketId;  // output socket
    SocketId to   = k_invalidSocketId;  // input socket
};

/// @brief Modelo central. Contiene nodos + links + generadores de IDs
///        monotonos. Toda mutacion pasa por la API de la clase para
///        garantizar IDs validos y consistencia de invariantes.
class Graph {
public:
    Graph() = default;

    // ----- CRUD de nodos -----

    /// @brief Crea un nodo con typeTag dado en position. ID auto-asignado.
    /// @return ID del nodo creado.
    NodeId addNode(std::string typeTag, glm::vec2 position);

    /// @brief Borra el nodo + todos sus sockets + todos los links incidentes.
    ///        No-op si el nodo no existe.
    /// @return true si borro algo, false si node id no existia.
    bool removeNode(NodeId id);

    /// @brief Acceso const. nullptr si no existe.
    const Node* findNode(NodeId id) const;

    /// @brief Acceso mutable (para mover, editar customData, etc).
    ///        nullptr si no existe.
    Node* findNode(NodeId id);

    /// @brief Vista completa (read-only) de los nodos. Util para
    ///        rendering y queries.
    const std::vector<Node>& nodes() const { return m_nodes; }

    // ----- CRUD de sockets -----

    /// @brief Agrega socket al nodo. Si el nodo no existe, retorna
    ///        k_invalidSocketId.
    /// @return ID del socket creado, o k_invalidSocketId si falla.
    SocketId addSocket(NodeId node,
                       SocketKind kind,
                       std::string typeTag,
                       std::string name = "");

    /// @brief Busqueda O(n) por ID. nullptr si no existe.
    const Socket* findSocket(SocketId id) const;

    // ----- CRUD de links -----

    /// @brief Conecta `from` (output) a `to` (input). Reglas v1:
    ///        1) ambos sockets deben existir.
    ///        2) from.kind == Output, to.kind == Input.
    ///        3) from.typeTag == to.typeTag (strict matching).
    ///        4) no se permite duplicar un link existente (mismo from + to).
    ///        5) un mismo input socket solo acepta UN link entrante
    ///           (los outputs pueden tener N).
    /// @return ID del link creado o k_invalidLinkId si alguna regla falla.
    LinkId addLink(SocketId from, SocketId to);

    /// @brief Borra el link por ID. No-op si no existe.
    bool removeLink(LinkId id);

    /// @brief Vista completa (read-only) de los links.
    const std::vector<Link>& links() const { return m_links; }

    /// @brief Busca el link cuyo input socket es `inputSocket`. Util
    ///        porque inputs son unicos (regla 5). nullopt si no hay.
    std::optional<LinkId> findLinkByInput(SocketId inputSocket) const;

    // ----- Validacion / queries -----

    /// @brief Verifica si dos sockets son conectables segun las 5 reglas
    ///        de addLink. NO crea el link — solo informa.
    bool canConnect(SocketId from, SocketId to) const;

    /// @brief Cuenta nodos.
    size_t nodeCount() const { return m_nodes.size(); }
    /// @brief Cuenta links.
    size_t linkCount() const { return m_links.size(); }

    /// @brief Limpia todo el grafo (resetea IDs tambien). Util para
    ///        "New Graph" en el editor.
    void clear();

    // ----- Serializacion -----

    /// @brief Schema version del JSON serializado. Bumpear cuando se
    ///        cambia la estructura del JSON de manera incompatible.
    static constexpr u32 k_schemaVersion = 1;

    /// @brief Convierte el grafo entero a JSON. Schema versionado.
    nlohmann::json toJson() const;

    /// @brief Construye un grafo desde JSON. Si el JSON es invalido o
    ///        version incompatible, retorna grafo vacio + loggea error.
    static Graph fromJson(const nlohmann::json& j);

private:
    std::vector<Node> m_nodes;
    std::vector<Link> m_links;
    NodeId   m_nextNodeId   = 1;
    SocketId m_nextSocketId = 1;
    LinkId   m_nextLinkId   = 1;
};

} // namespace Mood::NodeGraph
