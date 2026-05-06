# Plan — F2H16: Limpieza HistoryStack (deudas de undo/redo)

> **Leer primero:** `ESTADO_ACTUAL.md` (F2H15 cerrado),
> `MEMORY.md > project_pending_history_stack.md`. Hito propio
> intermedio entre F2H15 (UV editor) y F2H17 (Face Mode estilo
> Hammer). Insertado en el roadmap por feedback durante validación
> de F2H15: el dev importó cilindro, lo escaló, le puso 2 texturas,
> arrastró textura al suelo, y Ctrl+Z saltó hasta antes de elevarlo.

---

## Objetivo

Auditar y completar el sistema de undo/redo del editor para que
**toda acción significativa del usuario** se registre en el
HistoryStack. Approach **Blender-style** (granularidad por
intención del user, drag = 1 command, nombres humano-legibles),
manteniendo el command pattern existente del Hito 27 (NO refactor
a snapshot pattern).

**Lo que entra en F2H16**:
- Auditoría exhaustiva (Bloque B): lista concreta de todos los
  handlers que mutan scene/map/brushes/UVs sin pasar por
  HistoryStack.
- 4 comandos nuevos:
  - `EditBrushMaterialCommand`
  - `EditBrushUVCommand`
  - `SetTileCommand`
  - `EditMeshRendererMaterialCommand`
- Wireup en handlers correspondientes: cada drop / cada slider del
  Inspector / cada edición pushea su comando.
- UI: **statusbar muestra "Último: <nombre del comando>"** tras
  cada acción (Blender-style, ayuda al user a saber qué Ctrl+Z
  va a deshacer).
- Tests de undo/redo round-trip por comando nuevo.

**Lo que NO entra en F2H16**:
- Refactor a snapshot pattern (Blender-internal). El command
  pattern actual ya es Blender-style en behaviour observable.
- F6 "tweak last operator" (parametrizar comandos post-hoc para
  ajustar params del último ejecutado). Scope grande propio si
  emerge necesidad.
- Pilas separadas por modo (Object Mode vs Edit Mode tipo Blender).
  No aplica hasta F2H17 (face mode introduce un Edit-mode-like).
- Nuevos features que requieran su propio undo (delete brush,
  duplicar brush, etc.) — agregar en sus hitos si emergen.

---

## Filosofía de diseño

- **Mantener command pattern**: la decisión de Hito 27 sigue siendo
  correcta. Snapshot pattern requiere serializar todo el state del
  scene/brush al disco/memoria, lo cual no escala bien con el motor
  actual. Command pattern con `execute()/undo()` por mutación es
  más ligero y testeable.
- **Granularidad por intención del user**: un drag continuo de
  slider = 1 command (no 100). Patrón ya implementado via
  `pushEditIfDone` (Hito 32) — extender a TODOS los handlers que
  faltan.
- **Nombres descriptivos**: cada `command.name()` da una frase
  legible que aparece en el statusbar. Convención: verbo en
  infinitivo + objeto. Ej: "Asignar material a brush", "Editar
  UV scale", "Pintar tile (4, 7)".
- **Tests de round-trip por comando**: el patrón estándar es
  `execute → undo → estado igual al inicial`; `execute → undo →
  execute → estado igual al post`. Cada comando nuevo lleva un
  test dedicado.
- **NO scope creep**: NO agregar features nuevas (multi-edit,
  prefab undo, etc.). Solo cubrir las deudas existentes.

---

## Decisiones arquitectónicas

| # | Decisión | Valor |
|---|---|---|
| 1 | Patrón base | `ICommand` existente (Hito 27). Cada deuda nueva crea un `EditXxxCommand` o reusa `EditPropertyCommand<T>` cuando encaja. |
| 2 | Granularidad | Por edición completada (`IsItemDeactivatedAfterEdit` para sliders del Inspector; on-click-drop para drops). 1 acción del user = 1 command. |
| 3 | Snapshots | Por valor (no handles). Cuando el comando se ejecuta tras un undo, los handles pueden haber cambiado — los snapshots por tag/path permiten reconstruir. Mismo patrón que `BooleanOpCommand` (F2H12). |
| 4 | Statusbar UX | StatusBar muestra "Último: <name>" tras cada `push` exitoso. Persiste 3-5 segundos o hasta el próximo command. NO bloquea el FPS counter ni el resto de info. |
| 5 | Scope: drops | Texture sobre brush + Material sobre brush + Texture sobre tile + Material sobre MeshRenderer. Script drop NO entra (ya es undoable via SetScriptCommand de Hito 27 — verificar). |
| 6 | Scope: Inspector edits | UV editor (4 sliders del Bloque E de F2H15) + bc.material desde combo (si existe — verificar). |
| 7 | Scope: out-of-scope explícito | Cambios programáticos del scene desde scripts Lua / engine systems (esos NO son user actions y no van al stack). |
| 8 | Tests | 1 test de round-trip por comando nuevo en su `test_xxx_command.cpp`. Reusar el patrón de `test_edit_property_command.cpp` y `test_create_entity_command.cpp`. |
| 9 | Statusbar implementación | Reusa el `setStatusMessage` ya existente. Auto-clear tras N frames. |

