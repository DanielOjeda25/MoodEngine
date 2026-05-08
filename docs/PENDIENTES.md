# Pendientes vivos — MoodEngine

> Backlog de tareas conocidas que NO están planificadas en un hito específico
> todavía. Cada vez que cerrás un hito, mové aquí los pendientes que quedaron
> identificados pero no abordados, y eliminá los que ya hayas cubierto.
>
> Para el roadmap de hitos cerrados/programados, ver `HITOS.md`.
> Para el estado actual del proyecto y "dónde estamos", ver `ESTADO_ACTUAL.md`.

---

## Post-F2H28 (2026-05-08)

### Próximo a atacar

- **F2H29 — Block tool + drag-edit + vertex edit en ortos**.
  Continuación natural de F2H28 (que entregó layout + render + grid +
  click-select pero dejó las 3 features de edición afuera por scope).
  - **Block tool**: en una vista orto, click-drag dibuja un rectángulo
    de selección que al soltar crea un Box brush con esas dimensiones
    + altura default sobre el eje perpendicular. Preview en vivo en las
    otras 2 ortos + perspectiva durante el drag.
  - **Drag-edit**: clickear y arrastrar un brush ya existente desde
    cualquier orto lo mueve sobre el plano de esa vista (eje
    perpendicular intacto). Snap aplica al delta `pos = round(pos /
    snap) * snap` usando el `m_hammerSnapStep` que F2H28 ya expone.
    Update en vivo en las otras 3 vistas. Push de
    `MultiEditTransformCommand` al soltar (mismo patrón F2H23 iter 5).
  - **Vertex/edge edit**: teclas `1` / `2` (reservadas en F2H17)
    activan sub-modos en el orto. Click sobre un vertex/edge del brush
    lo selecciona + drag lo mueve con snap. Triangulación se regenera
    al soltar.
  - **Multi-selección de caras** (Shift+click sobre múltiples caras del
    mismo brush) entra como sub-bloque si el flow naturalmente lo
    demanda durante el vertex edit.

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

## Post-F2H28 (2026-05-08) — histórico resuelto

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
