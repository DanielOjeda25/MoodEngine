# PLAN F3H22 — Properties Editor con icons laterales (Blender style)

**Estado:** **A DEFINIR** (arrancar tras F3H21).
**Predecesor:** F3H21 (viewport pro — numpad views + render modes).
**Origen:** insertado en reorden 2026-05-27 a pedido del dev al cerrar F3H21:
> *"creo que debemos hacer un cambio importante, como lo hace blender, que tiene un panel con los iconos, y ahi el icono de cada seccion, sea el de materiales, scripts, etc, esto se que es un hito mas grande pero podriamos mejorar exponencialmente esto"*

Estaba anotado en `backlog-ux-gaps-editor` como follow-up; promovido a hito propio + insertado antes de F3H23 (Profiler).

---

## Avance de Sub-fase 3.4 (post-F3H21, reorden 2026-05-27)

```
F3H20 –  ✅ Snapping configurable Hammer-style
F3H21 –  ✅ Viewport pro: numpad views + 4 render modes
F3H22 –  — Properties Editor con icons laterales (Blender style) ⬅ próximo
F3H23 –  — Performance feedback: Profiler + Stats overlay
F3H24 –  — Comunicación al dev: Console + Toasts
F3H25 –  — Crash recovery + autosave (cierra Fase 3)
```

---

## Norte

**Hoy:** el Inspector es un panel scrollable largo con todos los componentes de la entity seleccionada apilados verticalmente. Una entity con muchos componentes (Transform + MeshRenderer + Light + Audio + Animator + Vehicle + Physics + Inventory + Script + ...) requiere scrollear y abrir/cerrar headers para encontrar lo que el dev busca.

**Post-F3H22:** una **barra vertical de icons** al lateral izquierdo del Inspector con categorías clickeables (estilo Properties Editor de Blender / Details panel de Unreal). Click en un icon muestra SOLO esa categoría — el resto se oculta. Categoría activa destacada. Cada icon con tooltip i18n. Estado per-instalación: la categoría seleccionada se persiste en `UserSettings.editor.inspectorActiveCategory`.

**Mecánica del editor:** seleccionás una entity → ves la barra de icons; click en "Mesh" → ves solo MeshRenderer + materiales; click en "Physics" → ves solo Rigidbody + Collider; click en "Script" → ves solo ScriptComponent + exposed properties; etc.

---

## Scope candidato

### Categorías (7 fijas — afinado 2026-05-27)

Tras revisión con el dev: muchas de las categorías iniciales (Material/Dialog/Quest/Vehicle/Mesh propios) eran **assets** que ya viven en el AssetBrowser. El Inspector solo necesita categorías de **componentes** que el dev edita per-entity. Resultado: **7 categorías** (Blender tiene ~10, Unity ~6 — 7 es buen punto medio).

| ID | Icon | Label | Componentes que agrupa |
|---|---|---|---|
| `object` | `ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT` | **Object** | Transform + Tag + VisGroupMembership. Posición/rotación/escala + nombre + grupo de visibilidad. Lo que define "dónde y qué es" la entity. |
| `render` | `ICON_FA_CUBE` | **Render** | MeshRenderer + Brush + Light + Camera + ParticleEmitter. Todo lo que **se ve** en el viewport: geometría, luces, cámaras, partículas. Light/Camera/Particles entran acá porque son singles que no merecen tab propio. |
| `animation` | `ICON_FA_PERSON_RUNNING` | **Animation** | Animator. Skeletal animation + clips + state machine + blending. Categoría propia porque tiene state complejo (current clip, blend weights, animation events). |
| `audio` | `ICON_FA_VOLUME_HIGH` | **Audio** | AudioSource + Listener. Fuentes de sonido + el listener (cámara que oye). Pocos campos pero conceptualmente distintos del render visual. |
| `physics` | `ICON_FA_BOLT` | **Physics** | RigidBody + Collider + Joint + Ragdoll + Cloth + Trigger + ForceField. Todo sistema físico (Jolt-backed). Trigger/ForceField caen acá porque comparten layer/mask del physics world. |
| `gameplay` | `ICON_FA_GAMEPAD` | **Gameplay** | Script + Inventory + ItemPickup + Dialog + Vehicle + Quest. Logic + state que define **el juego en sí**. Patrón común: componente que apunta a un asset (.lua/.mooditem/.mooddialog/.moodvehicle/.moodquest) + state per-instance. |
| `environment` | `ICON_FA_GLOBE` | **Environment** | EnvironmentComponent (singleton de la escena). Skybox + fog + tonemap + bloom + SSAO + SSR + CSM + color grading. Solo visible si la entity actual es la portadora del singleton. |