---

## Tipos públicos nuevos

```cpp
// editor/commands/EditBrushMaterialCommand.h
namespace Mood {

class EditBrushMaterialCommand : public ICommand {
public:
    EditBrushMaterialCommand(Scene* scene,
                              std::string entityTag,  // captura por tag, robusto a remap
                              MaterialAssetId oldMat,
                              MaterialAssetId newMat,
                              std::string label = "Asignar material a brush");
    void execute() override;
    void undo() override;
    std::string name() const override { return m_label; }
private:
    Scene* m_scene;
    std::string m_entityTag;
    MaterialAssetId m_oldMat, m_newMat;
    std::string m_label;
};

} // namespace Mood
```

```cpp
// editor/commands/EditBrushUVCommand.h
namespace Mood {

/// @brief Snapshot de los UV params de TODAS las caras del brush.
///        F2H17 (face mode) puede agregar variante per-cara.
struct BrushUVSnapshot {
    std::vector<glm::vec3> uAxes;
    std::vector<glm::vec3> vAxes;
    std::vector<glm::vec2> uvOffsets;
    std::vector<glm::vec2> uvScales;
    std::vector<f32>       uvRotations;
    std::vector<bool>      lockToWorlds;
};

BrushUVSnapshot captureBrushUV(const Csg::Brush& brush);
void applyBrushUV(Csg::Brush& brush, const BrushUVSnapshot& snap);

class EditBrushUVCommand : public ICommand {
public:
    EditBrushUVCommand(Scene* scene,
                        std::string entityTag,
                        BrushUVSnapshot oldSnap,
                        BrushUVSnapshot newSnap,
                        std::string label);
    void execute() override;
    void undo() override;
    std::string name() const override { return m_label; }
private:
    Scene* m_scene;
    std::string m_entityTag;
    BrushUVSnapshot m_oldSnap, m_newSnap;
    std::string m_label;
};

} // namespace Mood
```

```cpp
// editor/commands/SetTileCommand.h
namespace Mood {

class SetTileCommand : public ICommand {
public:
    SetTileCommand(GridMap* map,
                    EditorApplication* app,  // para callbacks updateTileEntity
                    u32 x, u32 y,
                    TileType oldType, TextureAssetId oldTex,
                    TileType newType, TextureAssetId newTex,
                    std::string label = "Pintar tile");
    void execute() override;
    void undo() override;
    std::string name() const override { return m_label; }
private:
    // ... (snapshots por (x, y))
};

} // namespace Mood
```

`EditMeshRendererMaterialCommand` similar a `EditBrushMaterialCommand`
pero para `mr.materials[0]`.

---

## Bloques

### A — Plan F2H16 (este archivo)

Documento del hito. Nace en `docs/`, se archiva al cierre.

### B — Auditoría exhaustiva

Barrer el editor entero buscando handlers que muten scene/map/
brushes/UVs sin pasar por HistoryStack. **Producir lista concreta**
en este documento (sección "Lista de deudas detectadas") con file:line
y categoría. Categorías:
- **Drops**: texture / material / script / mesh.
- **Inspector edits**: cada widget que muta state in-place.
- **Map ops**: setTile, updateTileEntity, etc.
- **Otros**: comandos de menú, atajos de teclado.

Output esperado: ~10-15 deudas listadas. F2H16 cubre las relevantes
del workflow actual (CSG + UV editor + drops). Las muy especializadas
(ej. ParticleEmitter UI edits si existen) van como "diferido a hito
del feature correspondiente" si emerge.

### C — Comandos nuevos: brushes

- `EditBrushMaterialCommand.{h,cpp}` con snapshot de
  `(MaterialAssetId old, new)`.
