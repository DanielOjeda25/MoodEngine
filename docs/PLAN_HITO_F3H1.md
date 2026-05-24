# PLAN F3H1 — Project Settings panel + persistencia `.moodproj`

**Estado:** Planeado (primer hito de Fase 3, arranca tras `v2.0.2-break-deferreds`).
**Predecesor:** cierre del break-deferreds (commits e462aff + d47121b + 22b0840 + 3c1a151).
**Origen:** principio "nada hardcodeado" definido como espina dorsal de Fase 3 (ver `feedback_no_hardcoded_values` en memoria + `PLAN_FASE3.md` sección 2). Este hito no migra hardcodes: construye el **chasis** sobre el cual los siguientes hitos van a colgar fields.

---

## Qué siente el usuario

El dev abre el editor con un proyecto. Va a `Edit > Project Settings...`. Aparece un panel modal (o dockeado, decidir en bloque D) con tabs:
- **General** — nombre del proyecto, autor, descripción, default map.
- **Spawn Defaults** — placeholder (vacío en F3H1, se llenará en F3H4).
- **Rendering** — placeholder (se llenará en F3H4+).
- **Physics** — placeholder.

Edita 2 fields: el **nombre del proyecto** (string) y el **target FPS** (int). Cierra el panel. El proyecto queda marcado dirty (asterisco en el titlebar). Guarda con Ctrl+S. Cierra el editor. Lo reabre, abre el mismo proyecto: los valores persisten.

Si edita el `.moodproj` a mano en un editor de texto y rompe el JSON, el editor carga con defaults sensatos y un warning visible en consola (no crashea).

**Lo que NO siente todavía en F3H1:** los valores no se *consumen* en ningún lado (target FPS no limita el frame rate todavía). Eso es F3H4. F3H1 es solo el chasis editable + persistente.

---

## Realidad técnica (qué sí / qué no)

**Sí en F3H1:**
- Modelo `ProjectSettings` (struct C++) con 2 fields prueba (`projectName`, `targetFps`).
- Persistencia: leído/escrito a `.moodproj` como subobjeto `"settings": {...}` (no archivo separado).
- Panel UI con tabs (General poblado, los otros 3 con placeholder text "Próximamente en F3H4+").
- Cambios marcan el proyecto dirty.
- Defaults sensatos: `projectName = stem del .moodproj`, `targetFps = 60`.
- Sin bump de schema (decisión arquitectónica clave — ver sección Decisiones).
- Tests: roundtrip del schema + lectura de `.moodproj` sin sección `settings` (back-compat con proyectos pre-F3H1).

**NO en F3H1 (queda para hitos siguientes):**
- Migración de hardcodes existentes a estos settings (F3H4).
- Consumo runtime del `targetFps` para limitar frame rate (depende de pipeline change, F3H4+).
- Validación cross-field (futuro F3H7 si emerge).
- Settings con tipos complejos (vec3, color, enum) — los agregamos en F3H4 con casos reales.

---

## Bloques

### A — Modelo `ProjectSettings` + serialización

`src/engine/project/ProjectSettings.{h,cpp}` (subcarpeta nueva si no existe; sino al lado de `Workspace.h` que ya vive en `engine/project/` per AUDIT-3).

```cpp
struct ProjectSettings {
    // General
    std::string projectName;   // default: stem del .moodproj
    int         targetFps = 60;

    // Sub-secciones futuras (placeholder, vacías en F3H1):
    // SpawnDefaults spawnDefaults;
    // RenderingSettings rendering;
    // PhysicsSettings physics;
};

nlohmann::json toJson(const ProjectSettings&);
ProjectSettings fromJson(const nlohmann::json&, const std::string& fallbackName);
```

Tests unitarios headless (no Qt/ImGui): roundtrip, missing fields → defaults, malformed JSON → defaults + warning.

### B — Integración con `.moodproj` loader/saver

Localizar el código actual de load/save del proyecto (probable `engine/project/ProjectLoader.*` o `EditorProjectActions_FileIO.cpp`). Agregar lectura del subobjeto `"settings"` al cargar; agregar escritura al guardar.

**Back-compat**: si `.moodproj` no tiene la key `settings`, usar defaults (sin warning — proyecto pre-F3H1 válido).

**Forward-compat**: si el JSON tiene keys que C++ no conoce todavía, ignorarlas en silencio (futuro-proof).

Test de integración: cargar un `.moodproj` de Fase 2 (sin settings) → carga OK con defaults. Guardar → ahora tiene settings con defaults. Re-cargar → settings preservados.

### C — Panel UI

`src/editor/panels/project/ProjectSettingsPanel.{h,cpp}` (subcarpeta `project/` nueva en `editor/panels/`).

