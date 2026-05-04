# Plan — F2H11: CSG arquitectura base

> **Leer primero:** `ESTADO_ACTUAL.md` (sub-fase 2.1 cerrada),
> `PLAN_FASE2.md` sección 4 (este hito era F2H9 plan original = "CSG
> arquitectura base"; renumerado a F2H11 por el desplazamiento de
> F2H4-F2H8 y la postergación de F2H9 docs).

---

## Objetivo

Establecer los **cimientos de CSG** (Constructive Solid Geometry)
estilo Hammer / TrenchBroom: tipos puros para representar brushes
3D, algoritmo determinístico brush → mesh para la primera primitiva
(**Box arbitraria con orientación**, no AABB), integración con el
ECS y el render pipeline existentes, y persistencia en `.moodmap`.

**Lo que NO entra en F2H11**:
- Operaciones booleanas (Union/Subtract/Intersect) — F2H12.
- Primitivas más allá de Box (cilindro, prisma, esfera, pirámide,
  wedge) — F2H13.
- Texturizado por cara con UV editor — F2H14.
- Selección visual con gizmos / multi-edit — F2H15.
- Compilación brush → mesh estática unificada con weld + cull
  internas — F2H16.

F2H11 es **arquitectura base + primer brush vivo en pantalla**. Sin
booleanos ni UV editor todavía: cada Box brush se renderiza con un
material plano por cara, sin deformación.

---

## Filosofía de diseño

- **Brush implícito = N planos**. Cada cara es la intersección de
  un plano con el resto. Esta representación es lo que hace
  posible más adelante el lock-to-world UV (F2H14) y la edición
  no destructiva.
- **Tipos puros, sin OpenGL ni ImGui**. Todo el código de
  `engine/world/csg/` es testeable headless con doctest.
- **Determinístico**: dado el mismo brush, `buildBrushMesh()`
  produce siempre el mismo VBO. Esto es crítico para diff-friendly
  serialización y testing.
- **Reusar lo existente**: `glm` para math, `meshoptimizer` (ya
  integrado en F2H6) queda disponible para futuros hitos de weld,
  `nlohmann/json` para persistencia, `EnTT` para components.
- **No reinventar matemática**: el algoritmo de plane intersection
  está documentado en "Real-Time Collision Detection" (Christer
  Ericson) cap 5. Trabajaremos siguiendo ese libro y el código
  open-source de TrenchBroom como referencia.

---

## Decisiones arquitectónicas

| # | Decisión | Valor |
|---|---|---|
| 1 | Carpeta del subsystem | `src/engine/world/csg/` |
| 2 | Representación de brush | Array de N planos (`Plane = {normal, distance}`); cada plano lleva metadata de cara (`materialIndex`, UV params placeholders) |
| 3 | Convención del plano | `dot(normal, p) - distance = 0`; `normal` apunta hacia afuera del brush; punto `p` está fuera del brush si `dot(normal,p) - distance > 0` |
| 4 | Triangulación | Fan triangulation desde el vértice 0 de cada cara (válida: caras de brush son **convexas por construcción**) |
| 5 | Tolerancia geométrica | `kEpsilon = 1e-4f` para tests de coincidencia de planos / vertex deduplication local por cara |
| 6 | Componente ECS | `BrushComponent { Brush brush; bool dirty; }` — `dirty` triggea rebuild de la mesh dinámica |
| 7 | Mesh runtime | Cada `BrushComponent` tiene una `MeshAsset` interna ("ephemeral", no persistida); rebuild on-demand cuando `dirty=true` |
| 8 | Persistencia `.moodmap` | Bump de schema: nuevo array `brushes[]` con `{transform, faces[]={planeNormal, planeDistance, materialIndex}}` |
| 9 | Render path | Por ahora no-instanced; cada brush emite 1 draw call con su mesh propia (instancing de brushes queda para F2H16 con la mesh unificada) |
| 10 | Material por defecto | Reusa el material "checker" o "default" que el editor ya tiene |
| 11 | UI | Botón **"Mapa > Añadir Box Brush"** y gizmo de Transform reusado del scene tree |

---

## Tipos públicos del subsystem

