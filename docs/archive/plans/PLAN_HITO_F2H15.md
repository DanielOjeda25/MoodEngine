# Plan — F2H15: Texturizado por cara + UV editor (lock-to-world)

> **Leer primero:** `ESTADO_ACTUAL.md` (F2H14 cerrado),
> `PLAN_FASE2.md` sección 4. Era F2H14 en plan original; renumerado
> +1 por el adelanto de multi-selección como F2H13. F2H16-F2H17
> también renumeran +1.

---

## Objetivo

Materializar **el** feature que justificó el approach brush
implícito de F2H11 (vs manifold mesh-based): **lock-to-world UVs**.
Cada cara del brush computa sus propias UVs desde un par de ejes
tangentes derivados del plano. Cuando `lockToWorld=true`, las UVs
se proyectan desde la posición **world** del vertex — la textura
"queda fija al mundo" y no se deforma al mover/escalar el brush.

**Lo que entra en F2H15**:
- `BrushFace` extendido con UV params per-cara (autoinicializados
  con tangent basis canónico desde la normal).
- `buildBrushMesh` recompute UVs reales (no más proyección planar
  trivial del F2H11).
- `BrushComponent` cachea `anyFaceLockToWorld`; si true, dirty se
  fuerza cada frame que cambia el transform.
- Sección **UV (Brush)** nueva en Inspector con sliders + toggle
  lock-to-world. **Aplica a todas las caras** del brush en F2H15.
- Schema bump `.moodmap` v10 → v11 con back-compat aditiva.

**Lo que NO entra en F2H15**:
- Material per-cara (cada cara con un material distinto). F2H16
  (face mode) lo entrega cuando esté la selección de cara
  individual.
- UV editor visual con preview 2D de la textura desplegada
  per-cara. Diferido — el UI de F2H15 es sliders numéricos, no
  drag interactivo de UVs sobre canvas. Si emerge necesidad, hito
  futuro propio (UI compleja).
- UV unwrapping automático tipo Blender. CSG con planos por cara
  no necesita unwrapping — cada cara es un polígono planar, las
  UVs son trivialmente proyección 2D.
