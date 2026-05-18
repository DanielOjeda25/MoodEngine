#include "engine/shader_graph/ShaderGraphGenerator.h"

#include "core/Log.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace Mood::ShaderGraph {

// =============================================================
// GenResult helpers
// =============================================================

std::vector<const GenMessage*> GenResult::messagesOf(GenSeverity sev) const {
    std::vector<const GenMessage*> out;
    for (const auto& m : messages) {
        if (m.severity == sev) out.push_back(&m);
    }
    return out;
}

// =============================================================
// Helpers internos del walk
// =============================================================

namespace {

// Estado mutable durante un walk del grafo.
struct WalkState {
    const Asset* asset = nullptr;
    const NodeGraph::Graph* graph = nullptr;

    // Cache de variables SSA ya emitidas. Map socketId -> nombre GLSL.
    std::unordered_map<NodeGraph::SocketId, std::string> pinExpr;

    // Visited set para deteccion de ciclos (DFS post-order). Si un
    // nodo aparece en `inStack`, encontramos un ciclo.
    std::unordered_set<NodeGraph::NodeId> visited;
    std::unordered_set<NodeGraph::NodeId> inStack;

    // Stream de declaraciones SSA emitidas en orden topologico
    // (output pins van escribiendo a este string).
    std::ostringstream decls;

    // Mensajes diagnosticos acumulados.
    std::vector<GenMessage> messages;

    bool hadError = false;

