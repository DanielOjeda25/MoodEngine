# PLAN F3H6 — Migración del bucket "Snap" a `.moodproj > Snap`

**Estado:** Planeado (sexto hito de Sub-fase 3.1, arranca tras `v2.5.0-fase3-hito5`).
**Predecesor:** F3H5 (migración Character — el patrón confirmado a 2 buckets).
**Origen:** Bucket 4 del audit F3H3 ([`docs/HARDCODED_AUDIT.md`](HARDCODED_AUDIT.md)) — **alta prioridad**. Es el primer bucket de UX del editor (no de gameplay) — el dev cambia esto frecuentemente al modelar con CSG/brushes.

---

## Qué siente el usuario

El dev abre Project Settings. Ya tiene 3 tabs (Performance + Gameplay + Character). Aparece un **tab "Snap" / "Ajuste"** nuevo con:

- **Pasos disponibles** — lista editable de unidades (`{1, 2, 4, 8, 16, 32, 64, 128}` default). El dev puede agregar 256 o quitar 1.
- **Paso default** — combo que selecciona cuál de los pasos es el initial (default index 4 → 16 unidades).
- **Umbral snap-to-vertex (NDC)** — slider 0.01 → 0.10, default 0.02. Qué tan cerca tiene que estar el cursor del vertex en pantalla para snapearle.
- **Umbral broadphase (px)** — slider 4 → 64, default 16. Distancia mínima de candidatos antes del refinement.

El dev de un proyecto de mundo abierto (mapas 1km × 1km) sube los pasos a `{16, 32, 64, 128, 256, 512}`. El dev de un proyecto interior detallado (cuartos chicos) los baja a `{0.25, 0.5, 1, 2, 4, 8}`. Per-proyecto = cada uno tiene su escala.

---

## Realidad técnica (qué sí / qué no)

**Sí en F3H6:**
- Struct nested `SnapSettings` en `ProjectSettings.h` con 4 fields:
  - `std::vector<int> stepsAvailable` (default `{1, 2, 4, 8, 16, 32, 64, 128}`).
  - `int defaultStepIndex` (default 4 → 16).
  - `f32 snapToVertexThresholdNdc` (default 0.02).
  - `f32 snapBroadphaseThresholdPx` (default 16.0).
- `toJson`/`fromJson` defensivos. Para `stepsAvailable` (array): solo escribir si difiere del default, leer + validar (descartar non-int + ordenar + dedupe).
- Tab "Snap" en `ProjectSettingsPanel` con UI custom (lista editable + sliders + reset buttons).
- Reads en `EditorApplication.cpp:212-224` (snap steps + default index) y `EditorApplication_Snap.cpp:45,49` (NDC + broadphase thresholds).
- Tests de roundtrip + back-compat + array malformed.
- ~8 keys i18n.

**NO en F3H6:**
- Mount radius, vehicle steer rates, friction multipliers (bucket Vehicle) — diferido.
- Sensitivities + zoom (bucket UserSettings > Editor) — F3H7 dedicado.
- Shortcuts (bucket separado) — hito propio porque requiere keymap UI.

---

## Bloques

### A — Extender `ProjectSettings` con `SnapSettings`
- Struct nested con `std::vector<int>` + 1 int + 2 f32.
- Validación al fromJson: array de ints únicos > 0, ordenados ascendente. Si malformed, defaults.
- Default index clamped al tamaño del array (si índice inválido, default 0).

### B — Tests
- 5-6 tests del struct:
  - Defaults → no subobject.
  - Custom steps → subobject completo.
  - Roundtrip preserva steps + default index + thresholds.
  - Malformed array (mezcla de tipos) → defaults silencioso.
  - Default index fuera de rango → clampea a 0.

### C — Tab Snap en `ProjectSettingsPanel`
- UI custom para la lista de pasos: `InputText` o `InputInt` per row + botones "+" / "-" para agregar/quitar.
- Combo para default index (rotula con el valor: "Default: 16 (index 4)").
- 2 SliderFloat para los thresholds + reset buttons + tooltips.

### D — Lecturas en EditorApplication
- `EditorApplication.cpp:212-224`: el array hardcoded `{1, 2, 4, 8, 16, 32, 64, 128}` se vuelve read de `m_project->settings.snap.stepsAvailable`. Sin project → defaults.
- `EditorApplication.cpp:215` (default index 4) → `m_project->settings.snap.defaultStepIndex`.
- `EditorApplication_Snap.cpp:45,49` → reads live de NDC + broadphase thresholds.

### E — i18n + asset sync
- 8 keys nuevas. Sync a build dir.

### F — Validación visual
- Tab Snap muestra el array + sliders + reset buttons.
- Cambiar array de pasos → al usar el editor de mapas, el snap step picker (G key) ofrece los nuevos pasos.
- Cambiar default → al cargar el proyecto, snap step arranca en el nuevo default.
- Cerrar editor + reabrir → values persisten.

---

## Decisiones tomadas (pre-implementación)

1. **`std::vector<int>` en el struct** (no `std::array<int, 8>`): el dev puede agregar/quitar pasos. Tamaño 8 era arbitrario.

2. **Validación estricta al fromJson**: array de ints positivos únicos. Mezclados/negativos/duplicados → defaults silencioso (no log, ignora y sigue).

3. **Default index referenciado por posición, no por valor**: si el dev tiene `{0.25, 1, 2}` y default index 0, "default = 0.25". Cambiar el array no rompe el index — solo si queda fuera de rango (clampea).

4. **UI custom para la lista** (no slider): `InputInt` + botones es lo natural para una lista de N valores. Más trabajo que un slider, pero el caso de uso lo justifica.

5. **Schema sin bump** (consistente con F3H1+F3H4+F3H5).

---

## Riesgos / a confirmar temprano

- **Snap step type**: el código actual usa `int` (1, 2, 4, ...). Si en el futuro el dev quiere snaps fraccionarios (0.25, 0.5), hay que cambiar a `f32`. Decisión ahora: mantener `int` (lo que está hoy). Si emerge demanda, sub-hito.

- **`std::vector` en `ProjectSettings` rompe POD**: el struct deja de ser trivially copyable. Esto puede impactar copies en otros lugares. Validar con build (probablemente OK porque ProjectSettings ya copia con campo nested struct GameplaySettings/CharacterSettings).

- **UI editable de array es más compleja que sliders**: probable que requiera ~50 LOC extra para "+/-" buttons + drag-reorder (opcional, scope creep).

---

## Tamaño estimado

Hito chico-mediano (~3-4h, un poco más que F3H4/F3H5 por la UI custom). El struct + reads son rápidos; el panel es donde está el trabajo nuevo.

## Cierre del hito

- [ ] Suite verde (+5-6 tests nuevos).
- [ ] Tab Snap con array editable + 2 sliders + reset buttons.
- [ ] Cambiar pasos del array → snap picker (G) usa los nuevos.
- [ ] Cambiar default index → nuevo proyecto arranca con el seleccionado.
- [ ] Persistencia `.moodproj`.
- [ ] Tag `v2.6.0-fase3-hito6`.
- [ ] Update `ESTADO_ACTUAL.md`, `HITOS.md`, `DECISIONS.md`. Crear `PLAN_HITO_F3H7.md` (último de Sub-fase 3.1 — UserSettings > Editor: sensitivities, zoom, click/drag thresholds).
