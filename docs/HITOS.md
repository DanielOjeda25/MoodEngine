# Roadmap de hitos — MoodEngine

Ver `MOODENGINE_CONTEXTO_TECNICO.md` sección 10 para la lista completa con detalle.

## Estado

- [x] **Hito 0** — Repositorio creado en GitHub.
- [x] **Hito 1** — Shell del editor (completado, tag `v0.1.0-hito1`).
- [x] **Hito 2** — Primer triángulo con OpenGL (completado, tag `v0.2.0-hito2`).
- [x] **Hito 3** — Cubo texturizado con cámara (completado, tag `v0.3.0-hito3`).
- [x] **Hito 4** — Mundo grid + colisiones AABB (completado, tag `v0.4.0-hito4`).
- [x] **Hito 5** — Asset Browser + gestión de texturas (completado, tag `v0.5.0-hito5`).
- [x] **Hito 6** — Serialización de proyectos y mapas (completado, tag `v0.6.0-hito6`).
- [ ] Hito 7 — Entidades, componentes, jerarquía.
- [ ] Hito 8 — Scripting con Lua.
- [ ] Hito 9 — Audio básico.
- [ ] Hito 10 — Importación de modelos 3D.
- [ ] Hito 11 — Iluminación Phong/Blinn-Phong.
- [ ] Hito 12 — Física con Jolt.
- [ ] Hito 13 — Gizmos y selección.
- [ ] Hito 14 — Prefabs.
- [ ] Hito 15 — Skybox, fog, post-procesado.
- [ ] Hito 16 — Shadow mapping.
- [ ] Hito 17 — PBR.
- [ ] Hito 18 — Deferred / Forward+.
- [ ] Hito 19 — Animación esquelética.
- [ ] Hito 20 — UI del juego con RmlUi.
- [ ] Hito 21 — Empaquetado standalone.
- [ ] Hito 22 — Undo/Redo.
- [ ] Hito 23 — Editor de materiales.
- [ ] Hito 24 — Particle system.
- [ ] Hito 25 — Linux.

## Hito 1 — Shell del editor

**Objetivo:** ventana SDL2 con contexto OpenGL 4.5 + Dear ImGui docking + logging + estructura completa de carpetas.

**Criterios de aceptación:** ver `MOODENGINE_CONTEXTO_TECNICO.md` sección 11.2.

**Siguiente paso tras completarlo:** Hito 2 — inicializar GLAD, escribir un shader básico, dibujar un triángulo al viewport (render a textura que ImGui muestra en el panel Viewport).

### Pendientes menores detectados en Hito 1

- ~~Solapamiento visual entre paneles al arrancar (layout inicial del dockspace).~~ Resuelto en Hito 2 con `DockBuilder` (`Dockspace.cpp`), respeta `imgui.ini` si existe.

## Hito 2 — Primer triángulo con OpenGL

**Objetivo:** dejar de ser shell vacío y dibujar geometría propia (triángulo RGB) en el panel Viewport, con pipeline de render real: GLAD + RHI abstracto + backend OpenGL + shaders propios + framebuffer offscreen.

**Criterios de aceptación:** ver `PLAN_HITO2.md` (todos cumplidos: automáticos + visuales + resize del panel + default layout sin superposiciones).

**Siguiente paso tras completarlo:** Hito 3 — cubo texturizado con cámara. Agregar stb_image (textura), matrices de modelo/vista/proyección con GLM, cámara FPS controlable con WASD + ratón en Play Mode, cámara orbital en Editor Mode. Empezar a usar doctest para testear matemática.

## Hito 3 — Cubo texturizado con cámara

**Objetivo:** pipeline 3D completo: matrices MVP, texturas con stb_image, dos cámaras (orbital / FPS) con entradas reales, modos Editor/Play, y primer target de tests con doctest.

**Criterios de aceptación cumplidos:** cubo rotando con textura en Viewport, resize sin distorsión, toggle Play/Stop con Esc para salir, botón verde/rojo en menu bar, tests pasando (10 casos, 37 asserciones).

