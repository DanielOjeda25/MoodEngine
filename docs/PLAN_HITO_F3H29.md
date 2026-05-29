# PLAN F3H29 — Camera limits (mundo más grande) + polish UX play mode + audit traducciones

**Estado:** ✅ **CERRADO** (cerrado 2026-05-29).
**Predecesor:** F3H28 (Grupos + Map Tools como categorías).
**Origen mixto:**
- **Item 3 del stub F3H27** ("Mundo grande"). Reportado por el dev: *"el tamaño del mundo es pequeño, ya que 1=1, si quiero hacer un mapa grande, me limita la cámara orbital"* + clarificación al cerrar decisiones: *"es no por un tema de jugabilidad, solo de desarrollo, he testeado crear un brush enorme y no llego a verlo, o alejarme para trabajar en los detalles"*.
- **Polish post-validación visual (mid-hito)**: durante la validación del Play mode con los nuevos camera limits, el dev detectó 3 issues no relacionados al far plane pero del mismo workflow: (1) el viewport render mode bar y los demás overlays del editor seguían visibles en Play mode compitiendo con el HUD del juego; (2) el popover de "Ajustes del snap" no tenía botón cerrar explícito; (3) la key `editor.menu.edit.project_settings` tenía valor en inglés en `es.json` + ~25 strings hardcoded sin pasar por `I18n::T` en paneles de assets/inspector. Decisión del dev: *"perdemos tiempo si separamos en otro [hito]"* → se expande F3H29 en lugar de abrir F3H30 separado.

---

## Sub-fase 3.4 (post-F3H28)

```
F3H20 – ✅ Snapping configurable Hammer-style
F3H21 – ✅ Viewport pro: numpad views + 4 render modes
F3H22 – ✅ Properties Editor con icons laterales (Blender style)
F3H23 – ✅ Performance feedback: Profiler + Stats overlay
F3H24 – ✅ Comunicación al dev: Console + Toasts
F3H25 – ✅ Crash recovery + autosave
F3H26 – ✅ Polish UX del editor
F3H27 – ✅ Parenting jerárquico de transforms
F3H28 – ✅ Grupos + Map Tools como categorías del Properties Editor
F3H29 – ✅ Camera limits + polish play mode + audit traducciones ⬅ este hito
F3H30 – 📝 HDRI dinámico + ciclo día/noche (stub)
```

---

## Norte

Hoy el dev no puede navegar el editor cómodamente en mapas medianos:
- `EditorCamera::m_far = 100.0f` hardcoded — cualquier brush a > 100m del eye se clipea (desaparece).
- `EditorCamera::k_maxRadius = 50.0f` hardcoded — la cámara orbital no se aleja más de 50m del target. Si el dev quiere "ver el edificio completo", no llega.
- `FpsCamera::m_far = 100.0f` — mismo problema en Play mode.
- `CameraComponent::farPlane = 100.0f` — default de cámaras gameplay nuevas también.

F3H29 sube estos limits 10× (default conservador) y los hace **configurables en runtime via UserSettings**.

---

## Research previo (lo que está ya OK)

| Camera | Far/clamp actual | Estado |
|---|---|---|
| **EditorCamera** (orbit perspectivo del editor) | far=100m, radius clamp [0.5, 50m] | ❌ Top blocker |
| **FpsCamera** (Play mode) | far=100m | ❌ Mapa se clipea |
| **OrthoCamera** (4-viewport del editor de mapas) | worldHeight clamp [1, 8192], proj [0.1, 4096] | ✅ Cubre mapas ≥ 8km |
| **CameraComponent** (cámaras gameplay in-scene) | far=100m default (editable hasta 10000) | ⚠️ Default chico |
| **MaterialPreviewRenderer / Thumbnails** | far=100m | ✅ Irrelevante (thumbnails) |

OrthoCamera tiene un `eyePos = center - forward * 1024.0f` hardcoded que podría tener problema en mapas con altura > 1km en Y, pero queda fuera de scope (uso edge-case).

---

## Decisiones cerradas

**D1 — Storage: UserSettings (per-instalación).** El dev clarificó al cerrar decisiones: *"es no por un tema de jugabilidad, solo de desarrollo, he testeado crear un brush enorme y no llego a verlo, o alejarme para trabajar en los detalles"*. La selección literal del AskUserQuestion fue ".moodproj > World" pero la justificación verbal apunta a **preferencia del dev** (per-instalación), no decisión del proyecto. Implementación efectiva: `UserSettings::EditorSettings` gana 2 fields nuevos.

