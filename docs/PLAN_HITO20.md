# Plan — Hito 20: UI del juego con RmlUi

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, sección 10 (Hito 20) de `MOODENGINE_CONTEXTO_TECNICO.md`.
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Objetivo

Separar la UI del editor (Dear ImGui — herramienta interna) de la UI del juego (RmlUi — HUDs, menús, pantallas). Hasta este hito el motor solo tiene ImGui; un shooter "Wolfenstein-like" necesita un HUD persistente con barra de vida, munición, crosshair + menús de pausa / opciones / inventario, y eso pide algo con CSS-like y document model en lugar de un immediate-mode.

- **RmlUi** como librería (HTML/CSS subset + scripting opcional con Lua).
- Render del UI a un cuarto framebuffer overlay del Viewport.
- HUD demo con barra de vida + munición + crosshair durante Play Mode.
- Menú de pausa con `Esc` (Play Mode) que mostraría Resume / Opciones / Salir al menú principal — la lógica conectada vía Lua bindings (placeholders por ahora).

No-goals: editor de UI WYSIWYG, theming dinámico cambiable en runtime, animaciones complejas de UI, localización i18n.

---

## Criterios de aceptación

### Automáticos

- [ ] Compila sin warnings nuevos.
- [ ] Tests: parser RML básico (cargar un `.rml` simple devuelve un Document válido).
- [ ] Cierre limpio.

### Visuales

- [ ] HUD del juego con HP/munición visible en Play Mode. Cambia al editar una variable Lua (`hud.setHp(50)`).
- [ ] `Esc` en Play Mode pausa y muestra menú con opciones. Click en "Resume" cierra el menú; click en "Salir" vuelve a Editor Mode.
- [ ] La UI del juego no se mezcla con la del editor (Dear ImGui) — son canvases separados.

---

## Bloque 0 — Pendientes arrastrados

- [ ] **Persistencia de `AnimatorComponent`/`SkeletonComponent`** del Hito 19 (clip activo + time se pierden al guardar). Incluir en este hito si aparece feedback del dev.

## Bloque 1 — Integración de RmlUi

- [ ] Agregar RmlUi vía CPM (`mikke89/RmlUi`, último release estable). Opciones: solo Core + Debugger, sin samples, sin Lua plugin si Lua-binding lo hacemos a mano.
- [ ] Wrappear el backend de RmlUi (input, render, system interface) en `engine/ui/RmlBackend.{h,cpp}`. El renderer interno de RmlUi necesita tex+geom+scissor — implementar contra la RHI propia (`OpenGLTexture`, `OpenGLMesh` dinámico).
- [ ] Linkear a `MoodEditor`. Asegurar que arranca sin abrir ningún Document (no debe cambiar el estado actual del editor).

## Bloque 2 — UI Layer overlay del viewport

- [ ] `engine/ui/UiLayer.{h,cpp}`: dueño del `Rml::Context` + `Rml::Document` activo. Render a su propio framebuffer (RGBA8) que se compone sobre `m_viewportFb` después del post-process pass.
- [ ] Input: capturar mouse/keyboard del panel Viewport SOLO en Play Mode (en Editor Mode el input va al gizmo / edit camera). Inyectar al `Rml::Context::ProcessKeyDown`/`ProcessMouseMove`.
- [ ] Resize del context: re-dimensiona junto con el viewport panel.

## Bloque 3 — HUD demo

- [ ] `assets/ui/hud.rml`: layout HTML-like con divs `<div class="hp">100</div>`, `<div class="ammo">36</div>`, crosshair en el centro.
- [ ] `assets/ui/hud.rcss`: estilos CSS-like (color, font, position, padding).
- [ ] `Rml::DataModel`: bindea variables `hp`, `ammo` a Lua (eventualmente) o a un struct C++ `HudState`.
- [ ] Mostrar en Play Mode automáticamente; ocultar en Editor Mode.

## Bloque 4 — Menú de pausa

- [ ] `assets/ui/pause_menu.rml`: 3 botones — Resume, Opciones (TBD, popup "Próximamente"), Salir al editor.
- [ ] `Esc` en Play Mode togglea el menu_visible. Mientras está visible, el FpsCamera deja de procesar input (gameplay pausado).
- [ ] Eventos: callback `onclick` de cada botón. "Salir" emite la misma señal que el botón Stop del MenuBar.

## Bloque 5 — Lua bindings

- [ ] Exponer en LuaBindings: `hud.setHp(n)`, `hud.setAmmo(n)`, `ui.showPauseMenu(bool)`. Para que un script Lua de gameplay actualice la UI.
- [ ] Documentar el patrón en DECISIONS.md (un único Context global de RmlUi, una API agradable para mutarlo desde Lua).

## Bloque 6 — Demo + tests + cierre

- [ ] Demo "Ayuda > Cargar HUD demo" abre `hud.rml` en el UiLayer.
- [ ] `tests/test_ui.cpp`: smoke test RmlUi (cargar un RML inline → Document válido → `GetElementById` resuelve).
- [ ] Smoke test visual completo (HUD visible en Play Mode, Esc abre el menú, Resume cierra, Salir vuelve al Editor).
- [ ] Actualizar `HITOS.md`, `ESTADO_ACTUAL.md`, `DECISIONS.md`.
- [ ] Tag `v0.20.0-hito20` + push.
- [ ] Crear `PLAN_HITO21.md` (Empaquetado standalone).

---

## Pendientes futuros que pueden aparecer en este hito

- **Texto con shaping completo** (HarfBuzz/FreeType). RmlUi trae backend FreeType simple; si aparecen bugs con tipografías no-latinas, evaluar.
- **Animaciones CSS** (transitions/keyframes). Soportadas por RmlUi pero no las usamos en este hito.
- **i18n / localización**. Diferido. Trigger: cliente que pida algo más que español.
- **Editor visual de UI dentro del propio editor** (panel de árbol DOM). Trigger: si la UI crece a docenas de elementos.
- **Theming runtime con paletas de colores intercambiables** (CSS variables). Trigger: feature de "modo daltonico" o "high contrast".
