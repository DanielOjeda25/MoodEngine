# Plan — F2H28: Editor de mapas 4-viewport (MVP visual + select + grid)

> Nota de naming: el workspace se llama **"Editor de mapas"** en la UI.
> Internamente seguimos hablando del estilo "Hammer" como inspiración
> técnica (4 viewports + grid 2D) — pero el label visible al user es
> "Editor de mapas".

> **Leer primero:** `ESTADO_ACTUAL.md`, `PENDIENTES.md` "Próximo a atacar",
> `DECISIONS.md` entries de F2H7 (workspaces) + F2H22 (toolbar) +
> F2H23 (dockspace polish).

---

## Objetivo

Habilitar un workspace **"Editor de mapas"** estilo *Valve Hammer Editor* (Source SDK)
con 4 viewports simultáneos: 1 perspectiva 3D + 3 ortográficas
(Top XY / Front XZ / Side YZ) en wireframe + grid 2D snap. El workspace
se registra como un tab más en la fila superior de workspaces (al lado de
Layout / Programar / Materiales) y permite **navegar y seleccionar** los
brushes desde cualquier vista.

**Beneficios**: alineación precisa entre vistas, percepción 2D del
mapa estilo BSP-mapping clásico, base sólida para el flow de
**creación/edición de brushes desde ortográficas** que entra en F2H29.

---

## Filosofía de diseño

- **Fidelidad Hammer** como norte visual y UX. Las 3 ortográficas se
  ven como en Hammer: wireframe negro sobre fondo gris claro + grid
  con líneas mayores cada 64 unidades + crosshair (dot central) +
  label de la vista en esquina superior izquierda ("top (x/y)" /
  "front (x/z)" / "side (y/z)").
- **Split en 2 hitos para no romper todo de una** (decisión confirmada
  por dev): F2H28 entrega **layout + render multi-viewport + grid +
  selección**. F2H29 entrega **block tool + drag-edit + vertex edit**.
- **Reuso máximo de la infra existente**:
  - `WorkspaceManager` ya tiene API para agregar workspaces — sumar
    "Editor de mapas" a `defaultWorkspaces()` + `isValidWorkspaceName`.
  - `Dockspace.cpp` ya tiene el patrón `buildXxxWorkspace` —
    sumar `buildHammerWorkspace` con dockspace partido en 4.
  - `ViewportPanel` ya soluciona FBO display + input mouse + click-
    select + drag-drop. La nueva variante ortográfica (`OrthoViewportPanel`)
    reusa ese patrón con axes configurables y sin rotación de cámara.
  - `SceneRenderer` ya hace 1 frame completo. Para 4 viewports
    extendemos el flow para aceptar **N renders por frame**, cada uno
    a su FBO, con su cámara y modo (PBR vs wireframe).
- **Grid 2D como shader fullscreen post-process** sobre la imagen
  ortográfica, NO como geometría dibujada. Es independiente del
  contenido del FBO y se calcula en NDC con snap step pasado por
  uniform — barato y nítido a cualquier zoom.
- **Cámara ortográfica con `world units` directos**: no hay FOV. El
  zoom es `worldHeight` (alto del frustum en unidades del mundo).
  Pan en `(right, up)` del plano de la vista.
- **Snap setting global** en la toolbar del workspace Hammer (no
  per-viewport). Cycleable con `Ctrl++` / `Ctrl+-` (subir / bajar);
  valores [1, 2, 4, 8, 16, 32, 64, 128]. Default 16.
- **Paleta de colores Valve + Garry's Mod (Opción A)**:
  - Wireframe brushes: celeste GMod `#6CC1E5`.
  - Grid mayor (cada 8×snap): naranja Valve `#F58220`.
  - Grid menor: gris medio `#7A7A7A`.
  - Brush seleccionado: naranja saturado `#FF9933`.
  - Fondo orto: gris claro `#C8C8C8`.
  - Crosshair central: naranja Valve.

---

## Decisiones arquitectónicas