Alternativa descartada:
- **`.moodproj > World` (per-proyecto)**: tendría sentido si la escala del mundo fuera decisión arquitectónica del proyecto (un FPS de pasillos vs open-world). Pero el dev dice explícito que no — es preferencia ergonómica del dev (cuánto quiere ver).

**D2 — Defaults nuevos: conservador 10×.** `editorCameraFarPlane = 1000.0f` (era 100), `editorCameraMaxOrbitRadius = 500.0f` (era 50). Cubre mapas urbanos / arenas medianas (HL2-style 500-1000m). Z-precision OK con 24-bit depth buffer + near=0.1 (granularidad ~10cm a 1km). Compatible con engines viejos sin reverse-Z.

Alternativas descartadas:
- **Agresivo 100× (far=10000, orbit=5000)**: cubre open-world GTA-style. Z-precision degrada cerca del far plane (z-fighting > 5km). El dev eligió conservador.
- **Extremo 1000× (far=100km)**: flight-sim. Fuera del scope MoodEngine.

**D3 — CameraComponent in-scene: subir default a 1000m también (mismo que EditorCamera).** Coherencia visual: lo que ves moviendo la cámara del editor = lo que ves por una cámara gameplay nueva sin tunear. El campo sigue editable en el Inspector (slider 1-10000) — solo cambia el default inicial. Implementación: el ctor de `CameraComponent` lee `UserSettings::editor().editorCameraFarPlane` para inicializar `farPlane`; valores override del .moodmap pre-F3H29 se preservan via JSON value-or-default.

Alternativa descartada:
- **`gameCameraFarPlane` separado**: campo distinto del editor far. Levemente más complejo de mantener en sync, valor agregado marginal.
- **No tocar (default 100m queda)**: cada gameplay camera se ajusta a mano. Default desfasado del workflow del editor.

**D4 — Reverse-Z infinito: out-of-scope.** Reverse-Z resuelve precision Z a cualquier distancia (perfecta en near, degradada-pero-aceptable hacia far). Requiere tocar TODOS los shaders (depth comparisons `< → >`), pipeline state (`glDepthFunc`, `glClearDepth`, `glDepthRange`), debug overlays, shadow passes. ~3-4 días de trabajo, hito propio si emerge demanda (mapas ≥ 5km con z-fighting reportado). F3H29 resuelve 90% del bug con D2 sin tocar shaders.

---

## Implementación (compacta)

**1. `UserSettings::EditorSettings`** (`src/core/UserSettings.h/.cpp`):
- `f32 editorCameraFarPlane = 1000.0f` (clamp `[100.0, 100000.0]` en `fromJson`).
- `f32 editorCameraMaxOrbitRadius = 500.0f` (clamp `[10.0, 50000.0]` en `fromJson`).
- `toJson` solo escribe si difieren del default (back-compat con settings.json pre-F3H29).

**2. `EditorCamera`** (`src/engine/scene/core/EditorCamera.h/.cpp`):
- Quitar `static constexpr float k_maxRadius = 50.0f` del namespace anónimo.
- `m_far = 100.0f` y `m_maxRadius = 50.0f` pasan a members default.
- Setters nuevos: `void setFarPlane(f32 v)`, `void setMaxOrbitRadius(f32 v)`.
- Usar `m_maxRadius` en lugar de `k_maxRadius` en los 3 callsites (`applyWheel`, `focusOn`, `setPose`, `beginLerpTo`).

**3. `FpsCamera`** (`src/engine/scene/core/FpsCamera.h/.cpp`):
- `m_far = 100.0f` member default.
- Setter `void setFarPlane(f32 v)`.

**4. `EditorApplication`** (`src/editor/application/EditorApplication_Run.cpp`):
- En `tickFrameMetrics`: leer `UserSettings::editor()` y `m_editorCamera.setFarPlane(...) + setMaxOrbitRadius(...)`. Cheap (2 floats), idempotente.
- En `enterPlayMode`: sync `m_fpsCamera.setFarPlane(...)` también.

**5. `CameraComponent`** (`src/engine/scene/components/Components_Render.h`):
- Default `farPlane = 1000.0f` (era 100). Cambio data-driven — proyectos pre-F3H29 que persistieron `farPlane=100` lo preservan via JSON `value("farPlane", 1000.0f)`.