- Escalado proporcional al área de la cara ("UV per metro
  cuadrado"). Diferido — los `uvScale` son free units por defecto.

---

## Filosofía de diseño

- **UVs computadas, no almacenadas en vertex**: cada cara guarda
  los **parámetros** (uAxis, vAxis, scale, offset, rotation, lock).
  Las UVs por vertex se computan en `buildBrushMesh` desde esos
  params. Cero vertex data extra en `BrushFace`.
- **Auto-tangent-basis al crear el brush**: cuando una primitiva
  se construye (`makeBoxBrush`, `makeCylinderBrush`, etc.) las
  caras reciben uAxis/vAxis canónicos derivados de la normal.
  Helper `defaultTangentBasis(normal) -> (uAxis, vAxis)`. El user
  puede override después editando los params.
- **Lock-to-world como flag por-cara**: distinto a un flag global
  por brush. Permite (en F2H16) tener algunas caras con world UVs
  y otras con local. En F2H15 el toggle del UI cambia las 6/N caras
  a la vez, pero el modelo soporta granularidad.
- **Dirty agresivo cuando hay lockToWorld**: el SceneRenderer
  rebuildea la mesh cada frame que el transform cambia si
  `bc.anyFaceLockToWorld == true`. Costo aceptable: ~1-3ms por
  brush en Debug. Si emerge cuello, optimizar a "rebuild solo si
  el delta del transform > kEps".
- **Back-compat full**: brushes v10 sin UV params se cargan con
  defaults sensatos. Comportamiento visual = igual que F2H14
  (proyección planar trivial cuando los params son default).

---

## Decisiones arquitectónicas

| # | Decisión | Valor |
|---|---|---|
| 1 | Donde viven los UV params | En `BrushFace` (F2H11 ya tenía placeholders comentados). Materialización de la promesa. |
| 2 | Tipos de los params | `glm::vec3 uAxis`, `glm::vec3 vAxis`, `glm::vec2 uvOffset`, `glm::vec2 uvScale`, `f32 uvRotation` (radianes), `bool lockToWorld`. |
| 3 | Defaults razonables | uAxis/vAxis = `defaultTangentBasis(normal)`, uvOffset = (0,0), uvScale = (1,1), uvRotation = 0, lockToWorld = false. |
| 4 | Auto-tangent-basis | Idéntico a `buildTangentBasis` que ya existe en `BrushMesh.cpp` para la proyección planar. Promovido a helper público en `Plane.h` o utility de CSG. |
| 5 | Cómputo de UV en `buildBrushMesh` | `uv = mat2(rotation) * (dot(p, uAxis), dot(p, vAxis)) * uvScale + uvOffset`. Si `lockToWorld`, `p` es world (post-transform); si no, local. |
| 6 | Pasaje de world matrix a `buildBrushMesh` | Nueva firma `buildBrushMesh(brush, worldMatrix)` opcional con default `glm::mat4(1.0f)`. Cuando se pasa identity, lock-to-world degenera a local (consistente). |
| 7 | Cache `anyFaceLockToWorld` en `BrushComponent` | Computado al rebuild de mesh; chequeado por el SceneRenderer cada frame. Si true → forzar dirty cuando cambia transform. |
| 8 | Detección de cambio de transform | Cada frame comparar `lastTransformWorldMatrix` (cacheada en `BrushComponent`) con `t.worldMatrix()`. Diff > kEps en cualquier elemento → dirty. |
| 9 | UV editor en Inspector | Sección nueva "UV (Brush)" después de "Brush (CSG)". Sliders: uvScale (DragFloat2 con minClamp 0.01), uvRotation (DragFloat con paso 1°), uvOffset (DragFloat2). Toggle: lock-to-world. **Aplica a todas las caras del brush** en F2H15. |
| 10 | Edición undoable | NO en F2H15. Editar UVs marca dirty, pero no pushea comando al HistoryStack. F2H16 con face mode + comandos dedicados maneja undo. Si emerge fricción, agregar antes. |
| 11 | Schema bump `.moodmap` v10 → v11 | Cada `face` en JSON gana 6 campos opcionales. Back-compat aditiva: faces v10 cargan con defaults. |

---

## Tipos públicos modificados

```cpp
// engine/world/csg/Brush.h (extendido)
namespace Mood::Csg {

struct BrushFace {
    Plane plane;
    u32 materialIndex = 0;
    // F2H15: UV params per-cara.
    glm::vec3 uAxis{1.0f, 0.0f, 0.0f};
    glm::vec3 vAxis{0.0f, 1.0f, 0.0f};
    glm::vec2 uvOffset{0.0f, 0.0f};
    glm::vec2 uvScale{1.0f, 1.0f};
    f32 uvRotation = 0.0f;        // radianes
    bool lockToWorld = false;
};

/// @brief F2H15: ejes tangentes ortonormales canonicos derivados
///        de la normal de un plano. Estables: depende solo de
///        la normal. Mismo algoritmo que el helper privado
///        buildTangentBasis de BrushMesh.cpp en F2H11, ahora
///        publico para que las primitivas lo usen al inicializar
///        sus caras.
void defaultTangentBasis(const glm::vec3& normal,
                          glm::vec3& outUAxis,
                          glm::vec3& outVAxis);

} // namespace Mood::Csg
```

```cpp
// engine/world/csg/BrushMesh.h (firma actualizada)
namespace Mood::Csg {

/// @brief Construye la mesh triangulada del brush. F2H15: las UVs
///        se computan desde los UV params per-cara. Si alguna
///        cara tiene `lockToWorld=true`, las posiciones world se
///        derivan via `worldMatrix * p_local` y se usan para el
///        calculo de UV. Si todas son lockToWorld=false,
///        worldMatrix es ignorada (el caller puede pasar identity).
BrushMeshData buildBrushMesh(const Brush& brush,
                              const glm::mat4& worldMatrix = glm::mat4(1.0f));

} // namespace Mood::Csg
```

```cpp
// engine/scene/components/BrushComponent.h (extendido)
namespace Mood {

struct BrushComponent {
    Csg::Brush brush;
    MaterialAssetId material = 0;
    std::unique_ptr<IMesh> meshCache;
    bool dirty = true;

    // F2H15: cache para detectar cambios de transform que
    // requieren rebuild (lock-to-world).
    bool anyFaceLockToWorld = false;
    glm::mat4 lastWorldMatrix{1.0f};
};

} // namespace Mood
```

---

## Bloques

### A — Plan F2H15 (este archivo)

Documento del hito. Nace en `docs/`; al cierre se mueve a
`docs/archive/plans/`.

### B — UV params en `BrushFace` + auto-tangent-basis

Modificar `engine/world/csg/Brush.h`:
- Agregar los 6 campos a `BrushFace`.
- Declarar `defaultTangentBasis(normal, &uAxis, &vAxis)`.

Modificar `Brush.cpp`:
- Implementar `defaultTangentBasis` (lógica copiada del
  `buildTangentBasis` privado de BrushMesh.cpp).
- En `makeBoxBrush`: cada cara llama a `defaultTangentBasis`
  para inicializar `uAxis` / `vAxis`.

Modificar `Primitives.cpp`:
- Mismo update en `buildPrismaticBrush`, `makeSphereBrush`,
  `makePyramidBrush`, `makeWedgeBrush`. Cada cara recibe tangent
  basis auto desde su normal.

Tests nuevos en `test_csg_brush.cpp` (~5-7 cases):
- `BrushFace` default tiene uAxis/vAxis válidos (no zero).
- `defaultTangentBasis` produce ejes ortogonales unitarios
  ortogonales a la normal.
- `defaultTangentBasis` es estable (misma normal → mismos ejes).
- `makeBoxBrush` produce caras con UV defaults sensatos.

### C — `buildBrushMesh` con UVs reales + lockToWorld

Modificar `BrushMesh.h`:
- Cambiar firma a `buildBrushMesh(brush, worldMatrix=identity)`.

Modificar `BrushMesh.cpp`:
- Para cada vertex, computar `(u, v)` desde los params:
  - Si `lockToWorld`: `p_for_uv = vec3(worldMatrix * vec4(p_local, 1))`.
  - Sino: `p_for_uv = p_local`.
  - `uvBeforeRot = (dot(p_for_uv, uAxis), dot(p_for_uv, vAxis))`.
  - `uvRotated = mat2(rotation) * uvBeforeRot`.
  - `uvFinal = uvRotated * uvScale + uvOffset`.

Tests en `test_csg_brush.cpp` (+5-7 cases):
- Box con uvScale=2 → UVs son 2x lo que serían con scale=1.
- Box con uvOffset=(0.5, 0.5) → UVs desplazadas.
- Box con uvRotation=90° → UVs rotadas.
- Box con lockToWorld=true: trasladar el brush en world space
  no cambia las UVs respecto al world (textura "fija al piso").
  Specifically: build mesh con worldMatrix=I, después con
  worldMatrix=translate(10,0,0) → las UVs en el segundo caso
  reflejan la traslación.

### D — Cache `anyFaceLockToWorld` + dirty agresivo

Modificar `BrushComponent.h`:
- Agregar `anyFaceLockToWorld` y `lastWorldMatrix`.

Modificar `SceneRenderer::renderScene` (brushPass):
- Antes de chequear `dirty`, chequear si la entidad tiene
  `bc.anyFaceLockToWorld == true` y `t.worldMatrix() != bc.lastWorldMatrix`
  → marcar dirty.
- Tras rebuild de mesh, actualizar `bc.anyFaceLockToWorld`
  recorriendo las caras y `bc.lastWorldMatrix = t.worldMatrix()`.

Tests de regresión en `test_csg_brush_ops.cpp` (no nuevos —
ya pasan porque las ops booleanas no tocan UV params).

### E — UV editor en Inspector

Modificar `editor/panels/scene/InspectorPanel.cpp`:
- Sección nueva "UV (Brush)" después de "Brush (CSG)".
- Solo mostrar si la entidad tiene `BrushComponent`.
- Widgets:
  - `DragFloat2` "uv scale" con clamp [0.01, 100].
  - `DragFloat` "uv rotation (deg)" con conversion a radianes.
  - `DragFloat2` "uv offset".
  - `Checkbox` "lock to world".
- Cualquier edición → marca `bc.dirty = true` y aplica el cambio
  a TODAS las caras del brush.
- Read-only display: cantidad de caras del brush, info "%d caras
  con lock-to-world".

### F — Persistencia `.moodmap` v10 → v11

Modificar `engine/scene/serialization/JsonHelpers.h`:
- Bump `k_MoodmapFormatVersion` 10 → 11.
- Doc del schema en el comentario.

Modificar `engine/scene/serialization/SceneSerializer.cpp`:
- `serializeBrush`: para cada face escribir uAxis, vAxis, uvOffset,
  uvScale, uvRotation, lockToWorld.
- `parseBrush`: leer con defaults sensatos si los campos faltan
  (back-compat v10).

Modificar `engine/scene/serialization/SceneLoader.cpp`:
- `applyEntitiesToScene`: al reconstruir el brush, copiar los UV
  params de `SavedBrushFace` a `BrushFace`.

Tests en `test_brush_persistence.cpp` (+5 cases):
- Round-trip de UV params per-cara.
- Mapa v10 sin UV params se carga con defaults sensatos.
- lockToWorld=true se preserva.

### G — Cierre + tag

- `docs/HITOS.md`: F2H15 [x] con resumen.
- `docs/ESTADO_ACTUAL.md`: F2H15 cerrado, F2H14 a "anterior".
- `docs/DECISIONS.md`: entrada con las 4-5 decisiones (UV params
  como datos, computed-not-stored, lock-to-world flag por-cara,
  dirty agresivo, edición no-undoable en F2H15).
- Archivar `PLAN_HITO_F2H15.md` a `docs/archive/plans/`.
- Tag `v1.6.0-fase2-hito15`.

---

## Suite

Cases nuevos esperados:
- `test_csg_brush.cpp`: +12-15 cases (tangent basis, UV
  computation, lockToWorld).
- `test_brush_persistence.cpp`: +5 cases (round-trip UV params).

**Suite estimada: 522 → ~545 cases / 7830 → ~7950 asserts.**

---

## Riesgos

- **Performance del dirty agresivo con muchos brushes lock-to-world**:
  si el dev pone 100 brushes con lock-to-world y arrastra el grid
  por la escena, son 100 × ~2ms = 200ms por frame de rebuild → 5
  FPS. Mitigación: limitar a "rebuild solo si distance(prev,
  current) > kEps" antes de marcar dirty. Si emerge en uso real,
  optimizar.
