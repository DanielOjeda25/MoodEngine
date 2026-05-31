# PLAN F4H3 — Segunda arma + swap + viewmodel

**Estado:** PLAN, sin codigo todavia.
**Predecesor:** F4H2 (Bloque A + B cerrado, tag `v3.2.0-fase4-hito2-B`).
**Origen:** PLAN_FASE4.md §5 — siguiente hito post-F4H2. Scope reducido al loop "tengo varias armas, las cambio, las veo en mano". Ammo + HUD + pickups van a F4H4+.

---

## Norte

El jugador HOY tiene una escopeta y dispara contra el maniqui. F4H3 le suma una segunda arma (pistola), un sistema de **arsenal** con multiples slots, y un **viewmodel** (la arma visible en la mano del player). Mecanica que cambia:

- Tecla 1/2/3/4 o scroll wheel selecciona arma.
- Q (o Tab) vuelve a la arma anterior usada.
- El render muestra la arma equipada flotando en primera persona.
- Cambiar de arma NO pierde la municion del arma actual (per-slot ammo).

---

## Frontera engine ↔ juego

**Engine provee:**
- Sistema de slots multi-arma generico (N slots configurables).
- Sistema de swap (next/prev/last/specific).
- Render del viewmodel (segunda camara o pass dedicado para evitar clip con paredes).
- Bindings Lua para query/swap desde scripts.

**Juego provee:**
- `assets/weapons/pistola.moodweapon` (la pistola concreta).
- Mesh del viewmodel de la pistola + escopeta.
- Keybindings en UserSettings (`weapon_1=1`, `weapon_next=mouse_wheel_up`, etc).

---

## Decisiones (cerradas con AskUserQuestion)

- **D1 — Swap input: scroll wheel + Q/Tab last-used.** Half-Life/Apex style. Scroll cicla, Q vuelve a la anterior. Mas moderno que Doom 1/2/3 numerico, mas flow. Requiere extender InputActions con scroll delta (SDL_MOUSEWHEEL es event, no estado) y semantica "just pressed" (one-shot), no solo "is pressed" (polled). Numerica 1/2/3 queda opcional via keybindings extras.
- **D2 — Arsenal: slot fijo por categoria (4 slots).** Quake/Doom style. `WeaponComponent.slots[4]` con `{weaponAssetId, currentAmmo}` por slot + `activeSlot` + `lastActiveSlot`. Cada slot mapea a una tecla. Per-slot ammo: cambiar de arma preserva la municion del arma anterior.
- **D3 — Viewmodel: mesh en escena con camara separada.** Estandar Quake/HL/Source. `.moodweapon.viewmodelMesh` ya existe en el schema (campo opcional). Renderer especifico con near plane bajo evita clip con paredes.
- **D4 — Sandbox: todas las armas del catalogo desde el inicio.** `handleAddPlayer` rellena todos los slots con `enumerateWeapons()` (no solo el primero). Sirve para validar swap sin tener que armar pickups. F4H4 traera pickups reales y `handleAddPlayer` volvera a slot 0 + escopeta default.

---

## Schema cambios

### `.moodweapon`

Sin cambios al schema. F4H2 ya incluye `viewmodelMesh` + `viewmodelMaterial` (campos opcionales). F4H3 los empieza a usar.

### `WeaponComponent`

```cpp
struct WeaponSlot {
    u32 weaponAssetId = 0;
    int currentAmmo   = -1; // -1 = auto-init al equipar primera vez
};

struct WeaponComponent {
    static constexpr u32 k_maxSlots = 4;
    WeaponSlot slots[k_maxSlots];
    u32        activeSlot     = 0;  // 0..3
    u32        lastActiveSlot = 0;  // para Q/Tab

    // Transients (no serializar):
    f32  fireTimer   = 0.0f;
    f32  reloadTimer = 0.0f;
    bool firing      = false;
};
```

**Migracion**: F4H2 tenia `WeaponComponent { weaponAssetId, currentAmmo }`. F4H3 lo mueve a `slots[0]`. SceneLoader migra automatico al cargar maps pre-F4H3 (lee `weaponPath` + `currentAmmo` viejos y los aplica al slot 0).

### `SavedWeapon`

```cpp
struct SavedWeapon {
    struct SavedSlot {
        std::string weaponPath;
        int         currentAmmo = -1;
    };
    SavedSlot slots[4];
    u32       activeSlot = 0;
};
```

