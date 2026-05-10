# PLAN HITO F2H39 — HUD framework extensible + paquete inicial estilo HL/Doom/Fallout

> **Estado**: en curso (2026-05-09).
> **Tag previsto**: `v1.29.0-fase2-hito39`.
> **Origen**: pendiente "HUD del juego procedural/minimalista" anotado
> post-F2H35 + dev confirma estética Half-Life como base con
> influencias Doom/Doom Eternal/Fallout/Metro/CoD.
> **Sustituye**: el F2H39 original era "optimización runtime".
> Postergado a hito futuro sobre la PC de escritorio del dev (GTX
> 1660 / Ryzen 5 5600G del baseline F2H2-F2H6) — la notebook actual
> (Iris Xe) daría números no comparables. PERFORMANCE.md ya marca
> que la sub-fase 2.1 cumplió: "el motor está listo para contenido
> real".

## Filosofía

**Half-Life es la inspiración base del motor.** El HUD honra eso:
tipografía grande para HP/AMMO, paleta naranja-amarillo característica,
minimalismo funcional. Layered con elementos más modernos: damage
vignette tipo Doom Eternal, hit markers tipo CoD, pickup
notifications tipo Fallout/HL.

**UI diegética (Pip-Boy / muñequera Metro) NO entra acá** — eso es
geometría 3D enganchada al brazo del personaje + render-to-texture.
Hito propio futuro. La arquitectura de F2H39 deja la puerta abierta:
un widget puede ser "DiegeticArmHud" que dispatchea su draw a un
mundo 3D en lugar de a un overlay 2D, sin tocar nada del framework.

## Pre-existente (post-audit)

- **`GameOverlay::draw()`** función global monolítica que dibuja todo
  (Crosshair + HP block + Ammo block + Pause menu) en un solo `draw()`
  call.
- **`GameState::hud()`** retorna `HudState{ int hp, int ammo }` —
  state mínimo de Hito 20.
- **LuaBindings**: tabla `hud` con 6 funciones (`setHp/setAmmo/setPaused`
  + getters).
- **SaveLoad**: `.moodsave` serializa `HudState` directo como JSON
  con keys `hp`, `ammo`. Schema sin versionar pero aditivo.
- **PlayerApplication_Frame.cpp** llama `GameOverlay::draw()` cada
  frame con `(dl, x0, y0, w, h, exitLabel, onExitCallback)`.

## Scope (decisiones confirmadas con dev)

### 1. Arquitectura extensible (Bloque B)

- **`HudWidget` como struct simple**, no clase con vtable. Campos:
  `name`, `enabled`, `draw_fn` (function pointer / `std::function`).
  Razón: ~10 widgets → vtable overhead innecesario. Función pura es
  testeable + sin estado oculto.
- **`HudContext`** struct con (`x0, y0, w, h, dt, dl, hud, paused`)
  para no pasar 7 parámetros a cada widget.
- **Registry estático**: lista de widgets default registrada al
  build. Iterable + togglable. Custom widgets futuros (Lua-driven o
  C++-driven) se registran extendiendo la lista.
- **Toggle por widget desde Lua**: `hud.set_widget("hitmarker", true)`.
  State en `HudState::widget_enabled` (map `name → bool`).
- **`HudState` extendido** (backward-compat con SaveLoad). Campos
  nuevos:
  - `int max_hp = 100`
  - `int mag = 30, max_mag = 30, reserve = 90` (rediseño de Ammo
    para distinguir cargador / reserva — HL/CoD style).
  - `f32 hit_marker_t = 0.0f` (countdown timer 0.3s)
  - `glm::vec2 damage_dir{0}` + `f32 damage_t = 0.0f`
  - `std::string interact_prompt` (vacío = no mostrar)
  - `std::deque<std::pair<std::string, f32>> pickup_queue` (mensaje
    + timestamp; queue limitada a ~5 entradas)

### 2. Estética default (paleta Half-Life)

| Elemento | Color | Uso |
|---|---|---|
| Acento principal | `#F58220` (naranja Valve) | HP/AMMO numbers, crosshair fallback |
| Acento secundario | `#FFCC00` (amarillo HL) | Pickup notifications, interact prompt |
| Cyan hit marker | `#10F0FF` | Hit confirmation |
| Rojo damage | `#FF3030` | Vignette + low HP shift |
| Blanco | `#F8F8F8` | Texto neutro |
| Background | `rgba(0,0,0,180-230)` | Boxes + pause overlay dim |

