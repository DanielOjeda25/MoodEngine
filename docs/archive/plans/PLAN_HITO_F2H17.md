# Plan — F2H17: Face Mode estilo Hammer + Material per-cara

> **Leer primero:** `ESTADO_ACTUAL.md` (F2H16 cerrado),
> `MEMORY.md > project_pending_face_mode.md`. Era F2H15 en plan
> original ("Brush selection y multi-edit"), después renumerado y
> reordenado por inserción de F2H13 (multi-selección) + F2H16
> (limpieza HistoryStack).

---

## Objetivo

Implementar el **sub-modo "Face Mode"** del editor estilo
Hammer/TrenchBroom: el dev presiona la tecla **3**, el viewport
empieza a pickear **caras individuales** del brush activo (no
brushes enteros), el Inspector muestra UV params SOLO de la cara
seleccionada, y se puede asignar **material distinto a cada
cara** con drop. Es el feature que el dev pidió explícitamente:
"lo quiero igual que el hammer".

**Lo que entra en F2H17**:
- **Sub-modo Face** del editor con toggle por tecla **3**
  (reservar `1` para vertex mode futuro, `2` para edge mode futuro).
- **Picking de cara** (raycast contra polígonos individuales,
  no AABB).
- **Outline visual de cara** seleccionada distinto al outline
  del brush (cyan brillante vs naranja).
- **Inspector per-cara**: UV editor opera sobre la cara
  seleccionada, no sobre todas.
- **Material per-cara real**: `BrushComponent` extendido a
  `std::vector<MaterialAssetId> materials` (slots);
  `face.materialIndex` indexea ese vector.
- **Multi-material rendering**: SceneRenderer agrupa vertices
  por slot y hace 1 draw call por material distinto (mismo
  patrón que `MeshRendererComponent` con submeshes).
- **Schema bump `.moodmap` v11 → v12** con back-compat
  aditiva (v11 con `material` único se lee como
  `materials = [material]`).
- **Comandos undoable**: `EditFaceUVCommand`, `EditFaceMaterialCommand`.

**Lo que NO entra en F2H17**:
- **Vertex / Edge mode** (teclas 1 y 2). Reservadas para hitos
  futuros si emerge necesidad. Editar vertices/aristas es
  sub-feature avanzada (no típica de mapping FPS).
- **Drag interactivo de UVs sobre canvas 2D** (panel UV editor
  visual estilo Blender). Diferido — el UI de F2H17 sigue siendo
  sliders numéricos, ahora per-cara.
- **Multi-selección de caras** (Shift+click sobre múltiples
  caras del mismo brush). Diferido si emerge.
- **Compilación brush → mesh estática unificada al guardar**
  (era F2H17 antes del rerouting; ahora es F2H18).

---

## Filosofía de diseño

- **Mode toggle minimal**: la tecla **3** activa Face Mode.
  Tecla `Esc` o **3** otra vez vuelve a Object Mode. Reservar
  `1` (vertex) y `2` (edge) sin implementar todavía. Aprende
  la convención Blender que el dev ya conoce.
- **Picking de cara reusa la geometría existente**: cada cara
  ya tiene su polígono computado en `buildBrushMesh`. El helper
  `pickFace` triangula la cara (fan) e intersecta cada
  triángulo con el ray. No requiere algoritmo nuevo.
- **Material per-cara via slots**: mismo patrón que
  `MeshRendererComponent` (`materials` array). `face.materialIndex`
  es un slot index, no un MaterialAssetId directo. Eso permite
  que dos caras compartan material sin duplicación.
- **Multi-material rendering**: `buildBrushMesh` produce N
  `BrushSubmeshData` (una por slot). `BrushComponent` cachea
  N `unique_ptr<IMesh>`. SceneRenderer itera y hace 1 draw call
  por submesh con su material correspondiente. Mismo patrón
  que el render de `MeshRendererComponent` con submeshes.
- **Back-compat full** del `.moodmap`: el campo `material`
  (string) de v11 se lee como `materials = [string]` en v12.
  Mapas viejos cargan visualmente idénticos.

---

## Decisiones arquitectónicas

