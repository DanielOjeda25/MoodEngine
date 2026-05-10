# PLAN — Sub-fase 2.5: Sistemas de juego engine-grade (Inventario / Diálogos / Quests)

> **Estado**: SKELETON / DRAFT (2026-05-10). Hitos individuales sin
> definir todavía — marcados `[TBD]`. **Completar este plan antes de
> tocar código.** El dev fue explícito: *"esto lo seguiré en mi
> computadora de escritorio y no quiero que divague o pierda el hilo"*.
>
> **Origen**: conversación 2026-05-10 post-F2H45 donde el dev definió
> scope nivel B (Pro Tools) y filosofía engine-grade. Ver `ESTADO_ACTUAL.md`
> sección 0 (handoff completo) y `DECISIONS.md` entrada 2026-05-10
> "Sub-fase 2.5 — commitment estratégico" para el marco no-negociable.
> Este documento **supersede** las líneas 285-303 de `PLAN_FASE2.md`
> (planeadas con scope A, ahora obsoletas).

---

## 1. Filosofía no-negociable

**Motor que crea juegos, no juego concreto.** Cualquier decisión técnica
de la sub-fase que viole uno de los siguientes 8 principios debe
detenerse y pedir confirmación al dev:

1. **Data-driven, no code-driven**: contenido se crea sin recompilar el motor.
2. **Sin semántica hardcodeada de gameplay**: el motor no conoce "XP", "mana", "level". Solo contenedores genéricos.
3. **Hooks Lua sobre primitivas**: el dev del juego enchufa lógica vía callbacks Lua, el motor solo dispara los eventos.
4. **Default + override en rendering**: widget HUD default funcional + capacidad de override total via Lua.
5. **Composabilidad con sistemas existentes**: predicados genéricos contra estado del motor (item_count / flag_set / area_entered / counter_at_least).
6. **Editor visual real, no JSON a mano**: cada sistema tiene su panel de editor dedicado.
7. **State vs rendering separados**: patrón F2H39 `GameState` ↔ `GameOverlay` — testeable sin ImGui.
8. **Engine-agnostic respecto al género**: la API sirve a RPG / shooter / walking sim / metroidvania / dungeon crawler.

---

## 2. Estructura de bloques

### Bloque 0 — Infra compartida (PRE-requisito de Bloques 2 + 3)

Sistemas reutilizables que consumen los editores de los Bloques siguientes.
Implementarlos PRIMERO evita reescribir cuando llegan los demás.

#### 0.1 Node-graph framework — `[TBD hitos]`

Framework de edición visual de grafos reutilizable. Usado por Dialog
Editor (Bloque 2) y Quest Editor (Bloque 3). Eventualmente Material
Editor pro version.

**Features mínimas:**
- Pan / zoom / snap a grid del canvas.
- Nodos draggeables con socket inputs/outputs tipados.
- Conexiones por curvas Bézier entre sockets compatibles.
- Selección múltiple + delete + copy/paste de nodos.
- Save/load del grafo a JSON.
- Undo/Redo de operaciones (add/remove/move/connect) via `HistoryStack`.

**Decisión abierta**: framework propio sobre ImGui DrawList vs adoptar
`imnodes` / `imgui-node-editor`. Documentar pros/cons con código de
muestra antes de elegir.

**Pros propio**: control total look-and-feel + integración estética + sin dependencia.
**Pros adoptar**: ~10x menos esfuerzo + battle-tested.

#### 0.2 3D preview widget — `[TBD hitos]`

Widget reutilizable de render-to-texture de un mesh/material con
controles de cámara orbital. Usado por Item Browser (Bloque 1) +
eventualmente Material Editor pro.

**Features mínimas:**
- Render-to-texture de un mesh con material aplicado.
- Cámara orbital con drag-to-rotate + scroll-to-zoom.
- Lighting básico hard-coded (key + fill + rim) para que el preview se vea bien.
- Background neutro (gradiente gris / checker pattern).
- Toggle wireframe / textured.

---

### Bloque 1 — Inventario (RECOMENDADO arrancar acá)

Diálogos y quests dependen de "tengo X item" para condicionales —
empezar por items evita hardcodear flags `game.has_key` que después
hay que migrar.

#### 1.1 ItemAsset schema + serialización — `[TBD hitos]`

`.mooditem` JSON con: id estable, nombre (i18n key), descripción
(i18n key), icono (path a textura), modelo 3D (path opcional), tags
custom (strings), stats (key-value genérico), stack rules (max stack
size, stackable bool).

**Decisión abierta**: ¿stats como `unordered_map<string, float>` libre
o struct con campos predefinidos? Lo segundo viola principio 2 (sin
semántica hardcodeada). Confirmar: key-value libre.

