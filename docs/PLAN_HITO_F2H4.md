# Plan — F2H4: Instancing del pase opaco estatico

> **Leer primero:** `ESTADO_ACTUAL.md` (F2H3 cerrado), `PLAN_FASE2.md` sección 4
> (F2H4 — el que era LOD, swap documentado en `DECISIONS.md` 2026-05-03),
> `docs/PERFORMANCE.md` (baseline F2H2 + post-F2H3).

---

## Objetivo

Atacar el cuello medido en F2H2 — `~50 µs por entidad en draw call setup`,
CPU-bound — agrupando entidades del pase opaco estatico por
`(meshId, materialId)` y dibujandolas con un solo `glDrawArraysInstanced`
en lugar de N llamadas a `glDrawArrays`. Las matrices `model` viajan como
atributo de instancia (locations 5-8 = mat4) en un VBO recyclable.

**Speedup esperado** (basado en cuello F2H2):
- 836 cubos del stress-test 10K — todos comparten mesh + material default.
  Con instancing: **836 draws → 1 draw call**.
- F2H3 se complementa: el frustum cull descarta entidades fuera del
  viewport antes de subirlas al VBO de instancias. F2H3 + F2H4 actuan en
  cadena: cull → group → batch.
- Predicción: `10K_full_view` pasa de 4.4 FPS a 30-60 FPS (probable vsync
  cap). Validar en Bloque F.

**Swap de numeracion vs PLAN_FASE2.md**: el F2H4 original era LOD. F2H4
LOD ataca **vertex/triangle setup cost**, no draw call submission cost
(que es el cuello real medido). Por eso swap: este F2H4 = instancing,
F2H5 = LOD. Documentado con razones en `DECISIONS.md`.

---

## Scope explicito v1

- Solo el pase **PBR opaco estatico** (mismo scope que F2H3). Skinned
  + shadow quedan fuera.
- Agrupacion por `(meshId, materialIdSlot0)`. Si una entidad tiene
  materiales distintos por submesh (vector con > 1 entry y materiales
  variados), **fallback** al draw individual no-instanced del F2H3 — no
  rompe nada existente.
- AABB world-space + frustum cull integrado: entidades fuera del frustum
  no entran al batch (no se suben al VBO).
- Re-upload del VBO de instancias cada frame con `glBufferData`
  (orphan + upload). MVP — el costo es ~50KB para 836 mat4 = micro
  segundos. Optimizar con persistent mapped buffers si una medicion
  posterior lo justifica.
- Shader nuevo `pbr_instanced.{vert,frag}` (vert con atributo `mat4 model`,
  frag idéntico al `pbr.frag` actual via `#include` o copia + comentario).

---

## Bloques

### A — Plan F2H4 (este archivo)

Documento del hito. Cierra al commit inicial.

### B — Helper de agrupacion + tests

Nuevo header `src/engine/render/scene_renderer/RenderBatching.h` (+ .cpp)
con:

- `struct BatchKey { MeshAssetId mesh; MaterialAssetId material; };`
  con operador `==` y hash (para `std::unordered_map`).
- `struct InstanceBatch { std::vector<glm::mat4> models; };`
- `using BatchMap = std::unordered_map<BatchKey, InstanceBatch, BatchKeyHash>;`
- `BatchMap groupByBatch(Scene&, AssetManager&, const Frustum&)` —
  PURO. No depende de GL. Recorre `TransformComponent + MeshRendererComponent`
  excluyendo `SkeletonComponent`. Para cada entidad:
  - Si tiene materiales distintos por submesh → registra como "no
    batcheable" (devuelve por separado, ver siguiente).
  - Si pasa el cull con AABB world-space (helpers de F2H3) → agrega
    `worldMatrix()` al batch correspondiente.
- Adicionalmente: `std::vector<Entity> nonBatchable` para que
  `SceneRenderer` las dibuje por el path no-instanced.

Tests en `tests/test_render_batching.cpp` (+10 cases):
- Grouping basico: 3 entidades con (mesh A, mat A) → 1 batch con 3 mats.
- Grouping mixto: 2x (A,A) + 2x (B,A) → 2 batches.
- Material distinto por slot: entidad con `materials = {A, B}` → no
  entra al batch (devuelve a `nonBatchable`).
- Cull integrado: entidad con AABB fuera del frustum NO entra al batch.
- Skinned exclusion: entidad con SkeletonComponent NO entra (la maneja
  el skinned pass aparte).
- Mesh asset null: entidad con mesh invalido cae a `nonBatchable`.

Suite esperada: 331 → 341+ test cases.

### C — `drawMeshInstanced` en IRenderer + OpenGLRenderer + InstanceBuffer

- `IRenderer::drawMeshInstanced(const IMesh& mesh, const IShader& shader, const f32* instanceData, u32 instanceCount, u32 strideBytes) = 0`.
  - `instanceData` apunta a las matrices model contiguas (4x4 floats por
    instancia).
  - `strideBytes` = `sizeof(glm::mat4)`.
- Nuevo `engine/render/backend/opengl/OpenGLInstanceBuffer.{h,cpp}`:
  - Posee un VBO. Metodo `upload(const f32* data, u32 sizeBytes)` con
    `glBufferData(... GL_DYNAMIC_DRAW)` orphan-then-fill (estandar).
  - Metodo `bindAsAttributeMat4(u32 location, u32 strideBytes)` que
    setea atributos en locations `[location, location+3]` con divisor=1.
- `OpenGLRenderer::drawMeshInstanced` implementa: bindea shader+mesh,
  bindea el InstanceBuffer en locations 5-8, llama
  `glDrawArraysInstanced(GL_TRIANGLES, 0, vc, instanceCount)`.
  Incrementa `m_stats.drawCalls += 1` y `m_stats.triangles += instanceCount * (vc/3)`.