- `EditBrushUVCommand.{h,cpp}` con `BrushUVSnapshot` capturando
  los 6 UV params de TODAS las caras.
- Tests: `test_edit_brush_material_command.cpp` y
  `test_edit_brush_uv_command.cpp`. Round-trip + nombre +
  comportamiento bajo handles invalidados (entity recreada por
  undo de un Delete, etc.).

### D — Comandos nuevos: tiles + meshes

- `SetTileCommand.{h,cpp}`: snapshot de `(x, y, oldType, oldTex)`.
  En execute hace `setTile(newType, newTex)` + actualiza la
  entidad-tile. En undo hace setTile(oldType, oldTex) + actualiza.
- `EditMeshRendererMaterialCommand.{h,cpp}`: snapshot de
  `mr.materials[0]` antes/después.
- Tests análogos.

### E — Wireup en handlers

- `processViewportTextureDrop`:
  - Caso brush: pushear `EditBrushMaterialCommand` (snapshot pre,
    crea material wrapper, push).
  - Caso tile: pushear `SetTileCommand` (snapshot pre, mutar grid).
- `processViewportMaterialDrop`:
  - Caso brush: `EditBrushMaterialCommand`.
  - Caso MeshRenderer: `EditMeshRendererMaterialCommand`.
- Inspector "UV (Brush)" sliders:
  - Cada slider/checkbox: capturar snapshot pre al `IsItemActivated`,
    snapshot post al `IsItemDeactivatedAfterEdit`, push
    `EditBrushUVCommand`.
  - Reusar el patrón `pushEditIfDone` adaptado.

**StatusBar UX**: tras cada `m_history.push(...)`, llamar
`m_ui.setStatusMessage("Último: " + cmd->name())` con un timeout
configurable (3-5 segundos). Implementar el timeout en StatusBar
(o usar el setStatusMessage existente con duración).

### F — Validación end-to-end del flow del user

Reproducir manualmente el flow exacto que el user reportó como bug:
1. Importar cilindro (Mesh drop o Box brush, según equivalencia).
2. Escalarlo más grande (gizmo + Ctrl+R).
3. Elevarlo del suelo (gizmo + Ctrl+W).
4. Asignarle 1ra textura (drop sobre el brush).
5. Asignarle 2da textura (drop sobre el brush).
6. Arrastrar textura al suelo (drop sobre tile vacío del grid).
7. **Ctrl+Z 6 veces** → debe revertir cada paso en orden inverso.
8. **Ctrl+Y 6 veces** → debe rehacer cada paso en orden directo.

Si todos los pasos funcionan: validado. Si alguno salta o hace
nada, debug + fix antes de cerrar.

### G — Cierre + tag

- `docs/HITOS.md`: F2H16 [x] con resumen.
- `docs/ESTADO_ACTUAL.md`: F2H16 cerrado, F2H15 a "anterior".
- `docs/DECISIONS.md`: entrada con las decisiones (mantener command
  pattern vs snapshot, granularidad por intención, statusbar UX
  Blender-style).
- Archivar `PLAN_HITO_F2H16.md` a `docs/archive/plans/`.
- Tag `v1.7.0-fase2-hito16`.

---

## Lista de deudas detectadas (Bloque B completo)

### Drops (`DemoSpawners.cpp`)

| # | Mutation | File:Line | Comando |
|---|---|---|---|
| 1 | Drop textura sobre brush — `bc.material = matId; bc.dirty = true` | DemoSpawners.cpp:625-626 | `EditBrushMaterialCommand` |
| 2 | Drop textura sobre tile — `m_map.setTile(...)` + `updateTileEntity` | DemoSpawners.cpp:645 | `SetTileCommand` |
| 3 | Drop material sobre brush — `bc.material = matId` | DemoSpawners.cpp:853-854 | `EditBrushMaterialCommand` |
| 4 | Drop material sobre MeshRenderer — `mr.materials[0] = matId` | DemoSpawners.cpp:871-874 | `EditMeshRendererMaterialCommand` |

**Drops que YA pushean correctamente** (no requieren cambio):
- `processViewportMeshDrop` → `pushCreatedEntities` ✓
- `processViewportPrefabDrop` → `pushCreatedEntities` ✓
- `processViewportScriptDrop` → verificar que pushee `EditPropertyCommand<string>` para `script.path`. Si no lo hace, agregar.

