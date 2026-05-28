# PLAN F3H24 — Comunicación al dev: Console mejorada + Toasts

**Estado:** ✅ **CERRADO** (2026-05-28) — tag `v2.24.0-fase3-hito24`. Implementación lineal en una sola tanda con 4 decisiones cerradas pre-implementación (vía AskUserQuestion al arrancar) — sin ajustes reactivos post-validación.
**Predecesor:** F3H23 (Performance feedback: Profiler + Stats overlay).
**Origen:** `PLAN_FASE3.md` Sub-fase 3.4 (consolidado ex-F3H25 Console + ex-F3H26 Toasts en uno solo — comparten la fuente de log severity pipeline). Renumerado F3H23 → F3H24 en reorden 2026-05-27 por insertar F3H22 (Properties Editor).

---

## Avance de Sub-fase 3.4 (post-F3H21, reorden 2026-05-27)

```
F3H20 –  ✅ Snapping configurable Hammer-style
F3H21 –  ✅ Viewport pro: numpad views + 4 render modes
F3H22 –  ✅ Properties Editor con icons laterales (Blender style)
F3H23 –  ✅ Performance feedback: Profiler + Stats overlay
F3H24 –  ✅ Comunicación al dev: Console + Toasts ⬅ este plan
F3H25 –  — Crash recovery + autosave (cierra Fase 3)
```

---

## Norte

`PLAN_FASE3.md` Sub-fase 3.4 declara:
> **F3H23 — Comunicación al dev: Console mejorada + Toasts.**
> *(Une el ex-F3H25 Console + ex-F3H26 Toasts — comparten fuente, log severity pipeline. Toasts = snippet transitorio de la Console.)*
> - **Console panel mejorada**: filtros por severidad (info/warn/error/debug). Search box. Click en `file:line` salta al editor de scripts/material. Botón "Copy as bug report".
> - **Toasts no-modales**: notificaciones efímeras esquina inferior derecha: "Guardado", "Asset importado", "Shader compilado OK", "Project Settings actualizadas". Lifetime configurable. Estilo VSCode.

**Mecánica del editor:** el dev ve feedback visual inmediato de las acciones que ejecuta (guardar, abrir mapa, copiar bug report). La Console existente (F2H37 ya tenía filtros por severity + iconos + clear + auto-scroll) se extiende con search por mensaje, copy formateado al clipboard, y click-to-open `.lua` paths. El sistema de toasts nuevo emite chips efímeros en la esquina inferior derecha con slide-in + fade-out, estilo VSCode.

---

## Decisiones cerradas (pre-implementación)

Las 4 decisiones se cerraron antes de tocar código vía AskUserQuestion al arrancar el hito. Cero ajustes reactivos post-validación — el dev confirmó al cerrar con "todo ok".

### D1 — Estilo de los toasts: VSCode (bottom-right + slide-in)
Confirmado por el dev al arrancar (vs `Unity Editor: top-right + fade` / `centro inferior: fade`). Implementado en `ToastsOverlay` con:
- Posición: esquina inferior derecha, stack vertical (más nuevos abajo, push hacia arriba).
- Slide-in horizontal en los primeros **200 ms** con easing `easeOutCubic` (desacelera al llegar a posición final).
- Visible 100% mientras vida > 400 ms.
- Fade-out lineal en los últimos **400 ms**.
- Background color por severidad (azul=Info, verde=Success, ambar=Warn, rojo=Error) + icon FontAwesome.
- Ancho fijo 320 px para alineación visual; alto auto-resize al contenido (PushTextWrapPos para wrap del mensaje).

