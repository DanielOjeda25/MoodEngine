# PLAN F3H18 — Validador de assets rotos

**Estado:** **A DEFINIR** (arrancar tras F3H17).
**Predecesor:** F3H17 (drag & drop con feedback visual).
**Origen:** `PLAN_FASE3.md` Sub-fase 3.3 lista "Validador de assets rotos".

---

## Avance de Sub-fase 3.3

```
F3H14 ✅ — Mejoras MeshThumbnailRenderer
F3H15 ✅ — Mejoras MaterialPreviewRenderer
F3H16 ✅ — Hover preview ampliada
F3H17 ✅ — Drag & drop con feedback visual
F3H18 –  — ⬅ próximo: Validador de assets rotos
F3H19 –  — Rename con cascada
```

---

## Norte

`PLAN_FASE3.md` declara:
> **F3H18 — Validador de assets rotos.**
> Panel "Problemas" que liste assets con: ref muerta (path no existe), import fallido (extensión soportada pero load error), version mismatch (schema antiguo), tamaño excesivo. Click → abre el asset / la entity que lo usa. Doble-click → fix sugerido.

**Mecánica del editor (cómo se siente):** el dev abre un proyecto que estuvo dormido 3 meses (o que recibió cambios desde otra rama). El editor sabe que algunos assets están rotos. Aparece un badge rojo o un menú "Problemas" con la lista — el dev clickea, ve qué entity / material / script está roto, qué le falta, y va al fix en 1 click.

Casos canónicos:
- Una textura referenciada por un material fue borrada o renombrada externamente.
- Un mesh con extensión `.fbx` no carga por corrupción / formato exótico.
- Un `.moodmap` v3 cuando el editor está en v5 (sin upgrader): listado como "outdated".
- Un PNG > 50 MB o un .fbx > 200 MB (configurable): warning de "asset pesado".

---

## Scope candidato

### Trabajo principal

1. **`AssetValidator`** (nueva clase, namespace `Mood::asset_validation`): API
   ```cpp
   struct AssetIssue {
     std::string assetPath;
     IssueType  type;        // BrokenRef / LoadFailed / SchemaMismatch / OversizedFile
     std::string detail;     // explicación humana
     std::string usedBy;     // ej. "Material 'metal_rusty.material'" — quien refiere al roto
   };
   std::vector<AssetIssue> scanProject(const Project& proj, const AssetManager& assets);
   ```
   Escaneo lateral (no en hot path), llamable on-demand o al abrir el proyecto.

2. **`AssetIssuesPanel`** (nueva clase `IPanel`): listado plano con icon por tipo + tooltip con `detail`. Click → si el ref es asset → seleccionar en Asset Browser; si es entity con ref roto → seleccionar en Hierarchy + Inspector marca el field en rojo.

3. **Badge en menubar** (top bar): chip rojo `! N` que abre el panel. Refresh manual (F5) o al recargar proyecto.

4. **Re-route via Rename con cascada** (F3H19): cuando F3H19 esté listo, el doble-click sobre un BrokenRef ofrece "buscar reemplazo" — pero F3H18 deja stub apropiado para que F3H19 conecte.

### Sites a tocar

- `src/engine/assets/validation/AssetValidator.h/.cpp` (NUEVO).
- `src/editor/panels/project/AssetIssuesPanel.h/.cpp` (NUEVO).
- `src/editor/application/EditorMenuBar.cpp`: badge `! N` que abre el panel.
- `src/editor/panels/scene/InspectorPanel_MeshRenderer.cpp` (y similares): marcar fields con ref muerta en rojo (consume el set de issues).

### Decisiones a tomar al arrancar

1. **Cuándo escanear**: ¿solo al abrir proyecto (1 vez) o también en background polling de mtime?
2. **Cobertura inicial**: ¿Tier 1 broken refs + load failed (más comunes) o también schema mismatch + oversized (más raros)?
3. **Listado plano vs agrupado**: ¿por tipo o por asset?
4. **Auto-fix**: ¿incluir reasignación batch ("reemplazar todas las refs a X por Y") o eso es F3H19?

---

## Alternativas a F3H18

### B) F3H19 (Rename con cascada)

Saltar el validador e ir al rename — más impactante para refactoring activo (mover archivos, renombrar). Pero F3H19 es más complejo (requiere índice de refs reverso) y F3H18 prepara el terreno.

### C) Backlog `backlog-ux-gaps-editor`

- ForceField/Cloth no spawnables desde UI.
- "Agregar sonido al activar mesh" workflow.

Items chicos que cierran fricciones pre-existentes.

---

## Recomendación

Yo (Claude) sugiero **opción A — Validador de assets rotos**:
1. Sigue el orden del plan.
2. Productividad inmediata para proyectos > 50 assets.
3. Prepara el terreno para F3H19 (índice de refs reverso reutilizable).

**Preguntas al dev cuando arranque F3H18:**
1. ¿Confirmás A o querés B/C?
2. ¿Cobertura inicial Tier 1 (broken refs + load failed) o ampliada (4 tipos)?
3. ¿Badge en menubar o solo abrible via menu View?

---

## Lo que NO toca F3H18

- F3H19 (Rename con cascada): hito propio.
- Sub-fase 3.4 (Viewport pro): F3H20+.
- Auto-fix batch ("reemplazar todas las refs"): scope de F3H19.
- Migrators de schema (`.moodmap` v3 → v5): puede listar el problema, no resolverlo automáticamente.
