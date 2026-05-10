# PLAN HITO F2H41 — 5 widgets HUD diferidos (Stamina/Objective/KillFeed/Compass/CRT)

> **Estado**: en curso (2026-05-09).
> **Tag previsto**: `v1.31.0-fase2-hito41`.
> **Origen**: pendiente "HUD widgets diferidos" anotado en F2H39
> cierre. Dev confirma scope: 5 widgets pure DrawList (sin nueva
> infra). Mini-map/Radar y themes alternativos quedan como hitos
> propios futuros.

## Filosofía

F2H41 EXTIENDE el framework de F2H39 — no lo reescribe. Cada widget
nuevo es:
1. Una función libre `drawXxx(HudContext&)` agregada a `GameOverlay.cpp`.
2. Un entry en el array `k_widgets[]` (registry hardcoded).
3. State asociado en `HudState` si necesita.
4. Bindings Lua para que scripts disparen / configuren.

Ese patrón es el contrato del framework — agregar widgets nunca
debería tocar más que esos 4 lugares.

## Scope (decisiones confirmadas con dev)

### 5 widgets nuevos (Bloques C)

1. **StaminaBar** (Skyrim/Metro):
   - **Posición**: bottom-left, debajo del HealthNumber.
   - **State**: `int stamina = 100, max_stamina = 100` en HudState.
   - **Visual**: barra horizontal pequeña verde-azul (color distinto
     al HP rojo-naranja para que no se confundan). Color shift a
     amarillo cuando stamina < 30%.
   - **Bindings Lua**: `setStamina`, `setMaxStamina`, getters.

2. **ObjectiveText** (HL2/CoD):
   - **Posición**: top-left, debajo del MenuBar (~120px desde top).
   - **State**: `std::string objective_text` en HudState. Vacío = no
     se dibuja.
   - **Visual**: caja oscura con border amarillo HL + texto blanco.
     Estilo "OBJETIVO: ..." prefix automático.
   - **Bindings Lua**: `setObjective`, `clearObjective`,
     `getObjective`.

3. **KillFeed** (Doom/CoD):
   - **Posición**: top-right, columna stack vertical.
   - **State**: `std::deque<KillEntry>` con `ttl` countdown (mismo
     patrón que `pickup_queue` de F2H39). `KillEntry { string text,
     ImU32 color, float ttl }`. Default lifetime 4s.
   - **Visual**: toast rojo con texto blanco. Cap visual 5 entradas.
   - **Bindings Lua**: `pushKill(text)` simple; opcional `pushKill
     Colored(text, r, g, b)` para diferenciar enemy types.

4. **CompassBar** (Skyrim/CoD):
   - **Posición**: top-center, debajo del MenuBar.
   - **State**: ninguno en HudState (lee `cameraForward` del
     `HudContext`).
   - **Visual**: barra horizontal con tickmarks cada 15° y
     letras N / E / S / W donde corresponda según yaw. Indicador
     central (downward triangle naranja) marca dirección actual.
     Ancho ~400 px en pantalla.
   - **Bindings Lua**: ninguno (auto-driven por la cam). Toggle
     desde `setWidget("compass_bar", false)`.
   - **Cambio único en arquitectura**: `HudContext` gana
     `glm::vec3 cameraForward` (default `vec3(0,0,-1)` para tests
     que no usan cam).

5. **CRT scanline overlay** (Fallout Pip-Boy effect):
   - **Posición**: full-screen overlay después de los demás.
   - **State**: ninguno (toggle desde `widget_enabled`).
   - **Visual**: líneas horizontales semi-transparentes (~20% alpha)
     espaciadas cada 3 px + ligera tinte verde-amarillo por bandas
     alternadas para chromatic aberration sutil. Por DrawList, sin
     shader. Default OFF (toggle Lua).
   - **Bindings Lua**: `setWidget("crt_scanline", true)`.

### `HudContext` extendido

```cpp
struct HudContext {
    ::ImDrawList* dl;
    float x0, y0, w, h;
    float dt;
    HudState* hud;
    float now;
    glm::vec3 cameraForward;  // F2H41: para CompassBar (yaw derivation)
};
```

Callers a actualizar:
- `EditorApplication::drawGameOverlay` (lee `m_playCamera.forward()`).
- `PlayerApplication::endFrame` (lee `m_playCamera.forward()` o
  equivalente).

