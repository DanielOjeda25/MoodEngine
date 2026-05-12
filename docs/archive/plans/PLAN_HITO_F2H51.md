# PLAN HITO F2H51 — Inventario (editor side: ItemAsset + Item Browser + InventoryComponent)

> **Estado**: DRAFT pendiente aprobación del dev (2026-05-11).
> **Tag previsto**: `v1.39.0-fase2-hito51`.
> **Origen**: Bloque 1 del plan `PLAN_SUBFASE_2_5.md` (Sub-fase 2.5
> Sistemas de juego). Primer pilar del sistema de inventario. Se
> enfoca en EL LADO EDITOR + DATA: permitir al dev definir items
> visualmente, persistirlos, y attachear `InventoryComponent` a
> entidades. El runtime (pickup/drop integrado + HUD widget + Lua
> bindings) queda para **F2H52**.

## Filosofía

F2H51 entrega la **autoría + el contenedor de datos**, no el
gameplay. Cuando cierre F2H51, el dev podrá:

- Crear un `.mooditem` desde el Item Browser (id estable + nombre +
  descripción + icono + tags + stats key-value libre + stack rules).
- Listar/buscar/filtrar items existentes en el Item Browser con
  iconos + categorías derivadas de tags.
- Adjuntar `InventoryComponent` a cualquier entidad (player, NPC,
  cofre, vendor) con uno de los 3 modos (lista plana / grid 2D /
  equipment slots).
- Editar el contenido del inventario desde el Inspector (agregar
  items por id + quantity + slot opcional).
- Persistir InventoryComponent en `.moodmap` (roundtrip JSON).
- Tener tests del schema + serialización + state básico del
  componente (add/remove/count/has en memoria).

Lo que NO podrá hacer todavía: pickear items del mundo, drop al
suelo, ver inventario en HUD, controlar inventario desde Lua. Eso es
**F2H52** (ItemPickupComponent + HUD widget + Lua bindings).
Mantengo el split editor/runtime porque:

1. Es checkpoint natural de validación — el dev autoriza el schema
   antes que el runtime quede atado a él (idéntico a F2H47→F2H48).
2. F2H52 dependerá del estado funcional pero también dependerá de
   integración con `TriggerComponent` + `GameOverlay` + `ScriptHost`
   que merecen su propio testing dedicado.
3. Hitos mantenibles: F2H51 ~10-12h, F2H52 ~8-10h. Total Inventario
   ~18-22h. Match con scope B Pro Tools.

**Principios engine-grade aplicados (no-negociables del marco
estratégico):**

1. **Data-driven**: items definidos en `.mooditem` JSON, editables
   100% visualmente. Cero C++ para crear contenido.
2. **Sin semántica hardcodeada de gameplay**: el schema NO conoce
   "weapon", "potion", "armor" específicos. Tags + stats key-value
   libre. El dev del juego define su propia ontología.
3. **Editor visual real**: Item Browser estilo Asset Browser con
   grid de cards + property editor. JSON crudo es output, no input.
4. **Composabilidad**: `InventoryComponent` attachable a CUALQUIER
   entidad — player, NPC, container, vendor, drop pile. Sin
   distinción semántica en el motor.
5. **State vs rendering separados**: schema + container state en
   `engine/inventory/` puro (testeable sin ImGui); UI en
   `editor/panels/inventory/`.

## Scope (decisiones a confirmar con el dev al arrancar)

### `.mooditem` schema v1

```jsonc
{
  "_version": 1,
  "id": "iron_sword",                  // ID estable; matchea filename sin extension
  "name_key": "items.iron_sword.name", // i18n key del nombre visible
  "name_literal": "",                  // fallback literal si no hay i18n; escape hatch
  "description_key": "items.iron_sword.desc",
  "description_literal": "",
  "icon_path": "icons/items/iron_sword.png", // path lógico (VFS) a textura del icono
  "model_path": "",                    // path opcional al .glb/.fbx del modelo 3D (vacío = no model)
  "tags": ["weapon", "metal", "common"], // strings libres; categoría derivada del primer tag
  "stats": {                           // key-value libre — el motor no interpreta
    "damage": 12.5,
    "weight": 3.0,
    "durability_max": 100.0
  },
  "stack": {
    "stackable": false,                // si true, varias unidades comparten slot
    "max_stack": 1                     // cap del stack (sólo si stackable=true)
  },
  "slot_size": {                       // tamaño para layout grid 2D Resident Evil
    "width": 1,                        // cells horizontal
    "height": 2                        // cells vertical
  }
}
```

