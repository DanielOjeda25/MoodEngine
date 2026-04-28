# Plan — Hito 23: TBD (candidatos por priorizar)

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 22 cerrado).
>
> **Estado:** **TBD**. El Hito 22 cerró en `v0.22.0-hito22` con el workflow de scripting (Asset Browser tab + drop + Nuevo Script + LuaApiPanel) + bonus fix del Fox al recargar. Antes de empezar el Hito 23, conversar con el dev qué línea continúa.

---

## Candidato A (arrastrado): Mini editor de scripts in-place

**Por qué importa:** fue el Bloque 5 stretch del Hito 22 que se difirió. Hoy el dev edita los `.lua` en VS Code y el hot-reload del ScriptSystem levanta los cambios — aceptable pero rompe el flow ("alt-tab al editor de código, escribir, guardar, alt-tab al editor del motor para ver el efecto").

**Bloques tentativos:**
- **A1 — Evaluación de `ImGuiColorTextEdit`** (https://github.com/BalazsJako/ImGuiColorTextEdit): license MIT compatible, ~3000 LOC, header + cpp. Pasa por CPM. Smoke test de integración.
- **A2 — `ScriptEditorPanel`**: nuevo panel registrado en `EditorUI`. Click en un script del Asset Browser (selección, no drag) lo abre acá.
- **A3 — Save con Ctrl+S**: escribe al disco el contenido del editor. El hot-reload del `ScriptSystem` (mtime check) lo levanta automáticamente.
- **A4 — Error highlighting**: la línea del último `lastError` del ScriptComponent se pinta en rojo en el editor. Si la entidad seleccionada tiene script con error, mostrarlo.

**Riesgos:** ImGuiColorTextEdit es lib externa que afecta el tamaño del paquete. Verificar antes de abrazarla — si pesa mucho y solo el editor la usa, mantener PRIVATE en `mood_engine_lib` o solo en MoodEditor.

---

## Candidato B: Exposed properties Lua

**Por qué importa:** estilo Unity. Hoy los scripts tienen constantes hardcoded (`local DEG_PER_SEC = 45.0`). Un dev quiere setearlas desde el Inspector sin tocar el `.lua`. Esto es la diferencia entre "scripts genéricos" y "scripts reusables como componentes".

**Bloques tentativos:**
- **B1 — Sintaxis declarativa**: el script declara `--[[exposed]] local speed = 5.0` y un pre-pass del ScriptSystem (parser de comentarios mágicos) detecta esos al cargar.
- **B2 — Schema en `.moodmap`**: el `ScriptComponent` serializado guarda los overrides del usuario por entidad.
- **B3 — Inspector dinámico**: al seleccionar una entidad con script, el Inspector lista las exposed properties con widgets según tipo (DragFloat para number, ColorEdit para vec3 RGB, InputText para string).
- **B4 — Reload sin perder valores**: hot-reload no pisa los overrides del Inspector.

**Riesgos:** schema upgrade del `.moodmap` con tipos arbitrarios. Decidir cómo persistir: number/string/bool/vec3 son fáciles; entity refs y arbitrary tables son tema.

---

## Candidato C: AI / pathfinding básico

**Por qué importa:** un shooter "Wolfenstein-like" necesita enemigos que se muevan. Hoy hay física pero no decisión. Pathfinding sobre el GridMap es la base.

**Bloques tentativos:**
- **C1 — A\* sobre GridMap**: función pura `findPath(map, start, end) -> vector<TileCoord>`. Tests headless directos.
- **C2 — `NavAgentComponent`**: target + speed + path cache.
- **C3 — Demo enemigo**: una entidad que persigue al player en Play Mode. Steering simple (dirección del próximo tile).
- **C4 — Visualización del path**: en F1 debug, dibujar el path activo de cada NavAgent como línea cyan.

**Riesgos:** A\* sobre grid es directo. Off-grid (mallas con assimp con geometría arbitraria) requiere navmesh — V1 grid-only.

---

## Candidato D: Save / load de gameplay

**Por qué importa:** el player carga un proyecto pero no puede "guardar partida". Si el HUD baja a HP=0, no hay forma de continuar después. Separa demo de juego real.

**Bloques tentativos:**
- **D1 — Schema `.moodsave`**: estado serializable del runtime (player position, HUD, scripts state, entity overrides). Distinto del `.moodmap` (snapshot vs definición).
- **D2 — Quick save / Quick load**: hotkeys F5/F9 estilo Half-Life que persisten/restauran.
- **D3 — Lua API**: `save.write(slot)` / `save.load(slot)` desde scripts.

**Riesgos:** decidir qué se persiste y qué no. Texturas/meshes: NO. Transforms y scripts: SÍ. Rigid bodies de Jolt mantienen estado interno (velocidades) que sería ideal preservar.

---

## Candidato E: Networking básico

**No arrancar todavía.** Multiplayer es una bestia gigante (autoritativo vs P2P, lag compensation, sync de scripts...). Esperar hasta tener un juego concreto que lo pida.

---

## Decisión recomendada

Sin más contexto del dev:

1. **Candidato A** (mini editor) si el dev valora cerrar la línea de scripting y tener un workflow pulido antes de avanzar. Es la continuidad natural del Hito 22.
2. **Candidato C** (AI/pathfinding) si el dev quiere ver gameplay loop antes que polish UX. Sin enemigos no hay shooter.
3. **Candidato B** (exposed properties) si el dev empieza a sentir que sus scripts son demasiado rígidos. Va bien después de A o C.

Mi sugerencia: **Candidato C (AI/pathfinding)** porque es el feature más alto de la pirámide para un shooter. Sin enemigos AI todo lo anterior es plumbing — con el primer enemigo persiguiendo al player, el motor recién muestra que puede hacer un juego de verdad.

---

## Antes de arrancar

Cuando se elija el candidato:
1. Borrar las secciones que no apliquen y dejar solo el plan del candidato elegido.
2. Desglosar cada bloque con criterios de aceptación medibles.
3. Identificar dependencias externas nuevas.
4. Estimar bloques en sesiones (~2-4 hs cada uno).
5. Listar pendientes arrastrados de Hitos 21/22 que se podrían atacar de paso (ver `HITOS.md` secciones "Pendientes menores detectados").
