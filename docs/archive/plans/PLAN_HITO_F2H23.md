# Plan — F2H23: Pase polish UX continuo (Inspector / Hierarchy / Console / StatusBar)

> **Leer primero:** `ESTADO_ACTUAL.md` (F2H22 cerrado), `PENDIENTES.md`
> entrada *"Pase polish UX general continuo"*. Patrón heredado de F2H16
> (HistoryStack Blender-style) + F2H19 (limpieza HistoryStack residual):
> auditoría con subagente → lista priorizada → fixes acotados sin
> refactor profundo.

---

## Objetivo

F2H22 atacó workspaces (renames a tareas) + Toolbar (componente nuevo) +
AssetBrowser (tabs). Quedan **4 panels** sin auditoría UX explícita:

1. **Inspector** — el panel más usado del editor (toda la edición de
   componentes pasa por acá).
2. **Hierarchy** — feedback de selección, iconos por tipo de entidad.
3. **Console** — filtrado por canal, colores por severidad.
4. **StatusBar** — layout y separadores visuales.

F2H23 entrega un pase de descubribilidad sobre estos 4 panels. Sin
features nuevas, sin refactor del modelo subyacente — solo polish de
UX inmediato.

## Filosofía de diseño

- **Auditoría primero, scope cerrado tras Bloque B**: subagente recorre
  el código real de cada panel, identifica fricciones concretas, y
  produce una lista priorizada (ALTA / MEDIA / BAJA). El plan se cierra
  recién en Bloque C cuando sabemos qué deuda hay realmente.
- **Acotar el scope con prioridad ≥ MEDIA**: si la auditoría encuentra
  20 ítems, atacamos los 5-8 más impactantes. El resto queda en
  `PENDIENTES.md` para hito futuro.
- **Sin refactor profundo**: cambios en `onImGuiRender` de cada panel,
  agregar tooltips, ajustar labels, sumar callbacks, redistribuir
  layout. NO tocar el modelo (Components.h, Scene, etc.).
- **Sin tests obligatorios para UI pura**: ImGui no es testeable sin GL.
  Validación visual del dev al cierre. Tests solo si emerge lógica
  testeable (ej. helper de filtrado en Console).
- **Iconos image-based diferidos**: la deuda de FontAwesome del Toolbar
  (F2H22) NO entra en F2H23. Hito chico propio si emerge — el polish
  con texto y colores cubre el 80% del valor.

## Decisiones arquitectónicas

| # | Decisión | Valor |
|---|---|---|
| 1 | Approach | Auditoría con subagente → lista priorizada → fixes acotados. |
| 2 | Scope cerrado tras | Bloque B (auditoría). |
| 3 | Prioridad mínima a atacar | MEDIA. ALTA siempre, BAJA descartado o anotado en `PENDIENTES.md`. |
| 4 | Refactor profundo | NO. Cambios en `onImGuiRender` y métodos auxiliares. |
| 5 | Tests | Solo si emerge lógica pura testeable (ej. `consoleFilter(level, msg)`). UI pura = validación visual. |
| 6 | FontAwesome / iconos image | DIFERIDO. Hito propio chico si emerge. |
| 7 | Persistencia de filtros (Console / Hierarchy) | LOCAL al panel (no `.moodproj`). Volátil entre sesiones. |

## Bloques

### A — Plan F2H23 (este archivo)

Cierra al commit inicial. Scope provisional; se concreta tras Bloque B.

### B — Auditoría con subagente

Subagente lee:
- `src/editor/panels/scene/InspectorPanel.{h,cpp}`
- `src/editor/panels/scene/HierarchyPanel.{h,cpp}`
- `src/editor/panels/debug/ConsolePanel.{h,cpp}`
- `src/editor/ui/StatusBar.{h,cpp}`