```cpp
// engine/world/csg/Plane.h
namespace Mood::Csg {

// dot(normal, p) - distance = 0  =>  p sobre el plano
// normal apunta hacia afuera del brush
struct Plane {
    glm::vec3 normal;
    float distance;
};

constexpr float kEpsilon = 1e-4f;

inline float signedDistance(const Plane& p, const glm::vec3& point);
inline bool intersectThreePlanes(const Plane& a, const Plane& b,
                                 const Plane& c, glm::vec3& outPoint);

} // namespace Mood::Csg
```

```cpp
// engine/world/csg/Brush.h
namespace Mood::Csg {

struct BrushFace {
    Plane plane;
    u32 materialIndex = 0;
    // UV params placeholders para F2H14:
    // glm::vec3 uAxis{1,0,0};
    // glm::vec3 vAxis{0,1,0};
    // glm::vec2 uvOffset{0,0};
    // glm::vec2 uvScale{1,1};
    // float uvRotation = 0.0f;
    // bool lockToWorld = false;
};

struct Brush {
    std::vector<BrushFace> faces;  // mínimo 4 para ser sólido cerrado
    AABB localAabb;                // computed cache
};

// Genera Box arbitraria orientada por una matriz.
// La box unitaria va de (-0.5,-0.5,-0.5) a (+0.5,+0.5,+0.5),
// 6 caras con normales canónicas, transformada por `worldFromLocal`.
Brush makeBoxBrush(const glm::mat4& worldFromLocal,
                   u32 materialIndex = 0);

// Computa la AABB local del brush a partir de sus planos.
// Usa intersección de cada triplete de planos.
AABB computeBrushAabb(const Brush& brush);

} // namespace Mood::Csg
```

```cpp
// engine/world/csg/BrushMesh.h
namespace Mood::Csg {

struct BrushMeshVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;          // F2H11: planar projection trivial; F2H14: real
    u32 materialIndex;
};

struct BrushMeshData {
    std::vector<BrushMeshVertex> vertices;
    std::vector<u32> indices;
    std::vector<u32> facesPerMaterial; // submesh ranges para multi-mat
};

// Algoritmo principal: brush implícito → mesh triangulada.
// 1. Para cada cara, intersectar su plano con todos los demás
//    planos para obtener candidatos a vértice.
// 2. Filtrar vértices que NO pertenecen al brush (signed distance
//    positiva contra cualquier otro plano).
// 3. Ordenar los vértices alrededor del centroide de la cara
//    (orden CCW visto desde fuera).
// 4. Fan-triangulate la cara desde su vértice 0.
// 5. Atributos: normal = plane.normal; uv = projection planar
//    trivial (lockToWorld vendrá en F2H14).
BrushMeshData buildBrushMesh(const Brush& brush);

} // namespace Mood::Csg
```

---

## Bloques

### A — Plan F2H11 (este archivo)

Documento del hito. Cierra al commit inicial.

### B — Tipos puros + tests del math de planos

Crear:
- `src/engine/world/csg/Plane.h`
- `src/engine/world/csg/Plane.cpp` (si las funciones no son inline)
- `tests/test_csg_plane.cpp` (15-20 cases):
  - `signedDistance` con punto sobre, dentro, fuera del plano
  - `intersectThreePlanes` con 3 planos no coplanares (resuelve)
  - `intersectThreePlanes` con 2 planos paralelos (no resuelve)
  - `intersectThreePlanes` con 3 planos coincidentes (no resuelve)
  - Casos numéricos: planos casi paralelos cerca de `kEpsilon`
  - Determinismo: misma entrada → misma salida bit a bit

Compilar + tests verdes antes de seguir. Commit:
`feat(F2H11 Bloque B): tipos Plane + math basico (intersectThreePlanes)`

### C — Brush + buildBrushMesh + tests

Crear:
- `src/engine/world/csg/Brush.h` y `.cpp` con `makeBoxBrush` y
  `computeBrushAabb`.