Toggleable a otros temas vía Lua si emerge demanda (ej.
`hud.set_theme("doom")`). En F2H39 v1: solo paleta HL hardcoded.

### 3. Paquete inicial — 8 widgets (Bloques C+D+E)

**Rediseñar (3 existentes)**:

1. **HealthNumber** — Hito 20 era rectángulo 140x70 con label "HP" +
   número chico. Rediseño HL: número GRANDE (font scale 2.5x) + label
   chico arriba "HEALTH" + barra horizontal procedural delgada al
   lado. Color shift cuando HP < 25% (de naranja a rojo). Posición:
   bottom-left.
2. **AmmoCounter** — split en mag/reserve estilo HL/CoD. "MAG:30/30"
   formato grande + "RESERVE: 90" abajo más chico. Icono procedural
   de bala (rectángulo + triángulo) al lado. Posición: bottom-right.
3. **Crosshair** — pre-F2H39 era + simple. Rediseño: dot central +
   cruz delgada outline + opcional spread (desactivado en v1).
   Toggleable estilo (dot only / cross only / dot+cross).

**Nuevos (4)**:

4. **DamageVignette** — vignette rojo radial procedural (cosine
   distance from center, alpha fade) + arco direccional indicando
   de dónde vino el daño. State: `damage_dir` + `damage_t` que
   decae en 0.5s. Drawing: `AddRectFilledMultiColor` para gradiente
   o función custom con N segmentos del arco.
5. **HitMarker** — cruz cyan transient `+` que aparece 0.3s al
   pegar. State: `hit_marker_t`. Lua: `hud.show_hit_marker()` resetea
   timer.
6. **InteractPrompt** — texto "[E] Levantar X" centrado arriba del
   crosshair, fade in/out con la presencia/ausencia del string.
   State: `interact_prompt`. Lua: `hud.set_interact_prompt("Levantar
   ammo")` + `hud.clear_interact_prompt()`.
7. **PickupNotification** — toast bottom-center con queue. Cada
   pickup aparece, fade in/out 2s. State: `pickup_queue`. Lua:
   `hud.push_pickup("Picked up: 10mm Ammo +12")`.

**Rediseñar (1 existente)**:

8. **PauseMenuOverlay** — Hito 20: rectángulo plano con 3 botones
   gris-azul. Rediseño Doom-style: dim background ~85% alpha, título
   "PAUSED" en font scale 4x (centro arriba), 3 botones con border
   geométrico (chevrons o ángulos) en lugar de rectángulo simple,
   hover effect con glow. "Continuar" / "Opciones" / "Salir al
   menú".

### 4. Bindings Lua nuevos (Bloque B)

```lua
-- Expandidos (Hito 20 mantiene los originales):
hud.set_max_hp(100)
hud.set_mag(30)
hud.set_max_mag(30)
hud.set_reserve(90)
hud.flash_damage(dir_x, dir_y)  -- vec2 normalized; (0,1)=enemigo enfrente
hud.show_hit_marker()
hud.set_interact_prompt("Levantar ammo")
hud.clear_interact_prompt()
hud.push_pickup("Picked up: 10mm Ammo +12")

-- Toggle de widgets:
hud.set_widget("hitmarker", true)
hud.is_widget_enabled("hitmarker")
```

### 5. Demo script `hud_demo.lua` actualizado

Extender el demo para ejercitar los widgets nuevos:
- Init: HP=100, MaxHp=100, Mag=12/30, Reserve=90.
- Cada 1s: drena 1 HP (igual que pre-F2H39).
- Cada 3s: triggers `flash_damage(random_dir)` + `show_hit_marker()`
  para que el dev vea los efectos en Play Mode sin scripting nuevo.
- Cada 5s: `push_pickup("Demo pickup #N")`.
- Cuando HP=0 → pause forzada (igual que pre-F2H39).

## Bloques de ejecución

