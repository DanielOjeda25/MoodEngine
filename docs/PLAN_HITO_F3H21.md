# PLAN F3H21 — Viewport pro: cámaras numpad + modos visualización

**Estado:** **A DEFINIR** (arrancar tras F3H20).
**Predecesor:** F3H20 (snapping configurable Hammer-style).
**Origen:** `PLAN_FASE3.md` Sub-fase 3.4 lista "Viewport pro".

---

## Avance de Sub-fase 3.4 (post-F3H20)

```
F3H20 –  ✅ Snapping configurable Hammer-style
F3H21 –  — Viewport pro: cámaras numpad + modos visualización ⬅ próximo
F3H22 –  — Performance feedback: Profiler + Stats overlay
F3H23 –  — Comunicación al dev: Console + Toasts
F3H24 –  — Crash recovery + autosave (cierra Fase 3)
```

---

## Norte

`PLAN_FASE3.md` Sub-fase 3.4 declara:
> **F3H21 — Viewport pro.**
> Cámaras numpad estilo Blender (1=front, 3=right, 7=top, 5=ortho toggle, .=focus selection). Modos de visualización del viewport (wireframe, solid, material preview, rendered) toggle con Z key dropdown.

**Mecánica del editor:** el dev navega la escena con teclas numpad como Blender (1 = front, 3 = side, 7 = top, 9 = invertir, 0 = camera view, . = enfocar selección). Cambia el modo de render del viewport (wireframe / solid / material preview / rendered) para iterar sobre la geometría sin la complejidad del shader final.

---

## Scope candidato

### Cámaras numpad

1. **Atajos de cámara perspectiva** en `EditorOverlay.cpp` o handler de teclas:
   - Numpad 1 → Front view (eye=-Z, target=origin, up=Y).
   - Numpad 3 → Right view (eye=+X).
   - Numpad 7 → Top view (eye=+Y, up=-Z).
   - Numpad Ctrl+1/3/7 → vistas opuestas (Back/Left/Bottom).
   - Numpad 5 → toggle ortográfica vs perspectiva del viewport activo.
   - Numpad . → focus camera en selección (centra + ajusta distance al AABB).
   - Numpad 9 → invertir view.

2. **Transición animada** (opcional, polish): lerp entre la cámara actual y la target durante ~200ms para que el dev no se desoriente.

### Modos de visualización

1. **Enum `ViewportRenderMode`** con 4 valores: Wireframe / Solid / MaterialPreview / Rendered.
2. **Override en SceneRenderer** según el modo:
   - Wireframe: GL_LINE polymode + sin texturas + colors per-entity hash.
   - Solid: como ahora pero sin texturas (color flat o albedo plano).
   - MaterialPreview: como ahora.
   - Rendered: como ahora + shadows + SSR + bloom (== Play mode visual).
3. **UI toggle**: dropdown en el viewport top bar (estilo Blender Z dropdown) o key Z para cycle.

### Persistencia

- Modo de visualización en `UserSettings.editor.viewportRenderMode` (per-instalación, no per-proyecto — preferencia del dev).
- Las cámaras numpad NO se persisten (cada vez que el dev clickea numpad reposiciona).

### Tests

- `UserSettings.editor.viewportRenderMode` roundtrip.
- Numpad keys mockables (testear el helper que computa la pose target sin pasar por SDL events).

---

## Decisiones a tomar al arrancar

1. **Animación entre vistas numpad**: ¿lerp suave (200ms) o teleport instantáneo? Blender hace lerp; Unity hace teleport.
2. **Modos de visualización gating**: ¿la wireframe debería verse en perspective + ortho ambos, o sólo perspective? Blender: ambos.
3. **Numpad 0**: ¿camera view (mover la editor cam a la PlayerCamera del scene) o reservado? Blender: camera view; útil para "ver lo que ve el jugador".
4. **Render mode rendered**: ¿reusar el SceneRenderer del Play mode (mismo pipeline) o uno paralelo? Reusar es más simple pero acopla.

---

## Lo que NO toca F3H21

- F3H22+ (Profiler / Console / Toasts / Crash recovery): hitos propios.
- Numpad orbit (numpad 2/4/6/8 para orbit incremental): backlog si emerge — Blender lo tiene pero es de uso bajo.
- Modo X-ray / transparent overlay: backlog (es feature avanzado, no esencial).
- Render mode "Vertex paint" / "Texture paint": fuera de scope (no tenemos paint tools).
