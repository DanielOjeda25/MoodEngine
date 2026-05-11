# Pendientes vivos — MoodEngine

> Backlog de tareas conocidas que NO están planificadas en un hito específico
> todavía. Cada vez que cerrás un hito, mové aquí los pendientes que quedaron
> identificados pero no abordados, y eliminá los que ya hayas cubierto.
>
> Para el roadmap de hitos cerrados/programados, ver `HITOS.md`.
> Para el estado actual del proyecto y "dónde estamos", ver `ESTADO_ACTUAL.md`.

---

## Post-F2H47 (2026-05-10) — Dialog Editor (autoría) cerrado

### Próximo a atacar

- **F2H48 — Dialog runtime** (segunda mitad del par editor/runtime del sistema de diálogos).
  - **Origen**: continuación natural de F2H47 — el editor produce `.mooddialog` pero todavía no hay sistema que los interprete en Play Mode.
  - **Scope esperado**:
    - **`engine/dialog/DialogSystem.h/cpp`**: state machine que recorre el árbol según el flow. Mantiene `current_node_id`, evalúa `condition_lua` por choice (filtra las visibles), aplica `on_select_lua` al elegir, salta al nodo destino vía el link del output socket correspondiente. Variables persistentes via `dialog.set_var/get_var` (map key-value tipo i18n pero por session de dialog).
    - **`AssetManager::loadDialog(logicalPath)`**: integración con VFS para que `DialogComponent` apunte a paths lógicos (`"dialogs/intro.mooddialog"`).
    - **HUD widget default** en el framework F2H39: caja inferior estilo HL2 con texto del NPC + opciones clickeables (con condition_lua aplicado para filtrar). Override-able vía `dialog.set_renderer(callback)` para juegos con HUD propio (engine-grade principle).
    - **Lua bindings nuevos** en tabla `dialog`: `start("npc_id")`, `is_active()`, `current_node()`, `advance(choice_index)`, `set_var/get_var`, `on_node_enter(callback)`, `on_choice(callback)`, `set_renderer(callback)`.
    - **`DialogComponent`** entity-side: campo `std::string dialog_path` + opcional `auto_start_on_interact`. Inspector del editor general permite asignar un `.mooddialog` por NPC.
    - **Tests de integración runtime**: state machine recorre asset de prueba, condiciones filtran choices, hooks Lua se invocan.
  - **Decisión técnica abierta para F2H48**: cómo se dispara el dialog en Play Mode. Opciones: (a) script Lua del NPC llama `dialog.start("npc_id")` cuando un trigger collide con player; (b) `DialogComponent` con flag `auto_start_on_interact` que el motor detecta automáticamente cuando player presiona E cerca del NPC; (c) combinación de ambos. Recomendación inicial: (c) — auto-start es ergonómico para casos típicos, Lua start es escape hatch para casos custom.

- **Después de F2H48**: **F2H49 — Demo characters Mixamo + escena narrativa completa** (Bloque 2.5 nuevo del plan, scope nivel B). Pedido del dev: *"que sentido tiene crear un sistema de diálogo, sino tenemos a quien asignar ni como sentir"*. Materializa el caso de uso end-to-end con assets reales:
  - **Player char** Mixamo con 5 anims (idle/walk/run/jump/wave).
  - **NPC char** Mixamo (medieval/fantasy) con 4 anims (idle/talking/wave/look_around).
  - **Pipeline FBX → glb** (Blender o gltf-pipeline CLI, decidir al arrancar).
  - **Demo scene `narrative_demo.moodmap`** con player spawn + NPC con `DialogComponent` + trigger interact "[E] Hablar".
  - **Update demo dialog** a 5-7 nodos con branching real (heroico/casual/agresivo).
  - **Animator state machines** (idle ↔ walk ↔ run ↔ jump del player; idle ↔ talking ↔ look_around del NPC).
  - **Welcome modal**: botón "Cargar demo narrativo" separado del Fox.
  - Validación tour completo: walk al NPC → press E → dialog → choice → branch → cierre.
  - **Estimado ~10-11h**, tag `v1.39.0-fase2-hito49`.
  - **NO confundir con F2H35 original** (Mixamo importer pipeline auto-detect/canonical skeleton) — F2H49 es content production específico para validar; F2H35 sería infra masiva diferida hasta que emerja necesidad real.
- **Después de F2H49**: continuar con **Inventario** (F2H50) y **Quest Editor** (F2H51) según `PLAN_SUBFASE_2_5.md`. Inventario es UI propia (grid + 3D preview, no node-graph); Quest Editor reusa el framework de F2H46 con flowchart de objetivos.

### Diferidos sin orden (emergentes post-F2H47)

- **Tipos de nodo dialog adicionales** (`condition`, `action`, `jump`): el schema v1 tiene solo `dialog_line`. Los hooks `condition_lua`/`on_select_lua` por choice cubren el 80%. Agregar tipos si emerge necesidad real en proyectos de devs externos.
- **Voiceover sync** (highlight de palabras según timing del audio): customData ya tiene `audio` path; sumar timing data + UI de waveform en hito propio de polish.
- **Animation `wait_for` flag**: customData tiene `animation` pero v1 solo dispara, no espera. Polish.
- **Theming custom del grafo** (paleta naranja Valve / temática narrativa): herencia de F2H46.
- **String tables dedicadas para gameplay** (vs reusar `assets/i18n/`): considerar si los devs externos piden separación por contexto. v1 reusa i18n existente.
- **Preview en-vivo dentro del editor** (sin entrar a Play Mode): feature de F2H48+.
- **Auto-save del Dialog Editor**: explícito en v1 por seguridad; auto-save con debounce + undo-a-disco para v2.
- **AssetManager::loadDialog**: pendiente para F2H48 (runtime). F2H47 usa I/O directo del filesystem.

### Histórico resuelto F2H47

