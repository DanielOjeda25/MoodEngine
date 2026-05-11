# Estado actual del proyecto — handoff para el agente

> **Propósito de este documento:** dar al próximo agente (Claude Code en Antigravity) el estado exacto del proyecto, comandos que ya sabemos que funcionan, y el siguiente paso concreto. Leer esto **antes** de `MOODENGINE_CONTEXTO_TECNICO.md` para saber dónde estamos parados.

---

## 0. ACTUALIZACIÓN F2H48.1 cerrado (2026-05-10)

**Cierre crítico pre-F2H49: serialización DialogComponent + DialogScriptHost (condition_lua reales).**
Tag: `v1.36.1-fase2-hito48-1`. Cita verbatim del dev: *"me gustaría que la demo sea completa como la B"*.

**Por qué F2H48.1**: tras cerrar F2H48 listé 4 deudas como "diferidas". El dev pidió evaluación honesta — 2 eran bloqueantes para una demo F2H49 creíble (serialización + hooks Lua reales), 2 eran YAGNI legítimo (persist dialogVars / mouse-clic). F2H48.1 ataca las 2 bloqueantes.

**Qué entrega F2H48.1**:
- **Serialización completa de `DialogComponent`** (`SavedDialog` + write/read EntitySerializer + apply SceneLoader). El NPC con dialog persiste en `.moodmap`. Tests roundtrip nuevos.
- **`DialogScriptHost`** singleton namespace con sol::state dedicada del juego. Bindings: tabla `dialog` (vars + read-only state), `hud`, `log`. NO bindings de entity (sin `self`). Wrap defensivo `"return (" + expr + ")"` + fail-safe a false en errores.
- **Wireup en init del editor + player**: `Dialog::DialogScriptHost::init()` post-I18n. Idempotente. Registra los hooks de `DialogSystem::setEvaluator/setExecutor` automáticamente.
- **`condition_lua`/`on_select_lua` ahora EJECUTAN realmente** — el dev puede escribir `dialog.get_var('met_guard') == 'true'` en una choice y aparece/desaparece según el state. `dialog.set_var('answered', 'yes')` desde un `on_select_lua` muta el var antes de la transición.
- **10 tests nuevos** en `test_dialog_script_host.cpp` + 3 en `test_scene_serializer.cpp`. Suite **739/8836** verde.

