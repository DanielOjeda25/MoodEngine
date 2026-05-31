# PLAN F4H4 — HUD de combate + Pickups en el mapa

**Estado:** PLAN, sin codigo todavia.
**Predecesor:** F4H3 cerrado (tag `v3.3.0-fase4-hito3`).
**Origen:** PLAN_FASE4.md §5 F4H3 original era un bloque grande (munición + cambio + HUD + pickups). El F4H3 entregado cubrió swap + viewmodel + arsenal multi-slot. F4H4 cierra los 2 faltantes del bloque original (HUD + Pickups) antes de saltar a proyectiles (que pasan a F4H5).

---

## Norte

Hoy el jugador ENTRA al Play con 4 armas auto-equipadas, dispara, cambia, ve el viewmodel cambiar... pero **no sabe cuanta vida le queda, no sabe cuanto ammo de cada arma, no ve cual arma esta activa sin abrir Inspector**. Y el flow de "encontrar armas en el mapa" no existe — todo aparece magicamente al spawn.

F4H4 cierra esos 2 vacios. El jugador:

- Ve **HP + armor** abajo-izquierda en pantalla en Play.
- Ve **ammo + nombre del arma activa** abajo-derecha.
- Al hacer swap, ve un overlay 2-3s con todas las armas que tiene equipadas (arsenal indicator transient).
- Al recibir daño, la pantalla pulsa un vignette rojo (damage flash).
- Se mueve por el mapa, **levanta armas del suelo** (mesh flotando + spinning estilo Quake/Doom), **botiquines** (verde) **y armadura** (azul), **cajas de ammo** (4 tipos por categoria de arma).
- Al recoger un arma que ya tiene equipada, NO duplica el slot — rellena su ammo a magazine size.
- El spawn del Player vuelve al **flow real**: arranca con **solo escopeta** (slot 0), HP=100, armor=0; el resto del arsenal lo encuentra/recoge en el mapa.

---

## Frontera engine ↔ juego

**Engine provee:**
- `HUDComponent` engine-generic con campos data-driven (no hardcodea "salud" o "ammo"; es un widget configurable).
- `PickupComponent` con tipo enum (`Weapon | Ammo | Health | Armor`) + payload + overlap trigger.
- `ArmorComponent` (gemelo de `HealthComponent`).
- Renderer del HUD via ImGui overlay sobre el viewport del Player (mismo patron `GameOverlay`).
- Bindings Lua para customizar HUD desde scripts del juego.

**Juego provee:**
- Layout concreto del HUD (`assets/hud/combat.moodhud` o similar — TBD si emerge necesidad de data-driven, sino hardcoded en `CombatHUDOverlay`).
- Meshes de pickups (placeholder cubo coloreado por ahora; arte real va F4H5+).
- Spawns iniciales de pickups en mapas demo.

---

## Decisiones (cerradas con AskUserQuestion)

- **D1 — Scope: Bundle HUD + Pickups.** El roadmap textual de PLAN_FASE4 splitteaba F4H3 (munición + swap + HUD + pickups). F4H3 entregado cubrió solo swap + viewmodel. F4H4 cierra los 2 faltantes antes de proyectiles (que pasan a F4H5 con bump del roadmap).
- **D2 — HUD estilo Half-Life/CS moderno (sutil).** HP + armor widget bottom-left, ammo + arma activa widget bottom-right. Arsenal indicator NO permanente — aparece overlay 2-3s al hacer swap (fade-out). Menos ruido visual que Doom esquinas. Descartado: Doom clasico (4 esquinas siempre), Quake bottom bar, Hybrid.
- **D3 — Pickups: kit completo (Weapon + Ammo + Health + Armor).** 4 tipos enum. Sandbox vuelve a "Player arranca con escopeta default + resto en el mapa". Descartado: solo armas + ammo, solo armas único tipo.

**Decisiones del agente (defaults convencionales — overridable por el dev si los ve):**