#### 1.2 Item Browser panel — `[TBD hitos]`

Panel del editor estilo Asset Browser pero específico para items.

**Features mínimas:**
- Grid de cards con icono + nombre + categoría (derivada de tags).
- Filtros por tag / categoría / search por nombre.
- Click → property editor a la derecha (campos del schema).
- 3D preview del modelo (consume Bloque 0.2).
- Botón "+" crea item nuevo con defaults.
- Botón "duplicar" copia el item seleccionado con id nuevo.

#### 1.3 InventoryComponent — `[TBD hitos]`

Componente attachable a entidades (no solo player — NPCs pueden tener
inventario, cofres son entidades con inventario).

**3 modos de layout configurables por instancia:**
- **Grid 2D Resident Evil**: cells WxH, items ocupan rectángulos según
  su `slot_size` definido en ItemAsset.
- **Lista plana**: items uno tras otro con cantidad. Estilo Skyrim /
  Diablo simplificado.
- **Equipment slots**: slots nombrados ("weapon", "head", "torso", etc)
  donde solo items con el tag correspondiente encajan. Para equipo.

**API Lua**:
- `inventory.add(entity, "item_id", quantity)`
- `inventory.remove(entity, "item_id", quantity)`
- `inventory.has(entity, "item_id", min_quantity)` → bool (para predicados de quests)
- `inventory.count(entity, "item_id")` → int
- `inventory.on_pickup(callback)`, `inventory.on_drop(callback)` — hooks

#### 1.4 Pickup/drop integrado — `[TBD hitos]`

Item entities en el mundo (entidades con `ItemPickupComponent` que
referencia un ItemAsset) son pickeables via trigger + interact prompt.

**Flow**:
1. Player entra al trigger del item → `interact_prompt` del HUD muestra "[E] Pick up X".
2. Player presiona E → `inventory.add(player, "item_id", 1)` + destroy item entity (o despawn animation).
3. Item se puede arrastrar/spawnear desde el Inspector via "Spawn Item" button que crea la entity con ItemPickupComponent referenciando.

#### 1.5 HUD widget inventory — `[TBD hitos]`

Widget del HUD framework F2H39/F2H41 (entry en `k_widgets[]`) que
renderea el inventario.

**Features mínimas:**
- Open/close con tecla configurable (default Tab).
- Grid visual del inventario actual del player.
- Drag-drop entre slots.
- Tooltip por hover (nombre + descripción + stats).
- **Default renderer + override via** `inventory.set_renderer(lua_callback)` para juegos con UI propia.

---

### Bloque 2 — Diálogos

#### 2.1 DialogTree schema + serialización — `[TBD hitos]`

`.mooddialog` JSON con: lista de nodos `{id, text (i18n key), portrait_path, audio_path, animation_id, options[]}`, donde cada option tiene `{label (i18n key), next_node_id, condition_predicate, on_select_hook}`.

#### 2.2 Dialog Editor (node-graph) — `[TBD hitos]`

Sobre el framework Bloque 0.1. Cada nodo de diálogo es un nodo del
grafo; cada opción es un socket output que conecta al nodo destino.
Conditions y hooks editables por nodo.

#### 2.3 Dialog runtime + HUD widget — `[TBD hitos]`

State machine que recorre el tree según las opciones elegidas por el
player. HUD widget default (caja inferior con texto + opciones
clickeables, estilo Half-Life 2). Override via Lua.

#### 2.4 Bindings Lua — `[TBD hitos]`

- `dialog.start("npc_id")`, `dialog.is_active()`, `dialog.current_node()`.
- `dialog.set_var("flag", value)`, `dialog.get_var("flag")` — variables persistentes.
- `dialog.on_node_enter(callback)`, `dialog.on_choice(callback)` — hooks.
- `dialog.set_renderer(lua_callback)` — override del rendering default.

---

### Bloque 3 — Quests

#### 3.1 Quest schema + serialización — `[TBD hitos]`

`.moodquest` JSON con: id, name (i18n key), description (i18n key),
state (Available / Active / Complete / Failed), objectives[], rewards[],
prerequisites[].

**Objectives** son predicados genéricos contra estado:
- `item_count(player, "item_id") >= N`
- `flag_set("flag_name") == bool`
- `area_entered("area_id")`
- `counter_at_least("counter_name", N)`
- `entity_killed_tag("enemy_tag", N)` — el counter "enemy_tag_killed" lo incrementa el dev desde Lua cuando un enemy con ese tag muere.

**Rewards** son acciones genéricas:
- `give_item("item_id", quantity)`
- `set_flag("flag_name", value)`
- `add_counter("counter_name", value)`
- `run_lua("script.lua")` — para todo lo demás.