| # | Decisión | Valor |
|---|---|---|
| 1 | Tecla del toggle | **3** (Face Mode). Reservar `1` (vertex), `2` (edge) sin implementar todavía. `Esc` también vuelve a Object Mode. |
| 2 | EditorMode actual | El enum `EditorMode { Editor, Play }` se mantiene; agregamos `EditorSubMode { Object, Face }` como **estado paralelo** que solo aplica en `EditorMode::Editor`. Toggle del submode no afecta Play Mode. |
| 3 | SelectionSet | Extender con `s32 activeFaceIndex = -1`. -1 = ninguna cara. Solo válido cuando submode == Face Y `selectionSet.active` tiene BrushComponent. |
| 4 | Picking de cara | Helper `Csg::pickFace(brush, rayOrigin, rayDir, worldMatrix) -> std::optional<u32>`. Triangula cada cara con fan-triangulation desde su centroide; ray-triangle intersection (Möller-Trumbore); devuelve el índice de cara más cercana. |
| 5 | Outline de cara | Render dedicado: dibuja el polígono de la cara seleccionada con líneas brillantes (cyan `(0.0, 0.95, 1.0)`). Brush entero queda con outline gris tenue (no naranja activo) cuando estás en Face Mode. |
| 6 | Material per-cara — modelo | `BrushComponent.material` (singular `MaterialAssetId`) → `materials` (`std::vector<MaterialAssetId>`). El `face.materialIndex` ya existente (F2H11) ahora indexea este vector. Default: `materials = [0]` (1 slot con material default). |
| 7 | `BrushMeshData` multi-submesh | `BrushMeshData` cambia a `std::vector<BrushSubmeshData>` con `{vertices, indices, materialSlot}` por entry. `buildBrushMesh` agrupa caras por `face.materialIndex`. |
| 8 | `BrushComponent.meshCache` | De `unique_ptr<IMesh>` (singular) a `std::vector<unique_ptr<IMesh>>` (uno por slot). |
| 9 | SceneRenderer brushPass | Itera submeshes, bind material correspondiente al slot, draw. Mismo flow que `drawMeshRenderer` existente. |
| 10 | Schema `.moodmap` v11 → v12 | Brush JSON cambia: `"material": "path"` → `"materials": ["path1", "path2", ...]`. Back-compat: si v11 tiene `material` (singular), se lee como `materials = [material]`. Cada `face` ya tiene `materialIndex` desde F2H11 (sin cambio). |
| 11 | Drop de material/textura sobre cara | En Face Mode + cara seleccionada: el drop **agrega un slot nuevo** al `materials` array si el material no está ya, asigna `face.materialIndex` al slot correspondiente. Push `EditFaceMaterialCommand`. |
| 12 | Drop sobre brush sin Face Mode | Comportamiento F2H15 conservado: asigna al slot 0 (primer material). |
| 13 | UV editor en Face Mode | Inspector muestra UV params de SOLO la cara activa. Si no hay cara activa, fallback al modo global (todas las caras). `EditFaceUVCommand` para per-cara. `EditBrushUVCommand` (F2H16) sigue existiendo para Object Mode. |
| 14 | Comandos undoable | `EditFaceUVCommand` (1 face snapshot pre/post) y `EditFaceMaterialCommand` (face.materialIndex pre/post + materials array pre/post). |

---

## Tipos públicos

```cpp
// editor/application/EditorMode.h (extendido)
enum class EditorSubMode {
    Object,  // default — pickea brushes enteros, gizmo de transform
    Face,    // F2H17 — pickea caras individuales del brush activo
    // Vertex, Edge — reservados para hitos futuros
};
```

```cpp
// editor/selection/SelectionSet.h (extendido)
struct SelectionSet {
    std::vector<Entity> selected;
    Entity active;
    s32 activeFaceIndex = -1;  // F2H17: indice de cara seleccionada,
                                // o -1 si no hay (Object Mode o sin
                                // cara picked).
};
```

```cpp
// engine/world/csg/Brush.h (extendido)
namespace Mood::Csg {

/// @brief F2H17: ray-cast contra los polígonos de cada cara del
///        brush. Devuelve el índice de la cara más cercana al
///        rayOrigin, o nullopt si el ray no toca ninguna cara.
///        Usado por el viewport picking en Face Mode.
std::optional<u32> pickFace(const Brush& brush,
                              const glm::vec3& rayOrigin,
                              const glm::vec3& rayDir,
                              const glm::mat4& worldMatrix);

} // namespace Mood::Csg
```

```cpp
// engine/world/csg/BrushMesh.h (cambio breaking)
namespace Mood::Csg {

struct BrushSubmeshData {
    std::vector<BrushMeshVertex> vertices;
    std::vector<u32>             indices;
    u32                          materialSlot;  // index dentro de bc.materials
};

struct BrushMeshData {
    std::vector<BrushSubmeshData> submeshes;
};

/// @brief F2H17: agrupa caras por materialIndex y produce N
///        submeshes (una por slot distinto).
BrushMeshData buildBrushMesh(const Brush& brush,
                              const glm::mat4& worldMatrix = glm::mat4(1.0f));

} // namespace Mood::Csg
```