- **`id`** debe matchear el filename del `.mooditem` (sin extension).
  El editor garantiza la invariante al crear/renombrar.
- **`tags`** son strings libres. La primera entrada define la
  categoría visible en el browser. Convención (no enforcement): los
  3 tags estándar `equipment_slot/<slot>` (head/torso/etc),
  `consumable`, `quest_item` los puede usar el dev pero el motor no
  los interpreta.
- **`slot_size`** se ignora si el layout del InventoryComponent es
  lista plana o equipment slots. Solo el grid 2D lo consume.

### Asset lifecycle

- **Path en filesystem**: `<project_root>/assets/items/<id>.mooditem`.
- **AssetManager** gana loader nuevo: `am.loadItem("items/iron_sword.mooditem")`.
- **Cache + slot 0 vacío** (mismo patrón que `loadDialog` F2H48).
- **Save**: el Editor escribe directo al filesystem. Auto-save OFF
  en v1 — botón "Save" explícito en el Inspector del item activo
  (igual que Dialog Editor F2H47).
- **Hot reload**: si el `.mooditem` cambia en disco, el Item Browser
  refleja el cambio (refresh manual con botón en v1; auto en backlog).

### Panels nuevos

#### `ItemBrowserPanel` (categoría Assets)

Lista todos los `.mooditem` del proyecto activo con UX tipo Asset
Browser pero específico:

- **Grid de cards** (cuadrículas ~96x96px) con icono + nombre + categoría.
- **Filtros** arriba: search por nombre + dropdown por tag.
- **Click** sobre una card → selecciona el item + abre property editor.
- **Right-click**: rename / delete / duplicate (copia con id nuevo).
- **Botón `+ New Item`** (modal con id input → crea `.mooditem` con
  defaults + abre property editor).
- **Refresh manual** (botón) que rescanea `assets/items/`.

**Layout default en workspace "Gameplay" (nuevo)**: ver bloque G.

#### `ItemPropertyEditorPanel` (categoría Assets)

Inspector contextual del item seleccionado. Muestra TODOS los
campos del schema:

- **Identity**: id (readonly post-creación), name + description con
  toggle key/literal (mismo patrón que F2H47).
- **Visual**: icon_path (drop target acepta payload `MOOD_TEXTURE`)
  + preview thumbnail. Model path opcional (drop target acepta
  `MOOD_MESH_ASSET`) + preview 3D pequeño (placeholder hasta que
  Bloque 0.2 del PLAN_SUBFASE_2_5 esté hecho — v1 sólo muestra el
  path text).
- **Categorization**: tags como chips agregables/removibles (input
  + Enter agrega; X borra).
- **Stats**: tabla key-value editable. Add/remove rows. Valores son
  floats. Keys son strings libres.
- **Stack**: checkbox `stackable` + spinner `max_stack` (greyed si
  no stackable).
- **Slot size**: 2 spinners width/height (greyed si layout no es grid).
- **Botón Save** explícito.

#### `InventoryInspectorSection` (extiende InspectorPanel existente)

NO es panel nuevo — es sección dentro del `InspectorPanel` cuando
una entity tiene `InventoryComponent`. Mismo patrón que las
secciones existentes (Transform / MeshRenderer / Animator).

- **Layout mode dropdown**: `flat_list` / `grid_2d` / `equipment_slots`.
- **Capacity** (depende del modo):
  - `flat_list`: spinner `max_items` (cap del número total de stacks distintos).
  - `grid_2d`: spinners `grid_width` x `grid_height` (cells).
  - `equipment_slots`: lista editable de slot names (ej. "head",
    "torso", "weapon_main", "weapon_off") con tag opcional por slot
    (sólo items con ese tag encajan).
- **Contenido inicial** (lista editable de `{item_id, quantity, slot_pos}`):
  - Add row: dropdown de item_ids existentes + spinner quantity + (slot opcional según mode).
  - Remove row: botón X por fila.
  - Drop target en la sección: aceptar payload `MOOD_ITEM_ASSET`
    desde Item Browser → agrega row con qty=1.

