# Plan — F2H20: Compilación brush → mesh estática unificada + export OBJ

> **Leer primero:** `ESTADO_ACTUAL.md` (F2H19 cerrado), `PLAN_FASE2.md`
> entrada **F2H14** ("Compilación brush → mesh optimizada"; el rerouting
> trasladó el feature a F2H20). Cumple el item explícito del plan original.
> Patrones reusados: F2H17 multi-material por slots, F2H11 build de mesh
> brush-implícito, F2H16 captura por tag.

---

## Objetivo

Tomar todos los brushes del mapa actual, compilarlos a **una mesh estática
unificada** con:
- **Weld global** de vértices coplanares (epsilon `1e-3` en world space).
- **Cull de caras internas** entre brushes adyacentes (caso simple: pareja
  exacta de polígonos antiparalelos coincidentes).
- **Merge por material slot** (un submesh global por path lógico de material,
  no por brush).

Exponer la compilación al dev de dos formas:
1. **Stats dialog**: menú `Mapa > Compilar mapa` muestra ventana con
   `brushes / faces sin cull / faces tras cull / vertices únicos / triángulos`.
2. **Export OBJ**: menú `Mapa > Exportar OBJ...` corre la compilación y
   escribe `.obj` (+ `.mtl` con materiales por path) al destino elegido.

---

## Filosofía de diseño

- **Scope MVP, no two-paths**: NO modificar `SceneLoader` ni el runtime del
  `MoodPlayer` para que cargue la mesh compilada en lugar de los brushes.
  El plan original sugería "esta mesh es lo que se renderiza en runtime,
  brushes solo en el editor" — diferido. F2H20 entrega la **compilación
  como vista derivada** + export, suficiente para que el dev itere y
  exporte. El runtime-load lo abre un hito futuro si emerge necesidad.
- **Helpers puros**: la compilación vive en `engine/world/csg/CompileMap.{h,cpp}`
  como funciones libres. No depende de Scene/AssetManager más allá de los
  inputs explícitos. Testeable sin GL.
- **Cull simple, no clipping general**: dos caras internas se detectan por
  **coincidencia exacta de polígonos** (mismos vértices ±epsilon) +
  normales antiparalelas. Cubre el caso típico (cubos pegados). Overlap
  parcial / clipping general → diferido. Documentar la limitación.
- **Sin schema bump**: nada se persiste en `.moodmap`. La compilación es
  on-demand desde el menú; corre rápido (~1ms para mapas típicos).
- **OBJ + MTL**: formato estándar interoperable (Blender, MeshLab). El
  `.mtl` lateral lista los materiales por nombre = path lógico (`textures/grid.png`).
- **Captura por path**, no MaterialAssetId: el compilador resuelve el id →
  path lógico via `AssetManager::pathOfMaterial()` para que el OBJ sea
  reproducible entre sesiones (mismo input → mismo output).

---

## Decisiones arquitectónicas

| # | Decisión | Valor |
|---|---|---|
| 1 | Persistencia de la mesh compilada | **No** (sin schema bump). On-demand. |
| 2 | Runtime-load de la compilada | **Diferido** (hito futuro). |
| 3 | Algoritmo de cull caras internas | Pareja exacta (polígonos coincidentes ±eps + normales antiparalelas). Overlap parcial = diferido. |
| 4 | Weld | Global, spatial-hash con grid epsilon `1e-3` en world space. |
| 5 | Agrupación de submeshes | Por **path lógico** de material (no MaterialAssetId), para reproducibilidad. |
| 6 | Layout interleaved del compiled | Igual que `brushSubmeshToInterleaved`: pos.xyz / color.rgb / uv.xy / normal.xyz = 11 floats. Para que el `.obj` salga consistente y un futuro runtime-load encaje sin transcoding. |
| 7 | Export OBJ | `.obj` + `.mtl` lateral. Vértices con `v` / `vn` / `vt`. Caras `f a/ta/na`. Submeshes con `usemtl <materialPath>`. |
| 8 | UI | Dos menú-items en `Mapa`: "Compilar mapa" (dialog stats) + "Exportar OBJ..." (file dialog destino + escritura). |
| 9 | Validación de brush degenerado | El compilador omite brushes con `< 4 caras` o `buildBrushMesh` que devuelva mesh vacía. Stats reportan los omitidos. |
| 10 | Tests | Round-trip de la compilación con casos sintéticos: 1 brush, 2 separados, 2 pegados (cara culleada), brush degenerado, 2 con materiales distintos. |

---

## Bloques

### A — Plan F2H20 (este archivo)

Documento del hito. Cierra al commit inicial.

### B — Compilador puro

`engine/world/csg/CompileMap.{h,cpp}`. API:

```cpp
struct CompiledVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

struct CompiledSubmesh {
    std::string materialPath;       // path lógico ("textures/grid.png" o "")
    std::vector<CompiledVertex> vertices;
    std::vector<u32> indices;       // triangle list, post-weld
};

struct CompileStats {
    u32 sourceBrushes = 0;
    u32 brushesSkipped = 0;          // degenerados
    u32 sourceFaces = 0;             // antes de cull
    u32 culledInternalFaces = 0;
    u32 totalTriangles = 0;
    u32 totalVerticesUnique = 0;     // post-weld
    u32 totalVerticesPreWeld = 0;
    u32 submeshCount = 0;            // = paths distintos en uso
};

struct CompiledMap {
    std::vector<CompiledSubmesh> submeshes;
    CompileStats stats;
};

/// @brief Recolecta los datos crudos de cada brush en mundo (sin weld ni cull).
///        Helper público para el compilador y para tests.
struct RawBrushFace {
    u32 brushIndex = 0;
    u32 faceIndex = 0;
    std::string materialPath;
    std::vector<glm::vec3> worldPolygonCcw;
    glm::vec3 worldNormal{0.0f};
    glm::vec3 uAxis{1.0f, 0.0f, 0.0f};
    glm::vec3 vAxis{0.0f, 1.0f, 0.0f};
    glm::vec2 uvOffset{0.0f};
    glm::vec2 uvScale{1.0f};
    f32 uvRotation = 0.0f;
    bool lockToWorld = false;
};

struct BrushSource {
    Csg::Brush brush;
    glm::mat4 worldMatrix{1.0f};
    std::vector<std::string> materialPaths;  // por slot
};

std::vector<RawBrushFace> collectFaces(const std::vector<BrushSource>& sources);

/// @brief Marca caras internas (pareja exacta, normales antiparalelas).
///        Devuelve un vector<bool> paralelo al input. `culled[i] == true`
///        => quitar esa cara del output.
std::vector<bool> markInternalFaces(const std::vector<RawBrushFace>& faces,
                                      f32 epsilon = 1e-3f);

/// @brief Compila el set completo. Función entry point.
CompiledMap compileMap(const std::vector<BrushSource>& sources,
                        f32 weldEpsilon = 1e-3f);
```

**Algoritmo internal-face cull**:
1. Para cada cara `i`, buscar `j > i` (otro brush) con:
   - `dot(normal_i, normal_j) < -0.9999` (antiparalelas).
   - `polygons_match(poly_i, poly_j, eps)` — mismo set de vértices ±eps,
     ignorando orden (CCW vs CW por la antiparalelidad esperada).
2. Si match: marcar ambas como culled.

**Weld**:
- Spatial hash: clave `(floor(x/eps), floor(y/eps), floor(z/eps))`.
- Para cada vértice nuevo, buscar en celda + 26 vecinas la más cercana
  con `distance < eps` y mismo material slot. Si existe, reusar índice.

### C — Wireup en el editor

`engine/world/csg/CompileMap.h` exporta también un helper de alto nivel:

```cpp
/// @brief Helper de conveniencia: itera el scene buscando entidades con
///        BrushComponent + TransformComponent y arma el vector de
///        BrushSource. Resuelve material slot → path lógico via
///        `AssetManager::pathOfMaterial()`. Usado por el editor desde
///        el menú "Mapa > Compilar mapa".
std::vector<BrushSource> collectBrushSourcesFromScene(
    Scene& scene, AssetManager& assets);
```

Editor wiring:
- `EditorUI`: nuevos `requestCompileMap()` / `consumeCompileMapRequest()` +
  `requestExportObj()` / `consumeExportObjRequest()` (con path destino).
- `MenuBar.cpp` (sub-menu `Mapa`): items "Compilar mapa" + "Exportar OBJ..."
  habilitados con `ui.hasProject()`.
- `EditorApplication`: dos handlers nuevos `handleCompileMap()` +
  `handleExportObj()`. El primero corre `compileMap` y muestra ImGui modal
  con stats (entidad propia en el render pass o popup oneshot). El segundo
  abre `pfd::save_file` para destino, llama a `compileMap` + `writeObj`.

### D — Export OBJ

`engine/world/csg/MapExportObj.{h,cpp}`. API:

```cpp
struct ObjExportResult {
    bool success = false;
    std::string errorMessage;     // si !success
    std::filesystem::path objPath;
    std::filesystem::path mtlPath;
};

/// @brief Escribe `compiled` como `.obj` + `.mtl` lateral (mismo basename,
///        extensión cambiada). Crea los directorios padre si no existen.
ObjExportResult writeObj(const CompiledMap& compiled,
                          const std::filesystem::path& objPath);
```

