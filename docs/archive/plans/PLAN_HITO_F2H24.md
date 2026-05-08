# Plan — F2H24: Reorganización interna del código (split de archivos grandes)

> **Leer primero:** `ESTADO_ACTUAL.md` (F2H23 cerrado), `PENDIENTES.md`
> entrada *"F2H24 candidato — Reorganización interna del código"*.
> Pedido explícito del dev tras polish iter 3 de F2H23:
> *"creo que hay archivos demasiado grandes que te cuesta arreglar,
> así que mejor debemos organizar, que ningún archivo tenga
> demasiadas líneas para que sea fácil de mantener"*.

---

## Objetivo

Atacar deuda técnica acumulada: archivos `.cpp` que violan el cap
configurado en memoria del proyecto (**soft 500 / hard 800 LOC**).
Refactor estructural sin cambio funcional — la suite tiene que pasar
extremo a extremo después de cada bloque, y el editor tiene que
arrancar idéntico al usuario final.

## Auditoría inicial (Bloque A — este archivo)

Conteo real con `wc -l` sobre el repo a 2026-05-07. Ordenado descendente:

| Archivo | LOC | Severidad |
|---|---:|---|
| `editor/panels/scene/InspectorPanel.cpp` | 1338 | **CRÍTICO** |
| `editor/application/EditorProjectActions.cpp` | 1272 | **CRÍTICO** |
| `editor/application/DemoSpawners.cpp` | 1188 | **CRÍTICO** |
| `player/PlayerApplication.cpp` | 1160 | **CRÍTICO** |
| `editor/application/EditorApplication.cpp` | 826 | **CRÍTICO** |
| `engine/render/scene_renderer/SceneRenderer.cpp` | 776 | ALTO |
| `engine/assets/loaders/MeshLoader.cpp` | 767 | ALTO |
| `editor/application/EditorOverlay.cpp` | 745 | ALTO |
| `engine/assets/manager/AssetManager.cpp` | 743 | ALTO |

**5 archivos CRÍTICOS** (>800 LOC, ~5784 LOC totales) son el target
obligatorio. Los **4 ALTO** (700-780) entran como objetivo opcional
Bloque C — solo si están a tiro y no rompen la suite. El subagente
inicial reportó números más bajos por path antiguo / archivos no
reorganizados; los reales se confirmaron con `wc -l` directo.

## Filosofía de diseño

- **Split por dominio funcional dentro de la misma clase**, siguiendo
  el patrón ya usado en el editor (`EditorApplication.cpp` →
  `EditorProjectActions.cpp` + `DemoSpawners.cpp` + `EditorOverlay.cpp`
  + `EditorPlayMode.cpp` + `EditorRenderPass.cpp` + `EditorScene.cpp`).
  Para clases con muchos métodos (InspectorPanel) → archivos parciales
  con sufijo descriptivo (`InspectorPanel_Transform.cpp`,
  `InspectorPanel_Material.cpp`, etc.) que comparten la misma clase
  declarada en `InspectorPanel.h`.
- **Cero cambio de API pública**. Headers intactos. Si emerge una
  oportunidad evidente de simplificar la firma, queda anotada en
  `PENDIENTES.md` para hito futuro.
- **Cero cambio funcional**. El editor y el player arrancan idénticos.
  Suite verde de extremo a extremo.
- **Helpers locales (anónimos en `.cpp`) viajan con su consumidor**.
  Si un helper solo lo usa una sección, va al archivo parcial de esa
  sección. Si lo usan dos, queda en el archivo principal.
- **CMake actualizado por bloque**, no al final. Compilar y correr
  tests entre splits para detectar regresiones rápido.
- **Si un archivo parcial del split queda >500 LOC**, parar y revisar
  — significa que la división no es lo suficientemente fina o que el
  archivo padre tiene complejidad real que necesita reducción de
  responsabilidades, no solo split textual.

## Decisiones arquitectónicas

| # | Decisión | Valor |
|---|---|---|
| 1 | Approach | Split incremental por archivo CRÍTICO, validar suite tras cada uno. |
| 2 | Patrón de split | Archivos parciales con sufijo descriptivo, misma clase, sin cambio de API. |
| 3 | Scope cerrado tras | Bloque B (4 CRÍTICOS). Bloque C (ALTO) opcional según presupuesto. |
| 4 | Cambio funcional | NO. Refactor puramente estructural. |
| 5 | API pública | NO se toca. Headers intactos. |
| 6 | Tests nuevos | NO obligatorios. La suite existente debe seguir verde. |
| 7 | CMake | Actualizar por bloque, no al final. |
| 8 | Cap reafirmado | Soft 500 / hard 800 — si un partial queda >500, revisar el split. |

## Bloques

### A — Plan F2H24 + auditoría (este archivo)

Auditoría inicial cerrada en la tabla de arriba. Plan provisional. Se
ajusta tras cada bloque si la realidad lo pide.

Cierra al commit inicial: `chore(F2H24 Bloque A): plan + auditoría`.

### B — Split de los 5 CRÍTICOS

**B.1 — InspectorPanel.cpp (1253 LOC)**

Cada componente del Inspector se renderiza en un bloque independiente.
División propuesta por dominio:

- `InspectorPanel.cpp` — núcleo: `onImGuiRender`, helpers compartidos
  (`pushEditIfDone`, `helpMarker`, `isDragActiveOfType`,
  `applyDeltaToSelection`), bucle de selección.
- `InspectorPanel_Transform.cpp` — sección Transform (multi-edit).
- `InspectorPanel_MeshRenderer.cpp` — sección MeshRenderer + Material
  drop targets.
- `InspectorPanel_Light.cpp` — sección Light (Point / Directional /
  Spot).
