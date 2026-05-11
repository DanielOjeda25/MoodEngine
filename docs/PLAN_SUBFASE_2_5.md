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

### Bloque 2.5 — Demo characters + escena narrativa completa (F2H49 + F2H50) ✅ CERRADO

> **Estado (2026-05-11)**: **F2H49 + F2H50 cerrados.**
> - **F2H49**: pipeline animaciones standalone (FBX anim-only) + Asset Browser tab Animations + Inspector external clips + X Bot/Y Bot demo rigs commiteados.
> - **F2H49.1**: extracción de diffuse color del aiMaterial cuando no hay texture map (template chars Mixamo).
> - **F2H50**: auto-attach de sibling `anim_*.fbx` al spawn + generator `narrative_demo.moodmap` + Welcome modal button + persistencia full de AnimatorComponent (`SavedAnimator` + `externalClips`) + regen de materiales auto en SceneLoader. Demo end-to-end: spawn NPC, walk + E para hablar, dialog HL2-style funcionando.
> Ver [`HITOS.md`](HITOS.md) entries F2H49/49.1/50 + [`DECISIONS.md`](DECISIONS.md) entries 2026-05-11.


**Origen**: pedido del dev post-F2H47 (2026-05-10): *"que sentido tiene
crear un sistema de diálogo, sino tenemos a quien asignar ni como
sentir"*. El dialog runtime sin un NPC tangible para probar es
abstracto — "press E al cubo gris" no tiene la misma sensación que
"press E al guardián con animación de saludo". Este Bloque materializa
el caso de uso end-to-end con assets reales.

**Slot**: entre Bloque 2 (Diálogos: F2H48 cierra el runtime técnico) y
Bloque 1 (Inventario, próximo en el orden lógico). Razón: una vez que
F2H48 cierra, queremos validar el sistema con un demo creíble antes de
avanzar a sistemas adicionales — si el feel del dialog está mal lo
descubrimos acá, no después de armar inventory + quests encima.

