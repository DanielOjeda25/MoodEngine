# PLAN F3H4 — Migración del bucket "Gameplay tier 1" a `.moodproj > Gameplay`

**Estado:** Planeado (cuarto hito de Sub-fase 3.1, arranca tras `v2.3.0-fase3-hito3`).
**Predecesor:** F3H3 (auditoría + catalogación de hardcoded values).
**Origen:** F3H3 catalogó ~22 hits a migrar repartidos en 10 buckets ([`docs/HARDCODED_AUDIT.md`](HARDCODED_AUDIT.md)). El bucket de alta prioridad y máximo impacto en feel es **Gameplay tier 1**: walk speed, crouch speed, jump velocity, jump cooldown. Además, F3H3 ya cerró parcialmente el bug de walk/crouch desync (Player↔Editor) unificando a 5.5/3.0 — F3H4 consolida eso al hacerlo editable desde un solo lugar.

---

## Qué siente el usuario

El dev abre el editor con un proyecto cargado. Va a `Edit > Project Settings...`. Hoy ve solo la sección "Performance" (Target FPS). En F3H4 ve una nueva sección **"Gameplay"** con 4 fields:

- **Walk speed** (m/s) — slider 1.0 → 12.0, default 5.5
- **Crouch speed** (m/s) — slider 0.5 → 6.0, default 3.0
- **Jump velocity** (m/s) — slider 1.0 → 15.0, default 5.5
- **Jump cooldown** (s) — slider 0.0 → 1.0, default 0.2

Cambia walk speed a 7.0 (estilo Doom Eternal). Cambia jump a 7.5 (más alto). Salva el proyecto (`Ctrl+S`).

Entra a PlayInEditor → camina rápido + salta alto.
Cierra el editor. Abre `mood_player.exe` con el mismo proyecto → mismas speeds. **Paridad total Editor↔Player** porque ambos leen los mismos fields del `.moodproj`.

---

## Realidad técnica (qué sí / qué no)

**Sí en F3H4:**
- Agregar 4 fields al struct `Mood::ProjectSettings` (`engine/project/ProjectSettings.h`).
- Extender `toJson`/`projectSettingsFromJson` para los 4 nuevos campos (mismo patrón defensivo de F3H1: solo escribir si difieren del default, leer solo si presente + type-check).
- Reintroducir el TabBar en `ProjectSettingsPanel` (F3H1 polish lo había eliminado por scope chico; ahora vuelve porque hay 2 secciones: Performance + Gameplay).
- Sección "Gameplay" nueva con los 4 SliderFloat. Cada cambio → `m_ui->requestProjectDirty()`.
- Lecturas en los 2 call-sites de cada speed:
  - `src/player/PlayerApplication_Frame.cpp` — hoy literales 5.5/3.0/5.5/0.2 → leer de `m_project->gameplay.walkSpeed` etc.
  - `src/editor/application/EditorPlayMode.cpp` — mismo.
- Test `test_project_settings.cpp` extendido: roundtrip de los 4 fields nuevos + back-compat (proyecto viejo sin la subkey carga con defaults).
- i18n: 5 keys nuevas (`editor.project_settings.section.gameplay`, `editor.project_settings.walk_speed`, `editor.project_settings.crouch_speed`, `editor.project_settings.jump_velocity`, `editor.project_settings.jump_cooldown`).

**NO en F3H4:**
- Capsule dimensions / eye height / headbob (bucket 2 del audit, `.moodproj > Character`) — eso es F3H5.
- Mount radius / brake ratio / steer rates (bucket 3, `.moodproj > Vehicle`) — diferido hasta demanda real.
- Gravity (bucket 5+7 consolidado) — F3H4 puede sumarlo si scope lo permite, pero por separado en sección Physics (requiere cablear el live value a `wheelRestCompression` para cerrar el segundo bug latente de F3H3).
- Snap defaults (bucket 4) — F3H7 (sensitivities + snap).
- Shortcuts (F3H6).

**Decisión a confirmar en arranque:** ¿F3H4 incluye gravity (bucket Physics) o solo Gameplay tier 1? Argumento a favor: cierra el bug latente de F3H3 (suspensión mal calibrada si gravity cambia) en el mismo hito que su contexto. Argumento en contra: scope creep — F3H4 estaba acotado a Gameplay; agregar Physics implica también cablear suspensión, que es complejo. Recomendación: **separar** — F3H4 solo Gameplay tier 1 (rápido, valida el patrón), F3H5 o F3H6 agrega Physics.

---

## Bloques

### A — Extender `ProjectSettings`
- Struct nested `Gameplay { float walkSpeed=5.5f; float crouchSpeed=3.0f; float jumpVelocity=5.5f; float jumpCooldownSec=0.2f; };`
- Field `Gameplay gameplay;` en `ProjectSettings`.
- `toJson(s)`: agregar subobjeto `"gameplay"` solo si algún field difiere del default (mantiene .moodproj limpio).
- `projectSettingsFromJson(j)`: leer `j["gameplay"]` si existe, defaults si no.

