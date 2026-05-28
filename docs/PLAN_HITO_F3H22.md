# PLAN F3H22 — Properties Editor con icons laterales (Blender style)

**Estado:** ✅ **CERRADO** (2026-05-27) — tag `v2.22.0-fase3-hito22`. Implementación + 5 rondas de polish reactivo con el dev (ver §Cierre + ajustes).
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

## Decisiones cerradas (investigación industrial 2026-05-27)

### D1 — Single categoría activa + botón "All" opcional

**Decisión:** un solo icon activo a la vez (Blender pattern). **Botón "All"** al final de la barra para volver al modo legacy (todo apilado scrollable).

**Referencias:**
- **Blender Properties Editor:** vertical list of icons en la Navigation Bar, **un solo tab activo a la vez** (single-select).
- **Unreal Details Panel:** scroll vertical con foldouts colapsables — no tabs. Tiene "Favorites" section para promover props muy usadas al tope.
- **Unity Inspector:** foldouts colapsables, no tabs.

**Por qué Blender:** el dev pidió explícitamente *"como lo hace blender, que tiene un panel con los iconos"* — su mental model es Blender, no Unreal/Unity. Single con "All" da escape al modo legacy si necesita ver todo de una.

### D2 — Iconos FontAwesome existentes (no pack custom)

**Decisión:** usar los 7 icons FontAwesome ya en el repo (`IconsFontAwesome6.h`).

**Referencias:**
- **Blender:** usa pack propio Blender Icons — diseñado for purpose, color-coded por función (white=scene/render, fuchsia=material, blue=modifications).
- **Unreal/Unity:** usan packs propios también.

**Por qué FA en lugar de pack custom:** trabajo de arte sin upside claro para F3H22; los icons FA son universalmente reconocidos. Si el dev quiere color-coding tipo Blender, polish reactivo en otro hito.

### D3 — Categorías sin componentes se OCULTAN (no se ven grises)

**Decisión:** un icon SOLO aparece en la barra si la entity tiene ≥1 componente de esa categoría. **Object siempre presente** (toda entity tiene Transform).

**Referencias:**
- **Blender:** *"Tabs related to the active object are displayed, with some only shown for certain object types"* — Blender oculta dinámicamente las tabs irrelevantes al tipo del objeto activo.
- **Unreal:** muestra categorías declaradas en código siempre (sus props pueden estar vacíos pero la categoría se ve).

**Por qué Blender:** reducir ruido visual. Si una entity es solo Mesh+Transform, ver 4 icons grises (Audio/Physics/Animation/Gameplay) es ruido. Convención Blender pura.

**Fallback:** si la entity nueva no tiene la categoría activa → auto-switch a **Object** (siempre presente).

### D4 — Barra de icons a la izquierda en columna

**Decisión:** barra vertical en el **lateral izquierdo** del panel Inspector. Ancho fijo ~32 px (icon + padding).

**Referencias:**
- **Blender:** Navigation Bar vertical, default a la derecha pero **flippable a izquierda con RMB > Flip to Left/Right** — el dev elige.
- **Unreal:** scroll vertical único (sin barra lateral).
- **Unity:** flat list (sin barra lateral).

**Por qué izquierda + columna:** Blender es la referencia que el dev mencionó. Lateral izquierdo usa el espacio sin reducir el ancho útil del Inspector body (los contenidos del Inspector son tall + narrow, una columna lateral suma sin restar).

**Follow-up posible:** soporte para flippear a derecha (Blender-like) como pref del UserSettings.

### D5 — Big-bang (los 7 de una vez, no incremental)

**Decisión:** implementar el wrapper + las 7 categorías en una sola sesión.

**Razón:** el framework de categorías es **1 sola pieza arquitectural** (un dispatcher que decide qué `InspectorPanel_*.cpp` invocar según categoría activa). Agregar categorías incrementalmente requeriría refactor del wrapper cada vez. Big-bang permite testing visual completo (validar las 7 categorías + transiciones + fallback) en una sola validación con el dev.

**Trade-off:** el commit final será grande (~15+ archivos del Inspector tocados + UI + UserSettings field + tests + i18n). Aceptable porque cada archivo se modifica MÍNIMAMENTE — solo se agrega la "tag de categoría" a cada `InspectorPanel_*::draw()`. La lógica vive en el dispatcher central.

### D6 (out-of-scope) — Filtro de texto en la barra

**Decisión:** **NO incluir** filtro de texto estilo Unreal "Search Details" en F3H22. Si emerge demanda, hito propio.

**Razón:** scope creep — el filtro de texto es feature ortogonal a la categorización por icons. Blender no lo tiene en Properties Editor; Unreal sí pero como feature aparte del scroll.

---

## Fuentes consultadas

