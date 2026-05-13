# PLAN HITO F2H52 — Inventory runtime (pickup + HUD widget + Lua bindings + dialog gating)

> **Estado**: DRAFT pendiente aprobación del dev (2026-05-12).
> **Tag previsto**: `v1.40.0-fase2-hito52`.
> **Origen**: cierra el lado runtime del Bloque 1 del `PLAN_SUBFASE_2_5.md`
> (Inventario). F2H51 cerró autoría + state + persistencia. F2H52 hace
> usable el sistema en Play Mode: items pickeables en el mundo + HUD
> visible + Lua bindings para que el dev controle todo desde scripts +
> integración con dialog choices condicionales por items.

---

## 🎮 Preguntas para el dev (en mecánicas — empezá leyendo esto)

Antes de arrancar la implementación, confirmá / ajustá estas 4 cosas:

### 1. ¿Quién es "el jugador" en tu juego?

El motor necesita identificar qué entidad es el jugador para que cuando vos escribas en Lua `inventory.has("items/llave.mooditem")` (sin pasar quién) lo busque automáticamente.

- **Propuesta:** el motor busca la entidad que tenga el tag **"Player"**.
- Si tu juego usa otro nombre (Hero, Protagonist, Nick), decímelo y lo configuro.
- Si querés, puede ser configurable: vos al inicio del juego decís *"esta entity es el player"* y el motor lo recuerda.

### 2. Cuando el jugador levanta un item, ¿desaparece del piso?

- **Propuesta:** SÍ por default — caso común (90% de los items).
- Habrá un flag por item para dejarlo fijo (máquinas que dan pociones infinitas, dispensers de munición, items de testing, etc.).

### 3. ¿Con qué tecla se abre el inventario?

- **Propuesta:** Tab por default.
- Configurable: si querés "I" (Doom), "Q" (otros juegos), o lo que sea, se puede cambiar desde Lua.

### 4. Cuando usás un item (poción, etc.), ¿el motor lo consume automáticamente?

- **Propuesta:** NO. El dev del juego decide qué hace cada item.
- Ejemplo poción: vos en Lua tipeás *"al usar poción → curar +50 HP + sacar la poción del inventario"*. Si el item es un arma equipable, no la sacás del inventario al usarla.
- Esto permite items que se usan repetidas veces (armas, herramientas) y items consumibles (pociones, comida) sin que el motor asuma cuál es cuál.

### Mecánicas que F2H52 entrega (resumen — para releer cuando vuelvas)

1. Items aparecen en el piso, te acercás, "[E] Levantar", apretás E, va a tu inventario.
2. Notificación tipo "+ Health Potion x1" al levantar (reuso del widget de pickups F2H39).
3. Tab abre tu inventario. Tooltip por hover con nombre + descripción + stats.
4. Click derecho sobre item → "Usar / Tirar". "Usar" dispara la lógica del juego (lo que el dev programe).
5. Arrastrar items entre slots (en grid 2D estilo RE, mover de un cell a otro).
6. Cofres / contenedores: te acercás, [E] Abrir, panel split tu inv ↔ inv del cofre, arrastrás entre los dos.
7. Diálogos con opciones que aparecen solo si tenés cierto item (la opción "Mostrar llave" aparece solo si tenés la llave).
8. Spawn de items desde el editor: arrastrás un item al viewport 3D y aparece en el mundo, sin código.
9. Hooks en Lua para programar las reacciones del juego (al levantar / al soltar / al usar).
10. HUD del inventario reemplazable: si querés un Pip-Boy custom o RE rotador 3D, podés reemplazar el dibujo desde Lua sin tocar el motor.
11. El motor sabe sumar el peso (o cualquier stat) del inventario → vos escribís la regla de encumbrance en Lua si la querés.

Cuando confirmes / ajustes las 4 preguntas, arranco con el primer bloque (items pickeables + tecla E). El resto del documento (abajo) es el detalle técnico para mí — no necesitás leerlo.

