# Plan — F2H33: Organización + face polish (VisGroups + texture alignment)

> **Leer primero:** `ESTADO_ACTUAL.md`, `PENDIENTES.md` "Próximo a atacar",
> `DECISIONS.md` entry F2H32 cierre (clip + carve via BooleanOpCommand
> extendido), entry F2H26 (patrón de schema bump aditivo v12→v13 que F2H33
> reusa para v13→v14), entry F2H17 (sub-modo Face + `activeFaceIndex` que
> F2H33 extiende a multi-select), entry F2H19
> (`EditBrushFaceMaterialCommand` que F2H33 puede extender o reusar).

---

## Objetivo

Cerrar el editor tipo Hammer en su **totalidad funcional**. F2H32 dejó el
flow de geometría completo (block + drag-edit + vertex/edge + pincel +
clip + carve); F2H33 agrega las 2 capas restantes que Hammer 4 tiene y
nosotros todavía no:

1. **VisGroups** — panel nuevo con grupos planos nombrados; cada grupo
   tiene nombre + color + flag hidden + N entities; toggle hide/show
   filtra render + Hierarchy + picking; persiste en `.moodmap` (schema
   bump aditivo v13→v14).
2. **Multi-select de caras** (Shift+click sobre caras del mismo brush) —
   pre-requisito para F2H33 Bloque D. F2H17 dejó solo `activeFaceIndex`
   único; F2H33 lo extiende a `selectedFaceIndices: vector<int>`
   manteniendo `activeFaceIndex` como "última clickeada".
3. **Texture alignment del Face Edit Sheet** — en sub-modo Face con
   N caras seleccionadas, botones **Align to face / Fit / Justify L/R/T/B**
   + checkbox **Treat as one face** que computa el bounding rect 2D
   compartido y aplica la operación de forma uniforme entre todas.

**Beneficios**: cierra el "Hammer feel" en organización (sin VisGroups,
escenas medianas se vuelven inmanejables) y en texturizado (sin alignment,
alinear texturas en caras no rectangulares es trabajo manual de UV
edition). Tras F2H33 = 31/44 hitos. Hammer cerrado funcional al 100%.

---

## Filosofía de diseño

- **Reuso máximo de la infra existente**:
  - VisGroups reusa el patrón de `Hierarchy` para listar entidades + el
    patrón del `Inspector` para editar nombre + color.
  - Hide/show de un VisGroup reusa los gates del render path: cualquier
    entity con `MeshRendererComponent` / `BrushComponent` chequea
    `entity.visgroup.hidden` antes de encolar; el picker hace lo mismo
    antes de testear el rayo.
  - Persistencia reusa el patrón aditivo de F2H26 (v12→v13 con
    `compiledMesh` opcional): un campo `visgroupId: uint64` opcional
    por entity + un array `visgroups: [VisGroup]` en el root del
    `.moodmap`. Archivos sin esos campos cargan con cero grupos y
    `visgroupId == 0` (= "sin grupo").
  - Multi-select de caras reusa el handler de Shift+click ya usado en
    multi-select de entities (F2H23 iter 5) — solo el target cambia.
  - Texture alignment reusa los `axisU` / `axisV` / `shift` / `scale`
    del `BrushFace` que F2H15 ya cachea para texture lock-to-world.
    Ningún cambio al schema de la cara — solo nuevas ops que mutan
    esos campos.
- **Validación incremental**: 4 commits feat (B VisGroups, C multi-select
  caras, D texture alignment, E docs+tag). Cada commit con build OK +
  verificación visual del dev antes de pasar al siguiente. Mismo patrón
  que F2H29-F2H32.
- **Schema bump aditivo, sin breaking change**: archivos `.moodmap` v13
  cargan correctamente con el código v14 (zero VisGroups + entities sin
  `visgroupId`). El flag de versión se actualiza al **guardar** —
  archivos viejos abiertos quedan persistidos como v14 al primer save.
- **VisGroup como "tag" plano**, no jerarquía**: alineado con Hammer 4.
  Jerárquico es scope mucho mayor (tree drag, propagación de
  hide/show, etc.) y no agrega valor 80/20.

---

## Decisiones arquitectónicas