### Reglas de visibilidad

- Cada icon solo aparece en la barra si la entity tiene **≥1 componente** de esa categoría. Un cubo con solo Transform+MeshRenderer ve solo **Object** y **Render** (no spammear icons grises).
- **Object** siempre visible (toda entity tiene Transform).
- Categoría activa destacada con background cyan (mismo patrón visual que el viewport render mode bar de F3H21).
- Si la entity nueva no tiene la categoría activa → fallback automático a **Object** (siempre presente).

### Persistencia + UX

- `UserSettings.editor.inspectorActiveCategory` (string id, default `"object"`). Acepta los 7 IDs de la tabla.
- La categoría seleccionada se preserva al cambiar de entity (sticky entre selecciones).
- Si la entity nueva no tiene la categoría activa → fallback a **Object** (siempre presente).
- Botón **"All"** opcional al final de la barra → modo legacy (todo apilado scrollable como hoy). Opt-in para devs que prefieren la vista flat.

### Tooltips i18n por categoría

Cada icon necesita tooltip claro que explique qué edita:

- **Object** — `"Posición, rotación, escala. Nombre y grupo de visibilidad."`
- **Render** — `"Geometría, materiales, luces, cámaras, partículas. Todo lo visible en el viewport."`
- **Animation** — `"Animaciones del esqueleto: clips, blending, state machine, eventos."`
- **Audio** — `"Fuentes de sonido + listener (la cámara que oye)."`
- **Physics** — `"Cuerpos rígidos, colliders, joints, ragdoll, cloth, triggers y fuerzas."`
- **Gameplay** — `"Scripts, inventario, diálogos, misiones, vehículos. La lógica del juego."`
- **Environment** — `"Skybox, niebla, bloom, SSAO, SSR, color grading. Render global de la escena."`

### Tests

- Categoría persistida roundtrip (`inspectorActiveCategory` toJson/fromJson).
- Filtrado: entity con N componentes en M categorías → solo se renderizan los de la categoría activa.
- Fallback al cambiar de entity (categoría activa no presente → "object").
- Validación de los 7 IDs aceptados; ID desconocido → fallback "object".

---

## Decisiones a tomar al arrancar

1. **Single categoría vs multi-pin** — Blender = single (un icon activo a la vez). Unreal Details = scroll con todo + filtro de texto. **Recomendación inicial: single + botón "All" opcional.**
2. **Iconos: FontAwesome existentes vs pack custom** — FontAwesome ya en el repo. **Recomendación: usar FA, los 7 icons propuestos ya existen en el codebase.**
3. **Posición de la barra: izquierda (Blender) vs arriba (Unity tabs)** — **Recomendación: izquierda en columna** (Blender), usa el lateral sin reducir ancho útil del panel.
4. **Big-bang vs incremental** — implementar los 7 de una vez o por bloques. **Recomendación: big-bang** porque el framework de categorías es 1 sola pieza; agregar categorías una a una requeriría refactor del wrapper cada vez.
5. **Filtro de texto opcional** en la barra (estilo Unreal "Search Details") — **out-of-scope F3H22** (deja para hito propio si emerge demanda).

---

## Lo que NO toca F3H22

- F3H23+ (Profiler / Console / Toasts / Crash recovery): hitos propios.
- Cambios al modelo de datos de componentes — solo se reorganiza el rendering del Inspector, no la ECS.
- Edición multi-entity per-categoría (multi-edit F3H8 sigue funcionando como hoy — no se cambia la mecánica, solo qué se muestra).
- Refactor de cada `InspectorPanel_*.cpp` individual (los 15+ archivos) — la categoría es un wrapper que decide qué llamar, los paneles internos no se tocan.
- Filtro de texto del Inspector (sería un nice-to-have separado, no es Blender-style).
- Drag & drop de categorías (Blender no lo tiene en Properties Editor).