---

## Filosofía

**Engine-grade strict (regla del VISION).** El motor expone **primitivas**;
el dev del juego implementa la semántica. Aplicado a F2H52:

- El motor NO sabe que "health_potion cura HP" — el dev registra
  `inventory.on_use("items/health_potion.mooditem", function(entity)
  hud.set_hp(hud.get_hp() + 50) end)` en Lua. Mismo modelo de hooks
  que F2H48.1 (Dialog).
- El motor NO sabe que "weight > capacity → reduce speed" — expone
  `inventory.sum_stat(entity, "weight")` y el dev tipea la regla en
  Lua sobre el CharacterController.
- El motor NO sabe que "key abre puerta" — el dev tipea
  `condition_lua = "inventory.has('items/key.mooditem')"` en una dialog
  choice o en un trigger.

El motor SÍ provee:
- Componente `ItemPickupComponent` (entity pickeable en el mundo).
- HUD widget default `inventory_panel` (open/close con Tab).
- Bindings Lua para mutar/leer/observar inventarios.
- Hooks `on_pickup/on_drop/on_use` que el dev engancha.

## Scope (decisiones a confirmar con el dev al arrancar)

### `ItemPickupComponent` (entity-side)

Nuevo componente en `Components.h`:

```cpp
struct ItemPickupComponent {
    std::string itemPath;      // p.ej. "items/iron_sword.mooditem"
    int         quantity = 1;
    bool        destroyOnPickup = true;  // false = stays in world (pickup
                                          //         spam para tests / item
                                          //         infinito tipo machine)
    // Estado runtime (no serializado): cache del ItemAssetId
    // (re-resuelto on-demand via AssetManager).
    u32         cachedItemId = 0;
};
```

**Flow del pickup:**
1. Entity en mundo tiene `MeshRenderer` (visual) + `TriggerComponent`
   (volumen detector) + `ItemPickupComponent`.
2. Player entra al trigger → `DialogInteractSystem` style system
   (`ItemPickupSystem`) muestra `hud.interact_prompt = "[E] Pick up <name>"`.
3. Player presiona E → `inventory.add(player_entity, itemPath, quantity)`.
4. Si `destroyOnPickup=true` → destruir entity. Trigger hook
   `inventory.on_pickup(entity, item_id, qty)` Lua.

**Spawn methods:**
- **Drag-drop desde Item Browser al viewport** (extends Bloque F del F2H51):
  payload `MOOD_ITEM_ASSET` aceptado por el viewport → crea entity con
  Transform (raycast del cursor al primer mesh/floor) + MeshRenderer
  (usa `ItemAsset.model_path` si está; si no, default cube) +
  TriggerComponent (halfExtents 0.5x0.5x0.5) + ItemPickupComponent.
- **Lua**: `inventory.spawn_pickup(item_path, x, y, z, qty)` → return entity_id.
- **Inspector**: una entity nueva puede agregar `ItemPickupComponent`
  manualmente desde popup "Add Component" cat. Logic.

**Persistencia `.moodmap`:** nuevo `SavedItemPickup { itemPath, quantity, destroyOnPickup }`
en `SavedEntity` (similar a `SavedDialog`). Bump aditivo.

### Lua bindings — tabla `inventory`