- `src/engine/world/csg/BrushMesh.h` y `.cpp` con `buildBrushMesh`.
- `tests/test_csg_brush.cpp` (20-25 cases):
  - `makeBoxBrush` con identity → 6 caras con normales canónicas,
    centro en origen, lados de 1.
  - `makeBoxBrush` con `glm::translate` → AABB se mueve.
  - `makeBoxBrush` con `glm::rotate(45deg, Y)` → AABB crece (caja
    rotada).
  - `buildBrushMesh` de Box unitaria → 24 vértices (4×6), 36 índices
    (6 caras × 2 triángulos × 3 índices), normales correctas, UVs
    no degenerados.
  - `buildBrushMesh` de Box rotada/escalada → mismas counts,
    normales rotadas, posiciones transformadas.
  - Determinismo: 2 calls con la misma input → mismas vertices/
    indices bit a bit.
  - Test de "sanidad volumétrica": el centroide de cada cara está
    dentro del aabb local.

Compilar + tests verdes. Commit:
`feat(F2H11 Bloque C): Brush + makeBoxBrush + buildBrushMesh con tests`

### D — Componente ECS + integración SceneRenderer

Crear:
- `src/engine/scene/components/BrushComponent.h`:
  ```cpp
  struct BrushComponent {
      Mood::Csg::Brush brush;
      std::shared_ptr<MeshAsset> meshCache;  // built lazily
      bool dirty = true;
  };
  ```
- En `SceneRenderer::renderScene`, antes del PBR static pass: barrer
  entidades con `BrushComponent`, si `dirty` → `buildBrushMesh` →
  upload a la `meshCache` (recyclable VBO/EBO) → marcar `dirty=false`.
  Emitir 1 draw call por brush (no-instanced en F2H11).
- Inspector: si la entidad tiene `BrushComponent`, mostrar
  `# faces`, `material por cara` (placeholder: combo box que
  apunta al `materialIndex`), AABB local. Read-mostly por ahora.

Probar abriendo el editor:
- Click derecho en viewport → "Add Brush Box" → aparece cubo de
  1×1×1 en el origen, renderizado con el material default.
- Mover el brush con el gizmo → el cubo se mueve. (El gizmo
  modifica `TransformComponent`, y el brush se reconstruye con
  `worldFromLocal = transform.world()`. Eso requiere flag dirty
  cuando el transform cambia — usaremos un dirty flag manual o
  re-build cada frame en F2H11; el caching agresivo es F2H16).

Commit:
`feat(F2H11 Bloque D): BrushComponent + integracion SceneRenderer`

### E — Persistencia `.moodmap`

- Bumpear schema de `.moodmap` (versión actual + 1):
  ```json
  {
    "schemaVersion": <N+1>,
    "entities": [...],
    "brushes": [
      {
        "transform": { "position": ..., "rotation": ..., "scale": ... },
        "faces": [
          {
            "planeNormal": [x,y,z],
            "planeDistance": d,
            "materialIndex": 0
          },
          ...
        ]
      },
      ...
    ]
  }
  ```
- `SceneSerializer::save`: barrer entities con `BrushComponent`,
  serializar el brush + el `TransformComponent`.
- `SceneLoader::applyOneEntity` (o handler dedicado para brushes):
  reconstruir el brush leyendo planos de los `faces[]`.
- Tests:
  - `test_brush_persistence.cpp` (8-10 cases):
    - Round-trip: 1 box brush → save → load → mismas faces y
      mismas normales/distances.
    - Round-trip de N brushes con transforms distintos.
    - Round-trip preserva `materialIndex` por cara.
    - Tolerancia: aceptar diferencias < `kEpsilon` para float
      round-trip.
- Migración: proyectos viejos sin `brushes[]` → leer como array
  vacío sin errores.

Commit:
`feat(F2H11 Bloque E): persistencia de brushes en .moodmap + tests`

### F — UI mínima (Add Box Brush)

- En `MenuBar`: nuevo submenú **"Mapa > Añadir Box Brush"** o
  botón en el toolbar.
- Acción: crea entity nueva con `TransformComponent`
  (`scale = (1,1,1)` en origen) + `BrushComponent`
  (`makeBoxBrush(identity)`).
- El gizmo de transform existente debería funcionar out-of-the-box
  porque BrushComponent escucha el transform de la entidad.
- Botón en Inspector: "**Recompute AABB**" (debug helper).

