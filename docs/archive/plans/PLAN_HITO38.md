# Plan — Hito 38: Save/Load + Main Menu (cerrado)

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 38 cerrado).

---

## Estado

**Hito 38 cerrado** (`v0.38.0-hito38`, suite **305/5947**). Primer hito que abre demos con progresión: el `MoodPlayer` arranca con un main menu y permite save/load del gameplay state.

## Bloques cerrados

- [x] **Bloque A — Save/Load del gameplay state**: nuevo módulo `engine/saving/SaveLoad.{h,cpp}` con `save(SaveData, path)` y `load(path) → optional<SaveData>`. `SaveData` persiste: `mapPath` (path lógico al `.moodmap` activo), `hud` (hp + ammo), `playerPosition`, `playerYaw`, `playerPitch`. Schema v1 JSON, validado en load (versión, malformado, campos faltantes → defaults). Sin persistir snapshots de Dynamic bodies para v1 (respawnean en su Transform original).
- [x] **Bloque B — Main menu del MoodPlayer**: nuevo flag `m_inMainMenu = true` al arranque. `drawMainMenu()` overlay translúcido con 3 botones: **New Game** (reset GameState + flag false), **Load Game** (`pfd::open_file` filtrando `.moodsave` → `applyLoadedSave()`), **Quit** (cierra app). Durante el menu, todos los updates del juego (camera, physics, scripts, animation, nav, particles) están skipped — el viewport queda como wallpaper del último frame. Flag `gameUpdating` propagado a todos los sistemas. F5 = quicksave a `<exeDir>/quicksave.moodsave` (sin dialog). `GameOverlay` "Salir al menu" en lugar de "Salir del juego" — Quit definitivo solo desde el main menu.
- [x] **Bloque C — Tests + cierre**:
  - 5 tests nuevos en `test_save_load.cpp`: round-trip básico + load de archivo inexistente + load de JSON malformado + load de versión futura + campos faltantes con defaults.
  - Suite total **305/5947** (antes Hito 37 cerrado: 300/5927).
  - Tag `v0.38.0-hito38`.

## Lo que NO se cubrió (para Hito 39 = cierre v1.0.0)

- Polish reactivo del Hito 37: cone particle shape, raycast layer/tag genérico, OBB trigger, setter runtime de friction.
- Tag final `v1.0.0` que cierra la Fase 1 del proyecto.

---

## Decisiones tomadas

Las nuevas en `DECISIONS.md` (2026-05-02):
- `.moodsave` separado del `.moodmap`: state runtime aparte del nivel design.
- Main menu con `m_inMainMenu` flag + `gameUpdating` propagado a todos los sistemas (en lugar de un GameMode enum).
- F5 = quicksave silencioso sin dialog para v1.

---

## Verificación visual del dev (criterios cumplidos)

- Doble-click a `MoodPlayer.exe` → ventana con fondo oscuro + menu modal "MoodEngine" con 3 botones.
- "New Game" → resetea HUD + entra al juego con char en spawn default.
- "Load Game" → file dialog filtra `.moodsave`. Eligiendo uno valido, hud + char position + camera orientation se restauran.
- F5 durante el juego → log `[engine] [info] Quicksave OK -> ...`.
- "Salir al menu" del pause overlay → vuelve al menu sin cerrar la app.
- Suite 305/5947 sin regresiones.
