# Plan — Hito 42: Editor de materiales visual minimal (cerrado)

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 42 cerrado).

---

## Estado

**Hito 42 cerrado** (`v0.42.0-hito42`, suite **319/6613**). Último item grande del backlog post-`v1.0.0`. Versión **lite** acordada con el dev: panel dedicado para edición de materiales standalone, sin requerir entidad seleccionada en el viewport. **NO incluye** preview esférico off-screen ni node-graph — esos quedan para Fase 2.

## Bloques cerrados

- [x] **A — `MaterialEditorPanel`**: nuevo panel dock-able. Combo con todos los `MaterialAsset` del AssetManager. Sliders escalares (`albedoTint`, `metallic`, `roughness`, `ao`) idénticos al Inspector. Drop slots para texturas (`albedo`, `metallic_roughness`, `normal`, `ao`) reusando el payload `MOOD_TEXTURE_ASSET` del AssetBrowser. Botón `X` por slot para limpiar la asignación.
- [x] **B — Integración**: registrado en `EditorUI::m_panels` (visible=false por default — togglear en menú "Ver > Material Editor"). `EditorApplication` inyecta el `AssetManager`. Agregado `MaterialEditorPanel.cpp` al CMakeLists.
- [x] **C — Cierre**: docs (HITOS.md, DECISIONS.md, ESTADO_ACTUAL.md), tag `v0.42.0-hito42` + push.

## Lo que NO se cubrió (Fase 2)

- **Preview esférico off-screen**: panel embebido con un viewport renderizando una esfera con el material actual. Requiere FBO chico + mini-scene + render dedicado. Documentado.
- **Node graph (estilo Substance/UE)**: feature mayor con su propio hito. Por ahora los materiales son sliders + texture slots planos.
- **Save As `.material`** desde el panel: para crear materiales nuevos sin pasar por el editor de archivos.
- **Undo/Redo en el panel**: el Inspector tiene undo (Hito 32-36); este panel no — diff intencional para v1.

---

## Decisiones tomadas

Las nuevas en `DECISIONS.md` (2026-05-02):
- MaterialEditor V1 lite (sin preview esférico, sin node-graph) — el dev firmó "a futuro deberemos mejorar todo eso".

---

## Verificación visual del dev (criterios cumplidos)

- En el editor, "Ver > Material Editor" muestra el panel.
- Combo lista todos los materiales registrados (incluye los del fallback default + los `.material` cargados del repo).
- Drop de textura desde Asset Browser sobre un slot → log + cambio inmediato visible en cualquier entidad que use ese material.
- Sliders escalares afectan el render en tiempo real.
- Suite 319/6613 sin regresiones.
