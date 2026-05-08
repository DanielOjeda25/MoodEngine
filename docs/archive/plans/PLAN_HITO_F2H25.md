# Plan — F2H25: Cull de overlap parcial en CompileMap

> **Leer primero:** `ESTADO_ACTUAL.md` (F2H24 cerrado), `PENDIENTES.md`
> sección "En curso — orden acordado con el dev" ítem 1, `DECISIONS.md`
> entries de F2H20 (compilación brush → mesh) que documenta el cull
> de pareja exacta ya implementado.

---

## Objetivo

Extender el cull de caras de `CompileMap.cpp` para que también
elimine **overlap parcial** entre brushes solapados, no solo el caso
de pareja exacta que F2H20 ya cubre.

**Contexto:** F2H20 implementó `markInternalFaces` que detecta dos caras
de brushes distintos con polígonos coincidentes ±epsilon y normales
antiparalelas — las elimina. Eso cubre brushes pegados perfectamente
"cara contra cara". Pero si un brush A y un brush B se **solapan
parcialmente** (B mete una esquina dentro de A, dos paredes de columna
rotada cruzan a un cuarto, etc.), las caras de A que quedan
**parcialmente dentro** de B siguen renderizándose dentro del volumen
de B — invisibles pero gastan vertices y pueden producir z-fighting.

