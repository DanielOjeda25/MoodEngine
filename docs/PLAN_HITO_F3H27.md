# PLAN F3H27 — Polish UX restante de Fase 3 (gaps de interacción)

**Estado:** 📝 **STUB** — abierto post-F3H26 con 4 items que el dev quiere arreglar antes de pasar a Fase 4 ("son detalles de interacción del editor, antes de comenzar a desarrollar"). Por arrancar.
**Predecesor:** F3H26 (Polish UX modal/menubar/toasts/preferences).
**Origen:** feedback del dev al validar F3H26 + conversación sobre workflow brush-based:
> *"quiero mejorar esta parte de grupos que estan separados, y los maptools que esten mas insertados mas en menus como los que hicimos en layout, como los de blender, que hicimos hito atras"*
> *"el tamaño del mundo es pequeño, ya que 1 = 1, si quiero hacer un mapa grande, me limita la camara orbital"*
> *"y otra cosa que me limita, es que aun no puedo cambiar el HDRI de forma dinamica, no hay ciclo de dia y noche, como en unreal engine"*

---

## Avance de Sub-fase 3.4 (post-F3H26)

```
F3H20 – ✅ Snapping configurable Hammer-style
F3H21 – ✅ Viewport pro: numpad views + 4 render modes
F3H22 – ✅ Properties Editor con icons laterales (Blender style)
F3H23 – ✅ Performance feedback: Profiler + Stats overlay
F3H24 – ✅ Comunicación al dev: Console + Toasts
F3H25 – ✅ Crash recovery + autosave
F3H26 – ✅ Polish UX del editor
F3H27 – 📝 Polish UX restante (gaps de interacción) ⬅ este stub
```

---

## Norte

F3H26 cerró los polish "menores" (modal / menubar / toasts / preferences sidebar). F3H27 cierra los polish "estructurales" que el dev nombró como bloqueantes para empezar a desarrollar contenido (Fase 4):

1. **Grupos + Map Tools** mejor integrados (paneles flotantes hoy → categorías del Properties Editor estilo F3H22).
2. **Parenting jerárquico de transforms** (un Empty como padre + brushes/meshes como hijos) — escala el level design.
3. **Mundo grande** — la cámara orbital limita mapas con escala 1u=1m. Subir el far plane + revisar zoom orto / pan / clipping.
4. **HDRI dinámico** + ciclo día/noche — hoy el skybox se carga 1 vez en `SceneRenderer ctor` y no se puede cambiar runtime.

---

## Items por cerrar (sin decisiones aún — pre-AskUserQuestion)

### Item 1 — Grupos + Map Tools como categorías del Properties Editor
**Hoy:** dos paneles flotantes separados ("Map Tools" + "Grupos") que ocupan tabs en el dock derecho. El "Grupos" está vacío con un botón "+ Nuevo grupo" — feature stub.

**Objetivo:** integrar ambos como categorías del Properties Editor (F3H22 ya tiene 7 categorías con icons laterales). Probablemente:
- Categoría nueva "Map Tools" (icon herramienta): selección, bloque, pincel, clip, vertex/edge/face submode, snap, labels, carve.
- Categoría nueva "Grupos" (icon grupo): lista de grupos del proyecto + new/delete/rename + asignar/quitar entidades. Hooked al `VisGroup` existente o a un sistema nuevo.

**Decisiones a cerrar:**
- ¿Grupos = `VisGroup` (visibility-toggle) o **`Group` nuevo** (parent transform + children)? El dev distinguió "mover el edificio entero" → eso es parent transform, no `VisGroup`.
- ¿Map Tools como categoría del Properties Editor o como **toolbar lateral persistente** (Blender T-key panel)?

### Item 2 — Parent/child transforms + hotkey Ctrl+G
**Hoy:** no existe parenting de `TransformComponent`. Multi-select + drag mueve N entidades pero sin relación lógica.

**Objetivo:** Empty/Group entity como padre + brushes/meshes como hijos. Mover el padre = mover hijos.

**Implementación tentativa:**
- `TransformComponent` gana `entt::entity parent = entt::null`.
- `TransformComponent::worldMatrix()` recursivo (acumula world del padre).
- Outliner: drag-drop entity sobre otra → reparent.
- Hotkey Ctrl+G: agrupa selección bajo un nuevo Empty padre centrado en el AABB.
- Hotkey Shift+Ctrl+G: desagrupa.
- Undo/Redo: comandos `SetParentCommand` + `GroupSelectionCommand`.

