# Pendientes vivos — MoodEngine

> Backlog de tareas conocidas que NO están planificadas en un hito específico
> todavía. Cada vez que cerrás un hito, mové aquí los pendientes que quedaron
> identificados pero no abordados, y eliminá los que ya hayas cubierto.
>
> Para el roadmap de hitos cerrados/programados, ver `HITOS.md`.
> Para el estado actual del proyecto y "dónde estamos", ver `ESTADO_ACTUAL.md`.

---

## Post-F2H21 (2026-05-07)

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

- **Pase de polish UX general del editor** (charlado con el dev tras
  cerrar F2H21: *"a futuro deberemos mejorar toda la UI para hacerla más
  fácil de entender"*). Los descubrimientos de F2H21 (Texture slots
  vacíos mostrando "missing.png" en vez de "<vacio>", botón X cortado
  fuera del panel, tooltips ausentes) son síntoma de un patrón general
  en muchos panels. Candidatos identificados a auditar:
  - Inspector: descubribilidad de drop targets (texturas, materiales,
    scripts, prefabs).
  - AssetBrowser: filtros / hover preview / drag hints.
  - Hierarchy: feedback visual de selección múltiple.
  - StatusBar: layout y colores informativos.
  - Console: filtrado por categoría con colores.
  Approach esperado: subagente recorre cada panel buscando
  inconsistencias de UX (label confuso, slot vacío indistinguible de
  asignado, accion sin feedback, etc.) → lista priorizada → fixes
  acotados sin refactor profundo. Mismo patrón que F2H16 / F2H19.
  Probable hito propio (F2H23+ tras 4-viewport).

- **Node-graph del Material Editor** (deuda explícita de F2H21 acotado):
  el plan F2 original F2H17 incluía node-graph con shader runtime
  compilation. F2H21 entregó solo preview esférico + Save (~80% del
  valor visual con ~20% del scope). Cuando emerja necesidad de
  materiales complejos (Mix entre 2 texturas, Multiply de máscaras,
  emissive, etc.), abrir hito propio con: tipos de nodo (TextureSample,
  ColorConstant, ScalarConstant, Multiply, Add, Mix, Output con pins
  Albedo/Metallic/Roughness/Normal/AO), conexiones por drag, GLSL
  compilado runtime con cache por hash del grafo, persistencia
  `.material` extendida. Riesgo: refactor del SceneRenderer para
  shaders custom por material (cada grafo distinto = shader distinto,
  rompe batching de F2H4). Probable F2H24+ si emerge necesidad.

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
- **Preview esférico del Material Editor con interacción de mouse**
  (orbit cam, zoom): F2H21 dejó rotación automática lenta. Nice-to-have
  si emerge en uso real.

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
