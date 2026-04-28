# Plan — Hito 22: TBD (candidatos por priorizar)

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 21 cerrado).
>
> **Estado:** **TBD**. El Hito 21 cerró en `v0.21.0-hito21` con MoodPlayer + packager funcionando. Antes de empezar el Hito 22, conversar con el dev qué línea de trabajo prioriza. Este documento esboza candidatos; cuando se decida uno, actualizar el plan con el desglose de bloques concretos.

---

## Candidato A: Workflow de scripting

**Por qué importa ahora:** el motor ya tiene Lua corriendo (Hito 8) + bindings (`self.transform`, `hud`, `log`) + hot-reload por mtime. Lo que falta es UX en el editor: hoy hay que alt-tab a VS Code para escribir un script, dropearlo a una entidad pegando el path a mano, y ver errores en el panel Inspector con texto plano.

**Bloques tentativos:**
- **A1 — Tab "Scripts" en el Asset Browser**: lista los `.lua` de `assets/scripts/` con metadata (líneas, last modified). Drag de un script a una entidad del Hierarchy → asigna ScriptComponent automáticamente. Pequeño, ~1 día.
- **A2 — "Nuevo Script" desde menú**: abre file dialog, crea `assets/scripts/foo.lua` con un template (`function onUpdate(self, dt) end`). Pequeño.
- **A3 — Mini editor de código in-place**: nuevo panel `Script Editor` con `ImGuiColorTextEdit` (lib externa que se integra fácil). Syntax highlighting Lua básico, save con Ctrl+S, error highlighting en la línea del último `lastError`. Mediano.
- **A4 — Doc panel**: panel "Lua API" navegable que lista los bindings disponibles (self.\*, hud.\*, log.\*) con ejemplos de uso. Pequeño una vez que A3 está.

**Riesgos:** ImGuiColorTextEdit es una nueva dependencia (CPM la baja). Verificar licencia compatible.

---

## Candidato B: Exposed properties Lua

**Por qué importa:** estilo Unity. Hoy los scripts tienen constantes hardcoded (`local DEG_PER_SEC = 45.0` en `rotator.lua`). Un dev quisiera setearlas desde el Inspector sin tocar el .lua. Esto es la diferencia entre "scripts genéricos" y "scripts reusables como componentes".

**Bloques tentativos:**
- **B1 — Sintaxis declarativa**: el script declara `--[[exposed]] local speed = 5.0` y la pre-pass del ScriptSystem detecta esos comentarios mágicos al cargar.
- **B2 — Schema en `.moodmap`**: el `ScriptComponent` serializado guarda los valores override del usuario por entidad.
- **B3 — Inspector dinámico**: al seleccionar una entidad con script, el Inspector lista las exposed properties y permite editarlas (DragFloat, ColorEdit, InputText según tipo).
- **B4 — Reload sin perder valores**: hot-reload no pisa los overrides del Inspector.

**Riesgos:** schema upgrade del `.moodmap`. Hay que decidir cómo persistir tipos arbitrarios (number vs string vs vec3 vs entity ref).

---

## Candidato C: AI / pathfinding

**Por qué importa:** un shooter "Wolfenstein-like" necesita enemigos que se muevan. Hoy hay física (Jolt) pero no hay nada que decida hacia dónde. Pathfinding sobre el GridMap es la base.

**Bloques tentativos:**
- **C1 — A\* sobre GridMap**: función pura `findPath(map, start, end) -> vector<TileCoord>`. Tests headless directos. Pequeño-mediano.
- **C2 — `NavAgentComponent`**: target + speed + path cache. AnimationSystem-equivalent pero para navegación.
- **C3 — Demo enemigo**: una entidad que persigue al player en Play Mode. Steering simple (direccion del próximo tile).
- **C4 — Visualización del path**: en F1 debug, dibujar el path activo de cada NavAgent como línea cyan.

**Riesgos:** A\* sobre grid es directo, pero el día que se quiera off-grid (mallas con assimp tienen geometría arbitraria) la abstracción se tiene que rehacer. V1 grid-only.

---

## Candidato D: Save / load de gameplay

**Por qué importa:** el player puede cargar un proyecto, pero no puede "guardar partida". Si el HUD baja a HP=0, no hay forma de continuar después. Es un feature que separa demo de juego real.

**Bloques tentativos:**
- **D1 — Schema `.moodsave`**: estado serializable del runtime (player position, HUD, scripts state, entity overrides). Distinto del `.moodmap` (snapshot vs definición).
- **D2 — `Quick save / Quick load`**: hotkeys F5/F9 estilo Half-Life que persisten/restauran el estado.
- **D3 — Lua API**: `save.write(slot)` / `save.load(slot)` desde scripts.

**Riesgos:** decidir qué se persiste y qué no. Las texturas/meshes obviamente NO; los Transforms y los scripts SÍ. Los rigid bodies de Jolt mantienen estado interno (velocidades, impulsos) que sería ideal preservar.

---

## Candidato E: Networking básico

**Por qué importa:** multiplayer es un feature top-tier. Pero también es una bestia gigante que abre 50 sub-problemas (autoritative server vs P2P, lag compensation, anti-cheat, NAT punching, sincronización de scripts...).

**Recomendación:** **NO arrancar con esto** hasta que haya pedido específico del dev. Los hitos previos no setearon ninguna abstracción para clientes/servidor; arrancar networking implica reabrir Scene/Components/Serialization desde cero.

---

## Decisión recomendada

Sin más contexto del dev, mi sugerencia es **A (workflow de scripting)** como Hito 22. Razones:

1. La infra Lua ya está. Una mejora UX se ve y se siente al instante.
2. Bloque por bloque es pequeño-mediano (no un sub-engine como C o E).
3. Desbloquea iteración más rápida en hitos posteriores: si vamos por C (AI) después, el dev va a escribir mucho Lua para definir comportamientos de enemigos. Tener buen tooling de scripts antes hace que C corra más rápido.

Si el dev tiene un juego concreto en mente (ej. "quiero un shooter 3D con enemigos") la priorización cambia y C podría ir primero — porque sin AI no hay gameplay loop, mientras que el workflow de scripting es polish.

---

## Antes de arrancar

Cuando se elija el candidato:

1. Borrar las secciones que no apliquen y dejar solo el plan del candidato elegido.
2. Desglosar cada bloque con criterios de aceptación medibles.
3. Identificar dependencias externas nuevas (ImGuiColorTextEdit para A, librería de pathfinding o implementación propia para C, etc.).
4. Estimar bloques en sesiones (~2-4 hs cada uno).
5. Listar pendientes arrastrados del Hito 21 que se podrían atacar de paso (ver `HITOS.md` sección "Pendientes menores detectados en Hito 21").
