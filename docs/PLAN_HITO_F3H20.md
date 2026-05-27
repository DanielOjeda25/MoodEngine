# PLAN F3H20 — Snapping configurable (CERRADO)

**Estado:** **CERRADO** — v2.20.0-fase3-hito20.
**Predecesor:** F3H19 (rename con cascada, cierre Sub-fase 3.3).
**Sucesor:** F3H21 — Viewport pro (cámaras numpad + modos visualización).

---

## Avance de Sub-fase 3.4

```
F3H20 –  ✅ Snapping configurable
F3H21 –  — Viewport pro: cámaras numpad + modos visualización ⬅ próximo
F3H22 –  — Performance feedback: Profiler + Stats overlay
F3H23 –  — Comunicación al dev: Console + Toasts
F3H24 –  — Crash recovery + autosave (cierra Fase 3)
```

---

## Lo que se entregó

**Grid snap (Hammer-style) para el gizmo translate perspectivo:**
- Cuantiza el delta del drag a múltiplos de un step configurable.
- Step default = 0.5 unidades (= metros), cycleable `0.125 / 0.25 / 0.5 / 1 / 2 / 4` con Ctrl++/Ctrl+- (en workspaces que no sean "Editor de mapas" — ese sigue usando su step int de orthos).
- Toggle `G` en el overlay del viewport.
- Modal `G` (tecla G + drag estilo Blender) respeta el mismo toggle.
- Objetos colocados off-grid se mueven en saltos limpios sin "jolt" inicial (snap al delta, no a la posición absoluta).

**Angle snap para el gizmo rotate + modal R:**
- Cuantiza el delta angular a múltiplos de `snapAngleDegrees`.
- Default = 15°, cycleable `5 / 10 / 15 / 30 / 45 / 90` desde el chip del status bar.
- Toggle `A` en el overlay del viewport.

**Status bar arriba del viewport (estilo Blender):**
- Chips horizontales con el step actual de cada snap activo:
  - `Grid 0.5` cuando G está on.
  - `Angle 15°` cuando A está on.
- Click sobre cada chip cicla forward el step.
- Sin snaps activos → status bar no se renderiza (sin ruido visual).

**Floating text durante translate drag:**
- Debajo del origen del gizmo: `X +1.500  (grid 0.5)` con el delta del eje activo + step si grid on.
- Sombra negra para legibilidad sobre fondos claros.

**Persistencia en `.moodproj`:**
- `settings.snap.grid_enabled` (bool, default false).
- `settings.snap.grid_step` (f32, default 0.5).
- `settings.snap.angle_enabled` (bool, default false).
- `settings.snap.angle_degrees` (f32, default 15.0).
- `settings.snap.vertex_enabled` (bool, default false) — usado solo por orthos del workspace "Editor de mapas".
- Forward-compat: keys de scale snap (feature removida en iter5) se ignoran al cargar `.moodproj` viejos.

---

## Decisiones tomadas en el camino (iters 4–7)

**Iter 1–3:** Implementé vertex snap perspectivo con marcadores yellow source/target estilo Blender "Closest" mode. Pivot fix: source dinámico (corner del objeto arrastrado más cercano al target en world). Marcadores grandes con outline blanco + label "SNAP".

**Iter 4 — pivot a Hammer:** El dev probó vertex snap y reportó "es medio raro". Hammer-style grid snap es más simple y predictible. Decisión: **reemplazar vertex snap perspectivo por grid snap**. El vertex snap orto (workspace "Editor de mapas") se mantiene intacto. Borradas las funciones `snapToVertexInScene`, `findSnapTargetForGizmo`, `closestVertexOnEntityToWorld`.

**Iter 5 — bug fix angle snap:** El angle snap solo aplicaba al modal R (tecla R + drag); el gizmo R (clickear los rings) ignoraba el toggle. Bug reportado por el dev. Portado al gizmo R en `EditorOverlay_Gizmo.cpp`.

**Iter 6 — status bar Blender-style:** El dev pidió que el step se vea en algún lado persistente, no solo durante el drag. Sacado el botón inline bajo el toggle G; agregada función `drawViewportSnapStatusBar` que pinta chips arriba-centro del viewport. Chips clickeables para ciclar step.

**Iter 7 — quitar scale snap:** El dev preguntó si scale snap se usa en Hammer. Honesto: no. Sus casos de uso (kits modulares, prop variations) son marginales y se resuelven tipeando en el Inspector. Removido por completo: `snapScaleEnabled`, `snapScaleIncrement`, toggle `S`, chip `Scale`, código del gizmo + modal, i18n keys. Forward-compat preserved (load ignora keys viejas).

**Iter 8 — backlog:** Agendizadas dos features post-F3H20 (no entran en este hito):
- **Alinear al grid** (one-shot): snappea pos absoluta de la selección al grid actual. Útil para limpiar objetos placed off-grid.
- **Drop to surface** (raycast hacia abajo): apoya el AABB de la entity sobre la primera superficie debajo. Para items sobre mesas/pisos. Memoria: `project_align_and_drop_backlog`.

---

## Lo que NO toca F3H20

- F3H21+ (Viewport pro / Profiler / Console / Toasts / Crash recovery): hitos propios.
- Snap a edge medio / centro de cara: backlog si emerge.
- Snap entre instancias de prefabs: scope distinto.
- Vertex snap del gizmo perspectivo: descartado en iter4 (no era Hammer-style).
- Scale snap: descartado en iter7 (no es Hammer-style, sin uso real).
- Face-align (orientar Y al normal): descartado pre-iter1 cuando el dev eligió "Vertex snap extendido" en lugar de "Face-align".

---

## Tests

`tests/test_project_settings.cpp` — 6 nuevos test cases para los toggles + steps F3H20:
- Snap F3H20 defaults (todos off, steps default).
- Snap F3H20 roundtrip (vertex/grid/angle preservados).
- Snap F3H20 sanitize grid_step ≤ 0 → default.
- Snap F3H20 sanitize angle_degrees > 360 → default.
- Snap F3H20 back-compat (pre-F3H20 sin toggles → defaults off).
- Snap F3H20 ignora keys scale_* (forward-compat).

20 tests Snap totales / 76 asserts — todos verdes.
