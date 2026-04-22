# Roadmap de hitos — MoodEngine

Ver `MOODENGINE_CONTEXTO_TECNICO.md` sección 10 para la lista completa con detalle.

## Estado

- [x] **Hito 0** — Repositorio creado en GitHub.
- [x] **Hito 1** — Shell del editor (completado, tag `v0.1.0-hito1`).
- [x] **Hito 2** — Primer triángulo con OpenGL (completado, tag `v0.2.0-hito2`).
- [x] **Hito 3** — Cubo texturizado con cámara (completado, tag `v0.3.0-hito3`).
- [ ] Hito 4 — Mundo grid + colisiones AABB.
- [ ] Hito 5 — Asset Browser + gestión de texturas.
- [ ] Hito 6 — Serialización de proyectos y mapas.
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

- FPS counter en la status bar no se visualiza claramente en todos los layouts: investigar orden de render de ventanas ImGui no-docked sobre paneles docked. No bloqueante.
