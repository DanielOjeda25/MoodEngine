# PLAN F3H21 — Viewport pro: cámaras numpad + modos visualización

**Estado:** **CERRADO** — `v2.21.0-fase3-hito21` (segundo hito de Sub-fase 3.4).
**Predecesor:** F3H20 (snapping configurable Hammer-style).
**Origen:** `PLAN_FASE3.md` Sub-fase 3.4 lista "Viewport pro" (consolidado ex-F3H21 + ex-F3H22 en uno solo, ver `PLAN_FASE3.md:89-95`).

---

## Cierre

**Lo entregado:**
- Numpad views con lerp Blender (1/3/7/Ctrl+N/9/0) — yaw shortest-path, smoothstep easing, skip input mientras lerping.
- 4 render modes (Wireframe/Solid/MaterialPreview/Rendered) — gating de post passes + shader branch para Solid + force-on conservador para Rendered.
- UI: 4 botones flotantes top-right del viewport (FontAwesome + tooltip i18n) + tecla Z cycle.
- F2H30 sub-modes 1/2/3 movidos a top-row only (libera numpad para views).
- Polish reactivo: floating text del delta en translate drag (F3H20) gateado al snap activo.

**Tests:** 16 cases / 53 asserts (UserSettings 3 fields + EditorCamera lerp).

**Decisiones tomadas durante implementación (suman a las 4 cerradas pre-impl):**
- D5: Solid shader branch en `pbr.frag` (uniform `uSolidShading`) vs override CPU. Reason: branch shader = ~5% costo PBR full, cero state per-draw.
- D6: F2H30 sub-mode keys 1/2/3 → top-row only. Reason: convención Blender pura (numpad reservado para views).
- Ajuste D4: Rendered NO fuerza SSR — artifacts garantizados sin normal RT + tuning. Bloom threshold default subido a 1.5 (vs 1.0 que sobre-brighteaba cielos).

**Backlog del hito:**
- Solid full con texture passthrough opcional (gris uniforme cubre 90% del use case, texture opt-in seria para ver UV layout).
- Rendered con SSR auto-tuneado por scale del scene.
- Properties Editor con icons laterales tipo Blender — el dev lo pidió como hito propio al cerrar F3H21. Anotado en backlog UX.

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

2. **Transición animada lerp 200ms** (decisión D1): pose-to-pose con easing `smoothstep` entre `cameraPose_before` y `cameraPose_target` durante `UserSettings.editor.smoothViewDurationMs` (default 200, range 0-1000). Toggle `smoothViewEnabled` para desactivar (=teleport). El driver vive en `EditorCamera::tick(dt)` — no bloquea input ni renderiza extra frames.

### Modos de visualización

1. **Enum `ViewportRenderMode`** con 4 valores: Wireframe / Solid / MaterialPreview / Rendered.
2. **Override en SceneRenderer** según el modo:
   - Wireframe: GL_LINE polymode + sin texturas + colors per-entity hash.
   - Solid: como ahora pero sin texturas (color flat o albedo plano).
   - MaterialPreview: como ahora.
   - Rendered: como ahora + shadows + SSR + bloom (== Play mode visual).
3. **UI toggle**: dropdown en el viewport top bar (estilo Blender Z dropdown) o key Z para cycle.

### Persistencia

- `UserSettings.editor.viewportRenderMode` (per-instalación) — modo de visualización del viewport.
- `UserSettings.editor.smoothViewEnabled` (bool, default true) — toggle del lerp numpad.
- `UserSettings.editor.smoothViewDurationMs` (int, default 200, clamp 0-1000) — duración del lerp.
- Las cámaras numpad NO se persisten (cada click numpad reposiciona desde cero — Blender/Unreal/Unity hacen lo mismo).

### Tests

- `UserSettings.editor.viewportRenderMode` roundtrip.
- Numpad keys mockables (testear el helper que computa la pose target sin pasar por SDL events).

---

## Decisiones cerradas (investigación industrial 2026-05-27)

### D1 — Lerp suave entre vistas numpad (default ON, ~200ms)

