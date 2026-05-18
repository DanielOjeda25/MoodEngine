# AUDIT-3 — Tech debt sprint (cierre de la tanda)

> **Tag**: `v1.49.3-audit-3`
> **Fecha**: 2026-05-17
> **Cadencia**: Último audit de la tanda inicial. Próxima AUDIT-4 reservada
> a si emerge dolor real (cada ~5 hitos como base, pero **solo si tiene
> contenido**). Hoy el HARD cap está limpio y el layer audit en cero —
> nada urgente queda.

## Objetivo cumplido

Cerrar los pendientes documentados en AUDIT-2: split del último HARD-cap
violator + extracción del primer pure helper como warmup DOD + fix de
las 2 violaciones de layer detectadas. Time budget: ~3h reales vs 3-4h
estimadas.

---

## Bloque A — Layer fixes (Workspace + i18n)

### Antes (post-AUDIT-2)

2 violaciones detectadas:

| Violación | Direction | Severity |
|---|---|---|
| `src/engine/scene/serialization/ProjectSerializer.h:17` → `editor/workspace/Workspace.h` | engine → editor | Media |
| `src/core/UserSettings.h:14` → `engine/i18n/I18n.h` | core → engine | Baja |

### Cambios aplicados

1. **`Workspace` struct** movido a `src/engine/project/Workspace.h` (era
   `editor/workspace/Workspace.h`). El struct es PURO (sin ImGui ni UI)
   y siempre perteneció conceptualmente al engine — modela el layout
   serializado del editor, pero el tipo en sí es agnóstico.
   - `WorkspaceManager` queda en `editor/workspace/` (es la UI de
     switching — sigue siendo editor-only).
2. **`I18n.h/cpp`** movido de `src/engine/i18n/` a `src/core/i18n/`.
   Carga tablas de traducción JSON; no depende de nada engine-specific
   y `core/UserSettings.h` lo necesita para `t()` (free function).
3. Updates de includes: 55 archivos `.cpp/.h` (replace mecánico
   `engine/i18n/I18n.h` → `core/i18n/I18n.h`) + 2 archivos para
   `Workspace.h` + `CMakeLists.txt` + `tests/CMakeLists.txt`.

### Resultado

**0 violaciones de layer** (verificado con grep cross-layer). El
codebase respeta jerarquía estricta `core/` ← `engine/` ← `editor/` /
`player/` / `systems/`.

---

## Bloque B — Split `tests/test_scene_serializer.cpp` 934 LOC

### Estrategia

Partición por **familia de componente** (no por test individual). Cada
TEST_CASE testea el round-trip save+load de un componente; agrupar por
dominio resulta en tres archivos comprensibles que se editan juntos
cuando se modifica el serializer de esa familia.

### Antes → Después

| Archivo | LOC antes | LOC después |
|---|---|---|
| `tests/test_scene_serializer.cpp` | 934 | **302** (core + grid + entities + Environment + upgraders v1/v6) |
| `tests/test_scene_serializer_lighting_physics.cpp` | — | **210** (Light + RigidBody + friction) |
| `tests/test_scene_serializer_gameplay.cpp` | — | **399** (Dialog + ItemPickup + Animator + Inventory) |
| `tests/test_scene_serializer_helpers.h` | — | **39** (NullTexture + nullFactory + tempPath compartido) |

**TOTAL: 950 LOC (3 archivos test + 1 header)** vs 934 LOC monolítico.
Diferencia +16 LOC = boilerplate de includes repetidos en cada
sibling. Trade-off aceptable a cambio de archivos navegables.

### Patrón establecido: `_helpers.h` para test fixtures

Análogo al `_Internal.h` de los splits de source (AUDIT-1/2): los
helpers que comparten varios archivos hermanos de test viven en un
header sibling con namespace dedicado (`Mood::SceneSerializerTests`).
NullTexture, `nullFactory()`, `tempPath()` quedaron portados allí.

---

## Bloque C — Pure helper `pickRayFromNdc` extracción

### Motivación (heredada de AUDIT-2 Bloque D)

El patrón de reconstrucción ray-en-world desde un punto NDC aparecía
**6 veces** inline en el codebase:

- `engine/scene/queries/ViewportPick.cpp:21-30`
- `engine/scene/queries/ScenePick.cpp:197-204`
- `editor/application/EditorOverlay_Internal.h:47-55` (helper inline ya
  pero duplicado del pattern)
- `editor/application/EditorApplication_RunInteractions.cpp:88-96`
- `editor/application/EditorRenderPass_Overlay.cpp:364-372`

