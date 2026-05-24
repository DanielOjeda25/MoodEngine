# PLAN F3H7 — Migración bucket `UserSettings > Editor` (sensibilidades + zoom + thresholds)

**Estado:** Planeado (séptimo y último hito de Sub-fase 3.1, arranca tras `v2.6.0-fase3-hito6`).
**Predecesor:** F3H6 (migración Snap + popover MapEditorTopBar — confirma que la UI puede vivir fuera de Project Settings cuando hace sentido).
**Origen:** Bucket 8 del audit F3H3 ([`docs/HARDCODED_AUDIT.md`](HARDCODED_AUDIT.md)) — **media prioridad**. Es el primer (y único) bucket de **per-instalación** (no `.moodproj`); los settings personales del dev viajan con su instalación del editor, no con cada proyecto.

---

## Qué siente el usuario

El dev abre `Edit > Preferences...` (el `UserPreferencesPanel` de F3H2 con la sección "General"). Aparece una **nueva sección "Editor"** con:

- **Zoom inicial ortho** — slider con el alto default del frustum (default 32 u). Mundo abierto = subir; interior = bajar.
- **Velocidad de zoom (wheel)** — slider 1.05x → 1.5x, default 1.1x. Cuán agresivo es cada tick del wheel sobre la cámara orto.
- **Tamaño del gizmo** — slider 30 → 120 px, default 60 (brazos translate/scale) + 55 (rotate ring). Pantallas 4K vs 1080p quieren distinto tamaño.
- **Umbral click vs drag** — slider 2 → 32 px, default 16 ortho / 4 perspectiva. Cuánto puede moverse el mouse antes de que el click se vuelva drag.

Estos valores **viajan con la instalación**, no con el proyecto. Un dev en una notebook con touchpad puede subir el threshold para que sus clicks no degeneren en drag accidental; otro dev en mouse con sensor 16000 DPI lo baja. Cada uno tiene su preferencia y la conserva al cambiar de proyecto.

---

## Realidad técnica (qué sí / qué no)

**Sí en F3H7:**
- Struct nested `EditorSettings` en `UserSettings` con 5-7 fields:
  - `f32 orthoInitialZoom` (default 32.0 — alto del frustum en world units).
  - `f32 orthoZoomFactor` (default 1.1 — multiplicador per wheel tick).
  - `f32 gizmoArmLengthPx` (default 60.0 — translate/scale).
  - `f32 gizmoRotateRadiusPx` (default 55.0).
  - `i32 clickDragThresholdOrthoPx` (default 16).
  - `i32 clickDragThresholdPerspectivePx` (default 4).
- `toJson`/`fromJson` defensivos (mismo patrón que F3H4-F3H6, sin bump schema `settings.json`).
- Sección "Editor" en `UserPreferencesPanel` con `SeparatorText` + sliders + reset buttons (helper `resetButton<T>` ya disponible de F3H4).
- Reads LIVE en los 5 call-sites del audit (OrthoCamera + EditorOverlay_Gizmo + ViewportPanel + OrthoViewportPanel).
- ~10-12 keys i18n bajo `editor.user_preferences.editor.*`.
- Tests: defaults, non-default, roundtrip, back-compat (`settings.json` pre-F3H7 sin subkey `editor` → defaults).

**NO en F3H7:**
- Shortcuts/keymap — no es parte del audit bucket 8, requiere UI de captura de keystrokes + conflict detection (hito propio post-Sub-fase 3.1 si emerge demanda).
- Autosave interval — no estaba en el audit; hoy no hay autosave (memoria proyecto: `feedback_no_autoopen_project`, el dev arranca con Welcome modal siempre — autosave sería ortogonal).
- Theme/idioma — ya están en F3H2 sección "General".
- Inspector HUD timers (bucket 9 del audit, baja prioridad) — diferido.
- Vehicle/Quality buckets — diferidos a Sub-fase 3.2+.

---

## Bloques

### A — Extender `UserSettings` con `EditorSettings`
- Struct nested con 6 fields f32/i32.
- `toJson`: solo escribir subobject `"editor"` si algún field difiere del default.
- `fromJson`: leer + clamp a rangos sanos (zoom factor > 1.0, thresholds >= 1, gizmo px > 0).
- Sin bump de schema `settings.json` (mismo patrón que F3H4 con `.moodproj`).