- **D4 — Arma duplicada al recoger: refill ammo a `magazineSize`.** Convencion Doom/HL/Quake. Si ya tengo la pistola con 3 balas y recojo otra pistola, paso a 12. No duplica slot ni dropea. Edge case: si todos los slots estan llenos y recojo una arma que NO tengo, va al primer slot vacio; si NO hay slot vacio, la pickup queda en el suelo (log warn). Backlog: replace/discard explicito como en Apex va a F4H8+.
- **D5 — Pickup despawn: permanente single-player.** Al recoger desaparece y NO respawnea. Respawn (multiplayer / arena infinita) queda agendizado a F4H10 (sistema de spawn / oleadas) que es donde tiene sentido.
- **D6 — Crosshair estatico.** Punto + 4 lineas (Quake style). NO se abre con spread (eso es feedback de game feel, queda F4H6 game feel pass). Color blanco default, configurable via UserSettings backlog.
- **D7 — Damage flash: vignette rojo full-screen.** Cuando `Health::applyDamage` se llama sobre el player, el HUD pulsa un overlay rojo radial (centro transparente, bordes rojos saturados, fade-out 0.4s). Intensidad escala con %hp perdido. Convencion Doom/HL/COD. NO mover camara (eso va F4H5/F4H6 — camera shake).
- **D8 — HUD hardcoded en C++ por ahora.** No `.moodhud` data-driven asset todavía. El render es ImGui en `CombatHUDOverlay`. Cuando emerja necesidad de variantes (HUD distinto por arma/personaje), F4H6+ puede extraer a asset. YAGNI ahora.

---

## Schema cambios

### `ArmorComponent` (nuevo)

Gemelo de `HealthComponent`. Engine-generic.

```cpp
struct ArmorComponent {
    f32  current = 0.0f;
    f32  max     = 100.0f;
    f32  absorbRatio = 0.66f; // % del dmg incoming que come la armor primero (HL2 suit)
};
```

`Health::applyDamage` extendido: si la entity tiene `ArmorComponent` con `current > 0`, consume `dmg * absorbRatio` de armor primero, el resto al HP. Si armor=0 todo va al HP.

### `PickupComponent` (nuevo)

```cpp
enum class PickupType : u8 { Weapon=0, Ammo=1, Health=2, Armor=3 };

struct PickupComponent {
    PickupType type = PickupType::Health;

    // Weapon: path al .moodweapon a entregar
    std::string weaponPath;

    // Ammo: cantidad + qué arma (filename del weapon asset, o "" = arma activa)
    int          ammoAmount = 0;
    std::string  ammoForWeapon;

    // Health/Armor: monto que entrega
    f32 healthAmount = 25.0f;
    f32 armorAmount  = 25.0f;

    // Animacion: spinning + bobbing
    f32 spinDegPerSec = 90.0f;
    f32 bobAmplitude  = 0.1f;
    f32 bobSpeed      = 2.0f;
    f32 spawnTime     = 0.0f; // set al crear, para offset bob

    bool consumed = false; // marca destroy en proximo tick
};
```

`PickupSystem::tickSystem(scene, dt, player)`:
- Para cada entity con `PickupComponent`: actualiza spin/bob del Transform.
- Trigger overlap con player (usa `TriggerComponent` + `OnTriggerEnter`-style polling) → aplica payload según `type`:
  - `Weapon` → `Weapon::equipWeaponInSlot` en primer slot vacio (o refill ammo si ya tiene).
  - `Ammo` → busca slot con matching weapon, suma ammo (clamp a magazineSize).
  - `Health` → `Health::heal(amount)` clamp a max.
  - `Armor` → `armor.current = min(armor.max, armor.current + armorAmount)`.
- Marca `consumed = true` → frame siguiente destroy.

### `HudState` (extendido)

`HudState` existente (F1 era HUD del MoodPlayer básico). Agregar campos para combat HUD:

```cpp
struct HudState {
    // ... existentes ...

    // F4H4 combat HUD
    f32  damageFlashTimer = 0.0f; // 0..0.4s para vignette
    f32  arsenalOverlayTimer = 0.0f; // 0..3.0s para overlay swap
    bool showCombatHud = true; // toggle global del HUD
};
```

