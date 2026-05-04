# Plan — F2H3: Frustum culling (pase opaco)

> **Leer primero:** `ESTADO_ACTUAL.md` (F2H2 cerrado), `PLAN_FASE2.md` sección 4 (F2H3),
> `docs/PERFORMANCE.md` (baseline GTX 1660), `DECISIONS.md` (entrada de F2H2 sobre Tracy).

---

## Objetivo

Cortar el coste de `PBR::staticPass` (medido en F2H2 al **70.74% del frame** con 836 cubos)
descartando antes del draw call las entidades cuyo AABB cae **fuera del frustum de la
cámara**. Esto es la primera optimización de Fase 2 — entra con datos del baseline F2H2,
no con intuición.

**Scope explícito v1:**
- Solo el pase **PBR opaco estático** (`PBR::staticPass`). El shadow pass + skinned
  quedan **fuera** de v1 (ROI bajo: shadow pass ni aparece en top 10 zonas Tracy).
- Implementación **plana** (loop linear sobre entidades). Jerárquico (BVH/octree) NO se
  hace ahora — diferido a hito propio cuando una medición confirme que el loop linear es
  cuello (probable solo a >100K entidades visibles, escenario que el motor todavía no
  alcanza por otros cuellos).
- API diseñada para que el upgrade interno a BVH no rompa callers (los callers piden
  "lista visible", no iteran por dentro).

---

## Bloques

### A — Plan F2H3 (este archivo)

Documento del hito. Cierra al commit inicial.

### B — `Frustum` + `AABB` helpers

Nuevo header `src/engine/render/pipeline/Frustum.h` (+ `.cpp` si hace falta) con:

- `struct Plane { glm::vec3 normal; f32 distance; }` (form `dot(n, p) + d`).
- `struct Frustum { Plane planes[6]; }`.
- `Frustum frustumFromViewProj(const glm::mat4& viewProj)` — extracción canónica de
  Gribb-Hartmann (6 planos = filas 3+row de la matriz combinada, normalizados).
- `bool aabbVisible(const AABB& aabbWorld, const Frustum& frustum)` — test de los 8
  vértices (o el truco "p-vertex" para optimizar) contra cada plano. Conservador:
  si TODOS los vértices están del lado negativo de un plano, fuera. En cualquier otro
  caso, dentro o intersect → tratado como visible.

Ya existe `core/math/AABB.h` (usado en `EditorApplication.cpp` line 5). Reusar.

### C — AABB world-space por entidad

Ya existe `MeshAsset::aabbMin` / `aabbMax` (local-space al mesh). Hace falta el AABB
**world-space** rotado/escalado por el `TransformComponent`.

Decisión: **calcular on-the-fly por entidad cada frame, sin caché por ahora.** El
costo (rotar 8 vértices + min/max) es ~30 ns por entidad — para 836 entidades = 25 µs
total. Negligible vs los 42 ms del staticPass. El caché con invalidación por
dirty-flag entra como sub-bloque solo si una medición posterior lo justifica.

Helper en `Frustum.h` (o un `BoundsHelpers.h`):
```cpp
AABB worldAabb(const AABB& localAabb, const glm::mat4& worldMatrix);
```

### D — Cull gate en `SceneRenderer::renderScene`

En `src/engine/render/scene_renderer/SceneRenderer.cpp`, función `renderScene`:

1. Construir `Frustum` una sola vez con `viewProj = projection * view`.
2. En el loop de PBR static, antes del `drawMesh`, llamar `aabbVisible(...)`. Si
   false, `continue;`.
3. Counter Tracy nuevo: `MOOD_PROFILE_PLOT("CulledEntities", n)` para tener visibilidad
   en el profiler de cuántas se descartan.
4. `MOOD_PROFILE_SCOPE("PBR::frustumCull")` envolviendo la fase de testing — para que
   el costo del culling sea medible separado del coste de los draws.

**No tocar** el shadow pass ni el skinned pass. **No tocar** el counter de
`FrameStats::drawCalls` — solo se incrementa en draws que efectivamente se hacen, lo
cual ya está bien (si se descarta antes, no se cuenta).

### E — Tests doctest

Nuevo archivo `tests/test_frustum.cpp`:

- `frustumFromViewProj`: matriz identidad → planos canónicos del NDC cube. Matriz con
  zoom (proj con FOV chico) → planos más juntos. Verificar normales unitarias.