**Siguiente paso tras completarlo:** Hito 4 — Mundo grid + colisiones AABB. Mapa definido como grid 2D renderizado como cubos 3D (modo "Wolfenstein simulado"), colisiones AABB propias para que el jugador no atraviese paredes, debug rendering básico (líneas, cajas).

### Pendientes menores detectados en Hito 3

- ~~FPS counter en la status bar no se visualiza claramente en todos los layouts.~~ Resuelto 2026-04-23: `StatusBar` usa `ImGui::BeginViewportSideBar(ImGuiDir_Down)` y se dibuja antes del dockspace para que el viewport recorte su work area antes de que los paneles docked lo consuman.

## Hito 4 — Mundo grid + colisiones AABB

**Objetivo:** primer "mundo" jugable. Mapa definido como grid 2D de tiles renderizado como cubos 3D, colisiones AABB propias que impiden atravesar paredes (con slide en esquinas), y debug rendering para visualizar las AABBs.

**Criterios de aceptación cumplidos:**
- Viewport muestra sala 8×8 con bordes sólidos y columna central (29 paredes).
- En Play Mode, WASD mueve y no atraviesa paredes; slide diagonal funciona.
- Tecla `F1` togglea debug draw (AABBs de tiles en amarillo + AABB del jugador en verde en Play).
- Log del canal `world`: `Mapa cargado: prueba_8x8 (29 tiles solidos)`.
- Cierre limpio, exit 0. Suite de tests: 30 casos, 159 asserciones.

**Siguiente paso tras completarlo:** Hito 5 — Asset Browser + gestión de texturas. AssetManager, VFS inicial, texturas por tile desde el editor, textura fallback rosa-negro, consola in-game. Antes de empezar, aplicar la convención de escala realista postergada del Hito 4 (ver `docs/PLAN_HITO5.md`).

### Pendientes menores detectados en Hito 4

- ~~**Convención de escala del mundo** diferida~~: resuelto en Hito 5 Bloque 0 (ver `DECISIONS.md` 2026-04-23). Ahora 1 unidad = 1 m SI, `tileSize=3 m`, player 0.6×1.8×0.6, velocidad 3 m/s, orbit radius 30.
- **`GridRenderer` inline en EditorApplication** — extraer cuando aparezca la segunda fuente de geometría (texturas por tile o meshes de archivo). Sigue pendiente post-Hito 5; ver `PLAN_HITO5.md` sección pendientes.
- **Face culling (`GL_CULL_FACE`)** no activado — evaluar cuando haya muchas mallas opacas.

## Hito 5 — Asset Browser + gestión de texturas

**Objetivo:** pasar de "motor que carga una textura hardcoded" a motor con catálogo de assets consultable desde el editor. AssetManager con cache, VFS sandbox, textura fallback, texturas por tile, Asset Browser con miniaturas reales, Console in-game.

