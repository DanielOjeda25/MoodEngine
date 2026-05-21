# PLAN HITO F2H75 — Cloth / telas que ondean (cierre Sub-fase 2.4)

> **Estado**: borrador para aprobar con el dev.
> **Origen**: último item de la Sub-fase 2.4 (Física avanzada) del plan original
> (F2H28 — cloth/soft body). Cierra la sub-fase.

## Qué siente el jugador

Banderas, estandartes y cortinas que **cuelgan por gravedad y flamean con el
viento**. La tela está anclada por uno o más bordes (el asta de la bandera, la
barra de la cortina) y el resto se mueve solo. Cuando una **zona de viento**
(los Force Fields de F2H72) la toca, ondea hacia donde sopla. Se ve como tela
real: superficie sólida iluminada, visible de los dos lados.

**Fuera de scope de este hito** (acordado con el dev, candidatos a follow-up):
- La tela NO choca con objetos (ni cajas que caen encima, ni el player que la
  atraviesa). Solo gravedad + viento + anclajes.
- Nada de soft bodies volumétricos (jelly/globos) — eso es otra mecánica.
- Nada de rasgado/corte de tela.
- No se importa tela desde un `.glb`: la tela es una **grilla procedural** NxM
  (elegís ancho, alto, resolución y qué borde se ancla). Importar mallas
  arbitrarias como tela es un pipeline aparte, mucho más grande.

## Decisión técnica de fondo

Jolt v5.2.0 ya trae soft bodies compilados (`SoftBodySharedSettings` +
`SoftBodyCreationSettings` + `SoftBodyMotionProperties`). La física la calcamos
del patrón Ragdoll/Vehicle.

**El lift real es el render**: hoy toda la geometría del engine es estática
(`GL_STATIC_DRAW`) o skinneada por matrices de hueso. La tela necesita un mesh
cuyos **vértices se reescriben cada frame** desde las partículas de Jolt, con
**normales recalculadas** para que la luz funcione. Eso es infra nueva (Bloque E).
Ya hay precedente de buffers dinámicos en el engine (`OpenGLParticleRenderer`,
`OpenGLInstanceBuffer`, `OpenGLDebugRenderer`) — reusamos ese estilo, no
inventamos un backend.

## Bloques

### A — `ClothLayout` (header puro, sin Jolt, testeable headless)
`src/engine/physics/cloth/ClothLayout.{h,cpp}`. Genera la grilla a partir de
parámetros de alto nivel:
- Input: `width`, `height` (metros), `resX`, `resY` (nº de partículas por lado),
  `anchorEdge` (enum: TopEdge / TopCorners / LeftEdge / etc.), `totalMass`.
- Output: posiciones locales de las partículas + lista de constraints:
  **structural** (vecinos directos), **shear** (diagonales) y **bend** (vecinos
  a 2 de distancia) — el set estándar de un mass-spring cloth. Marca qué
  partículas son ancladas (invMass = 0).
- Helpers puros: `vertexCount()`, `triangleIndices()` (para el mesh), validación.
- **Tests headless** (mirror de `test_ragdoll_layout` / `test_vehicle_config`):
  grilla 2×2 / NxM, conteo de constraints, anchors correctos, índices de
  triángulos sin degenerados.

### B — `PhysicsWorld_SoftBody.cpp` (wrapper Jolt)
Nuevo TU en `src/engine/physics/world/` (patrón de los otros `PhysicsWorld_*`).
- `u32 createCloth(const ClothLayout&, const glm::mat4& worldTransform)`:
  arma `SoftBodySharedSettings` (vertices + edges con compliance derivada de
  stiffness + pinned via invMass 0) + `SoftBodyCreationSettings`, lo agrega al
  `physicsSystem`, devuelve handle.
- `void destroyCloth(u32)`.
- `bool readClothVertices(u32, std::vector<glm::vec3>& outWorldPositions)`:
  lee las posiciones actuales de las partículas (world space) para el render.
- `void applyClothForce(u32, const glm::vec3& force, ...)`: punto de entrada
  para el viento (Bloque G).
- Storage en el `Impl`: `unordered_map<u32, JPH::Ref<...>> softBodies` +
  `nextSoftBodyId`. API declarada en `PhysicsWorld.h`.

### C — `ClothComponent` + serialización
- `Components.h`: serializado = `width/height/resX/resY/anchorEdge/totalMass/`
  `stiffness/damping/useGravity`; runtime (no persiste) = `clothId`,
  `std::vector<glm::vec3> vertexCache`, `dirty`.