`CombatHUDOverlay` lee `HealthComponent` + `ArmorComponent` + `WeaponComponent` del entity tag `"player"` + `HudState` cada frame.

### `ProjectAction::AddArmor / AddPickup*` (nuevos)

Cards en modal "+ Crear Entidad" tab "Gameplay":
- `AddArmor` (entity con `ArmorComponent` para spawn de armor pickup en el mapa? — o se cubre con AddPickupArmor).
- `AddPickupWeapon` / `AddPickupAmmo` / `AddPickupHealth` / `AddPickupArmor` — 4 cards en Gameplay tab. Cada una spawnea entity con `Transform + MeshRenderer (placeholder cubo) + PickupComponent + TriggerComponent` (esfera radius 1m, sensor).

### `handleAddPlayer` cambio

Sandbox D4 de F4H3 (rellena los 4 slots) → vuelve a **solo escopeta**:
- Slot 0: primera arma alfabetica del catalogo (sigue siendo `enumerateWeapons()` primera).
- Slots 1-3: vacios.
- HP=100/100, armor=0/100.

El dev valida pickups poniendo `AddPickupWeapon` con la pistola en el mapa cerca del spawn.

---

## Sub-tareas

### Sub-tarea 1 — ArmorComponent + Health::applyDamage extendido

- `Components_Gameplay.h`: nuevo `ArmorComponent`.
- `Health::applyDamage(scene, entity, dmg)`: chequea `ArmorComponent`, consume armor primero (con `absorbRatio`), resto al HP. Tests roundtrip.
- Serialization: `SavedArmor { current, max, absorbRatio }` opcional en `SavedEntity`.
- Inspector "Armadura" panel en Gameplay categoria (gemelo de Salud).
- Lua bindings: `armor.get(tag) / set / give`.

### Sub-tarea 2 — PickupComponent + PickupSystem

- `Components_Gameplay.h`: `PickupComponent` + enum `PickupType`.
- `engine/gameplay/pickup/PickupSystem.{h,cpp}`: `tickSystem(scene, dt, playerEntity, assets, audio)`. Maneja spin/bob + overlap detection + payload dispatch + cleanup `consumed=true`.
- Sin TriggerComponent dedicado de F2 — usamos detección de proximidad por distancia al player (simple esfera radius `pickupRadius` ~1.5m). Si emerge necesidad de trigger sofisticado, F4H4.1.
- Lua bindings: `pickup.spawn(type, position, payload)` para scripts de niveles.
- Serialization: `SavedPickup` con todos los campos.
- Tests: spawn pickup health → player con hp<max recoge → hp sube + pickup consumed. Idem ammo, weapon (slot vacio + slot lleno → refill), armor.

### Sub-tarea 3 — CombatHUDOverlay (estilo HL/CS sutil)

- `editor/overlays/CombatHUDOverlay.{h,cpp}` (~250 LOC). ImGui overlay invisible (no window) que dibuja:
  - **Bottom-left widget** (HP + armor): "100 / 100" + icono `ICON_FA_HEART` (rojo) + "100 / 100" + icono `ICON_FA_SHIELD_HALVED` (azul). Tamaño ~24px label + 32px numero. Padding 24px desde bordes.
  - **Bottom-right widget** (ammo + arma): "6 / 12" + nombre arma activa abajo (font medium gris). Tamaño 32px numero.
  - **Damage flash vignette**: si `hudState.damageFlashTimer > 0`, full-screen quad con shader vignette radial rojo (Quad ImGui via `ImDrawList::AddRectFilledMultiColor` aproximado). Alpha lerp `1.0 → 0.0` sobre 0.4s.
  - **Arsenal overlay** (al swap): si `hudState.arsenalOverlayTimer > 0`, top-center fila de 4 mini-iconos (cubo placeholder + nombre arma debajo). El slot activo con border highlight + scale ×1.1. Fade alpha sobre 3s.
  - **Crosshair**: centro pantalla, punto 2px + 4 lineas cruz 8px cada una, blanco con outline negro 1px (legibilidad sobre fondos claros). Toggle config en UserSettings backlog.