```cpp
// engine/scene/components/BrushComponent.h (extendido)
struct BrushComponent {
    Csg::Brush brush;
    /// @brief F2H17: slots de material. face.materialIndex indexea
    ///        este vector. Default: 1 slot con material 0
    ///        (look "blank gris"). Antes era un solo MaterialAssetId.
    std::vector<MaterialAssetId> materials = {0};
    std::vector<std::unique_ptr<IMesh>> meshCache;  // 1 por submesh
    bool dirty = true;
    bool anyFaceLockToWorld = false;
    glm::mat4 lastWorldMatrix{1.0f};
};
```

---

## Bloques

### A — Plan F2H17 (este archivo)

Documento del hito.

### B — Mode toggle + SelectionSet activeFaceIndex

- `EditorMode.h`: agregar `enum class EditorSubMode { Object, Face }`.
- `EditorApplication`: `EditorSubMode m_subMode = EditorSubMode::Object`.
- Tecla **3** alterna submode (en Editor Mode solamente).
- `SelectionSet`: campo `s32 activeFaceIndex = -1`.
- StatusBar muestra "Mode: Face" cuando submode != Object (similar al "Editor Mode" / "Play Mode").
- Cuando submode cambia a Object, reset `activeFaceIndex = -1`.

Tests: `test_selection_set.cpp` +2 cases (activeFaceIndex default, set/reset).

### C — Picking de cara

- `Csg::pickFace(brush, rayOrigin, rayDir, worldMatrix) -> std::optional<u32>`.
- Algoritmo: para cada cara, computar polígono en world (vertices via
  `collectFaceVertices` + transform por `worldMatrix`), fan-triangulate
  desde centroide, ray-triangle intersection (Möller-Trumbore), trackear
  el `t` mínimo positivo.
- Tests `test_csg_pick_face.cpp`: ray hit cada cara de una box,
  ray que no toca nada, ray paralelo a una cara (no debe hit), ray
  con worldMatrix transformado, brush degenerado.

### D — Wireup de picking en viewport

- `EditorApplication::processViewportClick` (o donde sea el handler):
  cuando submode == Face Y `selectionSet.active` tiene `BrushComponent`,
  llamar `pickFace(bc.brush, rayOrigin, rayDir, worldMatrix)` antes
  del `pickEntity` normal. Si hit → `selectionSet.activeFaceIndex = idx`.
  Si no hit → mantener faceIndex (no resetear; el user puede haber
  clickeado fuera para limpiar).
- Render outline de cara seleccionada: `EditorRenderPass` cuando
  `subMode == Face` Y `activeFaceIndex != -1`, dibujar el polígono
  de esa cara con líneas cyan brillantes.

### E — Inspector per-cara + EditFaceUVCommand

- `InspectorPanel`: cuando `submode == Face` Y cara seleccionada,
  los widgets del UV editor leen/escriben `bc.brush.faces[activeFaceIndex]`
  (no todas).
- Header de la sección cambia a "UV (Cara N)" en lugar de "UV (Brush)".
- `EditFaceUVCommand` análogo a `EditBrushUVCommand` pero con
  snapshot de UNA cara (struct `FaceUVSnapshot` con los 6 params
  de 1 face + face index). Push usando el patrón
  `IsItemActivated`/`IsItemDeactivatedAfterEdit` igual que F2H16.
- Tests: `test_edit_face_uv_command.cpp` con round-trip + tag inexistente
  + face index fuera de rango (no crashea).

### F — Material per-cara + Multi-material rendering

- `BrushComponent`:
  - `material` (singular) → `materials` (vector).
  - `meshCache` (singular) → vector<unique_ptr<IMesh>>.
  - Helper `materialOrMissing(slot)` con fallback al slot 0.
- `Csg::BrushMeshData` y `buildBrushMesh`:
  - Cambiar a `std::vector<BrushSubmeshData>`.
  - Agrupar caras por `face.materialIndex` durante el build.
  - Cada submesh tiene su propio `vertices` + `indices` + `materialSlot`.
- `Csg::brushMeshDataToInterleaved`:
  - Cambiar a per-submesh: helper `brushSubmeshToInterleaved(submesh)`.
- `SceneRenderer::PBR::brushPass`:
  - Iterar `bc.brush` → `buildBrushMesh` → loop sobre submeshes:
    - Obtener material del slot via `bc.materials[submesh.materialSlot]`.
    - Bind material como en `drawMeshRenderer`.
    - Crear/reusar IMesh del slot (`bc.meshCache[s]`).
    - 1 draw call por submesh.
- `EditFaceMaterialCommand` (drop sobre cara individual):
  - Si el material no está en `bc.materials`, agregar slot nuevo.
  - Asignar `face.materialIndex = slot`.
  - Snapshot pre/post de `face.materialIndex` + `bc.materials` para undo.
