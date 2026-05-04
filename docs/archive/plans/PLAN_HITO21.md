# Plan — Hito 21: Empaquetado standalone — **CERRADO**

> **Estado:** cerrado en tag `v0.21.0-hito21`. Todos los bloques completados según el plan original (sin cambios de scope). Resumen final en `HITOS.md` (sección Hito 21). Decisiones nuevas en `DECISIONS.md` (entradas 2026-04-28: `mood_engine_lib`, layout del paquete, V1 copia assets enteros).
>
> El cuerpo del plan abajo se conserva como histórico.

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 20 cerrado).
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Objetivo

Producir un **build distribuible del juego** ejecutable por usuarios finales sin Visual Studio ni nada del editor. Hasta el Hito 20 todo el proyecto vive como `MoodEditor.exe`: el editor abre proyectos, edita mapas, lanza Play Mode embebido. Un usuario que solo quiere *jugar* un proyecto MoodEngine no debería necesitar instalar nada.

Output esperado: una carpeta autocontenida (`<NombreJuego>/`) con un `.exe` runtime + `.dll`s + `assets/` del proyecto seleccionado, ejecutable por doble-click en cualquier Windows 10/11 sin VS.

- **Target nuevo `MoodPlayer`** (separado de `MoodEditor`): solo carga el proyecto, hace render + scripts + audio + input. Sin paneles ImGui, sin gizmos, sin Hierarchy/Inspector. Arranca en Play Mode directo.
- **Acción "Empaquetar proyecto"** en el menú Archivo del editor: copia `MoodPlayer.exe` + DLLs + `assets/` (filtrado) a una carpeta destino, listo para zippear y compartir.
- **`game.json`** en la raíz del paquete: indica qué `.moodmap` cargar al arranque.

No-goals: instalador (`.msi`/`.exe` de Inno Setup), code signing, auto-updater, multi-plataforma (Linux/Mac), DRM, splash screen animado, launcher.

---

## Criterios de aceptación

### Automáticos

- [ ] `MoodPlayer.exe` compila sin warnings nuevos.
- [ ] Tests: el `PackageBuilder` produce un manifest correcto dado un proyecto con N mapas + M assets.
- [ ] CI build sigue verde.

### Visuales

- [ ] Desde el editor: `Archivo > Empaquetar proyecto...` abre file dialog para elegir carpeta destino. Tras unos segundos crea `<destino>/<nombre_proyecto>/MoodPlayer.exe` + assets.
- [ ] Doble-click en `MoodPlayer.exe` empaquetado abre una ventana sin chrome de editor que muestra el mapa default del proyecto en Play Mode directo.
- [ ] `Esc` en `MoodPlayer` cierra la app (no hay editor al que volver).

---

## Bloque 0 — Pendientes arrastrados del Hito 20

- [ ] Botón "Opciones" del menú de pausa: hoy solo loguea. Si se quiere abordar, definir minimum viable: volume slider + checkbox fullscreen.
- [ ] Persistencia del Animator (clip activo + time) en `.moodmap`. Si aparece feedback del dev al usar el packager con un mapa con personajes animados.

## Bloque 1 — Target `MoodPlayer`

- [ ] Crear `src/player/PlayerApplication.{h,cpp}`: variante slim de `EditorApplication` sin paneles. Reusa `engine/`, `systems/`, `core/`, `platform/`, `engine/scripting/`, `engine/game/`, `engine/render/`, `EditorRenderPass` (renombrar/extraer si hace falta para no depender de "Editor").
- [ ] Punto de entrada `src/player/main.cpp`: lee `game.json` adyacente al exe, abre el proyecto + mapa default, arranca el loop en Play Mode.
- [ ] CMakeLists: nuevo `add_executable(MoodPlayer ...)` con su propia lista de fuentes (subset de las del editor). NO debe linkear ImGui (excepto si reusamos ImDrawList para el HUD; en ese caso linkear igual pero sin paneles).
- [ ] Fuentes compartidas extraídas a una `mood_engine_lib` (static lib): el editor y el player linkean ambas contra ella. Hace falta renombrar/refactor.

## Bloque 2 — Backend `EditorRenderPass` reutilizable

- [ ] El render del frame está en `EditorRenderPass.cpp` y usa `EditorApplication` como dueño del estado. Extraer la rutina pura `renderFrame(scene, cameras, framebuffers, light_grid, ...)` a `systems/RenderPass.cpp` (sin "Editor" en el nombre).
- [ ] El editor pasa a llamar `renderFrame(...)` con sus camaras + ambos FBs. El player hace lo mismo con la `FpsCamera`.

## Bloque 3 — Loop minimal del player

- [ ] `PlayerApplication::run()`: SDL + GL + ImGui-no-paneles + Scene + Scripts + Audio + Physics + Animation + Render. Frame por frame igual que el editor pero sin `ImGui::Begin`/paneles. La UI in-game (HUD + pausa de Hito 20) sí se mantiene — vive en `EditorPlayMode.cpp::drawGameOverlay`, también extraer para reuso.
- [ ] `Esc`: en player no togglea pausa-y-vuelve-al-editor sino que muestra el menú de pausa. Si el script forza pausa via `hud.setPaused(true)`, mismo menú. Botón "Salir al editor" se renombra a "Salir al menú principal" o "Salir del juego" — TBD por feedback.

