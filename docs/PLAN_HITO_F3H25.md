# PLAN F3H25 — Crash recovery + autosave

**Estado:** ✅ **CERRADO** (2026-05-28) — tag `v2.25.0-fase3-hito25`. Implementación lineal con 4 decisiones cerradas pre-implementación (vía AskUserQuestion al arrancar) + un debug largo del lanzamiento del editor (causa real: deploy stale de shaders `thumbnail_bg.*` en el sandbox).
**Predecesor:** F3H24 (Console + Toasts).
**Origen:** `PLAN_FASE3.md` Sub-fase 3.4 (cerraba originalmente la Fase 3; al cerrarlo el dev solicitó insertar F3H26 polish UX + F3H27 follow-ups antes de Fase 4).

---

## Avance de Sub-fase 3.4 (post-F3H24)

```
F3H20 – ✅ Snapping configurable Hammer-style
F3H21 – ✅ Viewport pro: numpad views + 4 render modes
F3H22 – ✅ Properties Editor con icons laterales (Blender style)
F3H23 – ✅ Performance feedback: Profiler + Stats overlay
F3H24 – ✅ Comunicación al dev: Console + Toasts
F3H25 – ✅ Crash recovery + autosave ⬅ este plan
F3H26 –  — Polish UX del editor (post-F3H25)
F3H27 –  — Grupos + Map Tools integrados + mundo grande + HDRI dinámico
```

---

## Norte

`PLAN_FASE3.md` Sub-fase 3.4 declaraba originalmente F3H25 como cierre de Fase 3:
> **F3H25 — Crash recovery + autosave.**
> Autosave del `.moodmap` cada N minutos (configurable en Preferences, default 5). On crash, al reabrir el proyecto → modal "Recuperar última sesión?". Lock file que detecta crashes.

**Mecánica:** el dev abre un proyecto → el editor escribe un lock file con su PID. Cada N min, si el mapa está dirty, el autosave escribe `.autosave/<map>.moodmap` con un rename atómico. Al cerrar limpio: lock + autosave borrados. Si el editor crashea: el lock queda huérfano (PID muerto en el SO). Al reabrir el proyecto, si detecta lock huérfano + autosave reciente, modal "¿Restaurar?".

---

## Decisiones cerradas (pre-implementación)

Cerradas al arrancar vía AskUserQuestion. Cero ajustes reactivos post-validación.

### D1 — Ubicación del autosave: subcarpeta oculta `.autosave/`
Confirmado por el dev (vs sufijo `level1.moodmap.autosave` / `%LOCALAPPDATA%`).
- Path: `<projectRoot>/.autosave/<mapname>.moodmap`.
- Ventajas: limpio (no contamina la lista de mapas), fácil de `.gitignore`, no abandona el proyecto si se mueve a otra máquina.
- Patrón tipo Unity (`Library/AutoSave/`) y JetBrains (`.idea/.autosave/`).

### D2 — Trigger: solo si dirty + N min
Confirmado (vs "siempre cada N min" / "dirty + N min de idle").
- Timer global que tickea cada frame con dt.
- Si pasaron `intervalMin × 60_000 ms` Y `m_projectDirty == true` → write.
- Si dirty=false al cumplir N min → reset timer silencioso (no spam de check).
- Sin idle tracking (overkill para v1).

### D3 — Recovery UX: modal blocking al abrir proyecto
Confirmado (vs toast con botón / auto-cargar sin preguntar).
- Al abrir proyecto: `LockFile::check` → si Orphaned + autosave más reciente que canónico → set `m_recoveryModalPending=true`.
- En el próximo frame, `processRecoveryModal()` abre popup ImGui blocking centrado.
- Botones "Restaurar" (carga autosave + marca dirty) / "Descartar" (borra autosave).
- Modal blocking porque el aviso no se debe perder accidentalmente.