- ~~Schema `.mooddialog` versionado + serialización JSON~~ — resuelto. `Asset { Graph + Metadata }` con `_version=1`, `Node::customData` con typed accessors `parseLine`/`writeLine`.
- ~~Auto-sync sockets ↔ choices array~~ — resuelto. `writeLine()` mantiene invariante N choices = N output sockets automáticamente.
- ~~`Graph::removeSocket(SocketId)`~~ — resuelto. Extension a F2H46 con cascade de links incidentes.
- ~~`DialogValidator`~~ — resuelto. 6 reglas (start_node, input/output sockets, choices sync, cycles via DFS, orphans) — cycles y orphans son Warning, no Error.
- ~~`DialogBrowserPanel` + `DialogEditorPanel` + `DialogNodeInspectorPanel`~~ — resueltos. 3 panels en categoría Narrative + accessors en EditorUI.
- ~~Sample demo `.mooddialog`~~ — resuelto. Menu "Ayuda > Demos > Cargar diálogo demo" genera programáticamente con 3 nodos pre-armados.
- ~~Regresión `test_workspace_manager.cpp` count==4 vs 5~~ — resuelto. Tests actualizados a 5 workspaces (narrative agregado en F2H46).

## Post-F2H46 (2026-05-10) — Node-graph framework + workspace "Narrativa" cerrado

### Próximo a atacar

- **F2H47 — Dialog Editor** (primer editor real construido sobre el framework F2H46).
  - **Origen**: continuación natural de Sub-fase 2.5 Bloque 0.1 → Bloque 2 según `PLAN_SUBFASE_2_5.md`. F2H46 cerró la infra reutilizable; F2H47 la consume para producir el primer editor real de contenido.
  - **Scope esperado**:
    - **DialogTree schema + serialización `.mooddialog`**: JSON con lista de nodos `{id, text (i18n key), portrait, audio, animation, options[]}`, donde cada option tiene `{label, next_node_id, condition_predicate, on_select_hook}`.
    - **Dialog Editor panel** sobre el framework F2H46: nodos `dialog_line` con sockets `flow` para conexiones entre líneas, sockets `choice` para opciones del jugador. `customData` JSON contiene texto, audio path, portrait.
    - **Dialog runtime + HUD widget default** (caja inferior con texto + opciones clickeables, estilo HL2). Override via Lua `dialog.set_renderer(callback)`.
    - **Bindings Lua**: `dialog.start("npc_id")`, `dialog.is_active()`, `dialog.set_var/get_var`, `dialog.on_node_enter`, `dialog.on_choice`.
    - **DialogComponent** entity-side: un NPC en escena con `DialogComponent` apunta a un `.mooddialog` que el Editor armó visualmente.
  - **Inventario antes de F2H47?**: La recomendación original era inventario primero (diálogos dependen de "tengo X item" para predicados), pero el dev pidió explícitamente "conversaciones" como caso de uso entendible — F2H47 puede arrancar con condiciones simples (flags booleanos `dialog.set_var/get_var`) sin esperar inventario. Predicados `item_count >= N` se agregan cuando inventario aterrice.

- **Decisión técnica abierta para F2H47**: cómo persistir el `customData` rico del nodo (texto multilínea, audio path, portrait selection, optional hooks). Opciones:
  - (a) Inspector contextual al seleccionar un nodo del grafo: muestra campos editables del schema según `typeTag` del nodo. Reusa InspectorPanel del editor.
  - (b) Panel dedicado "Dialog Properties" docked al lado del grafo.
  - (c) Modal popup al double-click sobre un nodo.
  - Recomendación: (a) — consistencia con el flujo de InspectorPanel del resto del editor (clickeás entidad → editás properties).

### Diferidos sin orden (emergentes post-F2H46)

- **Theming custom del grafo** (paleta naranja Valve del motor): el default de imgui-node-editor es funcional pero gris/azul genérico. Cuando los Dialog/Quest Editor estén operativos y el grafo sea visualmente prominente, agregar styling para matchear el resto del editor (orange selected outline, HL-yellow accents).
- **Animaciones de flow en links** (la lib soporta `ne::Flow(linkId)` para animar partículas a lo largo del link — útil para mostrar "data flowing" en debug del dialog tree).
- **Hierarchical grouping de nodos** (collapsible meta-nodes — útil para diálogos muy largos donde se quiere agrupar branches).
- **Export-to-image** del grafo (PNG para docs).
- **Copy/paste cross-graph** (un mismo node template usado en múltiples diálogos).
- **Search/filter de nodos por nombre** en grafos grandes.
- **Auto-layout** ("organize nodes" button que reordena automáticamente).
- **Comments / sticky notes** sobre el canvas.

### Histórico resuelto F2H46

- ~~Node-graph framework reutilizable~~ — resuelto. `engine/nodegraph/Graph.h/cpp` (puro) + `NodeGraphEditor.h/cpp` (pImpl sobre imgui-node-editor). 5 reglas de canConnect estrictas, IDs `u32` monotonicos sin reuso, serialización JSON con schema versionado.
- ~~5 NodeGraphCommands undoable~~ — resuelto. AddNode (con socket specs), RemoveNode (snapshot con socket-ID remap en undo), Move, AddLink, RemoveLink. Mismo patrón ICommand que el resto.
- ~~Workspace "Narrativa"~~ — resuelto. 5to tab al lado de "Diseño de Niveles", id ASCII `narrative`, dock layout 70/30 con Sandbox + Intro panel, sin paneles de scene/3D contaminando.
- ~~NodeGraphSandboxPanel debug~~ — resuelto. Demo de 4 nodos pre-cargado, toolbar i18n, save/load JSON a `node_graph_sandbox.json`.
- ~~Tests del data model~~ — resuelto. 25 test cases + 76 assertions cubren CRUD, las 5 reglas, cascade delete, roundtrip JSON, schema versioning.
- ~~Warning false-positive ImGui 1.92 ID conflict~~ — resuelto. `io.ConfigDebugHighlightIdConflicts = false` global en init.
- ~~Conflicto operators ImVec2 con ImGui docking~~ — resuelto. Patch idempotente al `imgui_extra_math.inl` via CMake `file(READ/WRITE)` en configure-time.