- `aabbVisible`:
  - AABB en el centro del frustum → visible.
  - AABB completamente detrás de la cámara → no visible.
  - AABB completamente al lado del frustum (fuera del near + del side) → no visible.
  - AABB que intersecta un plano (la mitad dentro) → visible (conservador).
  - AABB del tamaño del world (gigante) → visible siempre.
- `worldAabb`:
  - identidad world matrix → world == local.
  - traslación → AABB se mueve igual.
  - rotación 45° en Y → AABB crece (porque el bounding eje-alineado de un cubo rotado
    es más grande). Verificar rangos.
  - escala 2x → AABB escala 2x.

Suite esperada: **319 → 327** test cases (+8 nuevos), assertions suma proporcional.
Sin regresiones en los 319 existentes.

### F — Re-medir baseline F2H3 vs F2H2

Después de B-E listos:

1. Build Debug `MOOD_PROFILE=ON`.
2. Lanzar editor, abrir HUD.
3. Spawn 10K tris (836 cubos) — misma escena que F2H2.
4. Mover la cámara para que ~50% de los cubos queden fuera del viewport (rotar la
   cámara un par de veces).
5. Snapshot CSV con label `10K_post_F2H3`.
6. Capturar Tracy `.tracy` → exportar con `tracy-csvexport.exe`.
7. Comparar con baseline F2H2:
   - `PBR::staticPass` % esperado: **bajar significativamente** si el viewport solo ve
     ~400 cubos (mitad). Target: <50% del frame (vs 70.74% pre).
   - FPS esperado: **subir** notable (de 4 → 8-15 FPS rangoa estimado).
   - `PBR::frustumCull` zona nueva: <1% del frame.

8. Agregar columna **post-F2H3** a la tabla de `docs/PERFORMANCE.md` con los nuevos
   números. Si el FPS no sube significativamente con culling activo, **diagnosticar
   antes de cerrar**.

### G — Documentación + cierre

- Actualizar `docs/PERFORMANCE.md`: columna comparativa post-F2H3.
- Entrada en `docs/DECISIONS.md` (2026-05-XX): "Frustum culling plano vs jerárquico —
  rationale del flat por ahora, API extensible".
- `docs/HITOS.md`: marcar F2H3 [x] en sección Fase 2 con resumen del delta.
- `docs/ESTADO_ACTUAL.md`: sección 1 reescrita con F2H3 cerrado, próximo paso = F2H4
  (LOD). Mover F2H2 a "anterior, ya cerrado".
- Tag: `v1.1.1-fase2-hito3`.

---

## Suite

Todo el código nuevo es **aditivo** + **gateado por la rama "no visible"**. Los tests
existentes que no fuerzan offscreen siguen pasando. La verdadera regresión a vigilar:
escenas pequeñas donde TODO está dentro del frustum — el costo del culling no debe
restar FPS visible vs F2H2 (target: zona `PBR::frustumCull` <1% del frame).

Suite esperada al cierre: **327/6630-ish** (+8 cases, ~17 assertions extras).

## Riesgos

- **Falsos negativos** (AABB descartada que SÍ se ve): el test conservador solo
  descarta cuando los 8 vértices están afuera de un mismo plano. No debería haber
  falsos negativos. Si aparecen, debug visual: dibujar AABBs descartadas con
  `OpenGLDebugRenderer` en rojo y los visibles en verde.
- **Falsos positivos** (AABB que pasa el culling pero no se ve): aceptables; el costo
  es solo el draw call wasted, lo que ya estaba pagando antes. El número irá bajando
  con BVH futuro.
- **`worldAabb` con rotación** crece el AABB (bounding eje-alineado de cubo rotado).
  Aceptable para v1 — el cubo lo vemos un poco antes y un poco después de salir,
  pero no hay falsos negativos.

---

## Criterios de cierre

- [ ] `Frustum` + `aabbVisible` + `worldAabb` implementados con docstrings.
- [ ] Cull gate activo en PBR static pass (Tracy zona `PBR::frustumCull` visible).
- [ ] `MOOD_PROFILE_PLOT("CulledEntities", n)` reportando.
- [ ] 8+ tests nuevos en `test_frustum.cpp`, todos pasando.
- [ ] Suite global sin regresiones (327+/6630+).
- [ ] `docs/PERFORMANCE.md` con columna post-F2H3.
- [ ] `docs/DECISIONS.md` con entrada de la decisión flat-vs-jerárquico.
- [ ] `docs/HITOS.md` y `ESTADO_ACTUAL.md` actualizados.
- [ ] Tag `v1.1.1-fase2-hito3` pusheado.
