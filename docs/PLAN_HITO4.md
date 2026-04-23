# Plan — Hito 4: Mundo grid + colisiones AABB

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, sección 10 y 11 de `MOODENGINE_CONTEXTO_TECNICO.md`.
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Objetivo

Construir el primer "mundo" jugable: una grilla 2D de tiles renderizada como cubos 3D (estilo Wolfenstein 3D). El jugador (FpsCamera) puede moverse con WASD pero **no atravesar paredes** gracias a colisiones AABB propias entre el cuerpo del jugador y cada tile sólido.

Entregable: al entrar en Play Mode, apareces en una sala con paredes y podés caminar chocando con ellas. Un debug draw muestra las AABBs cuando se activa.

---

## Criterios de aceptación

### Automáticos

- [ ] `cmake --build build/debug --config Debug` compila sin errores ni warnings nuevos.
- [ ] Tests de doctest: `test_aabb.cpp` con casos de intersección AABB vs AABB y punto-vs-AABB. Todos pasan.
- [ ] Log del canal `world`: `Mapa cargado: <nombre> (<N> tiles solidos)`.
- [ ] Cierre limpio, exit 0.

### Visuales

- [ ] Al arrancar, el Viewport muestra un mapa con paredes (grid 8x8 o similar con bordes sólidos y un cubo central).
- [ ] En Play Mode, WASD mueve al jugador y no puede atravesar paredes (el movimiento se clampea).
- [ ] Con la tecla `F1` (o un checkbox en el Inspector placeholder), se togglea debug rendering de AABBs y se ven las cajas de colisión como líneas.
- [ ] El cubo texturizado del Hito 3 puede quedar como objeto de escena (ej. flotando en el centro del mapa) o reemplazarse por los cubos de tile — decidir durante implementación.

---

## Tareas

### Bloque 1 — Datos del mapa grid

- [x] `src/engine/world/GridMap.h`: estructura con `u32 width`, `u32 height`, `f32 tileSize`, `std::vector<u8> tiles` (enum TileType: Empty=0, SolidWall=1, extendible).
- [x] `src/engine/world/GridMap.cpp`: helpers `isSolid(x,y)`, `tileAt(x,y)`, AABB en mundo para un tile `aabbOfTile(x,y)`.
- [x] Mapa de prueba construido en código (en `EditorApplication` o helper): 8x8 con paredes en el borde y una columna en el centro.
- [x] Logger nuevo `world` en `src/core/Log.cpp` (`engine`, `editor`, `render`, `world`).

> Nota: `aabbOfTile` requiere el tipo `AABB`. Se creó `src/core/math/AABB.h` header-only (parte del Bloque 3) durante este bloque para no dejar el helper vacío. Los tests formales de AABB siguen en Bloque 3/6.

### Bloque 2 — Render del grid como cubos

- [x] `src/systems/GridRenderer.{h,cpp}` (o integrado en EditorApplication por ahora): itera tiles sólidos y dibuja un cubo por cada uno usando el `OpenGLMesh` del cubo del Hito 3.
- [x] Model matrix por tile = `translate(world_pos) * scale(tileSize)`.
- [x] Texturas: por ahora, la misma `grid.png` para todas las paredes. Hito 5 introducirá texturas por tile.
- [x] Optimización básica: cull trivial de tiles vacíos (no dibujar Empty). Frustum culling NO entra en este hito.

> Notas Bloque 2:
> - Integrado inline en `EditorApplication::renderSceneToViewport`; no se creó aún `src/systems/GridRenderer.{h,cpp}`. Extraer cuando aparezca una segunda fuente de geometría (Hito 5 con texturas por tile, o Hito 10 con meshes de archivo).
> - Render-bug encontrado en el camino: `OpenGLRenderer::drawMesh` hacía `shader.unbind()` al terminar, por lo que `glUniform*` llamado entre draws (mismo shader) operaba con program 0 bound y fallaba silenciosamente. Resuelto removiendo el unbind (commit separado `fix(render): drawMesh no desbindea...`).
> - Cámaras por defecto reposicionadas: `EditorCamera` con `radius=12` (mapa 8×8 en cuadro) y `FpsCamera` dentro del mapa en tile interior `(-1.5, 0.6, 2.5)`.

### Bloque 3 — Matemática AABB

- [x] `src/core/math/AABB.h`: struct `AABB { glm::vec3 min; glm::vec3 max; }` + funciones libres `intersects(a, b)`, `contains(a, point)`, `expanded(a, margin)`, `merge(a, b)`. Adelantado en Bloque 1.
- [x] `tests/test_aabb.cpp`: casos de borde (contacto, inclusión, separación, dimensiones degeneradas). 7 casos + subcases, 48 asserciones.

### Bloque 4 — Sistema de colisión jugador vs grid

