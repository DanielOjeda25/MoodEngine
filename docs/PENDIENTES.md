# Pendientes vivos — MoodEngine

> Backlog de tareas conocidas que NO están planificadas en un hito específico
> todavía. Cada vez que cerrás un hito, mové aquí los pendientes que quedaron
> identificados pero no abordados, y eliminá los que ya hayas cubierto.
>
> Para el roadmap de hitos cerrados/programados, ver `HITOS.md`.
> Para el estado actual del proyecto y "dónde estamos", ver `ESTADO_ACTUAL.md`.

---

## Post-F2H34 (2026-05-09) — Multi-face material drop cerrado

### Próximo a atacar

- **F2H35 — UX viewport polish (mini-hito)** — identificado en
  validación de F2H34. Dos features pequeñas que comparten dominio
  (UX del viewport) y son rápidas de cerrar juntas:
  - **Editor abre maximizado por defecto**: actualmente arranca en
    ventana ~1280x720 y el dev tiene que reajustar el dockspace
    manualmente al maximizar. Fix: `SDL_WINDOW_MAXIMIZED` flag al
    crear la ventana (o `SDL_MaximizeWindow` post-create). One-liner.
  - **Toggle wireframe/render shading en viewport** estilo Blender:
    botones overlay en la zona superior del viewport para alternar
    entre vista renderizada (PBR actual) y wireframe. Requiere flag
    en ViewportPanel + pasar a SceneRenderer / RenderPass para
    aplicar `glPolygonMode(GL_LINE)` o equivalente al pase opaco.
    ~1-2h, toca render pipeline.

### Activos sin orden definido (siguiente ola)

- **Hammer-style visual polish** (paquete candidato a hito chico
  futuro, posiblemente unificable con F2H35 si crece): cuando el dev
  pidió cerrar F2H33 mencionó varias mejoras visuales que NO entraron
  por scope. Agruparlas en un hito propio porque comparten dominio
  (render del wireframe + iconos del editor de mapas):
  - **Tint del wireframe por color del VisGroup**: cada brush en un
    grupo se renderea con el color del grupo (~30 LOC, alto valor
    visual — el color ya está en el `VisGroup` struct).
  - **Color por tipo de entidad**: lights=amarillo, audio=naranja,
    triggers=verde, etc. (mapa "tipo → color" + tintar el sphere icon
    actual). Hammer-style.
  - **Labels arriba de point entities**: texto del tag de la entidad
    arriba del icono en world. Hammer original tiene toggle "Map →
    Show Helpers".
  - **Pulir face picking UX**: el dev reportó que la selección de
    caras es difícil incluso después del fix de F2H33 Bloque C.
    *"funciona, es complejo supongo de entender, pero se puede pulir
    a futuro"*. Casos borde: rayo en bordes de cara, caras muy
    inclinadas respecto a la cámara, caras chicas dentro de brushes
    grandes. Investigar si hay margen en `Csg::pickFace` (epsilon /
    triangulación) o si es UX puro (preview hover).

- **VisGroups jerárquicos / drag desde Hierarchy / auto-asignar a
  current group / lock de VisGroup**: features avanzadas que Hammer
  4 tiene pero F2H33 no entregó. Diferidos hasta que emerja necesidad
  real (Hammer 4 plano cubre el 80/20).

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