- `InspectorPanel_Audio.cpp` — secciones AudioSource + AudioListener.
- `InspectorPanel_Physics.cpp` — secciones RigidBody + Collider.
- `InspectorPanel_Script.cpp` — sección Script con exposed properties.
- `InspectorPanel_Misc.cpp` — Tag / Camera / Brush / catch-all.

Target: cada partial **≤ 250 LOC**. `InspectorPanel.cpp` queda
≤ 350 LOC.

**B.2 — EditorProjectActions.cpp (1123 LOC)**

Mezcla 3 responsabilidades:

- `EditorProjectActions.cpp` — núcleo: dispatch, `confirmDiscardChanges`.
- `EditorProjectActions_FileIO.cpp` — `handleNewProject`,
  `handleOpenProject`, `handleSave`, `handleSaveAs`, `saveEditorState`,
  serialización.
- `EditorProjectActions_Map.cpp` — handlers de mapa
  (`handleAddBoxBrush`, `handleBooleanOp`, etc.).

Target: cada partial **≤ 450 LOC**.

**B.3 — DemoSpawners.cpp (1066 LOC)**

13 demos + drop handlers. División por categoría temática:

- `DemoSpawners.cpp` — núcleo: dispatch + helpers compartidos.
- `DemoSpawners_Enemies.cpp` — RotatorDemo, EnemyDemo, BotDemo, etc.
- `DemoSpawners_Hazards.cpp` — TrapDemo, FireDemo, etc.
- `DemoSpawners_Audio.cpp` — handlers de drop de audio + spawn
  ambiental.

Target: cada partial **≤ 400 LOC**. División final ajustada tras leer
el archivo y agrupar por afinidad real.

**B.4 — PlayerApplication.cpp (1160 LOC)**

Mezcla setup + frame loop:

- `PlayerApplication.cpp` — ctor, dtor, `run()`, dispatch input.
- `PlayerApplication_Init.cpp` — setup de SDL, GL, ImGui, sistemas
  (Physics, Script, Animation, Audio).
- `PlayerApplication_Frame.cpp` — render pass del frame, clear,
  Present.

Target: cada partial **≤ 500 LOC**.

**B.5 — EditorApplication.cpp (826 LOC)**

Confirmado CRÍTICO con conteo real. Aunque ya tiene partials, `run()`
y el ctor crecieron. Split propuesto:

- `EditorApplication.cpp` — núcleo: ctor mínimo, `run()`, dispatch
  principal.
- `EditorApplication_Init.cpp` — init de SDL, GL, ImGui, dockspace,
  font, theme.
- `EditorApplication_Systems.cpp` — setup de Physics, Script, Audio,
  Animation, AssetManager.

Target: cada partial **≤ 350 LOC**.

Cada B.X cierra con su propio commit:
`refactor(F2H24 Bloque B.X): split <Archivo> en N partials`.

### C — Split opcional de ALTO (>700 LOC) — si tiempo

Solo si Bloque B cerró rápido y la suite quedó verde sin sustos:

- `SceneRenderer.cpp` (776) — split por pase (shadow / forward / IBL).
- `MeshLoader.cpp` (767) — split por formato (OBJ / GLTF).
- `EditorOverlay.cpp` (745) — split por overlay (gizmo / grid / debug).
- `AssetManager.cpp` (743) — split por tipo de asset (texture / mesh /
  audio).

Si Bloque B fue costoso, Bloque C queda en `PENDIENTES.md` para hito
futuro.

### D — Validación final

- Build limpio Debug + Release.
- Suite completa: target ≥ 610/8359 (cifra F2H23, no debe regresionar).
- Smoke test del editor: arranque → crear proyecto → spawn cubo →
  inspector → save → reload → play → exit.
- Smoke test del player: build + run de un mapa simple.
- Verificación manual: cap respetado en todos los partials nuevos.
- Update memoria si emerge un patrón de split nuevo digno de feedback.

### E — Cierre F2H24

- `docs/HITOS.md` — entrada F2H24 con tag `v1.15.0-fase2-hito24`.
- `docs/ESTADO_ACTUAL.md` — sección "F2H24 cerrado".
- `docs/DECISIONS.md` — anotar patrón de split parcial reafirmado.
- `docs/PENDIENTES.md` — mover los ALTO no atacados (si aplica) +
  archivar entrada F2H24.
- Tag git + push origin/main.
- Counter en HITOS.md: **23/44 hitos de Fase 2 hechos**.

Commit final: `docs(F2H24 Bloque E): cierre - HITOS / ESTADO /
DECISIONS + tag`.

---

## Riesgos

- **Suite no cubre toda la lógica de los CRÍTICOS** (UI panels, demos,
  player init) — un cambio textual sin cambio funcional aparente
  podría romper algo que el test no captura. Mitigación: smoke test
  manual del editor entre cada B.X, no batch al final.
- **CMake `target_sources` con globs** podría no recoger los nuevos
  partials sin `cmake --regenerate`. Mitigación: enumerar sources
  explícitos donde aplique, regenerar build/ entre bloques.
- **Dependencias circulares ocultas** — un helper de InspectorPanel
  movido a un partial podría incluir un header que crea ciclo.
  Mitigación: build tras cada split antes de pasar al siguiente.
- **Scope creep**: tentación de "ya que estoy moviendo, simplifico
  esto otro". NO. Refactor puramente estructural; lo demás a
  `PENDIENTES.md`.

## Criterios de éxito

- 5 archivos CRÍTICOS quedan ≤ 500 LOC en cada partial resultante.
- Suite verde extremo a extremo.
- Editor + player arrancan idéntico al usuario.
- API pública (headers) intacta.
- Tag publicado.
