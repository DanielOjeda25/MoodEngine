#pragma once

// ShaderGraphGenerator (F2H62 Bloque B): convierte un ShaderGraph::Asset
// en GLSL fragment shader linkeable.
//
// Approach: plantilla + walk topologico. La plantilla
// `shaders/pbr_graph_template.frag` es una copia disciplinada de
// `pbr.frag` con 6 marcadores (DECLS / ALBEDO / METALLIC / ROUGHNESS
// / NORMAL / EMISSIVE) reemplazables. El generador walkea el grafo
// desde el OutputPBR hacia atras emitiendo declaraciones SSA
// (`vec3 v_<pinId> = ...;`) por cada output pin alcanzable, y
// expresiones finales para cada uno de los 5 slots del PBR.
//
// Strict typing: las conexiones del grafo son strict matching de
// typeTag (regla 3 de Graph::canConnect). El generador NO emite casts
// implicitos -- si un slot del OutputPBR no tiene una expresion del
// tipo correcto disponible, usa el literal del customData como
// fallback.
//
// Deteccion de ciclos: DFS con visited+stack. Si detecta back-edge,
// reporta error y aborta la generacion (caller hace fallback al
// pbr.frag estandar).
//
// El generador NO depende de OpenGL ni ImGui -- toma un Asset + el
// source de la plantilla como std::string. Testeable headless.

#include "core/Types.h"
#include "engine/shader_graph/ShaderGraphAsset.h"

#include <string>
#include <vector>

namespace Mood::ShaderGraph {

/// @brief Severity de un mensaje del generador.
enum class GenSeverity : u8 {
    Info    = 0,
    Warning = 1,
    Error   = 2,
};

/// @brief Mensaje diagnostico emitido durante la generacion. El editor
///        visual lo puede mostrar en un panel "Output" o similar.
struct GenMessage {
    GenSeverity severity = GenSeverity::Info;
    std::string text;
    /// Si el mensaje refiere a un nodo especifico, este es su id.
    /// k_invalidNodeId si es global.
    NodeGraph::NodeId nodeId = NodeGraph::k_invalidNodeId;
};

/// @brief Resultado de generateGlsl: GLSL emitido + diagnosticos.
struct GenResult {
    /// GLSL final del fragment shader (vacio si succeeded == false).
    std::string glsl;

    /// Mensajes diagnosticos acumulados. Pueden haber warnings aunque
    /// succeeded == true (ej. nodos no soportados que se reemplazaron
    /// por valores neutros).
    std::vector<GenMessage> messages;

    /// true si el GLSL es compilable. false si hubo error fatal
    /// (ciclo, OutputPBR ausente, plantilla invalida).
    bool succeeded = false;

    /// @brief Helper: filtra solo los mensajes de severity dada.
    std::vector<const GenMessage*> messagesOf(GenSeverity sev) const;
};

/// @brief Genera el fragment shader GLSL para el asset usando la
///        plantilla `templateSource`. La plantilla debe contener los
///        6 markers __SHADERGRAPH_*__.
///
///        Si el asset NO tiene OutputPBR (outputNodeId == k_invalidNodeId),
///        retorna result.succeeded = false con un GenMessage error.
///
///        Si la plantilla no contiene alguno de los markers requeridos,
///        idem (error).
///
///        Si hay ciclo en el grafo, error.
///
///        Nodos con typeTag desconocido emiten warning + se reemplazan
///        por un valor neutro (vec3(0) o 0.0) -- no aborta la generacion.
GenResult generateGlsl(const Asset& asset, const std::string& templateSource);

/// @brief Carga la plantilla por defecto (`shaders/pbr_graph_template.frag`)
///        desde disco. Helper para que el caller no tenga que hacer la
///        I/O. Retorna string vacio + log si falla la lectura.
std::string loadDefaultTemplate();

} // namespace Mood::ShaderGraph