- `processViewportTextureDrop` y `processViewportMaterialDrop`:
  - Si submode == Face Y se hizo `pickFace` exitoso → `EditFaceMaterialCommand`.
  - Sino, comportamiento F2H15 (asignar a slot 0 / `bc.materials[0]`).
- Schema `.moodmap` v11 → v12:
  - JSON brush: `"material": "path"` → `"materials": ["path1", ...]`.
  - Back-compat: si v11 tiene `material` (singular), se carga como
    `materials = [value]`.
- Tests:
  - `test_csg_brush.cpp` +cases: buildBrushMesh con caras de
    distintos slots produce N submeshes correctos.
  - `test_brush_persistence.cpp` +cases: round-trip de
    `materials` array; v11 con `material` único carga como
    array de 1.
  - `test_edit_face_material_command.cpp` con round-trip.

### G — Cierre + tag

- `docs/HITOS.md`: F2H17 [x] con resumen.
- `docs/ESTADO_ACTUAL.md`: F2H17 cerrado, F2H16 a "anterior".
- `docs/DECISIONS.md`: entrada con las decisiones (mode toggle
  con tecla 3 reservando 1/2; multi-material via slots reusando
  patrón MeshRenderer; back-compat aditiva del schema).
- Archivar plan.
- Tag `v1.8.0-fase2-hito17`.

---

## Suite

Cases nuevos esperados:
- `test_selection_set.cpp`: +2 cases (activeFaceIndex).
- `test_csg_pick_face.cpp`: ~8-10 cases.
- `test_csg_brush.cpp`: +5-7 cases (multi-submesh).
- `test_brush_persistence.cpp`: +3-5 cases (materials array + v11 back-compat).
- `test_edit_face_uv_command.cpp`: ~5-6 cases.
- `test_edit_face_material_command.cpp`: ~4-5 cases.

**Suite estimada: 552 → ~585 cases / 8158 → ~8350 asserts.**

---

## Riesgos

- **Picking de cara con polígonos no triangulares**: las caras de
  brush son polígonos planares (no triángulos). Triangulación con
  fan desde centroide es válida porque las caras son convexas
  (intersección de half-spaces) — pero atención a casos edge:
  cara con 3 vertices (triángulo), cara con N vertices muy chica.
  Mitigación: tests con esos edge cases.
- **Multi-material rendering breaking change**: `BrushMeshData`
  cambia de struct a struct-of-vector. Cualquier callsite que
  use `data.indices.empty()` directamente debe migrarse a iterar
  submeshes. Mitigación: cambio centralizado, audit todos los
  call sites antes del wireup.
- **Schema v11 → v12 mezclado con back-compat**: leer un v11 que
  tiene `"material"` (singular) y convertir a v12 con
  `"materials"`. Probar que un mapa pre-F2H17 cargado en F2H17 se
  ve idéntico.
- **`bc.meshCache` vector vs IMesh único**: cualquier callsite que
  asuma 1 IMesh por brush debe migrarse a iterar el vector. Audit
  + grep por `bc.meshCache`.
- **Tecla 3 vs ImGui input**: ImGui consume teclas en algunos
  contextos (input boxes, drag). El handler de tecla 3 debe
  chequear `!ImGui::GetIO().WantCaptureKeyboard`.

---

## Criterios de cierre

- [ ] Toggle de Face Mode con tecla 3 (e Esc para volver).
- [ ] Picking de cara funciona end-to-end: spawn brush, Face Mode,
      click sobre una cara → outline cyan distinto.
- [ ] Inspector per-cara: cambiar uvScale en Face Mode afecta
      SOLO esa cara.
- [ ] Drop de material sobre cara individual asigna SOLO esa cara.
- [ ] Multi-material rendering: brush con 2+ caras de distintos
      materiales se renderea correcto en viewport.
- [ ] Schema v12 con back-compat verificado (mapa v11 carga igual).
- [ ] Suite ~585 cases verde.
- [ ] `docs/HITOS.md`, `ESTADO_ACTUAL.md`, `DECISIONS.md` actualizados.
- [ ] Plan archivado.
- [ ] Tag `v1.8.0-fase2-hito17` pusheado.

---

## Refs canónicas

- TrenchBroom face attributes: https://trenchbroom.github.io/manual/latest/#face_attributes
- Hammer Editor 4 Face Edit: workflow histórico de mapping (clickear cara con tool dedicado, asignar textura, ajustar UVs).
- Blender Edit Mode con `1`/`2`/`3` para vertex/edge/face: la convención que el dev quiere replicar.
- Möller-Trumbore ray-triangle intersection: https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm
