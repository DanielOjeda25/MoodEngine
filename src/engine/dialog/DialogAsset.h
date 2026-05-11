#pragma once

// DialogAsset (F2H47): asset .mooddialog — arbol de dialogo editable
// visualmente en el editor. Reusa NodeGraph::Graph como container del
// arbol; agrega metadata (start node, defaults) y typed accessors al
// `customData` JSON de cada nodo.
//
// Convencion v1: el unico tipo de nodo es `dialog_line` (typeTag).
// Tipos `condition`/`action`/`jump` quedan diferidos a v2 cuando
// emerja necesidad real — los hooks `condition_lua`/`on_select_lua`
// por choice cubren el 80% de los casos practicos.
//
// Schema JSON del archivo:
//   {
//     "_version": 1,
//     "graph": { ...output de NodeGraph::Graph::toJson()... },
//     "metadata": {
//       "name": "intro_npc_guard",
//       "start_node_id": 7,
//       "default_portrait": "characters/guard.png",
//       "default_audio_bus": "voice"
//     }
//   }
//
// customData del nodo dialog_line (segun parseLine/writeLine):
//   {
//     "text_key": "dialog.intro.line_1",   // i18n key (vacio = literal)
//     "text_literal": "",                    // fallback literal (vacio = usa key)
//     "portrait": "characters/guard.png",   // sobrescribe default
//     "audio": "audio/voice/intro_1.ogg",   // opcional
//     "animation": "talk_idle",              // opcional
//     "choices": [
//       {
//         "label_key": "dialog.intro.choice_yes",
//         "label_literal": "",
//         "condition_lua": "",
//         "on_select_lua": ""
//       }, ...
//     ]
//   }
//
// Invariante crucial (mantiene editor + validator):
//   N choices == N output sockets del nodo (orden 1-a-1 por indice).
//   Si choices.empty(): el nodo tiene 1 output socket "continue".
//
// Este modulo NO depende de ImGui — testeable en mood_tests.

#include "core/Types.h"
#include "engine/nodegraph/Graph.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Mood::Dialog {

/// @brief Una opcion del jugador dentro de una line. El campo "label"
///        se rinde como un boton clickeable en el HUD; al elegirla,
///        el dialogo sigue al siguiente nodo via el socket output
///        correspondiente (por indice en el array).
struct Choice {
    std::string label_key;       // i18n key (vacio si literal)
    std::string label_literal;   // texto inline (vacio si key)
    std::string condition_lua;   // expr Lua bool ("" = siempre disponible)
    std::string on_select_lua;   // hook Lua al elegir ("" = no-op)
};

/// @brief Una linea de dialogo parseada desde el customData del nodo.
///        El editor + runtime consumen este struct; la persistencia
///        sigue siendo el customData JSON del nodo.
struct Line {
    std::string text_key;       // i18n key del texto del NPC
    std::string text_literal;   // fallback literal
    std::string portrait;       // path opcional (sobrescribe default)
    std::string audio;          // path opcional
    std::string animation;      // animation id opcional
    std::vector<Choice> choices;
};

/// @brief typeTag constante para nodos dialog_line. Centralizado aca
///        para evitar typos en multiples sitios del codigo.
inline constexpr const char* k_typeDialogLine = "dialog_line";

/// @brief typeTag de los sockets de flow. Toda conexion en un Dialog
///        es de tipo "flow" — no hay otros tipos en v1.
inline constexpr const char* k_socketFlow = "flow";

/// @brief Extension del filesystem para los archivos de dialog.
inline constexpr const char* k_fileExtension = ".mooddialog";

/// @brief Metadata del DialogAsset (no esta en el graph; complementa).
struct Metadata {
    std::string name;              // id estable sin extension
    NodeGraph::NodeId start_node_id = NodeGraph::k_invalidNodeId;
    std::string default_portrait;  // fallback para nodos sin portrait
    std::string default_audio_bus; // bus de audio para voiceovers
};

/// @brief Asset completo: graph + metadata.
class Asset {
public:
    Asset() = default;

    /// @brief Schema version del JSON. Bumpear cuando cambia la estructura
    ///        de forma incompatible.
    static constexpr u32 k_schemaVersion = 1;

    // ----- Accesores -----

    /// @brief Graph subyacente (read-only).
    const NodeGraph::Graph& graph() const { return m_graph; }

    /// @brief Graph mutable (para que el Editor agregue/elimine nodos).
    NodeGraph::Graph& graph() { return m_graph; }

    /// @brief Metadata read-only.
    const Metadata& metadata() const { return m_metadata; }

    /// @brief Metadata mutable.
    Metadata& metadata() { return m_metadata; }

    // ----- API de lines (high-level sobre customData del nodo) -----

    /// @brief Crea un nuevo nodo dialog_line con position dada + sockets
    ///        por defecto (1 input flow + 1 output flow "continue"). Si
    ///        es el primer nodo del asset, lo marca como start_node_id.
    /// @return el NodeId del nodo creado.
    NodeGraph::NodeId addLine(glm::vec2 position);

    /// @brief Parsea el customData del nodo y retorna un Line struct.
    ///        Si el nodo no es dialog_line, los campos quedan default.
    Line parseLine(NodeGraph::NodeId id) const;

    /// @brief Escribe un Line al customData del nodo y sincroniza los
    ///        output sockets para que matcheen size con choices array
    ///        (auto-sync invariante). Si choices vacio, garantiza
    ///        exactamente 1 output socket "continue".
    void writeLine(NodeGraph::NodeId id, const Line& line);

    // ----- Serializacion -----

    /// @brief Construye JSON completo (version + graph + metadata).
    nlohmann::json toJson() const;

    /// @brief Carga desde JSON. Si version incompatible o JSON invalido,
    ///        retorna asset vacio + loggea error.
    static Asset fromJson(const nlohmann::json& j);

    // ----- I/O de disco (F2H47 editor side) -----

    /// @brief Carga un asset desde un archivo `.mooddialog`. Retorna
    ///        nullopt si el archivo no existe / no parsea / schema
    ///        version incompatible. El nombre del archivo (sin extension)
    ///        se carga como `metadata.name` si el JSON no lo trae.
    static std::optional<Asset> loadFromFile(const std::filesystem::path& fsPath);

    /// @brief Persiste el asset al filesystem path dado. Crea directorios
    ///        intermedios si no existen. Sobreescribe si ya existe.
    /// @return true si la escritura fue exitosa, false + log si fallo.
    bool saveToFile(const std::filesystem::path& fsPath) const;

private:
    NodeGraph::Graph m_graph;
    Metadata         m_metadata;
};

} // namespace Mood::Dialog
