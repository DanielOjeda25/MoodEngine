# Plan — F2H8: Multi-mapa por proyecto + Save Map As

> **Leer primero:** `ESTADO_ACTUAL.md` (F2H7 cerrado), `PLAN_FASE2.md`
> sección 4 (F2H7 plan original = "Documentación pública"; F2H8 absorbe
> el "Save Map As" que estaba pendiente desde Hito 6, según comentario
> en `EditorProjectActions.cpp:267`). Multi-mapa será el sub-foco.

---

## Objetivo

Implementar el flujo completo de **gestión de mapas dentro de un
proyecto**: crear nuevos, abrir entre los existentes, guardar como
otro nombre, marcar uno como default, eliminar. El schema `.moodproj`
ya soporta `maps[]` desde Hito 6 — falta el UX para usarlo.

**Por qué ahora**: el dev pidió Save As completo (opción C de las 3
ofrecidas). El comentario del stub original (`Hito 6 pendiente`) dice
que se difirió "porque requiere UI multi-mapa". Hoy el schema está
listo, F2H7 cerró el cambio mayor de UX (workspaces) y este hito
completa el flujo natural de proyecto + mapas.

---

## Filosofía de diseño

- **Backend ya existe**: `Project::maps`, `Project::defaultMap`. Sin
  schema bump.
- **Sin back-compat de proyectos viejos**: el dev confirmó (en F2H7)
  que ya borró proyectos pre-Fase 2. Cualquier proyecto creado de
  ahora en más arranca con un solo mapa default y se va expandiendo
  con los nuevos handlers.
- **UI categorizada**: el menú Archivo gana submenú "Mapa" que aloja
  los 5 handlers del CRUD de mapas. Mantiene "Guardar (Ctrl+S)" en
  el nivel raíz por ser la operación dominante.
- **Confirmación obligatoria** en operaciones destructivas (delete,
  switch con cambios sin guardar). Reusar `confirmDiscardChanges`
  existente.

---

## Decisiones arquitectónicas

| # | Decisión | Valor |
|---|---|---|
| 1 | Scope | Multi-mapa **intra**-proyecto. Save Project As (copiar carpeta entera) NO entra. |
| 2 | Schema bump del `.moodproj` | NO necesario (`maps[]` y `defaultMap` ya existen). |
| 3 | Backwards compat | Sin (proyectos pre-F2H2 borrados). |
| 4 | Mínimo de mapas por proyecto | 1 (no permitir borrar el último). |
| 5 | Save Map As con nombre existente | Confirmar overwrite. |
| 6 | Switch con cambios sin guardar | Reusar `confirmDiscardChanges` (popup Sí/No/Cancelar). |
| 7 | Delete del mapa default | Reasignar default automáticamente al primero restante. |
| 8 | UI | Submenú "Archivo > Mapa" con 5 entries. Lista de mapas en sub-submenú "Abrir mapa". |

---

## Bloques

### A — Plan F2H8 (este archivo)

Documento del hito. Cierra al commit inicial.

### B — `MapsManager` puro + tests

Nuevo `src/editor/project/MapsManager.{h,cpp}`. PURO (no toca disk ni
ImGui). API:

```cpp
class MapsManager {
public:
    /// Inicializa con la lista de mapas del Project + el default.
    /// Si la lista está vacía, agrega el default automáticamente.
    void setMaps(std::vector<std::filesystem::path> maps,
                  std::filesystem::path defaultMap,
                  std::filesystem::path currentMap);

    bool addMap(const std::filesystem::path& path);     // false si duplicado
    bool removeMap(const std::filesystem::path& path);  // false si era el último
    void setCurrentMap(const std::filesystem::path& path);
    void setDefaultMap(const std::filesystem::path& path);

    const std::vector<std::filesystem::path>& maps() const;
    const std::filesystem::path& currentMap() const;
    const std::filesystem::path& defaultMap() const;

    bool contains(const std::filesystem::path& path) const;
    bool isDefault(const std::filesystem::path& path) const;
    usize count() const;
};
```

Tests en `tests/test_maps_manager.cpp` (~12 cases): setMaps con vacío
→ default automático, addMap deduplica, removeMap del último → false,
removeMap del default → reasigna automático, contains, isDefault, etc.

### C — Handlers en `EditorApplication`

`src/editor/application/EditorProjectActions.cpp`:

- `handleSaveMapAs()`:
  - `pfd::save_file` con default path `<project.root>/maps/<currentName>_copy.moodmap`.
  - Si el archivo existe, `pfd::message` confirm overwrite.
  - `SceneSerializer::save(...)` al nuevo path.
  - `m_project->maps` agrega el path relativo si no existe.
  - `m_currentMapPath` cambia al nuevo.
  - `m_projectDirty = true` para que el `.moodproj` persista la lista actualizada.