**Decisiones**:
1. Sol::state separada del per-entity de `ScriptSystem` — el host es global del juego, no de un script específico. Sin `self`, sin `physics`, sin `engine.exposed`.
2. NO exponer `dialog.start/advance/continueNext/stop` desde el host — riesgo de recursión durante `on_select_lua` (control de flujo viene del DialogSystem o de scripts de entity, no de los hooks).
3. Fail-safe a false en errores de Lua: ante parse error o runtime error de un `condition_lua`, la choice no aparece. Filosofía defensiva — el dev ve el warn en el log y arregla.
4. `reset()` del host NO toca `GameState::dialogVars()` (preserva la decisión F2H48 #2: vars sobreviven entre dialogs).
5. NO se ataca persistencia de `dialogVars` en save ni mouse-clic en choices — quedaron como diferidos sin urgencia. La demo F2H49 no los necesita.

**Próximo a atacar**: **F2H49 — Demo characters Mixamo + escena narrativa completa** (Bloque 2.5 del plan). Ahora con infra real para choices condicionales + escena guardable + .mooddialog persistido.

---

## 0.2. F2H48 cerrado (2026-05-10)

**Tercer hito real de Sub-fase 2.5 cerrado — Dialog runtime + HUD HL2-style + Lua bindings activos.**
Tag: `v1.36.0-fase2-hito48`. Cita verbatim del dev al validar: *"ya funciona"*.

**Qué entrega F2H48** (runtime completo + HUD + Lua + locking del char controller):
- **`DialogSystem`** state machine pura (`engine/dialog/DialogSystem.h/cpp`) — singleton namespace paridad con `GameState`. API completa: `start/advance/continueNext/stop/isActive/currentLine/availableChoices` + hooks inyectables `setEvaluator/setExecutor/setNodeEnterHook/setChoiceHook` (callbacks `std::function`, módulo sin deps sol2). Recorrido del graph via output sockets (`addLink` del F2H46 + invariante auto-sync de F2H47).
- **`AssetManager::loadDialog`** + cache + slot 0 (asset vacío) + nuevo partial `AssetManager_Dialog.cpp` (mismo patrón que `_Prefab.cpp`).
- **`DialogComponent`** entity-side: `{dialogPath, autoStartOnInteract, cachedDialogId}` agregable desde el popup "Add Component" (categoría Logic).
- **`DialogInteractSystem`** que conecta `TriggerComponent + DialogComponent + tecla E` con el state machine. Setea `hud.interact_prompt = "[E] Hablar"` cuando player adentro del trigger + dialog no activo; auto-arranca dialog al apretar E. Maneja también `tickActiveDialog` para digitos 1-9 → `advance(idx)`.
- **HUD widget `dialog_box`** HL2-style en `GameOverlay.cpp` — caja inferior 800px max o 70% de ancho, NPC text con wrap, choices numeradas, naranja Valve + amarillo HL. Toggleable via `hud.set_widget("dialog_box", false)`.
- **Char controller lock** via flag global `GameState::dialogActive()`: WASD/jump/crouch ignorados mientras hay dialog activo; mouse-look queda libre. Editor + Player ambos respetan el flag.
- **Lua bindings tabla `dialog`** con 10 funciones (`isActive/currentNode/advance/continueNext/stop/set_var/get_var/has_var/clear_vars/start`). `set_var/get_var` operan sobre `GameState::dialogVars()` (sobreviven entre dialogs por decisión).
- **Workspace Narrativa rediseñado** (3 columnas): Viewport 3D izq 30% / Dialog Editor centro 40% / col der dividida vertical con Node Inspector arriba + Dialog Browser abajo. Pedido del dev tras tour visual: necesita ver NPCs reales + posicionar triggers en escena 3D mientras edita el contenido narrativo.
- **18 tests nuevos + ~120 assertions**: `test_dialog_system.cpp` (15 tests) + 3 tests Lua en `test_lua_bindings.cpp`. Suite total: **726/8797** verde.

**Decisiones clave (confirmadas pre-implementación)**:
1. Trigger de start = `DialogComponent.autoStartOnInteract` + Lua escape hatch `dialog.start(path)` — opción (c) recomendada.
2. Vars persistencia: sobreviven en `GameState::dialogVars()` entre dialogs (habilita "el NPC recuerda que ya hablaste"). Persistencia en save deferred.
3. Sandbox Lua para condition/on_select: global del juego (acceso a inventory/quest/dialog/etc.).
4. DialogSystem singleton namespace (no clase instanciable) — paridad con GameState. Una sola conversación activa a la vez en v1.
5. Choice keys 1-9 (HL2 style, no clic con mouse en v1) + E para continueNext en nodos sin choices.
6. Hooks `LuaEvaluator/LuaExecutor` disponibles pero NO inyectados en v1 — `condition_lua`/`on_select_lua` se ignoran silenciosamente. Inyección real requiere sol::state global dedicada, deferred hasta caso real.
7. Workspace Narrativa con viewport 3D (no NarrativeIntro placeholder) — pedido del dev como prep para F2H49 (demo characters).

**Lo que falta para validación end-to-end real con personajes** (F2H49):
- Demo characters Mixamo (player + NPC) con anims (idle/walk/run/wave/talking).
- Pipeline FBX → glb (Blender o `gltf-pipeline` CLI).
- Demo scene `narrative_demo.moodmap` con player spawn + NPC con `DialogComponent` + trigger interact.
- Update demo dialog a 5-7 nodos con branching real.
- Animator state machines (player + NPC).
- Welcome modal: botón "Cargar demo narrativo" separado del Fox.

**Próximo a atacar**: **F2H49 — Demo characters Mixamo + escena narrativa completa** (Bloque 2.5 del plan). Tag previsto `v1.39.0-fase2-hito49`.

---

## 0.1. F2H47 cerrado (2026-05-10)

**Segundo hito real de Sub-fase 2.5 cerrado — Dialog Editor (autoría) activo.**
Tag: `v1.37.0-fase2-hito47`. Cita verbatim del dev al validar: *"todo ok"*.

**Qué entrega F2H47** (editor side completo end-to-end, sin runtime):
- **Schema `.mooddialog`** versionado (v1) con `Asset { Graph + Metadata }` puro testeable.
  - Tipo de nodo único en v1: `dialog_line` con customData `{text_key/literal, portrait, audio, animation, choices[]}`.
  - Cada `Choice = {label_key/literal, condition_lua, on_select_lua}` — los hooks Lua cubren el 80% de gameplay sin necesidad de tipos `condition`/`action` dedicados (diferidos a v2).
  - **Invariante crucial auto-sync**: N choices = N output sockets; mantenido por `writeLine()` automáticamente.
- **`DialogValidator`** con 6 reglas (start_node, input/output sockets, choices sync, cycles via DFS, orphans).
- **3 paneles del editor** (categoría Narrative):
  - `DialogBrowserPanel`: lista `*.mooddialog` de `assets/dialogs/` + New (popup con sanitización) + Refresh.
  - `DialogEditorPanel`: reusa `NodeGraphEditor` de F2H46 + toolbar (Save / + Línea / Marcar Inicio / Validar / Cerrar) + dirty tracking + Events → NodeGraphCommand (undoable).
  - `DialogNodeInspectorPanel`: contextual, toggle text_key↔text_literal, choices con add/remove inline → trigger auto-sync.
- **Workspace "Narrativa" rediseñado** (v6 ini): Dialog Editor central + Inspector der ~28% + Browser/Intro bottom tabulados ~28%. Sandbox queda solo en Ver > Debug.
- **Sample demo** en menú Ayuda > Demos > "Cargar diálogo demo" — genera `demo_intro.mooddialog` con 3 nodos + 2 choices ("¿qué te trae a estas tierras?" → heroico/casual) y lo abre.
- **35 tests nuevos + 109 assertions** (test_dialog_asset.cpp + test_dialog_validator.cpp). Suite total: **708/8694** verde.

**Lo que falta para que el sistema sea utilizable in-game** (F2H48):
- DialogSystem runtime que interprete el árbol durante Play Mode.
- HUD widget default (caja inferior estilo HL2) — override-able vía Lua.
- Lua bindings: `dialog.start("npc_id")`, `dialog.set_var/get_var`, `dialog.on_node_enter/on_choice` hooks.
- `DialogComponent` para entidades — un NPC en la escena con un `.mooddialog` apuntado dispara el dialog al interactuar.

**Fixes técnicos durante F2H47:**
- **Regresión F2H46 corregida**: `test_workspace_manager.cpp` tenía `count == 4` hardcoded → ahora 5 (workspace narrative agregado en F2H46). 7 assertions actualizadas.
- **Bump `imgui_layout_v5.ini → v6.ini`** porque el layout del workspace Narrativa cambió.
- **Extender NodeGraph::Graph** con `removeSocket(SocketId)` (cascade de links incidentes) — necesario para auto-sync de choices.

**Pendientes conocidos** (post-F2H47):
- **F2H48 — Dialog runtime**: ver arriba.
- Después de F2H48, **F2H4X — Inventory** (UI propia con grid + 3D preview, no node-graph).
- Después, **F2H4Y — Quest Editor** (reusa node-graph de F2H46 con flowchart de objetivos).
- Theming custom del grafo (paleta narrativa) diferido hasta que emerja necesidad visual.

---

## 0bis. ACTUALIZACIÓN F2H46 cerrado (2026-05-10)

**Sub-fase 2.5 arrancada con su primer hito real (Bloque 0.1 del plan).**
Tag: `v1.36.0-fase2-hito46`. Cita verbatim del dev al validar: *"todo ok"*.

**Qué entrega F2H46:**
- **Framework de node-graph reutilizable** (`engine/nodegraph/`): data model puro (`Graph.h/cpp`) + wrapper visual con pImpl (`NodeGraphEditor.h/cpp`) sobre la lib `thedmd/imgui-node-editor`. Patrón state-vs-rendering separados (testable sin ImGui).
- **5 NodeGraphCommands undoable** (`editor/commands/NodeGraphCommand.h/cpp`): Add/Remove Node, Move, Add/Remove Link. Snapshot-based undo para reconstruir nodos con sockets + links incidentes (con socket-ID remap).
- **Workspace nuevo "Narrativa"** (5to tab al lado de "Diseño de Niveles") con dock layout limpio 70/30: Sandbox a la izquierda + `NarrativeIntroPanel` informativo a la derecha. Sin paneles de scene/3D contaminando.
- **`NodeGraphSandboxPanel`** debug-only que ejercita el framework end-to-end (demo de 4 nodos + toolbar i18n + save/load JSON).
- **25 tests del data model** (76 assertions): CRUD, 5 reglas de canConnect con bad-input rejection, cascade delete, roundtrip JSON con schema versioning, fan-out validation.

**Cómo conecta con los próximos editores:**
- **F2H47 (Dialog Editor)**: cada `Node` = una línea hablada por NPC. Sockets `flow` para conexiones entre líneas, sockets `choice` para opciones del jugador. `customData` JSON contiene texto, audio path, portrait, animation hook.
- **F2H4X (Quest Editor)**: cada `Node` = un objetivo. Sockets `dependency`. `customData` con tipo de predicado + parámetros.
- **F2H4Y (Inventory)**: NO usa node-graph (UI propia con grid + 3D preview). Usa el HUD framework F2H39 para inventory widget.

**3 fixes técnicos aplicados durante la validación:**
1. **`ConfigDebugHighlightIdConflicts = false`** global en `EditorApplication_Init.cpp`. El debug-check nuevo de ImGui 1.92 da false-positives con imgui-node-editor (la lib maneja su propio ID stack). Nuestra suite de 8580 assertions cubre lo que él detectaría.
2. **Patch idempotente al header de la lib** (CMake configure-time, `file(READ/WRITE)`): `imgui_extra_math.inl` define `operator==/!=/(float, ImVec2)` que colisionan con `imgui.h` docking branch — guard con `MOOD_NE_OPS_GUARD + #ifndef IMGUI_DEFINE_MATH_OPERATORS_IMPLEMENTED`.
3. **Bump `imgui_layout_v3.ini → v5.ini`** + cambio de window titles en panels nuevos: usar `name()` directo (estable English, sin `###`) que matchea con `DockBuilderDockWindow`. La convención del editor es panel titles en inglés estable + contenido i18n.

**Pendientes conocidos** (post-F2H46):
- **F2H47 — Dialog Editor**: primer editor real construido sobre la infra de F2H46. Cita del dev: *"como creo conversaciones?"* → eso es F2H47.
- **Theming custom del grafo** (paleta naranja Valve del motor): diferido a cuando los Dialog/Quest Editor lo pidan, no especulativo.
- **Lua scripts traducibles** (deuda F2H43): sin urgencia hasta tener scripts de gameplay reales.

---

## 0bis. HANDOFF ACTIVO — Sub-fase 2.5 confirmada con scope Pro Tools (2026-05-10)

> **Mensaje para la próxima sesión del agente** (probablemente en otra máquina del dev — escritorio).
> El dev quiere arrancar Sub-fase 2.5 (diálogos / quests / inventario) y antes de tocar código exigió alineamiento estratégico. Esta sección es el resumen de esa conversación. **Leerla antes de hacer cualquier propuesta de implementación.**

### Decisión 1 — Framing: motor que crea juegos, no juego concreto.
Cita verbatim del dev: *"este sistema que haremos ahora es el más importante, me refiero a que debe ser la base para que a futuro cualquier dev al crear su juego pueda utilizar este sistema de inventario, quests, y diálogos, para sus juegos, osea debe ser versátil y realmente amigable de usar... aún no estamos creando un juego, estamos creando el motor que va a crear juegos"*.

Implicaciones que se acordaron y que aplican a TODAS las decisiones de diseño de la sub-fase:

1. **Data-driven, no code-driven.** Devs deben crear NPCs / quests / items SIN tocar código del motor. Todo es asset visualmente editable (`.mooddialog`, `.moodquest`, `.mooditem`).
2. **Sin semántica hardcodeada de gameplay.** El motor NO sabe qué es "XP", "mana", "vida" o cualquier recurso específico. API genérica: `stats.add("xp", 100)`, `inventory.add(player, "rupia", 5)` — el dev registra los recursos que su juego usa.
3. **Hooks Lua sobre primitivas, no callbacks hardcodeados.** Ej: `on_quest_complete(quest_id, callback)` — el callback Lua hace lo que ese juego quiera (XP, dialog flag, animación). Mismo patrón que ya tenemos con triggers + scripts.
4. **Default + override en rendering.** Cada sistema viene con widget HUD default funcional. `dialog.set_renderer(lua_callback)` permite override total para juegos con HUD propio.
5. **Composabilidad con sistemas existentes.** Quest objectives son predicados genéricos contra estado: `item_count(player, "key") >= 1`, `area_entered("temple")`, `flag_set("boss_defeated")`. NO inventamos tipos específicos como "kill 10 wolves".
6. **Editor visual real, no JSON a mano.** Cada sistema necesita su panel dedicado de editor visual. Si el dev abre `.json` crudo para crear contenido, fallamos.
7. **State vs rendering separados** (patrón F2H39 GameState ↔ GameOverlay). Toda la lógica vive en módulos puros sin deps gráficas → `mood_tests` los testea sin ImGui.
8. **Engine-agnostic respecto al género.** RPG / shooter con quests / walking sim / metroidvania / dungeon crawler — los 3 sistemas deben servir a todos. Si la API solo encaja con uno, fallamos.

### Decisión 2 — Scope nivel B (Pro Tools), no nivel A (mínimo viable).
Cita verbatim del dev: *"B, aunque nos lleve días"*.

Esto significa:
- **Dialog Editor**: node-graph visual interconectado estilo Unreal Blueprints / `ink` — pan/zoom, sockets, conexiones por curvas Bézier, snapping. **NO** una lista de nodos con dropdowns "next node".
- **Quest Editor**: flowchart visual de objectives + dependencies + rewards. **NO** una lista plana de objectives.
- **Item Browser**: panel con grid de cards + **3D preview rotable del modelo** + tags/filters + property editor por field. **NO** un Asset Browser plano.
- **Costo aceptado**: la sub-fase pasa de ~3 hitos (scope A en `PLAN_FASE2.md:285-303`) a **~15-20 hitos**. El roadmap original de Fase 2 queda obsoleto en ese rango.

### Decisión 3 — Orden de bloques propuesto (pendiente confirmar con dev al arrancar).

**Bloque 0 — Infra compartida** (antes de los 3 sistemas):
- **Node-graph framework reutilizable**: pan/zoom/snap, nodos draggeables, conexiones por sockets, save/load JSON. Lo usan Dialog Editor + Quest Editor + eventualmente Material Editor pro version.
- **3D preview widget reutilizable**: render-to-texture mini de mesh/material, controles de cámara orbital. Lo usa Item Browser + eventualmente Material Editor pro.

**Bloque 1 — Inventario** (recomendado primero — diálogos/quests dependen de "tengo X item" para condicionales):
- ItemAsset schema + serialización (`.mooditem`).
- Item Browser panel con 3D preview + tags/filters.
- InventoryComponent + soporte para 3 modos de layout (grid 2D RE-style + lista plana + equipment slots).
- Pickup/drop integrado con triggers + interact_prompt del HUD.
- Bindings Lua + i18n para nombres/descripciones.

**Bloque 2 — Diálogos:**
- DialogTree schema con nodos + opciones + variables + portrait/audio/animation hooks.
- Dialog Editor node-graph (sobre el framework del Bloque 0).
- Runtime + HUD widget default (override-able via Lua).
- Bindings Lua + i18n para texto de líneas.

**Bloque 3 — Quests:**
- Quest schema con objectives (predicados genéricos) + rewards genéricos + state machine.
- Quest Editor flowchart (sobre el framework del Bloque 0).
- Quest Log panel + HUD tracker widget.
- Predicados generic-purpose: `item_count`, `flag_set`, `area_entered`, `counter_at_least`, `entity_killed_tag`.
- Bindings Lua + i18n.

**Bloque 4 — Soporte para producción:**
- Stats/RPG primitives genéricos (sin hardcodear XP/HP/Mana específicos).
- Save extendido v3 (`.moodsave` con dialog state + quest state + inventory + stats).
- **Template/sample project** que muestre los 3 sistemas integrados — el "Hello World" del motor para juegos narrativos. Crítico para devs nuevos.
- Documentación developer-facing en `docs/CONTENT_PIPELINE.md` (esto sí justifica doc externo porque devs externos lo necesitan).

### Próxima acción concreta para la sesión que retome esto

1. **Leer [`docs/PLAN_SUBFASE_2_5.md`](PLAN_SUBFASE_2_5.md)** — skeleton del plan ya está commiteado. Tiene la filosofía y la estructura de bloques pero los hitos individuales están como `[TBD]`.
2. **Confirmar con el dev** el orden (recomendado: inventario primero) y la barra de "definición de done" por hito antes de arrancar.
3. **Decisión técnica abierta a evaluar**: para el node-graph framework del Bloque 0 — ¿implementar propio sobre ImGui DrawList (control total + más esfuerzo) o usar `imnodes` / `imgui-node-editor` (más rápido + menos control)? Documentar pros/cons antes de elegir.
4. **NO arrancar implementación** sin tener el plan completo y aprobado por el dev. El dev fue explícito: la sub-fase es la más importante del motor, no se improvisa.

### Lo que NO se quiere

- **NO scope reducido**. El dev rechazó explícitamente el nivel A.
- **NO assumptions de género**. Cualquier API que solo sirva para RPG o solo para shooter es bug de diseño.
- **NO hardcoded semantics** ("XP", "level", "mana" como conceptos del motor). El motor solo conoce contenedores genéricos.
- **NO empezar por diálogos si los items condicionan opciones de diálogo** — empezar por items evita migrar flags hardcodeados después.

---

## 1. ¿Dónde estamos?

**🚀 Fase 2 — F2H45 cerrado: cierre de deudas pre-Sub-fase 2.5 (AddComponentCommand undoable + Lato en Player con tildes + Console tooltip i18n).**
Tag: `v1.35.0-fase2-hito45`.
Validado visualmente por dev *"lo demás está OK"* tras tour de los 3 bloques. 3 deudas reales cerradas (deuda crónica F2H38 Lato Player + sweep i18n F2H43 incompleto + falta de undo en Add Component F2H44). Fix lateral: botón "Recompute mesh" del Inspector eliminado a pedido del dev (era debug breadcrumb sin uso real).

**🏁 Motor sobrado para contenido real: i18n completo (con tildes en ES) + UX onboarding pulido (Add Component undoable + Welcome demo Fox + workspaces ASCII-id + outline AABB real + Material Editor robusto + Ctrl+wheel snap orto + VisGroups help + Console tooltip i18n).** 43/44 hitos de Fase 2.

**Decisiones clave de F2H45:**

- **AddComponentCommand type-erased en lugar de N templates específicos.** Una sola clase `AddComponentCommand` con `std::function<void(Entity&)>` para `add` y `remove`. Helper templated `makeAddComponentCommand<T>(entity, label)` captura T en las dos closures. Razón: 11 componentes templated generarían 11 instanciaciones del comando completo en el binario; type-erase es 1 clase + 11 sitios de captura templated triviales. Coherente con cómo el popup ya usaba lambdas para los add. Trade-off aceptado: dos `std::function` por comando (allocación heap chica), aceptable porque add se usa raramente vs sliders del Inspector.
- **No snapshot del estado pre-add.** Si el dev hace add → edita campos → undo, los EditPropertyCommand quedan más arriba en el stack y se deshacen primero (LIFO). El AddComponentCommand solo necesita add/remove con default — los campos editados quedan capturados en sus propios commands. Razón: simplicidad + correctitud por composición. Probado en `test_add_component_command.cpp` con la interacción Add+EditProperty.
- **Lato en Player con mismo patrón que F2H38, sin FA merge.** El HUD del Player usa DrawList procedural (F2H39+) sin iconos FA. Cargar FA solo agregaría peso al atlas para nada. Si en el futuro un menú del Player necesita FA (improbable), agregarlo en su momento.
- **Sweep tildes delegado a subagente.** ~75 palabras corregidas en ~60 valores de `es.json` — refactor mecánico voluminoso (>15 ediciones del mismo patrón) calificó para delegación según convención del dev. Reporte del subagente listó decisiones (términos técnicos en inglés intactos, voseo argentino preservado, signos de apertura agregados) — judgment calls validados por mí post-hoc.
- **Console tooltip: keys i18n para texto, icons FA en código.** Los macros `#define ICON_FA_X "\xef\x86\x88"` no caben en JSON sin perder los bytes UTF-8. Solución: 8 keys nuevas (header + 6 levels + footer), construcción del tooltip en C++ concatenando icon + space + I18n::T(level). Patrón replicable para cualquier texto con icons inline.
- **"Recompute mesh" eliminado**, no escondido en grupo "Debug avanzado". Era debug breadcrumb del Hito 12+ que nunca se usó (no debería pasar el caso que lo justificaba — ninguna mutación del brush deja `dirty=false` por error). Eliminar > esconder cuando no aporta valor real.

**Implementación (F2H45 Bloques A-C):**

- **Bloque A — AddComponentCommand**: nuevo `editor/commands/AddComponentCommand.h` (header-only, ~80 LOC). Refactor del popup en `InspectorPanel.cpp`: struct `Item.makeCmdFn` ahora es `std::function<std::unique_ptr<ICommand>(Entity, std::string)>`; las 11 closures pasan a `[](Entity en, std::string lbl) { return makeAddComponentCommand<X>(en, std::move(lbl)); }`; handler del Selectable hace `historyStack()->push(it.makeCmdFn(e, label))`. Key i18n nueva `editor.cmd.add_component`. 8 tests nuevos en `test_add_component_command.cpp` (registrado en `tests/CMakeLists.txt`).
- **Bloque B — Lato en Player + tildes**: ~25 LOC en `PlayerApplication_Init.cpp` post-init de ImGui (mismo patrón que `EditorApplication_Init.cpp` F2H38, sin el bloque FA merge). Sweep mecánico de `es.json` delegado a subagente — 400 keys totales preservadas, ~75 palabras con tildes correctas.
- **Bloque C — Console tooltip i18n**: 8 keys nuevas en `assets/i18n/en.json` + `es.json`. Refactor de ~12 LOC en `ConsolePanel.cpp` (`ImGui::SetTooltip(literal_concat)` → `std::string body = ...; SetTooltip("%s", body.c_str())`). **Fix lateral del dev**: -8 LOC en `InspectorPanel_Brush.cpp` (botón Recompute mesh + comentario), -2 keys (`editor.panel.inspector.brush.recompute_mesh` en EN+ES).

**Pendientes conocidos** (post-F2H45):
- **Sub-fase 2.5 gameplay** (diálogos / quests / inventario). PLAN_FASE2 líneas 285-303. **Próximo a arrancar.**
- **Lua scripts traducibles** (deuda F2H43 que NO entró en F2H45): solo aplica a demos actuales, esperar a tener scripts gameplay reales.
- **USER_GUIDE/ + README + GIF + tutorial** (descartado por el dev en F2H44): hito propio post-Fase 2.
- **Mini-map / Radar**, **Themes HUD**, **HUD diegetic 3D**, **GPU optimizations** (sin urgencia, features diferidas).
- Validación full del Player con compiledMesh: al primer empaquetado real.

**Próximo paso**: **Sub-fase 2.5 gameplay** (confirmado por el dev como next-up).

### F2H44 (anterior, ya cerrado)

**🚀 F2H44 cerrado: polish onboarding UX (sin docs externos).**
Tag: `v1.34.0-fase2-hito44`.
Validado visualmente por dev *"funciona"*, *"perfecto"*, *"va ok"* tras tour de cada bloque. 5 gaps de UX cerrados sin agregar docs externos (deuda de `USER_GUIDE/` queda diferida intencionalmente — *"seguiremos agregando cosas que luego vamos a terminar cambiando"*).

**Decisiones clave de F2H44:**

- **Add Component popup sin command undoable.** Patrón consistente con los demos del menú `Ayuda > Demos` (también sin undo). Razón: la mayoría de los add son de componentes "fáciles de quitar manualmente" (eliminar el componente del Inspector tiene su propio gesto). AddComponentCommand undoable queda anotado como deuda futura por si emerge presión real.
- **Botón "Cargar mapa demo (Personaje animado)" en Welcome modal.** Un solo click → crea proyecto temp + spawnea Fox.glb. Razón: la primera vista del editor para un dev nuevo NO debería ser dockspace vacío, sino motor en acción. Demo elegido: animated character (visual fuerte sin saturar — Stress Scene completa intimida; Shadow demo es muy estático). El handler dispara `handleNewProject()` (síncrono via pfd) + `requestSpawnAnimatedCharacter()` consumido por el dispatch normal de spawners.
- **Outline AABB real para meshes seleccionados.** Bug crítico de descubribilidad: pre-F2H44 el outline de meshes usaba `glm::vec3(-0.5f, 0.5f)` hardcoded — cubito 1m³ centrado en el origen del transform, invisible para meshes grandes (Fox.glb ~3m largo). Fix: leer `MeshAsset::aabbMin/aabbMax` via AssetManager. Para entidades sin mesh ni brush (Light/Audio/Trigger/Camera/ParticleEmitter), AABB chico fijo 0.5m³ alrededor del origen → SIEMPRE hay feedback visual de selección (consistencia con orto que ya hacía esto desde F2H35).
- **Workspaces con ID ASCII estable separado del label visible.** Pre-F2H44 el `Workspace.name` era a la vez ID + label (`"Programar"`/`"Materiales"`/`"Editor de mapas"`) → traducir el label rompía la identidad persistida en `.moodproj`. F2H44 separa: `name` ahora es ID ASCII estable (`"layout"/"scripting"/"materials"/"map_editor"`), label visible viene de `T("workspace.<id>")`. Migración extendida en `WorkspaceManager::migrateWorkspaceName` cubre 3 generaciones (F2H7 inglés / F2H22 español / F2H44 ASCII). Comparaciones en código actualizadas en 5 archivos (MenuBar/EditorRenderPass/EditorApplication/EditorUI/EditorOverlay/Dockspace) + 8 tests del WorkspaceManager actualizados.
- **Material Editor: eliminado modo TwoColumns, siempre Vertical.** Pre-F2H44 el panel tenía layout adaptativo (>= 540px = 2 columnas controles izq | preview der). Pero `ImGui::Columns` no sincroniza alturas, y al docking en otros lados el preview quedaba flotando en el medio del panel desconectado del label "Preview". Solución: layout **siempre vertical** (preview arriba + controles abajo). Robusto a cualquier resize/dock. Trade-off aceptado: en monitores muy anchos se pierde la posibilidad de ver preview + controles lado a lado, pero el modo 2-col estaba roto en práctica.
- **Ctrl+ScrollWheel cicla snap step sobre orto viewports.** Atajo paralelo a Ctrl+= / Ctrl+- (más natural cuando ya tenes el mouse encima del viewport). Handler en `EditorApplication::processEvents` con triple gate: workspace `map_editor` + `SDL_GetModState() & KMOD_CTRL` + `liveCursor().hovered` en alguno de los 3 ortos. NO roba zoom de cámara perspectiva.
- **VisGroups help marker `(?)` con tooltip multilínea.** Onboarding contextual mínimo del concepto Hammer/Source para devs nuevos. Hover-only, no agrega ruido visual. Patrón estándar de ImGui (`TextDisabled("(?)") + IsItemHovered + SetTooltip`). Dos paradigmas validados: el panel sigue ofreciendo el control (botón "+ Nuevo grupo" + tooltip propio), y el `(?)` adicional explica el concepto cuando el dev lo necesita.
- **USER_GUIDE/* + README + GIF descartados explícitamente.** Era el bloque 5 propuesto en mi auditoría inicial. El dev rechazó: *"seguiremos agregando cosas que luego vamos a terminar cambiando"*. Razón sólida: docs externos rotarían más rápido que la implementación. El motor seguirá evolucionando hasta sub-fase 2.5+; documentar ahora sería re-trabajo. Hito propio cuando el motor estabilice (post-Fase 2 likely).
- **Subagente NO usado en F2H44.** A diferencia de F2H43 (255 keys en 23 archivos = caso textbook de delegación), F2H44 fueron 5 cambios pequeños y heterogéneos que no encajaban en un patrón mecánico. Cada bloque requirió judgment calls puntuales (categorías Add Component, choice del demo, mappings de migración workspaces, layout del Material Editor, gates del Ctrl+wheel). Hacer todo en main context fue lo correcto.

**Implementación (F2H44 Bloques A-E):**

- **Bloque A — Add Component button + popup**: nuevo método `InspectorPanel::renderAddComponentSection(Entity)` + helper `drawAddComponentPopup(Entity)`. Llamado al final del dispatch en `onImGuiRender`. Lista de 11 componentes con functors lambda capturando entity by mutable copy (entt handles son stable refs, capture by value es seguro). Search case-insensitive con std::tolower. ~170 LOC en InspectorPanel.cpp + ~10 LOC en .h.
- **Bloque B — Welcome demo button + outline fix**: `requestOpenDemoMap()` + `consumeOpenDemoMapRequest()` en EditorUI.h (~20 LOC). Botón en `EditorUI::drawWelcomeModal`. Handler en `EditorApplication_Run.cpp` consume el flag + llama `handleNewProject` + dispara spawn. Fix outline en `EditorRenderPass.cpp` línea ~600: branch nuevo `else if (sel.hasComponent<MeshRendererComponent>())` lee `MeshAsset::aabbMin/Max` via `m_assetManager->getMesh(mr.mesh)`. Else fallback a 0.5m³ para point entities.
- **Bloque C — Workspaces ID/label split**: `WorkspaceManager::defaultWorkspaces` retorna IDs ASCII. `migrateWorkspaceName` extendida con 4 mappings nuevos (`Layout→layout`, `Programar→scripting`, `Materiales→materials`, `Editor de mapas→map_editor`) + preserva los IDs nuevos como passthrough. `isValidWorkspaceName` valida los IDs nuevos. 5 archivos del editor con comparaciones actualizadas. **Material Editor**: eliminadas líneas del enum `LayoutMode::TwoColumns`, columnas, NextColumn — ~20 LOC neto eliminadas. **Ctrl+wheel**: bloque `else if` adicional en `processEvents` con triple gate (workspace + KMOD_CTRL + hovered). 4 keys i18n nuevas en JSONs + `OrthoViewportPanel.h` incluido en EditorApplication.cpp para acceder a `liveCursor()`.
- **Bloque D — VisGroups help (?)**: ~10 LOC en VisGroupsPanel.cpp tras el botón "+ Nuevo grupo" (`SameLine + TextDisabled("(?)") + IsItemHovered + SetTooltip`). 1 key i18n nueva `editor.panel.visgroups.help_tooltip` con explicación multilínea (`\n` literal en JSON).
- **Bloque E (este commit)**: docs + commits + tag + push.

**Pendientes conocidos** (post-F2H44):
- **Sub-fase 2.5 gameplay** (diálogos / quests / inventario). PLAN_FASE2 líneas 285-303.
- **AddComponentCommand undoable**: nuevo command genérico templated. Bajo, sin urgencia (mismo patrón que demos del Help menu).
- **Cambiar font del MoodPlayer a Lato** (deuda crónica desde F2H38, presión nueva en F2H43): habilita tildes en `es.json` sin tofu en gameplay.
- **Lua scripts traducibles** (deuda F2H43): binding `T("...")` desde sol2 + sweep de `assets/scripts/*.lua`.
- **Console tooltip multilínea i18n** (deuda C2 del subagente F2H43).
- **USER_GUIDE/ + README + GIF + tutorial** (era F2H7 + F2H44 Bloque 5, descartado por el dev): hito propio post-Fase 2 cuando el motor estabilice y docs no roten.
- **Mini-map / Radar** (CoD/Fallout): requiere render-to-texture topdown.
- **Themes alternativos del HUD** (Doom saturado / Fallout verde): theme runtime + bindings Lua.
- **HUD diegetic 3D** (Pip-Boy / muñequera Metro): requiere FPS arms primero.
- **Optimizaciones GPU side** (de F2H42): timestamp queries / CSM / frustum cull shadow. Sin urgencia con headroom 17x.
- Validación full del Player con compiledMesh: deuda menor heredada de F2H26.

**Próximo paso**: **TBD — definir con el dev**.

### F2H43 (anterior, ya cerrado)

**🚀 F2H43 cerrado: sistema de i18n completo (Editor + HUD + Player).**
Tag: `v1.33.0-fase2-hito43`.
Validado visualmente por dev *"cambia bien"* + *"todo va ok de momento"* tras tour completo en inglés y español. Switch live de idioma vía `Ver > Idioma > [English / Español]` que persiste en `%APPDATA%\MoodEngine\settings.json` (compartido Editor↔Player).

**Decisiones clave de F2H43:**

- **API namespaced `Mood::I18n::T("key")` (mismo patrón que `Mood::Log::*`)**, no clase singleton instanciable. Razón: el resto del proyecto usa funciones libres en namespaces para subsistemas globales del proceso (Log, GameState, ahora UserSettings + I18n). Coherencia + zero overhead vs `Singleton::instance()->T(...)`. La convención `T()` es corta a propósito — se llama 500+ veces y la brevedad importa.
- **Keys flat con dot notation**, no nested JSON (`"editor.menu.file": "Archivo"`, no `{"editor": {"menu": {"file": "Archivo"}}}`). Razón: lookup O(1) directo en `unordered_map<string, string>` sin recursión + grep-friendly + el _comment de metadata se mezcla con keys reales sin estructura especial.
- **Default Spanish (idioma del dev), fallback siempre Inglés**. Razón: el dev trabaja en español, arrancar en su idioma evita un click extra. Fallback inglés porque es el lingua franca de devtools y porque hay más probabilidad de que un colaborador internacional traduzca al inglés primero (las keys nuevas siempre van a en.json).
- **Diccionarios SIN tildes en `es.json`** (`MUNICION` en vez de `MUNICIÓN`, `RESISTENCIA` en vez de la versión correcta con acento). Razón: la font del MoodPlayer (ProggyClean default ImGui) no cubre Latin-1; los caracteres con tilde se verían como tofu (?) en gameplay. El editor (Lato F2H38) sí soporta Latin-1, pero mantenemos paridad para no tener UNA fuente de verdad partida en dos. Cuando se cambie el Player a Lato (deuda anotada), agregar tildes es un find-and-replace en es.json sin tocar código.
- **Persistencia GLOBAL del idioma** (`%APPDATA%\MoodEngine\settings.json`), no per-proyecto. Razón: el idioma es preferencia del USUARIO, no del proyecto — un colaborador brasileño abriendo un proyecto creado por vos esperaría su idioma. Abre la puerta a futuras settings globales (theme, recent-files límite, etc) sin meterlas dentro del `.moodproj`.
- **Iconos FontAwesome quedan FUERA del JSON**, se concatenan en código: `(std::string(ICON_FA_FOLDER " ") + I18n::T("editor.menu.file")).c_str()`. Razón: las constantes `ICON_FA_*` son `\uefxx` y meterlas en JSON requeriría escapado UTF-8 frágil + el icon NO depende del idioma (es universal).
- **Términos técnicos PBR (`metallic`/`roughness`/`albedo`/`ao`) NO se traducen**. Razón: nomenclatura PBR estándar internacional, los devs los reconocen en cualquier idioma + la API del MaterialAsset usa esos nombres como keys de schema (traducirlos rompería persistencia).
- **IDs internos de ImGui (popups/windows) usan `##nombre_modal`** en vez del título visible. Razón: si el title cambia entre idiomas, `BeginPopupModal` perdería identidad y el popup se cerraría/recrearía cada switch. El `##` prefix oculta el ID al usuario y mantiene identidad estable.
- **Logs (`Log::*->info/warn/error/debug`) NO se traducen** — quedan en inglés siempre. Razón: convención dev tools (los logs son para devs/colaboradores, no para usuarios finales) + son grep-friendly + no necesitan localización para Issue tracking.
- **Workspace names (`Layout`/`Programar`/`Materiales`/`Editor de mapas`) NO se traducen** en F2H43. Razón: viven en `WorkspaceManager.cpp` con el nombre como ID persistido en `.moodproj` (selección del workspace activo se guarda por nombre). Traducirlos requiere refactor: separar nombre interno (ID estable) del label mostrado. Diferido.
- **Subagente para barrido masivo del editor**: 23 archivos / 255 keys con misma transformación mecánica = caso de uso textbook de delegación. Bloque C1 (MenuBar) hecho manual primero como piloto del patrón → bloque C2 con subagente con la convención ya validada.

**Implementación (F2H43 Bloques A-E):**

- **Bloque A — infra i18n**: `engine/i18n/I18n.h/cpp` (singleton namespaced + JSON loader + fallback inglés + warn-once + interpolación fmt). 6 tests unitarios (init/fallback/missing/switch/fmt/ISO roundtrip). 11 keys ejemplo iniciales en `assets/i18n/{en,es}.json`.
- **Bloque B — selector + persistencia**: `core/UserSettings.h/cpp` con persistencia en `%APPDATA%\MoodEngine\settings.json`. Wire-up en EditorApplication_Init.cpp + PlayerApplication_Init.cpp (cargar settings + arrancar I18n con idioma persistido). Menú `Ver > Idioma > [English / Español]` en MenuBar con check del activo + save al click. 1 test extra del path resolver.
- **Bloque C1 — barrido MenuBar piloto** (~60 keys, manual): valida convención antes de escalar. Files/Map/Brush/Edit/View/Help submenús + Boolean ops + popups About/NotImpl + botones Play/Stop.
- **Bloque C2 — barrido editor con subagente** (~255 keys, 23 archivos): Hierarchy + Inspector (12 partials) + AssetBrowser + MaterialEditor + ScriptEditor + Console + LuaApi + Performance + Viewport + VisGroups + MapEditorTopBar + StatusBar + Toolbar + EditorUI (Welcome modal). Build verificado por subagente, suite re-corrida por mí.
- **Bloque D — HUD + Player + callers** (~20 keys, manual): GameOverlay HEALTH/AMMO/RESERVE/STAMINA/OBJECTIVE/PAUSED/CONTINUE/OPTIONS + EditorPlayMode/PlayerApplication_Frame exitLabel + PlayerApplication_SaveLoad menu (New Game/Load Game/Quit + dialogs).
- **Bloque E (este commit)**: docs + commits + tag + push.

**Pendientes conocidos** (post-F2H43):
- **Sub-fase 2.5 gameplay** (diálogos / quests / inventario).
- **Mini-map / Radar** (CoD/Fallout): requiere render-to-texture topdown del mundo cercano.
- **Themes alternativos del HUD** (Doom saturado / Fallout verde): requiere theme runtime + bindings Lua.
- **HUD diegetic 3D** (Pip-Boy / muñequera Metro): requiere FPS arms primero.
- **Cambiar font del MoodPlayer a Lato** (deuda crónica desde F2H38): permitiría agregar tildes a `es.json` sin tofu en gameplay. Fix chico, mismo patrón que F2H38 pero en `PlayerApplication_Init.cpp`.
- **Workspace names traducibles** (deuda F2H43): separar nombre interno persistido en `.moodproj` del label mostrado. Refactor mediano.
- **Lua scripts traducibles** (deuda F2H43): los string literals dentro de `assets/scripts/*.lua` (ej. `hud_demo.lua` con "Demo: explore the test map") quedaron sin envolver. Requiere binding `T("...")` desde Lua + sweep.
- **Console tooltip multilínea i18n** (deuda C2): tiene `ICON_FA_BUG` etc. concatenados al string literal, no triviales de pasar al JSON sin perder los icons.
- **Optimizaciones GPU side** (de F2H42): GPU timestamp queries / CSM / frustum cull shadow. Sin urgencia con headroom 17x actual.
- Validación full del Player con compiledMesh: deuda menor heredada de F2H26.

**Próximo paso**: **TBD — definir con el dev**.

### F2H42 (anterior, ya cerrado)

**🚀 F2H42 cerrado: optimización runtime (shadow map caching + VSync toggle).**
Tag: `v1.32.0-fase2-hito42`.
Validado por Tracy + CSV: stress scene (285 entidades / 17K tris) corre a **780 FPS sin VSync** (1.27 ms/frame), **17x mejor** que pre-fix. Cache hit rate del shadow: **99.996%** (1 record en 22317 frames de captura).

**Decisiones clave de F2H42:**

- **Shadow map caching por hash de escena, no invalidación dirty-flag.** Hash FNV-1a 64 incremental cada frame de (`lightDir` + `sceneCenter` + `sceneRadius` + `position/rotationEuler/scale/mesh id` por cada entidad con MeshRenderer). Si coincide con el frame anterior y `m_shadowMapValid`, skip `m_shadowPass->record(...)` y reusa la GLTexture existente. **Razón**: el ECS no tiene dirty flags por componente; trackearlos requeriría hookear todos los call sites que mutan transforms (Inspector, gizmos, Lua scripts, animaciones, físicas). Hash es O(N) con N entidades — barato (32 μs en stress) y captura **cualquier** cambio sin tocar call sites.
- **Off→on de shadows fuerza regenerado.** Si `shadowEnabled` pasó de false a true, `m_shadowMapValid=false` aunque el hash coincida. Sin esto, reactivar luz direccional con la misma escena reusaría una shadow map stale (o vacía si nunca se generó).
- **VSync toggle vive en el Performance panel, no en un menú global.** El panel ya es el lugar donde el dev mide FPS / frame_ms / draws — agregar el toggle ahí lo hace descubrible. Patrón request/consume idéntico a los spawners (`consumeVsyncToggleRequest` → EditorApplication aplica via `Window::setVSync`). Sync inicial del checkbox con el estado real del Window cubre el edge case del driver que rechace vsync al crear el contexto.
- **Skybox reorder probado y revertido.** Hipótesis: dibujar skybox post-PBR con depth=1 LEQUAL solo pinta donde la geometría no escribió, reduciendo overdraw. Resultado test4 (17913 frames): empate dentro del ruido (~744 vs ~776 fps). El "1.14 ms del skybox" no era trabajo del fragment shader sino GPU sync redistribuido. Revertido por: no aporta ganancia + rompe doc del SkyboxRenderer + convención estándar (skybox-first).
- **VSync ON por defecto, no OFF.** Mantenemos el default histórico — el toggle es para medir, no para producción. Sin VSync el GPU corre al 100% en escenas pequeñas tirando energía y calor sin razón.
- **Cierre temprano del hito.** A 780 FPS de headroom en stress real (200 cubos + 64 lights + Fox + CesiumMan + fuego + trigger Lua), no tiene sentido invertir más esfuerzo. Optimizaciones diferidas (GPU timing queries, CSM cascadas, frustum cull shadow pass): hito propio si emerge presión real.

**Implementación (F2H42 Bloques A-G):**

- **Bloque A**: handler `EditorApplication::processSpawnFullStressSceneRequest` en `DemoSpawners_Stress.cpp` que dispara los 8 spawners individuales. Menu item `Mapa > Spawn FULL STRESS SCENE (F2H42)` en `MenuBar.cpp`. CMake re-configurado con `MOOD_PROFILE=ON`.
- **Bloque B**: análisis Tracy `test1.tracy` (3212 frames) → top bottleneck `ShadowPass::record = 13.4 ms/frame (54.7%)`.
- **Bloque C**: `m_lastShadowSceneHash u64 + m_shadowMapValid bool` en `SceneRenderer.h`. Hash FNV-1a 64 + condicional skip de `record()` en `SceneRenderer_Render.cpp`. Wrappeado en `MOOD_PROFILE_SCOPE("ShadowPass::hash")` separado.
- **Bloque D**: re-medición `test2.tracy` → shadow record cae a 278 μs/frame (-98%). Cache hit rate 98%. Anomalía: Skybox sube a 13.8 ms/frame (GPU sync redistribuido por VSync absorbiendo).
- **Bloque E**: `Window::setVSync(bool)` + getter + miembro. PerformanceHudPanel checkbox + `consumeVsyncToggleRequest`. EditorApplication consume cada frame. Sync inicial en `EditorApplication_Init.cpp`.
- **Bloque F**: re-medición sin VSync `test3.tracy` (22317 frames) → **780 FPS / 1.27 ms-frame / 99.996% cache hit**. Validación HONESTA del fix.
- **Bloque H**: skybox reorder probado (`test4.tracy`) → empate, revertido.
- **Bloque G (este commit)**: docs + tag + push.

**Pendientes conocidos** (post-F2H42):
- **Sub-fase 2.5 gameplay** (diálogos / quests / inventario).
- **Sistema de i18n** (translation table + lookup en HUD strings, ahora que están unificadas).
- **Mini-map / Radar** (CoD/Fallout): requiere render-to-texture topdown del mundo cercano.
- **Themes alternativos del HUD** (Doom saturado / Fallout verde): requiere theme runtime + bindings Lua.
- **HUD diegetic 3D** (Pip-Boy / muñequera Metro): requiere FPS arms primero.
- **Optimizaciones GPU side** diferidas: timestamp queries, CSM cascadas, frustum cull shadow pass. Sin urgencia con headroom 17x actual.
- Validación full del Player con compiledMesh: deuda menor heredada de F2H26.

**Próximo paso**: **TBD — definir con el dev**.

### F2H41 (anterior, ya cerrado)

**🚀 F2H41 cerrado: 5 widgets HUD diferidos + 3 fixes laterales + i18n unificado.**
Tag: `v1.31.0-fase2-hito41`.
Verificado visualmente por dev: *"todo ok"* tras tour completo de los 5 widgets nuevos (StaminaBar / ObjectiveText / KillFeed / CompassBar / CRT scanline) + validación de los 3 fixes laterales (no-hover en Hierarchy durante Play, walk feel mejorado, spawn centrado en `(0,1.6,0)`) + unificación final i18n (todo el HUD en inglés post-feedback "no he visto cosas en español e ingles mescladas").

**Decisiones clave de F2H41:**

- **Widgets nuevos extienden el framework F2H39, no lo reescriben.** Cada uno es 4 cosas: (1) función libre `drawXxx(HudContext&)` en `GameOverlay.cpp`, (2) entry en `k_widgets[]`, (3) state en `HudState` si necesita, (4) bindings Lua si scripts lo controlan. Ese es el contrato del framework — agregar widgets nunca debería tocar más que esos 4 lugares.
- **Único cambio en arquitectura del framework**: `HudContext` gana `glm::vec3 cameraForward` (default `vec3(0,0,-1)` para tests) — necesario para CompassBar derive yaw. Callers (EditorPlayMode + PlayerApplication_Frame) leen `m_playCamera.forward()` y lo pasan al `draw()`.
- **CompassBar yaw via `atan2(forward.x, -forward.z)`**: convención `yaw=0=N=-Z`, `90=E=+X`, `180=S=+Z`, `270=W=-X`. Rango visible ±90° (180° total), tickmarks cada 15° con cardinales N/E/S/W destacados (1.5x altura + texto encima). Indicador central naranja apuntando abajo marca yaw actual. Auto-driven sin Lua bindings.
- **CRT scanline default OFF** — `widget_enabled` initializa con `crt_scanline=false` (los 12 demás son default true). Razón: efecto retro divisivo, dev decide si lo quiere por proyecto/escena vía `setWidget("crt_scanline", true)` desde Lua.
- **StaminaBar bypass si `max_stamina<=0`** — gameplay sin stamina no consume área de pantalla. Default `100/100` para que arranque visible si el dev no setea explícitamente.
- **KillFeed con `KillEntry { string text, ImU32 color, float ttl }`** — patrón ttl idéntico a `pickup_queue` de F2H39, pero con color custom por entry. Lifetime 4s (vs 2.5s pickup), cap visual 5. `pushKillColored(text, r, g, b)` permite diferenciar enemy types/headshots/etc.
- **3 fixes laterales descubiertos en validación:**
  - **Hover/pick spurious en Hierarchy panel durante Play Mode**: cursor SDL captured pero ImGui veía la pos OS warpeada al centro, marcando entries como hovered. Fix: `io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX)` en `EditorApplication::beginFrame` cuando `m_mode == Play && !GameState::paused()`. Off-screen pos → ImGui no hovera nada.
  - **Caminata "muy lenta con pasitos pegados"**: tuning del char controller — `k_walkSpeed 4.0→5.5 m/s`, `k_crouchSpeed 2.0→3.0`, headbob `freq 5.0→3.5 Hz` (paso ~1.6 m a 5.5 m/s = stride realista), `amp 0.04→0.05`. Aplicado en Editor + Player (paridad).
  - **Spawn no centrado**: legacy hardcoded `(-4.5, 1.6, 7.5)` cambiado a `(0, 1.6, 0)` en 4 lugares (Editor `m_playCamera` default + reset en `exitPlayMode`, Player `m_playCamera` default + reset en quickSave-Quit/SaveLoad). Convención del motor: spawn al centro del mapa.
- **i18n unificado a inglés post-feedback dev** (*"si a futuro tenemos seleccion de idioma no es lo ideal"*). Estado pre-fix: HUD mezclaba inglés (HEALTH/AMMO/STAMINA/PAUSED) y español (OBJETIVO/CONTINUAR/OPCIONES + exit labels callers + 2 strings demo Lua). Cambios: `OBJETIVO`→`OBJECTIVE`, `CONTINUAR`→`CONTINUE`, `OPCIONES`→`OPTIONS`, `"Salir al editor"`→`"EXIT TO EDITOR"`, `"Salir al menu"`→`"EXIT TO MENU"`, demo Lua `"Demo: explorar el mapa de pruebas"`→`"Demo: explore the test map"`, `"[E] Levantar item demo"`→`"[E] Pick up demo item"`. Sienta baseline para futura selección de idioma — los string literals serán keys de translation table sin tocar lógica del HUD.

**Implementación (F2H41 Bloques A-H):**

- **Bloque A**: plan en [`archive/plans/PLAN_HITO_F2H41.md`](archive/plans/PLAN_HITO_F2H41.md).
- **Bloque B**: expandir `HudState` (stamina/max_stamina, objective_text, kill_feed deque, KillEntry struct) + `HudContext.cameraForward`. Helpers nuevos en `GameState::*` (no GameOverlay) para que LuaBindings linkee en `mood_tests`.
- **Bloque C**: 5 widgets en `GameOverlay.cpp` + agregados al registry `k_widgets[]`. Total 13 widgets activos.
- **Bloque D**: bindings Lua expandidos (10 funciones nuevas en tabla `hud`).
- **Bloque E**: callers (EditorPlayMode + PlayerApplication_Frame) pasan `m_playCamera.forward()` al `HudContext`.
- **Bloque F**: `hud_demo.lua` extendido con timers para los 5 nuevos.
- **Bloque G**: build + validación visual con dev + 3 fixes laterales reactivos al feedback + unificación i18n reactiva al feedback final.
- **Bloque H (este commit)**: docs + tag.

**Pendientes conocidos** (post-F2H41):
- **Optimización runtime sobre la PC de escritorio** (era F2H39 original, postergado al hardware del baseline F2H2-F2H6). Se ataca cuando el dev esté en la desktop.
- **Mini-map / Radar** (CoD/Fallout): requiere render-to-texture topdown del mundo cercano. Hito propio mediano.
- **Themes alternativos del HUD** (Doom saturado / Fallout verde): requiere theme runtime + bindings Lua. Hito chico propio.
- **HUD diegetic 3D** (Pip-Boy / muñequera Metro): requiere FPS arms primero. Hito propio mayor futuro.
- **Sistema de i18n** (translation table + lookup en HUD strings, ahora que están unificadas).
- **Sub-fase 2.5 gameplay** (diálogos / quests / inventario).
- Validación full del Player con compiledMesh: deuda menor heredada de F2H26.

**Próximo paso**: **TBD — definir con el dev**.

### F2H40 (anterior, ya cerrado)

**🚀 F2H40 cerrado: Fix físicas Floor scale-RigidBody desync.**
Tag: `v1.30.0-fase2-hito40`.
Verificado por dev: *"bien ya funciona"* tras crear proyecto nuevo + enlargar Floor.scale.x 2.5x + Play → quedó parado en el suelo (pre-F2H40 caía infinito).

**🏁 HUD framework + físicas robustas (Floor / brushes / walls escalables sin desync) + arquitectura preparada para más widgets HUD y diegetic 3D futuros.** 38/44 hitos de Fase 2.

**Decisiones clave de F2H40:**
- **Auto-sync `halfExtents` desde `Transform.scale` solo para Box bodies**. Sphere/Capsule mantienen halfExtents independiente — el campo significa cosas distintas (radio, altura) que no escalan uniformemente desde un `Transform.scale` potencialmente no-uniforme.
- **`setBodyHalfExtents` (existente) en lugar de destroy+recreate**: preserva pose + velocity + contacts del body. Importante para Dynamic bodies escalados mid-frame (script Lua o gizmo en Editor Mode).
- **Cache `lastSyncedHalfExtents` en `RigidBodyComponent`** para detectar desync. Inicializado en `vec3(0)` para forzar primer sync al materializar el body. NO se serializa (estado runtime).
- **Pase de re-sync corre cada frame** después del materializar inicial. Overhead negligible (epsilon compare de 3 floats por entity con RigidBody).
- **Aplica también a `PlayerApplication::updatePhysics`** (paridad con Editor) por scripts Lua que muten Transform o saves cargados con scale enlargado.
- **Cubre 2 vectores**: scale via gizmo/Inspector + halfExtents editado directo en Inspector (Sphere/Capsule).

**Implementación (F2H40 Bloques A-D):**

- **Bloque A**: plan en [`archive/plans/PLAN_HITO_F2H40.md`](archive/plans/PLAN_HITO_F2H40.md).
- **Bloque B**: campo `lastSyncedHalfExtents` en `Components.h` + pase de re-sync en `EditorScene.cpp::updateRigidBodies` y `PlayerApplication_Frame.cpp::updatePhysics`.
- **Bloque C**: build OK + dev valida tras enlargar Floor.
- **Bloque D (este commit)**: docs + tag.

**Pendientes conocidos** (post-F2H40):
- **Optimización runtime sobre la PC de escritorio** (era F2H39 original): pendiente en hardware del baseline F2H2-F2H6. Se ataca cuando el dev esté en la desktop.
- **HUD widgets diferidos** (CompassBar, ObjectiveText, KillFeed, Stamina, Mini-map, CRT scanline, themes alternativos): cada uno se agrega extendiendo el registry de widgets en `GameOverlay.cpp`.
- **HUD diegetic 3D** (Pip-Boy / Metro muñequera / Doom plasma rifle display): requiere FPS arms primero. Hito propio mayor futuro.
- **Sub-fase 2.5 gameplay** (diálogos / quests / inventario).
- Validación full del Player con compiledMesh: deuda menor heredada de F2H26.

**Próximo paso**: **TBD — definir con el dev**.

### F2H39 (anterior, ya cerrado)

**🚀 F2H39 cerrado: HUD framework extensible + paquete inicial estilo HL/Doom/Fallout.**
Tag: `v1.29.0-fase2-hito39`.
Verificado visualmente por dev: *"los widgets se ven bien, vi la mayoria, validalo, cerra y vamos a lo siguiente"*. HEALTH naranja+barra, AMMO mag/reserve+ícono procedural de bala, crosshair, hit marker cyan, damage vignette rojo direccional, pickup notifications, interact prompt, pause menu Doom-style con título "PAUSED" gigante.

**🏁 HUD framework extensible activo + 8 widgets estilo HL/Doom/Fallout + arquitectura para CompassBar/ObjectiveText/KillFeed/HUD diegetic 3D futuros.** 37/44 hitos de Fase 2.

**Decisiones clave de F2H39:**
- **Sustituye al F2H39 original "optimización runtime"**: pospuesto a hito futuro sobre la PC de escritorio del dev (GTX 1660 / Ryzen 5 5600G del baseline F2H2-F2H6). La notebook actual (Iris Xe) daría números no comparables. PERFORMANCE.md ya marca que sub-fase 2.1 cumplió: *"el motor está listo para contenido real"* — los próximos hitos son features.
- **Half-Life como estética base + influencias Doom/Fallout/Metro/CoD**. Paleta naranja Valve `#F58220` + amarillo HL `#FFCC00` + cyan hit marker `#10F0FF` + rojo damage `#FF3030`. Tipografía grande para HP/AMMO (font scale 2.8x).
- **Widget framework extensible** (`HudWidget { name, draw_fn }` + registry + `HudContext` struct), no clase con vtable. Razón: ~10 widgets → vtable overhead innecesario; función pura es testeable + sin estado oculto. Custom widgets futuros (Lua-driven, diegetic 3D) extienden la lista.
- **Helpers de mutación viven en `GameState`, no `GameOverlay`**. Razón: `LuaBindings` necesita invocar `triggerHitMarker / pushPickup / etc` y `GameOverlay` depende de ImGui. Mover los helpers a `GameState` permite que el módulo de tests (que NO linkea ImGui) compile LuaBindings sin `mood_tests.exe` con unresolved externals.
- **`pickup_queue` usa `ttl` countdown** (no `spawnTime` absoluto). Razón: evitar dependencia de `ImGui::GetTime()` en `GameState::pushPickup` — el overlay decrementa el ttl con `dt` cada frame y popea cuando ttl≤0.
- **Lua bindings: 18 funciones nuevas en la tabla `hud`** preservando las 6 originales del Hito 20. State-based (no draw calls directos) — el patrón existente del Hito 20 escala perfecto al framework expandido.
- **Pause menu rediseñado Doom-style**: título "PAUSED" font scale 4x + 3 botones con border geométrico + chevrons en las 4 esquinas en hover. Sustituye el rectángulo gris-azul plano del Hito 20.
- **SaveLoad backward-compat**: campos nuevos opcionales en JSON. Saves pre-F2H39 cargan con defaults. Transient state (timers, queue) NO persiste.
- **Diferidos explícitamente** (anotados en PENDIENTES como hito propio futuro): CompassBar (Skyrim/CoD), ObjectiveText (HL2/CoD), KillFeed (Doom/CoD), Stamina/MagicBar (Skyrim/Metro), Mini-map/Radar (CoD/Fallout), CRT scanline overlay (Fallout Pip-Boy), themes alternativos (Doom paleta saturada / Fallout verde), HUD diegetic 3D (Pip-Boy / Metro muñequera / Doom plasma rifle display — requiere FPS arms primero).

**Implementación (F2H39 Bloques A-G):**

- **Bloque A**: plan en [`archive/plans/PLAN_HITO_F2H39.md`](archive/plans/PLAN_HITO_F2H39.md).
- **Bloques B-E unificados**: arquitectura widget framework + 8 widgets implementados + bindings Lua + SaveLoad + demo lua extendido.
- **Fix lateral SceneLoader**: auto-RigidBody Static al Floor cargado sin uno (proyectos pre-Hito 12). Cubre escenarios donde el `.moodproj` viejo perdía físicas del piso.
- **Bloque F — validación visual**: dev confirma widgets renderean bien.
- **Bloque G (este commit)**: docs + tag.

**Pendientes conocidos** (post-F2H39):
- **Optimización runtime sobre la PC de escritorio** (era F2H39 original, postergado al hardware del baseline F2H2-F2H6). Se ataca cuando el dev esté en la desktop.
- **Bug físicas Floor scale-RigidBody desync** (descubierto durante validación F2H39): cuando el dev cambia `Transform.scale` del Floor en Inspector o gizmo, el `RigidBody.halfExtents` no se sincroniza automáticamente — el visual cambia pero la colisión no. Player puede caer fuera del body. Fix lateral propio si emerge presión real.
- **HUD diegetic 3D** (Pip-Boy / Metro muñequera): requiere FPS arms (mesh + animator del brazo) primero. Hito propio mayor futuro.
- **Más widgets HUD de la lista diferida**: CompassBar, ObjectiveText, KillFeed, Stamina, Mini-map, CRT scanline overlay.
- **Sub-fase 2.5 gameplay** (diálogos / quests / inventario).
- Validación full del Player con compiledMesh: deuda menor heredada de F2H26.

**Próximo paso**: **TBD — definir con el dev**.

### F2H38 (anterior, ya cerrado)

**🚀 F2H38 cerrado: Default font ImGui a Lato.**
Tag: `v1.28.0-fase2-hito38`.
Verificado visualmente por dev: *"todo se ve perfecto"*. Lato a 15px legible inmediato; Console text-heavy es donde más se nota el upgrade. Sin re-flow / overflow / truncate en ningún panel.

**🏁 Hammer Editor cerrado funcional al 100% + iconos FontAwesome en TODO el editor + polish UX consolidado + tipografía profesional con Lato.** 36/44 hitos de Fase 2.

**Decisiones clave de F2H38:**
- **Lato Latin Regular a 15px** como default font del editor. Razón: ProggyClean es bitmap monoespaciada de 13px diseñada para terminales — pixely y poco kerning para UIs modernas; Lato es sans-serif TTF diseñada para pantalla, smooth scaling. 15px matchea convención de IDEs modernos (VSCode/JetBrains).
- **Custom GlyphRanges para Lato** que cubra Basic Latin + Latin-1 Supplement + General Punctuation subset (`{0x0020, 0x00FF, 0x2010, 0x2027, 0}`). General Punctuation incluye em-dash U+2014, en-dash U+2013, ellipsis U+2026, comillas curvas U+2018-201D — todos los caracteres "naturales" del español que ProggyClean no cubría.
- **FA merge a 13px explicit** (no más 0.0f implicit). ImGui 1.92 obliga a que primary y merge compartan convención de reference size; Lato a 15.0f explicit fuerza a FA también explicit. 13px ≈ 85% del texto = proporcional sin dominar.
- **`PixelSnapH = false` en Lato** (smooth scaling de sans-serif), `true` en FA (icons monocromos se benefician del snap a pixel).
- **NO se revierte el fix em-dash de F2H37** (`—`→`-` en EditorUI.cpp). Razón: el `-` es universal Latin, funciona con cualquier font. Mantener el fix evita reintroducir dependencia en el coverage de Lato si alguien cambia la font en el futuro.
- **NO se toca el MoodPlayer** — usa init propio (`PlayerApplication_Init.cpp`), sigue con ProggyClean. Fix chico posterior si emerge presión de coherencia visual.

**Implementación (F2H38 Bloques A-E):**

- **Bloque A**: plan en [`archive/plans/PLAN_HITO_F2H38.md`](archive/plans/PLAN_HITO_F2H38.md).
- **Bloque B**: edit puntual en `EditorApplication_Init.cpp` (~25 LOC neto).
- **Bloque C**: tour visual end-to-end. Sin issues.
- **Bloque D skipped** — todo OK al primer arranque.
- **Bloque E (este commit)**: docs + tag.

**Pendientes conocidos** (post-F2H38):
- **F2H39 — optimización runtime** (definido con el dev). Plan emergerá de un Bloque A de profiling Tracy + Performance HUD sobre escenas reales (la infra existe desde F2H2). Candidatos sin profilar todavía: skinned animation pass, particle update, brush mesh rebuild on dirty, físicas con N rigidbodies, audio 3D attenuation con N sources.
- **Cambiar font del MoodPlayer a Lato** (coherencia visual Editor↔Player) — fix chico, hito propio si emerge presión.
- **HUD procedural/minimalista MoodPlayer** (interés post-F2H35) — diferido tras F2H39.
- Validación full del Player con compiledMesh: deuda menor heredada de F2H26.

**Próximo paso**: **F2H39 — optimización runtime**. Plan en `docs/PLAN_HITO_F2H39.md` cuando arranquemos. Bloque A será profiling — el plan emerge de los datos.

### F2H37 (anterior, ya cerrado)

**🚀 F2H37 cerrado: FontAwesome icons en el resto del editor + polish UX general.**
Tag: `v1.27.0-fase2-hito37`.
Verificado visualmente por dev: MenuBar (los 6 menus + workspace tabs + Play/Stop con icon), Hierarchy (icon FA por tipo + multi-select polish naranja/amarillo/gris), VisGroupsPanel (consolidado al helper compartido), Inspector (icons en headers de 13 components), AssetBrowser (icons en 6 tabs), Console (icons por nivel + level filter toggles), StatusBar (FPS/mode/sub-mode con icons). Editor arranca sin asserts. Tour visual end-to-end OK.

**🏁 Hammer Editor cerrado funcional al 100% + iconos FontAwesome en TODO el editor + polish UX consolidado.** 35/44 hitos de Fase 2.

**Decisiones clave de F2H37:**
- **Hito unificado**: fusiona "extender FontAwesome al resto del editor" (post-F2H36) + "Pase de polish UX general continuo" (anotado desde F2H21+F2H22). Mismos paneles tocados → un solo hito sin doble pasada. Validado por dev al pedir el scope unificado.
- **Header `IconHelpers.h` separado de `IconsFontAwesome6.h`**. Razón: `IconsFontAwesome6.h` queda como "tabla pura de macros UTF-8" sin dependencias de scene/components; `IconHelpers.h` agrega los helpers que dispatchean sobre presencia de componentes (depende de `Components.h`/`Entity.h`). Inline en el header — no requiere .cpp porque es un switch puro.
- **`iconForEntity(Entity)` consolida `entityIconStr` duplicado** que vivía en HierarchyPanel + VisGroupsPanel pre-F2H37. Mismo orden de prioridad (MeshRenderer > Brush > Light > Audio > Script > Trigger > Camera > Particle > sin componente). Cualquier hito futuro que agregue un nuevo component type renombra/extiende este helper en un solo lugar.
- **Polish multi-select del Hierarchy con 3 colores**: naranja (active), amarillo claro (en seleccion pero no active), gris (hidden por VisGroup). Pre-F2H37 las secundarias se veían iguales al ImGui default — el dev tenia que abrir el Inspector para saber cuál era la primary.
- **Console level filter con 6 toggles SmallButton** en lugar de un dropdown. Razón: el dropdown obliga a un click + open + click; los toggles permiten activar/desactivar varios niveles con clicks puntuales. Cada toggle muestra el icon + tinte coloreado cuando ON, tenue cuando OFF — el estado se ve sin abrir nada. Estado persistido en `m_levelEnabled[6]` del header del panel (no en `.moodproj` — es un toggle ergonomico de sesión, default a "mostrar todos").
- **Rows del AssetBrowserPanel sin icon-por-row**. Razón: cada tab es type-pure (todos meshes en Meshes), agregar icon a cada row sería redundante con el icon del tab. El audit inicial lo proponía pero al implementar emergió que era ruido visual.
- **InspectorPanel_Brush.cpp upgrade TextDisabled → SeparatorText** durante el pase. Razón: los demás partials usan SeparatorText (F2H23 convention); el `TextDisabled("Brush (CSG)")` original era inconsistente. Fix gratis al estar tocando el header.
- **Fix lateral em-dash tofu en Welcome modal**: el carácter `—` (U+2014, General Punctuation) no está en ProggyClean ni en mi `k_iconRange` (FA Private Use Area). Pre-F2H36 ya se veía como `?` pero nunca se notó hasta el tour visual del Bloque I. Fix mínimo: reemplazar por `-` (hyphen-minus, U+002D, Basic Latin). Cambiar la default font a Lato (que ya está en `assets/ui/fonts/`) sería scope mayor — hito propio si emerge.
- **NO se agrega icon-por-tipo en cada row del AssetBrowser** (audit inicial lo sugería). Razón: cada tab es type-pure, redundante.
- **Width del botón Play/Stop bumped 64→80 px** para acomodar `ICON_FA_PLAY " Play"` / `ICON_FA_STOP " Stop"`.

**Implementación (F2H37 Bloques A-J en commits):**

- **Bloque A**: plan en [`archive/plans/PLAN_HITO_F2H37.md`](archive/plans/PLAN_HITO_F2H37.md).
- **Bloques B-H unificados**: TTF asset ya existía (F2H36) → solo extender header + crear helper + aplicar a 7 paneles + polish.
- **Fix lateral em-dash** (Bloque I validación): 3 ocurrencias en `EditorUI.cpp`.
- **Bloque I**: build + validación visual end-to-end con dev.
- **Bloque J (este commit)**: docs + tag.

**Pendientes conocidos** (post-F2H37):
- **TBD — definir con el dev**. Opciones:
  - Sub-fase 2.5 gameplay (diálogos / quests / inventario).
  - HUD procedural/minimalista del MoodPlayer (interés expresado post-F2H35).
  - Otra sub-fase del PLAN_FASE2 (optimización / runtime).
- **Cambiar default font ImGui a Lato** (existe en assets pero no se carga): nice-to-have. Resolvería el tofu del em-dash y mejoraría legibilidad general. Pero requiere revisar todo el editor para verificar que el spacing/alineamiento siguen OK con la métrica nueva. Hito propio si emerge.
- Validación full del Player con compiledMesh: deuda menor heredada de F2H26.

**Próximo paso**: **TBD — definir con el dev**.

### F2H36 (anterior, ya cerrado)

**🚀 F2H36 cerrado: FontAwesome icons en toolbars del editor de mapas.**
Tag: `v1.26.0-fase2-hito36`.
Verificado visualmente por dev: los 17 botones del Toolbar lateral (Mover/Rotar/Escala/Box/Cilindro/Cara) + MapEditorTopBar (Selecc./Bloque/Pincel/Clip/Objeto/Vertex/Edge/Cara/Snap V/Nombres/Carve) renderean con icono FA + label castellano. Editor arranca sin asserts. Mini-hito chico que cierra deuda arrastrada desde F2H22.

**🏁 Hammer Editor cerrado funcional al 100% + iconos en toolbars del workspace de mapas.** 34/44 hitos de Fase 2.

**Decisiones clave de F2H36:**
- **FontAwesome 6 free solid (`fa-solid-900.ttf`, ~417 KB)** descargada del repo oficial `FortAwesome/Font-Awesome` rama `6.x` al path `assets/ui/fonts/`. Asset estático commiteado al repo (no se regenera, no requiere build step).
- **Header `IconsFontAwesome6.h` con subset de ~15 macros**, no el header full de ~2000. Macros encoded en UTF-8 con escapes hex (`"\xef\x86\xb2"`) para no depender de la code page del source MSVC. Agregar un icono nuevo en hitos futuros = extender este header explícitamente (no autoimport).
- **Merge con default font (ProggyClean) usando `0.0f` como SizePixels**, no `13.0f`. Razón: ImGui 1.92 introdujo asserts que verifican que el merge use el mismo "reference size" que la dst font (implicit en `AddFontDefault`). Pasar 13.0f explicit triggera assert. Patrón confirmado en `imgui-src/docs/FONTS.md`.
- **GlyphMinAdvanceX dropeado** (override de mono-spacing). Razón: ImGui 1.92 tira otro assert si combinas glyph advance overrides + size 0.0f. Default rendering es suficiente — los iconos quedan al alto natural alineados con el texto.
- **Width del Toolbar bumped 72→92 px** para acomodar icon + label castellano sin truncar (`ICON_FA_CIRCLE " Cilindro"` no entra en 72 px).
- **Scope explícitamente acotado a los 2 toolbars del workspace "Editor de mapas"**. MenuBar / Hierarchy / Inspector / AssetBrowser / Console / StatusBar / paneles VisGroups/Material/Script quedan diferidos a F2H37 dedicado. Decisión alineada con la convención "un hito = un dominio acotado". Validado por dev al pedir continuar: *"deberemos integrarlos en otras areas del proyecto para que todo sea equitativo"*.

**Implementación (F2H36 Bloques A-E en commits feat + cierre):**

- **Bloque A**: plan en [`archive/plans/PLAN_HITO_F2H36.md`](archive/plans/PLAN_HITO_F2H36.md).
- **Bloques B+C unificados**: TTF asset + `IconsFontAwesome6.h` + merge en `EditorApplication_Init.cpp` + iconos en `Toolbar.cpp` + `MapEditorTopBar.cpp`.
- **Bug fix iter 2 — ImGui 1.92 asserts**: dropear PixelSnap typo (era PixelSnapH) + pasar 0.0f size al merge + dropear GlyphMinAdvanceX. 3 fixes acoplados resueltos en una sola iteración tras lanzar el editor 2 veces.
- **Bloque D — validación visual**: dev confirma iconos OK en los 17 botones, sin tofu.
- **Bloque E (este commit)**: docs + tag.

**Pendientes conocidos** (post-F2H36):
- **F2H37 — extender FontAwesome al resto del editor** (próximo, pedido explícito del dev): MenuBar (Archivo / Editar / Ver / Ayuda + tabs Layout/Scripting/...), Hierarchy con icon-por-tipo de entity (cube=mesh, lightbulb=light, music=audio, video=camera, etc.), Inspector con icons en headers de componentes, AssetBrowser con icons por tipo de asset (image=textura, music=audio, file-code=script, etc.), Console con icons por nivel (info/warn/error), StatusBar, paneles VisGroups/Material/Script. Plan a redactar al arrancar.
- **HUD del juego procedural/minimalista** (interés expresado post-F2H35): MoodPlayer con HUDs estilo Mirror's Edge / Doom Eternal vía ImGui DrawList o shaders custom. Diferido tras F2H37.
- Validación full del Player con compiledMesh: deuda menor heredada de F2H26.

**Próximo paso**: **F2H37 — extender FontAwesome al resto del editor**. Plan en `docs/PLAN_HITO_F2H37.md` cuando arranquemos.

### F2H35 (anterior, ya cerrado)

**🚀 F2H35 cerrado: Polish editor (UX viewport + Hammer-style visual).**
Tag: `v1.25.0-fase2-hito35`.
Verificado por dev end-to-end: editor arranca maximizado con dockspace balanceado (sin "Restablecer layout" manual); brushes en VisGroup tintan su color en los 3 ortos; point entities por tipo (Light amarillo / Audio naranja / Trigger verde / Camera azul / Particle violeta) con cubitos en orto + iconos 2D en perspective + labels flotantes con tag (toggle "Nombres" persistido); face picking muestra hover preview cyan antes de clickear; gizmo Rotate constante en pantalla (igual que Translate/Scale).

**Decisiones clave de F2H35:**
- **Editor maximizado via `SDL_GetDesktopDisplayMode` + `SDL_WINDOW_MAXIMIZED`** en lugar de 1280x720 + maximize async. Razón: `SDL_GetWindowSize` en el primer frame devolvía 1280x720 stale, el rebuild del Dockspace usaba ese WorkSize, los splits se persistían como offsets absolutos al ini → dockspace descuadrado al maximizar la ventana real. Crear directamente al display garantiza dimensiones correctas desde el frame 0.
- **Stamp `k_IniLayoutStamp = 1` per-proyecto.** El bumpear `imgui_layout_v3.ini` cubre el ini global, pero los `.moodproj` también guardan `iniLayout` por workspace (F2H7). Stamp invalida los iniLayouts persistidos cuando el dockspace builder cambia (mismo patrón que el ini global). Ausente = legacy 0 → invalidar y rebuild fresh con WorkSize correcto. Sin esto los proyectos viejos seguían descuadrados aunque la ventana arrancara maximizada.
- **Tint VisGroup color solo en wireframe orto, NO en perspective.** Perspective ya renderea PBR completo (no wireframe excepto outlines de selected); tintar el wireframe del VisGroup tendría sentido solo en orto donde todo es wireframe. Mantiene consistencia: tipo va al perspective via icon, organización (VisGroup) va al orto via wireframe color.
- **Cubitos point entity en orto: tamaño FIJO (r=0.4)**, NO proporcional al snap. Razón: con snap=64 los cubitos inflaban la vista; con snap=1 desaparecían. `r=0.4` matchea `k_iconPickRadius=0.6` de ScenePick para que el área visible coincida con el área pickable (sin sorprender al dev "lo veo pero no lo agarro").
- **Hover preview de face picking en cyan brillante** `(0.10, 0.95, 1.00)`. Probado: blanco tenue inicial desaparecía sobre texturas claras (probado contra textura blanca tilada del proyecto del dev). Cyan saturado contrasta con amarillo (active selected) y naranja (secondary selected) — el dev distingue hover-preview vs ya-seleccionado a primera vista.
- **Gizmo Rotate constante en pantalla = Translate/Scale.** Pre-F2H35 era `0.6 * max(localAabb)` world-space (F2H30 Bloque D) — al alejar la cam se hacía chico mientras los handles de Translate/Scale se mantenían a 60 px. Fix: `worldRadius = TARGET_PX / pixelsPerWorld` derivado de `(h/2) / (camDistance * tan(fovY/2))`. Target 70 px (mayor que k_armLen=60 para que el ring rodee los handles sin solaparse).
- **Toggle "Nombres" default ON, persistido por proyecto.** Default OFF (Hammer original) sería más alineado con la convención clásica, pero el dev pidió explícitamente *"labels default On"*. Persistencia opcional en `.moodproj` solo si != default (mismo patrón que coyoteWindowSec del Hito 40 G — los `.moodproj` viejos no se ensucian).
- **Detalles por tipo dentro del cubito orto descartados.** En iteración inicial agregué rayos/X/diagonal/frustum/burst dentro del cubo para diferenciar tipos sin label. Feedback dev: *"demasiado grande, como lo hace el hammer editor de valve?"*. Hammer Source clásico usa cubito chico + label de texto. La diferenciación fina viene del label (nombre de la entidad), no de la forma del icono — eso queda para Hammer-Source 2 / Hammer++ (FontAwesome icons, hito futuro).

**Implementación (F2H35 Bloques A-G en 9 commits feat/fix + cierre):**

- **Bloque A** (`a7c6484`): plan archivado.
- **Bloque B** (`cdf1cff` + `d0ea5e2`): editor maximizado + 3 fixes layout descuadrado.
- **Fix lateral** (`1734537`): VisGroupsPanel ColorPicker live-preview.
- **Bloque C** (`befdd29`): tint wireframe brushes por VisGroup color en ortos.
- **Bloque D** (`88bdba8`): paleta por tipo + iconos 2D perspective + cubitos orto + fix pickable Trigger/Camera/Particle.
- **Bloque E** (`d5c8da1`): labels point entities + toggle "Nombres" + feedback selección orto.
- **Bloque E.5** (`db1823a`): fix gizmo Rotate proporcional a cam.
- **Bloque F** (`4cf5e89`): hover preview face picking cyan.
- **Persistencia toggle** (`c44383f`): `Project::showEntityLabels` opcional.
- **Bloque G** (este commit): docs + tag.

**Pendientes conocidos** (post-F2H35):
- **Iconos image-based del Toolbar** (FontAwesome merge): pendiente desde F2H22, ahora reforzado por el feedback de iconos de point entities. Bajo riesgo, alto impacto visual. Probable hito chico futuro.
- **Hover preview en orto** (no perspective): F2H35 Bloque F solo cubre perspective; los ortos ya usan wireframe color del Bloque C que es feedback razonable, pero pintar la cara hovered cyan en orto requeriría pasarle el cursor del orto al pickFace y dibujar overlay específico. Diferido — emerge si el dev lo pide.
- **VisGroups jerárquicos / drag desde Hierarchy / lock**: features avanzadas Hammer 4. Diferidas hasta que emerja necesidad real.
- Validación full del Player con compiledMesh: deuda menor heredada de F2H26.

**Próximo paso**: **TBD — definir junto al dev**. Hammer Editor cerrado funcional al 100% + polish viewport + visual style. Opciones a considerar:
- Sub-fase 2.5 gameplay (diálogos / quests / inventario).
- Otra sub-fase del PLAN_FASE2 (optimización / runtime / polish UI general).
- Hammer-Source 2 polish (FontAwesome icons + posibles improvements).

### F2H34 (anterior, ya cerrado)

**🚀 F2H34 cerrado: Multi-face material drop.**
Tag: `v1.24.0-fase2-hito34`.
Verificado por dev: face mode → shift+click N caras → drop textura o material → las N caras reciben el material en una sola operación.

**Decisiones clave de F2H34:**
- **Sin command class nuevo, extender el existente.** `EditBrushFaceMaterialCommand` cambió `u32 faceIndex` + `u32 oldFaceMatIndex` + `u32 newFaceMatIndex` por `vector<u32>` paralelos. Constructor 1-cara preservado como wrapper que rellena vectores tamaño 1 — back-compat total con call-sites pre-existentes y los 7 tests del F2H19. Razón: sumar `EditBrushFacesMaterialCommand` (plural) sería duplicar 90% del código y forzar a las llamadoras a elegir entre dos clases con la misma intención.
- **Snapshot único de `bc.materials` compartido entre N caras.** Cuando el material es nuevo, un solo `push_back` al vector y todas las caras del set apuntan al mismo slot. Sin esto cada cara podría duplicar el slot al apply, inflando `bc.materials` y rompiendo el undo (el snapshot post se desincroniza con el real).
- **Validación atómica de faceIndices en `apply`** (todo o nada): si un solo faceIndex está fuera de rango, no muta nada. Garantiza que un command corrupto nunca deje el brush en estado parcial. Cubierto por test "faceIndex fuera de rango en una cara → no muta nada".
- **Helper `tryAssignMaterialToSelectedFaces` en namespace anónimo** del `DemoSpawners_Drop.cpp`, no en EditorApplication. Razón: la lógica solo existe en el flow de drop (texture + material drop comparten 100%); poner el helper en el header de EditorApplication la expondría a otros call-sites que no la necesitan. Devuelve `nullptr` cuando no aplica → llamadora cae al flow object-mode existente sin if extra.
- **Label dinámico singular/plural** según `selectedFaceIndices.size()`. "Asignar textura a cara" vs "Asignar textura a caras" en Editar > Deshacer.
- **NO se incluyó fix de "abrir maximizado" ni toggle wireframe/render** en este commit: el dev pidió no contaminar el cierre de F2H34 con cambios fuera de scope. Ambos quedan en F2H35 como mini-hito UX viewport propio.

**Implementación (F2H34 en 2 commits feat + 1 commit docs/cierre):**

- **Commit `<hash>` — feat(editor): EditBrushFaceMaterialCommand multi-cara**: extensión a vectores + constructor 1-cara wrapper + validación atómica + 5 tests nuevos.
- **Commit `<hash>` — feat(editor): drop textura/material aplica a multi-cara**: helper `tryAssignMaterialToSelectedFaces` + refactor de los 2 call-sites (texture + material drop) + label dinámico singular/plural.
- **Commit `<hash>` — docs(F2H34 cierre)**: HITOS / ESTADO / DECISIONS / PENDIENTES.

**Pendientes conocidos** (post-F2H34):
- **F2H35 — UX viewport polish (mini-hito)**: (1) abrir editor maximizado por defecto (~one-liner SDL flag); (2) toggle wireframe/render shading en viewport con botones overlay estilo Blender (~1-2h, toca render pipeline + un flag de mode). Identificado en validación F2H34.
- **Resto de "Hammer-style visual polish"** (mismo mini-hito o aparte): tint del wireframe por color del VisGroup, color por tipo de entity, labels arriba de point entities.

**Próximo paso**: **F2H35 — UX viewport polish**. Empezar por el fix de maximizado (trivial) + decidir si el toggle wireframe/render entra en el mismo hito o se separa.

### F2H33 (anterior, ya cerrado)

**🚀 F2H33 cerrado: Organización + face polish (VisGroups + multi-select de caras + texture alignment).**
Tag: `v1.23.0-fase2-hito33`.
Verificado por dev: VisGroups crear/asignar/toggle/eliminar funciona end-to-end; persistencia v13→v14 round-trip OK; face mode con multi-select via Shift+click pickea cualquier brush hovered en 1 click + active en amarillo / secundarias en naranja; alignment ops funcionan single y multi.

**Decisiones clave de F2H33:**
- **Schema bump aditivo v13→v14** mismo patrón que F2H26 v12→v13: array opcional `visgroups` top-level + campo opcional `visgroupId` por entity y brush. Mapas v13 cargan como v14 sin pérdida (array vacío + sin membership).
- **VisGroups planos (no jerárquicos)** alineados con Hammer 4 — sub-grupos diferidos si emerge demanda real. Membership 1-a-N (entity en máximo 1 grupo); refactor a `unordered_set<u64>` si emerge.
- **Player ignora VisGroups**: convención Hammer = VisGroups son del editor, el build final incluye todo. `applyEntitiesToScene(useCompiledMesh=true)` skipea `resetVisGroups` y no agrega `VisGroupMembershipComponent`. Si el dev oculta una torre para trabajar y luego prueba en Player, la torre se ve igual.
- **Refactor `SelectionSet`** breaking-internal: campo `i32 activeFaceIndex` (F2H17) → `std::vector<i32> selectedFaceIndices` + método `activeFaceIndex()` derivado de `back()`. Invariante: el último del vector es siempre la "active" (= primary para ops single-face). 7 call-sites migrados.
- **Face picking robust** (fix UX descubierto en validación): pre-F2H33 el pickFace solo testeaba el brush active → clickear otro brush requería 2 clicks. Fix: `pickEntity` primero, `pickFace` contra el brush hovered, replace + single si distinto del active. Resuelve la queja del dev *"la seleccion es como dificil... debo rotar y probar varias caras"*.
- **Active face en amarillo, secundarias en naranja**: distingue visualmente cuál es la primary cuando hay multi-select. Antes era todo naranja Half-Life uniforme.
- **Reuso de `EditBrushUVCommand` para alignment**: ya capturaba snapshot completo del brush, soporta multi-cara nativamente sin command nuevo.
- **"Treat as one face" con axisU/V heterogéneos** = log warn + aplica igual: heurística `dot(face.axis, primary.axis) < 0.99` detecta sistemas distintos. El resultado puede salir feo pero es decisión consciente del dev (Hammer 4 hace lo mismo).
- **Refactor CMake colateral**: race condition pre-existente en `add_custom_command POST_BUILD` de MoodEditor + MoodPlayer (ambos hacían `copy_directory` a la misma `Debug/`). Fix: `add_custom_target(mood_runtime_files ALL)` centralizado con `add_dependencies` para serializar el deploy sin perder paralelismo en compilación.
- **Bug fix `Ctrl++` en teclados ES sin numérico**: pre-F2H33 solo aceptaba `SDLK_EQUALS` (US/UK) y `SDLK_KP_PLUS` (numpad); en layout español la tecla a la derecha de Ñ manda `SDLK_PLUS`. Fix agregar al handler.

**Implementación (F2H33 Bloques A-E en 5 commits feat/fix + cierre):**

- **Bloque A (commit `<hash>`)**: plan en [`archive/plans/PLAN_HITO_F2H33.md`](archive/plans/PLAN_HITO_F2H33.md).
- **Bloque B (commit `<hash>`)**: VisGroups + schema bump + render/pick gates + grayed Hierarchy + Player skip + CMake refactor.
- **Bloque C (commit `<hash>`)**: multi-select de caras (Shift+click) + face picking robust + Inspector multi-cara.
- **Bloque D (commit `<hash>`)**: texture alignment Hammer-style (Align/Fit/Justify L/R/T/B + Treat as one face).
- **Fix lateral (commit `<hash>`)**: SDLK_PLUS para Ctrl++ del snap step en teclados ES sin numérico.
- **Bloque E (este commit)**: docs + tag.

**Pendientes conocidos** (post-F2H33):
- **Hammer-style visual polish** (sub-bloque candidato a hito chico futuro): tint del wireframe por color del VisGroup (~30 LOC, alto valor visual), color por tipo de entity (luz=amarillo, audio=naranja, etc.), labels arriba de point entities, mejoras al face picking (puede haber casos borde donde el rayo no pega). Diferidos como paquete.
- **Multi-face material drop**: el handler de `DemoSpawners_Drop.cpp` sigue aplicando solo al face active. Para que aplique a las N caras seleccionadas hay que extender `EditBrushFaceMaterialCommand` a multi-cara. Diferido — emerge si el dev lo pide.
- **VisGroups jerárquicos / drag desde Hierarchy / auto-asignar a current group / lock de VisGroup**: scope mayor, diferidos hasta que emerja necesidad.
- Validación full del Player con compiledMesh: deuda menor heredada de F2H26.

**Próximo paso**: **TBD — definir junto al dev**. Hammer Editor está cerrado funcional al 100%. Opciones a considerar:
- Sub-fase 2.5 gameplay (diálogos / quests / inventario).
- Hammer-style visual polish (sub-bloque del polish UX continuo).
- Otra sub-fase del PLAN_FASE2 (optimización / runtime / polish UI general).

### F2H32 (anterior, ya cerrado)

**🚀 F2H32 cerrado: Geometry tools (clip tool 2-click + carve UI Hammer-style).**
Tag: `v1.22.0-fase2-hito32`.
Verificado por dev: clip splittea 1 brush en Front/Back/Both según `T` cycle; carve resta brush activo por todos los intersectantes; Ctrl+Z restaura en ambos casos. UX hints visibles cuando faltan pre-condiciones.

**Decisiones clave de F2H32:**
- **Clip plane perpendicular al view del orto**: 2 clicks en orto definen una línea (no 3 clicks que serían coplanares en el view plane = degenerado). El plano se construye con `normal = cross(forwardAxis_orto, lineDir_world)` — perpendicular al forward (en el view plane) AND a la línea. Convención Hammer.
- **Clip = agregar plane a Brush::faces**: para Front, agregar `BrushFace { -clipPlane.normal, -clipPlane.distance }` (el interior queda del lado positivo del clipPlane original — convención de normales del motor: outward). Para Back, agregar el plano tal cual. `isBrushValid` filtra resultados degenerados (plano fuera del brush).
- **`BooleanOpCommand` reusado para clip y carve**: extendido el enum con `Clip` + skip de destroy/recreate cuando `bSnapshot.tag.empty()`. Sin command nuevo. Pasa para carve también (kind=Subtract con bSnapshot vacío porque los carvers no se destruyen).
- **Carve = subtract iterativo**: `fragments = [A]`; por cada carver B: `fragments = union de subtract(fragment, B)`. Si A queda completamente consumido → fragments vacío → solo destroy de A (sin spawn). Carvers preservados (Hammer).
- **Carve broadphase por AABB**: solo brushes cuyo AABB intersecta el del active entran al loop. Sin esto N² test booleano por click.
- **Sin keyboard shortcut para carve**: operación destructiva; obligatorio click explícito en el botón del toolbar para evitar accidentes (tecla `C` queda libre).
- **UX hints visibles via `setStatusMessage`**: clip + carve setean mensajes claros en el status bar cuando faltan pre-condiciones (sin selección, sin intersección, etc.). El log warn existente no era visible al dev mientras testeaba.
- **Sin schema bump**: spawn de fragments es state in-memory; el undo via `BooleanOpCommand` snapshot.

**Implementación (F2H32 Bloques A-D en 3 commits):**

- **Bloque A (commit `b1c9963`)**: plan en [`archive/plans/PLAN_HITO_F2H32.md`](archive/plans/PLAN_HITO_F2H32.md).
- **Bloques B+C unificados (commit `4c8d560`)**: clip + carve + UX hints + extensión a BooleanOpCommand. Partials nuevos: `EditorProjectActions_Clip.cpp` + `EditorProjectActions_Carve.cpp`.
- **Bloque D (este commit)**: docs + tag.

**Pendientes conocidos** (post-F2H32):
- **F2H33 — Organización + face polish** (próximo, último hito para cerrar el Hammer en su totalidad funcional): VisGroups (agrupar brushes con nombre + hide/show + persistencia, schema bump v13→v14) + texture alignment del Face Edit Sheet (Align/Fit/Justify L/R/T/B + Treat as one face).
- Validación full del Player con compiledMesh: deuda menor heredada de F2H26.
- **Hollow tool** (carve A con un brush "interior" para crear paredes huecas): scope incremental sobre carve. Diferido — no típico del flow básico de Hammer.
- **Multi-brush carve simultáneo** (selección de N A's en lugar de 1): la math de subtract lo soporta; UI sólo necesita iterar. Si emerge necesidad, agregar.
- **Clip tool en perspectiva 3D**: Hammer no lo tiene; gizmo manipulable para mover el plano libre = scope mucho mayor. Diferido.

**Próximo paso**: **F2H33 — Organización + face polish (VisGroups + texture alignment)**. Plan en `docs/PLAN_HITO_F2H33.md` cuando arranquemos.

### F2H31 (anterior, ya cerrado)

**🚀 F2H31 cerrado: Productivity selection + visual polish (marquee + group transform + snap-to-vertex + frustum + coords cursor).**
Tag: `v1.21.0-fase2-hito31`.
Verificado por dev: marquee detecta N entidades en cualquier orto y group transform las mueve juntas; snap-to-vertex hace el cursor pegarse al vertex objetivo; auto-close del pincel funciona clickeando vertex 1; frustum amarillo de la 3D cam visible en los 3 ortos; coords world del cursor siguen en vivo al mouse.

**Decisiones clave de F2H31:**
- **Tool selector mutually exclusive** en lugar de modifier: Select / CreateBlock / Pincel como radio (3 botones del toolbar). Default = Select estilo Hammer. Antes el block tool fireba siempre con drag en empty space; ahora hay que clickear "Bloque" para activarlo. Mismo modelo que Hammer Editor real.
- **Marquee hit-test "any corner inside"**: para cada entidad, proyectar los 8 corners del AABB world al ndc del orto; si CUALQUIER corner cae dentro del rectángulo, hit. Más liberal que "todos adentro" — alineado con Hammer (cualquier overlap selecciona).
- **Group transform reusa infra existente**: `OrthoDragSession::startPositions` ya iteraba `set.selected` desde F2H29 Bloque B; marquee solo cambia el contenido de `set.selected`, no el flow del drag-edit. Cero código nuevo de movimiento.
- **Snap-to-vertex toggle global** (no per-tool): al activar, AFECTA pincel + block tool corners + rubber band perspectivo simultáneamente. Threshold 0.02 ndc (~8 px screen) generoso para que el snap "agarre" temprano sin requerir precisión al pixel.
- **Broadphase por AABB world expandido**: el snap-to-vertex enumera vertices solo de brushes cercanos al cursor (AABB intersect con caja de threshold). Sin esto, escenas con cientos de brushes hacen N² por frame del drag.
- **Auto-close del pincel al clickear vertex 1**: pedido implícito del dev (*"cuando cierro todos los puntos y aprieto enter, no lo crea"* — porque clickeaba vertex 1 de vuelta generando degenerado). Fix: detectar click dentro de 1mm de `pointsWorld[0]` con `>= 3` puntos → auto-close. Coexiste con el cierre vía Enter (Blender-style).
- **Frustum dibujado a distancia "look-ahead" 4u en lugar del near-plane real**: el near-plane (~0.1m) sería invisible al render. El "rect de mira" a 4u es claramente visible y orientable; el dev percibe "qué mira la 3D cam" desde los 3 ortos.
- **Coords cursor solo cuando hovered**: el `m_liveCursor.hovered` ya lo reportaba el panel desde F2H30 Bloque C (para el pincel rubber band). Reutilizado para mostrar el label `(x, y, z)` en gris claro debajo del label de la vista.

**Implementación (F2H31 Bloques A-E en 3 commits):**

- **Bloque A (commit `c98cfad`)**: plan en [`archive/plans/PLAN_HITO_F2H31.md`](archive/plans/PLAN_HITO_F2H31.md).
- **Bloques B+C+D unificados (commit `3e0e414`)**: marquee + group transform + snap-to-vertex + auto-close pincel + frustum + coords cursor + helpers `brushAabbWorld` / `meshAabbWorld` expuestos.
- **Bloque E (este commit)**: docs + tag.

**Pendientes conocidos** (post-F2H31):
- **F2H32 — Geometry tools** (próximo, siguiente del plan "cerrar Hammer en su totalidad"): clip tool (3-click define plano que splittea brushes) + carve UI (boolean subtract con flow Hammer-style).
- **F2H33 — Organización + face polish**: VisGroups (agrupar brushes con nombre + hide/show + persistencia, schema bump v13→v14) + texture alignment del Face Edit Sheet.
- Validación full del Player con compiledMesh: deuda menor heredada de F2H26.
- Snap-to-vertex aplicado a vertex/edge edit: actualmente solo en pincel + block tool. Si emerge necesidad, extender al bloque 2.4e.
- Marquee Alt-modifier (remove): scope incremental, agregar si el dev lo pide.

**Próximo paso**: **F2H32 — Geometry tools (clip + carve)**. Plan en `docs/PLAN_HITO_F2H32.md` cuando arranquemos.

### F2H30 (anterior, ya cerrado)

**🚀 F2H30 cerrado: Polish del editor de mapas (vertex/edge edit + pincel poligonal + gizmo proporcional + W/E/R double-tap modal).**
Tag: `v1.20.0-fase2-hito30`.
Verificado por dev: vertex/edge edit funciona con snap absoluto al grid + rebasing al cierre, pincel poligonal crea prismas (4 vertices distintos → brush spawneado, dedupe corrigió "figura desaparece"), gizmo rotate sigue al AABB del brush, W/E/R con double-tap dispara modal Rotate/Scale con anillo amarillo visual, click confirma + Esc cancela.

**Cambio importante**: el esquema de atajos del modal pasó por **3 iteraciones** según feedback del dev:
- **Iter 1 (plan)**: G/R/S puros estilo Blender + W/E/R Maya conviven.
- **Iter 2**: dev pide "solo Blender, no Maya" → removidos W/E/R, R = modal Rotate, S = modal Scale.
- **Iter 3 (final, validado)**: dev pide hibrido double-tap → W = Translate gizmo, E single = Scale gizmo / E doble = modal Scale uniforme, R single = Rotate gizmo / R doble = modal Rotate libre. G y S removidos. Cuadrado central de uniform-scale gizmo eliminado (la S desapareció reemplazada por E doble).

**Decisiones clave de F2H30:**
- **Vertex/edge edit con snap WORLD-space (no local)**: el grid del workspace orto vive en world; snap en local quedaba desfasado para brushes con `tf.position != 0`. Solo se snappean los ejes que el dev MUEVE (`|deltaWorld[i]| > eps`), preservando coords no-grid-aligned en ejes intactos.
- **Rebasing al cierre del drag**: `newCentroidLocal = brush.localAabb.center()`, todos los planos se trasladan por `-newCentroid`, `tf.position += R * newCentroid`. Sin esto el gizmo quedaba en la posición vieja del brush. Mismo patrón que F2H12 boolean ops resolvió con `snapshotResultWorld`.
- **Dedupe en pincel**: dos clicks que snapean a la misma celda generaban polígono degenerado y `closePolygonDraw` cancelaba sin pista — el dev percibía "la figura desaparece". Fix: skipear el segundo click si está a < 1mm del último.
- **Gates anti-conflicto pincel/vertex-edit/block-tool**: activar pincel resetea sub-modo a Object + `activeFaceIndex = -1`; bloque 2.4e (vertex/edge edit) gateado con `!m_polyDraw.active`. Sin estos guards, un click del pincel en sub-modo Vertex disparaba edit accidental del brush selecto.
- **Toolbar lateral "Map Tools"** (columna derecha del workspace, ~10% ancho): pedido del dev *"hagamos uno superior"* mutado a *"prefiero columna lado derecho"*. Botones Objeto/Vertex/Edge/Cara/Pincel apilados verticalmente con highlight del activo + tooltips con shortcut.
- **Gizmo rotate radio = `0.6 * max(localAabb.size())` clamp >= 0.5m**: cubre BrushComponent (via `bc.brush.localAabb`) y MeshRendererComponent (via `MeshAsset::aabbMin/Max`). Antes era fijo 1m world — invisible dentro de brushes grandes.
- **W/E/R + double-tap (<0.4s) hibrido Maya/Blender**: scheme final que el dev validó. State `GizmoKeyTapState { lastKey, lastPressTime }` detecta double-tap. Single-tap toggea `GizmoMode`; double-tap arranca modal con misma tecla. Sin shortcuts cruzados — cada tecla tiene un único significado.
- **Modal con axis-lock X/Y/Z**: durante el modal, presionar X/Y/Z lockea/destrabe el constraint (alineado con Blender). Click izq confirma (push `MultiEditTransformCommand`); Esc cancela (revert al startValue snapshot).
- **Visual feedback del modal**: anillo amarillo (radio = distancia cursor-centro) + línea cursor→centro durante Rotate; línea sola durante Scale. Pedido del dev *"que aparezca ese circulo para rotar"*.

**Implementación (F2H30 Bloques A-E en 4 commits):**

- **Bloque A (commit `5674812` previo + plan en `docs/PLAN_HITO_F2H30.md`)**: plan archivado en [`archive/plans/PLAN_HITO_F2H30.md`](archive/plans/PLAN_HITO_F2H30.md).
- **Bloque B (commit `03ed19a`)**: vertex/edge edit con `Csg::pickVertex/pickEdge` + `EditBrushGeometryCommand` snapshot pre/post de planos + `tf.position` + sub-modos `Vertex` (tecla 1) / `Edge` (tecla 2) en `EditorSubMode`.
- **Bloque C (commit `4db0bd0`)**: pincel poligonal con `Csg::makePrismBrushFromPolygon` + validación CCW con auto-revert + dedupe + toolbar lateral `MapEditorTopBar` + gates anti-conflicto.
- **Bloque D (commit `67611ec`)**: gizmo proporcional + W/E/R double-tap modal con `ModalShortcutState` + `GizmoKeyTapState` + visual feedback. Implementación en `EditorOverlay_Modal.cpp` separado para no inflar `EditorApplication_Run.cpp`.
- **Bloque E (este commit)**: docs + tag.

**Pendientes conocidos** (post-F2H30):
- **MVP del editor estilo Hammer está completo**: 4-viewport + click-select + grid + block tool + drag-edit + vertex/edge edit + pincel poligonal + W/E/R modal. Próximo hito **TBD** — definir junto al dev qué prioriza.
- **Validación full del Player** con compiledMesh: deuda menor heredada de F2H26.
- **Multi-selección con marquee select** en orto (clásico Hammer rectángulo de selección sobre múltiples brushes): diferido — no bloquea el modeling flow.
- **Snap-to-vertex** (snap a vertices existentes del scene, no solo al grid): nice-to-have.
- **Brush poligonal cóncavo**: requiere refactor mayor del CSG core (asume convexos).
- Los demás items del backlog "Activos sin orden definido" siguen vivos: split fino de `SceneRenderer_Render.cpp`, iconos Toolbar, polish UX panels, node-graph Material Editor.

**Próximo paso**: **TBD — definir junto al dev**. El editor de mapas tiene su MVP funcional cubierto; queda alinear sobre qué priorizar (gameplay/scripting, polish UI/UX, optimización runtime, features nuevas del editor).

### F2H28 (anterior, ya cerrado)

**🚀 F2H28: Editor de mapas 4-viewport (MVP visual + select + grid).**
Tag: `v1.18.0-fase2-hito28`.

**Decisiones clave de F2H28:** workspace "Editor de mapas" (label castellano, no "Hammer") como 4to default + dockspace 2x2 (Top XZ / Viewport / Front XY / Side ZY) + 3 FBOs LDR + render orto wireframe (color flat celeste GMod / naranja selected) sobre fondo NEGRO (cambio dev sobre plan original gris) + grid 2D shader fullscreen con AA via `fwidth` (menor `(40,40,40)`, mayor naranja Valve `#F58220`) + refactor `pickEntity` → helper `pickEntityFromRay` para rayos paralelos del orto + snap step `m_hammerSnapStep` cycleable Ctrl++/Ctrl+- con label "Grid: Nu" arriba-derecha.

**F2H27 (F6 panel) fue descartado** durante implementación — redundante con Inspector; F6-real es scope grande propio.

### F2H25 + F2H26 (anteriores, ya cerrados)

**🚀 F2H25 + F2H26: Cull overlap parcial en CompileMap + Runtime-load de mesh compilada en MoodPlayer.**
Tags: `v1.16.0-fase2-hito25` (cull overlap + UI layout v2 stamp) + `v1.17.0-fase2-hito26` (schema v13 + Player path).

**Decisiones clave de F2H25:** algoritmo BSP-style polygon clipping con Sutherland-Hodgman extendido + 3 pre-tests críticos (cara entera afuera/adentro/coplanar) + stats `culledOverlapTriangles` + `splitFragments` + UI layout v2 stamp (`imgui_layout_v2.ini`).

**Decisiones clave de F2H26:** schema bump aditivo v12→v13 con `compiledMesh` opcional + layout PBR 11 floats interleaved + componente `CompiledMeshComponent` move-only + render path nuevo `compiledMeshPass` + flag `useCompiledMesh` en SceneLoader (Player=true, Editor=false).

### F2H24 (anterior, ya cerrado)

**🚀 F2H24 cerrado + Bloque C extendido: Reorganización interna del código.**
Tags: `v1.15.0-fase2-hito24` + `v1.15.1-fase2-hito24-bloque-c`.
Verificado automático: suite doctest **610/8359** verde después de cada Bloque B.X. Verificado por LOC: ningún partial supera 500 LOC; los 5 archivos CRÍTICOS originales (5784 LOC totales) reorganizados en 1 núcleo + N partials cada uno.

**Distribución LOC final** (núcleo + partials, 5 CRÍTICOS):

| Original | LOC original | Núcleo | Partials | Max partial |
|---|---:|---:|---:|---:|
| `InspectorPanel.cpp` | 1338 | 77 | 10 (+ Internal.h) | 208 |
| `EditorProjectActions.cpp` | 1272 | 106 | 6 | 329 |
| `DemoSpawners.cpp` | 1188 | 41 | 4 (+ Internal.h) | 399 |
| `PlayerApplication.cpp` | 1160 | 111 | 3 | 484 |
| `EditorApplication.cpp` | 826 | 173 | 2 | 435 |
| `SceneRenderer.cpp` | 776 | 242 | 1 (+_Render 590) | 590 |
| `MeshLoader.cpp` | 767 | 279 | 3 (+ Internal.h) | 213 |
| `EditorOverlay.cpp` | 745 | 310 | 1 (+ Internal.h) | 460 |
| `AssetManager.cpp` | 743 | 256 | 5 | 276 |

**Cambio importante**: F2H24 cumple el pedido explícito del dev al cerrar F2H23 (*"creo que hay archivos demasiado grandes que te cuesta arreglar"*) + extendido tras pedido posterior *"esa deuda chica podes hacerla ahora"* para cubrir los 4 ALTO 700-780 LOC. Refactor puramente estructural — ningún cambio funcional, ningún cambio de API pública. El editor y el player arrancan idénticos al usuario final. Los 4 archivos ALTO (700-780 LOC: `SceneRenderer`, `MeshLoader`, `EditorOverlay`, `AssetManager`) quedan en `PENDIENTES.md` como deuda chica.

**Decisiones clave**:
- **Split por dominio funcional con archivos parciales de la misma clase**: cada partial implementa métodos privados de la clase declarada en el header. Patrón ya usado pre-F2H24 (`EditorApplication.cpp` ya tenía `EditorProjectActions.cpp` + `DemoSpawners.cpp` + `EditorOverlay.cpp` + `EditorPlayMode.cpp` + `EditorRenderPass.cpp` + `EditorScene.cpp` desde Hito 16); F2H24 lo extiende a los 5 CRÍTICOS de Fase 2.
- **API pública intacta**: solo `InspectorPanel.h` recibió 13 métodos privados nuevos (`renderTagSection(Entity)`, `renderTransformSection(Entity)`, etc.) para que el dispatch del `onImGuiRender` quede legible — esto es API privada, no rompe nada externo.
- **Helpers compartidos via header interno** (`Foo_Internal.h`): namespace `Mood::detail` con `inline` functions y templates para que cada TU tenga acceso. Patrón aplicado a InspectorPanel (`pushEditIfDone` template + `helpMarker` + `isDragActiveOfType`) y DemoSpawners (`WorldYBounds` + `rotatedAabbWorldY`).
- **Cap soft 500 / hard 800 reafirmado**: ningún partial supera 500 LOC excepto `_Frame` y `_Run` que rondan 435-484 (loops monolíticos sin sub-secciones obvias para particionar más). Aceptable.
- **Validación incremental por Bloque B.X**: build + suite verde después de cada split antes de pasar al siguiente. Permite detectar regresiones rápido. La suite 610/8359 quedó idéntica antes y después de cada bloque (refactor sin cambios funcionales).
- **Bloque C (split de ALTO 700-780 LOC) skipped por presupuesto**: 4 archivos movidos a `PENDIENTES.md` como deuda chica. F2H24 entregó los 5 CRÍTICOS — los ALTO son menos urgentes (todos < 800 LOC = bajo el hard cap).
- **Errores resueltos durante el trabajo** (no requieren mención del dev pero quedan registrados): `glm/gtx/compatibility.hpp` removido del `InspectorPanel_Brush.cpp` (extensión experimental no necesaria); `APIENTRY` reemplazado por `GLAD_API_PTR` en `EditorApplication_Init.cpp` (no depende de inclusión transitiva de `<windows.h>`); `FrameStats` requiere include de `IRenderer.h` (definición completa) en `EditorApplication_Run.cpp` además de `SceneRenderer.h` (forward-decl).

**Implementación (Bloques A-E):**

- **Bloque A — Plan F2H24 + auditoría**: subagente recorrió `src/` con `wc -l`. Confirmé números reales con conteo directo (los del subagente eran ligeramente bajos por path antiguo). Plan documentado en `docs/archive/plans/PLAN_HITO_F2H24.md`.
- **Bloque B.1 — InspectorPanel.cpp**: 1338 LOC → núcleo 77 + 10 partials + Internal.h. Tag intermedio: `1cce00a`.
- **Bloque B.2 — EditorProjectActions.cpp**: 1272 LOC → núcleo 106 + 6 partials. Tag intermedio: `05e9afe`.
- **Bloque B.3 — DemoSpawners.cpp**: 1188 LOC → núcleo 41 + 4 partials + Internal.h. Tag intermedio: `dba4aa7`.
- **Bloque B.4 — PlayerApplication.cpp**: 1160 LOC → núcleo 111 + 3 partials. Tag intermedio: `01a80ae`.
- **Bloque B.5 — EditorApplication.cpp**: 826 LOC → núcleo 173 + 2 partials. Tag intermedio: `9abc38b`.
- **Bloque C extendido — split de ALTO 700-780 LOC** (atacado tras pedido del dev): C.1 AssetManager 743→256+5 partials, C.2 EditorOverlay 745→310+_Gizmo 460+Internal.h, C.3 MeshLoader 767→279+_Skeleton 184+_Geometry 213+_Animation 68+Internal.h, C.4 SceneRenderer 776→242+_Render 590 (deuda chica aceptada: frame loop monolítico bajo hard cap). Tag `v1.15.1-fase2-hito24-bloque-c` tras el cierre.
- **Bloque D — validación**: suite 610/8359 verde tras cada Bloque B.X; build OK Debug; cap soft 500 respetado en todos los partials nuevos.
- **Bloque E — cierre**: este documento + HITOS + DECISIONS + PENDIENTES + tag `v1.15.0-fase2-hito24`.

**Pendientes conocidos** (memoria + `PENDIENTES.md`):
- **F2H25 — 4-viewport Hammer-style layout** (próximo, heredado de F2H22→F2H23→F2H24 candidato): workspace nuevo "Hammer" con dockspace en 4 cuadrantes.
- **Split de archivos ALTO** — **CERRADO en Bloque C extendido tag `v1.15.1`**. Quedan documentadas las 4 splits + la deuda chica residual en `SceneRenderer_Render.cpp` (590 LOC, sobre soft 500 pero bajo hard 800 — frame loop monolítico).
- **Iconos image-based del Toolbar** (deuda explícita F2H22): FontAwesome / IcoMoon. Hito chico futuro.
- **CompoundCommand para batch undo** (deuda parcial cerrada en F2H23 con MultiEditTransformCommand).
- **Pase de polish UX continuo (siguiente ronda)**.
- **Node-graph del Material Editor** (deuda F2H21).
- **Runtime-load mesh compilada en MoodPlayer** (deuda F2H20).
- **Cull overlap parcial** (deuda F2H20).

**Próximo paso**: **F2H25 — 4-viewport Hammer-style layout**. Workspace "Hammer" con 4 cuadrantes: 1 perspectiva 3D + 3 ortográficas top/front/side en wireframe + drag-edit entre vistas con grid snap. Tradeoff: ~4× costo CPU del frame — mitigar reusando shadow pass / light grid compartido. Reusa la infra de workspaces de F2H7+F2H22 con un `buildHammerWorkspace` nuevo en `Dockspace.cpp`.


### Hitos anteriores

Para los detalles de hitos cerrados previos a F2H24 (F2H23, F2H22, ..., F2H1, Fase 1, Hito 0-42), ver [`archive/ESTADO_HISTORICO.md`](archive/ESTADO_HISTORICO.md). Split aplicado en F2H26 cierre — este documento mantiene solo el current state + último hito cerrado.

---

## 2. Entorno de build — lo que realmente funciona

### Toolchain real instalado en la máquina del dev

- **Visual Studio 2022 Community** → tiene MSVC 14.44 + SDK Windows 11 + CMake 3.31 bundleado. **Este es el que usamos.**
- **Visual Studio 2026 Community** → instalado **sin** workload de C++. No tiene compilador. Ignorar hasta que el dev lo complete o desinstale.

La ruta clave es:

```
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat
```

### Cargar entorno MSVC desde PowerShell (el comando que funciona)

```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 && <tu_comando_aqui>'
```

Desde un **Developer Command Prompt for VS 2022** del menú inicio, `cl` y `cmake` funcionan directamente sin esto.

### Versiones verificadas

- `cl` → `Microsoft (R) C/C++ Optimizing Compiler Version 19.44.35226 for x64`
- `cmake --version` → `3.31.6-msvc6`
- `git --version` → `2.49.0.windows.1`

### Generador CMake correcto

`Visual Studio 17 2022` (ya está en `CMakePresets.json`, no tocar).

---

## 3. Comandos que ya corrieron exitosamente

Desde la raíz del repo, con entorno MSVC cargado:

```bash
# Configurar (descarga deps la primera vez, ~5 min)
cmake --preset windows-msvc-debug

# Compilar
cmake --build build/debug --config Debug
```

Resultado: `build/debug/Debug/MoodEditor.exe` (3.9 MB) + `SDL2d.dll` (4.3 MB) copiada automáticamente al mismo directorio.

Para ejecutar:

```
.\build\debug\Debug\MoodEditor.exe
```

---

## 4. Qué tiene que hacer el próximo agente

### Tarea inmediata: TBD — definir junto al dev

F2H33 está cerrado (tag `v1.23.0-fase2-hito33`). **Hammer Editor cerrado funcional al 100%.** 31/44 hitos de Fase 2.

Opciones a considerar para el próximo hito (preguntar al dev al arrancar la sesión):

1. **Sub-fase 2.5 gameplay** — diálogos, quests, inventario. Es el siguiente bloque del PLAN_FASE2 si seguimos el orden.
2. **Hammer-style visual polish** (sub-bloque chico): tint del wireframe por color del VisGroup (~30 LOC, alto valor visual), color por tipo de entity (luz=amarillo, audio=naranja), labels arriba de point entities, mejoras al face picking UX (descubierto durante F2H33 — el dev mencionó *"se puede pulir a futuro"*). Diferidos como paquete.
3. **Otra sub-fase del PLAN_FASE2** — optimización, runtime, polish UI general.
4. **Multi-face material drop** — extender `EditBrushFaceMaterialCommand` a multi-cara para que dropear textura sobre N caras seleccionadas las afecte a todas (hoy aplica solo al active).

### Flujo recomendado en esta sesión

1. Preguntar al dev qué hito atacar (TBD entre las 4 opciones de arriba).
2. Crear `docs/PLAN_HITO_F2HN.md` con bloques A-E concretos.
3. Trabajar bloque por bloque marcando en el plan al cerrar cada uno.
4. Actualizar `docs/DECISIONS.md` cuando aparezca una decisión no trivial.
5. Al final: commits atómicos en español, merge a main, tag `v1.X.0-fase2-hitoN`, actualizar este documento + `docs/HITOS.md`, archive del plan.

---

## 5. Gotchas conocidos — cosas que ya se aprendieron por las malas

1. **VS 2026 Community se instaló sin el workload de C++.** El `Developer Command Prompt for VS 2026` abre pero `cl` y `cmake` no existen. No depender de VS 2026 hasta que el dev agregue el workload o lo desinstale. Usar siempre VS 2022.
2. **El `Developer Command Prompt` normal de Windows** (sin MSVC) no tiene `cl` en PATH. Si un comando falla con "cl no se reconoce", revisar que el prompt diga "Visual Studio 2022" en el banner.
3. **SDL2 en debug se llama `SDL2d.dll`**, no `SDL2.dll`. El `add_custom_command` POST_BUILD del CMakeLists copia la variante correcta según `$<TARGET_FILE:SDL2::SDL2>`.
4. ~~**GLAD no está incluido en Hito 1.**~~ Resuelto en Hito 2: `EditorApplication.cpp` usa `<glad/gl.h>` y llama `gladLoaderLoadGL()` tras crear el contexto. GLAD y el loader interno de ImGui conviven sin conflictos porque glad2 prefija sus símbolos con `glad_`.
8. **GLAD v2 usa naming distinto de v1.** Header: `<glad/gl.h>` (no `<glad/glad.h>`). Source: `gl.c` (no `glad.c`). Loader: `gladLoaderLoadGL()` / `gladLoaderUnloadGL()`.
9. **Dos máquinas de desarrollo.** Notebook con Intel Iris Xe (GL 4.5 OK, menos performance) y desktop con AMD Ryzen 5 5600G + NVIDIA GTX 1660. En el desktop, forzar NVIDIA para `MoodEditor.exe` desde el Panel de Control NVIDIA (sino Windows puede elegir la iGPU AMD). `imgui.ini` y `build/` NO viajan por git: cada máquina genera lo suyo.
10. **Shaders se buscan con path relativo `shaders/default.vert`.** Funciona desde la raíz del repo (VS_DEBUGGER_WORKING_DIRECTORY) y desde el directorio del exe (post-build `copy_directory shaders/`). Si agregás shaders nuevos, la copia es automática.
5. **El primer `cmake --preset` tarda ~5 minutos** porque baja y compila subdeps de SDL2 + ImGui + spdlog + GLM. Ese tiempo solo ocurre la primera vez; después el build incremental es segundos.
6. **spdlog busca pthread en Windows** y sale un warning de que no lo encuentra. Es esperado y benigno en Windows.
7. **GLM tira warnings de `cmake_minimum_required` deprecated** porque su CMakeLists usa sintaxis vieja. Ignorable; no es nuestro código.

---

## 6. Recordatorios de filosofía (sección 9 del doc técnico)

- Ship something: no romper el build entre commits.
- No implementar hitos futuros "por adelantar trabajo".
- No añadir dependencias que no estén en la sección 4 del doc sin consultar.
- Comentarios en español.
- Convención de commits: `tipo(scope): mensaje` en español.
- Preguntar al dev antes de asumir ante ambigüedades.

---

## 7. Archivos clave que tocar cuando

| Para... | Tocar |
|---|---|
| Añadir una dependencia | `CMakeLists.txt` (bloque CPM) |
| Cambiar flags de compilador | `cmake/CompilerWarnings.cmake` |
| Añadir un panel al editor | `src/editor/panels/*` + `EditorUI.(h|cpp)` |
| Registrar una decisión arquitectónica | `docs/DECISIONS.md` |
| Marcar progreso de hito | `docs/HITOS.md` |
| Cambiar log level | `src/core/Log.cpp` |

---

## 8. Al arrancar una sesión nueva

1. Leer este archivo entero.
2. Leer `docs/PLAN_HITO<N>.md` del hito en curso para ver qué tareas quedan.
3. Leer `MOODENGINE_CONTEXTO_TECNICO.md` si es la primera vez.
4. `git status` + `git log --oneline -10` + `git tag --sort=-v:refname | head -5` para ver tags y cambios recientes.
5. Preguntar al desarrollador: "¿seguimos con el Hito en curso o pasó algo nuevo?"
6. Actuar según la respuesta.