Por cada panel, identifica:
- Labels confusos (ej. "X" sin tooltip, abreviaciones que solo el dev del editor entiende).
- Drop targets sin feedback visual (button gris pero el dev no sabe que acepta drag).
- Acciones sin confirmación / sin undo donde tendría sentido.
- Layout que se rompe al redimensionar.
- Textos en castellano / inglés mezclados.
- Separators / grupos visuales ausentes.
- Tooltips ausentes o incompletos.

Output: lista priorizada (ALTA / MEDIA / BAJA) con archivo + línea + descripción
del fix sugerido. Subagente NO implementa, solo audita.

### C — Fixes Inspector

Atacar los ítems ALTA + MEDIA del Inspector identificados en B. Ejemplos
posibles (a confirmar con la auditoría):
- Drop targets de componentes (textura albedo, mesh, script) con label
  claro de "(drop aquí)" cuando vacío.
- Tooltips en cada slider explicando rango/unidad.
- Botón "Add Component" con dropdown organizado por categoría.

### D — Fixes Hierarchy

Atacar ítems del Hierarchy. Ejemplos:
- Iconos de tipo de entidad (cubo / luz / audio / brush) con `()` ASCII
  o color del label.
- Highlight visual del set de selección múltiple (F2H13) más claro.
- Botón "Eliminar seleccionadas" si emerge necesidad.

### E — Fixes Console + StatusBar

- Console: filtros por canal (engine / render / assets / editor / world / ...) con checkboxes; colores por severidad (info=blanco, warn=amarillo, error=rojo); botón "Clear".
- StatusBar: separators verticales entre secciones (FPS | Modo | Sub-modo | Último cmd | Mensaje); ancho fijo por sección para evitar saltos.

### F — Build + suite + validación visual

- Build editor + tests.
- Suite verde, esperado **610 → ~613 cases** (sin tests nuevos por lo
  general; +3-5 si emerge helper testeable).
- Validación visual del dev sobre cada uno de los 4 panels.

### G — Cierre

- `docs/HITOS.md`: nueva entrada F2H23 closed.
- `docs/ESTADO_ACTUAL.md`: F2H23 al top.
- `docs/DECISIONS.md`: entrada `2026-05-08` (o fecha de cierre).
- Tag: `v1.14.0-fase2-hito23`.
- `PENDIENTES.md`: F2H24 candidato = 4-viewport Hammer (heredado
  movido); ítems BAJA descartados quedan anotados.

---

## Suite

Cambios localizados a 4 archivos `panels/*.cpp` + tal vez 1-2 helpers.
No toca modelo / serializer / runtime. Riesgo de regresión bajísimo.

## Riesgos

- **La auditoría encuentra demasiadas fricciones**: scope explota. Mitigación: atacar solo prioridad ≥ MEDIA, anotar BAJA en `PENDIENTES.md`. Si los ítems con prioridad ≥ MEDIA pasan de 10, partir el hito en 2 (F2H23 = Inspector + Hierarchy; F2H24 = Console + StatusBar).
- **Subagente sobre-detecta**: si encuentra 30 ítems pero la mitad son nice-to-have de estética, el dev puede vetar el scope al cerrar Bloque B antes de arrancar las implementaciones.
- **Cambios visuales rompen muscle memory del dev**: por ejemplo si reorganizamos el botón "Add Component" del Inspector, el dev tiene que aprender la nueva ubicación. Mitigar: cambios mínimos, conservar el lugar de items críticos.

## Criterios de cierre

- [ ] Auditoría con subagente completa, lista priorizada en commit del Bloque B.
- [ ] Fixes ALTA + MEDIA implementados en bloques C-E.
- [ ] Validación visual del dev sobre cada panel.
- [ ] Suite verde sin regresiones.
- [ ] Tag `v1.14.0-fase2-hito23` pusheado.
- [ ] `PENDIENTES.md` actualizado: F2H24 = 4-viewport Hammer, ítems BAJA del Bloque B anotados, deudas explícitas resueltas.
