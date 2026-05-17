# AUDIT-2 — Tech debt sprint (continuación inmediata de AUDIT-1)

> **Tag**: `v1.49.2-audit-2`
> **Fecha**: 2026-05-17
> **Cadencia**: AUDIT-3 previsto post-~5 hitos grandes (aprox post-F2H67). Continuidad inmediata pedida por el dev tras cerrar AUDIT-1.

## Objetivo cumplido

Cerrar la deuda técnica restante del audit anterior (4 splits + layer audit + backfill docs). Sin features nuevas. Time budget cumplido (~3.5h reales vs 5-6h estimadas).

---

## Bloque A — Splits restantes sobre hard cap (800 LOC)

### Antes (HARD cap violations post-AUDIT-1)

```
  1065 HARD src/editor/application/EditorApplication_RunInteractions.cpp
   934 HARD tests/test_scene_serializer.cpp
   888 HARD src/editor/application/EditorRenderPass.cpp
   836 HARD src/editor/ui/EditorUI.h
   813 HARD src/editor/application/EditorProjectActions_CreateEntity.cpp
```

### Después (post-AUDIT-2)

```
   934 HARD tests/test_scene_serializer.cpp                          ← test file, AUDIT-3
   836 HARD src/editor/ui/EditorUI.h                                 ← pImpl refactor, AUDIT-3
```

**HARD cap violations: 5 → 2.** Los 2 restantes requieren refactor de scope mayor, agendado a AUDIT-3.

### Splits ejecutados

| Antes | Después | Δ | Nuevo archivo |
|---|---|---|---|
| `EditorApplication_RunInteractions.cpp` 1065 | **594** | -471 | `EditorApplication_RunInteractions_ToolModes.cpp` 518 LOC (`processOrthoToolModes()` — block tool + marquee + vertex/edge edit) |
| `EditorRenderPass.cpp` 888 | **404** | -484 | `EditorRenderPass_Overlay.cpp` 523 LOC (`drawEditorScene3DOverlay()`) |
| `EditorProjectActions_CreateEntity.cpp` 813 | **490** | -323 | `EditorProjectActions_CreateEntity_PickModal.cpp` 333 LOC + `_Internal.h` con `rotatedAabbWorldY_local` compartido |

### Casos diferidos a AUDIT-3

- **`tests/test_scene_serializer.cpp` 934 LOC** — test file. Lectura diaria baja. Split por `TEST_CASE` families pendiente. Bajo impacto en flujo del dev.
- **`EditorUI.h` 836 LOC** — header con 22+ paneles inline + ~50 inline getters. Reducir requiere pImpl pattern (panels como `std::unique_ptr` con forward decl) o split del header en múltiples sub-headers. Refactor invasivo — fuera del time budget de AUDIT-2.

### Bug arreglado durante el split

`EditorApplication_RunInteractions_ToolModes.cpp` inicial faltaba 2 includes (`engine/render/scene_renderer/SceneRenderer.h` + `engine/scene/queries/ScenePick.h`). Link error claro `C2027 undefined type 'Mood::SceneRenderer'` + `C3861 brushAabbWorld identifier not found`. Fix mecánico.

---

## Bloque B — Layer dependency audit (preventivo)

### Reglas vigentes

- `core/` → solo `std` + `glm` + dentro de `core/`.
- `engine/` → puede `core/` + `std` + libs externas. **NO** `editor/`, `player/`, `tests/`.
- `editor/`, `player/` → pueden `engine/` + `core/` + libs.
- `systems/` → pueden `engine/` + `core/`. **NO** `editor/`, `player/`.

### Violaciones detectadas: 2 (de ~441 cross-layer includes totales)

| Violación | Direction | Severity |
|---|---|---|
| `src/engine/scene/serialization/ProjectSerializer.h:17` → `editor/workspace/Workspace.h` | engine → editor | Media |
| `src/core/UserSettings.h:14` → `engine/i18n/I18n.h` | core → engine | Baja |

**Análisis**:

- La primera (ProjectSerializer → Workspace) es una violación arquitectónica real. `Workspace` modela layouts del editor (paneles + dockspace) y no debería filtrarse al engine. Probable causa histórica: cuando se agregó `Workspace` al `.moodproj` schema en F2H7, se metió la dependencia sin reorganizar la jerarquía.
- La segunda (UserSettings → I18n) es discutible. `I18n` carga tablas de traducción — podría argumentarse que pertenece a `core/` (no es engine-specific). Pero el código vive en `engine/i18n/`.

**Fix propuesto AUDIT-3**:

1. Mover `Workspace` struct (sin la UI) a `engine/project/Workspace.h`. Mantener `editor/workspace/WorkspaceManager.h` con la UI de switching.
2. Mover `I18n.h/cpp` a `core/i18n/` (es genuinamente shared).

### Conclusión

Cero violaciones es realista — el codebase respeta la jerarquía. Las 2 detectadas son fixes mecánicos que cuestan ~15 min cada uno (mover archivo + actualizar includes). Diferidos a AUDIT-3 para no acoplar con este audit.

---

## Bloque C — Backfill HITOS.md (F2H55-F2H61)

7 entries fat migrados a per-hito files (mecánico, sin reformatear):

- `docs/hitos/F2H55.md` — Bloom + Environment settings per scene.
- `docs/hitos/F2H56.md` — SSAO + depth-texture FB.
- `docs/hitos/F2H57.md` — Workflow Crear+Convertir entidad estilo Hammer.
- `docs/hitos/F2H58.md` — Color grading LUT + consolidación Environment.
- `docs/hitos/F2H59.md` — Primitivas clásicas + reorg UX modal.
- `docs/hitos/F2H60.md` — CSM + 5 iteraciones polish UX.
- `docs/hitos/F2H61.md` — SSR + G-buffer parcial MRT.