**6. UI** (`src/editor/panels/project/UserPreferencesPanel.cpp` categoría Viewport, nueva sección "Mundo"):
- `SliderFloat` Far plane editor (100 .. 100000 en log scale o `ImGuiSliderFlags_Logarithmic`).
- `SliderFloat` Max orbit radius (10 .. 50000 log).
- Reset buttons.

**7. i18n ES/EN**: ~6 keys nuevas (section "Mundo", 2 labels, 2 tooltips, hint).

**8. Tests** (`tests/test_user_settings_editor.cpp`):
- 4 nuevos: defaults / toJson omit-if-default / clamp [100, 100000] far / roundtrip.

---

## Backlog post-F3H29

- **Reverse-Z infinito** (D4) — hito propio si emerge z-fighting reportado en mapas grandes.
- **OrthoCamera eye dist** (1024 hardcoded) — edge case mapas con altura > 1km en Y, no reportado.
- **Move CameraComponent.farPlane editable range from `1-10000` to `1-100000`** — el Inspector slider actual max es 10000.
- **`.moodproj > World` per-proyecto override** — si en el futuro el dev quiere que un proyecto fuerce sus propios limits, agregar override.
- **Mejorar OrthoCamera default zoom** — hoy 32u (Hammer style); en mapas grandes el dev arranca con un viewport muy zoomeado-in. Posible: persistir el zoom último.
- **i18n keys de tooltips internos del tuning de vehículos** (~10 strings descriptivas del form de import) — quedaron hardcoded en ES; bajo impacto porque solo se ven al importar un .glb/.fbx.
- **Play mode shading mode**: el comportamiento "Play ignora viewport render mode" se mantiene como decisión de diseño (Play = game-feel real). La barra ya no se ve, así que la inconsistencia que reportó el dev queda resuelta. Si el dev cambia de opinión, revisar `EditorRenderPass.cpp` líneas 84-89.

---

## Decisiones polish (mid-hito)

**D5 — Overlays del editor ocultos en Play mode.** Pedido literal del dev: *"en play mode no debe mostrar nada externo solo el hud del juego, los modos de viewport solo en el editor no en play mode"*. Implementación: en `ViewportPanel::onImGuiRender`, las 3 sub-windows (`drawViewportToolsOverlay`, `drawViewportSnapStatusBar`, `drawViewportRenderModeBar`) skipean su render si `m_editorUi->mode() == EditorMode::Play`.

Alternativa descartada:
- **Disable + grayed-out**: dejar los botones visibles pero deshabilitados. El dev fue explícito en que en Play "no debe mostrar NADA externo". Hide gana.

**D6 — Botón cerrar explícito en popovers, no solo "click afuera".** Pedido del dev: *"falta el botón de cerrar"* en el popover de Ajustes del snap. ImGui `BeginPopup` cierra solo con click-afuera por default — el dev espera el patrón estándar de ventana con "X" arriba derecha. Implementación: `SmallButton("X")` + `CloseCurrentPopup()` en el header del popover en `InspectorPanel_MapTools.cpp:146-167`. Pattern reutilizable si emergen más popovers con scroll/contenido extenso.

**D7 — Keys i18n compartidas para acciones comunes.** Para evitar duplicar `editor.foo.cancel` / `editor.bar.cancel`, se agregaron keys "modal.common" reutilizables: `cancel`, `save`, `save_as`, `delete`, `edit`, `new`, `no_scene`, `no_scene_assets`. Esto fija el vocabulario UI del editor (Guardar/Cancelar/Eliminar/Editar/Nuevo) y permite mejorar la traducción de las 8 keys en un solo lugar.

---

## Decisiones polish round-2 (post-validación visual)

Durante la validación visual de D1-D7, el dev abrió múltiples gaps de UX adicionales sobre el flow de Inspector / Brush / Environment / mundo default. Decisión: *"perdemos tiempo si separamos en otro [hito]"* → se expande F3H29 en lugar de abrir F3H30.