- Llamado desde `EditorApplication_Run` Play mode (no editor — los overlays editor desaparecen en Play per F3H29).
- `setDamageFlash(intensity 0..1, duration 0.4s)`: invocado por `Health::applyDamage` cuando entity recibió damage es tag `"player"`.
- `setArsenalOverlay(duration 3.0s)`: invocado por `Weapon::swap*` cuando shooter es tag `"player"`.

### Sub-tarea 4 — handleAddPlayer cambio + Cards pickup

- `handleAddPlayer`: slot 0 = primera arma alfa, slots 1-3 vacios, HP=100, armor=0. Crea entity `__viewmodel` igual que F4H3.
- 4 nuevas cards en `ProjectAction::*` + modal Gameplay:
  - `AddPickupWeapon` con combo `.moodweapon` para elegir cual arma entrega.
  - `AddPickupAmmo` con combo arma + spinbox cantidad.
  - `AddPickupHealth` con spinbox `healthAmount` (default 25).
  - `AddPickupArmor` con spinbox `armorAmount` (default 25).
- Cada card spawnea entity con `Transform + MeshRenderer (cubo coloreado por tipo: amarillo/azul/verde/cian)` + `PickupComponent`.
- 2 cards extras: `AddHealthPack` y `AddArmorPack` ya cubiertas por `AddPickupHealth/Armor` — no duplicar.

### Sub-tarea 5 — Inspector Armor + Inspector Pickup

- `InspectorPanel_Armor.cpp`: gemelo de Salud. Slider current/max + slider absorbRatio (0..1). Reset buttons.
- `InspectorPanel_Pickup.cpp`: enum dropdown `PickupType` + campos condicionales según type. Sliders spin/bob.
- 2 helpers extraidos del patron `inspectorResetButton<T>` ya existente.

### Sub-tarea 6 — Tick wireup en EditorApplication_Run

- `Pickup::tickSystem(scene, dt, playerEntity, assets, audio)` despues de `Weapon::tickViewmodel` en Play mode.
- `CombatHUDOverlay::tick(dt) + render()` despues del scene render (overlay sobre Play view).
- `Health::applyDamage` notifica `combatHud->setDamageFlash(...)` cuando entity es `"player"` (chequea tag).
- `Weapon::applySwap` notifica `combatHud->setArsenalOverlay(3.0f)` cuando shooter es `"player"`.

### Sub-tarea 7 — i18n + iconos FA

- 12-15 keys i18n nuevas: `editor.menu.gameplay.pickup_weapon/ammo/health/armor`, `editor.panel.inspector.armor.*`, `editor.panel.inspector.pickup.*`, `editor.modal.create_entity.pickup_*`.
- Iconos FA: `ICON_FA_HEART` (HP), `ICON_FA_SHIELD_HALVED` (armor), `ICON_FA_BULLSEYE` (ammo), `ICON_FA_GIFT` (pickup generic — verificar existe en atlas).

### Sub-tarea 8 — Tests + suite full verde

- Tests nuevos esperables ~20:
  - `test_armor_component.cpp` (~6): defaults / applyDamage consume armor primero / armor=0 → todo al HP / absorbRatio clamp / serialize roundtrip.
  - `test_pickup_system.cpp` (~10): spawn pickup → player a distancia >radius → no recoge; player overlap → recoge + consumed; weapon pickup a slot vacio / a slot lleno → refill; ammo pickup a slot matching weapon / a slot sin matching → no-op; health/armor clamp a max; consumed=true en next tick.
  - `test_weapon_system.cpp` extension (~3): equipWeaponInSlot ya cubierto F4H3, pero pickup-equip via PickupSystem es path nuevo.
- Suite full verde 1372+/12100+.

### Sub-tarea 9 — Docs + commit + tag

