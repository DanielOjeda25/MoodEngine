# Code Audit pre-F4H2 — quick sweep

**Hito:** F4H1.5 (audit) → genera priorización para fixes reactivos antes de F4H2.
**Fecha:** 2026-05-29.
**Scope:** quick audit (D1 del plan F4H1.5). 3 buckets: LOC + duplicación + deadcode.

---

## Veredicto top-line

El codebase está **más limpio que esperado**. Disciplina del dev visible en:
- **0 TODOs reales** en src/ (todos los matches eran la palabra española "TODOS").
- **Helpers ya extraídos** en los puntos críticos: `inspectorResetButton`, `pushEditIfDone`, `beginComponentSection`, `findByTag` (Lua), etc.
- **Guards "spaghetti" no detectados**: los `if (!m_scene)` (38 callsites) son idiomatic, no patrón candidato a helper.
- **Archivos F4H1 / F3H28-31 recientes**: ninguno sobre cap (top = 356 LOC `InspectorPanel_Materials.cpp`).

**Deuda real: 4 archivos sobre hard cap 800 LOC**, todos con split lógico claro. El resto de la zona warning (501-800) está mayormente compuesto por headers de clases centrales / frame loops, donde el cap es flexible (memoria `feedback_file_size_limit` lo nombra explícito).

---

## A. Archivos por LOC

### A.1 — Sobre hard cap 800 (4 archivos, excluyendo dataset)

| # | Archivo | LOC | Diagnóstico | Propuesta |
|---|---|---|---|---|
| 1 | `src/editor/panels/scene/InspectorPanel_Environment.cpp` | **1149** | 9 funciones `drawEnvSky` / `drawEnvFog` / `drawEnvTonemap` / `drawEnvBloom` / `drawEnvSsao` / `drawEnvCsmShadows` / `drawEnvColorGrading` / `drawEnvSsr` + helpers + `renderEnvironmentSection` dispatcher | **Split en 3** por familia: <br>• `InspectorPanel_Environment.cpp` ~430 (header + Sky + Fog + dispatcher) <br>• `InspectorPanel_Environment_PostFX.cpp` ~530 (Tonemap+Exposure+Bloom+SSAO+SSR) <br>• `InspectorPanel_Environment_Visual.cpp` ~350 (CSM Shadows + Color Grading) |
| 2 | `src/engine/render/scene_renderer/SceneRenderer.cpp` | **894** | Ya tiene splits (`_Render`/`_Ortho`/`_Render_Lighting`). 5 funciones grandes residuales: `loadSkyboxAndIblFromBase`, `applyEnvironmentFromScene`, `synthesizeIdentityLut`, `ensureOitFb`, `endFrame` | **Extract environment** a `SceneRenderer_Environment.cpp` ~300 LOC (loadSkybox + applyEnv). Core baja a ~600. |
| 3 | `src/editor/panels/scene/InspectorPanel_Internal.h` | **885** | Header-only con templates: single-edit (helpFn / inspectorResetButton / inspectorBrokenRefBorder / fieldDragFloat3 / fieldColorEdit3 / etc.) + multi-edit (5 templates pesados de 60-150 LOC c/u) + `componentKeyForT` | **Split multi-edit** a `InspectorPanel_Internal_MultiEdit.h` ~370 LOC. Single-edit queda en `_Internal.h` ~515 LOC. |
| 4 | `src/engine/scene/serialization/SceneLoader.cpp` | **819** | Solo **1 LOC sobre cap**. Tiene 1 función gigante `applyEntitiesToScene` que materializa N tipos de componentes (Camera/Joint/Ragdoll/Cloth/Vehicle/Audio/Health/etc.) | **Diferible**: marginal sobre cap. Si F4H6 (enemy serialization) lo empuja sobre 850, se splittea entonces. Hoy NO TOCAR. |

### A.2 — Zona warning 501-800 (42 archivos)

**Top-10 de la zona warning**, ordenados por tamaño:

| Archivo | LOC | ¿Split sano? |
|---|---|---|
| `editor/application/EditorApplication.h` | 788 | ⚠️ Header de la clase principal. Splitear afectaría inclusión cruzada. **DIFERIR**. |
| `editor/application/EditorApplication_Run.cpp` | 777 | ⚠️ Frame loop con todas las fases (input/physics/scripts/render). Ya estructurado por bloques numerados. **DIFERIR**. |
| `engine/game/overlay/GameOverlay_Inventory.cpp` | 772 | Posible split: panel grande con N sub-secciones (slots/grid/equipment). Bajo uso runtime; F4 lo va a tocar. **DIFERIR a F4 contenido**. |
| `editor/application/DemoSpawners_Drop.cpp` | 759 | Demo seeds + N spawners hardcoded. Posible candidato deadcode si los demos no se usan más. **VERIFICAR** (sección D). |
| `editor/panels/scene/InspectorPanel.cpp` | 750 | Dispatcher + categorías. Bien organizado. **DIFERIR**. |
| `engine/game/overlay/GameOverlay.cpp` | 749 | HUD principal. F4H3 lo va a tocar (HUD de combate). **DIFERIR a F4H3**. |
| `editor/application/EditorProjectActions_CreateEntity.cpp` | 735 | F4H1 lo subió +66 LOC (handleAddDummy). 4 handlers grandes (CreateEntity con kit / Light / Environment / Dummy). **OK por ahora**, agendizar split si F4H6+ suma más kits. |
| `engine/render/scene_renderer/SceneRenderer_Render.cpp` | 720 | 4 pases PBR (static/skinned/brush/translucent). Posible split por pase. **DIFERIR**. |
| `editor/panels/assets/AssetBrowserPanel_ImportVehicle.cpp` | 706 | Modal grande de import. **DIFERIR** (raramente tocado). |
| `editor/panels/assets/ShaderGraphEditorPanel.cpp` | 698 | Editor de shader graph. **DIFERIR**. |

### A.3 — Archivos F4H1 / F3H28-31 (recién agregados)

| Archivo | LOC | Estado |
|---|---|---|
| `engine/gameplay/Health.cpp` | 95 | ✅ Sano |
| `engine/gameplay/Health.h` | 44 | ✅ Sano |
| `editor/panels/scene/InspectorPanel_Health.cpp` | 71 | ✅ Sano |
| `engine/scripting/bindings/LuaBindings_Health.cpp` | 82 | ✅ Sano |
| `editor/panels/scene/InspectorPanel_Groups.cpp` | 136 | ✅ Sano |
| `editor/panels/scene/InspectorPanel_MapTools.cpp` | 191 | ✅ Sano |
| `editor/panels/scene/InspectorPanel_Materials.cpp` | 356 | ✅ Sano (bajo soft cap) |
| `editor/ui/SnapPopoverContent.cpp` | 177 | ✅ Sano |
| `engine/render/sky/HosekWilkie.cpp` | 172 | ✅ Sano |
| `engine/render/sky/IBLBaker.cpp` | 161 | ✅ Sano |
| `engine/render/sky/ProceduralSkyRenderer.cpp` | 142 | ✅ Sano |

Total recientes: 11 archivos / 1627 LOC sanos. Indica que el dev (y el agente) tienen disciplina manteniendo el cap al crear archivos nuevos.

---

## B. Duplicación

### B.1 — Patrones que parecen duplicados pero NO son spaghetti

| Patrón | Callsites | Análisis | Acción |
|---|---|---|---|
| `if (m_scene == nullptr) return;` / `if (!m_scene) return;` | 38 | Guard de entrada a funciones que requieren scene. Idiomatic; extraerlo a macro lo haría menos legible. | **NO TOCAR** |
| `if (!m_assetManager)` | 24 | Idem guard de assets. | **NO TOCAR** |
| `findByTag(scene, tag)` en Lua bindings | 5 archivos | Cada `LuaBindings_*` redefine este helper anonymous-namespace. **Patrón duplicado real**. | **CANDIDATO**: extraer a `engine/scripting/bindings/BindingsCommon.h` con `findEntityByTag(Scene&, std::string)` inline. Beneficio: 1 cambio si cambia la convención de tags (ej. tags duplicados con escenarios). |

