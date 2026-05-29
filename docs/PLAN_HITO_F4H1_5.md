# PLAN F4H1.5 — Code audit pre-F4H2 + fixes priorizados

**Estado:** ✅ **CERRADO** (2026-05-29, tag `v3.1.1-fase4-hito1-5`).
**Predecesor:** F4H1 (sistema de salud/daño, tag `v3.1.0-fase4-hito1`).
**Origen:** pedido del dev al cerrar F4H1:
> *"creo que previamente deberiamos hacer una auditoria de codigo, porque vamos a escribir mucho codigo, y lo ideal es no tener archivos enormes, que no pasen de 500 lineas, codigo spaguetti repetitivo, deadcode"*

---

## Norte

F4H2-F4H11 (sistemas core de Fase 4: armas, IA, oleadas) van a sumar **bastante** código nuevo. Antes de escribirlo, queremos partir limpio: archivos bajo el soft cap LOC, sin deadcode, sin patrones repetidos sin extraer. Gemelo del F3H3 audit (que cubrió hardcoded values pre-F3H4-F3H7); F4H1.5 cubre estructura de código.

Quick audit, no full sweep. **3 buckets:**
- **A. LOC**: archivos > 500 LOC (soft cap memoria `feedback_file_size_limit`) → candidatos a split.
- **B. Duplicación**: patrones repetidos 5+ veces sin helper → candidatos a extraer.
- **C. Deadcode**: funciones/clases no llamadas + `Csg::unionOp` style code muerto + includes huérfanos.

**Lo que NO cubre F4H1.5** (agendizable a hitos propios si emerge demanda):
- Full audit de hardcoded post-F3H29+ (Fase 3 lo cubrió hasta F3H7).
- Refactor de arquitectura mayor (ej. ECS systems sin clase base unificada).
- Migrate de código C++ a Lua (data-driven es F4H2+ work).

---

## Plan

**Phase 1 — Sweep automatizado LOC.** `find + wc -l` sobre `src/` + `tests/`. Top-N archivos > 500 LOC listados en tabla.

**Phase 2 — Sweep manual sobre top-N + archivos F4H1 + F3H28-31** (modificados recientemente). Identificar:
- Funciones/clases no llamadas (grep símbolo definido vs usado).
- Patrones repetidos 5+ veces (ej. `if (!m_scene) return; if (!m_assetManager) return;` inline en cada handler).
- Includes huérfanos.

**Phase 3 — Output**: `docs/CODE_AUDIT_F4_PRE.md` con tabla `archivo → diagnóstico → propuesta acción` (split / extract / delete / no-op).

**Phase 4 — Dev prioriza**: AskUserQuestion con TOP-N items + decide cuáles fixear en este hito vs agendizar.

**Phase 5 — Fixes reactivos**: aplicar los elegidos. Cada fix lleva tests si toca lógica; el split de archivos solo si toca C++ con tests existentes.

**Phase 6 — Cierre**: actualizar CODE_AUDIT_F4_PRE.md marcando items fixeados ✅ / agendizados 📝. ESTADO_ACTUAL + HITOS + DECISIONS. Tag `v3.1.1-fase4-hito1-5`.

---

## Decisiones (pre-cerradas con AskUserQuestion)

**D1 — Scope: quick audit (top-15 LOC + duplicación evidente + deadcode flagrant).** Dev eligió quick over full. Si emerge mucha deuda, F4H1.5 termina con backlog de F4H1.6 / F4H1.7 sub-hitos.

**D2 — Estructura: hito separado F4H1.5 con fixes reactivos.** Audit + dev prioriza + aplico fixes + tag. F4H2 arranca sobre código limpio. Standalone audit (sin fixes) descartado — sin fixes el código queda igual y el norte del audit se diluye.

---

## Métrica de éxito