`IPanel` subclass. Pattern uniforme con otros panels (`PreferencesPanel` que vendrá en F3H2, `MaterialEditorPanel`, etc.). Tabs con `ImGui::BeginTabBar`. Tab "General" con 2 fields (`InputText` para nombre, `DragInt` para targetFps con rango [10, 240]).

Edit tracking: usar el patrón `pushEditIfDone` ya vigente en Inspector — undo/redo cubre los cambios del panel (decisión: queremos undo de "cambié el nombre del proyecto"? Probablemente sí, mismo patrón que Inspector). Si decidimos NO, el panel marca dirty pero los cambios no son undoables.

### D — Wiring (menu + dispatch + visibility)

- Nueva entrada en MenuBar: `Edit > Project Settings...` (atajo F2H42-style? Probablemente NO en F3H1, defer a F3H6).
- `ProjectAction::OpenProjectSettings` enum + `requestProjectAction` flow (gemelo de cualquier handle existente).
- Visibility default: cerrado. El menu lo abre.
- ¿Modal o dockeable? **Decisión: dockeable**, igual que Inspector. Razón: el dev puede tener Project Settings abierto y seguir editando el viewport (cambio incremental, ver impacto inmediato cuando llegue F3H4).

### E — Tests + validación

- Suite headless: roundtrip + back-compat con `.moodproj` pre-F3H1.
- Validación visual: abrir el editor, abrir Project Settings, cambiar nombre, guardar, cerrar, reabrir → nombre persistido.
- Test cruzado: cargar `.moodproj` corrupto (JSON inválido en sección `settings`) → editor no crashea, defaults aplicados, warning en consola.

---

## Decisiones tomadas (pre-implementación)

1. **Sin bump de schema explícito de `.moodproj`.** Aplicar el patrón de SaveLoad post-v2.0.2 (`ammo` removal): forward+backward compatible por defaults. Razón: agregar fields en cada hito de Fase 3 sin bumpear es escalable; bumpear cada hito sería verboso e innecesario. Si emerge un cambio incompatible (renombrar key, cambiar tipo), ahí sí bumpeamos.

2. **`.moodproj` como home, no archivo separado.** Settings vive como subobjeto `"settings": {...}` dentro del `.moodproj`. Razón: keep proyectos como unidad transportable; un archivo separado fragmenta el "qué es mi proyecto".

3. **Panel dockeable, no modal.** Razón documentada en bloque D.

4. **Undo/redo de cambios del panel: SÍ (mismo patrón Inspector).** Si el dev cambia targetFps por accidente, Ctrl+Z lo vuelve. Costo: cero (el patrón ya existe). Beneficio: consistencia UX.

5. **2 fields prueba en F3H1, no 5.** Razón: minimiza scope. Validamos infra. La migración de hardcodes (decenas de fields) es F3H4 con catálogo completo (F3H3).

---

## Riesgos / a confirmar temprano

- **¿`engine/project/ProjectLoader` existe como tal o el flow vive en `EditorProjectActions_FileIO.cpp`?** Confirmar antes de empezar para saber dónde insertar el load/save de settings. Si está esparcido, considerar promoverlo a `ProjectFile.{h,cpp}` en `engine/project/` (mantener scope F3H1, no es un refactor — solo localización).

- **Undo del nombre de proyecto puede ser raro UX.** Si Ctrl+Z desde el viewport revierte el nombre del proyecto (acción "lejana" del foco), confundir. *Mitigación posible*: el undo de Project Settings vive en un stack separado del scene. *Decisión*: en F3H1 usar el stack global (simple), si el feedback del usuario es malo, split en F3H10 (undo audit).

- **`projectName` colisiona con el nombre del archivo `.moodproj`?** Hoy probablemente el nombre se infiere del archivo. Ahora coexisten: nombre interno editable + nombre de archivo. *Decisión*: el nombre interno es metadata (puede diferir). El titlebar muestra el nombre interno si está seteado, sino el stem del archivo.

---

## Tamaño estimado

Hito chico-mediano. ~5 bloques pero todos contenidos: 1 struct, 1 toJson/fromJson, 1 panel `IPanel`, 1 wire al menu, tests. Sin tocar render/physics/etc. Posiblemente 4-6h de trabajo + validación visual.

## Cierre del hito

- [ ] Suite verde (incluye tests nuevos del roundtrip).
- [ ] Edit > Project Settings abre el panel.
- [ ] Cambios persisten al guardar + reabrir.
- [ ] Proyectos pre-F3H1 (sin sección `settings`) cargan sin warning.
- [ ] Proyecto con JSON corrupto en settings: defaults + warning, no crash.
- [ ] Tag `v2.1.0-fase3-hito1`.
- [ ] Update `ESTADO_ACTUAL.md`, `HITOS.md`, `DECISIONS.md`. Crear `PLAN_HITO_F3H2.md`.