### B.2 — Helpers ya extraídos (buena disciplina)

- `inspectorResetButton<T>` template — usado en 30+ callsites del Inspector. ✅
- `pushEditIfDone<T>` template — usado en todos los sliders del Inspector. ✅
- `beginComponentSection<T>` — todos los `renderXxxSection`. ✅
- `multiEditDragFloat / Color3 / Color4 / Checkbox / Combo` — multi-select del Inspector. ✅

---

## C. Deadcode

### C.1 — Deadcode preservado intencionalmente

| Función | Estado | Razón |
|---|---|---|
| `Csg::unionOp` | Usado solo por `BooleanOpCommand` (que es invocable internamente pero el menú UI ya no lo expone tras F3H26 D4) | Forward-compat: si emerge modelo non-convex futuro (BMesh), el algoritmo se reactiva sin reescribir. Documentado en F3H26 DECISIONS D4. **NO TOCAR**. |

### C.2 — Candidatos a verificar como deadcode real

| Archivo | LOC | Sospecha |
|---|---|---|
| `editor/application/DemoSpawners_Drop.cpp` | 759 | ⚠️ ~~Spawners de demos pre-Fase 3~~ **NO ES DEADCODE.** Verificado: es el handler de **drops del viewport** (textura/mesh/material/script al área 3D, F2H24 + F3H17). Nombre "Demo" es histórico — el módulo se renombró internamente pero el archivo conservó el prefix por costo de cambios cruzados. **NO TOCAR.** |

### C.3 — TODOs / FIXMEs

- **0 TODOs reales** en `src/`. Disciplina ✅.

---

## D. Resumen priorizado para el dev

### TIER 1 — Fixes recomendados en F4H1.5 (sobre hard cap)

1. ~~**Split `InspectorPanel_Environment.cpp`** 1149 → 3 archivos por familia~~ **AGENDIZADO A F4H1.6**: el split requiere extraer ~180 LOC de infraestructura común (kEnvDefaults + EditEnvironmentSubsetCommand class + 2 helpers como template) a un header compartido. Riesgo > beneficio inmediato — el archivo no se toca en F4H2-F4H11 (combate). Sub-hito propio con plan dedicado.
2. ✅ **Extract `SceneRenderer_Environment.cpp`** de `SceneRenderer.cpp` 894 → **554 + 298** (extracción de `loadSkyboxAndIblFromBase` + `applyEnvironmentFromScene`). Build verde.
3. ✅ **Split `InspectorPanel_Internal.h`** 885 → **412 + 506** (multi-edit extraído a `_Internal_MultiEdit.h`). Build verde.

### TIER 2 — Cleanup opcional (mejora real pero menor)

4. **Extract `findEntityByTag`** a helper compartido en bindings. ~10 min. Beneficio: futuro hito de tags-duplicados toca 1 archivo en lugar de 5.

### TIER 3 — Verificar antes de actuar

5. **`DemoSpawners_Drop.cpp` 759 LOC**: ¿se sigue usando? Si no, mover a `archive/` o borrar. ~5 min de revisión + acción.

### TIER 4 — Diferidos a hitos futuros

6. `SceneLoader.cpp` 819 → diferir a F4H6 (cuando enemy serialization lo empuje).
7. Zona warning 501-800 (42 archivos restantes): aceptables para soft cap, splittear oportunísticamente cuando se toquen.

---

## Apéndice — Stats del codebase

- **Total archivos cpp/h/inl auditados**: ~250.
- **LOC totales src + tests**: 122,592 (incluyendo `hosek_data_rgb.inl` 3832 dataset).
- **Archivos > 500 LOC**: 46 (18% del total).
- **Archivos > 800 LOC (hard cap)**: 4 (1.6%). Tier 1 los baja a 0.
- **TODOs reales**: 0.

---

**Próximo paso**: dev prioriza con `AskUserQuestion` cuáles items de Tier 1-3 fixeo en F4H1.5.