```lua
-- Lecturas (puras):
inventory.has(entity, item_path, min_qty)  -- bool (default min_qty=1)
inventory.count(entity, item_path)         -- int
inventory.sum_stat(entity, stat_name)      -- float (suma stats.<name> * qty
                                            --        de todos los entries)
inventory.entries(entity)                  -- array de {item_path, qty, slot_index}

-- Mutaciones:
inventory.add(entity, item_path, qty)      -- bool (true si entró todo)
inventory.remove(entity, item_path, qty)   -- bool (atómico)
inventory.move_slot(entity, from, to)      -- bool (solo Grid2D/Equipment)

-- Runtime helpers:
inventory.use(entity, item_path)           -- llama el callback on_use
                                            --   registrado. NO consume
                                            --   item automaticamente — el
                                            --   callback decide si remove
                                            --   adentro.
inventory.spawn_pickup(item_path, x, y, z, qty)  -- entity_id (mundo)

-- Hooks (callbacks que el dev registra una vez al start):
inventory.on_pickup(callback)   -- callback(entity, item_path, qty)
inventory.on_drop(callback)     -- callback(entity, item_path, qty)
inventory.on_use(callback)      -- callback(entity, item_path)

-- Shortcut "player" implícito (el motor cachea el primer entity con
-- TagComponent.name == "Player" — convención del demo F2H50):
inventory.has(item_path, min_qty)   -- asume player
inventory.count(item_path)
inventory.add(item_path, qty)
inventory.remove(item_path, qty)
```

**Filosofía hooks vs return values:**
- `inventory.add` retorna `bool` (true si entró todo el qty solicitado, false
  si fallo por capacity).