| # | Decisión | Valor |
|---|---|---|
| 1 | Workspace "Editor de mapas" | 4to workspace, registrado en `WorkspaceManager::defaultWorkspaces()` + filtro `isValidWorkspaceName`. Migración aditiva (no rompe `.moodproj` v12). |
| 2 | Layout dockspace | 2x2 cuadrantes equal-size. Top-left = Top, Top-right = Perspectiva, Bottom-left = Front, Bottom-right = Side. (Convención Hammer.) |
| 3 | Panels nuevos | `OrthoViewportPanel` con 3 instancias (Top/Front/Side). La perspectiva sigue usando el `ViewportPanel` existente. |
| 4 | FBOs | 4 FBOs independientes (uno por viewport). Cada uno con su propio resize. Costo: ~4× memoria (color+depth attachments). Aceptable para MVP. |
| 5 | Render path orto | `SceneRenderer::renderOrthoView(camera, fbo, axes)` nuevo. Aplica `glPolygonMode(GL_LINE)` en todos los meshes. Sin shadows / SSAO / postFX (simplifica). |
| 6 | Cámara orto | struct nuevo `OrthoCamera { glm::vec2 panOffset, float worldHeight, ViewAxes axes }`. ViewAxes = enum {TopXY, FrontXZ, SideYZ}. Matriz proj = `glm::ortho(...)`, view = lookAt según axes. |
| 7 | Grid 2D shader | `shaders/grid2d.vert/.frag` fullscreen. Uniforms: panOffset, worldHeight, snapStep, viewportSize. Líneas mayores cada 8×snap. |
| 8 | Crosshair | Dibujado con ImGui drawlist (no shader) en el centro del panel — punto + 2 líneas cortas. Color orange para diferenciar de las líneas del grid. |
| 9 | Click-select multi-viewport | `ViewportPick::pickFromOrtho(camera, ndc)` nuevo: ray ortográfico (dirección = -axes.forward, origen = ndc desproyectado al plano lejano). Selección compartida entre las 4 vistas (ya es global vía `EditorScene::selection`). |
| 10 | Snap setting | `EditorState::hammerSnapStep` (u32, default 16). Toolbar Hammer-only o atajo Ctrl++/Ctrl+-. F2H28 lo lee desde el grid shader y lo muestra en label arriba-derecha de cada orto. F2H29 lo aplica al drag-edit. |

---

## Bloques

### A — Plan F2H28 (este archivo)

### B — Workspace "Editor de mapas" + dockspace 4 cuadrantes

- `WorkspaceManager.cpp`:
  - Agregar `Workspace{"Editor de mapas", {}}` a `defaultWorkspaces()`.
  - Extender `isValidWorkspaceName` para aceptar "Editor de mapas".
- `Dockspace.cpp`:
  - Función nueva `buildHammerWorkspace(ImGuiID dockspaceId)`. Layout:
    1. Split horizontal 50/50 → top half + bottom half.
    2. Cada half: split vertical 50/50 → 4 cuadrantes.
    3. Dockear los 4 panels: "Hammer Top", "Viewport" (perspectiva
       central, reusa el panel existente), "Hammer Front", "Hammer Side".
  - Extender `buildLayoutForWorkspace` con la rama `"Editor de mapas"`.
- Validación: arrancar editor, switch al tab Hammer → ver 4 cuadrantes
  (3 vacíos por ahora, 1 con la perspectiva 3D).

### C — Panel ortográfico + cámara

- `editor/panels/scene/OrthoViewportPanel.h/.cpp` nuevo:
  - 3 enum constructor (TopXY / FrontXZ / SideYZ) define axes + label.
  - Render: ImGui::Image del FBO + grid 2D shader pass por debajo de
    la imagen + crosshair via ImGui drawlist.
  - Input: pan MMB drag (delta NDC * worldHeight → panOffset),
    zoom scroll (worldHeight *= 1.1^-wheel, clamp [1, 8192]).
  - Click-select: igual que el ViewportPanel pero `pendingClick` se
    consume con `pickFromOrtho`.
- `engine/scene/cameras/OrthoCamera.h` nuevo (o agregado al
  `Camera.h` si encaja): struct + helpers `viewMatrix()`,
  `projMatrix(aspect)`, `worldRayFromNdc(ndcX, ndcY)`.
- `editor/EditorState`: agregar `OrthoCamera m_hammerTopCam`,
  `m_hammerFrontCam`, `m_hammerSideCam`.

### D — Render path ortográfico (4 FBOs)

- `EditorApplication`: 3 FBOs nuevos (`m_orthoTopFB`, `_FrontFB`, `_SideFB`)
  + el existente del viewport perspectivo (`m_viewportFB`).
- `SceneRenderer`: método nuevo `renderOrthoView(scene, camera, fbo,
  selectedEntities)`:
  - Bind FBO → clear gris claro.
  - `glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)` antes del pase.
  - 1 pase: brushes + meshes con shader PBR pero color forzado a
    `vec3(0.05)` (líneas negras). Selected → resaltado en color cyan.
  - `glPolygonMode(GL_FRONT_AND_BACK, GL_FILL)` al salir.
  - Sin shadows / forward+ light grid / SSAO / postFX (innecesario en
    wireframe). Reusa `m_pbrShader` con uniform `uOrthoOverride=1`
    para forzar color flat o un shader nuevo simplificado
    (`shaders/wireframe_ortho.vert/.frag`) — decisión final en
    Bloque D según costo.