| # | Decisión | Valor |
|---|---|---|
| 1 | VisGroup struct | `{ uint64_t id, std::string name, glm::vec3 color, bool hidden }`. ID 0 reservado = "sin grupo" (default). Generación de ID nueva = `uint64_t nextId = 1; while (existsId(nextId)) ++nextId;` (lineal — VisGroups raramente > 100). |
| 2 | Membership por entity, no por grupo | Cada entity guarda 1 `uint64_t visgroupId` opcional (0 si no pertenece). Más simple que `set<EntityId>` por grupo: lookup O(1) al renderizar entity, sin sincronización entre listas. Tradeoff: una entity solo puede estar en 1 VisGroup a la vez (Hammer 4 también es así). |
| 3 | Hide gate en 3 lugares | Render: `MeshRendererComponent` / `BrushComponent` chequean `visgroupOf(entity).hidden` antes de encolar al render queue. Picking: `pickEntityFromRay` skipea entities ocultas. Hierarchy: las muestra grayed-out (no las oculta — el dev necesita verlas para sacarlas del grupo). |
| 4 | Schema bump v13→v14 aditivo | Schema agrega: array `visgroups: [{ id, name, color, hidden }]` en el root del `.moodmap`. Por entity, campo opcional `visgroupId: uint64`. Flag de versión `schemaVersion: 14` (era 13). Loader pre-v14: array vacío + `visgroupId = 0` para todas las entities. **No hay migración irreversible** — guardar un archivo v13 abierto lo sube a v14 sin pérdida de info. |
| 5 | UI VisGroups: panel propio | Panel nuevo `VisGroupsPanel` registrado en `EditorUI` (default-visible en workspace "Editor de mapas", oculto en otros). Layout: lista vertical de grupos con `[👁/🚫]` toggle hide + label nombre + count `(N entities)` + botón ⋮ menú contextual (Rename, Color, Delete, Add Selected). Botón "+ Nuevo grupo" arriba. |
| 6 | Asignar selected a VisGroup via menú contextual | Click derecho en un grupo → "Add Selected to Group" — agrega todas las entities del SelectionSet actual. Drag entities directo desde Hierarchy = scope mayor (DnD del ImGui), diferido si emerge. |
| 7 | Comandos undoable | Crear / borrar grupo, renombrar, cambiar color, toggle hidden, agregar / quitar entities del grupo → cada uno pushea un command dedicado. Patrón mismo que `EditScriptComponentCommand` de F2H19 (snapshot before/after del campo). Comandos nuevos: `CreateVisGroupCommand`, `DeleteVisGroupCommand`, `EditVisGroupCommand` (rename/color/hidden), `AssignVisGroupCommand` (entities → group). |
| 8 | Multi-select de caras: vector + active | `EditorApplication`: agregar `std::vector<int> m_selectedFaceIndices` que **siempre incluye** `m_activeFaceIndex` como último elemento. Click sin modifier = `selected = {clicked}`, `active = clicked`. Shift+click = toggle (si ya está, remove; si no, add). `active` es siempre el último clickeado (= primary para single-face ops). InspectorPanel detecta size > 1 y muestra label "N caras seleccionadas". |
| 9 | Texture alignment ops | 6 botones nuevos en el Face Edit Sheet: **Align to face**, **Fit**, **Justify L**, **Justify R**, **Justify T**, **Justify B**. Checkbox **Treat as one face** (default off). Ops aplican a TODAS las `selectedFaceIndices` con la math correspondiente. |
| 10 | Align to face | Reset `axisU`, `axisV` a los ejes del plano de la cara (siguiendo la convención Quake/Hammer: `axisU = perpUp(normal)`, `axisV = cross(normal, axisU)`). Reset `rotation = 0`, `shift = (0,0)`, `scale = (1,1)`. Es la op "vuelve a default". |
| 11 | Fit | Computa bounding rect 2D de los vertices de la cara proyectados a `(axisU, axisV)` actuales. Setea `scale = textureSize / rectSize`, `shift = -rectMin * scale`. Resultado: la textura ocupa exactamente el rect de la cara (puede deformar si la cara no es cuadrada). |
| 12 | Justify L/R/T/B | Mantiene scale; shift en U o V para que el borde izq/der/sup/inf de la textura toque el borde correspondiente del rect 2D de la cara. Mismo bounding rect que Fit pero solo afecta `shift` en un eje. |
| 13 | Treat as one face | Cuando ON + N > 1 caras: computa el bounding rect 2D **compartido** uniendo los rects individuales en el espacio `(axisU, axisV)` de la **primera** cara seleccionada (`activeFaceIndex` = la última, que en multi-select es la primera de la iteración por convención de orden estable). Aplica la op (Fit/Justify) con ese rect compartido. Sin esto, cada cara fitearía aparte y se vería texturas de distinto tamaño. |
| 14 | Comando undoable de alignment | Reuso de `EditBrushFaceMaterialCommand` (F2H19): el snapshot ya cubre `axisU/V/shift/scale/rotation`. Si el comando solo soporta 1 cara, extender a multi-cara con vector de snapshots, o nuevo `EditFaceUVCommand`. Decisión final en Bloque D según costo (probable extensión). |

