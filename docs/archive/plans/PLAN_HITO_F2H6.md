# Plan — F2H6: LOD system con cache en disco

> **Leer primero:** `ESTADO_ACTUAL.md` (F2H5 cerrado, Release medido),
> `PLAN_FASE2.md` sección 4 (F2H4 LOD original — diferido dos veces,
> retomado aquí), `docs/PERFORMANCE.md` (post-F2H5 + Release).

---

## Objetivo

Implementar un sistema de niveles de detalle (LOD) para que los meshes
complejos pesen menos cuando están lejos de la cámara. Es la última
optimización core de Sub-fase 2.1 y prepara al motor para contenido
real (Fox.glb ~1500 tris, kenney_survival props ~500-2000 tris cada uno,
modelos importados de catálogos AAA con 5-50K tris).

**Retomado de F2H4 original** (diferido dos veces — primero por
instancing F2H4, después por virtualización Hierarchy F2H5). Hoy con el
motor escalando a 60 FPS Release con 1M tris en cubos primitivos, el
LOD entra como cimiento para contenido real, no como optimización de
emergencia.

---

## Filosofía de diseño — cimiento sólido

Las decisiones siguen lo que hacen Unity, Unreal y Godot. Documentadas
con razones (ver `docs/DECISIONS.md` entrada del 2026-05-XX al cierre):

1. **Persistencia con cache en disco** (`assets/.cache/lods/`).
   - Auto-genera la primera vez con `meshoptimizer`. Persiste.
   - Re-genera si el source cambió (mtime o hash mismatch).
   - Borrar el cache = workflow para regenerar tras cambios al source.
   - **NO** schema bump del `.moodmap` — el cache es lateral al formato.
2. **Per-MeshAsset con default global** para los rangos de distancia.
   - Defaults sensatos: LOD 0 hasta 30m, LOD 1 hasta 80m, LOD 2 después.
   - Campos `lodDistances[2]` en `MeshAsset` para override per-mesh.
   - Sin UI editor en v1 (programmatic) — UI = hito futuro.
3. **Skinned meshes saltean LOD generation** en v1.
   - Reducir verts en un mesh skinned implica re-mapear los bone weights
     consistentemente. Es scope grande con risk de bugs visuales.
   - Skinned siempre usa LOD 0.

---

## Bloques

### A — Plan F2H6 (este archivo)

Documento del hito. Cierra al commit inicial.

### B — Dep `meshoptimizer` via CPM

- Agregar `wolfpld/meshoptimizer` al `CMakeLists.txt` raíz vía
  `CPMAddPackage`. Tag pinned (probable `v0.21` o latest stable).
- Linkear contra `mood_engine_lib`.
- No tests propios para la dep — confiamos en el upstream.

### C — `MeshAsset` con campos LOD + helper

`src/engine/render/resources/MeshAsset.h`:

- Renombrar `submeshes` → mantener (es LOD 0).
- Agregar `std::vector<SubMesh> lod1Submeshes` y `lod2Submeshes`
  (vacíos = no LOD generado, fallback a LOD 0).
- Agregar `glm::vec2 lodDistances{30.0f, 80.0f}` (umbrales LOD0→1, LOD1→2).
- Helper inline en el header (puro):
  ```cpp
  inline u32 selectLod(f32 distance, const glm::vec2& thresholds) {
      if (distance < thresholds.x) return 0;
      if (distance < thresholds.y) return 1;
      return 2;
  }
  ```
- Método `const std::vector<SubMesh>& submeshesForLod(u32 lod) const`
  que devuelve la lista correspondiente; fallback a LOD 0 si la lista
  pedida está vacía.

### D — Cache `.moodlod` en disco con hash + mtime

Nuevo `src/engine/assets/cache/LodCache.{h,cpp}`:

- Path del cache: `assets/.cache/lods/<hash>.moodlod` donde `<hash>` es
  FNV-1a 64-bit del path lógico del mesh (ej. `meshes/Fox.glb`).
