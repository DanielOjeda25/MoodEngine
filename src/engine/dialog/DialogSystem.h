#pragma once

// DialogSystem (F2H48): state machine que recorre un DialogAsset segun
// las choices que el jugador va eligiendo. Vive como singleton namespace
// (mismo patron que `GameState`) — el motor es single-threaded, una sola
// instancia activa de un dialog a la vez es suficiente para v1.
//
// Diseno:
// - El asset apuntado NO se duplica — DialogSystem guarda un puntero
//   non-owning. El caller (AssetManager) garantiza que el asset vive
//   mientras esta activo.
// - El recorrido sigue los `Link`s del Graph: al elegir choice index i,
//   se busca el output socket [i] del nodo actual, se busca el link
//   cuyo `from` es ese socket, y se salta al `to.ownerNode`. Si no hay
//   link saliente => el dialog termina (`stop()` automatico).
// - Variables del dialog (`set_var/get_var`) viven en `GameState::dialogVars()`
//   — sobreviven entre dialogs y entran al save (decision F2H48 #2).
// - Hooks Lua (`condition_lua`/`on_select_lua` por choice; `on_node_enter`
//   / `on_choice` callbacks globales) se invocan via callbacks
//   inyectados desde LuaBindings (`setEvaluator`/`setExecutor`/
//   `setNodeEnterHook`/`setChoiceHook`). Si no estan seteados, las
//   choices se asumen siempre disponibles (default true) y los
//   on_select_lua se ignoran silencioso. Esto mantiene este modulo
//   independiente de sol2 — testeable en `mood_tests`.
//
// Locking del char controller: el flag global `GameState::dialogActive()`
// queda true mientras `isActive()`. El char controller (Editor + Player)
// chequea ese flag y desactiva input cuando esta on (decision F2H48 #1).

#include "core/Types.h"
#include "engine/dialog/DialogAsset.h"
#include "engine/nodegraph/Graph.h"

#include <functional>
#include <string>
#include <vector>

namespace Mood::Dialog {

/// @brief Resultado de evaluar las choices del nodo actual. Cada entrada
///        guarda el indice original en el array de choices del Line (para
///        mapear al output socket correcto al elegirla) + el label
///        resuelto (i18n o literal segun viene del Choice).
struct AvailableChoice {
    int         choiceIndex;  // indice en line.choices del Line struct
    std::string label;        // texto resuelto (post-i18n)
};

namespace DialogSystem {

// ----- Lifecycle -----

/// @brief Comienza desde `metadata.start_node_id` del asset dado. Si el
///        asset es null, no tiene start_node valido, o el nodo no existe
///        en el graph, no-op + log warn.
/// @return true si efectivamente arranco un dialog.
bool start(const Asset* asset);

/// @brief Avanza por la choice `choiceIndex` (referido al array de
///        line.choices del nodo actual). El sistema:
///        1) Si esta inactivo o el indice esta fuera de rango, no-op.
///        2) Ejecuta `on_select_lua` de la choice (si executor seteado).
///        3) Busca el link saliente del output socket [choiceIndex].
///        4) Si hay link, salta al nodo destino (dispara on_node_enter).
///        5) Si no hay link, hace `stop()` (fin del dialog).
/// @return true si se ejecuto la transicion, false si fue no-op.
bool advance(int choiceIndex);

/// @brief Si el nodo actual no tiene choices (continue-only), avanza
///        por el unico output socket. Equivalente a `advance(0)` en
///        ese caso. Si tiene choices, requiere `advance(idx)` explicito.
/// @return true si avanzo, false si requeria choice explicito o no
///         habia link saliente.
bool continueNext();

/// @brief Cierra el dialog actual. Idempotente.
void stop();

/// @brief Limpia el estado completo (puntero al asset + currentNode).
///        NO toca `GameState::dialogVars()` — esos persisten a proposito.
///        Llamado por `EditorApplication::exitPlayMode`.
void reset();

// ----- Queries -----

/// @brief True mientras haya un nodo activo (`currentNodeId != invalid`).
bool isActive();

/// @brief NodeId del nodo en el que estamos parados. `k_invalidNodeId`
///        si inactivo.
NodeGraph::NodeId currentNodeId();

/// @brief Asset activo (puntero non-owning). nullptr si inactivo.
const Asset* currentAsset();

/// @brief Parsea el customData del nodo actual y devuelve el Line struct.
///        Default-init si inactivo o el nodo no es dialog_line.
Line currentLine();

/// @brief Lista de choices disponibles tras aplicar `condition_lua` por
///        cada una. Si el evaluator no esta seteado, todas las choices
///        del Line se devuelven (asumidas true). Si no hay choices en
///        el Line (continue-only), devuelve vector vacio — el caller
///        usa `continueNext()` o muestra "[continuar]" en el HUD.
std::vector<AvailableChoice> availableChoices();

// ----- Hooks (LuaBindings setea estos en F2H48 Bloque E) -----

/// @brief Callback que evalua una expresion Lua que devuelve bool.
///        Usado para `condition_lua` por choice. Si no esta seteado,
///        se asume true (choice disponible). Recibe la expresion como
///        string ("" se asume true sin invocar).
using LuaEvaluator = std::function<bool(const std::string& expr)>;
void setEvaluator(LuaEvaluator fn);

/// @brief Callback que ejecuta codigo Lua sin retorno. Usado para
///        `on_select_lua` por choice. "" no se invoca.
using LuaExecutor = std::function<void(const std::string& code)>;
void setExecutor(LuaExecutor fn);

/// @brief Hook global invocado al entrar a un nodo (tanto por start
///        como por advance/continueNext). El callback recibe el NodeId.
using NodeEnterHook = std::function<void(NodeGraph::NodeId)>;
void setNodeEnterHook(NodeEnterHook fn);

/// @brief Hook global invocado al elegir una choice (antes de transicionar).
///        El callback recibe (NodeId actual, choiceIndex).
using ChoiceHook = std::function<void(NodeGraph::NodeId, int)>;
void setChoiceHook(ChoiceHook fn);

/// @brief Limpia los 4 hooks (testing).
void clearHooks();

} // namespace DialogSystem
} // namespace Mood::Dialog