---

## Bloques

### A — Plan F2H33 (este archivo)

### B — VisGroups (panel + persistencia + hide/show + comandos)

- `engine/scene/VisGroup.h` nuevo: struct `VisGroup { uint64_t id, std::string name, glm::vec3 color, bool hidden }`.
- `engine/scene/Scene.h/.cpp`: agregar miembro `std::vector<VisGroup> m_visgroups` + helpers `findVisGroup(id) -> VisGroup*`, `addVisGroup(name, color) -> id`, `removeVisGroup(id)`, `entitiesInGroup(id) -> vector<Entity>` (linear scan, ok hasta ~100 grupos).
- `engine/ecs/components/`: nuevo `VisGroupMembershipComponent { uint64_t groupId }` opcional por entity, default `groupId = 0`. Alternativa más liviana: campo en `TagComponent` o un `unordered_map<EntityId, uint64_t>` en Scene. Decidir según costo de iteración del render queue.
- `engine/render/SceneRenderer.cpp` (y `_Render.cpp`): antes de encolar Mesh/Brush, chequear `scene.findVisGroup(entity.visgroupId).hidden` — skipear si true. Tomar decisión en el partial donde la queue se popula (no en el draw call) para no romper batching.
- `engine/scene/queries/ScenePick.cpp`: en `pickEntityFromRay` y `pickEntityFromMarquee`, skipear entities cuyo grupo está hidden.
- `editor/panels/VisGroupsPanel.{h,cpp}` nuevo: registrado en `EditorUI::registerPanels` con flag `showInWorkspace = MapEditor`. Render:
  - Lista vertical de grupos. Por cada grupo: `[👁/🚫]` toggle (push `EditVisGroupCommand` con `hidden`), label nombre (rename inline al double-click), count `(N entities)`, botón ⋮ (popup con Rename / Color picker / Delete / Add Selected to Group).
  - Botón "+ Nuevo grupo" arriba: spawnea grupo con nombre "Grupo N" + color random pastel (push `CreateVisGroupCommand`).
- `editor/commands/CreateVisGroupCommand`, `DeleteVisGroupCommand`, `EditVisGroupCommand`, `AssignVisGroupCommand` nuevos. Patrón: snapshot before/after del campo afectado. `DeleteVisGroupCommand` también snapshot del groupId de cada entity miembro (para undo).
- `engine/serialization/SceneLoader.cpp` + `SceneSaver.cpp`:
  - Bumpear `kSchemaVersion = 14`.
  - Loader: si el JSON tiene `visgroups` array, parsearlo; si no, `m_visgroups.clear()`. Por entity, parse `visgroupId` opcional (default 0).
  - Saver: emitir el array y el campo siempre (incluso si vacío / 0).
  - Loader detecta `schemaVersion < 14` → log info "Migrando v13→v14 (VisGroups vacíos por default)". No marcar dirty automático — el primer save explícito persiste como v14.
- Validación: dev abre map, click "+ Nuevo grupo" en panel → grupo creado con nombre "Grupo 1". Selecciona 3 brushes → click derecho en grupo → "Add Selected to Group" → los 3 ahora pertenecen. Toggle 👁→🚫 → los 3 desaparecen del viewport + grayed out en Hierarchy + no se pueden picar. Save + reload → state preservado. Ctrl+Z deshace cada operación en orden.

### C — Multi-select de caras (Shift+click)