- `ESTADO_ACTUAL.md` + `HITOS.md` + `DECISIONS.md` con entries F4H4.
- Commit con seccion "Chequear:" (Player spawn con solo escopeta; HUD bottom-left/right legible; recoge pickup health → HP sube + el pickup desaparece; recibe daño → vignette rojo; swap arma → arsenal overlay aparece y desaparece).
- Tag `v3.4.0-fase4-hito4`.

---

## Metricas de exito

- HUD HP+armor (BL) + ammo+arma (BR) visible en Play mode editor.
- Damage flash visible al `health.damage("player", N)` desde consola Lua.
- Arsenal overlay aparece al swap, fade-out 3s.
- 4 tipos de pickup spawneable desde modal Gameplay, recogibles, despawnable.
- Player spawn con solo escopeta + HP=100 + armor=0 (sandbox D4 de F4H3 revertido).
- Suite full verde, sin regresión F4H1-F4H3.
- LOC: `CombatHUDOverlay.cpp < 350`. `PickupSystem.cpp < 250`.

---

## Backlog (NO entra a F4H4)

- **F4H4.1** — TriggerComponent dedicado para pickups (en vez de distancia plana). Si el pickup overlap es flaky (ej. pickup dentro de pared, player saltando), refactor a Jolt sensor.
- **F4H4.2** — `.moodhud` asset data-driven (HUD distinto por personaje/arma). YAGNI hoy.
- **F4H5** — Armas de proyectil (rocket/plasma con splash). **Bump del roadmap original** — era F4H4 textual; ahora es F4H5.
- **F4H6** — Game feel pass: muzzle flash, hit marker, screen shake, crosshair dinamico con spread, pain reaction.
- **F4H7** — Replace/discard pickup arma cuando arsenal lleno (Apex style — pickup en suelo replace por arma droppeada).
- Crosshair configurable via UserSettings (color + tamaño + estilo punto/cruz/circle).
- Toggle showCombatHud via tecla H + UserSettings persist.

---

## Riesgos

- **Renumeración del roadmap**: F4H4 textual (proyectiles) pasa a F4H5; F4H5 (game feel) pasa a F4H6. Cascada hasta F4H7+. Mitigación: actualizar PLAN_FASE4.md + ESTADO_ACTUAL para reflejar.
- **Overlap detection por distancia plana**: si el player atraviesa el pickup pero el frame es muy rápido (60 fps con velocidad alta), podria saltar el overlap. Mitigación: radius generoso ~1.5m. Si emerge problema, F4H4.1 con Jolt sensor.
- **Damage flash invasivo en editor Play**: si el dev hace damage al player mientras testea, el vignette molesta. Mitigación: toggle `hudState.showCombatHud` + tecla H para ocultar entero (incluye flash).
- **Armor absorbRatio convención**: HL2 usa 0.66 (66% al armor). Doom no tiene armor real (security/megaarmor son packs). Default 0.66 es Half-Life-ish.

---

## Notas de implementación

- `CombatHUDOverlay` extiende el patron `GameOverlay` de F1 (HUD del MoodPlayer original). Probablemente mejor reusar `GameOverlay` con un modo "combat" en vez de overlay nuevo separado — verificar al arrancar Sub-tarea 3.
- `PickupComponent` overlap detection compite con `TriggerComponent` (F2H33). Por ahora pickup usa distancia plana — simple y suficiente. Si el sistema de triggers ya tiene la infra de overlap-against-player, reusarlo (Sub-tarea 2 audit primero).
- Iconos FA: verificar `ICON_FA_HEART` + `ICON_FA_SHIELD_HALVED` + `ICON_FA_GIFT` están en el atlas del proyecto antes de usar. Fallback `ICON_FA_PLUS` si falta.

---

## Cierre — F4H4 (2026-05-30, tag `v3.4.0-fase4-hito4`)

**Sub-tareas entregadas (9/9):**