**Caso de uso típico**: el dev attachea InventoryComponent al
chest entity con 3 items pre-cargados (loot inicial); attachea
otra InventoryComponent al player con grid 4x6 vacío.

### `engine/inventory/` modulo nuevo

- `engine/inventory/ItemAsset.h/cpp`: struct con todos los campos
  del schema + `toJson()`/`fromJson()`. NO depende de ImGui.
- `engine/inventory/InventoryState.h/cpp`: contenedor del estado
  runtime de un InventoryComponent. API:
  ```cpp
  struct InventoryEntry { ItemAssetId itemId; int quantity; int slotIndex; };
  class InventoryState {
    InventoryLayoutMode mode;          // flat_list / grid_2d / equipment_slots
    LayoutConfig config;               // capacity / grid / slot names
    std::vector<InventoryEntry> entries;

    bool add(ItemAssetId id, int qty, AssetManager& am);
    bool remove(ItemAssetId id, int qty);
    bool has(ItemAssetId id, int minQty = 1) const;
    int count(ItemAssetId id) const;
    // Para grid_2d / equipment_slots:
    bool placeAt(ItemAssetId id, int qty, int slotIndex, AssetManager& am);
    bool moveSlot(int fromSlot, int toSlot);
  };
  ```
  Lógica pura — testeable sin ImGui. Respeta `stackable` /
  `max_stack` / `slot_size` (consultando `AssetManager::getItem`).

### `InventoryComponent` (Components.h)

```cpp
struct InventoryComponent {
  Inventory::InventoryState state;     // contiene mode + config + entries
  // (sin más fields v1 — todo vive en state)
};
```

Agregable desde popup "Add Component" categoría Logic (igual que
DialogComponent F2H48).

### Persistencia `.moodmap`

Extender `SavedEntity` con `std::optional<SavedInventory>`:

```cpp
struct SavedInventoryEntry { std::string itemPath; int quantity; int slotIndex; };
struct SavedInventoryEquipmentSlot { std::string name; std::string tagFilter; };
struct SavedInventory {
  std::string layoutMode;              // "flat_list" / "grid_2d" / "equipment_slots"
  int maxItems = 0;                    // para flat_list
  int gridWidth = 0; int gridHeight = 0;// para grid_2d
  std::vector<SavedInventoryEquipmentSlot> equipmentSlots; // para equipment_slots
  std::vector<SavedInventoryEntry> entries;
};
```

**Patrón paths-no-ids** (mismo que F2H50 para AnimatorComponent
externalClips): el `itemPath` es path lógico
(`assets/items/iron_sword.mooditem`), no `ItemAssetId`. SceneLoader
resuelve via `loadItem`. Bump aditivo del schema — saves
pre-F2H51 se cargan con InventoryComponent ausente (no auto-add).

### Tests (Bloque H)

- `tests/test_item_asset.cpp`: schema roundtrip + missing-field
  defaults + bumped-version rejection + stats key-value preservation +
  id-matches-filename invariant.
- `tests/test_inventory_state.cpp`: add/remove/has/count en flat_list;
  stackable + max_stack overflow; placeAt + moveSlot en grid_2d;
  equipment_slots respeta tag filter; capacity overflow rechazado.
- Extender `tests/test_scene_serializer.cpp`: roundtrip
  `InventoryComponent` con cada uno de los 3 layout modes.

### Bloques de ejecución