## Post-F2H45 (2026-05-10) — Cierre de deudas pre-Sub-fase 2.5 (AddComponentCommand undoable + Lato Player con tildes + Console tooltip i18n)

### Histórico resuelto F2H45 — completado, ver detalles abajo

### Diferidos sin orden (emergentes post-F2H45)

- **Lua scripts traducibles** (deuda F2H43 que NO entró en F2H45):
  los string literals dentro de `assets/scripts/*.lua` (ej. `hud_demo.lua`
  con `"Demo: explore the test map"`, `"[E] Pick up demo item"`)
  quedaron sin envolver. Requiere binding Lua `T("...")` desde sol2 +
  sweep de los .lua. **Scope reducido** post-F2H45: solo aplica a demos
  actuales, esperar a tener scripts gameplay reales (Sub-fase 2.5+).
- (resto de diferidos heredados de F2H44/F2H43, ver bloques siguientes)

### Histórico resuelto F2H45

- ~~AddComponentCommand undoable~~ — resuelto en F2H45
  (`v1.35.0-fase2-hito45`). Type-erased via `std::function<void(Entity&)>`
  para `add` y `remove` (un solo header `AddComponentCommand.h` cubre
  los 11 componentes del popup). Helper templado
  `makeAddComponentCommand<T>(entity, label)` captura T en las dos
  closures. Refactor del popup en `InspectorPanel.cpp`: las 11 closures
  pasan a returnar `std::unique_ptr<ICommand>` y el handler hace
  `historyStack()->push(...)`. Sin snapshot del estado pre-add — los
  edits posteriores quedan capturados en sus propios EditPropertyCommand
  (LIFO). Key i18n nueva `editor.cmd.add_component`. 8 tests nuevos.
- ~~Cambiar font del MoodPlayer a Lato + tildes en `es.json`~~ — resuelto
  en F2H45. `PlayerApplication_Init.cpp` ahora carga
  `LatoLatin-Regular.ttf` 15px con range Basic Latin + Latin-1 Supplement
  + General Punctuation subset (mismo patrón que `EditorApplication_Init.cpp`
  F2H38). FA NO mergeada (HUD usa DrawList procedural, no necesita
  iconos). Sweep mecánico delegado a subagente: ~75 palabras corregidas
  en ~60 valores de `es.json` (sustantivos `acción/selección/posición`,
  HUD `MUNICIÓN/MENÚ`, `Diseño/Español/Añadir/tamaño`, signos `¿/¡`).
  400 keys totales preservadas, JSON parsea limpio.
- ~~Console tooltip multilínea i18n~~ — resuelto en F2H45. 8 keys nuevas
  (`editor.panel.console.help.header`, 6 `level.X`, `footer`). Los
  icons FA siguen en código (sus macros UTF-8 no caben en JSON sin
  perder bytes). Tooltip se construye con `std::string` concat: header
  + `"  " ICON_FA_X " " + I18n::T("level.X")` por nivel + footer.
- ~~Botón "Recompute mesh" del Inspector~~ — eliminado en F2H45 a pedido
  del dev como fix lateral. Era debug breadcrumb del Hito 12+ que forzaba
  `bc.dirty=true` por si alguna mutación del brush no marcaba dirty
  automáticamente. -8 LOC en `InspectorPanel_Brush.cpp`, -2 keys i18n.

## Post-F2H44 (2026-05-10) — Polish onboarding UX cerrado (sin docs externos)

