#pragma once

// DialogValidator (F2H47): chequea integridad de un DialogAsset.
// Pure utility — no muta el asset, solo retorna issues. El editor
// muestra los issues en un panel; el dev decide si guardar o no.
//
// Convencion v1: Error = problema que rompe runtime (start_node huerfano,
// dialog_line sin output socket). Warning = problema potencial pero no
// fatal (cycle detectado — algunos juegos los usan controladamente con
// flags Lua).

#include "core/Types.h"
#include "engine/dialog/DialogAsset.h"
#include "engine/nodegraph/Graph.h"

#include <string>
#include <vector>

namespace Mood::Dialog {

struct ValidationIssue {
    enum class Severity : u8 {
        Error   = 0,
        Warning = 1,
    };
    Severity severity = Severity::Error;
    NodeGraph::NodeId nodeId = NodeGraph::k_invalidNodeId;  // 0 = issue del asset entero (no nodo)
    std::string message;  // texto legible (en ingles, i18n al render)
};

/// @brief Corre todas las validaciones sobre el asset. Retorna lista
///        de issues — vacia si todo OK.
std::vector<ValidationIssue> validate(const Asset& asset);

/// @brief Reglas individuales (utiles para tests granulares).
namespace rules {

/// @brief start_node_id no es k_invalidNodeId y referencia un nodo
///        existente.
std::vector<ValidationIssue> checkStartNode(const Asset& asset);

/// @brief Cada nodo dialog_line tiene exactamente 1 input socket flow.
std::vector<ValidationIssue> checkInputSockets(const Asset& asset);

/// @brief Cada nodo dialog_line tiene >= 1 output socket flow.
///        Sin outputs => dialogo queda atascado en ese nodo.
std::vector<ValidationIssue> checkOutputSockets(const Asset& asset);

/// @brief El array choices del customData tiene mismo size que los
///        output sockets del nodo (invariante de auto-sync).
std::vector<ValidationIssue> checkChoicesSync(const Asset& asset);

/// @brief Detecta ciclos via DFS. v1 los reporta como WARNING (algunos
///        juegos los usan con flags Lua para escapar).
std::vector<ValidationIssue> checkCycles(const Asset& asset);

/// @brief Nodos huerfanos: dialog_lines (excepto el start) sin ningun
///        link entrante al input socket. WARNING (puede ser nodo en
///        construccion).
std::vector<ValidationIssue> checkOrphans(const Asset& asset);

} // namespace rules

} // namespace Mood::Dialog