**Decisiones a cerrar:**
- ¿`worldMatrix()` cacheado o recomputado por frame? Ya hoy es por frame.
- ¿Reparent preserva world-space (Blender/Unity) o local-space (no recalcula)? Estándar = preserva world.
- ¿Cómo se serializa al `.moodmap`? Hoy las entidades viven plano; agregar `parent_handle` por entidad.

### Item 3 — Mundo grande (camera limits)
**Hoy:** la EditorCamera orbital tiene un far plane y un radio de orbit calibrados para mapas pequeños (1u=1m, mapa de ~50×50u).

**Síntoma reportado:** al construir mapas con escala industrial (Unity/Unreal — 1u=1m con mapas de 1000×1000m), el orbit se siente truncado, la cámara no permite alejarse lo suficiente, los ortos se siegan.

**Probable fix:**
- `EditorCamera::farPlane`: dinámico según AABB de la escena (o configurable).
- `m_orbitDistance` clamp superior subido o eliminado.
- `OrthoCamera::worldHeight` clamp superior subido.
- Quizá un toggle "World scale: small / medium / large" en Project Settings que ajusta defaults sin cambiar la convención 1u=1m.

**Decisiones a cerrar:**
- ¿Auto-adapt al AABB del mapa o pref manual?
- ¿Far plane fijo grande (afecta z-fighting) vs reverse-Z (mejor para mapas grandes)?

### Item 4 — HDRI dinámico + ciclo día/noche
**Hoy:** `SceneRenderer::loadSkyboxAndIblFromBase("skyboxes/sky_kloofendal")` se llama 1 vez en el ctor. Cambiar el HDRI runtime no está expuesto en UI.

**Objetivo Unreal-style:**
- Inspector del EnvironmentComponent expone un dropdown/slider para elegir HDRI del proyecto.
- Opción "Time of day" con curve animable (0-24h) que blends entre múltiples HDRIs o aplica rotación al skybox + intensity/tint dinámicos.
- Posiblemente: directional light que sigue el "sun position" calculado del time-of-day.

**Implementación tentativa:**
- Exponer `SceneRenderer::swapSkybox(basePath)` (ya existe `loadSkyboxAndIblFromBase` privado; promover a método público).
- Asset Browser tab "Skyboxes" que ofrece drag-drop al EnvironmentComponent.
- `EnvironmentComponent` gana `skyboxBase: string` + opcional `timeOfDay: f32` + `dayHdri/nightHdri` para blend.
- Recálculo de IBL al swap (irradiance + prefilter) — operación cara, hacerla async o on-confirm.

**Decisiones a cerrar:**
- ¿HDRI estático swap-on-change vs blend dinámico time-of-day? Lo segundo es mucho más caro (2× IBL en memoria + lerp por frame).
- ¿Sol como entidad explícita (DirectionalLight con script de rotación) o auto-derivado del time-of-day?

---

## Hitos vecinos (alcance fuera de F3H27)

- **Sistema de partículas environment** (lluvia, nieve, polvo) — Fase 4.
- **LOD dinámico de meshes según distancia** — Fase 4.
- **Streaming de tiles** (mapa demasiado grande para caber en RAM) — Fase 4 o 5.
- **Volumetric fog** — ya hay fog escalar (F2H17), volumétrico es hito propio.
- **Procedural sky (Hosek/Wilkie)** — alternativa a HDRI estático. Hito propio si el dev pide después.

---

## Backlog explícitamente diferido a Fase 4

- Networking / multijugador (`project_out_of_scope`).
- i18n de la UI del juego (solo editor está i18n; el game runtime es out-of-scope).
- Cursores custom (`project_out_of_scope`).

---

## Lo que NO toca F3H27

- F4 inicia tras cerrar este hito. Cualquier feature de gameplay/contenido = Fase 4.
- Refactor del CSG no-convex (mesh editing tipo Blender) — sigue out-of-scope.
- Re-arquitectura del render pipeline (Vulkan, Deferred) — no es UX, no es Fase 3.