| # | Bloque | Estim. | Notas |
|---|--------|--------|-------|
| A | Plan (este doc) | 30 min | Aprobar con dev antes de B |
| B | `ItemAsset.h/cpp` schema + JSON serialization | ~1.5h | Struct + toJson/fromJson + tests del asset |
| C | `AssetManager::loadItem` + VFS path resolution + slot 0 | ~1h | Mismo patrón que loadDialog F2H48 |
| D | `InventoryState.h/cpp` lógica pura del contenedor | ~2h | 3 layout modes + add/remove/has/count/placeAt; tests dedicados |
| E | `InventoryComponent` en Components.h + popup Add Component | ~30 min | Categoría Logic, paridad DialogComponent |
| F | `ItemBrowserPanel` — grid de cards + filtros + new/rename/delete/duplicate | ~2.5h | Asset browser específico para items |
| G | `ItemPropertyEditorPanel` — editor contextual del item activo | ~2h | Identity/visual/tags/stats/stack/slot_size + Save explícito |
| H | `InventoryInspectorSection` en InspectorPanel — mode dropdown + capacity + entries editables + drop target | ~2h | Sección nueva en InspectorPanel existente |
| I | Persistencia `.moodmap`: `SavedInventory` + EntitySerializer write/parse + SceneLoader apply | ~1.5h | Bump aditivo, fallback a no-component si ausente |
| J | Workspace "Gameplay" nuevo + dock layout (Item Browser left + Property Editor right + Viewport center) + Welcome modal sin demo nuevo (post-F2H52) | ~1h | Mismo patrón que workspace "Narrativa" F2H46 |
| K | Tests del asset + state + serializer + manual visual tour con dev | ~1.5h | `test_item_asset.cpp` + `test_inventory_state.cpp` + extensión scene serializer |
| L | Cierre: docs (HITOS / ESTADO / DECISIONS / PENDIENTES / PLAN_SUBFASE_2_5) + commit + tag + push | ~1h | Patrón estándar |

**Total estimado**: ~17h. Hito largo (más que F2H47).

## Decisiones técnicas abiertas

### Default layout mode del InventoryComponent al crear

El PLAN_SUBFASE_2_5 sección 1.3 lista 3 modos pero no decide cuál
es default cuando el dev hace "Add Component → Inventory":

- (a) **`flat_list`** con max_items=20. Más universal — se entiende
  sin contexto del género del juego.
- (b) **`grid_2d`** con 4x6. Mejor UX visual pero asume género RPG.
- (c) **`equipment_slots`** con slots ["weapon", "head", "torso"]
  pre-cargados. Asume sub-género específico.