- Formato binario simple:
  - Header: `magic = "MLOD"`, version `u32`, source mtime `u64`,
    source size `u64`.
  - Para cada LOD (1 y 2): `u32 vertexCount`, datos de vertices
    (pos+color+uv+normal, mismo layout que el SubMesh runtime).
- API:
  - `bool loadFromCache(path, sourceMtime, sourceSize, MeshAsset& out)` —
    valida headers, retorna false si mismatch.
  - `void saveToCache(path, sourceMtime, sourceSize, const MeshAsset&)`.
- Tests: round-trip (crear, guardar, cargar, comparar bytes), invalidación
  por mtime, magic incorrecto.

### E — Auto-gen al `loadMesh`

`src/engine/assets/manager/AssetManager.cpp`:

- Después del load del mesh original (LOD 0), si NO tiene esqueleto:
  1. Computar mtime + size del source.
  2. Intentar `LodCache::loadFromCache(...)` → si OK, llenar lod1/lod2
     submeshes desde el cache.
  3. Si miss: generar LODs con `meshoptimizer::simplifyMesh`:
     - LOD 1 = target 50% de tris, error ratio 0.05.
     - LOD 2 = target 15% de tris, error ratio 0.10.
     - Si el mesh tiene <100 tris, NO generar LODs (el reducción daría
       resultados raros — fallback a LOD 0 siempre).
  4. Escribir cache con `LodCache::saveToCache`.
- Logging: `[lod] meshes/Fox.glb LOD0=1500 LOD1=750 LOD2=225 (cache=hit/miss)`.

### F — Selector LOD en `SceneRenderer` + `groupByBatch`

`src/engine/render/scene_renderer/RenderBatching.{h,cpp}`:

- `BatchKey` se extiende con `u32 lod` (default 0). Dos entidades con
  el mismo (mesh, material) pero LOD distinto van a batches separados.
- `groupByBatch` toma adicionalmente `cameraPos`. Para cada entidad:
  - Computar `distance = length(entity.worldPos - cameraPos)`.
  - `lod = selectLod(distance, asset->lodDistances)`.
  - Si el mesh no tiene LOD para ese nivel (lod1/2 vacíos), fallback a 0.
  - Insertar en `BatchKey{mesh, material, lod}`.

`SceneRenderer::renderScene`:

- Pasar `cameraPos` al `groupByBatch`.
- Al emitir el batch, usar `asset->submeshesForLod(key.lod).front().mesh`
  en lugar del LOD 0 fijo.
- Plot Tracy `PBR::LodDistribution` con counts de cada nivel (debug).

### G — Tests doctest

Nuevo `tests/test_lod.cpp` (~10 cases):

- `selectLod`: distancia 0 → 0; en el threshold lo → 1; pasada lo → 2.
- `selectLod` con ranges custom.
- `submeshesForLod` cuando lod1/2 están vacíos → fallback a 0.
- `LodCache::saveToCache` + `loadFromCache` round-trip de un mesh
  fabricado.
- `LodCache::loadFromCache` con mtime distinto → false (invalidate).
- `LodCache::loadFromCache` con magic incorrecto → false.

`tests/test_render_batching.cpp`:

- BatchKey con lod incluido en igualdad y hash.
- groupByBatch con 2 entidades cerca + 2 lejos del mismo mesh →
  2 batches (1 LOD0 + 1 LOD1 o 2).

### H — UI Inspector (readonly)

`src/editor/panels/scene/InspectorPanel.cpp`:

- Cuando una entidad con `MeshRendererComponent` está seleccionada,
  mostrar bajo "Mesh: ..." un texto:
  ```
  LOD 0: 1500 tris
  LOD 1: 750 tris
  LOD 2: 225 tris
  Distances: 30m / 80m
  ```
- Read-only en v1. Editar LODs custom o cambiar distances per-mesh =
  hito futuro.

