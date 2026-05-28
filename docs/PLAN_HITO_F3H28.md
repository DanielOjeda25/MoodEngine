# PLAN F3H28 — Grupos + Map Tools como categorías del Properties Editor (STUB)

**Estado:** 📝 **STUB** — Item 1 del stub original F3H27. Splitting confirmado por el dev tras F3H26: 4 items → 4 hitos separados (F3H27 parenting / F3H28 grupos+tools / F3H29 mundo grande / F3H30 HDRI dinámico). Por arrancar tras cierre de F3H27.
**Predecesor:** F3H27 (parenting jerárquico — base técnica para "Grupos").

---

## Sub-fase 3.4 (post-F3H27)

```
F3H20 – ✅ Snapping configurable Hammer-style
F3H21 – ✅ Viewport pro: numpad views + 4 render modes
F3H22 – ✅ Properties Editor con icons laterales (Blender style)
F3H23 – ✅ Performance feedback: Profiler + Stats overlay
F3H24 – ✅ Comunicación al dev: Console + Toasts
F3H25 – ✅ Crash recovery + autosave
F3H26 – ✅ Polish UX del editor
F3H27 – ✅ Parenting jerárquico de transforms
F3H28 – 📝 Grupos + Map Tools como categorías del Properties Editor ⬅ este stub
F3H29 – 📝 Mundo grande (camera limits / far plane / reverse-Z) (stub)
F3H30 – 📝 HDRI dinámico + ciclo día/noche (stub)
```

---

## Origen

Cita textual del dev al cerrar F3H26:
> *"quiero mejorar esta parte de grupos que estan separados, y los maptools que esten mas insertados mas en menus como los que hicimos en layout, como los de blender, que hicimos hito atras"*

Hoy:
- **Map Tools** y **Grupos** son dos paneles flotantes en el dock derecho (cada uno ocupa un tab).
- El panel "Grupos" tiene un botón `+ Nuevo grupo` pero está casi vacío — feature stub histórica.
- Los **Map Tools** (selección, bloque, pincel, clip, vertex/edge/face submode, snap, labels, carve) viven en una toolbar lateral persistente del workspace Editor de Mapas.

El dev pide unificar ambos como **categorías del Properties Editor** (chasis F3H22 con icons verticales clickeables tipo Blender).

---

## Norte

Reducir el clutter del dock derecho integrando "Grupos" + "Map Tools" como nuevas categorías del Inspector con icons laterales. Mental model Blender: el Properties Editor concentra todas las propiedades del proyecto/escena/objeto en un solo panel con sidebar de categorías. Cada categoría exclude del space toolbars/tabs separados.

---

## Items por cerrar (pre-AskUserQuestion)

### Item A — Categoría "Map Tools"
**Hoy:** toolbar lateral persistente del workspace Editor de Mapas con: selección / bloque (Box brush) / pincel (Paint Brush) / clip / vertex submode / edge submode / face submode / snap settings popover / labels toggle / carve.

**Objetivo:** categoría nueva del Properties Editor (icon herramienta) que reemplaza la toolbar lateral. Layout: SeparatorText sections (Selección / Brushes / Sub-mode / Snap / Acciones).

**Decisiones a cerrar:**
- ¿La toolbar lateral se elimina o se mantiene como acceso rápido + categoría sirve para configuración? Blender mantiene la T-key panel separada del Properties; Unreal Modeling Mode integra todo en Properties.
- ¿Submode vertex/edge/face en categoría = redundante con la top bar del map editor (F2H30)? Si el dev usa la categoría para configurarlo, la top bar puede quedar como solo-indicador.
- Carve / Boolean ops: ¿migran de Outliner context menu (F3H26) a la categoría también, o se quedan en context menu?

### Item B — Categoría "Grupos"
**Hoy:** panel flotante con botón stub `+ Nuevo grupo`. F3H27 introdujo Empty-as-parent (Group_<N>); F3H28 puede aprovechar eso como backend o construir un sistema paralelo de "Grupos" (VisGroup-like).

**Objetivo:** categoría nueva del Properties Editor (icon grupo) con lista de grupos del mapa + new/delete/rename + assign/unassign entidades.

**Decisiones a cerrar:**
- ¿"Grupos" = Empty-as-parent (F3H27, transform-parented) o `VisGroup` (visibility-toggle, sin parent transform)? El dev distinguió "mover el edificio entero" → eso es F3H27. Si "Grupos" es VisGroup-like, sirve para visibility-only (hide/show colecciones de entities por categoría) sin afectar transforms.
- ¿La categoría lista TODOS los Empty del mapa (Group_<N>), o un concepto de "grupo nombrado" separado del Empty?
- ¿Multi-membership permitido? Blender colecciones permiten 1 entity en N colecciones; Unity tags solo permiten 1.

### Item C — Migración + reorganización del dock derecho
**Side-effect:** al mover Map Tools + Grupos a categorías del Properties Editor, los paneles flotantes actuales se eliminan o quedan ocultos. Hay que verificar que no rompa el workspace persistido del usuario.

**Decisiones a cerrar:**
- ¿Eliminar los paneles flotantes del registry o solo ocultarlos por default?
- ¿Migración de proyectos viejos con layout custom? Probable: ignorar IDs de panels eliminados al cargar el layout (back-compat aditiva).

---

## Lo que NO toca F3H28

- F3H29 (mundo grande / camera limits) — hito propio.
- F3H30 (HDRI dinámico) — hito propio.
- Migrar otros paneles flotantes a categorías (Asset Browser / Console / Profiler) — no pidió el dev.
- Sistema de tags Unity-like (etiquetas string libres) — fuera de scope.

---

## Backlog post-F3H28 (estimado)

- Filtro de texto en la sidebar de categorías del Inspector (F3H22 D6 diferido).
- Custom user categories (dev define grupos propios de componentes) — fuera de scope, hito propio si emerge.
- Drag para reordenar categorías de la sidebar — Blender lo permite, fuera de scope.
