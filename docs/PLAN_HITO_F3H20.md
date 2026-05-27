# PLAN F3H20 — Snapping configurable

**Estado:** **A DEFINIR** (arrancar tras F3H19).
**Predecesor:** F3H19 (rename con cascada, cierre Sub-fase 3.3).
**Origen:** `PLAN_FASE3.md` Sub-fase 3.4 lista "Snapping configurable".

---

## Avance de Sub-fase 3.4 (post-consolidación)

```
F3H20 –  — ⬅ próximo: Snapping configurable
F3H21 –  — Viewport pro: cámaras numpad + modos visualización
F3H22 –  — Performance feedback: Profiler + Stats overlay
F3H23 –  — Comunicación al dev: Console + Toasts
F3H24 –  — Crash recovery + autosave (cierra Fase 3)
```

---

## Norte

`PLAN_FASE3.md` Sub-fase 3.4 declara:
> **F3H20 — Snapping configurable.**
> Grid snap (existe parcial), vertex snap (existe: F2H31C), ángulo snap (15°/45°/90°), face-align (orientar a normal). Defaults en Project Settings. Toggle visual en toolbar.

**Mecánica del editor:** el dev edita una entidad con el gizmo. Al rotar, si tiene "ángulo snap = 15°" activado, la rotación se ancla a múltiplos de 15° (con feedback visual del ángulo actual). Al mover, si tiene "vertex snap" activo + apunta cerca de un vértice de otra geometría, el target se ajusta a ese vértice. Al rotar contra una superficie con "face-align" activado, la entidad orienta su Y al normal de la superficie.

---

## Scope candidato

### Backend (engine)

1. **Estado de snap settings** en `ProjectSettings.snap` (ya existe parcialmente desde F3H6). Extender con:
   - `angleSnapStepDegrees` (default 15°, lista de presets `[5, 10, 15, 30, 45, 90]`).
   - `vertexSnapEnabled` (bool, default false — opt-in).
   - `faceAlignEnabled` (bool, default false — opt-in).
   - Grid snap ya está cubierto por F3H6.

2. **Lógica de snap en gizmo de rotate**: en `EditorGizmoRotate`, al computar la rotación delta, si `angleSnapStepDegrees > 0` y el dev tiene Shift presionado (o el toggle activado), snap el ángulo al múltiplo más cercano.

3. **Lógica de vertex snap**: durante drag del gizmo translate, raycast desde el cursor del mouse → si hit cerca de un vértice de otra entidad (umbral en world units), snap el target al vértice. Reusar `ScenePick` infra de F2H31C.

4. **Lógica de face-align**: durante drag del gizmo rotate, raycast desde el cursor → si hit superficie de otra entidad, snap la rotación para alinear el Y del target al normal del hit.

### UI

1. **Toolbar visual con toggles**: agregar 3 botones nuevos al toolbar del viewport (al lado de los existentes "Move/Refresh/Maximize/F"):
   - 📐 Angle snap toggle + popover con preset de grados.
   - 🔷 Vertex snap toggle.
   - 🧲 Face-align toggle.

2. **Popover de angle snap**: combo con presets `[5°, 10°, 15°, 30°, 45°, 90°]` + InputInt "Custom...".

3. **Indicador visual durante drag**: mostrar el ángulo actual (con snap aplicado) cerca del gizmo en tiempo real.

### Tests

- Angle snap: rotate exacto multiple de step → ángulo igual al input. Rotate intermedio → snap al múltiplo más cercano.
- Vertex snap: drag con target cerca de vértice → posición = vértice. Lejos del vértice → posición normal.
- Face-align: rotate contra superficie con normal conocido → Y del target == normal.
- Persistencia: settings.json round-trip + `.moodproj` round-trip.

---

## Decisiones a tomar al arrancar

1. **Activación**: ¿toggles persistentes via UI (estado del editor) o modifier key Shift mientras drag (estado temporal del gesto)? O ambos (toggle = default, modifier overrides momentáneo)?
2. **Vertex snap target**: ¿cualquier vértice de cualquier mesh visible, o solo mesh "snap-target" marcados? Performance vs ergonomía.
3. **Face-align orient**: ¿Y del target al normal (default), o axis configurable (X/Y/Z)?
4. **Indicador visual durante drag**: texto flotante cerca del gizmo, overlay 2D en esquina del viewport, o ambos?

---

## Lo que NO toca F3H20

- F3H21+ (Viewport pro / Profiler / Console / Toasts / Crash recovery): hitos propios.
- Grid snap: ya existe desde F3H6 (no extender — está cubierto).
- Snap a edge medio / centro de cara: backlog si emerge.
- Snap entre instancias de prefabs: scope distinto.
- Snap configurable per-axis (snap solo en X, no en Y/Z): backlog si emerge.