### D — Shader `pbr_instanced.{vert,frag}`

- Copy de `pbr.vert` con cambio: `uniform mat4 uModel` → `layout(location=5) in mat4 aModel`.
  Al multiplicar `aModel * vec4(aPos, 1.0)` reemplaza la formula actual.
- Frag: si es identico a `pbr.frag` actual (lee mismos uniforms y varyings),
  reusarlo via inclusion en el CMake o duplicarlo con comentario "DEBE
  estar sincronizado con `pbr.frag` — single source of truth en futuro
  si emerge cuello".
- `SceneRenderer::init` carga el nuevo shader como `m_pbrInstancedShader`.

### E — Cablear `SceneRenderer::renderScene`

En el bloque actual del PBR static pass:

1. Antes del loop, llamar `groupByBatch(scene, assets, frustum)` →
   `(BatchMap batches, vector<Entity> nonBatchable)`.
2. Si `batches` no vacio:
   - Bind `m_pbrInstancedShader`, aplicar uniforms (mismos que el path
     actual: textures, lights, fog, IBL).
   - Para cada `(key, batch)`: bind mesh + materiales del key, upload
     instances al VBO, llamar `drawMeshInstanced`.
3. Para `nonBatchable`: usar el path actual (bind `m_pbrShader`, loop
   con `setMat4 uModel` + `drawMesh`). Es el fallback del F2H3.
4. Plot Tracy `PBR::BatchedDrawCalls` = numero de batches emitidos
   (no de instancias). Plot `PBR::Instances` = numero total de instancias.
5. El counter `PBR::CulledStatic` del F2H3 se mantiene (cull se hace
   ahora dentro de `groupByBatch`).

### F — Re-medir baseline post-F2H4

1. Build Debug `MOOD_PROFILE=ON`.
2. Borrar CSV viejo, lanzar editor.
3. Spawn 10K. Para cada escenario, snapshot CSV con label apropiado:
   - `Empty` (control).
   - `10K_full_view` — esperado: drastico drop de draw calls (de 743 a ~5).
     FPS esperado: 60 (vsync) o cerca. Speedup vs F2H2: x10+.
   - `10K_no_view` — esperado: identico al F2H3 (60 FPS / 1 draw / 12 tris)
     porque el cull saca todo antes de batchear. **No regresion.**
4. Capturar Tracy `.tracy` durante los snapshots, exportar con csvexport.
5. Comparar con baseline F2H2 + F2H3:
   - `PBR::staticPass` % esperado: <5% (el draw es una sola call).
   - `PBR::BatchedDrawCalls` plot: 1-3 (por mesh+material distintos).
   - `PBR::Instances` plot: ~836 - culled.
6. Actualizar `docs/PERFORMANCE.md` con columna "post-F2H4".

### G — Documentacion + cierre

- Actualizar `docs/PERFORMANCE.md`: columna comparativa post-F2H4.
- Entrada en `docs/DECISIONS.md` (2026-05-XX): "Instancing como F2H4
  con swap vs LOD original — rationale". Y otra entrada: "Re-upload
  cada frame vs persistent mapped — MVP simple".
- `docs/HITOS.md`: marcar F2H4 [x] (con nota del swap).
- `docs/ESTADO_ACTUAL.md`: seccion 1 reescrita con F2H4 cerrado, proximo
  paso = F2H5 LOD. Mover F2H3 a "anterior, ya cerrado".
- `docs/PLAN_FASE2.md`: documentar el swap F2H4 ↔ F2H5 en seccion 4.
- Tag: `v1.1.2-fase2-hito4`.

---

## Suite

Aditivo + gateado. Test path no-instanced del F2H3 sigue funcional para
fallback. Tests nuevos en `test_render_batching.cpp` (+10 cases).

Suite esperada al cierre: **341+/6700+** sin regresiones.

## Riesgos

- **Bug en uniforms compartidos**: si el `pbr_instanced.frag` queda
  desincronizado del `pbr.frag` (uniforms reordenados, varyings nuevos)
  los meshes salen mal pintados. Mitigacion: visual diff con/sin
  instancing en escena identica.
- **Atributo de instancia mal alineado**: si las locations 5-8 colisionan
  con algo que ya usa el `pbr.vert`, el VAO no rinde. Verificar el .vert
  actual antes de Bloque D.
- **Materiales con texturas distintas dentro del mismo batch**: NO debe
  pasar — el `BatchKey` incluye `materialId`. Si el material tiene
  texture distinta por slot, el `materialId` ya es distinto. Test
  defensivo en Bloque B.
- **Performance del VBO upload**: 50KB cada frame es trivial pero un
  benchmark con 100K instancias daria 6.4MB upload por frame. Aceptable
  como MVP — F2H? optimizara cuando emerja.

---

## Criterios de cierre

- [ ] `RenderBatching.h` con helper puro + 10 tests pasando.
- [ ] `IRenderer::drawMeshInstanced` + `OpenGLInstanceBuffer` implementados
      con docstrings claras.
- [ ] Shader `pbr_instanced.vert/frag` cargados por `SceneRenderer::init`.
- [ ] `SceneRenderer::renderScene` emite batches en lugar de draws
      individuales para entidades batcheables; non-batchable cae al
      path F2H3.
- [ ] Plots Tracy `PBR::BatchedDrawCalls` + `PBR::Instances` reportando.
- [ ] `docs/PERFORMANCE.md` con columna post-F2H4 + speedup medido.
- [ ] `docs/DECISIONS.md` con entrada del swap F2H4↔F2H5 + decision MVP
      del re-upload.
- [ ] `docs/HITOS.md` y `ESTADO_ACTUAL.md` actualizados.
- [ ] Tag `v1.1.2-fase2-hito4` pusheado.
