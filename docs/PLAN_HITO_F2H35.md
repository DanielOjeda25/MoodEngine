# PLAN HITO F2H35 — Polish editor: UX viewport + Hammer-style visual

> **Estado**: en curso (2026-05-09).
> **Tag previsto**: `v1.25.0-fase2-hito35`.
> **Origen**: items identificados durante validación de F2H34 + paquete
> "Hammer-style visual polish" anotado post-F2H33.

## Objetivo

Cerrar varias mejoras chicas de UX del editor que comparten dominio
(viewport + render del wireframe + iconos de point entities). Cada
una sería pequeña aislada; agruparlas en un hito mantiene los commits
ordenados por bloque y un solo tag de cierre.

## Scope (decisiones confirmadas con dev)

1. **Editor abre maximizado por defecto** — el dev tenía que
   maximizar manualmente y reajustar el dockspace cada arranque.
2. **NO se incluye toggle wireframe/render en perspective** — ya
   tenemos wireframe en los 3 ortos del workspace "Editor de mapas"
   (F2H28). Confirmado por dev: *"olvdalo, ya tenemos wireframe en
   el editor de mapas"*.
3. **Tint del wireframe de brushes por color del VisGroup** — solo
   brushes (no point entities; los point entities tienen color por
   tipo en Bloque D). El color ya está en el struct `VisGroup`.
4. **Color por tipo de entidad** — lights=amarillo, audio=naranja,
   trigger=verde, etc. Tinta los iconos sphere actuales (overlay
   perspective + ortos).
5. **Labels arriba de point entities** — texto del tag flotante
   arriba del icono en world. **Default ON**. Toggle desde un
   botón nuevo en el toolbar lateral "Map Tools" sección
   "Visualización", persistido en `.moodproj`.
6. **Pulir face picking UX** — investigar `Csg::pickFace` (epsilon
   de triangulación) y/o agregar preview hover (la cara bajo el
   cursor se resalta tenue antes de clickear).

## Bloques de ejecución

| # | Bloque | Estim. | Notas |
|---|--------|--------|-------|
| A | Plan (este doc) | 5 min | Se archiva en cierre |
| B | Editor maximizado | 10 min | One-liner: `SDL_WINDOW_MAXIMIZED` flag al `SDL_CreateWindow` |
| C | Tint brushes por VisGroup color | ~30-50 LOC | Pasar `VisGroup.color` al pase wireframe del orto + perspective overlay si aplica |
| D | Color por tipo de entidad | ~1h | Mapa puro `entity_type → color`; tintar iconos sphere actuales + representaciones en orto |
| E | Labels point entities + toggle | ~1-2h | Render de texto en world (proyectar a screen + ImGui DrawList overlay) + bool `showEntityLabels` en EditorUI + botón toolbar + persistencia en `.moodproj` |
| F | Pulir face picking | ~1-2h | Investigar primero; si epsilon arreglarlo; si no, preview hover de la cara bajo cursor |
| G | Cierre | 30 min | Docs (HITOS/ESTADO/DECISIONS/PENDIENTES) + commits agrupados + tag + push |

## NO entra en F2H35

- VisGroups jerárquicos / drag desde Hierarchy / lock de VisGroup.
- Iconos image-based del Toolbar (FontAwesome merge) — pendiente
  separado en `PENDIENTES.md`.
- Multi-brush carve simultáneo / clip tool en perspective 3D.
- Toggle wireframe/render en perspective viewport (descartado por dev).

## Riesgos y tradeoffs

- **Bloque E (labels)**: render de texto en world requiere proyectar
  cada point entity a screen-space y dibujar con ImGui DrawList del
  viewport panel. Si hay muchas entities (>500), considerar culling
  por frustum + LOD del label (ocultar si distancia > X).
- **Bloque F (face picking)**: scope abierto. Si al investigar veo
  que el epsilon de `pickFace` es el problema → fix concreto. Si es
  UX puro (caras chicas, ángulos extremos) → preview hover. Decidir
  durante el bloque.
- **Bloque D (color por tipo)**: el color de los iconos point entity
  hoy es el outline F2H13 (naranja activa, amarillo selected). El
  tipo va en el icono, no en el outline — preservar la convención
  selection ≠ tipo.

## Validación esperada

- Editor arranca en pantalla completa sin tocar nada.
- Brushes en un VisGroup se ven con su color.
- Lights amarillas, audios naranja, triggers verdes (visualmente
  distintos del default).
- Tags de point entities visibles arriba de cada icono al arrancar;
  botón "Nombres" del toolbar los oculta/muestra; el toggle persiste
  al cerrar/abrir el proyecto.
- Face picking se siente más confiable (criterio subjetivo del dev).
