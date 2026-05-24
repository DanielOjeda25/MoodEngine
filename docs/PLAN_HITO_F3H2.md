# PLAN F3H2 — User Preferences panel + persistencia `UserSettings`

**Estado:** Planeado (segundo hito de Sub-fase 3.1, arranca tras `v2.1.0-fase3-hito1`).
**Predecesor:** F3H1 (Project Settings — chasis para per-proyecto).
**Origen:** F3H1 hizo el "lugar donde editar" per-proyecto (`.moodproj > settings`). Falta el gemelo per-instalación: preferencias del dev que viajan entre proyectos (tema, idioma, atajos, comportamiento del editor). Hoy `core/UserSettings` ya persiste idioma + theme (F2H43 + F2H76) — F3H2 los expone en un panel dedicado, espejo del de F3H1, y agrega fields seed para shortcuts (F3H6) + autosave (F3H7).

---

## Qué siente el usuario

El dev abre el editor. Va a `Edit > Preferences...` (entry que ya existe desde F2H76 — hoy abre un popup modal limitado). El popup actual se convierte en un panel **estilo Unity Preferences**: ventana flotante centrada + tamaño fijo (igual feel que Project Settings F3H1).

Cambia el tema de "Oscuro" a "Claro". Cambia el idioma de "Español" a "English". Cierra. Vuelve a abrir el editor (o abre otro proyecto): las preferencias persisten — son del usuario, no del proyecto.

Más sutilmente: cuando F3H6 (atajos configurables) y F3H7 (autosave) cierren, sus settings vivirán en este mismo panel. F3H2 es el chasis para ellos.

---

## Realidad técnica (qué sí / qué no)

**Sí en F3H2:**
- Refactor del popup modal de Preferences existente (F2H76) a `IPanel` no-dockeable centrado (mismo pattern que F3H1).
- Tema (Oscuro/Claro) — ya persistido en `UserSettings`, expuesto en el panel nuevo.
- Idioma (Español/English) — ya persistido, expuesto en el panel.
- Tab "General" con esos 2 fields. NO tabs adicionales (mismo principio de F3H1 polish: nada de placeholders vacíos).
- Eliminación del popup modal viejo de MenuBar (`m_showPreferencesPopup` y su `BeginPopupModal("##prefs", ...)`).
- Tests: roundtrip de `UserSettings::save/load` (probablemente ya existen para idioma; verificar para theme).

**NO en F3H2 (queda para hitos siguientes):**
- Shortcuts configurables (F3H6 — sub-fase 3.1).
- Autosave interval (parte de F3H7 si entra ahí, o F3H27 directamente).
- Font size, layout density (F3H7 — migración de constantes UI).
- Mouse sensitivity (mismo).
- Editor behavior toggles avanzados.

---

## Bloques

### A — Auditoría de `UserSettings` existente
Confirmar qué fields ya viven en `core/UserSettings.{h,cpp}` (post-F2H43 + F2H76). Probable inventario: `Language`, `Theme`. Verificar API actual: `init()/save()/language()/setLanguage()/theme()/setTheme()`. Validar tests que cubren roundtrip.

Si faltan fields que F3H2 necesita exponer: agregarlos como struct fields opcionales (mismo patrón que `ProjectSettings`).

### B — Refactor del popup → IPanel
`editor/panels/project/UserPreferencesPanel.{h,cpp}` (subcarpeta nueva o reusar `editor/panels/project/` — decidir en arranque). Si comparte folder con `ProjectSettingsPanel`, ambos viven juntos (tematizable, sub-fase 3.1).

IPanel subclass, default `visible = false`, abre desde MenuBar `Edit > Preferences...`. Mismas flags que F3H1: `NoResize | NoCollapse | NoDocking` + centrado + tamaño fijo.

Eliminar:
- `m_showPreferencesPopup` flag de `MenuBar.h`
- `m_prefsOpen` flag (F2H79)
- El bloque `BeginPopupModal("##prefs_modal", ...)` con todo su body en `MenuBar.cpp`

