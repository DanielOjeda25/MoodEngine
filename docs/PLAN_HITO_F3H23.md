# PLAN F3H23 — Performance feedback: Profiler + Stats overlay

**Estado:** ✅ **CERRADO** (2026-05-28) — tag `v2.23.0-fase3-hito23`. Implementación en 2 pasos (StatsBar primero, ProfilerBuffer + Panel después) con 1 ronda de polish reactivo del dev (overlay del viewport → StatusBar inferior por superposición visual con Asset Browser).
**Predecesor:** F3H22 (Properties Editor con icons laterales — Blender style).
**Origen:** `PLAN_FASE3.md` Sub-fase 3.4 (consolidado ex-F3H23 Profiler + ex-F3H24 Stats overlay en uno solo, ver `PLAN_FASE3.md`). Renumerado de F3H22 → F3H23 en reorden 2026-05-27 por insertar F3H22 (Properties Editor).

---

## Avance de Sub-fase 3.4 (post-F3H21, reorden 2026-05-27)

```
F3H20 –  ✅ Snapping configurable Hammer-style
F3H21 –  ✅ Viewport pro: numpad views + 4 render modes
F3H22 –  — Properties Editor con icons laterales (Blender style)
F3H23 –  — Performance feedback: Profiler + Stats overlay ⬅ este plan
F3H24 –  — Comunicación al dev: Console + Toasts
F3H25 –  — Crash recovery + autosave (cierra Fase 3)
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

## Decisiones cerradas + ajustes reactivos

### D1 — Estilo overlay: Unity bottom bar (single line al pie del editor)
Confirmado por el dev al arrancar: `Unity bottom bar (compacto)` vs Quake/Unreal. Implementado en la **StatusBar global del editor** (la barra que ya tenía FPS + Modo + Proyecto), no como sub-window overlay del viewport. Razón: ahorra espacio horizontal de la imagen del viewport y centraliza todos los "indicadores globales del editor" en una sola línea siempre visible.

### D2 — Stats overlay toggleable per-widget (no all-or-nothing)
Cada chip tiene su propio bool en `UserSettings.editor.statsOverlay.show<X>` (7 flags). Defaults: `showFps`/`showDrawcalls`/`showTris` = true (las 3 métricas más usadas day-to-day); `showMemGpu`/`showMemCpu`/`showLights`/`showEntities` = false. El dev arma su HUD desde Preferences > Editor > "Stats overlay del viewport". `anyEnabled()` helper en el struct.

### D3 — Profiler ring buffer single-thread (no per-thread)
Tracy ya maneja per-thread vía sus macros internas (`ZoneScopedN` captura el thread ID). El ring in-engine de F3H23 vive en el main thread del editor — donde corren todos los `MOOD_PROFILE_SCOPE` existentes (~50 call-sites en SceneRenderer/SceneSerializer/AssetManager/Editor). Mantener single-thread elimina contención de locks en hot path. Si emerge demanda (futuro worker thread del asset import o physics step async), se agrega per-thread map por `std::thread::id` en hito propio.

### D4 — Hook al ring vía RAII en `MOOD_PROFILE_SCOPE` (no macro nueva)
La macro existente se extendió para emitir Tracy zone **+** RAII `Mood::detail::ScopeTimer` que pushea al `ProfilerBuffer` global en su destructor. Beneficio: los ~50 scopes ya instrumentados alimentan el profiler in-engine automáticamente, sin tocar call-sites. Cuando `TRACY_ENABLE=OFF` (release sin profiling), el RAII sigue corriendo — el ProfilerPanel funciona aunque Tracy esté apagado. Cuando `MOOD_PROFILE=OFF` en CMake, las macros caen a no-op total (zero overhead).

### D5 — GPU markers GL_TIME_ELAPSED OUT-OF-SCOPE en F3H23
Por ahora el ProfilerPanel muestra **CPU time** únicamente. `glBeginQuery(GL_TIME_ELAPSED)` requiere driver sync per-query que tira el FPS si se mide cada pase del pipeline. Si emerge demanda (típicamente al optimizar shadow / SSAO), se agrega en hito propio con muestreo throttled (1/30 frames p.ej.).

### D6 — VRAM via NVX_gpu_memory_info (NVIDIA-only por ahora)
`GL_NVX_gpu_memory_info` está disponible en drivers NVIDIA y reporta `TOTAL_AVAILABLE - CURRENT_AVAILABLE` (en KB) = usado por el proceso. Sin la extensión (AMD/Intel/Mesa) el helper devuelve 0 y el chip muestra "—" sin fallar. AMD tiene `GL_ATI_meminfo` (similar pero distinta semántica); se agrega cuando un dev con AMD reporte la falta. Mac/Linux: equivalentes futuros con `MTLDevice.currentAllocatedSize` / `dri3` query.

### Ajuste reactivo A — Overlay del viewport → StatusBar inferior
Primera implementación dibujaba un single-line chip en el centro-pie del viewport image (Unity-style absoluto). El dev reportó superposición visual con el header del Asset Browser cuando ambos paneles coincidían en altura. Refactor: mover los chips a la `StatusBar` global del editor (la barra que ya tenía FPS + Modo + Proyecto). Beneficio adicional: la StatusBar es global (no por-viewport), siempre visible aunque el dev cierre el viewport o cambie de workspace.

### Ajuste reactivo B — `kLabelColumnWidth` 160 → 240 en User Preferences > Editor
Los labels largos en español ("Tamaño gizmo (mover/escalar)", "Retraso preview al pasar el cursor") pisaban la columna del slider con el ancho original de 160 px. Subido a 240 px — cabe holgado en el modal de 540 px de ancho. Polish menor, registrado acá porque se descubrió validando los toggles de F3H23.

---

## Backlog del hito (no cerrado en F3H23)

- **GPU markers** vía `glBeginQuery(GL_TIME_ELAPSED)` para medir tiempo por pase del SceneRenderer (Shadow / SSAO / Bloom / SSR / PBR static / PBR skinned). Toggle en Preferences "GPU markers (cuesta ~5% FPS)". Hito propio cuando el dev quiera optimizar el render pipeline.
- **VRAM AMD** vía `GL_ATI_meminfo`. Trivial agregar al helper actual (~10 LOC). Diferido hasta que un dev con AMD reporte "—" en VRAM.
- **Per-scope histograma** en el ProfilerPanel (hover sobre una fila → mini-chart de los últimos N samples de ese scope). Hoy el histograma muestra solo el último frame. Hito propio si el dev quiere tendencias visuales por scope.
- **Export CSV del Profiler** para análisis externo. Snapshot a `<project>/.cache/profiler/snapshot_<timestamp>.csv`. Gemelo del PerformanceHud snapshot existente (F2H2 Bloque G).
- **Profiler con marcadores de eventos** (asset load, scene save, etc.) como overlay sobre el histograma. Útil para correlacionar spikes con acciones del dev.

---

## Lo que NO toca F3H23

- F3H24+ (Console + Toasts / Crash recovery): hitos propios.
- Profiler con histograma de GPU stalls / pipeline analysis: out-of-scope (es trabajo de Tracy/RenderDoc).
- Per-entity profiler (ms por entity): out-of-scope, requiere overhead grande.