- [Properties Editor — Blender 5.1 Manual](https://docs.blender.org/manual/en/latest/editors/properties_editor.html)
- [Details Panel Customizations — Unreal Engine 5.7](https://dev.epicgames.com/documentation/en-us/unreal-engine/details-panel-customizations-in-unreal-engine)
- [Level Editor Details Panel — Unreal Engine 5.7](https://dev.epicgames.com/documentation/unreal-engine/level-editor-details-panel-in-unreal-engine)
- [Unity Foldout — UI Toolkit](https://www.foundations.unity.com/components/foldout)
- [Blender Properties UI — Blender Studio](https://studio.blender.org/tools/addons/cloudrig/properties-ui)

---

## Cierre + ajustes reactivos (2026-05-27)

La implementación inicial siguió las 6 decisiones, pero el dev pidió 5 ajustes durante la validación visual. Los registramos acá porque sobreescriben parcialmente las decisiones originales.

### Ajuste A — Barra vertical (no horizontal) y eliminar botón "All"
El primer intento usó barra horizontal con botón "All" al final. El dev rechazó ambos:
> *"prefiero que se mas en vertical un minipanel vertical que al clickear renderize esa seccion... el all solo confunde, prefiero separar"*

→ Barra movida al lateral izquierdo, vertical (28×28 botones). Botón "All" eliminado (y el modo "legacy todo apilado" con él) — siempre hay una categoría única activa. La decisión D1 queda actualizada: single-select sin escape al modo legacy.

### Ajuste B — Environment como singleton implícito (auto-spawn + oculto del Outliner)
Inicialmente Environment requería: a) seleccionar la entity portadora del componente, b) crearla manualmente desde el menú "Add Entity". El dev quiso pattern Blender estricto:
> *"hay que tirar mas a blender... en blender es automatico, ya esta en el rendeer view"*

Cambios encadenados:
1. **Categoría Environment siempre visible** (scene-wide) — accesible sin selección, busca el singleton en la scene.
2. **Auto-spawn `ensureEnvironmentExists()`** llamado tras `loadProject` y `openMap` — todo proyecto tiene Environment desde el arranque.
3. **Bloqueado el delete** en `EditorScene::deleteSelectedEntity` (singleton inviolable).
4. **Removido del menú "Add Entity"** (no tiene sentido crear uno cuando ya existe).
5. **Oculto del Outliner** (`HierarchyCollect`) — listarlo como entity confunde al dev (no se borra, no se duplica). La única forma de editarlo es vía la categoría Environment del Inspector.

→ Environment es ahora **completamente implícito**: el dev nunca lo ve como entity selectable, solo como categoría del Inspector. Mismo mental model que **World Properties** de Blender.

### Ajuste C — Components colapsados por defecto + eliminar toolbar "Plegar/Expandir todo"
Con categorías filtrando, la toolbar global ya no agrega valor:
> *"el de plegar todo o expandir no me interesa, ese sacalo, por defecto deben estar colapsados"*

→ Removido `ImGuiTreeNodeFlags_DefaultOpen` de `beginComponentSection`. Removido `renderSectionToolbar()` + miembro `m_forceSectionState`.

### Ajuste D — Environment al tope de la barra (no al final)
La barra inicial listaba Object al tope. El dev pidió Environment primero:
> *"que enviroment este arriba del todo"*

→ Reordenado en `renderCategoryBar`. Justificación: Environment es scene-wide (siempre accesible) y conceptualmente "el mundo entero" → arriba.

### Ajuste E — Bar del viewport render mode: transparente + cuadrado + tooltips cortos
La bar del modo de render (F3H21) también se ajustó en esta tanda:
> *"el fondo negro no me gusta, sacalo que sea transparente"*
> *"podes hacer que los iconos de wireframe, sean mas cuadrados, ademas hay mucho texto, prefiero que diga 'wireframe' 'solid', 'material' 'render'"*

→ `WindowBgAlpha=0.0` + `ImGuiWindowFlags_NoBackground`. Botones cuadrados 28×28 con `Button` (no `SmallButton`). `FramePadding=(0,0)` + `ButtonTextAlign=(0.5, 0.5)` explícito para centrar glyphs FA. Tooltips reducidos a una palabra (Wireframe/Solid/Material/Render).

### Ajuste F — Rendered ya NO fuerza SSAO/Bloom
La D-implicit del plan inicial fue: "Rendered fuerza todos los post passes". El dev objetó:
> *"estas forzando alguna configuracion en el render preview, que luego le quita al usuario el poder de activar o no, como el AO"*

→ Removido `m_forcePostPasses` del `SceneRenderer`. Material vs Rendered se diferencia ahora **solo** por `skipPostPasses` (Material salta; Rendered respeta los flags del Environment). Si el dev quiere AO/Bloom en Rendered, los activa desde el EnvironmentComponent — igual que Blender.

### Métricas finales (post-ajustes)
- 7 categorías efectivas (Environment + Object + Render + Animation + Audio + Physics + Gameplay).
- `UserSettings.editor.inspectorActiveCategory` (default `"object"`, 7 IDs válidos — sin `"all"`).
- 13 test cases / 26 asserts F3H21+F3H22 (test_user_settings_editor.cpp + test_editor_camera_lerp.cpp).
- Build limpio + 1236/1236 tests pasaron.

---

## Lo que NO toca F3H22

- F3H23+ (Profiler / Console / Toasts / Crash recovery): hitos propios.
- Cambios al modelo de datos de componentes — solo se reorganiza el rendering del Inspector, no la ECS.
- Edición multi-entity per-categoría (multi-edit F3H8 sigue funcionando como hoy — no se cambia la mecánica, solo qué se muestra).
- Refactor de cada `InspectorPanel_*.cpp` individual (los 15+ archivos) — la categoría es un wrapper que decide qué llamar, los paneles internos no se tocan.
- Filtro de texto del Inspector (sería un nice-to-have separado, no es Blender-style).
- Drag & drop de categorías (Blender no lo tiene en Properties Editor).