- `EditorApplication.h`: agregar `std::vector<int> m_selectedFaceIndices`. Mantener `m_activeFaceIndex` = último clickeado (igual que `m_selectionSet.active` para entities). Invariante: `selectedFaceIndices.empty() ⇔ activeFaceIndex == -1`; sino, `activeFaceIndex == selectedFaceIndices.back()`.
- `EditorApplication_Run.cpp` bloque 2.4f (face picking del raycast cuando `subMode == Face`):
  - Sin modifier: `selectedFaceIndices = {clicked}`, `activeFaceIndex = clicked`.
  - Shift+click: si `clicked` ya está → remove (si era el active, `active = back()` o -1 si vacío). Si no está → push back + `active = clicked`.
  - Ctrl+click reservado para futuro (toggle no-shift). Diferido si emerge.
- `editor/panels/InspectorPanel_*.cpp` (sección Face Edit Sheet): si `selectedFaceIndices.size() > 1`, mostrar header "N caras seleccionadas (active = #idx)" + iterar al editar para que los cambios afecten a todas. Comando UV/material existente (`EditBrushFaceMaterialCommand`) probablemente requiere extensión a multi-cara — ver Bloque D.
- `EditorRenderPass.cpp`: el highlight de la cara activa actual (overlay rojo/naranja) ahora itera `selectedFaceIndices` mostrando todas las seleccionadas; la "active" puede tener color ligeramente distinto para que el dev sepa cuál es la primary.
- Validación: dev entra sub-modo Face (tecla 3), click en cara A → selected = {A}. Shift+click en cara B → selected = {A,B}, ambas highlighted. Shift+click en A de nuevo → selected = {B}. Click sin modifier en cara C → selected = {C}.

### D — Texture alignment (Align/Fit/Justify + Treat as one face)

- `engine/world/csg/BrushOps.h/.cpp`: nuevos helpers
  - `alignFaceToFace(face) -> void` — resetea `axisU/V` a los ejes del plano de la cara, shift/scale/rotation a default. Convención Quake/Hammer (Quake3-style: el axis dominante del normal define la base).
  - `fitFaceToBoundingRect(face, brush, scope) -> void` — computa rect 2D de los vertices proyectados a `axisU/V`; setea `scale = textureSizePx / rectSize`, `shift = -rectMin * scale`. `scope` = `Single` (rect de esta cara) o `Group` (rect compartido pasado como param).
  - `justifyFaceLRTB(face, brush, side, scope) -> void` — mismo computo del rect; shift en U (L/R) o V (T/B) para que el borde correspondiente coincida.
  - Helper internal `computeFaceUvBoundingRect(face, brush) -> Rect2D` que ambas usan.
- `editor/panels/InspectorPanel_*.cpp` (sección Face Edit Sheet): agregar 6 botones (Align to face / Fit / Justify L/R/T/B) en grid 3x2 + checkbox **Treat as one face** (default off; visible solo si `selectedFaceIndices.size() > 1`).
- Handler de cada botón: itera `selectedFaceIndices`. Si checkbox ON + N>1 → primero computa rect compartido uniendo los rects individuales **en el espacio `(axisU, axisV)` de la primera cara** (asume axisU/V coherentes; si no, log warn — alineado con convención Hammer "todas las caras del set deben tener el mismo basis" para Treat as one face). Después aplica la op a cada cara con ese rect.
- Comando undoable: extender `EditBrushFaceMaterialCommand` a multi-cara (snapshot vector de pairs `<faceIndex, BrushFace>` before / after) o crear `EditFaceUVCommand` nuevo. Probable extensión — la estructura del comando ya cubre el snapshot por cara, solo cambia el container. Decisión final al implementar.
- Validación: spawn cubo. Tecla 3 → sub-modo Face. Shift+click 3 caras adyacentes. Click "Fit" sin Treat as one face → cada cara muestra la textura completa (puede tener escalas distintas si las caras son distinto tamaño). Toggle Treat as one face. Click "Fit" → las 3 caras comparten un solo wrap de textura coherente. "Justify L" → la textura se shifta a la izquierda en las 3. Ctrl+Z deshace todo agrupado (1 comando para los 3 cambios de cara).

### E — Cierre

- Test manual completo (dev valida visualmente):
  - **VisGroups**:
    - Crea 2 VisGroups "Edificios" + "Vegetación".
    - Asigna 3 brushes a "Edificios", 2 a "Vegetación".
    - Toggle 🚫 en "Edificios" → 3 brushes desaparecen del viewport, no pickables, grayed en Hierarchy.
    - Save + reload → state preservado.
    - Ctrl+Z varias veces → toggle vuelve, asignaciones vuelven, grupos eliminados.
  - **Multi-select caras**:
    - Sub-modo Face. Shift+click 4 caras de un cubo → header del Face Sheet dice "4 caras seleccionadas (active = #2)".
  - **Texture alignment**:
    - Sin checkbox: Fit aplicado → 4 fits independientes.
    - Con checkbox: Fit aplicado → 1 wrap unificado.
    - Justify L → texture shifteada a izq en las 4.
    - Align to face → 4 caras vuelven al default UV.
    - Ctrl+Z deshace cada op agrupada (1 paso por click).