| # | Bloque | Estim. | Notas |
|---|--------|--------|-------|
| A | Plan (este doc) | 15 min | Se archiva en cierre |
| B | Arquitectura extensible (HudWidget + registry + expand HudState + bindings Lua) | ~1h | Refactor de `GameOverlay.cpp`. Sin pérdida de funcionalidad — los 3 widgets originales se reescriben como funciones individuales registradas |
| C | Rediseño HealthNumber + AmmoCounter + Crosshair | ~45 min | Estética HL: número grande, mag/reserve split, dot+cross adaptativo |
| D | Widgets nuevos (DamageVignette + HitMarker + InteractPrompt + PickupNotification) | ~1h | 4 funciones nuevas + state expansion + bindings + demo Lua extension |
| E | Rediseño PauseMenuOverlay (Doom-style) | ~30 min | Título grande + botones con border geométrico + hover glow |
| F | Build + validación visual con dev en Play Mode | ~15 min | Spawn `hud_demo` + entrar a Play, verificar que los 8 widgets se ven OK |
| G | Cierre | 30 min | Docs (HITOS/ESTADO/DECISIONS/PENDIENTES) + commits + tag + push |

**Total estimado**: ~4h. Hito mediano con scope acotado.

## Diferidos a hitos futuros (anotar en PENDIENTES al cierre)

- **CompassBar** (Skyrim/CoD): barra horizontal arriba con marker
  N/S/E/W + objective markers proyectados.
- **ObjectiveText** (HL2/CoD): texto persistente lateral con el
  objetivo actual.
- **KillFeed** (Doom/CoD): lista de kills transient arriba derecha.
- **Stamina/MagicBar** (Skyrim/Metro): segunda barra al lado del HP
  para sistemas de estamina/magia/oxígeno.
- **Mini-map / Radar** (CoD/Fallout): topdown view del mundo cercano.
- **CRT scanline overlay** (Fallout Pip-Boy effect global): post-
  process tipo scanline + chromatic aberration sobre todo el HUD
  para "feel retro". Toggleable.
- **HUD diegetic 3D** (Pip-Boy / Metro muñequera / Doom plasma rifle
  display): geometría 3D enganchada al brazo del player + render-to-
  texture del HUD a una textura del modelo. Hito propio mayor —
  requiere FPS arms (mesh + animator del brazo) primero.
- **Themes alternativos** (Doom paleta saturada / Fallout monocromo
  verde): toggle desde Lua. v1 hardcodea HL paleta.

## Riesgos y tradeoffs

- **Riesgo: el rediseño rompe SaveLoad backward-compat.** Mitigación:
  campos nuevos en `HudState` se serializan opcionales (default
  values al deserializar saves viejos). Schema del .moodsave sigue
  sin versionar.
- **Riesgo: paleta naranja choca con el outline naranja del editor
  selected.** No aplica — el HUD es del Player (Play Mode), no del
  editor. En el editor Play Mode el outline solo se ve en Editor
  Mode regular; cuando el dev entra a Play el editor desactiva sus
  overlays.
- **Tradeoff: SaveLoad no versiona schema.** Si en hitos futuros
  agregamos más fields a HudState, el .moodsave se actualiza
  silenciosamente. Riesgo bajo (es runtime state, no project state).
- **Tradeoff: vignette procedural via DrawList AddRect con muchos
  triángulos puede ser caro en GPUs viejas.** Mitigación: limitar a
  ~16 segmentos del vignette + dampening del alpha. Si emerge
  necesidad de fidelidad mayor, pasar a fullscreen shader pass.

## Validación

- Build limpio Debug + Release.
- Editor en Play Mode con el demo `hud_demo.lua` activo:
  - HealthNumber bottom-left, número grande, color shift cuando
    HP < 25%.
  - AmmoCounter bottom-right con mag/reserve split.
  - Crosshair centro-pantalla.
  - DamageVignette parpadea cuando el demo dispara `flash_damage`.
  - HitMarker aparece transient cuando dispara `show_hit_marker`.
  - InteractPrompt se ve cuando se setea, desaparece al limpiar.
  - PickupNotification toast bottom-center con queue.
  - Pause menu rediseñado con título grande + botones border.
- MoodPlayer.exe arranca con un proyecto que tiene el demo →
  mismos efectos visuales.
- Suite **633/8455** verde.

## NO entra en F2H39

- Diegetic UI 3D (Pip-Boy, muñequera Metro).
- CompassBar / ObjectiveText / KillFeed / Stamina / Mini-map.
- CRT scanline overlay.
- Themes alternativos.
- Refactor del SaveLoad para versionar schema.
- FPS arms (mesh + animator del brazo del player).
- Sound feedback (hit marker sound, low HP heartbeat).