Migracion back-compat: si el JSON tiene `path`/`currentAmmo` plano (F4H2), se mapea a `slots[0]`.

### `UserSettings.input.keybindings` nuevos

```json
"weapon_next":  "mouse_wheel_up",
"weapon_prev":  "mouse_wheel_down",
"weapon_last":  "q",
"weapon_1":     "1",
"weapon_2":     "2",
"weapon_3":     "3",
"weapon_4":     "4"
```

Defaults agregados en `UserSettings::fillKeybindingDefaults`.

---

## Sub-tareas

### Sub-tarea 1 — Multi-slot WeaponComponent + serialization

- `Components_Gameplay.h`: refactor a `WeaponSlot` + `slots[4]` + `activeSlot` + `lastActiveSlot`.
- `WeaponSystem`: actualizar `fire/reload/equipWeapon/canFire/ammoLeft` para usar `slots[activeSlot]`.
- `SceneSerializer.h`: extender `SavedWeapon` con `slots[]`.
- `EntitySerializer.cpp` + `_Parse.cpp` + `SceneLoader.cpp`: roundtrip + migracion back-compat F4H2.
- Tests: extender `test_weapon_system.cpp` con multi-slot equip + activeSlot tracking. Extender `test_scene_serializer_gameplay.cpp` con roundtrip multi-slot.

### Sub-tarea 2 — Swap API

- `WeaponSystem::swapToSlot(scene, shooter, slot) → bool`.
- `WeaponSystem::swapNext(scene, shooter) → bool` (cicla 0→1→2→3→0).
- `WeaponSystem::swapPrev(scene, shooter)`.
- `WeaponSystem::swapLast(scene, shooter)` (toggle activeSlot ↔ lastActiveSlot).
- Validacion: skip slots vacios al swapNext/swapPrev (no se queda en `weaponAssetId=0`).
- Reset `fireTimer` + `reloadTimer` al hacer swap (interrumpe reload en curso, mejor feel).
- Tests: swap basico, swap a slot vacio, swap cicla, swapLast toggle, swapNext skip vacios.

### Sub-tarea 3 — InputActions extendido

- `InputActions::pollScrollDelta() → int` (consume eventos SDL_MOUSEWHEEL desde el ultimo poll).
- `InputActions::wasActionTriggered(action) → bool` (one-shot, true SOLO en el frame del press, no mientras se sostiene).
  - Mantiene `prevState` mapeado por action para detectar transicion released→pressed.
- Para scroll: `weapon_next`/`weapon_prev` resuelven a `Binding{type: MouseWheel, code: +1/-1}`; `isActionPressed` queda false (no se "sostiene" un scroll), `wasActionTriggered` devuelve true cuando hay delta del signo esperado.
- Lua bindings: `Input.was_action_triggered(action) → bool`.
- Tests: `test_input_keybindings.cpp` extendido con scroll binding parse + wasActionTriggered semantics.

### Sub-tarea 4 — Bridge swap en EditorApplication

- `EditorApplication::tickSystems` Play mode: ademas de fire/reload, chequea swap:
  - `weapon_next` → `Weapon::swapNext`
  - `weapon_prev` → `Weapon::swapPrev`
  - `weapon_last` → `Weapon::swapLast`
  - `weapon_1`/`weapon_2`/`weapon_3`/`weapon_4` → `Weapon::swapToSlot(N)`
- Mismo patron del fire bridge: tag `"player"` lookup.

### Sub-tarea 5 — Viewmodel render

**Opcion mas simple primero**: viewmodel como entity hija del player (Transform parent F3H27), con offset `(0.2, -0.15, -0.5)` (right, down, forward del player). Auto-sync de world position via parenting. Renderer normal con near plane reducido a `0.01f` (vs 0.1 default) para evitar clip.

- `ViewmodelComponent { u32 meshAssetId, u32 materialAssetId, glm::vec3 offset, glm::vec3 rotEulerDeg, glm::vec3 scale }` o reusar `MeshRendererComponent` + tag especial `__viewmodel`.
- `ViewmodelSystem::tickSystem(scene, dt, camera)`:
  - Encuentra entity con tag `"__viewmodel"` (engine-generic).
  - Si el `WeaponComponent` del player cambio `activeSlot`, swap del mesh del viewmodel al `weaponAssetId.viewmodelMesh`.
  - Sync `Transform.position = camera.pos + camera.right*0.2 + camera.up*-0.15 + camera.forward*0.5`.
  - Sync `Transform.rotation` a la camara para que el viewmodel rota con la vista.