- [x] Representar al "jugador" en Play Mode con una AABB pequeña (0.4 x 0.9 x 0.4) centrada en la posición de `FpsCamera`. Subido desde 0.3 para dar margen al near clipping plane (ver decisiones).
- [x] `src/systems/PhysicsSystem.{h,cpp}`: función `moveAndSlide(const GridMap&, const glm::vec3& mapWorldOrigin, AABB& box, const glm::vec3& desiredDelta) -> glm::vec3`.
  - Resolver el movimiento eje por eje (X primero, Z después, Y por separado) para permitir sliding contra paredes.
  - En cada eje: detectar qué tiles tocaría la AABB tras el delta; si alguno es sólido, clampear al borde de ese tile.
- [x] En `EditorApplication::updateCameras` del modo Play: capturar el delta deseado de `FpsCamera::computeMoveDelta`, pasarlo por `PhysicsSystem::moveAndSlide`, y aplicar el delta real vía `FpsCamera::translate`.

### Bloque 5 — Debug rendering de AABBs

- [x] ~~`src/engine/render/IDebugRenderer.h` (o ampliar `IRenderer`)~~: por ahora clase concreta `OpenGLDebugRenderer` directa (sin interfaz), con `drawLine(a, b, color)` y `drawAabb(box, color)`. Acumulan en un `std::vector<Vertex>` CPU y flushean con `GL_LINES` una vez por frame. Extraer a `IDebugRenderer` cuando aparezca un segundo backend.
- [x] `OpenGLDebugRenderer`: VBO dinámico con growth policy (2x al rebasar), shader `shaders/debug_line.{vert,frag}` (vec3 pos + vec3 color + uView/uProjection, color plano).
- [x] Toggle con tecla F1 (campo `m_debugDraw` en EditorApplication). En Play Mode se dibuja también el AABB verde del jugador además del amarillo de los tiles sólidos.

### Bloque 6 — Tests

- [x] `test_aabb.cpp`: intersección, contención, merge. Al menos 6 casos. Ver Bloque 3.
- [x] `test_grid.cpp`: construcción, setTile válido/fuera-de-rango, `isSolid` dentro y fuera del mapa (decisión: fuera = sólido), `aabbOfTile` en map-local. 5 casos.
- [x] `test_collision.cpp`: delta cero, área libre, pared en ±X, esquina con slide, Y sin colisión, respeto a `mapWorldOrigin`, AABB degenerada. 8 casos.

Total suite tras Bloque 6: 30 casos, 159 asserciones.

### Bloque 7 — Cierre

- [ ] Recompilar y verificar criterios.
- [ ] Actualizar `docs/HITOS.md`, `docs/DECISIONS.md`, `docs/ESTADO_ACTUAL.md`.
- [ ] Commits atómicos en español.
- [ ] Tag `v0.4.0-hito4` + push.
- [ ] Crear `docs/PLAN_HITO5.md`.

---

## Qué NO hacer en el Hito 4

- **No** integrar Jolt Physics todavía (Hito 12). Las colisiones AABB son propias y temporales.
- **No** implementar broadphase (BVH, spatial hashing). La grilla ya es el broadphase natural.
- **No** agregar dinámica (gravedad, rebotes). Solo "no atravesar paredes".
- **No** meter NPCs o AI: eso es parte del gameplay, Hito 8+.
- **No** cargar mapas desde archivo (eso es parte de serialización, Hito 6).
- **No** cambiar el pipeline de render (deferred, PBR). Seguimos con el shader default + textura.

---

## Decisiones durante implementación

- **`GridMap` sin estado de "world origin"**: El mapa queda en map-local coords (tile (0,0) en XZ=(0,0)). El offset para centrar el mapa en el origen del mundo lo computa `EditorApplication::mapWorldOrigin()` y se pasa explícito al renderer y a `moveAndSlide`. Evita tocar la API de `GridMap` por una sola consumidora; si aparece una segunda ventana o viewport mostrando el mismo mapa transformado distinto, mover el origin a `GridMap`.
- **`FpsCamera` separa cómputo y aplicación del delta**: `computeMoveDelta(dir, dt)` + `translate(delta)` reemplazan internamente a `move()`. Permite pasar el delta por el sistema de colisiones antes de aplicarlo. `move()` queda como conveniencia (usado por los tests).
- **AABB del jugador 0.4 × 0.9 × 0.4** en vez del 0.3 × 0.9 × 0.3 sugerido por el plan: con `half-extent=0.15` la distancia mínima del ojo al muro era 0.15, apenas 0.05 por encima del near clipping plane (0.1). Con pitch > 0 el frustum se metía visualmente en la pared. `half-extent=0.2` da el doble del near plane de margen sin cambiar el proyector.
- **Y no colisiona contra el grid en este hito**: los muros se consideran infinitos en Y. `moveAndSlide` aplica `desired.y` directo. Coincide con "No dinámica (gravedad, rebotes)" del plan. Si llegan techos/pozos, reevaluar.

---

## Pendientes que quedan para Hito 5 o posterior

_(llenar al cerrar el hito)_
