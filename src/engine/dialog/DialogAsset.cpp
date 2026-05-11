#include "engine/dialog/DialogAsset.h"

#include "core/Log.h"

#include <algorithm>
#include <fstream>

namespace Mood::Dialog {

using namespace Mood::NodeGraph;

// =============================================================
// API de lines
// =============================================================

NodeId Asset::addLine(glm::vec2 position) {
    const NodeId id = m_graph.addNode(k_typeDialogLine, position);
    if (Node* n = m_graph.findNode(id)) {
        // Sockets default: 1 input flow + 1 output flow ("continue").
        m_graph.addSocket(id, SocketKind::Input,  k_socketFlow, "in");
        m_graph.addSocket(id, SocketKind::Output, k_socketFlow, "continue");
        // customData default vacio salvo el array choices (mantiene
        // forma esperada por parseLine).
        n->customData = nlohmann::json{
            {"text_key",     ""},
            {"text_literal", ""},
            {"portrait",     ""},
            {"audio",        ""},
            {"animation",    ""},
            {"choices",      nlohmann::json::array()},
        };
    }
    // Si es el primer nodo del asset, marcarlo como start.
    if (m_metadata.start_node_id == k_invalidNodeId) {
        m_metadata.start_node_id = id;
    }
    return id;
}

Line Asset::parseLine(NodeId id) const {
    Line line;
    const Node* n = m_graph.findNode(id);
    if (!n) return line;
    if (n->typeTag != k_typeDialogLine) return line;
    const nlohmann::json& cd = n->customData;
    line.text_key     = cd.value("text_key",     std::string{});
    line.text_literal = cd.value("text_literal", std::string{});
    line.portrait     = cd.value("portrait",     std::string{});
    line.audio        = cd.value("audio",        std::string{});
    line.animation    = cd.value("animation",    std::string{});
    if (cd.contains("choices") && cd["choices"].is_array()) {
        for (const auto& cj : cd["choices"]) {
            Choice c;
            c.label_key       = cj.value("label_key",       std::string{});
            c.label_literal   = cj.value("label_literal",   std::string{});
            c.condition_lua   = cj.value("condition_lua",   std::string{});
            c.on_select_lua   = cj.value("on_select_lua",   std::string{});
            line.choices.push_back(std::move(c));
        }
    }
    return line;
}

void Asset::writeLine(NodeId id, const Line& line) {
    Node* n = m_graph.findNode(id);
    if (!n) return;
    if (n->typeTag != k_typeDialogLine) return;

    // 1) Escribir customData.
    nlohmann::json choicesArr = nlohmann::json::array();
    for (const Choice& c : line.choices) {
        choicesArr.push_back(nlohmann::json{
            {"label_key",     c.label_key},
            {"label_literal", c.label_literal},
            {"condition_lua", c.condition_lua},
            {"on_select_lua", c.on_select_lua},
        });
    }
    n->customData = nlohmann::json{
        {"text_key",     line.text_key},
        {"text_literal", line.text_literal},
        {"portrait",     line.portrait},
        {"audio",        line.audio},
        {"animation",    line.animation},
        {"choices",      std::move(choicesArr)},
    };

    // 2) Auto-sync output sockets con choices array (invariante v1).
    //    Convencion: si choices vacio -> 1 socket "continue"; si N
    //    choices -> N sockets "choice_<i>" (donde i = indice 0-based).
    //    Los links incidentes a sockets sobrantes se borran (cascade).
    const size_t target = line.choices.empty() ? 1u : line.choices.size();
    // 2a) Crear sockets faltantes.
    while (n->outputs.size() < target) {
        const size_t i = n->outputs.size();
        const std::string sockName = line.choices.empty()
            ? std::string("continue")
            : (std::string("choice_") + std::to_string(i));
        m_graph.addSocket(id, SocketKind::Output, k_socketFlow, sockName);
        // refrescar puntero n — addSocket NO invalida (m_nodes no
        // reasignado), pero defensivo.
        n = m_graph.findNode(id);
        if (!n) return;
    }
    // 2b) Borrar sockets sobrantes (desde el final hacia atras).
    while (n->outputs.size() > target) {
        const SocketId surplus = n->outputs.back().id;
        m_graph.removeSocket(surplus);
        n = m_graph.findNode(id);
        if (!n) return;
    }
    // 2c) Renombrar (los nombres pueden quedar inconsistentes tras
    //     adds/removes). Igual el HUD usa los choices array para text;
    //     el nombre del socket solo aparece en el editor.
    for (size_t i = 0; i < n->outputs.size(); ++i) {
        n->outputs[i].name = line.choices.empty()
            ? std::string("continue")
            : (std::string("choice_") + std::to_string(i));
    }
}

// =============================================================
// Serializacion
// =============================================================

nlohmann::json Asset::toJson() const {
    return nlohmann::json{
        {"_version", k_schemaVersion},
        {"graph",    m_graph.toJson()},
        {"metadata", nlohmann::json{
            {"name",              m_metadata.name},
            {"start_node_id",     m_metadata.start_node_id},
            {"default_portrait",  m_metadata.default_portrait},
            {"default_audio_bus", m_metadata.default_audio_bus},
        }},
    };
}

// =============================================================
// I/O de disco
// =============================================================

std::optional<Asset> Asset::loadFromFile(const std::filesystem::path& fsPath) {
    std::ifstream in(fsPath);
    if (!in.is_open()) {
        Log::engine()->warn("[DialogAsset] no se pudo abrir '{}'", fsPath.generic_string());
        return std::nullopt;
    }
    nlohmann::json j;
    try {
        in >> j;
    } catch (const std::exception& e) {
        Log::engine()->error("[DialogAsset] parse error en '{}': {}",
                              fsPath.generic_string(), e.what());
        return std::nullopt;
    }
    Asset a = fromJson(j);
    // Si el JSON no traia metadata.name pero el archivo si tiene
    // stem (filename sin extension), usar el stem como name. Mantiene
    // consistencia con la convencion del DialogBrowser.
    if (a.m_metadata.name.empty()) {
        a.m_metadata.name = fsPath.stem().string();
    }
    return a;
}

bool Asset::saveToFile(const std::filesystem::path& fsPath) const {
    std::error_code ec;
    std::filesystem::create_directories(fsPath.parent_path(), ec);
    // ec no-fatal — si parent_path() ya existe create_directories retorna false sin error.
    std::ofstream out(fsPath);
    if (!out.is_open()) {
        Log::engine()->error("[DialogAsset] no se pudo abrir '{}' para escritura",
                              fsPath.generic_string());
        return false;
    }
    try {
        out << toJson().dump(2);
    } catch (const std::exception& e) {
        Log::engine()->error("[DialogAsset] error al escribir '{}': {}",
                              fsPath.generic_string(), e.what());
        return false;
    }
    return true;
}

Asset Asset::fromJson(const nlohmann::json& j) {
    Asset a;
    if (!j.is_object()) {
        Log::engine()->error("[DialogAsset] fromJson: JSON no es objeto");
        return a;
    }
    const u32 version = j.value("_version", 0u);
    if (version != k_schemaVersion) {
        Log::engine()->error(
            "[DialogAsset] fromJson: schema version {} != esperado {}",
            version, k_schemaVersion);
        return a;
    }
    if (j.contains("graph") && j["graph"].is_object()) {
        a.m_graph = Graph::fromJson(j["graph"]);
    }
    if (j.contains("metadata") && j["metadata"].is_object()) {
        const auto& m = j["metadata"];
        a.m_metadata.name              = m.value("name",              std::string{});
        a.m_metadata.start_node_id     = m.value("start_node_id",     k_invalidNodeId);
        a.m_metadata.default_portrait  = m.value("default_portrait",  std::string{});
        a.m_metadata.default_audio_bus = m.value("default_audio_bus", std::string{});
    }
    return a;
}

} // namespace Mood::Dialog