- `EntitySerializer` + `SceneLoader`: round-trip aditivo (mirror ForceField).
- Gate del `SceneSerializer`: agregar `hasCloth` a la cadena OR (tela standalone
  sin mesh propio — el mesh lo genera el sistema).

### D — `ClothSystem` (`src/systems/physics/`)
- `tick(Scene&, PhysicsWorld&, ...)` calcado de RagdollSystem:
  - **Materialize lazy**: `dirty && clothId==0` → `buildClothLayout` +
    `createCloth`. Setea el mesh dinámico (Bloque E) con el conteo de vértices.
  - **Sync por frame**: `readClothVertices` → escribe al VBO dinámico +
    recalcula normales por triángulo (promediadas por vértice).
  - Cleanup en delete (destroyCloth + reset handle).
- Wire en el play loop del editor (`EditorApplication_Run` / `tickPhysics`),
  después del step, solo en Play. En Editor sin Play: pose de reposo colgando
  (analítica o un par de steps de relax), para que se vea algo sin simular.

### E — Render de mesh dinámico (la parte nueva)
- `DynamicMesh` (o variante de `OpenGLMesh` con `GL_DYNAMIC_DRAW`): VBO que se
  reescribe cada frame con `glBufferSubData` (posición + normal + uv). UVs fijas
  (de la grilla), posición + normal actualizadas desde la sim.
- Path de render para cloth: **doble cara** (las telas se ven de los dos lados →
  `glDisable(GL_CULL_FACE)` para esos draws, o normales invertidas en el back).
  Material lit estándar (reusa el shader PBR/forward existente).
- Recalculo de normales en CPU en el sync (Bloque D) — barato para grillas de
  ~20×20.

### F — Editor
- *Add Component → Physics → Cloth*.
- Inspector: dimensiones, resolución (con un cap sano, ej. 40×40), combo de
  anchor, stiffness/damping/mass, toggle gravedad. i18n en/es.
- Debug-draw opcional bajo F1: puntos en las partículas ancladas (para ver de
  dónde cuelga). Barato, ayuda a autoría.

### G — Integración con Force Fields + demo
- Extender `ForceFieldSystem` para que las zonas de viento afecten también a las
  telas (hoy solo `addForce` a rigid bodies Dynamic). Para soft body: aplicar la
  fuerza del campo a las partículas de adentro vía `applyClothForce`. Esto es lo
  que hace que la bandera **flamee** en vez de solo colgar.
- Demo `assets/maps/cloth_demo.moodmap`: una bandera anclada por el borde
  superior + una `WindZone` direccional pulsante que la hace ondear. (Si el
  viento constante se ve estático, una leve variación temporal vende mejor el
  efecto — se evalúa en validación.)

### H — Cierre
- `docs/hitos/F2H75.md`, one-liner en `HITOS.md`, reemplazar sección 0.1 de
  `ESTADO_ACTUAL.md`, decisiones clave en `DECISIONS.md` (mass-spring + compliance,
  por qué grilla procedural y no mesh import, doble cara, mesh dinámico).
- Archivar este plan en `docs/archive/plans/`. Tag `v1.66.0-fase2-hito75`.
- Marca el **cierre de la Sub-fase 2.4** (Física avanzada).

## Riesgos / dudas a resolver en el camino
1. **Tuning del feel**: stiffness/damping/compliance de Jolt soft body son
   sensibles; va a requerir iteración runtime con el dev (como el damping del
   auto en F2H70.2). Defaults conservadores primero.
2. **Cómo aplica fuerza Jolt a las partículas**: confirmar la API exacta de
   soft body para "viento" (puede ser por-vértice o un campo global). Se valida
   en Bloque B/G; si la API directa no existe, fallback = mover partículas o
   usar el gravity factor + un offset.
3. **Costo del recalc de normales**: si una grilla grande pesa, capear la
   resolución o mover el recalc a un paso más barato.
4. **LOC**: Bloque E puede empujar archivos de render; respetar soft 500 / hard
   800 y partir si hace falta.

## Estimación
~6-9h, repartido: A (1h) · B (1.5h) · C (1h) · D (1.5h) · E (2h, lo nuevo) ·
F (1h) · G (1h) · H (0.5h). El render dinámico (E) es el de mayor incertidumbre.

## Orden propuesto
A → B → C (andamiaje físico + datos, todo testeable) → **E** (sacar temprano el
riesgo del render) → D (conectar sim↔mesh) → F → G → validación runtime → H.