### I — Documentación + cierre

- Re-medir performance con un escenario que beneficie del LOD
  (spawnar 50 Foxes en un grid de 100m → comparar tris dibujados con
  y sin LOD).
- Actualizar `docs/PERFORMANCE.md`: tabla "Post-F2H6" con stress de
  meshes complejos (no cubos primitivos).
- Entrada en `docs/DECISIONS.md` (2026-05-XX): "LOD con cache en disco
  + per-mesh ranges — rationale del cimiento sólido vs parche simple".
- `docs/HITOS.md`: marcar F2H6 [x] con resumen.
- `docs/ESTADO_ACTUAL.md`: sección 1 reescrita con F2H6 cerrado, mover
  F2H5 a "anterior". Próximo paso = F2H7 (UI reorg) o lo que decidas.
- `docs/PLAN_FASE2.md`: documentar el shift definitivo (F2H4 instancing,
  F2H5 hierarchy, F2H6 LOD; F2H7+ siguen el orden original).
- Tag: `v1.1.4-fase2-hito6`.

---

## Suite

Aditivo + gateado. El path no-LOD (mesh con <100 tris o skinned)
funciona idéntico al pre-F2H6. Tests nuevos: ~10 en `test_lod.cpp` +
2 en `test_render_batching.cpp` para BatchKey con lod.

Suite esperada al cierre: **357+/6800+** sin regresiones.

## Riesgos

- **`meshoptimizer` puede dar mesh degenerado** con assets exóticos
  (multi-submesh con UV seams). Mitigación: validar el output (count > 0
  y dentro del bounding box original) antes de aceptar como LOD válido.
  Si falla, fallback a LOD 0 con warning log.
- **Cache invalidation incorrecto**: si el dev edita el source pero no
  cambia el mtime (ej. git checkout que mantiene mtimes), el cache
  sigue stale. Mitigación: incluir hash del contenido (FNV-1a sobre
  los primeros 4KB del archivo) en el header del cache para detectar
  cambios silenciosos. Si emerge como problema, full hash.
- **Cache files corruptos**: si el sistema de archivos se corrompe o
  el dev manipula `.moodlod` manualmente, el load puede romper. Mitigación:
  magic + version + size checks. Si fallan, log warning y regenerar.
- **Performance del genertion al primer load**: con 100 meshes complejos
  el primer abrir del proyecto puede tardar 5-30 segundos. Aceptable como
  cost de instalación (mismo patron que Unity/Unreal). Después: instant.
- **Bumpear el sistema de assets**: agregar el directorio `.cache/` en
  `.gitignore`. Documentar en CONTRIBUTING.

---

## Criterios de cierre

- [ ] `meshoptimizer` linkeado y funcional.
- [ ] `MeshAsset` con campos LOD + helper `selectLod` + `submeshesForLod`.
- [ ] `LodCache` con save/load + mtime/hash validation.
- [ ] `AssetManager::loadMesh` auto-genera LODs al primer load + cachea.
- [ ] Skinned meshes saltean (logueado).
- [ ] `groupByBatch` agrupa por (mesh, material, lod).
- [ ] `SceneRenderer::renderScene` emite el LOD correcto.
- [ ] Plot Tracy `PBR::LodDistribution` reportando.
- [ ] Inspector muestra info de LODs read-only.
- [ ] 10+ tests nuevos en `test_lod.cpp`, todos pasando.
- [ ] Suite global sin regresiones (357+/6800+).
- [ ] `docs/PERFORMANCE.md` con escenario de meshes complejos +
      comparativa con/sin LOD.
- [ ] `docs/DECISIONS.md` con entrada del rationale.
- [ ] `docs/HITOS.md` y `ESTADO_ACTUAL.md` actualizados.
- [ ] `.gitignore` incluye `assets/.cache/`.
- [ ] Tag `v1.1.4-fase2-hito6` pusheado.