1. ✅ **ArmorComponent + Health::applyDamage extendido**. `ArmorComponent{current, max=100, absorbRatio=0.66}` engine-generic. `Health::applyDamage` consume armor primero (clamp `absorbRatio` a `[0,1]` defensivo), sobrante al HP. Serialization `SavedArmor` roundtrip. Lua bindings `armor.give/set/get`.
2. ✅ **PickupComponent + PickupSystem**. Enum `PickupType{Weapon,Ammo,Health,Armor}`. `PickupComponent` con payload por tipo + spin/bob/radius. `Pickup::tickSystem(scene, dt, player, assets)` recorre entities, anima Transform (spin Y + bob sinusoidal), detecta overlap por distancia plana, aplica payload, destroy on consumed. Refill ammo si arma ya equipada (D4). Slot vacio search para weapon.
3. ✅ **HUD widgets armor + arsenal_overlay** (R1 vs plan: reuso framework `GameOverlay` existente en vez de crear `CombatHUDOverlay` separado). `HudState` extendido con `armor/max_armor` + `arsenal_overlay_t` + `arsenal_slots[4]` + `arsenal_active_slot`. Nuevos widgets: `drawArmorNumber` (bottom-left arriba de health, azul `(80,160,255)`); `drawArsenalOverlay` (top-center 4 slots con activo highlighted naranja + fade alpha 3s). Damage flash (`drawDamageVignette`) ya existia desde F2H39 — solo wireamos el trigger. Crosshair (`drawCrosshair`) ya existia.
4. ✅ **handleAddPlayer revert**. Sandbox D4 de F4H3 (rellena 4 slots) → solo slot 0 + escopeta default + HP=100 + armor=0. El dev ahora encuentra el resto del arsenal vía pickups en el mapa.
5. ✅ **Inspector Armor + Inspector Pickup**. `InspectorPanel_Armor.cpp` (~85 LOC) gemelo de Salud con sliders + reset buttons. `InspectorPanel_Pickup.cpp` (~95 LOC) con dropdown enum `PickupType` + campos condicionales por type + slider `pickupRadius`.
6. ✅ **Tick wireup en `EditorApplication_Run::tickSystems`**. Encuentra `playerEntity` por tag, `Pickup::tickSystem(scene, dt, player, assets)` despues de `Weapon::tickViewmodel`. Sync `HudState`: HP/armor/ammo/arsenal_slots/active. Transitions detection con `m_f4h4_prevHitFlashTimer` + `m_f4h4_prevActiveSlot` (miembros nuevos en `EditorApplication.h`) → trigger `GameState::triggerDamageFlash` + `triggerArsenalOverlay`.
7. ✅ **i18n + Add Component menu entries**. 18 keys i18n nuevas (es+en): inspector armor (3) + pickup (7) + component name/desc (6) + `hud.label.armor`. Menu Logic del Inspector "Add Component" gana 3 entradas: Health (faltaba desde F4H1) + Armor + Pickup.
8. ✅ **Tests F4H4**. 39 tests nuevos verdes: 9 en `test_armor_component.cpp` (defaults / sin ArmorComponent flujo F1 intacto / armor=0 al HP / consume 66% / agotamiento / absorbRatio=1.0 / =0.0 / dmg masivo mata / clamp defensivo) + 11 en `test_pickup_system.cpp` (defaults / health full no consume / health damaged consume + destroy / fuera radio no recoge / armor pickup ok / armor sin component no consume / weapon sin AssetManager skip / sin player no destroy / consumed=true → next tick / health clamp max / armor clamp max). **Suite full 1391/12164 verde** (+39 cases / +94 asserts vs F4H3: 1352 → 1391). 0 regresion.
9. ✅ **Docs + commit + tag** (este bloque + commit `v3.4.0-fase4-hito4`).

**Ajustes reactivos durante implementación:**