### D2 — Click en `file:line` del Console: solo `.lua` con sistema externo
Confirmado por el dev al arrancar (vs `No implementar en F3H24 (diferir)`). Implementado en `ConsolePanel`:
- Helper `findLuaPath()` detecta paths que contengan `.lua` (con o sin `:N` línea opcional) escaneando hacia atrás desde el match hasta un delimitador (`whitespace / ( / [ / ' / " / <`).
- Si encuentra path, agrega un `ImGui::SmallButton(ICON_FA_ARROW_UP_RIGHT_FROM_SQUARE)` al final de la línea del log.
- Click → `ShellExecuteA(nullptr, "open", filePath, ...)` — abre el archivo con el editor default del SO (VSCode si está asociado, sino notepad).
- El sufijo `:N` se trunca antes del ShellExecute (Windows no acepta `:N` en path).
- Out-of-scope: paths `.material` / `.json` / `.moodprefab` etc (extensión trivial si emerge demanda).

### D3 — Emisiones automáticas de toasts: save + asset import + shader compile + prefs update (4 sitios)
Confirmado por el dev al arrancar (vs `Solo errores + save (2)` / `Todo warn+err+critical automático`).

**Sitios efectivamente cableados en F3H24:**
1. **Save proyecto** (`handleSaveMap` en `EditorProjectActions_FileIO.cpp`) — Success "Proyecto guardado: <name>" tras `ProjectSerializer::save` OK; Error "Error al guardar: <msg>" en el catch.
2. **Save Map As** (`handleSaveMapAs` en `EditorProjectActions_Map.cpp`) — Success "Mapa guardado: <filename>".
3. **Open Map** (`handleOpenMap`) — Info "Mapa abierto: <filename>".
4. **Open Project** (`tryOpenProjectPath`) — Info "Proyecto abierto: <name>".
5. **Preferences saved** (`UserPreferencesPanel::onImGuiRender`) — tracking `m_changedSinceOpen` (setea true cuando hay `saveNow=true` en cualquier toggle/slider/reset/cambio de tema/idioma); al detectar transición visible: true→false con changes pendientes, emite Success "Preferencias guardadas".

**No cableados en F3H24, backlog explícito:**
- **Asset import** (texture/mesh/vehicle) — no hay un único call-site claro; el flujo va por copy a la carpeta del proyecto + rescan del AssetBrowser. Diferido a hito propio cuando se centralice un `AssetImporter`.
- **Shader compile** — el MaterialEditor + ShaderGraph compilan sin un único trigger explícito. Diferido.

### D4 — Console+ con search por mensaje (reemplaza filter por channel)
Decisión menor de implementación. El filter pre-F3H24 buscaba substring del **channel** (`engine`, `render`, `script`, etc); poco útil day-to-day. Lo reemplazamos por search **case-insensitive** sobre `text + " " + channel` (cubre ambos sin requerir prefijos). El campo `m_channelFilter` se renombra a `m_messageFilter` (64 chars en lugar de 32 — los keywords del bug suelen ser más largos). Key i18n `editor.panel.console.filter_hint` queda obsoleta pero preservada (forward-compat con el JSON viejo); nueva key `editor.panel.console.search_hint` con el texto correcto.

---

## Implementación

### Engine (core)
- `src/core/Toasts.{h,cpp}`: cola global thread-safe (mutex defensivo — hoy todo main thread pero cubrimos futuros worker threads). API: `push(severity, message, lifetimeMs)` + helpers `pushInfo/pushSuccess/pushWarn/pushError` + `snapshot()` (copy) + `tick(dtMs)` + `clear()` + `size()`. Estructura `Toast { severity, message, remainingMs, totalMs }`. Severity enum: Info / Success / Warn / Error.
- `UserSettings::EditorSettings` gana 2 fields:
  - `toastsEnabled` (bool, default true) — gate global; off = `push()` es no-op silencioso (logs siguen capturándose por LogRingSink).
  - `toastsLifetimeMs` (int, default 3000 ms ≈ Unity, sanitize clamp `[500, 10000]`).

### Editor (UI)
- `src/editor/ui/ToastsOverlay.{h,cpp}`: render del stack en esquina inferior derecha. Lifecycle:
  - `tick()` aging vive en `EditorApplication::tickFrameMetrics` (junto al FpsCounter).
  - `draw()` se llama desde `EditorUI::draw` después del Dockspace (por encima de todo el chrome).
  - Sub-window flags: `NoDecoration | NoDocking | NoMove | NoFocusOnAppearing | NoNav | NoInputs | NoSavedSettings | AlwaysAutoResize`.
  - `BgAlpha` y `StyleVar_Alpha` modulados por la curva slide+visible+fade.
