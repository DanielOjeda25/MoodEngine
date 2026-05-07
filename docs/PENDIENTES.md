# Pendientes vivos — MoodEngine

> Backlog de tareas conocidas que NO están planificadas en un hito específico
> todavía. Cada vez que cerrás un hito, mové aquí los pendientes que quedaron
> identificados pero no abordados, y eliminá los que ya hayas cubierto.
>
> Para el roadmap de hitos cerrados/programados, ver `HITOS.md`.
> Para el estado actual del proyecto y "dónde estamos", ver `ESTADO_ACTUAL.md`.

---

## Post-F2H18 (2026-05-06)

### ~~1. Reorganización de menús `Archivo > Mapa`~~ — RESUELTO en F2H18

Cerrado en F2H18 (tag `v1.9.0-fase2-hito18`): `Mapa` y `Brush` se
promovieron a top-level. Demos agrupados bajo `Ayuda > Demos ▶`.
Mejoras de UX adicionales mencionadas por el dev ("a futuro haremos
mejoras") quedan disponibles si emergen: toolbar lateral con iconos
de brush (Hammer-style), atajos de teclado (Ctrl+B = Box, etc.),
renombre `Brush → Geometría` cuando entren shapes no-brush.

---

### 2. Limpieza HistoryStack residual

**Qué:** después de F2H16 (limpieza HistoryStack — drops + UV editor) y de
F2H17 (multi-material), quedan acciones del editor que mutan estado **sin
push al HistoryStack** y por tanto no son undoable con Ctrl+Z.

**Sospechosos identificados** (auditar al arrancar el hito):
- Drop de material/textura **en Face Mode** sobre una cara: F2H17 wireó la
  lógica del slot pero no auditó si el push al stack sigue funcionando para
  el caso multi-slot. Posible regresión.
- Cambio del slot activo de una cara desde el Inspector (si ese widget
  emerge como parte del polish de F2H17).
- Cualquier mutación nueva añadida en F2H18+ sin command explícito.

**Cuándo:** hito propio post-F2H17 antes de que la deuda crezca otra vez.
Mismo approach que F2H16: subagente recorre el editor, produce lista
concreta, comandos nuevos mínimos, wireup en handlers.

**Referencias:**
- `docs/DECISIONS.md` entry 2026-05-06 "HistoryStack Blender-style — wireup,
  no refactor (F2H16)" — patrón a reusar.
- `src/editor/application/DemoSpawners.cpp` — handlers de drop.
- `src/editor/panels/scene/InspectorPanel.cpp` — UV editor + futuros widgets
  per-cara.