## Bloque 4 — `game.json` + carga del proyecto

- [ ] Schema mínimo:
  ```json
  {
    "version": 1,
    "name": "Mi juego",
    "project": "Project.moodproj",
    "default_map": "maps/level1.moodmap"
  }
  ```
- [ ] `PlayerApplication` lee `game.json` con `<exe_dir>/game.json`. Falla con un MessageBox amigable si falta o es inválido.
- [ ] El path del proyecto + mapas es relativo al directorio del `.exe`.

## Bloque 5 — Acción "Empaquetar proyecto" en el editor

- [ ] `engine/packaging/PackageBuilder.{h,cpp}`: dado un `Project` y un `dest_dir`, copia:
  - `MoodPlayer.exe` (ubicado junto a `MoodEditor.exe` post-build, o resuelto desde `CMAKE_RUNTIME_OUTPUT_DIRECTORY`).
  - DLLs runtime (`SDL2.dll`, etc.) — la lista la define el CMakeLists post-build del player.
  - El `.moodproj` + todos los `.moodmap` referenciados.
  - `assets/` filtrado: solo lo que aparezca como path en algún componente serializado o en algún script Lua. Si es muy complejo en V1, copiar el `assets/` entero (más simple, peor en tamaño).
  - Genera `game.json` con name + project path + default_map.
- [ ] `MenuBar`: nuevo item `Archivo > Empaquetar proyecto...` (gris si no hay proyecto). Abre `pfd::select_folder`. Loguea progreso. MessageBox al terminar con éxito o error.
- [ ] El editor NO debe poder empacar el `.moodproj` que tenga cambios sin guardar (gris si dirty, o pedir guardar primero).

## Bloque 6 — Tests

- [ ] `tests/test_package_builder.cpp`: dado un `Project` mock con 2 mapas + N assets, `PackageBuilder` produce el árbol esperado en un dir temp + un `game.json` válido. No requiere lanzar el player.
- [ ] (Opcional) Smoke test manual del `MoodPlayer.exe` empaquetado: un script PowerShell que copia un proyecto pequeño, lanza el .exe, captura el primer frame, verifica que termina con exit code 0 al recibir Esc.

## Bloque 7 — Cierre

- [ ] Smoke manual del dev: empaquetar el proyecto `test/` actual y enviarlo a otra máquina (o `cmd /c` desde otra ruta). Doble-click → arranca en Play Mode.
- [ ] Actualizar `MOODENGINE_CONTEXTO_TECNICO.md` sección 10 con el estado del packager.
- [ ] Commits atómicos en español: `feat(player)`, `feat(packaging)`, `refactor(render)`, `test(packaging)`.
- [ ] Tag `v0.21.0-hito21`.
- [ ] Crear `docs/PLAN_HITO22.md` (siguiente hito a definir — candidatos: GUI editor de mapas más rico, networking, AI/pathfinding, save/load de gameplay).
- [ ] Actualizar `ESTADO_ACTUAL.md` y `HITOS.md`.

---

## Decisiones a tomar a lo largo del hito

- [ ] **¿Una `mood_engine_lib` o sources duplicados?** static lib es más limpio pero exige que los headers estén bien expuestos (sin filtrar `EditorApplication.h` por ejemplo). Probable: lib.
- [ ] **¿El menú de pausa del player muestra "Salir del juego" o "Volver al menú"?** Sin un menú principal estandar, primer pase: "Salir del juego" → cierra la app.
- [ ] **¿Qué assets copia el packager?** "Todo `assets/`" (simple, ~MB de más) vs "solo referenciados" (preciso, requiere walk de scripts Lua y serializadores). V1: todo.
- [ ] **¿`MoodPlayer.exe` es console o windowed?** Probable windowed (sin terminal asomando). Necesita `/SUBSYSTEM:WINDOWS` en MSVC.
- [ ] **¿Qué hace el player si encuentra un script con error?** El editor muestra error en Inspector + canal log. El player no tiene Inspector — solo log a archivo (al menos) + opcionalmente un overlay rojo de error.

---

## Riesgos identificados

- **Refactor del render**: `EditorRenderPass.cpp` está acoplado a `EditorApplication`. Extraerlo puede tocar muchas líneas; cuidar de no romper el editor.
- **Empaquetado de DLLs**: la lista de DLLs depende del compilador y la config (Debug vs Release). El SDL2 debug es `SDL2d.dll`, release es `SDL2.dll`. El packager debe usar el `.exe` Release del player, no el Debug.
- **Path relativos del proyecto**: el `.moodproj` actualmente guarda paths absolutos (TBD verificar). Si es así, el packager debe reescribirlos como relativos al `game.json`.
- **Animaciones se pierden al re-cargar**: pendiente arrastrado del Hito 19. Si el dev empaqueta un proyecto con un personaje animado y al abrir el player el clip arranca de cero, ya tenemos ese feedback documentado. Decidir si solucionarlo en este hito o seguir difiriendo.
