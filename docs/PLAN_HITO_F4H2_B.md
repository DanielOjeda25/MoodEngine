# PLAN F4H2 Bloque B — Wiring final: input + UX + AssetBrowser + audio

**Estado:** ✅ **CERRADO** (tag `v3.2.0-fase4-hito2-B`, 2026-05-29).
**Predecesor:** F4H2 Bloque A (WeaponSpec asset + WeaponComponent + WeaponSystem + Lua bindings + Inspector).
**Origen:** scope split decidido al cerrar Bloque A — la infraestructura data-driven cerro completa, pero el e2e "click → mata maniqui" necesita el input bridge + UX. Mantener bloque A solo seria un commit gigante.

---

## Entregado

**Sub-tareas del plan original** (1-4 completas; Sub-5 demos extras al backlog):

- ✅ **Sub-1 — Input bridge data-driven**: `UserSettings.input.keybindings` con defaults `fire=mouse_left`, `reload=r`. `InputActions::isActionPressed(action)` resuelve string → SDL_SCANCODE/SDL_BUTTON. Lua binding `Input.is_action_pressed(action)`. Bridge en `EditorApplication::tickSystems` Play mode: encuentra entity con tag `"player"`, setea `wc.firing` y llama `Weapon::fire/reload` con `m_playCamera` origin/dir. **9 tests** en `test_input_keybindings.cpp` (defaults / roundtrip / mouse / letras / special keys / case-insensitive / trim / unknown).
- ✅ **Sub-2 — AudioDevice cascade**: `setupLuaBindings(audio)` + `ScriptSystem::update(audio)` + 2 callers actualizados (`EditorApplication_Run` + `PlayerApplication_Frame`). El audio real llega a `setupWeaponBindings` (placeholder nullptr eliminado).
- ✅ **Sub-3 — AssetBrowser tab "Armas"**: scan plano de `assets/weapons/*.moodweapon` con drag payload `MOOD_WEAPON_ASSET` (queda funcional aunque drag-drop al Inspector se desactivó por el rediseño UX — el tab sigue siendo el catálogo visual de armas).
- ✅ **Sub-4 — Crear Entidad → Gameplay → Player**: card auto-tagueada `"player"` + WeaponComponent con la **primera arma del catálogo auto-asignada** (orden alfa case-insensitive). `enumerateWeapons(rescanFromDisk=true)` helper en AssetManager (escanea filesystem + carga lo que falte en cache). Guardia anti-duplicado: si ya hay un `"player"` aborta con warn.

**5 fixes adicionales aplicados en el mismo bloque** (autorizados por el dev: *"arregla todo ahora, AHORA EN ESTE HITO"*):

- ✅ **Fix #1 — Auto-tag "player"**: la card crea entity con tag literal `"player"` (no `"Arma_N"` legacy), Inspector lo refleja.
- ✅ **Fix #2 — Brushes con colisión por defecto (convención Hammer)**: TODOS los 11 brush handlers + el block tool + el polygon-draw pasan ahora por `spawnBrushEntity` que agrega `RigidBodyComponent` Static Box halfExtents derivado del scale. Resuelve "brush atravesable en Play" reportado en F3H29 backlog.
- ✅ **Fix #3 — Botón "Restablecer" en Transform**: `EditTransformResetCommand` atómico (position/rotation/scale/useQuaternion en un solo Ctrl+Z).
- ✅ **Fix #4 — Popup "Add Component" posicionado**: `SetNextWindowPos` capturado antes de `OpenPopup` (bottom-left + 2px gap). Antes flippeaba arriba tapando el botón.
- ✅ **Fix #5 — Ctrl+Z en Transform**: bug del helpMarker pisando `GetItemID()` — el tracker rastreaba el `(?)` TextDisabled en vez del DragFloat3, así que NUNCA commiteaba. Reordenado a `DragFloat3 → pushEditIfDone → helpMarker`. Diagnóstico via logs `[inspector] commit '...' al HistoryStack` + `[ctrl+z] undo solicitado`.

**3 bugs adicionales detectados en validación e2e + corregidos en el mismo bloque** (visual feedback del dev mientras probaba):

- ✅ **Bug Plane/Quad/Capsule atravesables** — los handlers de esos 3 brushes bakeaban el scale en el CSG (`Csg::makeBoxBrush(scale(10, 0.05, 10))` + `Transform.scale=(1,1,1)`), así la auto-sync `halfExtents = scale * 0.5` generaba colisión de 1m³ en el centro del plane 10×10. Fix: `spawnBrushEntity(brush, prefix, label, initialScale)` recibe el scale visible, los handlers afectados pasan CSG unitario + scale en Transform. Visual idéntico (render aplica worldMatrix al brush), colisión correcta.
- ✅ **Bug brushes pierden RigidBody tras save/reload** — `SavedBrush` no serializaba el RigidBody. Fix: agregar 7 campos opcionales (`hasRigidBody/type/shape/halfExtents/mass/friction/isSensor`) + serialize/parse + en `applyBrushFromSaved` fallback Static Box `halfExtents = scale * 0.5` para mapas viejos sin el bloque (back-compat).
- ✅ **Bug UX Inspector Weapon** — drag-drop reemplazado por combo `BeginCombo` que enumera `AssetManager::enumerateWeapons()` (lista todas las `.moodweapon` cargadas). Strings hardcoded mezcla es/en reemplazados por **18 keys i18n** nuevas (`editor.panel.inspector.weapon.*`). Card "Weapon empty" del Gameplay tab eliminada (placeholder vacío sin pickup logic — F4H4 traerá WeaponPickup real con mesh + trigger).

