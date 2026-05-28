# PLAN F3H22 — Performance feedback: Profiler + Stats overlay

**Estado:** **A DEFINIR** (arrancar tras F3H21).
**Predecesor:** F3H21 (viewport pro — numpad views + render modes).
**Origen:** `PLAN_FASE3.md` Sub-fase 3.4 (consolidado ex-F3H23 Profiler + ex-F3H24 Stats overlay en uno solo, ver `PLAN_FASE3.md:91`).

---

## Avance de Sub-fase 3.4 (post-F3H21)

```
F3H20 –  ✅ Snapping configurable Hammer-style
F3H21 –  ✅ Viewport pro: numpad views + 4 render modes
F3H22 –  — Performance feedback: Profiler + Stats overlay ⬅ próximo
F3H23 –  — Comunicación al dev: Console + Toasts
F3H24 –  — Crash recovery + autosave (cierra Fase 3)
```

---

## Norte

`PLAN_FASE3.md` Sub-fase 3.4 declara:
> **F3H22 — Performance feedback: Profiler + Stats overlay.**
> *(Une el ex-F3H23 Profiler + ex-F3H24 Stats overlay — comparten métricas runtime FPS/drawcalls/tris/mem GPU. Stats es "vista mínima del Profiler".)*
> - **Profiler in-engine**: Panel con timing por subsistema (Render / Physics / Scripts / Animation / Audio). GPU markers básicos. Frame graph (últimos N frames como histograma). N configurable. Tracy ya integrado desde F2H2.
> - **Stats overlay**: overlay con FPS, drawcalls, triángulos, mem GPU/CPU, lights activas, entities. Cada widget toggleable desde Preferences. Estilo Quake `r_speeds`.

**Mecánica del editor:** el dev abre un panel "Profiler" para ver dónde se está gastando el frame (ej. "Render::Materials = 8 ms / Physics = 2 ms / Scripts = 1 ms"). Y un overlay siempre-visible en el viewport con FPS + drawcalls + tris (estilo Quake `r_speeds`) que el dev puede toggle desde Preferences.

---

## Scope candidato

### Profiler in-engine

1. **Reusar `MOOD_PROFILE_SCOPE`** (ya en uso pasa F2H2 con Tracy macros). Pero Tracy es app externa — necesitamos buffer in-engine paralelo para mostrar en panel ImGui.
2. **Ring buffer in-engine** de N frames (default 240 ≈ 4 segundos a 60fps) con timings por scope key (string hash).
3. **Panel UI** con tabla `Scope | Avg | Min | Max | %frame` + histograma de los últimos N frames.
4. **GPU markers** opcionales — `glPushDebugGroup`/`glPopDebugGroup` ya están en el SceneRenderer; queryObject GL_TIME_ELAPSED para medir GPU time por pass.

### Stats overlay

1. **Toggles** en `UserSettings.editor.stats.{fps, drawcalls, tris, memGpu, memCpu, lights, entities}` (7 bools, default `fps=true` los demás false).
2. **Overlay** flotante esquina superior-izquierda del viewport con cada widget visible si el flag está ON.
3. **Source de las métricas**: `SceneRenderer::frameStats()` (drawcalls/tris ya existe desde F2H2), `FpsCounter::tick`, OpenGL queries para mem GPU, `getCurrentRSS()` para CPU.

### Persistencia

- `UserSettings.editor.statsOverlay.*` (struct nested con 7 bools).
- `UserSettings.editor.profilerFrameCount` (int, default 240, clamp [60, 1200]).

### Tests

- StatsOverlay roundtrip (7 bools).
- ProfilerFrameCount sanitize.
- Ring buffer del profiler (push N+1 frames, validar wraparound).

---

## Decisiones a tomar al arrancar

1. **Profiler ring buffer**: ¿per-thread o single? Single es más simple, Tracy ya tiene per-thread vía macros.
2. **Stats overlay always-visible vs toggleable global**: ¿una key (F11?) togglea todo el overlay, o cada widget tiene su switch independiente en Preferences?
3. **GPU markers**: ¿default ON o opt-in en Preferences? GL_TIME_ELAPSED tiene costo no trivial (driver sync).
4. **Estilo del overlay**: Quake `r_speeds` (multiline text esquina) vs Unreal (chips con valores numericos) vs Unity (single bar al pie).

---

## Lo que NO toca F3H22

- F3H23+ (Console / Toasts / Crash recovery): hitos propios.
- Profiler con histograma de GPU stalls / pipeline analysis: out-of-scope (es trabajo de Tracy/RenderDoc).
- Per-entity profiler (ms por entity): out-of-scope, requiere overhead grande.
- Properties Editor con icons laterales tipo Blender: hito propio post-F3H22 si emerge antes de F3H24.
