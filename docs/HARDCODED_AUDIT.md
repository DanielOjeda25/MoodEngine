# Auditoría de valores hardcodeados — F3H3

> **Propósito:** mapa de las constantes de comportamiento que el usuario final del editor querría poder editar sin recompilar. Output de F3H3 (Sub-fase 3.1 "El editor te respeta"). NO incluye código — solo el catálogo para que F3H4-F3H7 sepan qué migrar, dónde, y en qué orden.
>
> **Regla aplicada para filtrar:** *"¿un dev/jugador final querría tunear este valor sin recompilar?"* — si la respuesta es no (es decisión arquitectónica, magic number de Jolt fijo, buffer size de implementación), se descarta y se anota la razón.
>
> **Generado:** 2026-05-24 vía 3 sweeps paralelos (Gameplay+Physics / Editor+UI / Render).

---

## Resumen ejecutivo

**Hits totales catalogados:** ~57 (28 gameplay+physics + 13 editor+UI + 16 render).

**Distribución post-filtro:**

| destino | hits a migrar | observación |
|---|---|---|
| `.moodproj` | **22** | Gameplay (speeds, jump, headbob) + Vehicle (steer/friction) + Snap config + Quality (shadow res). El bucket más grande. |
| `UserSettings` | **9** | Gravity global + sensitivities (gizmo size, zoom factor, click/drag threshold). |
| `Inspector` | **4** | Timers de HUD feedback (hit marker, damage flash, pickup TTL, kill feed). Baja prioridad. |
| `.moodmap` | **0** | Nada candidato (todo lo per-mapa ya vive en `EnvironmentComponent` / `PlayerSpawn` desde F2H86). |
| Ya expuesto / descartar | **22** | F2H86 expuso ~80% del render via `EnvironmentComponent`; LightGrid + BloomPass mips son arquitectónicos. |

**Hallazgos críticos (no son solo "hardcoded" — son bugs latentes o redundancias):**