- `EditorApplication::handleAddPlayer` crea entity hija con tag `"__viewmodel"` + `ViewmodelComponent` vacio.

**Limitacion conocida**: si el player camara mira directo a una pared cercana, el viewmodel clip-thru. Solucion proper requiere render pass dedicado con depth buffer aparte (HL/Source style). Agendizado a **F4H3.1** si emerge.

### Sub-tarea 6 — Pistola demo data

- `assets/weapons/pistola.moodweapon`:
  - displayName: "Pistola"
  - category: hitscan
  - damage: 25.0
  - range: 80.0
  - pellets: 1 (sin dispersion)
  - spreadDeg: 0.5 (minima)
  - fireRatePerSec: 5.0 (rapida)
  - magazineSize: 12
  - reloadTimeSec: 1.2
- `assets/weapons/viewmodels/`: placeholder meshes (cubo escalado o cilindro) hasta que haya art.

### Sub-tarea 7 — Inspector Weapon multi-slot UI

- `InspectorPanel_Weapon.cpp`: refactor del combo unico a 4 slots editables.
- Cada slot tiene: combo `(sin arma)` + catalogo, slider Ammo, indicador "ACTIVE" si es el slot activo.
- Boton "Set active" por slot (para debug en editor).

### Sub-tarea 8 — handleAddPlayer auto-rellena slots

- Sandbox D4: rellena `slots[0..N-1]` con `enumerateWeapons()` hasta `min(catalogo, k_maxSlots)`. `activeSlot=0`.

### Sub-tarea 9 — Tests + docs

- Suite full verde. ~15-20 tests nuevos esperables (multi-slot serialization + swap mechanics + input bindings nuevos + viewmodel sync).
- Commit con "Chequear:" bullets (entrar al editor, crear Player → debería tener 2+ armas en los slots, scroll cambia arma, ver viewmodel cambiar, click dispara la arma activa, recarga la arma activa).
- ESTADO_ACTUAL / HITOS / DECISIONS actualizados.
- Tag `v3.3.0-fase4-hito3`.

---

## Metricas de exito

- 2+ armas en el catalogo (`shotgun.moodweapon` + `pistola.moodweapon`).
- `WeaponComponent.slots[4]` con per-slot ammo persistente.
- Swap funciona via scroll + Q.
- Viewmodel visible y cambia al hacer swap.
- 0 strings hardcoded del juego en `src/engine/`.
- LOC: WeaponSystem.cpp < 500 (refactor + swap APIs). ViewmodelSystem.cpp < 200.
- Suite verde.

---

## Backlog (NO entra a F4H3)

- **F4H3.1** — Viewmodel render pass dedicado (HL/Source style con depth buffer aparte) si el clip-thru molesta.
- **F4H4** — HUD de combate (salud + ammo + arma activa via GameOverlay).
- **F4H5** — Pickups de armas en el suelo (mesh + trigger + overlap script).
- **F4H6** — Recoil + camera shake al disparar.
- **F4H7** — Animaciones de reload + swap del viewmodel.

---

## Riesgos

- **Migracion F4H2 → F4H3**: mapas viejos con `WeaponComponent.weaponAssetId` plano deben cargar al `slots[0]`. Si rompe back-compat, el dev pierde el progreso. Mitigacion: SceneLoader fallback exhaustivo + test roundtrip de mapa F4H2.
- **Scroll wheel semantica**: SDL emite eventos discretos; si el frame loop no los polea, se pierden swaps. Mitigacion: `InputActions::pollScrollDelta` se llama dentro del event loop ANTES del bridge.
- **Viewmodel clip-thru**: agendizado a F4H3.1; near plane reducido es palliativa.
- **`activeSlot` apunta a slot vacio**: validacion en swap APIs + display "(sin arma)" en HUD.

---

## Cierre — F4H3 (2026-05-30, tag `v3.3.0-fase4-hito3`)

**Sub-tareas entregadas (9/9):**