### Diferidos sin orden (emergentes post-F2H44)
- **USER_GUIDE/* + README + GIF + tutorial**: descartado en F2H44 por
  el dev (*"seguiremos agregando cosas que luego vamos a terminar
  cambiando"*). Reactivar como hito propio post-Fase 2 cuando el motor
  estabilice y los workflows queden congelados. Empezar por GIF demo
  de 30s (motor en acción) + GETTING_STARTED.md (5 pasos: open → spawn
  brush → texturizar → physics → script).
- **Material Editor con `ImGui::Tables`**: el modo TwoColumns fue
  eliminado en F2H44 (preview siempre vertical). Si emerge necesidad
  de mostrar preview + controles + node-graph lado a lado, migrar a
  Tables (sincroniza heights correctamente).
- **Workspaces como definiciones declarativas en JSON**: si los
  workspaces crecen a >6 (animation/timeline/profile), considerar
  moverlos a `assets/workspaces/*.json` (icono + ID + layout default).
- (resto de diferidos heredados de F2H43, ver bloque siguiente)

### Histórico resuelto F2H44

- ~~Workspaces con nombres ambiguos para devs nuevos~~ — resuelto en
  F2H44 (`v1.34.0-fase2-hito44`). Renombrados a "Layout / Scripting /
  Materials Library / Level Design" (EN) y "Layout / Scripting /
  Biblioteca de Materiales / Diseno de Niveles" (ES). Refactor:
  `Workspace.name` ahora es ID ASCII estable (`"layout"/"scripting"/
  "materials"/"map_editor"`), label visible viene de `T("workspace.<id>")`.
  Migración 3 generaciones (F2H7/F2H22/F2H44) preserva iniLayouts
  custom de proyectos viejos.
- ~~Sin "Add Component" en Inspector~~ — resuelto en F2H44. Botón
  centrado al final del Inspector + popup ImGui con search + lista
  agrupada por categoría (Render/Physics/Audio/Logic/World) + filtra
  los componentes que la entidad ya tiene. 11 componentes agregables.
- ~~Demos enterrados en Ayuda > Demos (primera vista = dockspace
  vacío)~~ — resuelto en F2H44. Botón "Cargar mapa demo (Personaje
  animado)" en Welcome modal: 1 click → crea proyecto temp + spawnea
  Fox.glb. Primera vista del editor para devs nuevos = motor en acción.
- ~~VisGroups sin onboarding contextual~~ — resuelto en F2H44. Marker
  `(?)` agregado al lado del botón "+ Nuevo grupo" con tooltip
  multilínea explicando el concepto Hammer/Source.
- ~~Outline AABB invisible para meshes seleccionados~~ — resuelto en
  F2H44. Pre-fix usaba cubo unitario hardcoded `(-0.5/0.5)` en
  `EditorRenderPass.cpp` — el outline del Fox.glb (~3m largo) era
  invisible. Fix: leer `MeshAsset::aabbMin/aabbMax` real via
  AssetManager. Para entidades sin mesh ni brush (Light/Audio/Trigger/
  Camera/ParticleEmitter), AABB chico fijo 0.5m³ alrededor del origen.
- ~~Material Editor se descuadra al ensanchar el panel~~ — resuelto en
  F2H44. Modo TwoColumns eliminado (ImGui::Columns no sincroniza
  alturas, preview quedaba flotando suelto). Layout siempre Vertical
  (preview arriba + controles abajo) — robusto a cualquier dock.
- ~~Sin Ctrl+ScrollWheel para snap step en orto~~ — resuelto en F2H44.
  Atajo paralelo a Ctrl+= / Ctrl+- (más natural cuando el mouse ya
  está sobre el viewport). Triple gate (workspace `map_editor` +
  `KMOD_CTRL` + `liveCursor().hovered` en alguno de los 3 ortos) para
  no robar zoom de cámara perspectiva.

## Post-F2H43 (2026-05-10) — Sistema de i18n completo cerrado (Editor + HUD + Player)

### Diferidos sin orden (emergentes post-F2H43)

- **Lua scripts traducibles** (deuda F2H43, no resuelta en F2H45): los
  string literals dentro de `assets/scripts/*.lua` (`hud_demo.lua` con
  `"Demo: explore the test map"`, `"[E] Pick up demo item"`) quedaron
  sin envolver. Requiere binding Lua `T("...")` desde sol2 + sweep de
  los .lua. Esperar a tener scripts gameplay reales (Sub-fase 2.5+).
- **Plurales en i18n**: ej. "1 entity" vs "5 entities" en HUD se
  resuelve hoy con la misma key `editor.panel.hierarchy.count` que dice
  "{} entidades" (siempre plural). Si emerge presión visual, evaluar
  lib externa (gettext) o resolver inline con condicional.
- **Mini-map / Radar** (CoD/Fallout — requiere render-to-texture
  topdown del mundo cercano. Hito propio mediano).
- **Themes alternativos del HUD** (Doom saturado / Fallout verde —
  requiere theme runtime + bindings Lua. Hito chico propio).
- **Optimizaciones GPU side** diferidas en F2H42: GPU timestamp
  queries, CSM cascadas, frustum cull shadow pass. Sin urgencia con
  headroom 17x actual.

> **Resueltos en hitos posteriores** (no listar como pendientes):
> - Cambiar font Player a Lato + sweep tildes en `es.json` → resuelto
>   en F2H45 Bloque B.
> - Workspace names traducibles → resuelto en F2H44 Bloque C.
> - Console tooltip multilínea i18n → resuelto en F2H45 Bloque C.

### Histórico resuelto

- ~~Sistema de i18n completo (Editor + HUD + Player)~~ — resuelto en
  F2H43 (`v1.33.0-fase2-hito43`). **Infra**: `engine/i18n/I18n.h/cpp`
  con API namespaced `Mood::I18n::T("key")` + JSON loader + fallback
  inglés + warn-once por key faltante + interpolación fmt-style.
  **Persistencia**: `core/UserSettings.h/cpp` en
  `%APPDATA%\MoodEngine\settings.json` (compartido Editor↔Player).
  **Selector**: menú `Ver > Idioma > [English / Español]` en MenuBar.
  **Barrido**: ~347 keys totales en cada JSON (en + es), 24 archivos
  del editor + 4 del HUD/Player envueltos en `T()`. Bloque C2
  delegado a subagente (refactor mecánico voluminoso). Tests: 7
  unitarios nuevos (6 i18n + 1 UserSettings), 640/8475 verde.

## Post-F2H42 (2026-05-10) — Optimización runtime cerrada (shadow caching + VSync toggle)

- **Diferidos sin orden** (emergentes post-F2H39 + post-F2H42, no en
  PLAN_FASE2):
  - **Mini-map / Radar** (CoD/Fallout — requiere render-to-texture
    topdown del mundo cercano. Hito propio mediano).
  - **Themes alternativos del HUD** (Doom saturado / Fallout verde —
    requiere theme runtime + bindings Lua. Hito chico propio).
  - **Optimizaciones GPU side** diferidas en F2H42: GPU timestamp
    queries (medición real GPU vs CPU stall), CSM cascadas (mejora
    calidad shadow en escenas grandes), frustum cull shadow pass.
    **Sin urgencia** — motor a 780 FPS en stress (17x headroom). Hito
    propio si emerge presión real (escenas >1000 meshes, target VR,
    mobile port).

### Histórico resuelto

- ~~Optimización runtime sobre PC de escritorio~~ — resuelto en F2H42
  (`v1.32.0-fase2-hito42`). **Shadow map caching por hash de escena**
  (FNV-1a 64 incremental de transforms + mesh ids + light dir): cache
  hit rate **99.996%** en escena estática, ShadowPass::record cae de
  **13.4 ms/frame a 278 μs/frame (-98%)**. **VSync toggle** en
  Performance panel para medir FPS real. Stress scene (285 entidades /
  17K tris): **780 FPS sin VSync (1.27 ms/frame)** vs estimado ~45
  fps pre-fix = **17x mejor**. Skybox reorder probado y revertido
  (era GPU sync, no overdraw del shader). Tracy: `test1-4.tracy`,
  CSV: `performance_baseline.csv` con 5 labels.

## Post-F2H41 (2026-05-09) — 5 widgets HUD diferidos + 3 fixes laterales + i18n unificado cerrado

### Histórico resuelto

- ~~5 widgets HUD diferidos (StaminaBar / ObjectiveText / KillFeed /
  CompassBar / CRT scanline)~~ — resuelto en F2H41 (`v1.31.0-fase2-hito41`).
  Total post-F2H41: 13 widgets en `k_widgets[]`. CompassBar derive yaw
  via `atan2(cameraForward.x, -cameraForward.z)`; KillFeed con `KillEntry`
  + lifetime 4s + cap 5; CRT scanline default OFF; StaminaBar bypass si
  `max_stamina<=0`; ObjectiveText con prefix `OBJECTIVE: ` automático.
  Bindings Lua expandidos: 10 funciones nuevas en tabla `hud`.
- ~~Hover/pick spurious en Hierarchy panel durante Play Mode~~ —
  resuelto en F2H41 fix lateral. `io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX)`
  en `EditorApplication::beginFrame` cuando Play && !paused.
- ~~Caminata "muy lenta con pasitos pegados"~~ — resuelto en F2H41 fix
  lateral. walkSpeed 4→5.5 m/s, crouchSpeed 2→3, headbob freq 5→3.5 Hz,
  amp 0.04→0.05.
- ~~Spawn no centrado~~ — resuelto en F2H41 fix lateral. Cambio de
  `(-4.5, 1.6, 7.5)` legacy a `(0, 1.6, 0)` en 4 lugares (Editor + Player
  defaults + resets).
- ~~HUD mezcla español/inglés~~ — resuelto en F2H41 i18n unification.
  Todo el HUD ahora en inglés (HEALTH / AMMO / STAMINA / OBJECTIVE /
  PAUSED / CONTINUE / OPTIONS / EXIT TO X). Sienta baseline para futura
  selección de idioma.

## Post-F2H40 (2026-05-09) — Fix físicas Floor scale-RigidBody desync cerrado

### Histórico resuelto

- ~~Bug físicas Floor scale-RigidBody desync~~ — resuelto en F2H40
  (`v1.30.0-fase2-hito40`). Auto-sync `halfExtents = Transform.scale
  * 0.5` para Box bodies + cache `lastSyncedHalfExtents` para detectar
  cambios + `setBodyHalfExtents` (existente, preserva pose + velocity
  + contacts via Jolt `BodyInterface::SetShape`). Aplicado en
  `EditorScene::updateRigidBodies` y `PlayerApplication::updatePhysics`.
  Cubre 2 vectores: scale via gizmo/Inspector + halfExtents editado
  directo en Inspector.

## Post-F2H39 (2026-05-09) — HUD framework extensible cerrado

- ~~**HUD widgets diferidos** (CompassBar / ObjectiveText / KillFeed /
  Stamina / CRT scanline)~~ — **5 de 7 resueltos en F2H41**
  (`v1.31.0-fase2-hito41`). Quedan 2 que se diferencian en arquitectura
  o scope:
  - **Mini-map / Radar** (CoD/Fallout): topdown view del mundo
    cercano. **Diferido a hito propio mediano** — requiere render-to-
    texture, diferente del DrawList puro del framework.
  - **Themes alternativos** (Doom paleta saturada / Fallout monocromo
    verde): toggle desde Lua. **Diferido a hito chico propio** —
    requiere theme runtime + bindings Lua.

### Activos sin orden definido (siguiente ola)

- **Cambiar font del MoodPlayer a Lato** (coherencia visual
  Editor↔Player): F2H38 solo carga Lato en el editor. El Player
  usa init propio (`PlayerApplication_Init.cpp`) que sigue con
  ProggyClean. Fix chico, mismo patrón que F2H38. Hito propio si
  emerge presión de coherencia visual.

- **HUD del juego procedural/minimalista** (interés expresado por dev
  post-F2H35, scope mayor): explorar HUDs en MoodPlayer dibujados
  via shader procedural (ImGui DrawList API o shaders custom) en
  lugar de sprites/textures. Approach Mirror's Edge / Doom Eternal —
  círculos, barras, anillos, hexágonos calculados por math
  (`length(uv-center) < radius`) sin assets. Pros: cero peso de
  assets, escala perfecta, animable trivialmente. Mini-hito futuro
  con scope a definir junto al dev (cobertura: barra de vida,
  munición, mira, indicadores de daño, etc.).

### Histórico resuelto

- ~~Cambiar default font ImGui a Lato~~ — resuelto en F2H38
  (`v1.28.0-fase2-hito38`). Lato Latin Regular a 15px reemplaza
  ProggyClean en el editor. Custom GlyphRanges cubre Basic Latin +
  Latin-1 + General Punctuation subset (em-dash + comillas curvas +
  ellipsis). FA merge ajustada a 13px explicit para satisfacer
  convencion de reference size de ImGui 1.92. Sin re-flow / overflow
  en ningún panel — dev validó *"todo se ve perfecto"* al primer
  arranque.

## Post-F2H37 (2026-05-09) — FontAwesome al resto del editor + polish UX cerrados

- **HUD del juego procedural/minimalista** (interés expresado por dev
  post-F2H35, scope mayor): explorar HUDs en MoodPlayer dibujados
  via shader procedural (ImGui DrawList API o shaders custom) en
  lugar de sprites/textures. Approach Mirror's Edge / Doom Eternal —
  círculos, barras, anillos, hexágonos calculados por math
  (`length(uv-center) < radius`) sin assets. Pros: cero peso de
  assets, escala perfecta, animable trivialmente. Mini-hito futuro
  con scope a definir junto al dev (cobertura: barra de vida,
  munición, mira, indicadores de daño, etc.).

### Histórico resuelto

- ~~Pase de polish UX general continuo (deuda F2H21+F2H22)~~ —
  resuelto en F2H37 (`v1.27.0-fase2-hito37`) fusionado con la
  extensión FontAwesome. Hierarchy con polish multi-select (3
  colores: naranja active / amarillo secundaria / gris hidden);
  Console con 6 toggles para filter por log level + icons; Inspector
  con icons en headers de 13 components (consistencia visual);
  AssetBrowser con icons por tab (Texturas/Meshes/Prefabs/
  Materiales/Scripts/Audio); StatusBar con icons en FPS/mode/sub-mode
  (refuerzo accesible para devs daltonicos sobre el color existente);
  MenuBar con icons en los 6 menus principales + workspace tabs +
  Play/Stop.
- ~~Extender FontAwesome al resto del editor (deuda F2H36)~~ —
  resuelto en F2H37. Consolidación: helper `iconForEntity` en
  `IconHelpers.h` reemplaza el `entityIconStr` duplicado en
  HierarchyPanel + VisGroupsPanel. Header `IconsFontAwesome6.h` crece
  de ~15 a ~45 macros para cubrir entity types, asset types, log
  levels, MenuBar items, workspace tabs.
- ~~Em-dash tofu en Welcome modal (bug pre-existente)~~ — resuelto
  como fix lateral en F2H37 Bloque I. El carácter `—` (U+2014) no
  está en ProggyClean ni en `k_iconRange` de FA. Fix mínimo:
  reemplazar por `-` (U+002D) en EditorUI.cpp (3 ocurrencias). Si en
  el futuro se carga Lato como default, el fix es innecesario.

## Post-F2H36 (2026-05-09) — FontAwesome icons en toolbars del editor de mapas cerrado

### Histórico resuelto

- ~~Iconos image-based del Toolbar (deuda explícita F2H22)~~ —
  resuelto en F2H36 (`v1.26.0-fase2-hito36`) para Toolbar +
  MapEditorTopBar (17 botones). FontAwesome 6 free solid mergeada al
  atlas default de ImGui. Resto del editor (MenuBar, Hierarchy,
  Inspector, AssetBrowser, Console, StatusBar, paneles auxiliares)
  diferido a F2H37 — resuelto.

## Post-F2H35 (2026-05-09) — Polish editor cerrado (UX viewport + Hammer-style visual)

### Activos diferidos sin orden definido

- **Hover preview de face picking en orto** (no solo perspective):
  F2H35 Bloque F solo cubre perspective. Los ortos ya usan wireframe
  color por VisGroup que es feedback razonable, pero pintar la cara
  hovered cyan en orto requeriría pasarle el cursor del orto al
  pickFace + dibujar overlay específico via debugRenderer. Diferido
  — emerge si el dev lo pide.

- **VisGroups jerárquicos / drag desde Hierarchy / auto-asignar a
  current group / lock de VisGroup**: features avanzadas que Hammer
  4 tiene pero F2H33 no entregó. Diferidos hasta que emerja necesidad
  real (Hammer 4 plano cubre el 80/20).

### Histórico resuelto

- ~~Editor abre maximizado por defecto~~ — resuelto en F2H35 Bloque B
  (`v1.25.0-fase2-hito35`). `SDL_GetDesktopDisplayMode` para crear la
  ventana al tamaño del display + flag `SDL_WINDOW_MAXIMIZED`. Fix
  acoplado: dockspace se descuadraba por iniLayout stale del
  `.moodproj` con WorkSize pre-maximize → stamp `k_IniLayoutStamp`
  per-proyecto invalida los iniLayouts viejos.
- ~~Toggle wireframe/render shading en perspective~~ — descartado por
  dev (*"olvdalo, ya tenemos wireframe en el editor de mapas"*).
- ~~Tint del wireframe por color del VisGroup~~ — resuelto en F2H35
  Bloque C. Helper `wireframeColorForEntity` en SceneRenderer_Ortho.
- ~~Color por tipo de entidad~~ — resuelto en F2H35 Bloque D. Paleta
  Hammer-style + iconos 2D nuevos en perspective + cubitos orto +
  fix lateral pickable Trigger/Camera/Particle.
- ~~Labels arriba de point entities~~ — resuelto en F2H35 Bloque E.
  Toggle "Nombres" default ON con persistencia opcional en `.moodproj`.
- ~~Pulir face picking UX~~ — resuelto en F2H35 Bloque F. Hover
  preview cyan en perspective antes de clickear (pickFace cada frame
  si el cursor está sobre el viewport en Face Mode).
- ~~VisGroupsPanel ColorPicker no persistía cambio~~ — bug F2H33
  resuelto en F2H35 (live preview + snapshot pre + push al
  IsItemDeactivatedAfterEdit).
- ~~Gizmo Rotate proporcional al AABB no constante en pantalla~~ —
  bug F2H30 resuelto en F2H35 Bloque E.5. Derivar worldRadius desde
  pixelsPerWorld para que el ring sea ~70 px constantes.

## Post-F2H34 (2026-05-09) — Multi-face material drop cerrado

### Histórico resuelto

- ~~Multi-face material drop~~ — resuelto en F2H34 (`v1.24.0-fase2-hito34`).
  `EditBrushFaceMaterialCommand` extendido a vectores con back-compat
  via constructor 1-cara wrapper. Helper `tryAssignMaterialToSelectedFaces`
  en `DemoSpawners_Drop.cpp` aplica el material a las N caras del set
  con un solo `push_back` del slot.

## Post-F2H33 (2026-05-09) — Hammer Editor cerrado funcional al 100%

### Histórico resuelto

- ~~VisGroups~~ — resuelto en F2H33 (`v1.23.0-fase2-hito33`). Schema
  bump aditivo v13→v14, panel "Grupos" con TreeNode + sub-lista de
  miembros + comandos undoable + hide gates en render/pick/Hierarchy
  (grayed) + Player skip (convención Hammer).
- ~~Multi-select de caras + texture alignment~~ — resuelto en F2H33.
  `selectedFaceIndices` vector + `setSingleFace`/`toggleFace`/
  `containsFace` helpers + active en amarillo distintivo + Inspector
  applyToScope multi + 6 botones Align/Fit/Justify L/R/T/B + checkbox
  "Treat as one face" + `BrushOps` con `computeFaceUvRect` /
  `alignFaceToFace` / `fitFaceToRect` / `justifyFaceToRect`.
- ~~Face picking en 1 click sobre cualquier brush hovered~~ — resuelto
  en F2H33 Bloque C. Pre-F2H33 el `pickFace` solo testeaba el brush
  active → 2 clicks para cambiar. Fix: pickEntity primero, pickFace
  contra el hovered, replace + single si distinto del active.
- ~~Race CMake POST_BUILD MoodEditor + MoodPlayer~~ — resuelto en
  F2H33 Bloque B como refactor colateral. `add_custom_target(
  mood_runtime_files ALL)` centralizado.
- ~~Ctrl++ en teclados ES sin numérico~~ — resuelto en F2H33 como
  fix lateral. Agregar `SDLK_PLUS` al handler del snap step (la tecla
  `+` a la derecha de Ñ en layout español).

## Post-F2H32 (2026-05-09)

### Activos sin orden definido (siguiente ola, post-Hammer)

- **Split fino de `SceneRenderer_Render.cpp`** (deuda chica residual de
  Bloque C extendido del F2H24): el archivo quedó en 590 LOC tras el
  split, sobre el soft-cap 500 pero bajo hard-cap 800. El frame loop
  (renderScene) es una unidad cohesiva con muchas variables locales
  compartidas entre pases — partir más fino requeriría extraer métodos
  privados con muchas dependencias como parámetros sin aportar
  legibilidad. Si emerge presión (ej. agregar pases nuevos lo lleva
  cerca del hard-cap), candidato a método privado por pase: shadow
  bind / Forward+ SSBO upload / PBR static instanced / PBR skinned /
  brushes CSG / particulas. Diferido.

- **Iconos image-based del Toolbar** (deuda explícita F2H22): la toolbar
  actual usa labels en castellano (`Mover`/`Rotar`/`Escala`/etc.) tras
  feedback del dev *"no tenemos iconos para usar para gizmo, en blender
  es como esto..."*. Iconos verdaderos requieren mergear FontAwesome o
  similar al init de ImGui (~5-10 LOC + binario de la font). Bajo
  riesgo, alto impacto visual. Probable hito chico o sub-bloque de un
  hito UX futuro.

- **Pase de polish UX general continuo** (charlado tras F2H21+F2H22):
  F2H22 atacó workspaces (renames + visibility default + arranque
  limpio), Toolbar (componente nuevo) y AssetBrowser (tabs + scroll
  fijo). Quedan otros panels pendientes de auditoría:
  - Inspector: descubribilidad de drop targets (texturas, materiales,
    scripts, prefabs).
  - Hierarchy: feedback visual de selección múltiple.
  - Console: filtrado por categoría con colores.
  - StatusBar: layout y colores informativos.
  Approach: subagente recorre cada panel buscando inconsistencias de UX
  → lista priorizada → fixes acotados sin refactor profundo. Mismo
  patrón que F2H16 / F2H19. Hito propio si emerge presión.

- **Node-graph del Material Editor** (deuda explícita de F2H21 acotado):
  el plan F2 original F2H17 incluía node-graph con shader runtime
  compilation. F2H21 entregó solo preview esférico + Save (~80% del
  valor visual con ~20% del scope). Cuando emerja necesidad de
  materiales complejos (Mix entre 2 texturas, Multiply de máscaras,
  emissive, etc.), abrir hito propio con: tipos de nodo (TextureSample,
  ColorConstant, ScalarConstant, Multiply, Add, Mix, Output con pins
  Albedo/Metallic/Roughness/Normal/AO), conexiones por drag, GLSL
  compilado runtime con cache por hash del grafo, persistencia
  `.material` extendida. Riesgo: refactor del SceneRenderer para
  shaders custom por material (cada grafo distinto = shader distinto,
  rompe batching de F2H4). Probable hito propio si emerge necesidad.

### Diferidos no urgentes (mencionar al dev si se acercan al scope)

- **Validación full del Player con compiledMesh** (deuda menor F2H26):
  el path `useCompiledMesh=true` está implementado y testeado a nivel
  unitario, pero NO se probó end-to-end con `MoodPlayer.exe` cargando
  un proyecto empaquetado. Validar al primer empaquetado real que
  haga el dev — debe ver en logs "SceneLoader: usando compiledMesh
  (N submeshes, brushes individuales NO spawneados)".
- **F6 panel real estilo Blender** (intentado y descartado en F2H27):
  el F6 con sliders de Transform fue redundante con Inspector. El F6
  REAL ajusta params del operador (size del Box, segments del cilindro,
  etc.) — requiere parametrizar comandos con metadata "params editables"
  + UI dedicada + re-ejecución del comando. Hito grande propio si emerge
  necesidad.
- **Preview esférico del Material Editor con interacción de mouse**
  (orbit cam, zoom): F2H21 dejó rotación automática lenta. Nice-to-have
  si emerge en uso real.

## Post-F2H32 (2026-05-09) — histórico resuelto

- ~~Clip tool (2-click plane split)~~ — resuelto en F2H32
  (`v1.22.0-fase2-hito32`). MapTool::Clip + ClipKeepMode {Front, Back,
  Both} + Csg::clipBrushByPlane via add face al brush. Visual feedback
  (marker p1 + linea p1->cursor / p1->p2). UX hints visibles cuando
  faltan pre-condiciones. BooleanOpCommand extendido con kind=Clip.
- ~~Carve UI Hammer-style~~ — resuelto en F2H32. Boton "Carve" en el
  toolbar resta el active brush por todos los brushes que intersectan
  su AABB. Carvers preservados. Reusa BooleanOpCommand kind=Subtract
  con bSnapshot vacio para skipear destroy/recreate de carvers en
  undo. Broadphase por AABB.

## Post-F2H31 (2026-05-08) — histórico resuelto

- ~~Marquee select en orto~~ — resuelto en F2H31 (`v1.21.0-fase2-hito31`).
  Enum nuevo MapTool (Select/CreateBlock/Pincel) en EditorMode.h +
  toolbar reorganizado en 2 secciones. Drag empty space + Select
  dibuja rect amarillo y al soltar hit-testea AABB world.
- ~~Group transform (multi-entity drag)~~ — resuelto en F2H31. La infra
  ya existía desde F2H29 Bloque B (`OrthoDragSession::startPositions`
  ya iteraba `set.selected`); marquee llena el set y el drag-edit
  mueve N entidades juntas con `MultiEditTransformCommand`.
- ~~Snap-to-vertex toggle~~ — resuelto en F2H31. Tecla V o boton "Snap V"
  togglea `m_snapToVertexEnabled`; helper `snapToVertexOrGrid` con
  broadphase AABB + threshold ndc 0.02 (~8 px). Aplicado en pincel
  (rubber band incluido) + block tool corners.
- ~~Auto-close del pincel al click sobre vertex 1~~ — resuelto en F2H31.
  Antes el dev intuitivamente clickeaba vertex 1 de vuelta y eso
  generaba poligono degenerado; ahora si pointsWorld.size() >= 3 y un
  click cae dentro de 1mm de pointsWorld[0] → auto-close.
- ~~Frustum de cámara perspectiva en ortos~~ — resuelto en F2H31. Rect
  amarillo a 4u distancia + 4 lineas tenues desde camPos a las
  esquinas. Camera basis extraida de la transpose del 3x3 del view
  matrix.
- ~~Coords world del cursor en orto~~ — resuelto en F2H31. Label
  `(x.x, y.y, z.z)` gris claro debajo del label de la vista, solo
  cuando `m_liveCursor.hovered`.

## Post-F2H30 (2026-05-08) — histórico resuelto

- ~~Vertex/edge edit en ortos~~ — resuelto en F2H30 (`v1.20.0-fase2-hito30`).
  `Csg::pickVertex/pickEdge` + sub-modos Vertex/Edge en EditorSubMode
  + drag mueve los planos incidentes con snap absoluto WORLD-space +
  rebasing al cierre + `EditBrushGeometryCommand` para Ctrl+Z agrupado.
- ~~Brush poligonal "pincel"~~ — resuelto en F2H30. Tecla B / botón del
  toolbar lateral activa modo; clicks agregan vertices snappeados;
  Enter cierra spawneando un brush prismático via
  `Csg::makePrismBrushFromPolygon`. Validación CCW con auto-revert +
  dedupe de clicks consecutivos en la misma celda.
- ~~Gizmo rotate proporcional al AABB del brush~~ — resuelto en F2H30.
  Radio = `0.6 * max(localAabb.size())` clamp >= 0.5m. Cubre
  BrushComponent (via `bc.brush.localAabb`) y MeshRendererComponent
  (via `MeshAsset::aabbMin/Max`).
- ~~Atajos modales Blender-style~~ — resuelto en F2H30 con esquema
  hibrido tras 3 iteraciones de feedback. Final: W = Translate gizmo;
  E single = Scale gizmo / E doble = modal Scale uniforme; R single =
  Rotate gizmo / R doble = modal Rotate libre. X/Y/Z lockean axis
  durante el modal. G y S removidos (no shortcuts cruzados). Cuadrado
  central de uniform-scale gizmo eliminado.

## Post-F2H29 (2026-05-08) — histórico resuelto

- ~~Drag-edit + block tool en ortos~~ — resuelto en F2H29
  (`v1.19.0-fase2-hito29`). DragState pulse-style en panel +
  OrthoDragSession con MultiEditTransformCommand + OrthoBlockToolSession
  con preview celeste GMod en 4 vistas + spawnBoxBrushAt con rebasing
  a local space. Vertex/edge edit (Bloque D original) diferido a F2H30
  como paquete polish unificado con 3 features nuevas (ver DECISIONS).
- ~~4-viewport Hammer-style layout~~ — resuelto en F2H28
  (`v1.18.0-fase2-hito28`) bajo el label "Editor de mapas". Workspace
  nuevo registrado como 4to default + dockspace 2x2 + 3 ortos
  wireframe + grid 2D shader + click-select cross-viewport + snap
  cycleable Ctrl++/Ctrl+-. Drag-edit / block tool / vertex edit
  diferidos a F2H29 por decisión explícita de split (ver DECISIONS).
- ~~Cull de overlap parcial~~ — resuelto en F2H25
  (`v1.16.0-fase2-hito25`). BSP-style polygon clipping con 3 pre-tests
  críticos. Stats nuevas `culledOverlapTriangles` + `splitFragments`
  en el dialog de compile.
- ~~Runtime-load de mesh compilada en MoodPlayer~~ — resuelto en F2H26
  (`v1.17.0-fase2-hito26`). Schema bump v12→v13 con `compiledMesh`
  opcional aditivo. Componente nuevo `CompiledMeshComponent` + render
  path nuevo. Flag `useCompiledMesh` en SceneLoader (Editor=false,
  Player=true).
- ~~UI layout fijo por defecto~~ — resuelto en F2H25 como fix lateral
  (`imgui.ini` → `imgui_layout_v2.ini`). Pedido del dev: "por defecto
  la UI es fija, luego el user acomoda".

## Post-F2H19 (2026-05-07)

**Backlog vacío** — F2H18 cerró la reorganización de menús; F2H19 cerró la
limpieza de HistoryStack residual (auditoría con subagente confirmó que
los items BAJA quedan fuera de scope alineado con la convención
Blender/Unity).

### Histórico resuelto

- ~~Reorganización de menús `Archivo > Mapa`~~ — resuelto en F2H18
  (`v1.9.0-fase2-hito18`). Mejoras adicionales (toolbar lateral con
  iconos Hammer-style, atajos Ctrl+B, renombre `Brush → Geometría`)
  diferidas si emergen.
- ~~Limpieza HistoryStack residual~~ — resuelto en F2H19
  (`v1.10.0-fase2-hito19`). 2 comandos nuevos
  (`EditBrushFaceMaterialCommand`, `EditScriptComponentCommand`) +
  wireup en 3 sitios. Items BAJA descartados explícitamente
  (acciones del menú Mapa, toggles de modo/selección).

### Diferidos no urgentes (mencionar al dev si se acercan al scope)

Estos NO están en el backlog activo pero quedan registrados para que
emerjan si el dev los pide:

- **F6 panel estilo Blender** (tweak last operator post-hoc) —
  diferido desde F2H16.
- **Vertex / Edge mode** (teclas 1, 2 reservadas en F2H17). Sub-feature
  avanzada no típica de mapping FPS.
- **Multi-selección de caras** (Shift+click sobre múltiples caras del
  mismo brush) — diferido desde F2H17.
- **Undo de exposed property overrides** del ScriptComponent (deuda
  Hito 24) — comando dedicado si emerge.