1. **🐛 ~~Discrepancia walk/crouch speeds Editor vs Player~~ — ✅ FIXED en cierre F3H3.** [src/player/PlayerApplication_Frame.cpp:180-181](src/player/PlayerApplication_Frame.cpp#L180-L181) usaba 4.0/2.0 mientras [src/editor/application/EditorPlayMode.cpp:504-505](src/editor/application/EditorPlayMode.cpp#L504-L505) usaba 5.5/3.0. El comentario en EditorPlayMode L499-503 documentaba que F2H41 hizo un tuning intencional citando FPS convention (HL2~5.5, CoD~6, Doom Eternal~7) — pero el runtime Player nunca recibió el update. El usuario que corría `mood_player.exe` sentía un juego más lento que en PlayInEditor. **Fix reactivo en cierre F3H3**: Player unificado a 5.5/3.0 con comentario `// F3H3 fix: paridad con EditorPlayMode`. F3H4 igual migrará ambos a `.moodproj > Gameplay` para que el dev edite un solo lugar (consolidando el patrón).

2. **🔁 ~~Gravity con tres sources of truth~~ — ✅ FIXED en cierre F3H3.** [src/engine/physics/world/PhysicsWorld.cpp:105](src/engine/physics/world/PhysicsWorld.cpp#L105) definía `-9.81` para el mundo Jolt; [src/engine/physics/vehicle/VehicleConfig.cpp:80](src/engine/physics/vehicle/VehicleConfig.cpp#L80) y [src/engine/physics/vehicle/VehicleConfigWriter.cpp:22](src/engine/physics/vehicle/VehicleConfigWriter.cpp#L22) usaban `9.81` para resolver la fórmula analítica de suspensión. Si el dev cambia gravity a `-3.71` (Marte), la suspensión quedaba mal calibrada porque seguía resolviendo con `9.81`. **Fix reactivo en cierre F3H3**: nueva constante `Mood::physics::kEarthGravityMagnitude` en PhysicsWorld.h, los 3 sites consumen el mismo símbolo. Cuando F3H4 migre gravity a `.moodproj > Physics`, este valor pasa a ser el default y las fórmulas de suspensión necesitan que F3H4 las cablee al valor live (no a la constante) — pero el "single source of truth" está resuelto.

3. **🎯 Snap step default = 16** ([src/editor/application/EditorApplication.cpp:215](src/editor/application/EditorApplication.cpp#L215)) — alto prioridad porque el dev cambia esto frecuentemente. Per-proyecto tiene sentido (mapa de mundo abierto vs interior detallado tienen escalas distintas).

4. **🌑 Shadow map size = 2048** ([src/engine/render/passes/ShadowPass.h:54](src/engine/render/passes/ShadowPass.h#L54)) — único candidato real del bucket render. Impacto notable: VRAM (2048² × 4 cascades × float = 64 MB) + calidad de sombras. Quality preset clásico por proyecto (low=512, med=1024, high=2048, ultra=4096).

**Recomendación de orden para F3H4-F3H7:**

- **F3H4 — Gameplay tier 1** (bucket `.moodproj > Gameplay`): walk/crouch speeds, jump velocity, jump cooldown. Cierra la discrepancia Editor↔Player como side-effect. Alta prioridad, scope chico, valida el patrón "hardcoded → `.moodproj`" antes de hitos más grandes.
- **F3H5 — Character + headbob** (bucket `.moodproj > Character`): capsule dimensions, eye height, headbob freq/amplitude. Media prioridad, complementa F3H4.
- **F3H6 — Atajos configurables** (era el plan original): bucket `UserSettings > Shortcuts`. Sigue con keymap completo, sin tocar gameplay.
- **F3H7 — Sensitivities + snap** (buckets `UserSettings > Editor` + `.moodproj > Snap`): gizmo sizes, mouse zoom factor, snap steps, snap default. Quality-of-life del editor.
- **Diferidos (más tarde en Sub-fase 3.1 o 3.4):**
  - Vehicle (`steer rates, friction multipliers`) → cuando emerja demanda real de tuning de vehículos por proyecto.
  - HUD timers (`hit marker, damage flash, pickup TTL, kill feed`) → baja prioridad, esperar señal del dev.
  - Shadow map size → Sub-fase 3.4 (Viewport + Performance), encaja con presets de calidad.

---

## Tabla completa por bucket

### Bucket 1 — `.moodproj > Gameplay` (alta prioridad)

| file:line | valor | uso |
|---|---|---|
| [src/player/PlayerApplication_Frame.cpp:175](src/player/PlayerApplication_Frame.cpp#L175) | 5.5f | jump velocity magnitude |
| [src/player/PlayerApplication_Frame.cpp:176](src/player/PlayerApplication_Frame.cpp#L176) | 0.2f | jump cooldown anti-hold |
| [src/player/PlayerApplication_Frame.cpp:184](src/player/PlayerApplication_Frame.cpp#L184) | 5.5f | walk speed (Player, F3H3 fix paridad ✅) |
| [src/player/PlayerApplication_Frame.cpp:185](src/player/PlayerApplication_Frame.cpp#L185) | 3.0f | crouch speed (Player, F3H3 fix paridad ✅) |
| [src/editor/application/EditorPlayMode.cpp:504](src/editor/application/EditorPlayMode.cpp#L504) | 5.5f | walk speed (Editor PlayInEditor) |
| [src/editor/application/EditorPlayMode.cpp:505](src/editor/application/EditorPlayMode.cpp#L505) | 3.0f | crouch speed (Editor PlayInEditor) |
| [src/editor/application/EditorPlayMode.cpp:237](src/editor/application/EditorPlayMode.cpp#L237) | 3.0f | mount radius threshold (E para subirse a vehículo) |

### Bucket 2 — `.moodproj > Character` (media prioridad)

| file:line | valor | uso |
|---|---|---|
| [src/player/PlayerApplication_Frame.cpp:172](src/player/PlayerApplication_Frame.cpp#L172) | 0.5f | capsule half-height standing |
| [src/player/PlayerApplication_Frame.cpp:173](src/player/PlayerApplication_Frame.cpp#L173) | 0.1f | capsule half-height crouch |
| [src/player/PlayerApplication_Frame.cpp:174](src/player/PlayerApplication_Frame.cpp#L174) | 0.4f | capsule radius |
| [src/player/PlayerApplication_Frame.cpp:412](src/player/PlayerApplication_Frame.cpp#L412) | 0.7f | eye height standing |
| [src/player/PlayerApplication_Frame.cpp:413](src/player/PlayerApplication_Frame.cpp#L413) | 0.3f | eye height crouching |
| [src/player/PlayerApplication_Frame.cpp:415](src/player/PlayerApplication_Frame.cpp#L415) | 5.0f | headbob frequency (rad/s) |
| [src/player/PlayerApplication_Frame.cpp:416](src/player/PlayerApplication_Frame.cpp#L416) | 0.04f | headbob amplitude |

### Bucket 3 — `.moodproj > Vehicle` (media prioridad — esperar demanda)

| file:line | valor | uso |
|---|---|---|
| [src/editor/application/EditorPlayMode.cpp:387](src/editor/application/EditorPlayMode.cpp#L387) | 0.5f | brake vs reverse speed ratio |
| [src/editor/application/EditorPlayMode.cpp:416](src/editor/application/EditorPlayMode.cpp#L416) | 3.0f | vehicle steer turn-in rate |
| [src/editor/application/EditorPlayMode.cpp:417](src/editor/application/EditorPlayMode.cpp#L417) | 6.0f | vehicle steer return rate |
| [src/engine/physics/world/PhysicsWorld_Vehicle.cpp:181](src/engine/physics/world/PhysicsWorld_Vehicle.cpp#L181) | 0.7f | tire friction multiplier (road) |
| [src/engine/physics/world/PhysicsWorld_Vehicle.cpp:186](src/engine/physics/world/PhysicsWorld_Vehicle.cpp#L186) | 0.5f | tire friction multiplier (off-road) |

### Bucket 4 — `.moodproj > Snap` (alta prioridad)

| file:line | valor | uso |
|---|---|---|
| [src/editor/application/EditorApplication.cpp:212-224](src/editor/application/EditorApplication.cpp#L212-L224) | `{1,2,4,8,16,32,64,128}` | snap steps disponibles |
| [src/editor/application/EditorApplication.cpp:215](src/editor/application/EditorApplication.cpp#L215) | 4 (index) | snap step default → 16 unidades |
| [src/editor/application/EditorApplication_Snap.cpp:45](src/editor/application/EditorApplication_Snap.cpp#L45) | 0.02f | snap-to-vertex threshold NDC |
| [src/editor/application/EditorApplication_Snap.cpp:49](src/editor/application/EditorApplication_Snap.cpp#L49) | 16.0f | snap broadphase mínimo |

### Bucket 5 — `.moodproj > Physics` (media prioridad)

| file:line | valor | uso |
|---|---|---|
| [src/engine/physics/world/PhysicsWorld.h:130](src/engine/physics/world/PhysicsWorld.h#L130) | 1.0f | body mass default |
| [src/engine/physics/world/PhysicsWorld.h:131](src/engine/physics/world/PhysicsWorld.h#L131) | 0.5f | body friction default |
| [src/engine/physics/world/PhysicsWorld_Internal.h:218](src/engine/physics/world/PhysicsWorld_Internal.h#L218) | 1.0f | impact speed threshold (auto-ragdoll) |

### Bucket 6 — `.moodproj > Quality` (media prioridad, Sub-fase 3.4)

| file:line | valor | uso |
|---|---|---|
| [src/engine/render/passes/ShadowPass.h:54](src/engine/render/passes/ShadowPass.h#L54) | 2048 | shadow map resolution (cuadrada) |
| [src/engine/render/passes/ShadowPass.h:55](src/engine/render/passes/ShadowPass.h#L55) | 4 | CSM cascade count default |

### Bucket 7 — `.moodproj > Physics` (alta — gravity)

| file:line | valor | uso |
|---|---|---|
| [src/engine/physics/world/PhysicsWorld.h](src/engine/physics/world/PhysicsWorld.h) | `kEarthGravityMagnitude = 9.81f` | constante canonica (F3H3 ✅) — owner |
| [src/engine/physics/world/PhysicsWorld.cpp:107](src/engine/physics/world/PhysicsWorld.cpp#L107) | usa `physics::kEarthGravityMagnitude` | gravity del mundo Jolt (lee del owner ✅) |
| [src/engine/physics/vehicle/VehicleConfig.cpp:85](src/engine/physics/vehicle/VehicleConfig.cpp#L85) | usa `physics::kEarthGravityMagnitude` | g en fórmula suspensión (lee del owner ✅) |
| [src/engine/physics/vehicle/VehicleConfigWriter.cpp:24](src/engine/physics/vehicle/VehicleConfigWriter.cpp#L24) | usa `physics::kEarthGravityMagnitude` | g en write-time `attach_y` (lee del owner ✅) |

> F3H3 ya unificó las 3 ocurrencias del literal a la constante `kEarthGravityMagnitude`. F3H4 migra el valor a `.moodproj > Physics` y debe cablear el live value a la fórmula de suspensión (hoy lee de la constante; el día que el dev cambie gravity, suspension necesita el live value, no el default). **Decisión**: `.moodproj > Physics` (gameplay-relevant > debug-only) — un proyecto de Marte tiene gravity distinta a uno de Tierra.

### Bucket 8 — `UserSettings > Editor` (media prioridad — F3H7)

| file:line | valor | uso |
|---|---|---|
| [src/engine/scene/core/OrthoCamera.h:37](src/engine/scene/core/OrthoCamera.h#L37) | 32.0f | alto inicial frustum ortho (zoom) |
| [src/engine/scene/core/OrthoCamera.h:39-40](src/engine/scene/core/OrthoCamera.h#L39-L40) | 1.0f / 8192.0f | límites mín/máx zoom ortho |
| [src/engine/scene/core/OrthoCamera.h:114](src/engine/scene/core/OrthoCamera.h#L114) | 1.1f | factor velocidad zoom wheel |
| [src/editor/ui/EditorOverlay_Gizmo.cpp:57](src/editor/ui/EditorOverlay_Gizmo.cpp#L57) | 60.0f | tamaño brazo gizmo translate/scale (px) |
| [src/editor/ui/EditorOverlay_Gizmo.cpp:58](src/editor/ui/EditorOverlay_Gizmo.cpp#L58) | 55.0f | radio gizmo rotate (px pantalla) |
| [src/editor/ui/EditorOverlay_Gizmo.cpp:76](src/editor/ui/EditorOverlay_Gizmo.cpp#L76) | 70.0f | target tamaño ring rotate (px) |
| [src/editor/panels/scene/OrthoViewportPanel.cpp:217](src/editor/panels/scene/OrthoViewportPanel.cpp#L217) | 16.0f | click vs drag threshold ortho |
| [src/editor/panels/scene/ViewportPanel.h:200](src/editor/panels/scene/ViewportPanel.h#L200) | 4 px | click vs drag threshold perspectiva |

### Bucket 9 — Inspector (baja prioridad — HUD feedback timers)

| file:line | valor | uso |
|---|---|---|
| [src/engine/game/state/GameState.cpp:46](src/engine/game/state/GameState.cpp#L46) | 0.3f | hit marker feedback duration |
| [src/engine/game/state/GameState.cpp:53](src/engine/game/state/GameState.cpp#L53) | 0.5f | damage flash duration |
| [src/engine/game/state/GameState.cpp:62](src/engine/game/state/GameState.cpp#L62) | 2.5f | pickup notification TTL |
| [src/engine/game/state/GameState.cpp:81](src/engine/game/state/GameState.cpp#L81) | 4.0f | kill feed entry TTL |

> Bucket débil — `GameState` no tiene representación en Inspector. Si migra, probablemente vaya a un componente nuevo `HudConfigComponent` o a `.moodproj > HUD`. Esperar señal del dev antes de mover.

### Bucket 10 — Profundidad de ortho clip (caso edge)

| file:line | valor | uso |
|---|---|---|
| [src/engine/scene/core/OrthoCamera.h:101](src/engine/scene/core/OrthoCamera.h#L101) | 0.1f / 4096.0f | near/far plane ortho |

> Baja prioridad — los devs rara vez tocan near/far de la cámara ortho. Si emerge fricción al editar mapas muy grandes, migrar a `.moodproj > Editor`.

---

## Descartados (no migrar)

**Arquitectónicos / fijos por implementación:**

- `LightGrid tile size = 16` ([src/engine/render/pipeline/LightGrid.h:34](src/engine/render/pipeline/LightGrid.h#L34)) — compartido CPU↔shader, cambiar requiere recompile del shader. Decisión Forward+ fija.
- `BloomPass mip levels = 6` ([src/engine/render/passes/BloomPass.h:81](src/engine/render/passes/BloomPass.h#L81)) — pirámide de bloom precomputada, memoria estática. Cambiar requiere realloc de framebuffers.
- `MAX_BONES = 128` (en shaders, no listado) — fijo por UBO layout.
- Padding/spacing de ImGui (`ItemSpacing`, `FramePadding`, etc.) — decisión estética unified (F2H77 `applyMetrics`), NO es candidato a per-proyecto.

**Ya expuestos vía `EnvironmentComponent` (F2H86) — no re-migrar:**

- Fog mode/color/density/start/end ([src/engine/render/passes/Fog.h:34-39](src/engine/render/passes/Fog.h#L34-L39)).
- Exposure default + tonemap mode ([src/engine/render/scene_renderer/SceneRenderer.h:276-277](src/engine/render/scene_renderer/SceneRenderer.h#L276-L277)).
- Bloom enable/threshold/intensity/radius ([src/engine/render/scene_renderer/SceneRenderer.h:281-284](src/engine/render/scene_renderer/SceneRenderer.h#L281-L284)).
- SSAO enable/radius/intensity ([src/engine/render/scene_renderer/SceneRenderer.h:287-289](src/engine/render/scene_renderer/SceneRenderer.h#L287-L289)).
- SSR enable/steps/thickness/stepSize/intensity ([src/engine/render/scene_renderer/SceneRenderer.h:293-297](src/engine/render/scene_renderer/SceneRenderer.h#L293-L297)).
- CSM split lambda + cascade count ([src/engine/render/scene_renderer/SceneRenderer.h:306-307](src/engine/render/scene_renderer/SceneRenderer.h#L306-L307)).

**Ya expuestos vía `.moodproj` (F2H40 G — coyote + jump buffer):**

- `coyoteWindowSec` / `jumpBufferWindowSec` — leídos por ambos PlayerApplication_Frame y EditorPlayMode. F3H4 sigue el mismo patrón.

---

## Notas operativas

- **Mecánica per-bucket**: cada hito F3H4+ toma un bucket completo, no cherry-picks. Esto evita migraciones a medias y garantiza consistencia (ej. si migrás walk speed sin crouch speed, el .moodproj queda mitad expuesto mitad hardcoded).
- **Schema sin bump**: igual que F3H1, cada bucket nuevo agrega keys opcionales bajo `settings.<bucket>` en `.moodproj`. Back-compat por defaults — proyectos viejos siguen cargando sin migrar.
- **Cero refactor del Player runtime**: el Player ya tiene la infra para leer `.moodproj` (es el mismo struct `Project`). F3H4 solo agrega los fields y los reads en ambos call-sites.
- **Test plan por hito**: cada migración agrega un test de roundtrip del bucket (mismo patrón que `test_project_settings.cpp` de F3H1).
- **Validación visual obligatoria**: cualquier cambio que afecte feel (walk speed, gizmo size, snap default) requiere prueba antes de cerrar el hito — un valor distinto puede sentirse mal aunque "compile y pasen los tests".