Pattern repetido:
```cpp
const glm::vec4 nearH = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
const glm::vec4 farH  = invVP * glm::vec4(ndcX, ndcY, +1.0f, 1.0f);
if (nearH.w == 0.0f || farH.w == 0.0f) return ...;
const glm::vec3 nearW = glm::vec3(nearH) / nearH.w;
const glm::vec3 farW  = glm::vec3(farH)  / farH.w;
const glm::vec3 dir   = glm::normalize(farW - nearW);
```

### Implementación

Nuevo header `src/core/math/Ray.h` con dos free functions:

```cpp
struct Ray { glm::vec3 origin; glm::vec3 direction; };
struct NearFar { glm::vec3 nearW; glm::vec3 farW; };

std::optional<NearFar> unprojectNearFar(const glm::mat4& invVP,
                                        float ndcX, float ndcY);

std::optional<Ray> pickRayFromNdc(const glm::mat4& invVP,
                                  float ndcX, float ndcY);
```

`pickRayFromNdc` normaliza la dirección (caso 95% de uso —
ray-intersection-against-AABB / faces). `unprojectNearFar` deja los
puntos crudos para callers que prefieren el segmento sin normalizar
(`ViewportPick` y plane intersection — el parámetro `t` queda en
unidades fraction-of-segment).

### Tests (`tests/test_ray.cpp` — 7 cases)

1. Centro NDC + perspective → ray apunta a -Z desde el near plane.
2. Esquina superior-derecha + FOV 90 → componentes +X, +Y, -Z.
3. Ortho → todas las direcciones paralelas, origenes diferentes.
4. `unprojectNearFar` devuelve near/far sin normalizar.
5. `invVP` degenerada (W=0) → `nullopt` para ambas APIs.
6. Longitud unitaria preservada para NDCs arbitrarios (5 samples).

### Call sites actualizados

| Archivo | Reducción |
|---|---|
| `ViewportPick.cpp` | -8 LOC (inline → `unprojectNearFar` 1 call) |
| `ScenePick.cpp` | -10 LOC |
| `EditorOverlay_Internal.h::cameraRayFromScreen` | -8 LOC |
| `EditorApplication_RunInteractions.cpp` (face pick) | -7 LOC |
| `EditorRenderPass_Overlay.cpp` (hover) | -9 LOC |

**~42 LOC de duplicación eliminada + ~135 LOC de tests headless** =
patrón DOD validado: extracción + tests + reemplazo + zero
regressions en la suite.

---

## Bloque D — Split `EditorUI.h` 836 LOC por categoría

### Estrategia elegida (Opción 2 del scoping)

**Inline bodies fuera del header, declaraciones quedan**. Los pares
request/consume eran ~30 funciones, ~4-6 LOC cada una en el header.
Solución: mover los bodies a archivos `.inl` sibling, agrupados por
categoría, y `#include`-los al final de `EditorUI.h`. Mantiene API
exacta (siguen siendo inline para el linker) sin pImpl invasivo.

### Particiones

| Archivo | LOC | Categoría |
|---|---|---|
| `src/editor/ui/EditorUI.h` | 836 → **483** | Declaraciones + miembros + accessors de panel |
| `src/editor/ui/EditorUI_Spawn.inl` | — → **143** | 19 pares request/consume de demos spawn |
| `src/editor/ui/EditorUI_Tools.inl` | — → **79** | Gizmo / sub-mode / tools / snap / carve / TogglePlay |
| `src/editor/ui/EditorUI_Entity.inl` | — → **54** | Create / convert / delete / save prefab |
| `src/editor/ui/EditorUI_Project.inl` | — → **56** | Recents / maps / project actions / boolean op |

**EditorUI.h: 836 → 483 LOC (-353, -42%)**. Bajo el SOFT cap por
primera vez en la historia del archivo.

### Convención nueva: `EditorUI_*.inl`

- `.inl` lives next to `.h` con el mismo prefix.
- Header: solo declaraciones + private members + comments.
- `.inl`: solo inline bodies de métodos de la clase enclosing,
  agrupados por dominio.
- `#include`d al final del `.h` (después del `} // namespace`) para
  que la clase ya esté declarada cuando se ven las definiciones.
- No incluir un `.inl` desde otro módulo — son siempre transitivos
  vía el `.h` correspondiente.

---

## Resultado consolidado de la tanda de audits

```
                      AUDIT-1   AUDIT-2   AUDIT-3
HARD cap violations:    5         2         0
Layer violations:       2*        2         0
Pure helpers extracted: 0         0         1 + 7 tests
Per-hito files:         1         8         8 (sin cambio)
```

`*` AUDIT-1 no midió layer violations explícitamente; AUDIT-2
descubrió las 2 que AUDIT-3 cierra.

**Top archivos (post-AUDIT-3)**:

```
   788 SOFT src/engine/render/scene_renderer/SceneRenderer_Render.cpp
   786 SOFT src/editor/application/EditorApplication.h
   772 SOFT src/engine/game/overlay/GameOverlay_Inventory.cpp
   724 SOFT src/engine/game/overlay/GameOverlay.cpp
   688 SOFT tests/test_quest_system.cpp
   688 SOFT src/editor/panels/assets/ShaderGraphEditorPanel.cpp
```

24 archivos en SOFT cap (501-800 LOC) — bajo umbral de "atender pronto"
pero ninguno en zona roja. Próximos candidatos de split natural si
emergen: `SceneRenderer_Render.cpp` y `EditorApplication.h`.

---

## Convenciones consolidadas tras la tanda

### Tamaño

- **Hard cap 800 LOC, soft cap 500 LOC** para `.cpp` y `.h`. Headers
  grandes son aceptables si su contenido es declarativo y testeable
  por composición (no requiere pImpl).
- **Sizeometer obligatorio** antes de cierre de hito y antes de cierre
  de audit. `tools/sizeometer.sh 30 > docs/audits/AUDIT_N_before.txt`.

### Split patterns establecidos

- `<Nombre>_Internal.h` — helpers compartidos entre `.cpp` siblings
  (namespace dedicado). Ejemplos: `DemoSpawners_Internal.h`,
  `EditorOverlay_Internal.h`, `EditorProjectActions_CreateEntity_Internal.h`,
  `GameOverlay_Internal.h`.
- `<Nombre>_<Family>.inl` — inline bodies de métodos de la clase
  enclosing (AUDIT-3 nuevo). Ejemplos: `EditorUI_Spawn.inl`,
  `EditorUI_Tools.inl`, `EditorUI_Entity.inl`, `EditorUI_Project.inl`.
- `<Nombre>_<Family>.cpp` — split de una clase grande por sub-feature.
  Ejemplos: `EditorApplication_Run.cpp`, `EditorApplication_Init.cpp`,
  `EditorRenderPass_Overlay.cpp`, `GameOverlay_Inventory.cpp`,
  `EditorProjectActions_CreateEntity_PickModal.cpp`.

### Tests

- **Tests de helpers** (test_ray.cpp model): headless, no GL, cubre
  golden path + edge cases + degenerate input. ~5-10 cases por helper.
- **Splits de test files**: mismo patrón de `_helpers.h` para fixtures
  compartidas (AUDIT-3 nuevo).

### Layer

- `core/` solo `std` + `glm` + dentro de `core/`.
- `engine/` puede `core/` + libs. NO `editor/`, `player/`, `tests/`.
- `editor/`, `player/`, `systems/` pueden `engine/` + `core/`.
- Verificable con grep cruzado — 0 violaciones es realista y mantenible.

---

## Pendientes futuros (sin urgencia)

### Code

1. **Pure helpers candidatos restantes** (de AUDIT-2 Bloque D):
   `aabbFromTwoPoints`, `snapWorldPositionToGrid`, `groupClickedFaceByBrush`.
   Extraer si emerge demanda; no medida de impacto urgente.
2. **Hot paths DOD profiling** — solo si tooling de perf justifica.
3. **Dead code audit** — el compilador ya warning-iza; bajo retorno
   estimado.

### Docs

4. **Backfill F2H1-F2H54** a per-hito files si emerge fricción
   operacional. Hoy no la hay — son entries inmutables, no se editan.

### Refactors invasivos diferidos

5. **`EditorUI.h` pImpl real** (queda en 483 LOC tras Bloque D — bajo
   SOFT cap). pImpl sigue siendo opción para si la clase crece a 700+
   LOC. Hoy no es necesario.

---

## Costo real vs estimado

- **Bloque A**: ~30 min (mecánico — `git mv` + replace + build verify).
- **Bloque B**: ~45 min (3 archivos test reescritos + helper header +
  CMake updates).
- **Bloque C**: ~50 min (helper + 7 tests + 5 call sites actualizados).
- **Bloque D**: ~45 min (4 archivos `.inl` + reescritura de `.h`).
- **Cierre**: ~30 min (este reporte + commit + tag).
- **Total**: ~3h reales (estimado 3-4h). Tercer audit consolidó la
  curva — patrones reutilizados sin fricción.

---

## Cierre de la tanda

Tres audits, ~10h totales, **0 HARD cap violations + 0 layer
violations + primer pure helper + 7 tests headless**. La cadencia
"1 audit cada ~5 hitos" funcionó: cada uno cerró un tipo distinto de
deuda y el segundo aprendió del primero.

Próximo audit: **solo si emerge dolor concreto**. No agendar audits
vacíos por cumplir la cadencia.