Sustituir el `if (ImGui::MenuItem(...preferences...))` por `ui.requestShowUserPreferences()` (gemelo del F3H1 `requestShowProjectSettings()`).

### C — Tab General con 2 fields
- **Tema**: Combo con 2 opciones ("Oscuro" / "Claro"). Cambio → llama `UserSettings::setTheme(...)` + `EditorThemes::apply(...)` para aplicar inmediato + auto-save.
- **Idioma**: Combo con 2 opciones ("Español" / "English"). Cambio → `UserSettings::setLanguage(...)` + `I18n::setLanguage(...)` + auto-save. El cambio surte efecto next frame (los `T()` resuelven contra el dict nuevo).

Sin tabs adicionales — mismo principio que F3H1 polish (no placeholders vacíos).

### D — Migración UX del popup → panel
El popup actual probablemente tiene labels "Tema" / "Idioma" hardcodeados o vía i18n. Verificar i18n keys existentes y reusar. Si las keys mencionan "F2H76" o similar, limpiar (regla `feedback_no_internal_milestone_refs_in_ui`).

### E — Tests + validación
- Tests headless: roundtrip `UserSettings::save/load` para todos los fields que F3H2 toca (suite probable: cobertura ya existe; agregar lo que falte).
- Validación visual: abrir Preferences, cambiar tema → ImGui colors cambian al instante. Cambiar idioma → labels del editor cambian next frame. Cerrar editor, reabrir → preferencias persisten.

---

## Decisiones tomadas (pre-implementación)

1. **Espejo de F3H1**: mismas flags de ventana, mismo pattern de IPanel, mismo flujo de menú (`requestShow*` directo en EditorUI sin pasar por request/consume). Razón: consistencia — el dev aprende un patrón, aplica a los dos.

2. **Eliminar popup viejo**: no coexistir con el panel nuevo. Pulir = simplificar, no agregar paralelo.

3. **Sin tabs en F3H2**: mismo principio que F3H1 polish — el panel arranca con 1 sección (General). Cuando F3H6 (shortcuts) y F3H7 (UI constants) lleguen, el TabBar se reintroduce naturalmente.

4. **Auto-save al cambiar**: el dev cambia tema → se aplica + persiste al instante, sin botón "Save". Razón: UX Unity/Unreal — Preferences no tiene save button, todo es live. Si rompe algo, undo no aplica (es preferencia global, no scene state).

---

## Riesgos / a confirmar temprano

- **`m_showPreferencesPopup` / `m_prefsOpen` pueden tener consumers en MenuBar más allá del block que veo.** Confirmar antes de borrar.

- **EditorThemes::apply puede requerir un frame de delay para resurfacear.** Probable: cambio inmediato porque ImGui re-pulls colors cada frame. Validar en vivo.

- **Idioma: el cambio re-traduce strings cargados en panels abiertos?** Probable: sí, porque `T()` se llama cada frame en el render de cada panel. Si algún panel cachea strings al ctor, ese no actualiza hasta cerrar/reabrir el panel (caso edge, documentar).

---

## Tamaño estimado

Hito chico-mediano. Similar a F3H1: ~5 bloques contenidos. Sin tocar render/physics/etc. ~3-5h de trabajo + validación visual. Más rápido que F3H1 porque la mayoría de la infraestructura (`UserSettings`) ya existe.

## Cierre del hito

- [ ] Suite verde (incluye tests nuevos si se agregaron fields a `UserSettings`).
- [ ] `Edit > Preferences...` abre el nuevo panel (no el popup viejo).
- [ ] Cambio de tema se aplica al instante + persiste al reabrir.
- [ ] Cambio de idioma re-traduce al instante + persiste.
- [ ] Popup viejo eliminado de MenuBar (sin código muerto).
- [ ] Tag `v2.2.0-fase3-hito2`.
- [ ] Update `ESTADO_ACTUAL.md`, `HITOS.md`, `DECISIONS.md`. Crear `PLAN_HITO_F3H3.md` (auditoría de hardcoded values + catalogación).