**D8 — Inspector UX Blender-style.** Pedido del dev: *"en object esta agregar component, en render tambien, confunde... la parte de materiales me parece muy confusa entre mesh y brushes, no se enseñan bien, veo mas texto que lugares de edicion, te dije que me gustaria que sea mas como blender, donde agrupa los materiales en una lista y cada uno tiene su nivel de edicion"*. Cambios:
- **+ Agregar Componente** solo aparece en categoría `object` (no se repite en Render/Materials/etc).
- **Materiales unificados Blender-style** (MeshRenderer + Brush): ListBox vertical de slots + panel de edición del slot seleccionado. Helpers compartidos `drawPbrMultipliers / drawShaderGraph / drawBlending` en archivo nuevo `InspectorPanel_Materials.cpp/.h`.
- **Colapsables defaults**: Surface / Shader / Blending / UV / Info colapsados por default (antes abiertos = ruido visual).
- **Reset buttons (↺)**: en cada slider PBR para volver al default.
- **Menos texto**: removidos status hints `albedo:0 MR:0`, `(graphs en assets/shaders/graphs/...)`, IOR presets, `Slot N — material asignado`, `(id N)`, `UV (Brush)`.
- **Info colapsable del brush ELIMINADO**: el dev lo consideró innecesario.

**D9 — Sweep textos colgados.** Pedido del dev: *"mientras haya menos texto posible mejor, que se entienda que se trabaja como si fuera blender / ahora quiero que vayas por cada panel y revises si hay textos asi de molestos e innecesarios y los elimines"*. Cleanup:
- `InspectorPanel_Environment.cpp`: 3 hints debug (skybox_hint, csm_hint, ssr_hint) removidos.
- `InspectorPanel_Physics.cpp`: body_id_hint (RigidBody) + ragdoll state_hint removidos.
- `InspectorPanel_Joint.cpp`: fixed_help (párrafo informativo) + constraint_id_hint removidos.
- `UserPreferencesPanel.cpp`: 4× live_apply_hint duplicado removido (era `"Los cambios se aplican al soltar el slider"` 4 veces, una por sub-sección).
- **Static/Dynamic discovery hint** removido (lo agregué inicialmente al re-emerger la propiedad y el dev lo pidió fuera).

**D10 — Persistencia materiales Brush + Undo delete brush (bugs).** Reportados por el dev: *"le acabo de poner 2 materiales a un brush, guardé, cerré el proyecto y volví a abrir no persistió los materiales / borré un brush y no apreté ctrl+z y no volvió"*. Root causes y fixes:
- **Persistencia rota**: cuando el dev arrastra una textura a una cara, `BrushComponent.materials` guarda un material wrapper con path `__runtime_tex#N` que `loadMaterial()` no resuelve al reload. Fix en `SceneSerializer::serializeBrush`: si el path empieza con `__runtime_tex#` o `__tex#`, resolverlo al path real de la textura (`MaterialAsset.albedo`). Fix simétrico en `SceneLoader::applyBrushFromSaved`: si el path termina en `.png`/`.jpg`/etc, usar `loadTexture + createMaterialFromTexture` en vez de `loadMaterial`.
- **Undo delete brush vacío**: `serializeEntityToJson` skipea brushes (porque la serialización viaja por `serializeBrush` separado), entonces `DeleteEntityCommand` capturaba un SavedEntity sin info. Fix: capturar también un `SavedBrush` paralelo en el ctor + aplicar via `SceneLoader::applyBrushFromSaved` en el undo.

**D11 — Material Preview sin fog.** Reportado por el dev: *"porque en material preview si alejo la camara comienza a ponerse blanco?"*. Root cause: la fog se setea en el lighting pass (no en post-process), por lo que `m_skipPostPasses = true` en Material/Solid/Wireframe NO desactivaba la fog — el "blanco-a-distancia" era fog en uniform. Fix: `uFogMode = m_skipPostPasses ? 0 : static_cast<int>(m_fog.mode)` en `SceneRenderer_Render_Lighting.cpp`. Resultado: fog visible **solo** en modo Rendered (que es lo que pidió el dev).

**D12 — Environment singleton auto-recover.** Pedido del dev: *"porqué otra vez dice que debo agregar el componente environment? no se supone que venía incrustado automáticamente... COMO BLENDER, para que debería agregar el componente si es obvio que al final del día lo agregarán debe estar implícito"*. Cambios:
- **`handleNewProject`** ahora llama `ensureEnvironmentExists()` después de `rebuildSceneFromMap()` (gap del flow nuevo-proyecto: el Environment se generaba al cargar un .moodmap pero no al crear uno desde cero).
- **Inspector auto-recover**: si el dev entra a categoría 🌍 Environment y el singleton no existe, se auto-crea silenciosamente via request/consume pattern (`EditorUI::requestEnsureEnvironment()` + `EditorApplication_Run::pumpUiRequests` lo consume). Cubre casos: proyectos pre-F3H22 sin Environment serializado, dev hace delete del singleton, scene reset. Pattern Blender World Properties (no se puede "no tener world").