### B — Tests
- 5 tests en `test_user_settings.cpp` (crear archivo nuevo si no existe — UserSettings hoy no tiene tests porque escribe a APPDATA real; usar path inyectable via test helper o un wrapper de mock — ver decisión D1 abajo):
  - Defaults → no subobject "editor".
  - Custom values → roundtrip preserva los 6 fields.
  - Back-compat: `settings.json` sin subkey "editor" → defaults.
  - Malformed (`"editor": "garbage"`) → defaults silencioso.
  - Out-of-range (zoom factor 0.5) → clamp a min sano.

### C — Sección "Editor" en `UserPreferencesPanel`
- `SeparatorText("editor.user_preferences.editor.section")` debajo de "General".
- 4-6 `SliderFloat`/`SliderInt` con tooltips i18n por field.
- Reset buttons (`resetButton<T>` helper ya existe).
- **Cambios aplican LIVE** (mismo patrón F3H2 — `UserSettings::save()` por cambio + los reads ya son live).

### D — Lecturas en los 5 call-sites
- `OrthoCamera.h:37` → `UserSettings::editor().orthoInitialZoom` (en el ctor o init de cámara).
- `OrthoCamera.h:114` → `UserSettings::editor().orthoZoomFactor` (en el método `zoom`).
- `OrthoCamera.h:39-40` (límites min/max zoom) → **NO migra**: son safety bounds del editor, no preferencia user.
- `EditorOverlay_Gizmo.cpp:57,58,76` → reads de `UserSettings::editor().gizmoArmLengthPx` / `gizmoRotateRadiusPx`.
- `ViewportPanel.h:200` → `UserSettings::editor().clickDragThresholdPerspectivePx`.
- `OrthoViewportPanel.cpp:217` → `UserSettings::editor().clickDragThresholdOrthoPx`.

### E — i18n + asset sync
- 10-12 keys nuevas (section + label + hint por field). Sync a build dir.

### F — Validación visual
- Sección "Editor" muestra los sliders + reset buttons.
- Mover gizmo size slider → gizmo cambia en vivo en el viewport.
- Mover threshold click/drag → click ligero no debe disparar drag.
- Cerrar editor + reabrir → values persisten.
- Cambiar de proyecto → values siguen iguales (per-instalación confirmado).

---

## Decisiones a tomar (pre-implementación)

### D1 — UserSettings sin tests vs UserSettings con tests inyectables

**Contexto:** F3H2 explícitamente NO agregó tests porque `UserSettings` escribe a `%APPDATA%\MoodEngine\settings.json` real (test contaminaría el state del dev). F3H7 agrega un struct nested con sanitize logic — testear sería deseable. Decisión documentada en F3H2 cierre: *"si el módulo crece (F3H6 shortcuts, F3H7 autosave/font/density), refactorear ahí con tests aislados"*.

**Opciones:**
- **(a)** Sin tests, validación visual como F3H2. Simple, sin refactor.
- **(b)** Refactor `UserSettings` para aceptar path inyectable (default APPDATA pero override en tests). Tests aislados con tmpfile.
- **(c)** Mantener APPDATA pero los tests del struct nested operan sobre `nlohmann::json` puro (testean `EditorSettings::toJson`/`fromJson` aislados del filesystem).

**Recomendación:** **(c)** — split la responsabilidad: `UserSettings::editor()` accessor sigue leyendo de APPDATA, pero `EditorSettings::toJson`/`fromJson` son funciones libres testeables. Tests cubren sanitize/roundtrip/back-compat sin tocar disco. Es el patrón que ya funcionó con `GameplaySettings`/`CharacterSettings`/`SnapSettings` en `test_project_settings.cpp`.

### D2 — Live reads vs snapshot al startup

**Contexto:** F3H4/F3H5/F3H6 leen LIVE cada frame de `m_project->settings.*` (el dev edita y siente al próximo tick). UserSettings tiene un patrón distinto: `UserSettings::language()` se lee al startup y los listeners notifican via callbacks (idioma + tema están así desde F2H76/F2H43).