**Decisión:** **lerp suave default ON, duración 200ms**, toggleable en User Preferences (`smoothViewEnabled` + `smoothViewDurationMs`).

**Referencias:**
- **Blender:** "Smooth View" es timer-based, default ON. La duración vive en Preferences > Input > Timer 1 ("Smooth View"). Convención dominante del 3D content creation.
- **Unity:** teleport por default en numpad views; lerp solo en F (Frame Selected).
- **Unreal:** teleport.

**Por qué Blender:** el dev ya usa Blender como referencia mental (memoria `feedback_no_reinventar_rueda`); el lerp evita la desorientación que el plan stub anticipó.

### D2 — Wireframe + render modes en perspectiva + ortho ambos

**Decisión:** los 4 render modes (Wireframe / Solid / MaterialPreview / Rendered) funcionan en **viewport perspectivo + los 3 ortográficos**. Sin gating.

**Referencias:**
- **Blender:** shading modes (Z dropdown) están disponibles en ambos modos de proyección.
- **Unreal:** view modes (Alt+1/2/3/4) funcionan en perspective + las 4 ortho.
- **Unity:** shading modes funcionan en ambos.

**Por qué:** es 100% convención industrial. Restringir genera fricción sin upside.

### D3 — Numpad 0 = camera view (a la primera `CameraComponent`)

**Decisión:** Numpad 0 mueve la editor camera a la pose de la **primera entidad con `CameraComponent`** de la escena. Si no hay cámara → no-op + toast "No camera in scene".

**Referencias:**
- **Blender:** Numpad 0 = "View Active Camera" — entra al view-through-camera de la activa.

**Diferencia con Blender:** Blender entra a un modo "viendo a través de la cámara"; nosotros copiamos la pose a la editor camera (sin estado dual). Más simple. Si en el futuro el dev pide "view-through" real, hito propio.

**Fallback sin cámara:** toast no-modal (F3H23 lo cubre nativamente; por ahora `std::cerr` + no-op).

### D4 — Render mode "Rendered" reusa el SceneRenderer del Play mode

**Decisión:** **reusar.** El modo Rendered del viewport usa el mismo pipeline (shadows + SSR + bloom + tonemap) que el Play.

**Referencias:**
- **Blender:** "Rendered" usa el render engine completo (Cycles/Eevee) — mismo pipeline del render final.
- **Unreal:** "Lit" mode = pipeline completo del PIE (Play in Editor).
- **Unity:** "Shaded" mode = mismo pipeline del runtime.

**Por qué:** mantener 2 pipelines paralelos es deuda perpetua — cualquier feature de render (bloom, SSR, CSM, color grading) hay que duplicarla. Acoplamiento aceptable: el pipeline ya es el que importa visualmente.

**Implementación:** flag `ViewportRenderMode` consumido por `SceneRenderer::renderScene` (gateando passes: shadows + SSR + bloom según el modo).

---

## Fuentes consultadas

- [Viewport Modes in Unreal Engine 5.7](https://dev.epicgames.com/documentation/unreal-engine/viewport-modes-in-unreal-engine)
- [Unity Scene view Draw Modes — Manual 6000.2](https://docs.unity3d.com/6000.2/Documentation/Manual/ViewModes.html)
- [Blender Manual — Viewport Overlays / Shading](https://docs.blender.org/manual/en/latest/editors/3dview/display/overlays.html)
- [Blender — Where to find Ortho Mode (Numpad 5 toggle)](https://cookwithrome.com/blender/where-to-find-ortho-mode-blender/)

---

## Lo que NO toca F3H21

- F3H22+ (Profiler / Console / Toasts / Crash recovery): hitos propios.
- Numpad orbit (numpad 2/4/6/8 para orbit incremental): backlog si emerge — Blender lo tiene pero es de uso bajo.
- Modo X-ray / transparent overlay: backlog (es feature avanzado, no esencial).
- Render mode "Vertex paint" / "Texture paint": fuera de scope (no tenemos paint tools).