**Distinción importante**: NO es el F2H35 original ("Mixamo importer
pipeline") del `PLAN_FASE2.md` — ese sería infrastructure para que
devs externos importen rigs Mixamo arbitrarios masivamente (auto-detect
+ canonical skeleton mapping + pack imports). Este Bloque es **content
production** para validar nuestros sistemas: bajamos 2 chars
específicos, los integramos manualmente, listo. El importer mayor
queda diferido a hito propio futuro si emerge demand real.

#### 2.5.1 Adquisición de assets Mixamo — `[TBD hito chico]`

**Player character**: humanoide cualquiera (sugerencia: "Ely",
"Mousey" o similar — figura clara para identificarse).
- Animaciones: **idle**, **walk**, **run**, **jump** (al menos
  jump-start), **wave** (saludo). 5 anims mínimo.

**NPC character**: humanoide distinguible del player (sugerencia: un
character medieval/fantasy para encajar con el demo dialog actual del
"guardián que enfrenta al dragón").
- Animaciones: **idle**, **talking** (gestos al hablar), **wave**,
  **look_around** (NPC reacciona pasivamente cuando no hay
  interacción). 4 anims mínimo.

**License**: Mixamo es free para uso comercial vía cuenta Adobe (la
licencia cubre exporte como FBX/glTF y uso en juegos). Credit no es
required por la TOS de Mixamo pero conviene documentar la fuente para
trazabilidad (en `assets/meshes/demo_characters/CREDITS.md`).

**Pipeline FBX → glb**:
- Mixamo exporta FBX nativo. El motor consume glTF/glb via Assimp
  (F2H10 pipeline existente, ya validado con Fox.glb del F2H44).
- Conversión via Blender (UI tradicional) o `gltf-pipeline` CLI
  (automatizable en `tools/`). Decidir al arrancar — Blender es más
  flexible para fixes manuales (orientation/scale/material tweaks),
  CLI es más reproducible.
- Validar: skeleton importa OK, todas las anims preservadas, scale
  correcto (Mixamo default es ~1 unit per cm; nuestro motor usa
  metros — escalado 0.01x al importar).

#### 2.5.2 Shipping de assets + organización — `[TBD]`

- `assets/meshes/demo_characters/player.glb` con todas sus anims
  embebidas (o como `.bin` separado si emerge size issue).
- `assets/meshes/demo_characters/npc.glb` mismo patrón.
- `assets/meshes/demo_characters/CREDITS.md` con source Mixamo +
  links + license note.
- Texturas embebidas (Mixamo exporta con diffuse + normal típicamente);
  el `AssetManager` ya extrae embedded textures desde F2H26.

#### 2.5.3 Demo scene `narrative_demo.moodmap` — `[TBD]`

Mapa pequeño dedicado al test narrativo:
- **Plano grande** (Floor brush 20x20m) con material básico.
- **Spawn point del player** en (0, 1, 0).
- **NPC entity** posicionada a ~4m del spawn (`(4, 0, 0)`) con:
  - `MeshRendererComponent` apuntando a `npc.glb`.
  - `AnimatorComponent` con clip default "idle" + state machine
    contextual (cambia a "talking" cuando dialog activo).
  - `DialogComponent` (F2H48) apuntando a `dialogs/demo_intro.mooddialog`.
  - `TriggerComponent` esfera radius 2m alrededor — cuando player
    entra, se muestra interact prompt "[E] Hablar".
- **Iluminación**: directional sun + ambient. Sin gimmicks.
- **Skybox**: el del Fox demo o uno simple.

#### 2.5.4 Update demo dialog con branching — `[TBD]`

El `demo_intro.mooddialog` actual tiene 3 nodos lineales. Para nivel B
expandirlo a 5-7 nodos con branching real:
- Nodo greeting (NPC saluda) con 3 choices.
- Choice "heroico" → nodo respuesta heroica + 2 sub-choices.
- Choice "casual" → nodo respuesta casual + 1 cierre.
- Choice "agresivo" (con condition_lua `dialog.get_var('aggressive') == true`)
  → nodo amenaza (sólo visible si flag previo seteado) → cierre.
- Branches eventualmente convergen a un nodo "fin" único.

Las choices que disparan animaciones específicas en el NPC quedan
explícitas: `animation = "talking"` por default; choice "agresivo"
podría disparar "look_around" en el NPC como reacción.

#### 2.5.5 Welcome modal: botón "Cargar demo narrativo" — `[TBD]`

Nuevo botón abajo del Fox demo (no reemplaza el Fox):
- Click → crea proyecto temp + carga `narrative_demo.moodmap` +
  abre el `.mooddialog` en el Dialog Editor.
- Tooltip: "Demo narrativo: NPC con diálogo y elección de jugador.
  Press E al guardián en Play Mode."

#### 2.5.6 Animator state machines para los chars — `[TBD]`

**Player char**:
- State `idle` cuando velocidad horizontal < 0.1 m/s.
- State `walk` cuando velocidad 0.1-3 m/s.
- State `run` cuando velocidad > 3 m/s.
- State `jump` triggered cuando space + grounded; auto-vuelve a
  idle/walk al aterrizar.
- Wave: triggered manualmente por tecla específica (Z?) o automático
  al primer encuentro con el NPC.

**NPC char**:
- State `idle` default.
- State `talking` cuando `DialogSystem::isActive() && currentSpeaker == this`.
- State `wave` triggered cuando dialog arranca por primera vez (greet).
- State `look_around` con timer aleatorio (cada 8-15s mira a un lado
  random) cuando idle prolongado.

#### 2.5.7 Validación end-to-end — `[TBD]`

Tour visual con el dev:
1. Welcome modal → "Cargar demo narrativo" → escena se carga.
2. Player char visible en el spawn, animación idle corriendo.
3. NPC visible a unos metros, animación idle con look_around
   periódicamente.
4. WASD para caminar → animación walk transition.
5. Acercarse al NPC → trigger entra → interact prompt "[E] Hablar"
   aparece en HUD.
6. Presionar E → dialog box aparece, NPC animación cambia a "talking".
7. Choices visibles → click en una → dialog avanza, NPC sigue talking.
8. Branch heroico → 2 sub-choices con condition_lua aplicado.
9. Branch agresivo solo visible si seteamos flag manualmente (test
   del condition system).
10. Cierre dialog → NPC vuelve a idle, prompt desaparece.

#### 2.5.8 Estimaciones de scope nivel B

**Bloques de implementación** (preliminar, refinar al escribir
`PLAN_HITO_F2H49.md`):

| # | Bloque | Estim. |
|---|--------|--------|
| A | Plan específico F2H49 | 30 min |
| B | Pipeline FBX→glb decisión + setup (Blender o CLI) | ~1h |
| C | Bajada de assets (manual, dev hace login Mixamo) + conversión | ~1.5h |
| D | Shipping + CREDITS.md + validación visual de los chars statics | ~30 min |
| E | Demo scene `.moodmap` | ~1.5h |
| F | Update `demo_intro.mooddialog` con branching | ~30 min |
| G | Welcome modal botón + handler | ~30 min |
| H | Animator state machines (player + NPC) | ~2-3h |
| I | Validación tour completo con dev | ~30 min |
| J | Cierre docs + commit + tag + push | ~1h |

**Total estimado**: ~10-11h. Hito mediano-largo.

**Tag previsto**: `v1.39.0-fase2-hito49` (asumiendo F2H48 toma
`v1.38.0`).

#### 2.5.9 Decisiones técnicas abiertas para F2H49

- **Pipeline FBX → glb**: Blender manual vs `gltf-pipeline` CLI.
  Recomendación: Blender en F2H49 (flexibilidad para fixes), evaluar
  CLI cuando F2H35 (Mixamo importer pipeline) emerja.
- **Anim retargeting**: si las anims de Mixamo no calzan exactamente
  con nuestro AnimationSystem (offset de root motion, root bone
  naming, scale), evaluar fixes en Blender pre-export vs runtime.
- **Char controller del player en la demo scene**: reusar el char
  controller F2H30 (capsule + WASD/jump) o setup propio. Reusar
  porque ya está validado.
- **Interact prompt**: usar el `interact_prompt` del HUD F2H39 (ya
  tenemos infra). El "press E" lo dispara el TriggerComponent del NPC.
- **Locking del player durante dialog**: cuando dialog activo,
  desactivar input del char controller para que no camine mientras
  habla. Decidir el mecanismo en F2H48 (probablemente flag global
  `GameState::dialog_active` que char controller chequea).

#### 2.5.10 NO entra en F2H49

- Mixamo importer pipeline general (F2H35 — auto-detect rigs +
  canonical skeleton + masivo packs).
- NPC AI behavior (pathfinding patrullaje + decision making). El NPC
  está parado en su posición — animado pero no se mueve. Para hacerlo
  patrullar usaríamos NavSystem (F2H23) en un hito separado.
- Combat anims (attack, hit, die). El demo es narrativo, no de combate.
- Facial expressions / lipsync. Estilo Half-Life pero ese feature es
  mayor.
- Multiple NPCs en la escena. Un solo NPC es suficiente para validar.

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