#### 3.2 Quest Editor (flowchart) — `[TBD hitos]`

Sobre el framework Bloque 0.1. Objectives como nodos con dependencias
(some objectives unlock when others complete). Rewards como nodos
finales. Prerequisites como inputs del quest entero.

#### 3.3 Quest runtime + Quest Log panel + HUD tracker — `[TBD hitos]`

State machine que evalúa los objectives cada frame (o vía eventos —
decidir). Quest Log panel del editor para inspeccionar quests activos
durante Play Mode. HUD widget tracker que muestra el quest activo (el
ObjectiveText de F2H41 es el embrión — extender para mostrar el
quest tracked).

#### 3.4 Bindings Lua — `[TBD hitos]`

- `quest.start("id")`, `quest.complete("id")`, `quest.fail("id")`.
- `quest.objective_complete("id", "obj_id")`.
- `quest.track("id")` — marca el quest como el que se muestra en el tracker.
- `quest.set_flag("flag")`, `quest.add_counter("counter", N)`.
- `quest.on_complete(callback)`, `quest.on_objective_complete(callback)`.

---

### Bloque 4 — Soporte para producción

Sin esto los 3 sistemas no son "engine-grade" reales.

#### 4.1 Stats / RPG primitives genéricos — `[TBD hitos]`

`StatsComponent` con map `unordered_map<string, float>` libre + modifiers
con duración (buffs/debuffs).

**API Lua**:
- `stats.set(entity, "key", value)`, `stats.get(entity, "key")` → number.
- `stats.add(entity, "key", delta)` — atomic.
- `stats.add_modifier(entity, "buff_id", key, delta, duration_sec)`.
- `stats.on_change("key", callback)` — hook cuando un stat cambia.

**NO hardcodea XP/HP/Mana**. El dev del juego usa `stats.set(player, "xp", 100)` y registra su semántica.

#### 4.2 Save extendido v3 — `[TBD hitos]`

Bumpea `.moodsave` a v3 con `dialog_state`, `quest_state`, `inventory`,
`stats`. Backward-compat: saves pre-v3 cargan con defaults para campos
nuevos (mismo patrón que F2H38/F2H39).

**Atómico v3**: el save serializa el state COMPLETO de los 4 sistemas
para que un load garantice estado consistente (ej. quest tracked sigue
tracked al volver).

#### 4.3 Template/sample project — `[TBD hitos]`

"Hello World" narrativo del motor — proyecto auto-cargable desde
Welcome modal (botón "Cargar proyecto demo: aventura narrativa") que
muestra los 4 sistemas integrados:

- NPC con `DialogTree` configurado en un Dialog Editor visualmente.
- Quest "Encontrar la llave" con objectives (`item_count == 1` para
  "key") y rewards (`give_item("treasure")`).
- Inventario del player con grid 4x6 + slot de "weapon".
- Stats genérico con "hp" y "gold" registrados.

**Critical para devs nuevos**: sin un sample funcional, los 3 sistemas
parecen abstractos y son difíciles de adoptar.

#### 4.4 Docs developer-facing — `[TBD hitos]`

Doc nuevo `docs/CONTENT_PIPELINE.md` (NO `CLAUDE.md` — esto sí justifica
doc externo porque devs externos lo necesitan):

- Cómo crear un item desde cero (Item Browser tutorial paso a paso).
- Cómo armar un diálogo simple en el Dialog Editor.
- Cómo configurar un quest con objectives y rewards.
- Cómo enganchar los hooks Lua para extender comportamiento.
- Ejemplos comunes (NPC merchant, quest collect-N, dialog con flags).

---

## 3. Decisiones técnicas pendientes (a resolver al arrancar cada bloque)

### Bloque 0.1 — Node-graph framework: propio vs `imnodes`/`imgui-node-editor`

| Aspecto | Propio | `imnodes` / `imgui-node-editor` |
|---------|--------|---------------------------------|
| Esfuerzo inicial | Alto (~3-5 hitos) | Bajo (~1 hito integración) |
| Control look-and-feel | Total | Limitado al API de la lib |
| Integración estética con motor | Garantizada | Requiere theming custom |
| Battle-tested | No | Sí (`imgui-node-editor` usado en producción) |
| Dependencia externa nueva | No | Sí (header-only OK) |
| Capacidad de evolucionar features motor-específicas | Total | Constreñido al API |

**Recomendación previa al evaluar**: `imgui-node-editor` (de thedmd) por
balance esfuerzo/calidad. Pero confirmar con código de muestra antes
de comprometer.

### Bloque 1.3 — InventoryComponent: ¿qué modo de layout es default?

3 opciones (grid 2D RE / lista plana / equipment slots) son configurables
por instancia. Pero el Inspector necesita un default cuando se crea el
componente. **Propuesta**: lista plana (más universal, se entiende sin
contexto). Devs RPG cambian a grid 2D, devs survival horror a equipment.