### D4 — Lock file format: JSON con PID + timestamp + version
Confirmado (vs touch / PID solo).
- Path: `<projectRoot>/.moodproj.lock`.
- Contenido: `{"pid": 12345, "started_at": "2026-05-28T15:32:10Z", "engine_version": "v2.24.x"}`.
- Detección de PID huérfano:
  - Windows: `OpenProcess(SYNCHRONIZE, ...) + WaitForSingleObject(0)` — distingue "PID libre" de "PID zombie".
  - POSIX: `kill(pid, 0)` con `errno == ESRCH`.
- Si el PID guardado == PID actual (caso raro: misma sesión reabriendo) → tratado como Clean (no auto-recovery).

---

## Implementación

### Engine (core)
- `src/core/UserSettings.{h,cpp}`: 2 fields nuevos en `EditorSettings`:
  - `autosaveEnabled` (bool, default `true`) — gate global.
  - `autosaveIntervalMin` (int, default `5`, clamp `[1, 60]` en `fromJson`).
- Persistencia: solo se escribe en `editorSettingsToJson` si el field difiere del default.

### Editor (módulos nuevos)
- `src/editor/application/LockFile.{h,cpp}`:
  - `Status check(projectRoot)` — Clean / Orphaned / InUse.
  - `bool acquire(projectRoot)` — escribe JSON con PID + timestamp ISO8601 + engine version.
  - `void release(projectRoot)` — idempotente (no error si no existe).
  - `int currentPid()` + `bool isProcessAlive(int pid)` — helpers exportados para tests.
- `src/editor/application/Autosave.{h,cpp}`:
  - `setup(projectRoot, mapRelPath, writeFn, dirtyFn)` — callbacks inyectados (no acopla a `EditorApplication`).
  - `tick(dtMs)` — incrementa timer; si `prefs.autosaveEnabled && timerMs ≥ intervalMs && dirtyFn()` → ejecuta `writeFn(targetPath.tmp)` + `std::filesystem::rename(tmp, final_)`.
  - `clearOnDisk()` — borra autosave + tmp. Llamado tras Save manual + Open Map + Close Project + dtor.
  - `autosaveMoreRecentThanCanonical()` — compara mtimes vía `last_write_time`.
- `src/editor/application/EditorApplication_RecoveryModal.cpp` NUEVO (sigue split pattern de `_Init` / `_Run` / `_FileIO`):
  - `processRecoveryModal()` — popup ImGui blocking centrado (440×auto), botones "Restaurar" / "Descartar".
  - "Restaurar": `SceneSerializer::load(autosavePath) + rebuildSceneFromMap + applyEntitiesToScene + ensureEnvironmentExists + applyEnvironmentFromScene` + `m_projectDirty = true` (canónico stale).

### Editor (cableado en EditorApplication)
- `tryOpenProjectPath`: tras setear `m_project`, `LockFile::check` + `acquire` + `m_autosave.setup(...)`. Si `Orphaned && autosaveMoreRecentThanCanonical()` → `m_recoveryModalPending = true`.
- `handleSave`: tras `ProjectSerializer::save` OK → `m_autosave.clearOnDisk()` (canónico ya safe).
- `handleSaveMapAs`: tras éxito → `clearOnDisk` + re-`setup` con el nuevo `currentMapPath`.
- `handleOpenMap`: tras cargar → `clearOnDisk` (autosave del mapa viejo) + re-`setup` con nuevo path.
- `handleCloseProject`: `LockFile::release` + `m_autosave.clearOnDisk` + `teardown`.
- `~EditorApplication`: si `m_project.has_value()` → `release + clearOnDisk + teardown` (cierre limpio salvaguarda).
- `tickFrameMetrics`: `m_autosave.tick(dtD * 1000.0f)` cada frame.
- `pumpUiRequests`: primera línea = `processRecoveryModal()` (consume el flag).

### UI (UserPreferencesPanel)
- Nueva sección "Autosave del mapa" con `Checkbox` enabled + `SliderInt` interval `[1, 60] min` + reset button.

