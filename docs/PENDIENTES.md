# Pendientes vivos — MoodEngine

> Backlog de tareas conocidas que NO están planificadas en un hito específico
> todavía. Cada vez que cerrás un hito, mové aquí los pendientes que quedaron
> identificados pero no abordados, y eliminá los que ya hayas cubierto.
>
> Para el roadmap de hitos cerrados/programados, ver `HITOS.md`.
> Para el estado actual del proyecto y "dónde estamos", ver `ESTADO_ACTUAL.md`.

---

## Post-F2H20 (2026-05-07)

### Activos

- **F2H22 candidato — 4-viewport Hammer-style layout** (charlado con el
  dev tras cerrar F2H20). Workspace nuevo "Hammer" con dockspace en 4
  cuadrantes: 1 perspectiva 3D + 3 ortográficas (top / front / side) en
  wireframe (`glPolygonMode(GL_LINE)` + grid 2D + crosshair). Drag-edit
  de brushes / vertices entre vistas con grid snap. Multi-cámara: 4
  framebuffers (o uno reutilizado con N renders) + shadow pass /
  light grid compartido para mitigar el ~4× costo CPU del frame.
  Decisión de prioridad: **arrancar inmediatamente después de F2H21**
  (material editor) — flow CSG queda sólido como herramienta antes de
  meterse en física avanzada o features visuales. Si emerge presión de
  "demo bonito" antes que "workflow cómodo", postponer a sub-fase 2.7.

### Diferidos no urgentes (mencionar al dev si se acercan al scope)

- **Runtime-load de mesh compilada en `MoodPlayer`** (deuda de F2H20):
  player carga la mesh estática unificada en lugar de los brushes
  individuales — habilita "brushes solo en el editor" del plan original
  F2H14. Schema bump del `.moodmap` con `compiledMesh` opcional. Hito
  futuro si emerge necesidad de loading time.
- **Cull de overlap parcial** (deuda F2H20): clipping general
  polígono-polígono. Diferido si emerge necesidad real con mapas
  grandes.
- **F6 panel estilo Blender** (tweak last operator post-hoc) — diferido
  desde F2H16.
- **Vertex / Edge mode** (teclas 1, 2 reservadas en F2H17).
- **Multi-selección de caras** (Shift+click sobre múltiples caras).

## Post-F2H19 (2026-05-07)

**Backlog vacío** — F2H18 cerró la reorganización de menús; F2H19 cerró la
limpieza de HistoryStack residual (auditoría con subagente confirmó que
los items BAJA quedan fuera de scope alineado con la convención
Blender/Unity).

### Histórico resuelto

- ~~Reorganización de menús `Archivo > Mapa`~~ — resuelto en F2H18
  (`v1.9.0-fase2-hito18`). Mejoras adicionales (toolbar lateral con
  iconos Hammer-style, atajos Ctrl+B, renombre `Brush → Geometría`)
  diferidas si emergen.
- ~~Limpieza HistoryStack residual~~ — resuelto en F2H19
  (`v1.10.0-fase2-hito19`). 2 comandos nuevos
  (`EditBrushFaceMaterialCommand`, `EditScriptComponentCommand`) +
  wireup en 3 sitios. Items BAJA descartados explícitamente
  (acciones del menú Mapa, toggles de modo/selección).

### Diferidos no urgentes (mencionar al dev si se acercan al scope)

Estos NO están en el backlog activo pero quedan registrados para que
emerjan si el dev los pide:

- **F6 panel estilo Blender** (tweak last operator post-hoc) —
  diferido desde F2H16.
- **Vertex / Edge mode** (teclas 1, 2 reservadas en F2H17). Sub-feature
  avanzada no típica de mapping FPS.
- **Multi-selección de caras** (Shift+click sobre múltiples caras del
  mismo brush) — diferido desde F2H17.
- **Undo de exposed property overrides** del ScriptComponent (deuda
  Hito 24) — comando dedicado si emerge.
