# AUDIT-1 — Tech debt sprint inter-hitos (post-F2H62)

> **Tag**: `v1.49.1-audit-1`
> **Fecha**: 2026-05-17
> **Cadencia establecida**: 1 audit cada ~5 hitos grandes. AUDIT-2 previsto post-F2H67 (aprox).

## Objetivo cumplido

Bajar deuda técnica acumulada en 62 hitos de Fase 2 **sin features nuevas**. Audit acotado a 1 día. Scope acordado con el dev: Bloques A + E + cierre.

---

## Bloque A — Splits de archivos sobre el hard cap (800 LOC)

### Antes (top 20 por LOC, pre-audit)

```
  1617 HARD src/editor/application/EditorApplication_Run.cpp
   936 HARD src/engine/game/overlay/GameOverlay_Inventory.cpp
   934 HARD tests/test_scene_serializer.cpp
   905 HARD src/engine/game/overlay/GameOverlay.cpp
   888 HARD src/editor/application/EditorRenderPass.cpp
   836 HARD src/editor/ui/EditorUI.h
   813 HARD src/editor/application/EditorProjectActions_CreateEntity.cpp
   788 SOFT src/engine/render/scene_renderer/SceneRenderer_Render.cpp
   ...

TOTAL: 454 archivos, 82798 LOC.
HARD cap (>800): 7 archivos.
SOFT cap (501-800): 18 archivos.
```

### Splits ejecutados (top 3 ofensores)

| Antes | Después | Δ | Nuevo archivo |
|---|---|---|---|
| `EditorApplication_Run.cpp` 1617 | 607 LOC | **-1010** | `EditorApplication_RunInteractions.cpp` 1065 LOC (extracción de `processViewportInteractions()`) |
| `GameOverlay_Inventory.cpp` 936 | 772 LOC | **-164** | `GameOverlay_InventorySplit.cpp` 203 LOC (F2H52 Bloque I container split mode) |
| `GameOverlay.cpp` 905 | 724 LOC | **-181** | `GameOverlay_DialogBox.cpp` 115 LOC + `GameOverlay_PauseMenu.cpp` 113 LOC |

### Después (top 15 por LOC, post-audit)

```
  1065 HARD src/editor/application/EditorApplication_RunInteractions.cpp   ← nuevo, AUDIT-2 candidate
   934 HARD tests/test_scene_serializer.cpp                                ← test file
   888 HARD src/editor/application/EditorRenderPass.cpp                    ← AUDIT-2 candidate
   836 HARD src/editor/ui/EditorUI.h                                       ← header, AUDIT-2
   813 HARD src/editor/application/EditorProjectActions_CreateEntity.cpp   ← AUDIT-2 candidate
   788 SOFT src/engine/render/scene_renderer/SceneRenderer_Render.cpp
   779 SOFT src/editor/application/EditorApplication.h
   772 SOFT src/engine/game/overlay/GameOverlay_Inventory.cpp              ← spliteado en este audit
   724 SOFT src/engine/game/overlay/GameOverlay.cpp                        ← spliteado en este audit
   ...

TOTAL: 458 archivos (+4 splits), 82996 LOC (+198 por boilerplate de namespace/includes).
HARD cap: 7 → 5 archivos.
SOFT cap: 18 → 21 archivos.
```

**Resultado neto**: 2 archivos salieron del hard cap. El #1 ofensor (`EditorApplication_Run.cpp` 1617 LOC) bajó a 607 (debajo del SOFT cap). El nuevo `_RunInteractions.cpp` 1065 LOC reemplaza la posición #1 — focused responsibility, candidato a sub-split en AUDIT-2.

### Convención establecida durante el split

Patrón **archivos parciales sobre la misma clase** (existente en el repo desde F2H10+):
- Para `EditorApplication`: nuevo método privado declarado en `EditorApplication.h`, implementado en `EditorApplication_<Suffix>.cpp`.
- Para namespaces sin clase (ej. `Mood::GameOverlay`): forward-declare en `<Module>_Internal.h`, definir en `<Module>_<Topic>.cpp`. Promovido `displayNameOfItem` de anonymous namespace a public namespace para permitir reuso entre `.cpp` siblings.

### Sizeometer (`tools/sizeometer.sh`)

Script bash, multiplataforma, sin dependencias. Reporta top N archivos `.cpp`/`.h` por LOC con flag HARD (>800) / SOFT (501-800) / OK. Excluye `build*/`, `_deps/`, third-party. Patrón de invocación:

```bash
tools/sizeometer.sh 30        # top 30
tools/sizeometer.sh 30 > docs/audits/AUDIT_N_before.txt  # snapshot
```

---

## Bloque E — Slim de docs

### Antes