### CMake
- `mood_engine_lib`: no toca (LockFile + Autosave + Recovery viven en `MoodEditor`).
- `MoodEditor`: agregados `Autosave.cpp`, `EditorApplication_RecoveryModal.cpp`, `LockFile.cpp`.
- `mood_tests`: agregado `src/editor/application/LockFile.cpp` + `tests/test_lock_file.cpp`.

### Tests
- `test_user_settings_editor.cpp`: 4 nuevos F3H25 (defaults, toJson omit, clamp `[1,60]`, roundtrip).
- `test_lock_file.cpp` NUEVO: 8 cases (check sin lock → Clean; acquire crea archivo; release idempotente; PID huérfano → Orphaned; JSON malformado → Clean; JSON sin pid → Clean; PID propio = alive; PID inválido = dead).
- Suite **1265 → 1277 cases** (+12 cases / +23 asserts vs F3H24).

### i18n
- 11 keys nuevas (es + en):
  - 5 para Preferences > Autosave section: `section`, `enabled`, `enabled.tooltip`, `interval_min`, `interval_min.tooltip`.
  - 3 toasts: `autosaved`, `session_recovered`, `session_recovery_failed`.
  - 4 modal recovery: `title`, `body`, `restore`, `discard`.

---

## Ajustes reactivos post-validación

### Crash del lanzamiento del editor (deploy stale)
Al lanzar el editor por primera vez tras F3H25, salía con `Fatal: <garbage>` (1 char de basura). Log se cortaba en "Shader compilado: shaders/pbr.vert + shaders/pbr.frag" tras `AudioDevice init`.

**Causa real (no F3H25):** `cmake --build --target MoodEditor` NO ejecuta el target `mood_runtime_files ALL` que copia shaders/assets a `build/.../shaders/`. Los shaders `thumbnail_bg.vert/frag` (necesarios para `MaterialPreviewRenderer`) no estaban deployados. El ctor de `MaterialPreviewRenderer` lanzaba `runtime_error` al fallar a abrir el shader; `e.what()` retornaba un puntero a string ya liberada (stack-corruption pattern) → "Fatal: <garbage>".

**Fix:** rebuild explícito del target `mood_runtime_files`. Sin cambios de código.

**Lección para `feedback_build_validar_siempre`:** al validar visualmente tras agregar **nuevos shaders / assets**, usar siempre `cmake --build --target mood_runtime_files` (no solo `MoodEditor`). Si solo se modificó código C++ sin nuevos assets, `--target MoodEditor` alcanza.

---

## Backlog del hito (no cerrado en F3H25)

- **Asset import / shader compile toasts** — herencia de F3H24 (sin `AssetImporter` centralizado).
- **Concurrent-editors warning** — hoy `Status::InUse` se trata como Clean (overwrite). Si dos instancias abren el mismo proyecto en paralelo, la 2da pisa el lock. Soporte real = hito propio.
- **Autosave incremental** — hoy reescribe el `.moodmap` completo cada N min. Para mapas grandes podría ser overhead; diferido a Fase 4 si emerge.
- **Recovery modal con preview** — mostrar diff "última sesión vs canónico" antes de decidir (similar a Git mergetool). Hito propio si emerge demanda.
- **Lock con file lock OS** — `flock`/`LockFileEx` en lugar de solo PID file. Más robusto contra carreras concurrentes, pero hoy no las tenemos.

---

## Lo que NO toca F3H25

- F3H26 (polish UX del editor) — hito siguiente, separado.
- F3H27 (grupos + tools integrados + mundo grande + HDRI dinámico) — stub, abre después de F3H26.
- Crash recovery del editor mismo (recuperar imgui_layout post-crash) — out-of-scope, el `imgui_layout_vN.ini` ya persiste por config.
- Multi-undo del recovery — al restaurar autosave, el HistoryStack queda limpio (no se intenta recuperar comandos pre-crash).