- Update docs (`HITOS.md`, `ESTADO_ACTUAL.md`, `DECISIONS.md`, `PENDIENTES.md`), archive plan en `docs/archive/plans/PLAN_HITO_F2H33.md`, commit + tag `v1.23.0-fase2-hito33` + push.

---

## Lo que NO entra en F2H33 (queda para futuro)

- **VisGroups jerárquicos** (sub-grupos, drag tree). Diferido — Hammer 4 es plano.
- **Drag-and-drop entities desde Hierarchy a VisGroup**: scope mayor (DnD ImGui custom). El menú contextual "Add Selected" cubre el flow básico.
- **Auto-asignar a "current VisGroup"**: convención Hammer donde brushes nuevos se asignan al grupo actualmente seleccionado. Diferido — fácil de agregar como toggle si el dev lo pide.
- **Filtrar Hierarchy por VisGroup activo**: nice-to-have. Por ahora todas las entities visibles, las hidden grayed.
- **Lock de VisGroup** (caras no editables aun cuando visibles): scope incremental, agregar si emerge.
- **Hollow tool** (pendiente desde F2H32): scope incremental sobre carve, no típico del flow básico.
- **Vertex split** (mover vertex genera 2 brushes): scope mayor. Diferido.
- **Texture alignment Treat as one face con axisU/V heterogéneos**: si las caras seleccionadas tienen basis distintos (caras no-coplanares con normales rotadas), el rect compartido no es well-defined. Hammer 4 muestra warning en ese caso. F2H33 log warn + skip de la op. Refinar si emerge.

---

## Tradeoffs conocidos

- **VisGroup membership 1-a-N (entity en máximo 1 grupo)**: Hammer 4 es así. Si emerge necesidad de membership múltiple (entity en "Edificios" + "Norte"), refactor a `unordered_set<uint64_t> groupIds` por entity. Costo: lookup hidden ahora es O(K) donde K = membership avg, en lugar de O(1). Aceptable hasta K~5.
- **Schema bump v13→v14 aditivo, sin downgrade**: archivos guardados v14 NO cargan en código v13 (el campo `visgroups` se ignora pero `schemaVersion: 14` rechaza por el check del loader). Misma política que v12→v13 de F2H26. Si emerge necesidad real de "abrir mapas nuevos en builds viejos", agregar `--ignore-version-mismatch` flag — diferido.
- **Hide en render NO es por componente disable**: las entities ocultas siguen en el `Scene` con su `Transform` activo + sus colliders presentes en physics. Solo el render queue + el picker las skipea. Si el dev tiene scripts en una entity oculta, los scripts siguen corriendo (alineado con Hammer — un VisGroup no es "disable", es "ocultar en viewport"). Si emerge necesidad real, agregar flag `disableScripts` separado al VisGroup.
- **Multi-select caras: mantener invariante active ∈ selected**: si el dev shift-clickea una cara que ya está active → remove → active pasa a `back()`. Edge case: solo 1 cara seleccionada y shift-click la misma → selected vacío + active = -1 → vuelve al state "ninguna seleccionada". Aceptable.
- **Treat as one face requiere axisU/V coherentes**: si el dev mezcla caras con basis distintos (ej. arriba + costado de un cubo, normales 90°), el rect 2D compartido no tiene sentido geométrico. F2H33 detecta y log warn. Refinar a "rotar axisU de las caras secundarias para alinear con la primera" si emerge necesidad.
- **Alignment ops pueden producir UVs negativas o > 1**: convención Quake/Hammer es UV ilimitada (la textura wraps). El renderer ya lo soporta (sampler GL_REPEAT default). No requiere cambio.
- **Comando de alignment multi-cara puede explotar el HistoryStack**: 4 caras × 6 botones = 24 entries por sesión "alinear 1 brush". Mitigación: el comando es 1 entry por click independientemente del N de caras (snapshot vector). Idéntico al patrón `MultiEditTransformCommand` para multi-entity.