| Archivo | Líneas |
|---|---|
| `docs/HITOS.md` | 197 (1 paragraph de ~10K chars por hito Fase 2) |
| `docs/ESTADO_ACTUAL.md` | **1464** (12+ secciones "0bis" acumuladas sin compactar) |

### Después

| Archivo | Líneas | Cambio |
|---|---|---|
| `docs/HITOS.md` | 201 | F2H62 fat → one-liner (~10K chars → ~250 chars). Older entries quedan inline. |
| `docs/ESTADO_ACTUAL.md` | **161** | -1303 líneas (-89%). Reescritura completa: keep header + AUDIT-1 + F2H62 slim + plan + operational docs (entorno build, comandos, gotchas). |
| `docs/hitos/F2H62.md` | 116 | nuevo — detalle full del último hito |
| `docs/hitos/README.md` | 56 | nuevo — convención de format |

### Convención nueva (desde F2H62 / AUDIT-1)

- **`HITOS.md`** = índice. 1 línea por hito (`F2H<N> — <título> — tag — fecha — [detalle](hitos/F2H<N>.md)`).
- **`docs/hitos/F2H<N>.md`** = detalle largo del hito (bloques, decisiones, citas del dev, limitaciones). Inmutable post-cierre.
- **`ESTADO_ACTUAL.md`** = solo último hito + último audit + pendientes. **Sin acumular "0bis"** — la sección 0 se reemplaza en cada cierre.
- **`DECISIONS.md`** = append-only, se navega por búsqueda.

### Backfill diferido

Los entries F2H1-F2H61 quedan inline en `HITOS.md` con formato fat. Migración a per-hito files agendada para AUDIT-2 si el dolor de mantenimiento lo justifica (hoy son inmutables — no se editan, no generan fricción operacional).

---

## Convenciones nuevas para futuros hitos

(Establecidas en este audit, documentadas en `ESTADO_ACTUAL.md` section 4 "Qué hacer al arrancar próxima sesión".)

1. **Cierre de hito**: detalle largo va a `docs/hitos/F2H<N>.md`. HITOS.md solo gana 1 línea.
2. **ESTADO_ACTUAL.md**: cada cierre **reemplaza** la sección 0 (no se acumulan "0bis").
3. **Archivo nuevo o modificado >800 LOC**: split obligatorio antes de commitear el hito.
4. **Sizeometer**: correr antes de cada cierre de hito para verificar que no creamos nuevos ofensores.

---

## AUDIT-2 — Candidatos identificados

Para el próximo audit (post-~5 hitos grandes, aprox F2H67):

### Code (5 archivos sobre HARD cap)

1. `EditorApplication_RunInteractions.cpp` 1065 LOC — sub-split por sub-modos (face/vertex/edge/polygon/clip/marquee).
2. `test_scene_serializer.cpp` 934 LOC — split por TEST_CASE families.
3. `EditorRenderPass.cpp` 888 LOC — extraer cada pass como helper file.
4. `EditorUI.h` 836 LOC — extraer paneles secundarios a forward decls.
5. `EditorProjectActions_CreateEntity.cpp` 813 LOC — split por tipo de entity (mesh / primitive / light).

### Nuevos audits propuestos (no en AUDIT-1 por scope)

- **Layer dependency audit**: grep de `#include "editor/"` desde `engine/`, etc. Cero violaciones permitidas.
- **Pure helpers audit**: identificar lógica entremezclada con GL/ImGui y extraer free functions testeables.
- **Dead code audit**: funciones/clases sin referencias.

---

## Lecciones aprendidas durante el audit

1. **Forward decl `struct` vs `class` en MSVC**: declarar `struct Foo` cuando la definición real es `class Foo` rompe el name mangling en MSVC (no en GCC) → link error críptico tipo "unresolved external". Lección: siempre matchear el tipo exacto (`class` o `struct`) al forward-declarar.
2. **Anonymous namespaces no se comparten entre TUs**: las funciones definidas en `namespace { ... }` son locales al .cpp. Para reusar entre siblings, hay que promover al namespace público (con declaración en header compartido).
3. **Splits de funciones grandes**: cuando una función monolítica usa solo `this->m_*` y getters globales, extraerla como método privado es mecánico y de bajo riesgo. Si usa locales del scope outer, requiere pasarlos como parámetros (más invasivo).
4. **El sizeometer es trivial pero alto valor**: 30 minutos de scripting → métrica continua. Ya estaba en la memoria del agente como regla soft 500/hard 800, pero sin medición no se enforced.

---

## Costo

- **Bloque A**: ~3.5 hs (split + diagnose link error + rebuild verde).
- **Bloque E**: ~1.5 hs (refactor docs + slim ESTADO_ACTUAL + per-hito file).
- **Cierre**: ~30 min (este reporte + commit + tag).
- **Total**: ~5.5 hs. Dentro del budget de 1 día acordado.
