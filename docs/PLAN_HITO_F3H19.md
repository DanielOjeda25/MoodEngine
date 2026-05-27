# PLAN F3H19 — Rename con cascada

**Estado:** **A DEFINIR** (arrancar tras F3H18).
**Predecesor:** F3H18 (validador de assets rotos).
**Origen:** `PLAN_FASE3.md` Sub-fase 3.3 lista "Rename con cascada".

---

## Avance de Sub-fase 3.3

```
F3H14 ✅ — Mejoras MeshThumbnailRenderer
F3H15 ✅ — Mejoras MaterialPreviewRenderer
F3H16 ✅ — Hover preview ampliada
F3H17 ✅ — Drag & drop con feedback visual
F3H18 ✅ — Validador de assets rotos
F3H19 –  — ⬅ próximo: Rename con cascada (cierra Sub-fase 3.3)
```

---

## Norte

`PLAN_FASE3.md` declara:
> **F3H19 — Rename con cascada.**
> Renombrar un asset (mesh/material/texture/script/dialog/item/vehicle) actualiza todas las refs en la escena + en otros assets que lo referencian. Confirmación previa con preview del diff ("13 entidades + 2 materiales serán actualizados"). Undo deshace todo en un solo Ctrl+Z.

**Mecánica del editor (cómo se siente):** el dev decide renombrar `metal_rust.material` → `metal_oxidized.material` desde el Asset Browser. Aparece un dialog: "13 entidades + 2 prefabs hacen ref a este asset — ¿confirmar rename y actualizar todas las refs?". Click Sí → el archivo se renombra en disco + todas las refs en la scene + los .moodprefab + .material que referenciaban la textura se actualizan a la vez. Ctrl+Z deshace todo (renombrado + cascada).

---

## Scope candidato

### Trabajo principal

1. **`AssetRefIndex`** (nueva clase, namespace `Mood::asset_refs`): índice reverse de "qué entidades / materiales / prefabs refieren a path X". Reusa la lógica de walk del `AssetValidator` (F3H18) pero con output `std::unordered_map<std::string, std::vector<RefSite>>`.
   ```cpp
   struct RefSite {
     enum class Source { Entity, Material, Prefab } source;
     std::string description; // "Entity 'NPC_01' / ScriptComponent.path"
     Entity entity{};         // si source==Entity
     std::string assetPath;   // si source==Material o Prefab
   };
   std::unordered_map<std::string, std::vector<RefSite>> buildRefIndex(...);
   ```

2. **`RenameAssetCommand`** undoable: snapshot del estado pre-rename (incluye disk path + todas las refs) + ejecuta rename + actualiza refs. `undo` restaura.

3. **Dialog de confirmación**: modal con tabla "13 refs serán actualizadas: ..." + botones Confirmar / Cancelar.

4. **Wire desde Asset Browser**: context menu "Renombrar" sobre un asset (right-click) → input modal con nuevo nombre → genera el ref index → muestra dialog → ejecuta `RenameAssetCommand`.

5. **Wire desde AssetIssuesPanel** (F3H18): boton "Reemplazar..." en cada issue → file picker de asset nuevo → ejecuta el "rename inverso" (todas las refs al path roto pasan al path nuevo).

### Sites a tocar

- `src/engine/assets/refs/AssetRefIndex.h/.cpp` (NUEVO).
- `src/editor/commands/RenameAssetCommand.h/.cpp` (NUEVO).
- `src/editor/panels/assets/AssetBrowserPanel_Tabs.cpp`: context menu + rename dialog.
- `src/editor/panels/project/AssetIssuesPanel.cpp`: boton "Reemplazar..." en cada issue.
- `src/editor/application/EditorApplication_Run.cpp`: pumpUiRequests para procesar rename requests.

### Decisiones a tomar al arrancar

1. **Cobertura de tipos**: ¿todos los assets (mesh/material/texture/script/dialog/item/vehicle/animation/prefab) o solo los más usados?
2. **Persistencia del rename**: cada componente que refiere al asset por string-path se serializa al `.moodmap` con el nuevo path. ¿Re-save automático del .moodmap o solo en-memory + dirty flag?
3. **Conflict resolution**: si el nuevo nombre ya existe, ¿error / sufijar `_2` / pedir confirmación de overwrite?
4. **Undo granularidad**: ¿un solo comando (atomic rename + cascada) o comando compuesto (rename file + N edit-component commands)?

---

## Alternativas a F3H19

### B) Cerrar Sub-fase 3.3 sin rename

F3H14-F3H18 ya cierran 5/6. Saltar F3H19 y arrancar Sub-fase 3.4 (Viewport pro). Pero rename es feature pedido históricamente — backlog mediano.

### C) Backlog UX (memoria `backlog-ux-gaps-editor`)

- ForceField/Cloth no spawnables desde UI.
- "Agregar sonido al activar mesh" workflow.

Items chicos que cierran fricciones pre-existentes.

---

## Recomendación

Yo (Claude) sugiero **opción A — Rename con cascada**:
1. Cierra Sub-fase 3.3 con la trifecta del Asset Browser (thumbnails + drag&drop + validador + rename).
2. Reusa la lógica de walk de F3H18 (`AssetRefIndex` ~ generalización del scanner).
3. Rename es feature pesado pero scope acotado (1 comando + 1 dialog + 1 wire).

**Preguntas al dev cuando arranque F3H19:**
1. ¿Confirmás A o querés B/C?
2. ¿Cobertura inicial: todos los tipos o subset (mesh/material/texture/script)?
3. ¿Conflict resolution al hacer rename a nombre existente: error / sufijo auto / confirm?

---

## Lo que NO toca F3H19

- Sub-fase 3.4 (Viewport pro): F3H20+.
- Move (cambiar carpeta) de un asset: similar a rename pero scope distinto.
- Bulk rename (renombrar N assets a la vez): backlog si emerge.
- Schema migration al renombrar (ej. `texture_path` → `texturePath` en JSON): no aplica acá.
