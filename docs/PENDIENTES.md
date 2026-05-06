# Pendientes vivos — MoodEngine

> Backlog de tareas conocidas que NO están planificadas en un hito específico
> todavía. Cada vez que cerrás un hito, mové aquí los pendientes que quedaron
> identificados pero no abordados, y eliminá los que ya hayas cubierto.
>
> Para el roadmap de hitos cerrados/programados, ver `HITOS.md`.
> Para el estado actual del proyecto y "dónde estamos", ver `ESTADO_ACTUAL.md`.

---

## Post-F2H17 (2026-05-06)

### 1. Reorganización de menús `Archivo > Mapa`

**Qué:** el menú `Archivo > Mapa` acumuló items mezclando file ops (Nuevo,
Guardar, Guardar como, Cargar) con geometría (Añadir Brush > Box/Cylinder/...,
Boolean > Subtract/Union/Intersect). Crece sin estructura clara.

**Por qué duele:** items dispares en el mismo nivel; añadir features nuevas
empuja la lista a ser inutilizable.

**Idea de solución:** separar en dos menús top-level: `Archivo` (solo file
ops) y `Mapa` o `Brush` (geometría + booleanos). Alternativa: dejar
`Archivo > Mapa` solo con file ops y mover geometría a un menú propio
`Geometría` o a la toolbar del viewport (Hammer-style).

**Cuándo:** hito propio. Tocarlo aislado para no mezclar con feature work.

**Referencias:**
- `src/editor/application/EditorMenuBar.cpp` (handler del menú).
- `src/editor/application/EditorProjectActions.cpp` (acciones).

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