Commit:
`feat(F2H11 Bloque F): UI - Anadir Box Brush + gizmo integrado`

### G — Cierre + tag

- `docs/HITOS.md`: F2H11 [x] con resumen 1 línea.
- `docs/ESTADO_ACTUAL.md`: sección 1 reescrita con F2H11 cerrado;
  F2H10 movido a "anterior".
- `docs/DECISIONS.md` entrada con fecha 2026-05-XX:
  - "CSG a mano (no manifold/Carve) — rationale: brush implícito
    con planos por cara es lo que habilita lock-to-world UV en
    F2H14, y manifold opera sobre mesh triangulada perdiendo ese
    feature".
  - "Tolerancia geométrica `kEpsilon = 1e-4f`".
- Tag: `v1.2.0-fase2-hito11`.

Commit final:
`docs(F2H11 Bloque G): cierre - HITOS / ESTADO / DECISIONS`

---

## Suite

Cases nuevos esperados:
- `test_csg_plane.cpp`: ~15-20 cases, ~50-80 asserts.
- `test_csg_brush.cpp`: ~20-25 cases, ~80-120 asserts.
- `test_brush_persistence.cpp`: ~8-10 cases, ~30-50 asserts.

**Suite estimada: 396 → ~440-450 cases / 6948 → ~7100-7200 asserts.**

---

## Riesgos

- **Edge cases del clipping**: planos coincidentes, planos casi
  paralelos, brushes degenerados (< 4 caras, caras coplanares
  duplicadas, normal cero). Mitigación: cada caso entra como test
  específico en `test_csg_brush.cpp` con un assert claro de "este
  brush es inválido, no buildea mesh".
- **Robustez numérica**: `kEpsilon = 1e-4f` puede ser muy chico
  para brushes grandes (>100 unidades) o muy grande para brushes
  chicos (<0.01 unidades). Mitigación: tests con escalas extremas;
  si emerge problema, escalar epsilon por bbox.
- **Performance del rebuild dinámico**: rebuildear la mesh cada
  vez que `dirty=true` puede ser caro si hay miles de brushes
  editándose simultáneamente. F2H11 acepta esto — F2H16 lo
  optimiza con la mesh estática unificada.
- **Persistencia round-trip de floats**: doubles vs floats en JSON.
  Mitigación: serializar como float (precisión limitada pero ok
  para geometría de mapa) y aceptar `kEpsilon` de tolerancia en
  los tests.
- **Desplazamiento del gizmo vs transform**: cuidar que mover el
  brush con el gizmo no rote o escale los planos del brush
  internamente — los planos viven en local space del brush, el
  transform los lleva a world. Si esto se enreda, los UV de F2H14
  van a sufrir. Mitigación: tests de "transform aplicado dos veces
  no deforma el brush".

---

## Criterios de cierre

- [ ] `src/engine/world/csg/` con `Plane.h`, `Brush.h`, `BrushMesh.h`
      + sus `.cpp`. Cero includes de OpenGL/ImGui en este subsystem.
- [ ] `BrushComponent` integrado en `engine/scene/components/`.
- [ ] `SceneRenderer` renderiza brushes con 1 draw call por brush.
- [ ] Persistencia `.moodmap` round-trip verde con tests
      (`test_brush_persistence.cpp`).
- [ ] UI: **Mapa > Añadir Box Brush** crea brush vivo, movible con
      gizmo de transform.
- [ ] Suite ~440+ cases verde.
- [ ] `docs/HITOS.md`, `ESTADO_ACTUAL.md`, `DECISIONS.md` actualizados.
- [ ] Tag `v1.2.0-fase2-hito11` pusheado.

---

## Refs canónicas

- "Real-Time Collision Detection" (Christer Ericson, 2005) — cap 5
  sobre planes y intersection tests.
- TrenchBroom source: https://github.com/TrenchBroom/TrenchBroom —
  carpeta `lib/vm/` para math y `lib/kdl/` para utilidades; carpeta
  `common/src/Model/Brush*.cpp` para el algoritmo de brush mesh.
- Quake / Half-Life map editor (Hammer 4) — referencia histórica
  del workflow.