- **R1 — Widgets en framework `GameOverlay` existente** (vs `CombatHUDOverlay` separado del plan). El plan inicial proponia crear un overlay nuevo dedicado. Al inspeccionar el código, `GameOverlay` ya tenía: framework `HudWidget` extensible, `HudState` con `hp/mag/reserve/damage_t`, `triggerDamageFlash` helper, y widgets para `drawHealthNumber/drawAmmoCounter/drawDamageVignette/drawCrosshair` — el 80% del HUD propuesto. Refactor: extender `HudState` con campos F4H4 + agregar 2 widgets nuevos + sync desde bridge. Resultado: ~150 LOC ahorradas + comportamiento consistente con HUD existente. Documentado al cierre.
- **R2 — Helper `triggerArsenalOverlay` en `GameState`** (no en `GameOverlay`). Mismo patron que `triggerHitMarker`/`triggerDamageFlash`: vive en `GameState.cpp` (sin ImGui dependency) para que Lua bindings + bridge lo invoquen sin arrastrar `imgui.h` a tests headless. Decision consistente con F2H39.
- **R3 — `applyHealthPickup`/`applyArmorPickup` retornan `bool` (no consume si no aplica)**. Inicialmente eran void → siempre marcaban `consumed=true` al overlap. Test fail expuso: si el player esta full HP y pisa un botiquin, el botiquin se borraba sin efecto (anti-pattern Doom: el botiquin queda en el suelo hasta que sea util). Fix: retornar `bool` → solo consume si aplico algo. Convencion Doom/Quake.
- **R4 — Detección de damage/swap por polling en el bridge** (no callbacks). Para evitar acoplar `engine/gameplay/Health.cpp` ↔ `engine/game/state/GameState.h` (rompe layering), el bridge `EditorApplication_Run` cada frame detecta transitions: `hitFlashTimer` subió respecto al frame anterior → `triggerDamageFlash`; `activeSlot` cambió → `triggerArsenalOverlay`. Miembros `m_f4h4_prevHitFlashTimer` + `m_f4h4_prevActiveSlot` en `EditorApplication.h` cachean el estado.
- **R5 — Cards de pickup en el modal NO se agregaron** (plan decia 4 cards `AddPickupWeapon/Ammo/Health/Armor`). En vez, los 3 componentes nuevos (Health/Armor/Pickup) se agregaron al menu "Add Component" en la categoria Logic del Inspector. Workflow: dev crea Empty → "Add Component" → Pickup → edita type en Inspector. Mas YAGNI que 4 cards separadas; HealthComponent tambien faltaba en el menu desde F4H1 (resuelto de paso).

**Bugs build-time fixados:**

- **B1 — `WeaponSpec` forward-decl en `AssetManager.h` no era suficiente**. `PickupSystem.cpp` usaba `spec->magazineSize` con `Spec*` de `getWeapon` — necesitaba `WeaponSpec.h` completo. Fix: include `engine/gameplay/weapon/WeaponSpec.h` en `PickupSystem.cpp`.
- **B2 — `LuaBindings_Armor.cpp` faltaba en tests CMakeLists**. El test target compila `LuaBindings.cpp` que llama `setupArmorBindings` pero el archivo del binding no se compilaba → unresolved external. Fix: agregar a `tests/CMakeLists.txt` (cascade del fix del main CMakeLists).

**Backlog post-F4H4:**
- **F4H4.1** — Triggers Jolt sensors para pickup overlap (vs distancia plana). Si el dev reporta pickups missing por velocidad alta o atravesar paredes.
- **F4H4.2** — `.moodhud` asset data-driven (HUD distinto por personaje/arma). YAGNI hoy.
- **F4H5** — Armas de proyectil (rocket/plasma con splash). **Bump del roadmap original** PLAN_FASE4 — era F4H4 textual, ahora F4H5.
- **F4H6** — Game feel pass: muzzle flash, hit marker, screen shake, crosshair dinámico con spread, pain reaction.
- **F4H7** — Replace/discard arma cuando arsenal lleno (Apex style).
- Crosshair configurable via UserSettings (color, tamaño, estilo).
- Toggle showCombatHud con tecla H + UserSettings persist.
- HudState reserve ammo (hoy F4H4 lo deja en 0 — ammo backpack viene cuando emerja).