    void emitInfo(const std::string& msg, NodeGraph::NodeId nid = NodeGraph::k_invalidNodeId) {
        messages.push_back({GenSeverity::Info, msg, nid});
    }
    void emitWarning(const std::string& msg, NodeGraph::NodeId nid = NodeGraph::k_invalidNodeId) {
        messages.push_back({GenSeverity::Warning, msg, nid});
    }
    void emitError(const std::string& msg, NodeGraph::NodeId nid = NodeGraph::k_invalidNodeId) {
        messages.push_back({GenSeverity::Error, msg, nid});
        hadError = true;
    }
};

// Formatea un float para GLSL: siempre incluye decimal (1.0 no 1) y
// usa precision corta. Evita locale issues (en algunos locales el
// separador decimal es ',' lo cual rompe GLSL).
std::string fmtFloat(float v) {
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << std::fixed << std::setprecision(6) << v;
    std::string s = oss.str();
    // Limpiar zeros trailing pero conservar al menos un decimal.
    size_t lastNonZero = s.find_last_not_of('0');
    if (lastNonZero != std::string::npos && s[lastNonZero] == '.') {
        s.resize(lastNonZero + 2);  // "1." -> "1.0"
    } else if (lastNonZero != std::string::npos) {
        s.resize(lastNonZero + 1);
    }
    return s;
}

// Emite literal GLSL para un valor vec4 segun el typeTag del socket.
std::string literalFor(const std::string& socketType, glm::vec4 v) {
    if (socketType == kSocketType_Float) {
        return fmtFloat(v.x);
    }
    if (socketType == kSocketType_Vec2) {
        return "vec2(" + fmtFloat(v.x) + ", " + fmtFloat(v.y) + ")";
    }
    if (socketType == kSocketType_Vec3) {
        return "vec3(" + fmtFloat(v.x) + ", " + fmtFloat(v.y) + ", " + fmtFloat(v.z) + ")";
    }
    if (socketType == kSocketType_Vec4) {
        return "vec4(" + fmtFloat(v.x) + ", " + fmtFloat(v.y) + ", "
                       + fmtFloat(v.z) + ", " + fmtFloat(v.w) + ")";
    }
    return "0.0"; // fallback
}

// Valor neutro segun typeTag: util para fallback de nodos no soportados.
std::string neutralFor(const std::string& socketType) {
    if (socketType == kSocketType_Float) return "0.0";
    if (socketType == kSocketType_Vec2)  return "vec2(0.0)";
    if (socketType == kSocketType_Vec3)  return "vec3(0.0)";
    if (socketType == kSocketType_Vec4)  return "vec4(0.0)";
    return "0.0";
}

// Nombre GLSL de la variable SSA para un output pin.
std::string ssaName(NodeGraph::SocketId pinId) {
    return "v_" + std::to_string(pinId);
}

// Tipo GLSL string para un typeTag.
std::string glslTypeOf(const std::string& socketType) {
    if (socketType == kSocketType_Float) return "float";
    if (socketType == kSocketType_Vec2)  return "vec2";
    if (socketType == kSocketType_Vec3)  return "vec3";
    if (socketType == kSocketType_Vec4)  return "vec4";
    return "float";
}

// Forward declaration
std::string resolveInputExpression(WalkState& state,
                                    const NodeGraph::Node& node,
                                    const NodeGraph::Socket& inputSocket);

// Visita un nodo: emite declaraciones SSA para sus output pins. Si ya
// fue visitado, no-op (lazy). Detecta ciclos via inStack.
void visitNode(WalkState& state, NodeGraph::NodeId nodeId) {
    if (state.visited.count(nodeId)) return;
    if (state.inStack.count(nodeId)) {
        state.emitError("Ciclo detectado en el grafo (nodo " +
                        std::to_string(nodeId) + ")", nodeId);
        return;
    }
    state.inStack.insert(nodeId);

    const NodeGraph::Node* node = state.graph->findNode(nodeId);
    if (!node) {
        state.inStack.erase(nodeId);
        return;
    }

    // 1. Recursivamente visitar nodos upstream para que sus SSA vars
    //    existan antes de que las usemos.
    for (const auto& inSock : node->inputs) {
        const auto link = state.graph->findLinkByInput(inSock.id);
        if (!link.has_value()) continue;
        const auto& links = state.graph->links();
        const auto it = std::find_if(links.begin(), links.end(),
            [id = *link](const NodeGraph::Link& l) { return l.id == id; });
        if (it == links.end()) continue;
        const NodeGraph::Socket* fromSock = state.graph->findSocket(it->from);
        if (!fromSock) continue;
        visitNode(state, fromSock->ownerNode);
        if (state.hadError) {
            state.inStack.erase(nodeId);
            return;
        }
    }

    // 2. Emitir declaraciones SSA del nodo actual segun su NodeKind.
    const auto kindOpt = nodeKindFromTypeTag(node->typeTag);
    if (!kindOpt.has_value()) {
        // Nodo desconocido: emitir valores neutros para sus outputs.
        state.emitWarning("Nodo con typeTag desconocido: '" + node->typeTag + "'", nodeId);
        for (const auto& outSock : node->outputs) {
            const std::string var = ssaName(outSock.id);
            state.decls << "    " << glslTypeOf(outSock.typeTag) << " " << var
                        << " = " << neutralFor(outSock.typeTag) << ";\n";
            state.pinExpr[outSock.id] = var;
        }
        state.visited.insert(nodeId);
        state.inStack.erase(nodeId);
        return;
    }

    const NodeKind kind = *kindOpt;

    // Resolver expresiones de inputs (literal o variable SSA upstream).
    auto inExpr = [&](size_t idx) -> std::string {
        if (idx >= node->inputs.size()) return "0.0";
        return resolveInputExpression(state, *node, node->inputs[idx]);
    };

    // OutputPBR es terminal -- el caller lee state.pinExpr de sus
    // input sockets para los 5 slots. No emite declaraciones propias.
    if (kind == NodeKind::OutputPBR) {
        // Pre-cache las expresiones de los 5 inputs en pinExpr usando
        // el socket id como key.
        for (size_t i = 0; i < node->inputs.size(); ++i) {
            state.pinExpr[node->inputs[i].id] = inExpr(i);
        }
        state.visited.insert(nodeId);
        state.inStack.erase(nodeId);
        return;
    }

    // Para cada output, emitir una declaracion SSA segun el kind.
    auto emitOut = [&](size_t idx, const std::string& rhs) {
        if (idx >= node->outputs.size()) return;
        const auto& out = node->outputs[idx];
        const std::string var = ssaName(out.id);
        state.decls << "    " << glslTypeOf(out.typeTag) << " " << var
                    << " = " << rhs << ";\n";
        state.pinExpr[out.id] = var;
    };

    switch (kind) {
        case NodeKind::Color:
            emitOut(0, "vec3(" + fmtFloat(getColorValue(*node).x) + ", "
                                + fmtFloat(getColorValue(*node).y) + ", "
                                + fmtFloat(getColorValue(*node).z) + ")");
            break;
        case NodeKind::Float:
            emitOut(0, fmtFloat(getFloatValue(*node)));
            break;
        case NodeKind::UV:
            emitOut(0, "vUv");
            break;
        case NodeKind::Time:
            // uTime ya esta declarado en el pbr_graph_template.frag base
            // (lo agregamos como uniform en Bloque B si no existe).
            emitOut(0, "uTime");
            break;
        case NodeKind::SampleTexture: {
            const std::string path = getTexturePath(*node);
            if (path.empty()) {
                state.emitWarning("SampleTexture sin texture_path -- emitiendo (0,0,0,0)",
                                  nodeId);
                emitOut(0, "vec4(0.0)");
                emitOut(1, "vec3(0.0)");
                emitOut(2, "0.0");
                break;
            }
            // v1: NO soportamos texturas custom desde el grafo aun (eso
            // requiere bind dinamico de samplers desde el material -- Bloque D).
            // Por ahora emitimos sample del uAlbedoMap como placeholder y
            // logueamos warning.
            state.emitWarning("SampleTexture: bind dinamico no soportado v1 -- "
                              "usando uAlbedoMap como fallback (path='" + path + "')",
                              nodeId);
            const std::string sampleExpr = "texture(uAlbedoMap, " + inExpr(0) + ")";
            // Pre-emit la sample una vez en un temporary para no llamar
            // texture() 3 veces si los 3 outputs estan conectados.
            const std::string tmp = "smp_" + std::to_string(nodeId);
            state.decls << "    vec4 " << tmp << " = " << sampleExpr << ";\n";
            emitOut(0, tmp);
            emitOut(1, tmp + ".rgb");
            emitOut(2, tmp + ".a");
            break;
        }
        case NodeKind::Add:
            emitOut(0, "(" + inExpr(0) + " + " + inExpr(1) + ")");
            break;
        case NodeKind::Multiply:
            emitOut(0, "(" + inExpr(0) + " * " + inExpr(1) + ")");
            break;
        case NodeKind::Lerp:
            emitOut(0, "mix(" + inExpr(0) + ", " + inExpr(1) + ", " + inExpr(2) + ")");
            break;
        case NodeKind::Power:
            emitOut(0, "pow(" + inExpr(0) + ", " + inExpr(1) + ")");
            break;
        case NodeKind::OneMinus:
            emitOut(0, "(1.0 - " + inExpr(0) + ")");
            break;
        case NodeKind::Saturate:
            emitOut(0, "clamp(" + inExpr(0) + ", 0.0, 1.0)");
            break;
        case NodeKind::Clamp:
            emitOut(0, "clamp(" + inExpr(0) + ", " + inExpr(1) + ", " + inExpr(2) + ")");
            break;
        case NodeKind::Dot:
            emitOut(0, "dot(" + inExpr(0) + ", " + inExpr(1) + ")");
            break;
        case NodeKind::Cross:
            emitOut(0, "cross(" + inExpr(0) + ", " + inExpr(1) + ")");
            break;
        case NodeKind::Normalize:
            emitOut(0, "normalize(" + inExpr(0) + ")");
            break;
        case NodeKind::Length:
            emitOut(0, "length(" + inExpr(0) + ")");
            break;
        case NodeKind::Reflect:
            emitOut(0, "reflect(" + inExpr(0) + ", " + inExpr(1) + ")");
            break;
        case NodeKind::Fresnel: {
            // Fresnel-Schlick aproximation: pow(1 - NdotV, power).
            // vWorldNormal y uCameraPos - vWorldPos para V. Se asume
            // disponible en el pbr.frag base (lo cual es cierto -- esta
            // declarado en el shader).
            const std::string power = inExpr(0);
            emitOut(0, "pow(1.0 - max(dot(normalize(vWorldNormal), "
                       "normalize(uCameraPos - vWorldPos)), 0.0), " + power + ")");
            break;
        }
        case NodeKind::NormalMap: {
            // v1: emitir el vWorldNormal directo. Tangent space decode
            // requiere TBN matrix que no esta en el vert actual.
            state.emitWarning("NormalMap: tangent-space decode no soportado v1 -- "
                              "emitiendo vWorldNormal", nodeId);
            emitOut(0, "normalize(vWorldNormal)");
            break;
        }
        case NodeKind::OutputPBR:
            // Ya manejado arriba.
            break;
    }

    state.visited.insert(nodeId);
    state.inStack.erase(nodeId);
}

// Resuelve la expresion GLSL para un input socket: si esta conectado,
// usa el SSA del output upstream; si no, usa el literal del customData.
std::string resolveInputExpression(WalkState& state,
                                    const NodeGraph::Node& node,
                                    const NodeGraph::Socket& inputSocket) {
    const auto linkId = state.graph->findLinkByInput(inputSocket.id);
    if (linkId.has_value()) {
        const auto& links = state.graph->links();
        const auto it = std::find_if(links.begin(), links.end(),
            [id = *linkId](const NodeGraph::Link& l) { return l.id == id; });
        if (it != links.end()) {
            const auto ssaIt = state.pinExpr.find(it->from);
            if (ssaIt != state.pinExpr.end()) {
                return ssaIt->second;
            }
        }
    }
    // Literal fallback. El customData del nodo guarda los socket_literals
    // por input socketId.
    const glm::vec4 lit = getSocketLiteral(node, inputSocket.id);
    return literalFor(inputSocket.typeTag, lit);
}

// String replace simple (first occurrence). Util para los markers
// __SHADERGRAPH_*__ que aparecen una sola vez en la plantilla.
bool replaceOnce(std::string& haystack, const std::string& needle,
                 const std::string& replacement) {
    const size_t pos = haystack.find(needle);
    if (pos == std::string::npos) return false;
    haystack.replace(pos, needle.length(), replacement);
    return true;
}

} // namespace