- `EditorApplication_Frame`: invocar 4 renders por frame sólo si el
  workspace activo es Hammer (en otros workspaces los 3 ortos no se
  renderizan, ahorra el ~3× costo CPU).

### E — Grid 2D shader

- `assets/shaders/grid2d.vert` + `grid2d.frag`:
  - Vert: fullscreen quad estándar (gl_VertexID magic).
  - Frag: para cada pixel, computa `worldXY = panOffset + ndc * 0.5
    * worldHeight * vec2(aspect, 1)` y dibuja línea si
    `mod(worldXY.x, snapStep) < lineWidth` (y mismo en Y). Líneas
    mayores cada `snapStep * 8` (color más oscuro).
- `OrthoViewportPanel`: dibuja el grid PRIMERO (background pass),
  después la imagen del FBO con blending alpha (mesh sobre grid). O
  alternativa: shader del grid se ejecuta DENTRO del FBO antes del
  brush pass, simplifica el panel.

### F — Click-select multi-viewport

- `engine/scene/queries/ViewportPick.h/.cpp`:
  - Nuevo `pickFromOrtho(camera, ndcX, ndcY, scene) -> Entity`.
  - Ortho ray: origen en `camera.panOffset + ndc * 0.5 *
    worldHeight * (right, up)` proyectado a `farPlane`. Dirección:
    `-camera.forward()`. Reusa el broadphase AABB ya existente.
- `EditorApplication`: en el frame loop, consumir `clickSelect` de
  los 3 OrthoViewportPanel y aplicar selección global (la selección
  existente en `EditorScene::selection` ya se refleja en el
  Inspector + outline de las 4 vistas).
- Validación: clickear un brush en Top → se ve seleccionado en
  Front, Side y Perspectiva. El Inspector muestra sus props.

### G — Cierre

- Snap setting: variable `m_hammerSnapStep` en EditorState +
  atajo Ctrl++ / Ctrl+- (registrados sólo en workspace Hammer).
  Mostrar valor actual en label "Grid: 16u" arriba a la derecha de
  cada orto panel.
- Test manual: dev arranca editor, switch a Hammer, debe ver 4
  vistas, hacer click-select cross-viewport, cambiar snap.
- Update docs (`HITOS.md`, `ESTADO_ACTUAL.md`, `DECISIONS.md`,
  `PENDIENTES.md`), commit + tag `v1.18.0-fase2-hito28` + push.

---

## Lo que NO entra en F2H28 (queda para F2H29)

- **Block tool** (dibujar rectángulo en una orto → crear brush). Es
  el flow más característico de Hammer pero requiere infra de
  preview en otros 2 viewports + commit final.
- **Drag-edit** de brushes existentes (mover en orto con grid snap +
  ver actualización en otros viewports).
- **Vertex/edge edit** desde ortográficas.
- **Frustum de cámara perspectiva** dibujado en ortos (líneas que
  muestran el view frustum del 3D en cada vista 2D).
- **Coordenadas del cursor** en world units mostradas en cada orto
  (label "X: 32, Y: 64").
- **Multi-selección con rectángulo de selección** en orto (clásico
  Hammer marquee select).

Estas features dependen de la base estable de F2H28; meterlas todas
juntas inflaría el hito a 12+ bloques con bugs cruzados.

---

## Tradeoffs conocidos

- **~4× costo de frame en workspace Hammer**: 4 renders independientes
  vs. 1 actual. Mitigación: shadow pass + light grid se ejecutan
  UNA SOLA VEZ (no por viewport) y se reusan para los 4 renders. En
  F2H28 los ortos no usan shadows así que el costo extra es solo
  por la geometría (~ +60-80% por viewport adicional, no ×). Si
  emerge presión real, optimizar con frustum culling per-viewport.
- **Memoria de FBOs**: 4 color+depth attachments a resolución del
  panel. A 1280×720 son ~30 MB total. Aceptable.
- **Shader override para wireframe**: si el dev cambia a un shader
  PBR custom (rama node-graph del Material Editor en el futuro), el
  override flat color de las ortos puede romper. Riesgo bajo
  porque F2H21 dejó solo el preview esférico (sin nodos). Si emerge
  el problema, el shader `wireframe_ortho` dedicado lo evita.