- Hooks `on_pickup`/`on_drop`/`on_use` se disparan SIEMPRE en éxito.
  No interrumpibles (no son veto-style — son "after success
  notifications"). Si el dev necesita veto, lo implementa con
  `inventory.has` check antes.

### Dialog choice gating por items (extensión F2H48.1)

`DialogScriptHost` (sol::state global del juego) gana acceso a la tabla
`inventory` con los shortcuts player-implicit. Permite:

```jsonc
// dialog_node.choice
{
  "label_key": "convince_with_evidence",
  "condition_lua": "inventory.has('items/evidence.mooditem')",
  "on_select_lua": "inventory.remove('items/evidence.mooditem', 1)"
}
```

Sin tocar el schema `.mooddialog` — solo más bindings disponibles para
el `condition_lua` / `on_select_lua`. Mismo fail-safe que F2H48.1
(parse error → choice no aparece).

### HUD widget `inventory_panel` (default renderer)

Nuevo widget en el registry de F2H39 (16vo widget). Default OFF.
Toggle con Tab (configurable) o via Lua `hud.set_widget("inventory_panel", true)`.

**Rendering por mode:**
- **FlatList**: tabla con icono (32x32) + nombre + qty + tooltip por
  hover (description + stats).
- **Grid2D**: grid de cells (64x64) con icon centrado + badge qty en
  esquina si stackable y qty>1. Drag entre cells (mover slot).
  Right-click → menú "Use / Drop".
- **EquipmentSlots**: lista vertical de slots labeled (cada uno con su
  tag_filter visible en tooltip). Drag desde otra parte del inventario
  → solo encaja si tag matches.

**Container split mode** (`hud.open_container(entity_id)`): renderea 2
columnas — player inventory izq + container inventory der. Drag entre
las dos transfiere items. Cierre con ESC o cuando player sale del trigger.

**Override engine-grade:** `inventory.set_renderer(callback)` permite al
dev del juego override total del rendering (caso: HUD custom estilo Pip-Boy
o RE inventory rotador 3D). Si callback registrado, el motor llama eso
en lugar del default.

### Encumbrance (NO se implementa — es del juego)

El motor expone `inventory.sum_stat(entity, "weight")`. El dev del juego
hace:

```lua
function on_update(entity, dt)
    local weight = inventory.sum_stat(entity, "weight")
    local capacity = 50.0
    if weight > capacity then
        entity.character.speed_multiplier = 0.5
    else
        entity.character.speed_multiplier = 1.0
    end
end
```

El motor NO sabe que existe "encumbrance" como concepto. Solo provee la
primitiva de sumar stats. Cualquier mecánica derivada vive en Lua.

### "Use item" (NO se implementa — es del juego)

`inventory.use(entity, item_path)` invoca el callback `on_use` registrado.
El callback hace lo que el dev del juego decida:

```lua
inventory.on_use(function(entity, item_path)
    if item_path == "items/health_potion.mooditem" then
        hud.set_hp(hud.get_hp() + 50)
        inventory.remove(entity, item_path, 1)
    elseif item_path == "items/iron_sword.mooditem" then
        -- equip logic
    end
end)
```

Motor NO hardcodea "potion → heal". Engine-grade preservado.

### Bloques de ejecución

| # | Bloque | Estim. | Notas |
|---|--------|--------|-------|
| A | Plan (este doc) | 30 min | Aprobar con dev antes de B |
| B | `ItemPickupComponent` + popup Add Component + persistencia `SavedItemPickup` | ~1.5h | Componente + write/parse/apply + 2 tests roundtrip |
| C | `ItemPickupSystem` (interact_prompt + tecla E → add + destroy + hook on_pickup) | ~1.5h | Mismo patrón que DialogInteractSystem F2H48 |
| D | Drag-drop al viewport para spawn pickup desde Item Browser | ~1h | Extends `DemoSpawners_Drop.cpp` con handler `MOOD_ITEM_ASSET` payload |
| E | Lua bindings tabla `inventory` (lecturas + mutaciones + player-implicit shortcuts + sum_stat + spawn_pickup) | ~2h | Mismo patrón que `dialog` tabla F2H48. Resolución de "player" via tag scan |
| F | Lua hooks `on_pickup`/`on_drop`/`on_use` + binding `inventory.use` | ~1h | Callbacks std::function inyectados; mismo modelo `setEvaluator/Executor` que F2H48.1 |
| G | `DialogScriptHost` extendido con `inventory.*` (gating de choices) | ~30 min | Sumar bindings al sol::state existente |
| H | HUD widget `inventory_panel` default renderer (3 modes) + Tab toggle + tooltip + drag entre slots | ~3h | Widget framework F2H39; 16vo en el registry |
| I | HUD container split mode + `hud.open_container(entity_id)` binding | ~1.5h | Reusa renderer del Bloque H con flag |
| J | Override: `inventory.set_renderer(callback)` para HUDs custom | ~30 min | Mismo patrón que `dialog.set_renderer` planeado de Bloque 1.5 |
| K | Tests Lua bindings + tests dialog choice gating con items + tests roundtrip `ItemPickupComponent` | ~2h | `test_lua_bindings.cpp` extend + nuevo `test_inventory_runtime.cpp` |
| L | Sample Lua demo (pickup + use heal potion + dialog choice por evidence) | ~1h | `assets/scripts/inventory_demo.lua` ejercita el flow completo |
| M | Tour visual con dev | 30 min | Spawn item → walk → press E → ver HUD → use potion → trigger dialog choice por key |
| N | Cierre docs + commit + tag | ~1h | Patrón estándar |

**Total estimado**: ~17-18h. Hito mediano-grande (paridad con F2H51).

## Decisiones técnicas abiertas

### Resolución del "player entity" implícito

¿Cómo encuentra el motor a "player" cuando el dev escribe
`inventory.has("items/key.mooditem")` sin pasar entity?

- (a) **Scan por TagComponent.name == "Player"** al primer call, cache la entity.
  Si no hay tal entity → warn + return false.
- (b) **GameState::playerEntity()** seteado por el dev al inicio del juego
  via `engine.set_player(entity)`.

**Recomendación v1**: (a) por convención del demo F2H50 (la entity del
player tiene tag "Player"). Si esa convención no se cumple, el dev usa la
forma explícita `inventory.has(entity, path)`.

### `destroyOnPickup` default

¿`true` o `false` cuando el dev crea un nuevo `ItemPickupComponent`?

- (a) **`true` (default)**: pickup desaparece al levantarlo. Caso común
  (90% de los items).
- (b) **`false`**: pickup queda en el mundo. Caso raro (items infinitos
  / test fixtures).

**Recomendación v1**: (a) `true`. El dev cambia para machines/test.

### Auto-add `ItemPickupComponent` al spawn (drag-drop)?

¿Cuando el dev arrastra un `.mooditem` al viewport, qué componentes se
agregan a la entity?

- Mínimo: `TransformComponent` + `MeshRenderer` (si `model_path` está) +
  `TriggerComponent` (halfExtents 0.5x0.5x0.5) + `ItemPickupComponent`.
- ¿RigidBody Static? Útil si el dev quiere físicas (caer al piso).
  Pero RB Static no cae — tendría que ser Dynamic + un RB que se duerma.
  Scope creep.

**Recomendación v1**: SIN RigidBody. El pickup flota en la posición del
spawn. Si el dev quiere físicas, agrega RB manualmente desde Inspector.

### Tab key como toggle del `inventory_panel`

¿Hardcodeada o configurable?

- (a) **Hardcoded "Tab"** en `ItemPickupSystem`/`InventoryUiSystem`.
- (b) **Configurable** via `inventory.set_toggle_key(SDL_KEY_TAB)`.

**Recomendación v1**: (b) configurable, default Tab. El dev puede
cambiar a "I" (Doom-style) si quiere.

### `inventory.use` semantic

¿`inventory.use` consume el item automáticamente o el callback decide?

- (a) **Auto-consume**: motor llama `state.remove(item_id, 1)` antes del
  callback. Callback solo reacciona.
- (b) **Callback decide**: motor solo invoca callback con `(entity, item_path)`.
  Si callback no remove, el item se queda. Permite usos repetidos
  (ej. arma equipada).

**Recomendación v1**: (b) callback decide. Más flexible — equip/unequip
no consume; potion sí consume. El dev tipea `inventory.remove(...)` dentro
del callback cuando aplica.

## Diferidos a hitos futuros (NO entran en F2H52)

- **Crafting / combine items** (RE: herb_red + herb_green → herb_mega) —
  hito propio Sub-fase 3+ si emerge.
- **Item durability degradation per use** — campo `durability_current`
  + Lua hook que el dev decrementa por `on_use`. Sumar como flag opcional
  en otro hito.
- **Steal/trade UI con NPCs** — derivación del container split mode pero
  con consent del NPC. Hito propio.
- **Examine 3D del item** (RE rotar item con cámara orbital) — requiere
  Bloque 0.2 del PLAN_SUBFASE_2_5 (3D preview widget reusable). Diferido.
- **Quick-use hotkeys** (1-9 atajos a items específicos) — feature UI
  específica. Si emerge, hito chico.
- **Currency / vendor** — distinto sistema. Fuera de scope F2H52.
- **Drag-drop pickup → spawn físico** (item cae al piso con física) —
  necesita RB Dynamic + sleep. Si emerge, hito chico.
- **Inventory weight como speed_multiplier hardcoded** — viola engine-grade.
  Dev del juego lo hace en Lua leyendo `sum_stat`.

## Riesgos y tradeoffs

- **Riesgo: Lua bindings + hooks crean acoplamiento implícito.** El dev
  registra `on_use` una vez al start del juego; si registra 2 veces el
  segundo override silenciosamente al primero. Mitigación: warn en logs
  si `on_use` se registra > 1 vez. Mismo patrón F2H48.1 con dialog hooks.

- **Riesgo: HUD widget con 3 modes complica el render.** Cada mode tiene
  layout distinto, drag rules distintas, tooltip placement distinto.
  Mitigación: separar lógica en `drawFlatList` / `drawGrid2D` /
  `drawEquipmentSlots`; tests visuales de cada mode en el tour.

- **Riesgo: spawn_pickup desde Lua puede crear entity huérfanas si el
  dev no controla cleanup.** Mitigación: `destroyOnPickup=true` por
  default; warning si entity sin Trigger/Mesh.

- **Tradeoff: "player implicit" via tag scan.** Si el dev del juego usa
  otro tag (ej. "Hero", "Protagonist"), los shortcuts no funcionan.
  Aceptable: el dev usa forma explícita con entity, o documentamos la
  convención "Player" en el VISION.

- **Tradeoff: HUD widget con drag entre slots requiere tracking de
  estado del drag.** ImGui native dragdrop no es trivial entre cells
  custom. Mitigación: usar el mismo payload pattern que el Item Browser
  (`MOOD_INVENTORY_SLOT` con `{entity_id, slot_index}` 8 bytes).

## Validación (tour visual con dev)

1. Welcome modal → "Nuevo proyecto" → workspace Gameplay.
2. Item Browser muestra `iron_sword`/`health_potion`/`mysterious_key`/`Daga`
   (los 4 commiteados).
3. **Drag `health_potion` al viewport** → entity con MeshRenderer + Trigger
   + ItemPickup aparece en el mundo en posición del drop.
4. Press Play → spawn player (entity con tag "Player" + InventoryComponent
   FlatList max_items=20).
5. Walk hacia el pickup → trigger entra → HUD muestra "[E] Pick up Health Potion".
6. Press E → pickup desaparece + HUD muestra notif "+ Health Potion x1"
   (re-using `pushPickup` widget de F2H39).
7. Press **Tab** → HUD widget `inventory_panel` abre, muestra
   "Health Potion x1" con icon + tooltip.
8. Dev script Lua registra `inventory.on_use(callback)` que heal HP +50.
9. Click en "Health Potion" en el HUD → menú "Use" → callback dispara,
   HP sube, item desaparece del inventario (porque callback hace remove).
10. **Drag `iron_sword` al viewport** + drag `mysterious_key` también.
11. Walk al sword → press E → equipped (callback de on_use no remove
    porque es weapon).
12. Add NPC con DialogComponent. El dialog tiene una choice con
    `condition_lua = "inventory.has('items/mysterious_key.mooditem')"`.
13. Walk al NPC → press E (dialog) → la choice "Show key" aparece solo
    porque el player tiene la key. Otras choices no condicionadas
    siempre visibles.
14. Save proyecto → reload → todo persiste (inventory entries +
    ItemPickup entities en el mundo).

## NO entra en F2H52

- Crafting/combine items (deferred).
- Durability degradation (deferred).
- Steal/trade con NPCs (deferred).
- Examine 3D item (deferred — necesita 3D preview widget).
- Currency/vendor (deferred).
- Quick-use hotkeys (deferred).
- Inventory weight UI (peso/capacity visible al jugador) — el dev del
  juego implementa con Lua + `hud.text_label` si quiere. Motor expone
  `sum_stat` y listo.
- Auto-pickup (sin tecla E, levanta al pasar por encima) — convención
  del juego, no del motor. Dev lo hace en Lua.
- Animation de pickup (mano agarrando, motion del item al inventario) —
  visual polish, fuera de scope.

---

## Bloques de ejecución resumidos para el commit (cuando llegue a Bloque N)

| Bloque | Entregable |
|--------|-----------|
| A | Plan (este doc) — archivado en cierre |
| B | `ItemPickupComponent` + popup Add Component + `SavedItemPickup` |
| C | `ItemPickupSystem` (interact_prompt + tecla E + hook on_pickup) |
| D | Drag-drop viewport spawn pickup |
| E | Lua bindings tabla `inventory` (lecturas/mutaciones/sum_stat/spawn_pickup/player-implicit) |
| F | Lua hooks on_pickup/on_drop/on_use + `inventory.use` |
| G | DialogScriptHost extendido con `inventory.*` (gating choices) |
| H | HUD widget `inventory_panel` default (3 modes + Tab toggle + tooltip + drag) |
| I | HUD container split mode + `hud.open_container` |
| J | `inventory.set_renderer` override engine-grade |
| K | Tests Lua bindings + dialog gating + ItemPickupComponent roundtrip |
| L | Sample `inventory_demo.lua` |
| M | Tour visual con dev |
| N | Cierre docs + commit + tag |