### Bloque 2.3 — Dialog runtime: ¿tick por frame o event-driven?

Tick por frame es simple pero scala mal (cientos de dialog trees activos
es raro pero posible en mundos abiertos). Event-driven (state machine
solo procesa cuando hay input) es más eficiente pero requiere arquitectura
más cuidadosa.

**Propuesta**: tick por frame en v1 — solo un dialog activo a la vez
en la práctica (player solo habla con un NPC). Refactor a event-driven
si emerge presión.

### Bloque 3.1 — Quest objectives: ¿predicados puros declarativos o lambdas?

Predicados declarativos (`{type: "item_count", item: "key", min: 1}`) son
serializables sin custom code y editables visualmente. Lambdas Lua
permiten predicados arbitrarios pero requieren tocar código.

**Propuesta**: predicados declarativos para los 5 tipos identificados
(item_count, flag_set, area_entered, counter_at_least, entity_killed_tag)
+ escape hatch `lua_predicate("script_name")` para casos custom. Cubre
99% de los casos sin sacrificar power.

---

## 4. Riesgos identificados

- **Scope creep**: el dev autorizó "días que cueste" — pero hay que
  evitar gold-plating. Cerrar cada hito con criterios duros: "node-graph
  framework hace pan + zoom + sockets tipados con curvas Bézier + save/load
  + undo de las 5 operaciones core. No agregar export-to-image o
  hierarchical grouping a v1".

- **API instability**: cada hito agrega bindings Lua. Si la primera
  versión del API es mala, devs externos sufren breaking changes.
  **Mitigación**: revisar la API con el dev antes de exponerla via Lua
  (no solo "funciona" — también "es la firma correcta para los próximos
  10 años").

- **Editor visual no usable**: el riesgo más alto del scope B. Si los
  node-graph editors son confusos o lentos, los devs externos los
  evitarán y harán contenido en JSON crudo — fallando el principio 6.
  **Mitigación**: usability test del Dialog Editor con el dev sentado
  haciendo un diálogo real ANTES de cerrar Bloque 2. Si tarda más de
  15 min en armar un diálogo simple de 3 nodos, refinar.

- **Performance del runtime**: 1000 quests activos evaluando objectives
  cada frame podría ser cuello. **Mitigación**: arrancar simple (tick por
  frame), instrumentar con `Profiler` desde el principio, optimizar al
  emerger presión real (no premature).

- **Save/load v3 backward-compat**: complejo porque hay 4 sistemas
  nuevos. **Mitigación**: schema versionado + campos opcionales en JSON
  + tests que carguen saves v2 (F2H38/F2H39/F2H41) garantizando que
  siguen funcionando. Patrón establecido.

---

## 5. Diferidos explícitos (lo que NO entra en Sub-fase 2.5)

- **Combat system**: damage types, hitboxes, etc. Stats system del
  Bloque 4.1 provee el contenedor genérico pero no implementa combat
  loop. Sub-fase 2.6+ si emerge.
- **Cinematics / cutscenes**: dialog system puede ser usado como capa,
  pero no hay cinematic editor en Sub-fase 2.5.
- **AI behaviors complejos**: NavSystem del Hito 23 cubre pathfinding;
  state machines de AI (utility AI, GOAP, behavior trees) son sub-fase
  propia futura.
- **Networking**: multiplayer de inventario / dialog está fuera de
  Fase 2 entera (Fase 3 candidate).
- **Modding API**: exponer estos sistemas a mods externos (Steam Workshop
  style) es post-Fase 2.
- **Localización de assets distintos a strings**: voiceover por idioma,
  texturas localizadas (signos en español vs inglés en el mundo), etc.
  Strings cubre 99% del caso, el resto es Fase 3.

---

## 6. Próxima acción concreta

**Cuando la próxima sesión retome este plan**:

1. Leer `ESTADO_ACTUAL.md` sección 0 + `DECISIONS.md` entrada
   "Sub-fase 2.5 commitment estratégico" + este documento.
2. Confirmar con el dev:
   - Orden propuesto (Bloque 0 → 1 → 2 → 3 → 4) ¿OK?
   - Recomendación inventario primero ¿OK?
   - Decisión técnica Bloque 0.1 (propio vs lib): ¿evaluar al arrancar
     o tener preferencia ya?
3. Para el Bloque 0.1 (primer hito real): escribir un PLAN_HITO_F2HXX
   específico siguiendo el patrón habitual (objetivo, bloques de
   ejecución A-H, criterios de aceptación, validación, NO entra, etc).
4. **NO** arrancar implementación sin plan específico aprobado.
