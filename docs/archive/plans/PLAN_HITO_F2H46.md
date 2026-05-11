# PLAN HITO F2H46 — Node-graph framework (integración `imgui-node-editor`)

> **Estado**: DRAFT pendiente aprobación del dev (2026-05-10).
> **Tag previsto**: `v1.36.0-fase2-hito46`.
> **Origen**: primer hito real de Sub-fase 2.5 (Bloque 0.1 del plan
> [`PLAN_SUBFASE_2_5.md`](PLAN_SUBFASE_2_5.md)). Pre-requisito de los
> editores visuales de Dialog (Bloque 2) y Quest (Bloque 3).
> **Decisión técnica ya tomada con dev**: usar la lib
> [`imgui-node-editor`](https://github.com/thedmd/imgui-node-editor) de
> thedmd en lugar de framework propio. Pros: ~3 hitos menos esfuerzo,
> battle-tested (Unreal-like tools), header-only-ish. Caveat aceptado:
> si emerge que la lib no permite algo crítico, forkear / reescribir
> entonces.

## Filosofía

Este hito **integra** un framework de node-graph editor reutilizable
al motor. No implementa Dialog Editor ni Quest Editor — solo la infra
que esos editores consumen. Validación visual = un panel "Node Graph
Sandbox" (hideable via debug flag) que demuestra que pan/zoom/sockets/
conexiones/save-load funcionan.

**Separación dura state vs rendering** (patrón F2H39 GameState ↔
GameOverlay):

- `engine/data/nodegraph/` → modelo de datos puro (nodos, sockets,
  conexiones por ID). NO depende de ImGui ni de la lib externa.
  Testeable en `mood_tests`.
- `engine/ui/nodegraph/` → wrapper sobre `imgui-node-editor` que
  consume el modelo de datos y lo renderea + maneja edición visual.
  Depende de ImGui + la lib.

Razón: Dialog/Quest serializan grafos a JSON. La serialización no
debería depender de ImGui. Tests unitarios del data model no necesitan
ImGui linkeado.

## Scope (decisiones confirmadas con dev)

### Sub-bloques

#### 1. Integración de la lib (`imgui-node-editor`)

- Agregar como subdirectorio en `third_party/imgui-node-editor/`
  (git submodule o copia directa — decidir al arrancar Bloque B según
  cómo prefiere el dev manejar third_party).
- CMake target nuevo `imgui_node_editor` linkeado a ImGui.
- Build verification: integra como dependencia de `mood_engine_lib`.

#### 2. Modelo de datos puro (`engine/data/nodegraph/`)

```cpp
namespace Mood::NodeGraph {

using NodeId   = u32;
using SocketId = u32;
using LinkId   = u32;

enum class SocketKind { Input, Output };

struct Socket {
    SocketId   id;
    NodeId     ownerNode;
    SocketKind kind;
    std::string typeTag;  // e.g. "dialog_flow", "quest_objective" — los
                          // editores específicos definen qué tipos son
                          // compatibles. v1: cualquier tipo conecta con
                          // su mismo nombre.
    std::string name;     // i18n key opcional
};

struct Node {
    NodeId      id;
    std::string typeTag;  // e.g. "dialog_line", "quest_objective_item_count"
    std::string title;    // i18n key
    glm::vec2   position;
    std::vector<Socket> inputs;
    std::vector<Socket> outputs;
    nlohmann::json customData;  // payload específico del tipo de nodo;
                                // los editores específicos lo interpretan
};

struct Link {
    LinkId   id;
    SocketId from;  // output socket
    SocketId to;    // input socket
};

struct Graph {
    std::vector<Node> nodes;
    std::vector<Link> links;
    NodeId   nextNodeId   = 1;
    SocketId nextSocketId = 1;
    LinkId   nextLinkId   = 1;

    // API CRUD
    NodeId   addNode(std::string typeTag, glm::vec2 pos);
    void     removeNode(NodeId id);
    SocketId addSocket(NodeId node, SocketKind kind, std::string typeTag, std::string name);
    LinkId   addLink(SocketId from, SocketId to);
    void     removeLink(LinkId id);

    // Serialización
    nlohmann::json toJson() const;
    static Graph fromJson(const nlohmann::json& j);
};

} // namespace Mood::NodeGraph
```

#### 3. Wrapper visual (`engine/ui/nodegraph/`)

```cpp
namespace Mood::NodeGraphUI {

/// @brief Maneja el contexto de imgui-node-editor + drawing del grafo.
///        Una instancia por panel del editor (Dialog Editor, Quest
///        Editor, etc).
class NodeGraphEditor {
public:
    NodeGraphEditor();
    ~NodeGraphEditor();

    /// Renderea el grafo en el ImGui window actual. Gestiona pan/zoom/
    /// drag/conexiones internamente via imgui-node-editor.
    void draw(NodeGraph::Graph& graph);

    /// Selección actual (para Inspector contextual de propiedades del
    /// nodo seleccionado).
    std::vector<NodeGraph::NodeId> selectedNodes() const;

private:
    struct Impl;  // pImpl para no exponer headers de imgui-node-editor
    std::unique_ptr<Impl> m_impl;
};

} // namespace Mood::NodeGraphUI
```

**pImpl** para que clientes del header NO necesiten incluir
`imgui-node-editor.h` — evita contaminar autocompletado de todo
proyecto.

#### 4. Panel Sandbox para validación (`editor/panels/debug/NodeGraphSandboxPanel`)

Panel debug-only (toggle desde menú "Debug" del editor) que:

- Muestra un grafo demo con 5-6 nodos pre-cargados de tipos variados
  (input nodes, transform nodes, output nodes — solo para visualizar).
- Permite agregar nodos via right-click menu.
- Permite conectar / desconectar / mover / borrar nodos.
- Tiene botones "Save to demo.json", "Load from demo.json" para
  testear roundtrip.
- "Reset" vuelve al grafo demo inicial.

**No agendado**: este panel NO es producto final — solo valida que la
infra funciona. Queda en el menú Debug indefinidamente como utilidad
para futuros devs que extiendan el framework.

#### 5. Undo/Redo integration

`NodeGraphCommand` heredado de `ICommand` que soporta las 5 operaciones
core:

- `AddNodeCommand(graph, typeTag, pos)`
- `RemoveNodeCommand(graph, nodeId)` — captura el nodo + links incidentes para undo.
- `MoveNodeCommand(graph, nodeId, oldPos, newPos)` — merge consecutivos de drag.
- `AddLinkCommand(graph, fromSocket, toSocket)`
- `RemoveLinkCommand(graph, linkId)` — captura el link para undo.

El Sandbox panel los pushea al `HistoryStack` del editor. Validación
del undo: Ctrl+Z debe revertir las 5 operaciones correctamente.

### Bloques de ejecución

| # | Bloque | Estim. | Notas |
|---|--------|--------|-------|
| A | Plan (este doc) | 30 min | Aprobar con dev antes de arrancar B |
| B | Integración lib + CMake | ~1h | Submodule + CMakeLists + minimal include test que compile |
| C | Modelo de datos puro (`engine/data/nodegraph/`) | ~1.5h | Graph + Node + Socket + Link + CRUD + toJson/fromJson |
| D | Wrapper visual (`engine/ui/nodegraph/NodeGraphEditor`) | ~2h | pImpl + draw() consume Graph y renderea via imgui-node-editor |
| E | Sandbox panel debug + grafo demo pre-cargado | ~1h | Toggle desde Debug menú + 5 botones (save/load/reset/add node/etc) |
| F | NodeGraphCommand + integración con HistoryStack | ~1.5h | 5 commands core + merge de move + tests de undo redo |
| G | Tests unitarios del data model | ~1h | tests/test_nodegraph_data.cpp — CRUD + serialización roundtrip |
| H | Cierre: validación con dev + docs (HITOS/ESTADO/DECISIONS/PENDIENTES) + commits + tag + push | ~45 min | Patrón estándar |

**Total estimado**: ~8h. Hito chico-mediano.

## Decisiones técnicas abiertas

### Submodule vs copia directa de `imgui-node-editor`

Pros submodule: actualizable a versiones nuevas vía `git submodule
update`. Pros copia directa: zero-deps reproducible, no surprise build
breaks por upstream changes.

**Recomendación**: copia directa en `third_party/imgui-node-editor/`
(igual que cómo `imgui`, `glm`, `EnTT`, etc están en `third_party/`).
Versión fija. Si emerge necesidad de actualizar, manual bump.

**Confirmar con dev** al arrancar Bloque B.

### Tipo de socket: compatibilidad estricta o libre

v1: dos sockets conectan si su `typeTag` es idéntico. v2 podría tener
matrix de compatibilidad (e.g. "int" cast a "float", "any" como wildcard).

**Recomendación v1**: estricto, mismo `typeTag`. Los editores específicos
deciden la taxonomía.

### Persistencia del grafo

- JSON via `nlohmann::json` (ya disponible en el motor).
- Schema versionado desde v1 con campo `"_version": 1` para migrations
  futuras.

## Diferidos a hitos futuros (cuando emerjan en Dialog/Quest Editor)

- **Theming custom del grafo** para que matchee la estética del motor
  (paleta naranja Valve, etc). v1 usa default de `imgui-node-editor`.
- **Animaciones de conexiones** (flow visual entre sockets).
- **Hierarchical grouping de nodos** (collapsible meta-nodes).
- **Export-to-image** (PNG del grafo para docs).
- **Comments / sticky notes** sobre el canvas.
- **Tipos custom complejos** de sockets con preview inline (ej. preview
  del audio en un socket de "audio_path").

Estos son features que `imgui-node-editor` soporta o se pueden
implementar por encima — agregarlos cuando los Dialog/Quest Editor los
necesiten, no especulativamente en F2H46.

## Riesgos y tradeoffs

- **Riesgo: integración de `imgui-node-editor` rompe build**. Mitigación:
  Bloque B aislado — si emerge problema serio (mismatch ImGui version,
  warnings de compilación, etc), evaluar antes de seguir.

- **Riesgo: API del wrapper queda mal y los editores específicos sufren**.
  Mitigación: Bloque D escribe el wrapper EN PARALELO al uso conceptual
  desde el Sandbox panel (Bloque E) — si el API se siente incómodo en
  el caller del Sandbox, lo rediseñamos antes de cerrar.

- **Riesgo: Performance con grafos grandes (>500 nodos)**. Sin presión
  real ahora — los Dialog/Quest típicos serán <100 nodos. Si emerge,
  profilear y optimizar.

- **Tradeoff: pImpl agrega indirection en draw()** (~1 dereference por
  llamada). Negligible. Beneficio: el header del wrapper no contamina
  con `imgui-node-editor` headers.

- **Tradeoff: dependencia externa nueva (`imgui-node-editor`)**. El
  dev ya aceptó esto. License es MIT — compatible.

## Validación

- Build limpio Debug + Release.
- Tests pasan (suite existente + 8-12 tests nuevos para data model).
- Sandbox panel toggleable desde menú "Debug" del editor.
- En el Sandbox:
  - Grafo demo se carga con 5-6 nodos al abrir.
  - Pan funciona (middle-click drag o equivalente del lib).
  - Zoom funciona (scroll).
  - Drag de un nodo lo mueve, undo (Ctrl+Z) revierte.
  - Right-click sobre canvas → menú "Add Node" → agrega nodo, undo revierte.
  - Conectar dos sockets via drag desde output a input → link aparece,
    undo lo borra.
  - Click derecho sobre nodo → "Delete" → nodo + links incidentes
    desaparecen, undo restaura.
  - "Save to demo.json" escribe el grafo, "Load from demo.json" lo
    levanta idéntico.
- Validación visual con dev: tour del Sandbox tocando las 5 operaciones.

## NO entra en F2H46

- Dialog Editor real (Bloque 2 — F2H47+).
- Quest Editor real (Bloque 3 — F2HXX+).
- 3D preview widget (Bloque 0.2 — F2H47 o paralelo a este).
- Theming del grafo a estética del motor (cuando emerja en Dialog/Quest).
- Multi-graph workspace tabs (un grafo a la vez en v1).
- Copy/paste cross-graph (mismo grafo solo).
- Search/filter de nodos por nombre.
- Auto-layout (botón "organize nodes" que reordena automáticamente).
- Export-to-image.
