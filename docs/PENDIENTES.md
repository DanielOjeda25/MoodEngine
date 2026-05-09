# Pendientes vivos — MoodEngine

> Backlog de tareas conocidas que NO están planificadas en un hito específico
> todavía. Cada vez que cerrás un hito, mové aquí los pendientes que quedaron
> identificados pero no abordados, y eliminá los que ya hayas cubierto.
>
> Para el roadmap de hitos cerrados/programados, ver `HITOS.md`.
> Para el estado actual del proyecto y "dónde estamos", ver `ESTADO_ACTUAL.md`.

---

## Post-F2H38 (2026-05-09) — Default font ImGui a Lato cerrado

### Próximo a atacar

- **F2H39 — optimización runtime** (definido con el dev). Plan
  emergerá de un Bloque A de profiling Tracy + Performance HUD
  sobre escenas reales — la infra existe desde F2H2. Candidatos
  sin profilar todavía: skinned animation pass, particle update,
  brush mesh rebuild on dirty, físicas con N rigidbodies, audio
  3D attenuation con N sources. Plan a redactar en
  `docs/PLAN_HITO_F2H39.md` cuando arranquemos.

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