- 0 archivos sobre hard cap (800 LOC).
- ≤ 3 archivos en zona de warning (500-800 LOC) con plan de split agendado para hito específico.
- 0 funciones/clases obviamente deadcode tras el audit (las que se conserven por forward-compat quedan documentadas).
- Top-5 patrones repetidos extraídos a helpers (si los hay).

---

## Lo que NO toca F4H1.5

- Migración masiva de código C++ a Lua data-driven — F4H2-F4H11 work.
- Re-arquitectura del render pipeline / ECS / etc.
- Tests nuevos de features (solo tests que correspondan a refactors aplicados).
- Audit de Fase 3 retrasada (hardcoded post-F3H29 — agendizable a F4H1.6 si emerge demanda).

---

## Cierre

**3 fixes aplicados:**

1. ✅ **Split `InspectorPanel_Internal.h` 885 → 412 + 506.** Multi-edit helpers (`allMatch` + 5 templates `multiEdit{Color3,DragFloat,Checkbox,Combo,Color4}`) extraídos a `InspectorPanel_Internal_MultiEdit.h`. Include desde `_Internal.h` para mantener API back-compat. Sin cambios funcionales — solo split por LOC.

2. ✅ **Extract `SceneRenderer_Environment.cpp` 894 → 554 + 298.** Los 2 métodos relacionados al Environment (`loadSkyboxAndIblFromBase` + `applyEnvironmentFromScene`) movidos a archivo aparte. Includes simétricos con el core. CMake actualizado.

3. ✅ **Extract `findEntityByTag` a `BindingsCommon.h`.** 3 copias del helper (Health/Ragdoll/Vehicle) reemplazadas por `using bindings::findEntityByTag`. Beneficio: futuro hito de Fase 4 (F4H2 weapons / F4H6 enemies) suma N bindings nuevos sin duplicar el walk. F4H2+ cambio de convención de tags toca 1 archivo en lugar de N.

**1 fix agendizado:**

4. 📝 **Split `InspectorPanel_Environment.cpp` 1149 LOC → backlog F4H1.6.** El split requiere extraer ~180 LOC de infraestructura común (`kEnvDefaults` + `EditEnvironmentSubsetCommand` class + `drawSectionResetButton` template + `drawSectionDivider`) a un header compartido. Riesgo de break > beneficio inmediato. El archivo no se toca en F4H2-F4H11 (combate). Sub-hito propio con plan dedicado.

**Verificación:** suite full `1309/11931` verde post-refactors. Build verde MoodEditor + MoodPlayer.

**LOC residual sobre hard cap 800** (2 archivos, ambos diferidos con razón):
- `InspectorPanel_Environment.cpp` 1149 → F4H1.6.
- `SceneLoader.cpp` 819 (solo 1 LOC sobre cap, marginal) → F4H6 cuando enemy serialization lo empuje.

**Decisiones cerradas durante el hito:**

- **D3 (reactivo) — `DemoSpawners_Drop.cpp` NO es deadcode.** Sospecha inicial del audit; verificación reveló que es el handler de drops del viewport (textura/mesh/material/script → área 3D del editor, F2H24 + F3H17). Nombre "Demo" es histórico. No tocar.
- **D4 (reactivo) — `InspectorPanel_Environment.cpp` split aplazado a F4H1.6.** Trade-off de scope: pulir cap LOC vs riesgo de break. Eleyido riesgo bajo + plan F4H1.6 dedicado.

**Backlog post-F4H1.5:**

- **F4H1.6** — Split `InspectorPanel_Environment.cpp` 1149 → 3 archivos (Sky+Fog / PostFX (Tonemap+Bloom+SSAO+SSR) / Visual (Shadows+ColorGrading)). Plan necesita un header `_Environment_Internal.h` con forward decls + helpers comunes como inlines/templates.
- **F4H1.7** (si emerge demanda) — Audit hardcoded post-F3H29 (extender F3H3 con valores agregados en sub-fases 3.3/3.4).