### Inspector "UV (Brush)" (`InspectorPanel.cpp`)

| # | Widget | File:Line | Comando |
|---|---|---|---|
| 5 | `DragFloat2 "uv scale"` | InspectorPanel.cpp:1090-1093 | `EditBrushUVCommand` (multi-field, captura todos los UV params) |
| 6 | `DragFloat "uv rotation (deg)"` | InspectorPanel.cpp:1097-1101 | `EditBrushUVCommand` (mismo) |
| 7 | `DragFloat2 "uv offset"` | InspectorPanel.cpp:1104-1108 | `EditBrushUVCommand` (mismo) |
| 8 | `Checkbox "lock to world"` | InspectorPanel.cpp:1111-1118 | `EditBrushUVCommand` (mismo) |

**Nota sobre granularidad**: el `EditBrushUVCommand` captura los **6 UV params completos** del brush en pre/post snapshot. Cuando el user mueve uvScale, el comando guarda todos los UV antes y todos después. Eso es OK porque snapshots son baratos (96 bytes para una box de 6 caras).

**No requieren comando**:
- InspectorPanel.cpp:1072 — botón "Recompute mesh" (`bc.dirty = true`). Debug helper que NO muta state visible — solo fuerza un rebuild de mesh, idempotente. Saltearlo del stack.

### Total

**4 comandos nuevos** (`EditBrushMaterialCommand`, `EditBrushUVCommand`, `SetTileCommand`, `EditMeshRendererMaterialCommand`) cubren las 8 deudas detectadas.

---

## Suite

Cases nuevos esperados:
- `test_edit_brush_material_command.cpp`: ~4-5 cases.
- `test_edit_brush_uv_command.cpp`: ~5-6 cases.
- `test_set_tile_command.cpp`: ~4-5 cases.
- `test_edit_mesh_renderer_material_command.cpp`: ~3-4 cases.

**Suite estimada: 531 → ~550 cases / 8074 → ~8200 asserts.**

---

## Riesgos

- **Auditoría incompleta**: si dejamos handlers sin command, el
  bug del user persiste en otras formas. Mitigación: lista
  exhaustiva en Bloque B antes de implementar.
- **Captura de snapshots por handle**: handles invalidados tras
  undo/redo de Delete pueden romper comandos previos del stack.
  Mitigación: capturar por tag (igual que `BooleanOpCommand`),
  buscar entidad por tag en `execute/undo`.
- **Drag continuo del slider crea ruido en el log**: si capturamos
  snapshot pre en cada `IsItemActivated`, podemos terminar con
  comandos para clicks instantáneos que no editaron nada.
  Mitigación: solo push si `oldSnap != newSnap` (comparar
  estructuralmente).
- **StatusBar timeout interfiere con otros mensajes**: el FPS
  counter o un mensaje "Mapa guardado" pueden quedar tapados.
  Mitigación: priorizar mensajes manualmente (último command tiene
  prioridad media, FPS no se toca, "Mapa guardado" sobreescribe).
- **Tests de undo/redo con scene + map**: requieren TestAssets
  (AssetManager headless) y/o TestScene helpers. Reusar el patrón
  de `test_brush_persistence.cpp`.

---

## Criterios de cierre

- [ ] Lista de deudas en este plan completada con file:line y
      comandos asignados.
- [ ] 4 comandos nuevos implementados con tests de round-trip
      verde.
- [ ] Wireup en handlers correspondientes.
- [ ] StatusBar muestra "Último: <name>" tras cada push.
- [ ] Validación end-to-end del flow del user (importar cilindro,
      escalar, elevar, 2 texturas, drop al suelo) — Ctrl+Z 6
      veces revierte cada paso.
- [ ] Suite ~550 cases verde.
- [ ] `docs/HITOS.md`, `ESTADO_ACTUAL.md`, `DECISIONS.md` actualizados.
- [ ] Plan archivado.
- [ ] Tag `v1.7.0-fase2-hito16` pusheado.

---

## Refs canónicas

- Blender undo system: https://docs.blender.org/manual/en/latest/interface/undo_redo.html
- Blender Python API operator framework (origen del "operator =
  unidad de undo"): https://docs.blender.org/api/current/bpy.types.Operator.html
- Hito 27 de MoodEngine: introducción del HistoryStack + ICommand.
- Hito 32 de MoodEngine: `pushEditIfDone` + InspectorEditTracker
  para granularidad de drag de slider.
