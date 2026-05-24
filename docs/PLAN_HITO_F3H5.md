# PLAN F3H5 — Migración del bucket "Character" a `.moodproj > Character`

**Estado:** Planeado (quinto hito de Sub-fase 3.1, arranca tras `v2.4.0-fase3-hito4`).
**Predecesor:** F3H4 (migración Gameplay tier 1 — el patrón validado).
**Origen:** Bucket 2 del audit F3H3 ([`docs/HARDCODED_AUDIT.md`](HARDCODED_AUDIT.md)). F3H4 estableció el patrón "constantes → struct nested en `ProjectSettings` → sección nueva en panel → reads live en Editor + captura al load en Player". F3H5 lo replica para Character (capsule + eye + headbob).

---

## Qué siente el usuario

El dev abre Project Settings. Ya tiene tabs Performance + Gameplay (F3H4). Ahora aparece un **tab "Character"** nuevo con:

- **Altura standing** (m) — slider, default 1.0 (capsule total)
- **Altura crouch** (m) — slider, default 0.2
- **Radio capsule** (m) — slider, default 0.4
- **Altura ojos standing** (m) — slider, default 0.7 (offset desde centro de capsule)
- **Altura ojos crouch** (m) — slider, default 0.3
- **Frecuencia headbob** (Hz) — slider, default 5.0
- **Amplitud headbob** (m) — slider, default 0.04

El dev sube radio capsule a 0.6 → el personaje es más ancho, no entra por puertas estrechas. Baja headbob freq a 3.0 → el "bobing" se siente más relajado.

Como F3H4: edita en el panel + ve el cambio en el siguiente tick de Play.

---

## Realidad técnica (qué sí / qué no)

**Sí en F3H5:**
- Struct nested `CharacterSettings` en `ProjectSettings.h` (7 fields f32).
- `toJson`/`fromJson` defensivos (mismo patrón F3H4 — subobject `"character"` solo si difiere).
- Tab "Character" nuevo en `ProjectSettingsPanel` con 7 SliderFloat + tooltips + reset buttons (helper `resetButton<T>` ya existe).
- Reads en EditorPlayMode (`k_charHalfHeightStand/Crouch/Radius` + headbob freq/amplitude) y Player (gemelo).
- Tests de roundtrip + back-compat.
- 9 keys i18n nuevas (`editor.project_settings.character.*`).

**NO en F3H5:**
- Mount radius (vehicle, bucket 3) — deferido a F3H6 o cuando emerja demanda.
- Snap defaults (bucket 4, alta prioridad) — F3H7.
- Shortcuts (bucket separado) — F3H6 dedicado.
- Gravity (bucket 7) — F3H7 o hito Physics dedicado (requiere cablear el live value a la fórmula de suspensión, que es más complejo que un slider — ver bug latente documentado en `HARDCODED_AUDIT.md`).
- **Live tuning durante Play** (problema diferido en F3H4 polish): F3H5 NO lo aborda. El dev sigue con el flow "abrir panel → editar → ver en próximo tick". Si emerge fricción, plantear como mini-hito dedicado (`PlayerControllerComponent` editable en Inspector, o HUD overlay quick-tune).

---

## Bloques

### A — Extender `ProjectSettings` con `CharacterSettings`
- Struct nested con 7 fields f32 + defaults documentados (refs al Hito 30 + F2H41).
- Field `CharacterSettings character;` en `ProjectSettings`.
- Helpers `characterToJson`/`characterFromJson` defensivos en anonymous namespace.

### B — Tests del struct
- 5-6 tests nuevos en `test_project_settings.cpp` siguiendo el patrón F3H4:
  - Default → no subobject.
  - Non-default → subobject con solo los fields cambiados.
  - Roundtrip preserva los 7 fields.
  - Back-compat pre-F3H5 (sin `"character"`) carga con defaults.
  - Malformed (`"character": "string"`) → defaults.

### C — Tab Character en `ProjectSettingsPanel`
- `BeginTabItem("Character")` después de Gameplay.
- `drawCharacterSection(ProjectSettings&)` con los 7 SliderFloat usando el helper `drawSlider` ya refactorizado en F3H4 (acepta `defaultValue` + emit reset button automático).