**Criterios de aceptación cumplidos:**
- Convención de escala 1 unit = 1 m SI adoptada (Bloque 0, arrastrado del Hito 4).
- Textura `missing.png` (chequered rosa/negro) cargada en slot 0 del `AssetManager`; cualquier fallo de carga cae ahí.
- `VFS` rechaza `..`, `.`, paths absolutos y leading `/`/`\`; `AssetManager` lo usa para resolver paths lógicos.
- `AssetBrowserPanel` muestra miniaturas reales de los PNG de `assets/textures/` con click-to-select.
- Paredes del mapa renderizadas con 2 texturas distintas (grid.png en borde, brick.png en columna central).
- `ConsolePanel` acoplado al inferior-derecho, muestra todos los logs del `LogRingSink` coloreados por nivel con filtro por canal.
- Tests: suite 35/179 (+5 para VFS).

**Siguiente paso tras completarlo:** Hito 6 — Serialización de proyectos y mapas (`.moodproj` + `.moodmap`). Antes, el drag & drop desde Asset Browser a tile del viewport y el refactor `GridRenderer`, ambos arrastrados del Hito 5.

### Pendientes del Hito 5 (todos resueltos o con condición de disparo clara)

- ~~Tests de `AssetManager`.~~ Resueltos en Hito 6 Bloque 0 (factory inyectable + `MockTexture`, 7 casos).
- ~~Drag & drop Asset Browser → tile del viewport.~~ Resuelto en Hito 6 Bloque 0 (`ViewportPick` + `BeginDragDropTarget`).
- ~~Extraer `GridRenderer` de `EditorApplication`.~~ Resuelto en Hito 6 Bloque 0 (`src/systems/GridRenderer.{h,cpp}`).
- ~~Menú "Restablecer layout".~~ Resuelto en Hito 6 Bloque 0 (`Ver > Restablecer layout`).
- ~~Color configurable del AABB del jugador en debug draw.~~ El objetivo real (distinguir visualmente player de tiles) ya está cumplido: tiles amarillos, player verde. "Configurable vía UI" no tiene caso de uso sin panel de Settings; si se quiere cambiar, se edita la constante en `EditorApplication::renderSceneToViewport`. Cerrado.
- ~~Hot-reload del `AssetBrowserPanel`.~~ Resuelto post-cierre de Hito 6: `AssetManager::reloadChanged()` compara mtime y re-invoca la factory. El botón "Recargar" dispara el reupload al GPU entre frames (safe: no destruye handles mid-draw).
- **Tracy profiler**: no es un pendiente olvidado, es una **integración planificada con condición de disparo explícita**. El doc técnico sección 9.3 y 9.5 establece "no pre-optimizar, introducir complejidad sólo cuando aparece el problema concreto". Tracy entra cuando los frames se vuelvan medibles como pesados — objetivos concretos: escenas complejas del Hito 11 (iluminación) o Hito 17 (PBR). Hasta entonces no aporta valor.

## Hito 6 — Serialización de proyectos y mapas

**Objetivo:** persistir el estado del editor en disco (`.moodproj` + `.moodmap`) con JSON versionado. Ciclo completo desde el menú Archivo: nuevo, abrir, guardar, cerrar.

**Criterios de aceptación cumplidos:**
- `Archivo > Nuevo Proyecto` abre diálogo nativo (portable-file-dialogs), crea estructura `<root>/maps/`, `<root>/textures/`, `<root>/<name>.moodproj` y guarda el estado actual del editor como `maps/default.moodmap`.
- `Archivo > Abrir Proyecto` filtra `*.moodproj` y carga el mapa default.
- `Archivo > Guardar` (Ctrl+S) escribe ambos archivos; grayado si no hay proyecto.
- `Archivo > Cerrar proyecto` vuelve al mapa de prueba hardcodeado.
- Título dinámico `MoodEngine Editor - <name>` con `*` si hay cambios sin guardar.
- Versionado: `k_MoodmapFormatVersion = 1`, `k_MoodprojFormatVersion = 1`. Cargar versión futura falla con mensaje claro.
- Tests: 61 casos / 281 asserciones (+6 JSON helpers, +6 SceneSerializer, +7 ProjectSerializer, +7 AssetManager con factory mock).
- Bonus (Bloque 0): `GridRenderer` extraído, `ViewportPick` + hover cyan, drag & drop Asset Browser→tile, pan estilo Blender (middle-drag), `Ver > Restablecer layout`, debug lines 2 px, tests AssetManager.

**Siguiente paso tras completarlo:** Hito 7 — Entidades, componentes, jerarquía. Integrar EnTT detrás de una fachada (`Entity`, `Scene`), panel Hierarchy (árbol de entidades), Inspector funcional editando componentes. Componentes básicos: `Transform`, `MeshRenderer`, `Camera`, `Light`.

### Pendientes menores detectados en Hito 6

Ver `docs/PLAN_HITO6.md` sección "Pendientes que quedan para Hito 7 o posterior":
- Bloque 5 (editor state persistence) diferido — polish UX, no bloqueante.
- "Guardar como" completo (hoy duplica estructura via Nuevo Proyecto).
- Diálogo de confirmación para descartar cambios sin guardar (Cerrar/Abrir/Nuevo).
- UI para múltiples mapas por proyecto (el schema ya los soporta).
- Hot-reload de `.moodmap` editado externamente.