**D13 — Mundo vacío default (Floor eliminado).** Pedido del dev: *"este es el mesh que se importa automáticamente en cada mapa por defecto, en los programas industriales, comienza el mundo vacío no? que es lo que pienso que deberíamos hacer"*. Pre-F3H29 cada mapa nuevo arrancaba con un cubo Floor 12×0.1×12m con `grid.png` como albedo simulando un tablero de tiles. Fix: bloque de generación de Floor eliminado de `rebuildSceneFromMap`. Industria estándar (Unity/Unreal/Hammer/Godot) arranca sin floor pre-spawneado — el dev arma su piso con un Box Brush.

Trade-off documentado: en Play mode sin piso los objetos físicos caen al vacío (comportamiento estándar de los engines mencionados). El grid del viewport (helper visual) sigue presente, así que el dev tiene referencia espacial.

**D14 — Texture drop sin tile-grid legacy.** Reportado por el dev: *"yo arrastro esta textura, pero paso de cierto límite, y se desaparece es como que el tile no tiene lugar, las texturas no deberian ser cubos, deberian ser opciones validas para a futuro contruir algo usando brushes"*. Root cause: `processViewportTextureDrop` tenía un fallback a `pickTile` del tile-grid legacy de Fase 1 — si la textura no caía sobre un brush, pintaba un cubo-pared (`SetTileCommand`). Si pasaba el límite del grid, el `pickTile` no hacía hit y la textura "desaparecía". Fix:
- Eliminado fallback al tile-grid.
- **Drop sobre MeshRenderer** (nuevo): la textura se aplica al slot 0 del mesh — simétrico al drop de material. Cubre el caso natural "tirar textura a un mesh `.glb`".
- **Drop sobre nada**: log silencioso, drop ignorado (texturas son recursos, no entidades).

Las primitivas del **modal de entidades** (Cube/Sphere/Plane/etc) siguen spawneables — el dev clarificó: *"las primitivas estan bien"*.

---

## Lo que NO toca F3H29

- F3H30 (HDRI dinámico + ciclo día/noche) — hito propio.
- Refactor del pipeline render para Reverse-Z (D4).
- OrthoCamera limits (ya OK).
- MaterialPreview / Thumbnails far (irrelevante).
- LOD / culling distance — propio del pipeline, no del editor.
- Cambiar el shading mode del Play mode (sigue siendo MaterialPreview-equivalente forzado). Ahora la inconsistencia es invisible porque la barra está oculta.
- Tooltips de tuning fino de import vehicle (decel, handbrake, steer, etc.) — bajo impacto, backlog.
- Welcome modal title `"MoodEngine - bienvenida"` (ID interno mixto inglés/español). No se ve al usuario (es el ID de ImGui); el título visible ya está en i18n.

---

## Cierre — checklist

- [x] D1-D14 documentadas
- [x] UserSettings + EditorCamera + FpsCamera + CameraComponent
- [x] UI sección "Mundo" en Preferences
- [x] Tests: 1288 verde / 11853 asserts
- [x] i18n: 8 keys comunes + 25 keys específicas
- [x] Overlays Play mode ocultos
- [x] Botón cerrar popover snap
- [x] Inspector UX Blender-style (D8): materiales unificados ListBox + slot panel, +Agregar Componente solo en Object, colapsables defaults, reset buttons ↺
- [x] Sweep textos colgados (D9): Environment / Physics / Joint / UserPreferences
- [x] Bug fixes (D10): persistencia materiales Brush + undo delete brush
- [x] Material Preview sin fog (D11)
- [x] Environment singleton auto-recover (D12)
- [x] Mundo vacío default (D13): Floor 12×12 grid eliminado de `rebuildSceneFromMap`
- [x] Texture drop sin tile-grid legacy (D14): brush / mesh / nada
- [x] Build MoodEditor + mood_runtime_files + mood_tests
- [x] Validación visual en editor (todos los items arriba)