Formato OBJ:
```
mtllib <basename>.mtl
o map_compiled
v 1.0 2.0 3.0
v ...
vn 0.0 1.0 0.0
vn ...
vt 0.0 0.0
vt ...
usemtl textures/grid.png
f 1/1/1 2/2/2 3/3/3
f ...
usemtl textures/brick.png
f ...
```

`.mtl`:
```
newmtl textures/grid.png
map_Kd textures/grid.png
newmtl textures/brick.png
map_Kd textures/brick.png
```

Material vacío (path == "") → `newmtl _default` con un `Kd 0.8 0.8 0.8`.

### E — Tests

`tests/test_compile_map.cpp` (y `test_map_export_obj.cpp`):
- `compileMap` con 0 brushes → submeshes vacío, stats coherentes.
- 1 box brush en origen → 1 submesh, 12 tris, 8 vértices únicos post-weld.
- 2 boxes separados (distintos materiales) → 2 submeshes, 24 tris, 16 verts.
- 2 boxes pegados (cara compartida) → 1 submesh (mismo material), **2 caras
  cullled** (la pareja interna), 20 tris (24 - 2 caras × 2 tris/cara).
- Brush con < 4 caras → omitido, `stats.brushesSkipped == 1`.
- `writeObj` con compiled trivial → contenido del `.obj` y `.mtl` matchea
  golden file (assert sobre subset: `mtllib`, `usemtl`, count de líneas
  `v`/`vn`/`vt`/`f`).
- `markInternalFaces` con dos polígonos coincidentes pero normales paralelas
  (no antiparalelas) → no marca como interna.
- Weld: dos brushes con vértices a `0.5e-3` de distancia → mergeados
  (epsilon 1e-3); a `2e-3` → no mergeados.

### F — Validación visual + suite

- Build editor + tests.
- Suite verde, esperado `582 + ~12 cases / 8219 + ~30 asserts`.
- Validación visual:
  - Spawn 2 boxes pegados (`Brush > Añadir > Box` × 2 + mover uno hasta tocar el otro).
  - `Mapa > Compilar mapa` → dialog muestra `2 brushes, 12 faces, 2 cara(s) interna(s) culleada(s), 8 vertices únicos, 20 triángulos`.
  - `Mapa > Exportar OBJ...` → elegir destino → archivo creado, abrirlo en
    Blender o MeshLab y confirmar que se ve el bloque sin caras internas.

### G — Cierre

- `docs/HITOS.md`: nueva sección F2H20 closed.
- `docs/ESTADO_ACTUAL.md`: F2H20 al top.
- `docs/DECISIONS.md`: entrada `2026-05-07` "F2H20: compilación on-demand,
  no persistir, runtime-load diferido".
- Tag: `v1.11.0-fase2-hito20`.

---

## Suite

Todo el código nuevo es **aditivo**. No toca `SceneLoader` / `SceneSerializer` /
`SceneRenderer`. Solo agrega:
- `CompileMap.{h,cpp}` (~250 LOC).
- `MapExportObj.{h,cpp}` (~120 LOC).
- 2 handlers en `EditorApplication` (~60 LOC) + 2 menu items.
- 2 archivos de test (~250 LOC).

Suite esperada: **597/8249** (sin regresiones, ~15 cases nuevos).

## Riesgos

- **Cull no detecta overlap parcial**: documentado como limitación. El dev
  puede ver "no se cullaron tantas caras como esperaba" y eso es esperado.
  Mejora futura cuando emerja necesidad real.
- **Lock-to-world UVs en compiled**: las UVs se proyectan con `worldMatrix`
  cuando `lockToWorld=true`, igual que `buildBrushMesh`. Asegurar que el
  compilador respeta este flag para que el OBJ exportado sea visualmente
  igual al editor.
- **Materiales sin path lógico**: `pathOfMaterial(id)` puede devolver
  string vacío si el material es un instance creado en runtime
  (`createMaterialFromTexture`). Tratamiento: agrupar todos los empty
  bajo `_default` en el `.mtl`.

## Criterios de cierre

- [ ] `compileMap` produce mesh esperada para casos sintéticos (tests).
- [ ] `markInternalFaces` cubre el caso pareja-exacta + normales antiparalelas.
- [ ] Weld global con epsilon `1e-3` reduce vértices en escenas con brushes pegados.
- [ ] Menú `Mapa > Compilar mapa` muestra dialog con stats reales.
- [ ] Menú `Mapa > Exportar OBJ...` produce `.obj` + `.mtl` válidos
      (verificable abriendo en Blender o MeshLab).
- [ ] Suite verde sin regresiones.
- [ ] Tag `v1.11.0-fase2-hito20` pusheado.