1. ✅ **Multi-slot `WeaponComponent`** + serialization. `WeaponSlot{weaponAssetId, currentAmmo}` × 4 + `activeSlot` + `lastActiveSlot`. SavedWeapon migrado a `slots[]` con back-compat F4H2 (parser detecta formato viejo `path`/`currentAmmo` plano → mapea a `slots[0]`).
2. ✅ **Swap API** `swapToSlot/swapNext/swapPrev/swapLast` en `Weapon::`. Cycle skipea slots vacios. Swap resetea `fireTimer/reloadTimer`. `applySwap` helper centralizado.
3. ✅ **InputActions extendido**: `BindingType::MouseWheel` con code +1/-1. `resolveBinding("mouse_wheel_up/down")`. `wasActionTriggered` con semantica one-shot (transicion released→pressed para keys/mouse, delta-del-frame para wheel). `notifyScrollEvent` + `pollScrollDelta` + `endFrame` para state lifecycle. 7 keybindings nuevos defaults (`weapon_next/prev/last/1..4`).
4. ✅ **Bridge swap** en `EditorApplication::tickSystems` Play mode: `wasActionTriggered` por cada accion + dispatch a `Weapon::swap*`. `endFrame()` al final para capturar prevPressed. `notifyScrollEvent` desde `processEvents()` al recibir `SDL_MOUSEWHEEL`.
5. ✅ **Viewmodel render**: `ViewmodelComponent{offsetCamSpace, extraRotEulerDeg, scale, syncMeshOnSwap, lastSeenWeaponId}`. `Weapon::tickViewmodel(scene, camPos, camForward, camUp, assets)` sync transform a la camara + swap mesh al cambiar slot (cubo missingMeshId fallback si no hay `viewmodelMesh`). Render usa MeshRenderer normal. F4H3.1 puede agregar render pass dedicado si emerge clip-thru.
6. ✅ **Demo data**: `assets/weapons/pistola.moodweapon` (25 dmg / 80 m / 1 pellet / 0.5° spread / 5 rate / 12 mag / 1.2 reload).
7. ✅ **Inspector_Weapon multi-slot**: 4 tabs (uno por slot). Marker `*` en tab del slot activo. Boton "Activar este slot" en tabs no-activos. Cada tab muestra combo `.moodweapon` + spec read-only + slider Ammo del slot correspondiente. 2 i18n keys nuevas (`active_slot`, `set_active`).
8. ✅ **handleAddPlayer auto-rellena** los 4 slots con `enumerateWeapons()` (hasta k_maxSlots). `activeSlot=0`. Tambien crea entity `"__viewmodel"` hija con cubo placeholder + ViewmodelComponent (la tickViewmodel adopta el mesh del arma al primer frame).

**Tests** (sobre `mood_tests` previo): nuevos en `test_weapon_system.cpp` (multi-slot equipWeaponInSlot + swap APIs + fire usa slot activo) + en `test_input_keybindings.cpp` (scroll wheel + digitos + defaults F4H3 + scroll delta state). Tests del viewmodel quedan en validacion manual editor (depende de SceneRenderer + camara).

**Ajustes reactivos durante implementacion:**

- **R1 — Helper `activeSlotOf` con clamp defensivo.** `wc.activeSlot >= k_maxSlots` se clampea a 0 en lugar de UB. Cubre escenarios de serializacion corrupta o maps post-bump del `k_maxSlots`.
- **R2 — `equipWeapon` opera sobre el slot activo** (back-compat con F4H2 API). Nuevo `equipWeaponInSlot(slot, path)` para init explicito. Asi los callers viejos siguen funcionando (no need to update Lua bindings, SceneLoader, etc.).
- **R3 — Sandbox D4 cambio.** El plan decia que handleAddPlayer rellena "primero del catalogo". El cierre rellena TODOS los slots (hasta 4) — mejor para validar swap.
- **R4 — Viewmodel via componente + system free function**, NO ECS system class. Mismo patron Health/Weapon. `tickViewmodel(scene, camera...)` desde EditorApplication_Run.
- **R5 — Viewmodel mesh fallback al cubo si `spec.viewmodelMesh` empty**, asi siempre hay algo visible. F4H3.1 puede agregar mesh art real para shotgun/pistola.

**Backlog post-F4H3:**

- **F4H3.1** — Viewmodel render pass dedicado (HL/Source style) si clip-thru con paredes molesta. Mesh art real para escopeta + pistola. Animacion idle/walk del viewmodel.
- **F4H4** — HUD de combate (salud + ammo + arma activa) sobre GameOverlay.
- **F4H5** — Pickups de armas en el suelo (mesh + trigger + overlap script). Refactor de handleAddPlayer a "solo arma default + resto via pickup".
- **F4H6** — Recoil + camera shake al disparar.
- **F4H7** — Animaciones de reload + swap del viewmodel.