**Por qué ahora**: pendiente declarado de F2H20 ("clipping general
polígono-polígono. Diferido si emerge necesidad real"). El siguiente
hito (F2H26 = runtime-load del Player) entrega la mesh compilada al
runtime — vale la pena que esa mesh esté lo más limpia posible antes
de gastarla en el path crítico.

---

## Filosofía de diseño

- **Algoritmo geométrico estándar**: BSP-style clipping. Para cada cara
  F del brush A, iterar sobre cada otro brush B != A y partir F en
  fragmentos: los que caen **fuera** de B se conservan, los que caen
  **dentro** de B se descartan (son interiores). La unión de los
  fragmentos sobrevivientes es lo que reemplaza a F en el output.
- **Reusar la infra existente**: `Plane`, `signedDistance`, `kPlaneEpsilon`,
  el patrón de `markInternalFaces` para detectar caras antiparalelas
  coincidentes (ese path NO se toca — F2H25 corre **después** del cull
  exacto, sobre las caras que sobrevivieron).
- **Polígonos convexos como nuestro modelo**: cada `RawBrushFace.worldPolygonCcw`
  es un polígono convexo (planos por cara → caras convexas por
  construcción, ver `BrushMesh.h`). El clipping contra un plano
  produce 0, 1 o 2 polígonos convexos. Útil porque
  Sutherland-Hodgman + split polygon funcionan tal cual sin tener
  que descomponer.
- **Edge cases conservadores**: si el clipping produce un polígono con
  área ~0 (degenerado tras epsilon de plano), descartar. Si produce un
  fragmento con < 3 vertices únicos, descartar. Mejor perder un
  triangulito esquina que escribir un triángulo mal formado en la
  mesh compilada.
- **Coexistencia con cull de pareja exacta de F2H20**: orden = primero
  `markInternalFaces` (cull exacto, baratísimo), después el clip
  contra brushes restantes solo de las caras que sobrevivieron. La
  cara antiparalela perfecta ni entra al algoritmo nuevo.

---

## Decisiones arquitectónicas

| # | Decisión | Valor |
|---|---|---|
| 1 | Algoritmo | BSP clipping con `splitPolygonByPlane(poly, plane) -> (above, below)`. |
| 2 | "Above" vs "below" | Above = lado positivo del plano = afuera del brush (planos del brush apuntan hacia afuera por convención de F2H11). |
| 3 | Orden con cull exacto | Cull exacto primero (`markInternalFaces`); cull overlap parcial solo sobre caras NO marcadas. |
| 4 | Output | Cada cara que sobrevive el clip puede dividirse en N fragmentos (N≥0). Cada fragmento se triangula igual que una cara normal. |
| 5 | Stats nuevas | `culledOverlapTriangles` (triángulos descartados por overlap) y `splitFragments` (caras partidas por overlap). |
| 6 | Coplanaridad | Si el polígono está coplanar a un plano del brush B (degenerado), tratar como "exterior" (no clippear) — lo cubre el cull exacto si aplica. |
| 7 | Epsilon | Reusar `kPlaneEpsilon` (1e-4f). |
| 8 | Performance | O(F × B × Pavg) donde F=caras sobrevivientes, B=brushes, Pavg=planos promedio por brush. Aceptable para mapas típicos (≤ 100 brushes). Si emerge presión, optimizar con AABB-test brush-vs-brush antes del clip. |
| 9 | Tests | Casos: brush solapado parcial → caras partidas; brushes idénticos → cull total (degenera al exact); brushes disjuntos → cero cull (smoke); brush adentro de otro → todas las caras del interior se cullean. |

---

## Bloques

### A — Plan F2H25 (este archivo)
Definir scope, algoritmo, stats nuevas.

### B — Implementación
Editar `src/engine/world/csg/CompileMap.{h,cpp}`:
- Agregar helper interno `splitPolygonByPlane(const std::vector<glm::vec3>& poly, const Plane& plane, f32 eps) -> std::pair<std::vector<glm::vec3>, std::vector<glm::vec3>>` (above, below).
- Agregar helper interno `cullPolygonAgainstBrush(const std::vector<glm::vec3>& poly, const Brush& brush, const glm::mat4& worldMatrix) -> std::vector<std::vector<glm::vec3>>` que devuelve los fragmentos exteriores.
- En `compileMap`, después del cull exacto, iterar caras sobrevivientes y aplicar el clip contra cada otro brush. Reemplazar `RawBrushFace` por una lista de `RawBrushFace` (uno por fragmento) con UV recomputadas.
- Extender `CompileStats` con `culledOverlapTriangles` y `splitFragments`.

### C — Tests
Crear casos en `tests/test_compile_map.cpp`:
- "Brushes solapados parcial": 2 boxes que se cruzan en una esquina. Verificar que el polígono interior de cada uno está culleado y que aparecen fragmentos partidos.
- "Brush completamente dentro de otro": el chico desaparece en compilación; el grande conserva todas sus caras.
- "Brushes disjuntos": cero cull, output igual al pre-F2H25.
- "Pareja exacta cara-contra-cara" (sanity): el path de cull exacto sigue funcionando, no se duplica el trabajo en el de overlap.
- "Polygon split count": para un caso conocido, contar exactamente N fragmentos esperados.

### D — Build + suite + validación visual
- Build editor + tests; suite verde con cases nuevos.
- Validación con el editor: spawnear 2 brushes solapados, `Mapa > Compilar mapa (stats)`, verificar que `culledOverlapTriangles > 0` y `splitFragments > 0`. Exportar OBJ y abrir en visualizador para confirmar mesh limpia.

### G — Cierre
- `HITOS.md`: entrada F2H25 cerrado, tag `v1.16.0-fase2-hito25`.
- `ESTADO_ACTUAL.md`: sección 1 reescrita; F2H24 desplazado a anterior.
- `DECISIONS.md`: entrada con razones del algoritmo BSP + orden con cull exacto.
- `PENDIENTES.md`: tachar F2H25 de "En curso", F2H26 pasa al tope.
- Archivar `PLAN_HITO_F2H25.md` en `docs/archive/plans/`.
- Commits agrupados (1 feat + 1 docs) + tag + push tras autorización.

---

## Lo que NO entra

- Optimización con AABB-test brush-vs-brush antes del clip — diferido a
  hito de performance si emerge presión.
- Cull de caras coplanares parciales (dos caras coplanarias parcialmente
  superpuestas) — caso muy raro, no scope.
- Persistencia del compilado en `.moodmap` — F2H26.
- F6 panel — F2H27.
- Hammer 4-viewport — F2H28.