### Demo `hud_demo.lua` extendido (Bloque F)

Agregar al script existente:
- Cada 1.5s: drain stamina (parecido a HP, pero recupera al pausar
  gameplay).
- Init: setObjective("Demo: explorar el mapa de pruebas").
- Cada 8s: pushKill con texto rotativo ("Killed: Headcrab #N",
  "Killed: Manhack #N", etc).
- Toggle `crt_scanline` cada 12s para que el dev vea el efecto on/off.
- Compass: ninguna acción Lua — auto-driven.

## Bloques de ejecución

| # | Bloque | Estim. | Notas |
|---|--------|--------|-------|
| A | Plan (este doc) | 10 min | Se archiva en cierre |
| B | Expandir HudState (stamina, objective_text, kill_feed, KillEntry) + HudContext.cameraForward | ~15 min | Edit en GameState.h + GameOverlay.h |
| C | Implementar 5 widgets en GameOverlay.cpp + agregar al registry | ~1.5h | StaminaBar trivial; ObjectiveText box+text simple; KillFeed reusa pattern PickupQueue; CompassBar requiere math de yaw + drawing tickmarks; CRT scanline = loop de líneas horizontales |
| D | Bindings Lua expandidos | ~15 min | setStamina/setMaxStamina, setObjective/clearObjective, pushKill |
| E | Callers passan cameraForward al HudContext (PlayerApplication + EditorPlayMode) | ~10 min | One-liner en cada caller |
| F | hud_demo.lua extendido para ejercitar los 5 nuevos | ~10 min | Mantener compatibilidad con timers existentes (HP, damage, pickup, prompt) |
| G | Build + validación visual con dev | ~15 min | Tour: stamina abajo izq, objective texto arriba izq, kill feed arriba der, compass arriba centro con yaw vivo, scanline toggleable |
| H | Cierre: docs (HITOS/ESTADO/DECISIONS/PENDIENTES) + commits + tag + push | ~25 min | Patrón estándar |

**Total estimado**: ~3h.

## Diferidos a hitos chicos propios (anotar en PENDIENTES al cierre)

- **Mini-map / Radar** (CoD/Fallout): requiere render-to-texture
  topdown del mundo cercano. Hito propio mediano.
- **Themes alternativos** (Doom saturado / Fallout verde): requiere
  theme runtime + bindings Lua. Hito chico propio.

## Riesgos y tradeoffs

- **Riesgo: CRT scanline a 3 px de espaciado puede ser caro** en
  pantallas 1440p (~480 líneas en H). Mitigación: spacing >=3 px +
  alpha bajo + skip-pixel patrón si emerge perf issue. DrawList line
  primitives son cheap pero no gratis. Validar Bloque G.
- **Riesgo: CompassBar yaw derivado del `cameraForward` puede chocar
  con FpsCamera.yaw existente** si el dev quiere consistencia exacta
  con el internal yaw state. Aceptable tradeoff: el compass es UI,
  derivar de `forward` es semánticamente claro y se actualiza naturally
  con cam moves.
- **Tradeoff: 5 widgets agregan ~5 KB de codigo + ~600 LOC**.
  Aceptable — el framework es para esto.
- **Tradeoff: HudState crece con `kill_feed` deque**. Memoria
  negligible (kill entries son strings cortos, queue cap visual 5).

## Validación

- Build limpio Debug + Release.
- Editor en Play Mode con `hud_demo.lua` activo:
  - StaminaBar bottom-left visible debajo de HealthNumber, drena con
    el demo.
  - ObjectiveText top-left "OBJETIVO: Demo..." visible.
  - KillFeed top-right empieza vacío, recibe entradas cada 8s.
  - CompassBar top-center con N/E/S/W rotando al mover el mouse en
    Play (cam yaw real).
  - CRT scanline OFF por default; al togglear desde Lua se ve
    overlay tenue de líneas.
- MoodPlayer.exe arranca con un proyecto que tenga el demo → mismos
  efectos.
- Suite **633/8455** verde.

## NO entra en F2H41

- Mini-map / Radar.
- Themes alternativos.
- HUD diegetic 3D (cosa de gameplay/3D, ya descartado).
- Refactor del registry de widgets a vector dinámico.
- Sound feedback (hit marker sound, low HP heartbeat).
- Compass con objective markers proyectados a screen-space (eso
  requiere world-space objectives — agregar después si emerge
  necesidad real).