- `ConsolePanel` extendido (`m_channelFilter` → `m_messageFilter` 64 chars, search case-insensitive sobre `text + " " + channel`, botón "Copy as bug report" que formatea filtrados al clipboard, detector `findLuaPath` + `SmallButton` con `ICON_FA_ARROW_UP_RIGHT_FROM_SQUARE` que dispara `ShellExecuteA`).
- `UserPreferencesPanel` extendido con `m_changedSinceOpen` + `m_wasVisibleLastFrame` para emitir toast al cerrar con cambios; nueva sección "Toasts (notificaciones)" con checkbox enabled + SliderInt lifetime + reset button.
- `IconsFontAwesome6.h`: nuevo glyph `ICON_FA_ARROW_UP_RIGHT_FROM_SQUARE` (0xF08E, dentro del rango FA cargado).

### CMake
- `mood_engine_lib`: agregado `src/core/Toasts.cpp`.
- `MoodEditor`: agregado `src/editor/ui/ToastsOverlay.cpp`.
- `mood_tests`: agregado `src/core/Toasts.cpp` (junto a UserSettings) + `tests/test_toasts.cpp`.

### Tests
- `test_user_settings_editor.cpp`: 6 cases F3H24 nuevos (defaults toasts, toJson omit defaults, toJson include only changed, roundtrip, clamp lifetime [500,10000], tipo invalido ignorado).
- `test_toasts.cpp` NUEVO: 7 cases (push+snapshot orden, tick decrementa+descarta, lifetime default desde UserSettings, lifetime explícito override, toastsEnabled=false → no-op, clear, tick con dt 0/negativo no-op).
- Suite **1265/11787 verde** (+13 cases / +36 asserts vs F3H23).

### i18n
- 16 keys nuevas (es + en):
  - 5 para Console+: `search_hint`, `copy_bug_report`, `copy_bug_report.tooltip`, `open_lua`, (y se preserva `filter_hint` viejo por back-compat).
  - 5 para Preferences > Toasts section: `section`, `enabled`, `enabled.tooltip`, `lifetime_ms`, `lifetime_ms.tooltip`.
  - 7 para mensajes de los toasts: `project_saved`, `project_save_error`, `project_opened`, `map_saved`, `map_opened`, `preferences_saved`, `console_copied`.

---

## Backlog del hito (no cerrado en F3H24)

- **Asset import** y **shader compile** toast emisiones — diferidos por falta de single call-site. Si en F3H25 (Crash recovery) se introduce un `AssetImporter` centralizado, agregar 1 línea. Mientras tanto, el dev ve el feedback vía status bar message + log de la Console.
- **Click `.lua:N` salta a línea N** — hoy el ShellExecute solo abre el archivo (sin línea). VSCode tiene `code -g file:line`; podríamos detectar VSCode instalado y usar ese binario en vez de `ShellExecute`. Hito propio si emerge demanda.
- **Click en paths `.material` / `.moodprefab` / `.json`** — extender el `findLuaPath` a un detector multi-extensión. Trivial.
- **Toasts con acción** (botones "Undo" / "Ver detalle" embebidos en el toast) — útil para "Project save failed: <error> [Reintentar]". Requiere refactor del overlay para callbacks. Hito propio si emerge demanda.
- **Dedup de toasts idénticos** — hoy 2 pushes de la misma message generan 2 chips. Convención VSCode/Mantine: refrescar el lifetime del existente. Hito propio si el dev nota spam.

---

## Lo que NO toca F3H24

- F3H25 (Crash recovery + autosave) — cierra Fase 3, hito propio.
- Persistencia del log entre sesiones — el LogRingSink es runtime-only.
- Toasts con HTML/markdown — texto plano único; los chips de VSCode son texto plano también.
- Toasts modales / con botones de acción — out-of-scope (ver backlog).