### D — Lecturas en call-sites
- `EditorPlayMode::updateOnFootCharController`: los `constexpr f32 k_charHalfHeightStand/Crouch/Radius` se vuelven `const f32 = m_project->settings.character.*` (gemelo de los `k_walkSpeed` etc. de F3H4). Headbob freq/amplitude también — están más abajo en la misma función.
- `PlayerApplication`:
  - Agregar `CharacterSettings m_character;` al struct privado.
  - Capturar en `Init` después de gameplay (`m_character = loaded->settings.character;`).
  - `PlayerApplication_Frame.cpp` reemplaza los 7 literales con reads de `m_character.*`.

### E — i18n + asset sync
- 9 keys nuevas (`editor.project_settings.section.character` + 7 labels + 7 hints — aunque label y hint pueden compartir prefijo, son 14 keys con el mismo pattern que F3H4. Si querés ahorrar, dejar solo label + hint compartido tipo "altura del personaje").
- Sync `assets/i18n/*.json` → `build/debug/Debug/assets/i18n/*.json`.

### F — Validación
- Suite verde (tests nuevos + sin romper existentes).
- Visual: tab Character muestra 7 sliders + reset buttons; cambiar radio capsule → en Play el personaje no entra por pasajes finos que antes pasaba; cambiar headbob freq → el bob se siente distinto.
- Persistencia: editar → Ctrl+S → cerrar editor → reabrir → values persisten.
- Paridad Player: editado → `mood_player.exe` siente lo mismo.

---

## Decisiones tomadas (pre-implementación)

1. **Copy-paste del patrón F3H4**: mismo struct nested, mismo helper `drawSlider`, mismo `resetButton<T>`. NO inventar nada nuevo — F3H4 ya validó el approach. Cuando F3H6/F3H7 entren, F3H5 confirma que el patrón escala a 3+ buckets.

2. **7 fields exactos del bucket 2 del audit**: capsule (3) + eye (2) + headbob (2). Mantiene scope chico (~2-3h igual que F3H4).

3. **Schema sin bump** (consistente con F3H1+F3H4): subobject `"character"` opcional, back-compat por defaults.

4. **Slider rangos sensatos**:
   - Altura standing: 0.5 → 2.5 m (jugador chico hasta basquetbolista)
   - Altura crouch: 0.1 → 1.5 m
   - Radio: 0.2 → 1.0 m
   - Eye standing: 0.0 → 1.5 m
   - Eye crouch: 0.0 → 0.8 m
   - Headbob freq: 0.5 → 10.0 Hz
   - Headbob amplitude: 0.0 → 0.2 m

5. **Reset buttons** ya disponibles (F3H4 polish). Reusar.

---

## Riesgos / a confirmar temprano

- **Eye height depende de capsule half-height + radius**: el cálculo en EditorPlayMode (`eyeStand = k_charHalfHeightStand + k_charRadius - 0.2f`) tiene `-0.2f` hardcoded. Si el dev cambia el offset eye, ese `-0.2f` debe consultarse del eye height setting nuevo, no quedar hardcoded. Anotar para no dejar dual-source-of-truth.

- **Capsule re-creation**: cambiar half-height/radius en Play exige destruir + recrear el `playerCharId` Jolt body. Sin eso, los cambios solo aplican al próximo enter Play. Decidir en arranque: aceptar el delay (más simple) o re-crear el body cuando los settings cambien (más complejo, requiere comparar live vs anterior).

- **Headbob amplitude alto puede romper visual**: 0.2 m de amplitude con freq 10 Hz puede inducir motion sickness. No es bug — es "le di rango libre al dev". Anotar como expected.

---

## Tamaño estimado

Hito chico (~2-3h, idéntico a F3H4). Copy-paste del patrón. Sin nueva infra. El test plan ya está claro.

## Cierre del hito

- [ ] Suite verde (+5-6 tests nuevos).
- [ ] Tab Character con 7 sliders + reset buttons.
- [ ] Cambiar radio capsule se siente en Play (personaje más ancho/angosto, choca distinto).
- [ ] Cambiar headbob freq/amp cambia el bob visualmente.
- [ ] Persistencia `.moodproj` + paridad Player.
- [ ] Tag `v2.5.0-fase3-hito5`.
- [ ] Update `ESTADO_ACTUAL.md`, `HITOS.md`, `DECISIONS.md`. Crear `PLAN_HITO_F3H6.md` (bucket Shortcuts — el más distinto de los buckets gameplay, requiere keymap UI nueva).
