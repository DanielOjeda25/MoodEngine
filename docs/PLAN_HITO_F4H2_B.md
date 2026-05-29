# PLAN F4H2 Bloque B — Wiring final: input + UX + AssetBrowser + audio

**Estado:** PLAN, dependencias listas tras F4H2 Bloque A (tag `v3.2.0-fase4-hito2-A`).
**Predecesor:** F4H2 Bloque A (WeaponSpec asset + WeaponComponent + WeaponSystem + Lua bindings + Inspector).
**Origen:** scope split decidido al cerrar Bloque A — la infraestructura data-driven cerro completa, pero el e2e "click → mata maniqui" necesita el input bridge + UX. Mantener bloque A solo seria un commit gigante.

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

Al cerrar bloque B → tag `v3.2.0-fase4-hito2`. F4H2 entero cerrado. Listo para F4H3 (segunda arma + swap entre armas).

---

## Backlog post-F4H2

- **F4H2.1** — Decals de impacto. Requiere DecalComponent + render pipeline.
- **F4H2.2** — VFX .moodvfx asset type. Hoy el WeaponSystem usa puff procedural hardcoded.
- **F4H2.3** — Reload animation (skeletal anim del viewmodel).
- **F4H2.4** — Recoil + camera shake al disparar.
- **F4H2.5** — Bullet tracer visible.
- **F4H3** — Segunda arma (pistola) + swap input.