**Decisión:** seguir el patrón LIVE — `UserSettings::editor().xxx()` se llama cada frame en los 5 call-sites. Es 5 lookups a un `std::optional` cacheado, costo negligible. Beneficio: el dev mueve el slider y ve el gizmo cambiar de tamaño en vivo (mismo "feel" que F3H4/F3H5/F3H6).

### D3 — Sección "Editor" en el UserPreferencesPanel vs panel/ventana nueva

**Contexto:** F3H2 dejó el `UserPreferencesPanel` con UNA sola sección "General". F3H7 agrega la segunda. Opciones: agregar como segunda `SeparatorText` o introducir TabBar (Performance/Gameplay/Character en Project Settings tienen TabBar desde F3H4).

**Decisión:** **TabBar** (consistente con `ProjectSettingsPanel`). Tab 1 "General" (Tema + Idioma — lo de F3H2), Tab 2 "Editor" (lo de F3H7). Si Sub-fase 3.2+ agrega más secciones (autosave, shortcuts) tienen su tab propio.

**Alternativa descartada:** SeparatorText apilado. Simple pero no escala — Unity/Unreal tienen Preferences como ventana con tabs/categorías a la izquierda, no como scroll vertical infinito.

### D4 — Click vs drag threshold separado ortho/perspectiva

**Contexto:** el audit bucket 8 reporta **dos** thresholds distintos: 16 px ortho (OrthoViewportPanel) y 4 px perspectiva (ViewportPanel). Decisión: ¿unificar o mantener separados?

**Decisión:** **mantener separados**. Razones:
- El default de 16 px ortho es porque el orto tiene cursor más grueso (snap-to-vertex indicator). 4 px funcionaría pero el dev tendría que precisar más con el mouse.
- 4 px en perspectiva es porque el viewport 3D no tiene cursor especial, los clicks son sobre meshes y la precisión del mouse es la que manda.
- Son dos UX distintas, dos números distintos. Forzarlos a uno único genera fricción.

---

## Riesgos / a confirmar temprano

- **`UserSettings` con sub-structs** — F3H7 introduce el primer struct nested en `UserSettings`. Pattern: `m_editor` member del singleton + `editor()` getter const. Replica el patrón de `ProjectSettings::gameplay/character/snap` pero en otro singleton. Validar que `save()` siga atómico (write-temp + rename).
- **Reads en `OrthoCamera.h`** — la cámara es un struct usado por `OrthoViewportPanel`. Acceder a `UserSettings::instance()` desde un header de engine huele a layer violation. Decisión a tomar en bloque D: ¿pasar los valores al ctor de la cámara o leer en el método (`zoom`) en runtime?

**Recomendación temprana:** pasar al ctor + setters del editor (sigue el patrón de `EditorScene::headbob*` que recibe el valor de afuera). Engine no depende de UserSettings.

---

## Tamaño estimado

Hito chico-mediano (~3-4h, similar a F3H5/F3H6). El struct + tests + UI es trabajo conocido; lo nuevo es el wiring desde UserSettings (singleton) a los call-sites de engine sin layer violation.

## Cierre del hito

- [ ] Suite verde (+5 tests nuevos).
- [ ] Sección/tab "Editor" en `UserPreferencesPanel` con 4-6 sliders + reset buttons.
- [ ] Cambios en vivo: mover slider → gizmo cambia tamaño, viewport ortho cambia zoom factor.
- [ ] Persistencia en `%APPDATA%\MoodEngine\settings.json`.
- [ ] Validación visual: cerrar+reabrir editor preserva values, cambiar de proyecto preserva values.
- [ ] Tag `v2.7.0-fase3-hito7`.
- [ ] Update `ESTADO_ACTUAL.md`, `HITOS.md`, `DECISIONS.md`.
- [ ] **CIERRE de Sub-fase 3.1 "El editor te respeta"** — todos los buckets de UX/gameplay del audit migrados (Gameplay/Character/Snap → `.moodproj`, Editor → `UserSettings`). Próximos hitos abren Sub-fase 3.2 (Inspector + Hierarchy pulidos, F3H8-F3H13).
- [ ] Crear `PLAN_HITO_F3H8.md` (primer hito Sub-fase 3.2 — TBD según prioridad que el dev quiera atacar).