**Recomendación v1**: (a) `flat_list` max_items=20. Coincide con la
propuesta del PLAN_SUBFASE_2_5 sección 1.3 ("más universal, se
entiende sin contexto"). El dev cambia con dropdown si quiere otro.

### Items pre-cargados al crear `.mooditem` desde Item Browser

Cuando el dev hace `+ New Item`, ¿con qué defaults se inicializa?

- (a) **Vacío total** (todo strings empty, no tags, no stats).
- (b) **Defaults sensatos** (name_literal = id, icon_path apunta
  a un placeholder `icons/items/placeholder.png` que se shippea con
  el motor, stack stackable=false max_stack=1).

**Recomendación v1**: (b) defaults sensatos. El placeholder de icono
evita que items sin icono salgan magenta. El dev sobrescribe lo que
necesite. Shippeamos `assets/icons/items/placeholder.png` (un cubo
gris con `?` blanco) ~ 32x32 px.

### Drop target en el InventoryInspectorSection

Cuando el dev arrastra un item desde el Item Browser a la sección
Inventory del Inspector de una entidad:

- (a) **Agrega entry con qty=1** y deja al dev ajustar cantidad.
- (b) **Abre modal** preguntando cantidad y slot opcional.

**Recomendación v1**: (a) qty=1, dev ajusta. Modal interrumpe el
flow para casos simples (mayoría). El dev cambia qty con el spinner
de la row después.

### Icon path: textura cargada o thumbnail file-based

Para mostrar el icon del item en cards del Browser:

- (a) **Cargar como `Texture2D` via AssetManager** (mismo pipeline
  que materials F2H42). Garantiza consistencia + cache. **Costo**:
  cada icon ocupa un slot del cache de texturas.
- (b) **Leer raw del filesystem on-demand** y emit ImGui::ImageTexture
  con descriptor temporal. Menos memoria persistente pero más I/O.

**Recomendación v1**: (a) — reusar pipeline existente. El dev espera
que cambios de icon se reflejen consistentemente y el cache ya
maneja invalidación. Items reales de un juego serían ~50-200, no es
un problema de memoria.

### Sample items para validación visual

¿Shippeamos `.mooditem` samples en el repo (igual que `.mooddialog`
demo)?

**Recomendación v1**: SÍ — 3 items sample en `assets/items/`:
- `iron_sword.mooditem` (weapon, stats damage)
- `health_potion.mooditem` (consumable, stackable, max_stack=10)
- `mysterious_key.mooditem` (quest_item, no stats)

Más placeholder icon. Total ~5 KB de assets. Cargan automáticamente
desde el Item Browser al abrir cualquier proyecto que tenga
`assets/items/`. Validan que el browser + property editor + carga
funcionan end-to-end.

### Workspace nuevo "Gameplay" vs reusar "Narrativa"

¿El Item Browser vive en su propio workspace o en uno existente?

- (a) Workspace nuevo **"Gameplay"** con layout dedicado: Viewport 3D
  + Item Browser + Item Property Editor + InspectorPanel.
- (b) Reusar workspace "Narrativa" agregando un tab al Dialog Browser.
- (c) Default workspace (sin layout específico) — el dev abre el
  Item Browser desde menú.

**Recomendación v1**: (a) workspace nuevo "Gameplay". Inventario y
quests (F2H53+) comparten contexto — verlos juntos tiene sentido.
"Narrativa" sigue siendo solo Dialog. Tres workspaces totales
post-F2H51: Default / Narrativa / Gameplay.

## Diferidos a hitos futuros

- **F2H52 — Inventory runtime**: ItemPickupComponent (entidad
  pickable en el mundo) + integración con TriggerComponent +
  interact_prompt del HUD + DialogChoice gating por items + HUD
  widget inventory default (open/close con Tab + grid visual +
  drag-drop entre slots + tooltip) + Lua bindings (`inventory.add`,
  `remove`, `has`, `count` + hooks `on_pickup`, `on_drop`).
- **F2H53 — Quest editor** (Bloque 3 del PLAN_SUBFASE_2_5): node-graph
  reusando framework F2H46/47 para quest flowchart.
- **3D preview widget reusable** (Bloque 0.2 del PLAN_SUBFASE_2_5):
  render-to-texture orbital camera. v1 del ItemPropertyEditor sólo
  muestra el path text del modelo; el preview real cuando emerja
  hito dedicado (compartido con Material Editor pro futuro).
- **Equipment effects pipeline**: cuando se equipa un item con tag
  `equipment_slot/*`, aplicar sus `stats` como modificadores al
  StatsComponent del owner. Requiere StatsComponent + sistema de
  modifiers que es Bloque 4.1 del plan — diferido.
- **Crafting / smelting / merging**: combinaciones de items para
  crear otros items. Fuera de scope Sub-fase 2.5 entera.
- **Currency/economy**: items con stat "value" para vendor systems.
  Requiere NPC vendor — diferido a sub-fase futura.
- **Hot reload del Item Browser**: refresh automático cuando un
  `.mooditem` cambia en disco vía filesystem watcher. v1 es manual.
- **Localización de tags**: si los tags se muestran al jugador
  (categoría en HUD inventory), tendrían que ser i18n keys. v1
  asume tags son sólo para el editor — el HUD usa name_key.
- **Bulk operations en Item Browser** (delete múltiples, batch
  rename, batch tag-add): YAGNI para v1.

## Riesgos y tradeoffs

- **Riesgo: 3 layout modes complican el state + el inspector**.
  flat_list + grid_2d + equipment_slots son 3 ramas distintas en
  add/remove/placeAt + 3 UIs distintas en el Inspector.
  **Mitigación**: separar la lógica en `InventoryState` con polimorfismo
  sobre `LayoutMode` enum (switch en cada operación, encapsulado).
  Tests dedicados por modo. Si emerge complejidad real, refactor a
  variant + visitor.

- **Riesgo: serialización del state con paths-no-ids puede romperse**
  si el dev renombra un `.mooditem` entre save y load.
  **Mitigación**: mismo patrón ya validado con F2H50 (paths
  logicales en lugar de IDs no estables). SceneLoader emite warning
  si `loadItem(path)` falla y deja la entry con `itemId=0` (slot
  vacío). El dev ve el warning + corrige.

- **Riesgo: Inspector con N items + N stats por item = inspector
  visualmente abrumador**.
  **Mitigación**: tabla de stats colapsable + entries del inventory
  con scrollbar interno a partir de N>10. Mismo patrón que F2H47
  con choices.

- **Tradeoff: schema v1 sin "category" explícito**. Categoría se
  deriva del primer tag. Funciona pero acopla orden de tags a
  display. Aceptable: el dev del juego decide el orden; el editor
  garantiza que el primer tag siempre exista.

- **Tradeoff: 3 modos en v1 vs más simple "solo flat_list"**.
  Implementar solo flat_list reduce ~3-4h de scope. Pero el
  PLAN_SUBFASE_2_5 explícitamente lista los 3 modos como
  fundacional. Reducir ahora sería deuda inmediata cuando F2H52
  HUD widget necesite el grid 2D para "Resident Evil style".
  Mantengo los 3.

- **Tradeoff: Save explícito vs auto-save**. Igual decisión que
  F2H47: explícito. v2 puede agregar auto-save.

## Validación

- Build limpio Debug + Release (CMake + tests).
- Suite completa (tests previos + nuevos de ItemAsset + InventoryState +
  scene serializer extendido) verde.
- **Tour visual con dev en workspace "Gameplay"** (cada paso debe
  funcionar de punta a punta):
  1. Welcome modal → "Nuevo proyecto" → workspace "Gameplay" se
     activa por default.
  2. Item Browser muestra los 3 items sample (`iron_sword`,
     `health_potion`, `mysterious_key`) con icons + categorías.
  3. Click en `iron_sword` → Property Editor muestra todos los
     campos editables. Cambiar `damage` de 12.5 a 15 → Save →
     reload del editor → cambio persiste.
  4. `+ New Item` → modal → `magic_wand` → archivo creado en
     `assets/items/magic_wand.mooditem` con defaults. Aparece en el
     browser.
  5. Right-click sobre `iron_sword` → Duplicate → `iron_sword_2`
     creado con misma data + id distinto.
  6. Entity nueva en escena → Inspector → Add Component →
     Inventory. Dropdown layout muestra los 3 modos. Default
     `flat_list` max_items=20.
  7. Arrastrar `health_potion` desde Item Browser al Inspector
     Inventory section → entry agregada con qty=1. Cambiar qty a 5
     con el spinner.
  8. Save scene (`.moodmap`) → close → reopen → InventoryComponent
     persiste con 5 potions + layout_mode flat_list.
  9. Cambiar layout a `grid_2d` 4x6 → entry de potions se preserva
     (queda en slot 0). Cambiar a `equipment_slots` con ["weapon",
     "head"] → entries no-equipables se quedan en buffer
     unassigned (warning visible).

## NO entra en F2H51

- **Runtime del inventario**: pickup/drop entities, HUD widget
  visible, Lua bindings, interact_prompt con items, gating de
  dialog choices por items. Todo eso es F2H52.
- **3D preview del modelo** en el Property Editor — solo muestra
  path text. Preview real cuando Bloque 0.2 del PLAN_SUBFASE_2_5
  esté implementado.
- **StatsComponent** y aplicación de modifiers de items equipados.
  Bloque 4.1 del plan — diferido.
- **Currency / vendor / crafting** — fuera de scope Sub-fase 2.5.
- **Hot reload por filesystem watcher** — manual en v1.
- **Localización de tags** — tags son editor-facing en v1.
- **Bulk operations** — borrar N items a la vez, batch rename, etc.
- **Editor undo/redo de operaciones de inventario** — el undo
  histórico es nice-to-have pero no esencial; el save explícito
  cubre el caso del "deshacer" de facto.

---

## Bloques de ejecución resumidos para el commit (cuando llegue a Bloque L)

| Bloque | Entregable |
|--------|-----------|
| A | Plan (este doc) — archivado en cierre |
| B | `ItemAsset.h/cpp` (puro, sin ImGui) |
| C | `AssetManager::loadItem` + VFS path resolution + slot 0 |
| D | `InventoryState.h/cpp` (puro, 3 layout modes) |
| E | `InventoryComponent` + popup Add Component |
| F | `ItemBrowserPanel` (categoría Assets) |
| G | `ItemPropertyEditorPanel` (categoría Assets) |
| H | `InventoryInspectorSection` en InspectorPanel |
| I | Persistencia `.moodmap` (SavedInventory + write/parse/apply) |
| J | Workspace "Gameplay" + dock layout |
| K | Tests del asset + state + serializer + tour visual |
| L | Cierre: docs + commit + tag + push |