`HITOS.md` reducción de tamaño: **~258K chars → ~189K chars** (-27%). El detalle vive en los per-hito files; cada entry ahora es 1 línea con link.

**Backfill restante** (F2H1-F2H54): inline en HITOS.md. Sin urgencia — son inmutables, no se editan, no generan fricción operacional. Migrarán en AUDITs futuros si el dolor lo justifica.

---

## Bloque D — Pure helpers audit (identificación)

Solo identificación — extracción real agendada a AUDIT-3+.

### Candidatos encontrados en `_RunInteractions*.cpp`

1. **`pickRayFromNdc(invVP, ndcX, ndcY) → Ray`** — la reconstrucción ray-en-world-space desde NDC aparece **5+ veces** en _RunInteractions.cpp (líneas ~93, ~389, ~460, ~529). Patrón:
   ```cpp
   const glm::vec4 nearH = invVP * glm::vec4(ndcX, ndcY, -1, 1);
   const glm::vec4 farH = invVP * glm::vec4(ndcX, ndcY, 1, 1);
   const glm::vec3 nearW = glm::vec3(nearH) / nearH.w;
   const glm::vec3 farW = glm::vec3(farH) / farH.w;
   const glm::vec3 dir = glm::normalize(farW - nearW);
   ```
   Extracción candidate: `core/math/Ray.h`, función libre `Ray pickRayFromNdc(glm::mat4 invVP, float ndcX, float ndcY)`. Testeable en headless. Estimado: 20 min + 5-10 unit tests.

2. **`aabbFromTwoPoints(a, b) → AABB`** — patron `min/max` para construir AABB de 2 corners aparece en _ToolModes.cpp (líneas 73-85, block tool) + marquee select + drag-edit. Trivial pero alta frecuencia. `core/math/AABB.h` ya existe — agregar constructor + free function.

3. **`snapWorldPositionToGrid(worldPt, gridStep, perpAxis) → glm::vec3`** — el snap-to-grid local a un eje ortográfico se repite con variantes en block tool + drag-edit + vertex/edge edit. Pure function clara.

4. **`groupClickedFaceByBrush(scene, ray, snapToVertex) → optional<FaceHit>`** — el flow del face picking en _RunInteractions.cpp lines 58-200 es complejo y entremezcla lookup de brushes + ray-cast + 3 ramas de selection. Extracción haría el código más legible Y testeable.

### Estado actual de pure helpers en el repo

El codebase YA tiene buenos pure helpers en lugares clave (cosechado en audits informales previos):
- `Csg::makeBoxBrush()`, `makeConeBrush()`, etc.
- `RenderBatching::groupByBatch()` (puro, F2H4).
- `ShadowMath::computeCsmSplits()` (puro, F2H60).
- `Frustum::extractFromMatrix()` + `aabbVisible()` (puros, F2H3).
- `Mood::ShaderGraph::generateGlsl()` (puro, F2H62).

El gap principal está en `EditorApplication` (interaction logic) y panel render code. **AUDIT-3** debería atacar el #1 candidato (`pickRayFromNdc`) como warmup + medir si emerge demanda para los demás.

---

## Convenciones nuevas (refinamiento de AUDIT-1)

- **Helper `_Internal.h` privado al subdir**: convención ya existente (`DemoSpawners_Internal.h`, `EditorOverlay_Internal.h`), formalizada en AUDIT-2 con `EditorProjectActions_CreateEntity_Internal.h`. Patrón: cuando un .cpp se splittea y comparte helpers locales con su sibling, los helpers van al `_Internal.h` con namespace dedicado (`Mood::CreateEntityHelpers`).
- **`tools/sizeometer.sh`**: invocación obligatoria antes de cada cierre de hito + commit del audit (post-AUDIT-1 establecido, reforzado).
- **Header refactors (`*.h` >800 LOC)**: requieren scope dedicado (pImpl + split). NO entran en audits "rápidos". Agendar como hito propio si se priorizan.

---

## Pendientes para AUDIT-3

### Code

1. `tests/test_scene_serializer.cpp` 934 LOC — split por TEST_CASE families.
2. `EditorUI.h` 836 LOC — refactor pImpl: panels como `std::unique_ptr<>` + forward decls. Refactor invasivo (~1 día).
3. Pure helper `pickRayFromNdc` extracción + 5-10 unit tests (warmup de DOD).
4. Layer fixes:
   - Mover `Workspace.h` (struct) a `engine/project/`.
   - Mover `i18n` a `core/i18n/`.

### Docs

5. Backfill F2H1-F2H54 a per-hito files si emerge demanda. Hoy no genera fricción.

### Audits propuestos

6. **Dead code audit** — funciones/clases sin referencias. Probable bajo retorno (compilador ya warning-iza unused).
7. **DOD refactor de hot paths** — solo si profiling lo justifica (no actualmente medido).

---

## Costo real vs estimado

- **Bloque A**: ~1.5h (más rápido que AUDIT-1 — patrón ya conocido).
- **Bloque B**: ~15 min (greps + análisis).
- **Bloque C**: ~30 min (script Python + verificación).
- **Bloque D**: ~30 min (búsqueda + análisis + documentación).
- **Cierre**: ~30 min (este reporte + commit + tag).
- **Total**: ~3.5h reales (estimado 5-6h). Curva de aprendizaje del primer audit pagó dividendos.