### B — Tests del struct
- Agregar 4-5 tests a `tests/test_project_settings.cpp`:
  - Default → toJson sin subobjeto gameplay.
  - Setear walkSpeed → toJson escribe gameplay.walk_speed.
  - Roundtrip preserva los 4 fields.
  - Back-compat: `.moodproj` sin `"gameplay"` carga con defaults.
  - Malformed input: `"gameplay": "string"` → defaults silenciosos.

### C — Reintroducir TabBar + sección Gameplay en `ProjectSettingsPanel`
- En `onImGuiRender`, envolver el contenido en `BeginTabBar("##project_settings_tabs")`.
- Tab 1: "Performance" (Target FPS, ya existe — mover el código actual al tab).
- Tab 2: "Gameplay" (nuevo) → llama `drawGameplaySection(settings.gameplay)`.
- `drawGameplaySection`: 4 SliderFloat con tooltips, mismo layout label-control 2-col de Performance.

### D — Lecturas en los call-sites
- `PlayerApplication_Frame.cpp` líneas ~175-185: reemplazar literales por reads de `m_project->settings.gameplay.<field>` (capturar al cargar, igual que `coyoteWindowSec`).
- `EditorPlayMode.cpp` líneas ~504-505 + jumpVel/cooldown si están: mismo.
- Asegurar que ambos call-sites tengan acceso al `Project` actual (ya lo tienen via `m_project`).

### E — i18n + asset sync
- 5 keys en `es.json` + `en.json`.
- Sync de copias a `build/debug/Debug/assets/i18n/`.

### F — Validación
- Suite verde (tests nuevos + sin romper roundtrips existentes).
- Validación visual: abrir Project Settings → tab Gameplay → cambiar walk speed → entrar a PlayInEditor → caminar y sentir el cambio. Salvar → cerrar → reabrir editor → values persisten.
- Validación de paridad Player: lanzar `mood_player.exe` con el proyecto → caminar a la misma velocidad que en Editor.

---

## Decisiones tomadas (pre-implementación)

1. **4 fields exactos**: walk, crouch, jump velocity, jump cooldown. NO incluir mount radius (vehicle), NO incluir headbob (character). Scope chico, patrón claro.

2. **Schema sin bump** (mismo que F3H1 + F3H2): agregar la subkey `"gameplay"` es backward+forward compatible por defaults.

3. **Struct nested vs flat**: nested (`gameplay.walkSpeed`). Esto da estructura limpia en JSON (`"gameplay": {"walk_speed": 5.5, ...}`) y en C++ (`settings.gameplay.walkSpeed`). Igual que se hará para Character (F3H5) y Physics.

4. **TabBar reintroducido**: ya no es placeholder vacío como en F3H1 polish — hay 2 secciones con contenido real. Cuando F3H5+F3H6 agreguen más, ya estará listo.

5. **Sliders en vez de DragFloat**: rango cerrado (walk 1-12, jump 1-15), valores intuitivos. Combo de presets no aplica (no hay "FPS standard speeds" como hay para "30/60/120/144 fps").

6. **NO undo en F3H4**: misma decisión que F3H1 (diferido a F3H10 audit de undo). El edit en el panel marca dirty; Ctrl+Z desde Project Settings no revierte el slider (limitación conocida documentada).

---

## Riesgos / a confirmar temprano

- **Reads en los call-sites**: ambos sitios (Player + Editor) hoy usan `constexpr f32`. Cambiar a `const f32 k_walkSpeed = m_project ? m_project->settings.gameplay.walkSpeed : 5.5f;` requiere acceso al Project en ambos. Editor ya lo tiene; Player necesita verificar.

- **Default mismatch entre struct y i18n**: el default del struct (5.5/3.0/5.5/0.2) debe coincidir con lo que el hint del slider muestra al usuario. Si cambia uno, cambiar el otro. Anotar en el código con un comentario.

- **Validación visual obligatoria**: cambiar walk speed a 12.0 y caminar tiene que sentirse rápido, no romper headbob ni colisión. Si headbob freq queda hardcodeada (sigue siendo F3H5), el feel a alta velocidad puede sentirse mal — anotar como follow-up para F3H5.

---

## Tamaño estimado

Hito chico (~2-3h): 4 fields + 1 sección de UI + 5 tests + i18n + asset sync. Sin nuevos sistemas, sin refactor de infra. La infra de F3H1 (Project Settings panel + persistencia + dirty flag + Project pointer en EditorUI) ya está.

## Cierre del hito

- [ ] Suite verde (incluye 4-5 tests nuevos en `test_project_settings.cpp`).
- [ ] Project Settings panel muestra TabBar con 2 tabs: Performance + Gameplay.
- [ ] Cambiar walk speed se siente al instante en PlayInEditor.
- [ ] Cambiar crouch/jump velocity/cooldown funciona igual.
- [ ] Cerrar editor + reabrir → values persisten en `.moodproj`.
- [ ] `mood_player.exe` con el proyecto siente la misma velocidad que Editor (paridad).
- [ ] Tag `v2.4.0-fase3-hito4`.
- [ ] Update `ESTADO_ACTUAL.md`, `HITOS.md`, `DECISIONS.md`. Crear `PLAN_HITO_F3H5.md` (bucket Character tier 1: capsule + eye height + headbob).