- `handleNewMap()`:
  - `pfd::save_file` para nombre. Default `<project.root>/maps/untitled.moodmap`.
  - Si existe, confirm overwrite.
  - Crear `GridMap` vacío (16x16, tileSize 3.0 — mismo default que `buildInitialTestMap`).
  - `SceneSerializer::save(emptyMap, name, ...)` con scene vacío.
  - Agregar a `m_project.maps`. Switch al nuevo mapa (cargar como currentMap).
  - `confirmDiscardChanges` antes si el actual está dirty.

- `handleOpenMap(const std::filesystem::path& mapPath)`:
  - `confirmDiscardChanges` si el actual está dirty.
  - Cargar el mapa via `SceneSerializer::load`.
  - `rebuildSceneFromMap` + `applyEntitiesToScene`.
  - `m_currentMapPath = mapPath`.
  - Update window title.

- `handleSetDefaultMap()`:
  - Marca `m_project->defaultMap = m_currentMapPath`.
  - `m_projectDirty = true`.
  - Status message "Mapa actual establecido como default".

- `handleDeleteCurrentMap()`:
  - Si `m_project.maps.size() == 1`, popup error "no se puede eliminar el último mapa".
  - Confirm popup "¿Eliminar 'level1.moodmap'? Esta acción no se puede deshacer".
  - Borrar archivo de disco.
  - Quitar de `m_project.maps`.
  - Si era el default, reasignar default al primer restante.
  - Cargar otro mapa (ej. el primero del array).

### D — UI: menú "Archivo > Mapa"

`src/editor/ui/MenuBar.cpp`: agregar submenú "Mapa" dentro de
"Archivo" después del separator de "Guardar". Estructura:

```
Archivo
├─ Nuevo proyecto
├─ Abrir proyecto
├─ Recientes
├─ Cerrar proyecto
├─ ──────────────
├─ Guardar (Ctrl+S)
├─ ──────────────
├─ Mapa
│  ├─ Nuevo mapa
│  ├─ Abrir mapa
│  │  ├─ • default.moodmap   ← marcado con bullet si es current
│  │  ├─ level1.moodmap
│  │  └─ level2.moodmap
│  ├─ Guardar mapa como...
│  ├─ Establecer como default ★
│  └─ Eliminar mapa actual
├─ ──────────────
├─ Empaquetar proyecto
└─ Salir
```

`★` se muestra ya marcado si el mapa actual es el default.

Cada `MenuItem` clickea emite request via `EditorUI::request*` (mismo
patrón que `requestProjectAction`). Los handlers se ejecutan en
`EditorApplication::processRequests` después del frame de UI.

### E — Tests + cierre

Tests del helper `MapsManager` en Bloque B (~12 cases). Tests del
flujo SerializerSerializer ya cubren round-trip de mapas. Lo único
que NO se testa con doctest es el flujo del Editor con `pfd::*` —
verificado por el dev a ojo.

Cierre:
- `docs/HITOS.md`: F2H8 [x].
- `docs/ESTADO_ACTUAL.md`: sección 1 reescrita con F2H8 cerrado.
- Entrada en `docs/DECISIONS.md` (2026-05-XX): "Multi-mapa
  intra-proyecto". Documentar que Save Project As se difirió.
- Tag: `v1.1.6-fase2-hito8`.

---

## Suite

Cambios aditivos. MapsManager puro. Tests nuevos: ~12 cases.
Suite esperada al cierre: **394+/6960+** sin regresiones.

## Riesgos

- **Conflicto de nombres en disco**: si el user crea mapa con nombre
  que ya existe, confirm overwrite es UX estándar.
- **`pfd::save_file` en Windows con paths con caracteres especiales**:
  ya hay precedente del Hito 14 (`processSavePrefabRequest` sanitiza
  el filename antes de pasarlo a pfd). Replicar el mismo helper.
- **Cargar map corrupto / faltante**: `SceneSerializer::load` ya
  devuelve `std::optional`. Si falla, popup error y mantener current.
- **Switch durante Play Mode**: bloquear desde el menú (gray out de
  los items de Mapa) — switching de mapa durante play es un caso
  raro y puede tener implicaciones físicas (Jolt bodies del map
  anterior).

---

## Criterios de cierre

- [ ] `MapsManager` puro implementado con docstrings.
- [ ] 5 handlers cableados: SaveMapAs / NewMap / OpenMap / SetDefault / DeleteMap.
- [ ] UI submenú "Archivo > Mapa" funcional con 5 items + lista de mapas.
- [ ] Tests del MapsManager (12+ cases pasando).
- [ ] Suite global sin regresiones (394+/6960+).
- [ ] Verificación visual del dev: crear 2 maps, switchear, eliminar,
      establecer default, reabrir el proyecto y verificar que el
      default elegido se carga.
- [ ] `docs/HITOS.md`, `ESTADO_ACTUAL.md`, `DECISIONS.md` actualizados.
- [ ] Tag `v1.1.6-fase2-hito8` pusheado.