// =============================================================
// generateGlsl
// =============================================================

GenResult generateGlsl(const Asset& asset, const std::string& templateSource) {
    GenResult result;

    if (asset.outputNodeId() == NodeGraph::k_invalidNodeId) {
        result.messages.push_back(
            {GenSeverity::Error, "El grafo no tiene OutputPBR.",
             NodeGraph::k_invalidNodeId});
        return result;
    }

    // Validar que la plantilla tiene los 7 marcadores esperados
    // (F2H63 sumo __SHADERGRAPH_OPACITY__).
    const std::string requiredMarkers[] = {
        "__SHADERGRAPH_DECLS__",
        "__SHADERGRAPH_ALBEDO__",
        "__SHADERGRAPH_METALLIC__",
        "__SHADERGRAPH_ROUGHNESS__",
        "__SHADERGRAPH_NORMAL__",
        "__SHADERGRAPH_EMISSIVE__",
        "__SHADERGRAPH_OPACITY__",
    };
    for (const auto& m : requiredMarkers) {
        if (templateSource.find(m) == std::string::npos) {
            result.messages.push_back(
                {GenSeverity::Error,
                 "Plantilla GLSL no contiene marker '" + std::string(m) + "'.",
                 NodeGraph::k_invalidNodeId});
            return result;
        }
    }

    // Walk topologico desde el OutputPBR.
    WalkState state;
    state.asset = &asset;
    state.graph = &asset.graph();

    visitNode(state, asset.outputNodeId());

    if (state.hadError) {
        result.messages = std::move(state.messages);
        return result;
    }

    // Extraer las 5 expresiones finales de los inputs del OutputPBR.
    // El visitNode del terminal pobla pinExpr con los socketIds de sus
    // inputs como keys (en orden: albedo, metallic, roughness, normal, emissive).
    const NodeGraph::Node* outputNode = state.graph->findNode(asset.outputNodeId());
    if (!outputNode || outputNode->inputs.size() < 5) {
        result.messages.push_back(
            {GenSeverity::Error,
             "OutputPBR malformado (esperados al menos 5 inputs -- "
             "6 con F2H63 opacity, encontrados " +
             std::to_string(outputNode ? outputNode->inputs.size() : 0) + ").",
             asset.outputNodeId()});
        result.messages.insert(result.messages.end(),
            state.messages.begin(), state.messages.end());
        return result;
    }

    auto exprFor = [&](size_t idx, const char* fallbackType) -> std::string {
        const auto it = state.pinExpr.find(outputNode->inputs[idx].id);
        if (it != state.pinExpr.end()) return it->second;
        return neutralFor(fallbackType);
    };

    const std::string albedoExpr    = exprFor(0, kSocketType_Vec3);
    const std::string metallicExpr  = exprFor(1, kSocketType_Float);
    const std::string roughnessExpr = exprFor(2, kSocketType_Float);
    const std::string normalExpr    = exprFor(3, kSocketType_Vec3);
    const std::string emissiveExpr  = exprFor(4, kSocketType_Vec3);
    // F2H63: opacity es el 6to input. Assets viejos (5 inputs) no
    // tienen slot 5 -> exprFor cae al fallback. Para back-compat
    // tratamos opacity ausente como literal 1.0 (full opaque).
    const std::string opacityExpr   = (outputNode->inputs.size() >= 6)
        ? exprFor(5, kSocketType_Float)
        : std::string("1.0");

    // Substituir markers en la plantilla.
    std::string out = templateSource;
    replaceOnce(out, "__SHADERGRAPH_DECLS__",     state.decls.str());
    replaceOnce(out, "__SHADERGRAPH_ALBEDO__",    albedoExpr);
    replaceOnce(out, "__SHADERGRAPH_METALLIC__",  metallicExpr);
    replaceOnce(out, "__SHADERGRAPH_ROUGHNESS__", roughnessExpr);
    replaceOnce(out, "__SHADERGRAPH_NORMAL__",    normalExpr);
    replaceOnce(out, "__SHADERGRAPH_EMISSIVE__",  emissiveExpr);
    replaceOnce(out, "__SHADERGRAPH_OPACITY__",   opacityExpr);

    // Safety: si quedo algun marker sin reemplazar (la plantilla tiene el
    // marker en mas de un lugar -- p.ej. en un comentario Y en codigo, y
    // replaceOnce solo agarro la primera), abortar antes de mandar GLSL
    // basura al driver. Esto cazo F2H62 polish: el template real tenia
    // los markers documentados en comments arriba, los reemplazos pisaban
    // los comments y dejaban los puntos de inyeccion intactos -> GLSL roto.
    const size_t residual = out.find("__SHADERGRAPH_");
    if (residual != std::string::npos) {
        const size_t end = out.find("__", residual + 2);
        const std::string token = (end != std::string::npos)
            ? out.substr(residual, end + 2 - residual)
            : std::string("__SHADERGRAPH_???");
        result.messages.push_back(
            {GenSeverity::Error,
             "Plantilla GLSL malformada: el marcador '" + token +
             "' aparece mas de una vez (o no fue substituido). Cada "
             "marcador debe aparecer EXACTAMENTE una vez en la plantilla.",
             NodeGraph::k_invalidNodeId});
        result.messages.insert(result.messages.end(),
            state.messages.begin(), state.messages.end());
        return result;
    }

    result.glsl = std::move(out);
    result.messages = std::move(state.messages);
    result.succeeded = true;
    return result;
}

// =============================================================
// loadDefaultTemplate
// =============================================================

std::string loadDefaultTemplate() {
    const std::filesystem::path path = "shaders/pbr_graph_template.frag";
    std::ifstream in(path);
    if (!in) {
        Log::engine()->warn("[ShaderGraph] loadDefaultTemplate: no se pudo abrir '{}'",
                              path.generic_string());
        return {};
    }
    std::ostringstream oss;
    oss << in.rdbuf();
    return oss.str();
}

} // namespace Mood::ShaderGraph