- **Tangent basis inestable cerca de polos**: el algoritmo
  `defaultTangentBasis` elige un helper axis (Y o X) según donde
  apunta la normal. Cerca de la transición (normal cerca de
  (0.9, 0, 0.4)) el flip puede causar UVs discontinuas entre
  caras. Mitigación: tests con normales "patológicas". Si emerge,
  cachear el tangent basis al construir la primitiva en lugar
  de recomputarlo por cara.
- **Schema bump impacta proyectos existentes**: los `.moodmap`
  guardados con v10 hoy van a cargarse en v11 con UV params
  default. Comportamiento visual igual al de antes. Validar con
  un proyecto que el dev ya tenga.
- **UV rotation en grados vs radianes**: el UI muestra grados
  (intuitivo); el storage es radianes. Conversion `glm::radians(deg)`
  al leer del UI; `glm::degrees(rad)` al mostrar. Tests cubren
  el round-trip.

---

## Criterios de cierre

- [ ] `BrushFace` con UV params + `defaultTangentBasis` declarado
      en header.
- [ ] `buildBrushMesh` computa UVs reales con lock-to-world.
- [ ] Inspector tiene sección "UV (Brush)" funcional.
- [ ] `BrushComponent` cachea `anyFaceLockToWorld` + dirty
      agresivo cuando lockToWorld activo.
- [ ] Schema `.moodmap` v11 con back-compat.
- [ ] Suite ~545 cases verde.
- [ ] Validación visual: spawn brush, asignar material con textura,
      cambiar uvScale → textura escala. Toggle lockToWorld + mover
      brush → textura "fija al mundo".
- [ ] `docs/HITOS.md`, `ESTADO_ACTUAL.md`, `DECISIONS.md` actualizados.
- [ ] Plan archivado.
- [ ] Tag `v1.6.0-fase2-hito15` pusheado.

---

## Refs canónicas

- TrenchBroom UV editor: https://trenchbroom.github.io/manual/latest/#face_attributes
- Quake .map format face spec: ` ( p1 ) ( p2 ) ( p3 ) <texture> <uOffset> <vOffset> <rotation> <uScale> <vScale> ` — el approach histórico de UV params per-cara que F2H15 replica.
- "Real-Time Rendering" 4th ed., cap 6 — proyecciones planares y tangent basis.