---

## Norte

El motor ya sabe armar el disparo (`weapon.fire(...)`), pegarle al maniqui (raycast + damage), spawnar feedback (sonido stub + particle), recargar, equipar. Lo que falta es:

1. **Click izquierdo dispara** — sin escribir `weapon.fire(...)` a mano en consola.
2. **Crear arma desde el editor** sin tocar JSON ni Hierarchy raw.
3. **Browser de armas** — drag-drop de `.moodweapon` files.
4. **Sonidos al disparar** — el AudioDevice del editor llega a los bindings Lua.

---

## Sub-tareas

### Sub-tarea 1 — Input bridge data-driven (D4)

UserSettings se extiende con un mapa de keybindings:

```json
"input": {
    "keybindings": {
        "fire":      "mouse_left",
        "reload":    "r",
        "interact":  "e",
        "jump":      "space"
    }
}
```

Archivos:
- `src/engine/project/UserSettings.h` + `.cpp`: agregar `struct InputSettings { std::map<std::string, std::string> keybindings; }`. Defaults: `fire="mouse_left"`, `reload="r"`.
- Lua binding nuevo: `Input.is_action_pressed("fire") -> bool`. Implementado en un `LuaBindings_Input.cpp` que mantiene un mapa string → GLFW key/button + checkea cada frame.
- El `EditorApplication::tick` setea `wc.firing = Input.is_action_pressed("fire")` para todas las WeaponComponent que sean del player local. Para identificar "player local" — usar tag `"player"` por convencion (engine-generic, el juego decide qué tag usa).
- Settings UI: el dev puede editar el binding desde Settings panel (texto libre por F4H2 bloque B, UI rebindable mejor pa post-F4).

Tests:
- `tests/test_input_keybindings.cpp` (parse roundtrip, default fill, lookup case-insensitive).

### Sub-tarea 2 — AudioDevice en Lua bindings

Extender la cascada para que `setupWeaponBindings` reciba un AudioDevice real.

Archivos:
- `LuaBindings.h`: `setupLuaBindings` agrega `AudioDevice* audio = nullptr` parameter.
- `ScriptSystem::update` agrega `AudioDevice* audio = nullptr`.
- Todos los callers de `ScriptSystem::update` pasan `&m_audioDevice`.
- `LuaBindings.cpp`: pasa `audio` a `setupWeaponBindings`.

Sin tests nuevos — solo cascada de wiring.

### Sub-tarea 3 — AssetBrowser tab "Weapons"

`AssetBrowserPanel`:
- Nueva tab "Weapons" filtrada por `.moodweapon`.
- Icono dedicado (reusar `ICON_FA_GAMEPAD`).
- Drag payload type `"MOOD_WEAPON_ASSET"` para drag-drop al Inspector.
- `rescan()` recorre `assets/weapons/` (o donde sea, segun convencion del proyecto) y filtra por extension.
- Selection en weapon abre `WeaponPropertyEditorPanel` (futuro F4H2.1 — por ahora solo lista).

### Sub-tarea 4 — Crear Entidad → Gameplay → "Arma (vacía)"

`EditorProjectActions_CreateEntity_PickModal.cpp`: tab Gameplay agrega item "Arma (vacía)". Hace:
- Crea Entity con `TagComponent("arma")`, `TransformComponent` default, `WeaponComponent` con `weaponAssetId=0`.
- Selecciona la entity y abre el Inspector.

### Sub-tarea 5 — Demo PANDEMONIUM (opcional)

Si el dev quiere mas armas demo: pistola/rifle/lanzacohetes en `assets/weapons/`. Cada uno es solo un `.moodweapon` con diferentes stats.

---

## Tests + Verificación

- Suite `mood_tests` debe quedar verde (sin regresion).
- E2E manual: editor → cargar mapa con maniqui (F4H1) → crear arma → drag shotgun.moodweapon → Play → click → maniqui muere.

---

## Cierre F4H2 completo

Bloque B cerrado con tag `v3.2.0-fase4-hito2-B` (2026-05-29). F4H2 entero cerrado. Próximo: F4H3 (segunda arma + swap entre armas, viewmodel).

---

## Backlog post-F4H2

- **F4H2.1** — Decals de impacto. Requiere DecalComponent + render pipeline.
- **F4H2.2** — VFX .moodvfx asset type. Hoy el WeaponSystem usa puff procedural hardcoded.
- **F4H2.3** — Reload animation (skeletal anim del viewmodel).
- **F4H2.4** — Recoil + camera shake al disparar.
- **F4H2.5** — Bullet tracer visible.
- **F4H3** — Segunda arma (pistola) + swap input.
