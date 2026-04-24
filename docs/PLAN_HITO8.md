# Plan — Hito 8: Scripting con Lua

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, sección 4.16 de `MOODENGINE_CONTEXTO_TECNICO.md` (scripting con Lua 5.4 + sol2).
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Objetivo

Permitir que **entidades tengan comportamiento escrito en Lua**, no en C++. Meta del hito:
- Una entidad con `ScriptComponent{ path = "scripts/rotador.lua" }` carga el archivo, expone `self` (la entidad) + API del motor, y llama `onUpdate(dt)` cada frame.
- Hot-reload: al detectar que el `.lua` cambió en disco, se reempaqueta el script sin reiniciar el editor.
- API mínima accesible desde Lua:
  - `Transform` del `self` (read/write).
  - `Input` (consultar teclas / mouse).
  - `Log` (nivel info/warn).
  - `scene:createEntity(name)` / `scene:destroyEntity(entity)`.
- Ejemplo funcional: un cubo que rota en Y con un script Lua de 3 líneas.

---

## Bloque 0 — Pendientes arrastrados

- [ ] Revisar que no haya pendientes del Hito 7 que bloqueen scripting. En principio ninguno: el ECS ya está listo y `Scene` soporta `addComponent<ScriptComponent>`.

## Bloque 1 — Dependencias Lua + sol2

- [ ] CPMAddPackage para `walterschell/Lua` (wrapper CMake de Lua 5.4; Lua upstream no trae CMake nativo).
- [ ] CPMAddPackage para `ThePhD/sol2` (header-only, versión `v3.3.0`).
- [ ] Linkear ambos a `MoodEditor`.
- [ ] Compilar un smoke test: `sol::state` + cargar `"print('hola')"` para confirmar que engancha.

## Bloque 2 — ScriptComponent + ScriptSystem

- [ ] `src/engine/scene/Components.h`: agregar `ScriptComponent { std::string path; bool loaded = false; }`. El `sol::state` NO vive en el componente (no es copiable); vive en un mapa `entt::entity -> sol::state` en `ScriptSystem`.
- [ ] `src/systems/ScriptSystem.{h,cpp}`:
  - `void update(Scene&, float dt)`: para cada entidad con `ScriptComponent`, si no está cargada, leer el archivo y `sol::state::script_file(...)`. Luego llamar `onUpdate(self, dt)` si existe en el script.
  - API expuesta a Lua vía `usertype` de sol2: `Entity`, `TransformComponent`, `Log` (ver Bloque 3 para scope).
  - Manejo de errores: si el script tira, logear al canal `script` y desactivar ese script (pone `loaded=false` + guardar el error para no spamear).
  - **Lifecycle**: exponer `clear()` que vacía el mapa `entt::entity -> sol::state`. Llamarlo desde `EditorApplication::rebuildSceneFromMap` (que hace `registry.clear()`) para evitar handles muertos en el ScriptSystem.

## Bloque 3 — API C++ → Lua (SCOPE ACOTADO)

> **Decisión 2026-04-23:** Bloque 3 achicado al mínimo que el demo
> `rotator.lua` requiere. `Input` y mutaciones de escena (`createEntity`/
> `destroyEntity`) se difieren al Hito 8+ o cuando aparezca un demo que
> los necesite. Evita scope creep exponiendo API que nadie usa todavia.

- [ ] `src/engine/scripting/LuaBindings.{h,cpp}` centraliza los bindings.
  - `Entity`: sólo `.tag` (read-only) y `.transform` (devuelve `TransformComponent&`). Nada más.
  - `TransformComponent`: `.position`, `.rotationEuler`, `.scale` con acceso por componente `.x/.y/.z`.
  - `Log`: funciones libres `log.info(str)` y `log.warn(str)`.
- [ ] **Diferidos (con trigger explícito):**
  - `Input` bindings (`input.isKeyDown("W")` etc.). **Trigger:** primer script de gameplay que necesite teclado.
  - `scene:createEntity(name)` / `scene:destroyEntity(entity)`. **Trigger:** primer script que necesite spawn/despawn en runtime.
  - `hasComponent<T>()` y similares. **Trigger:** cuando aparezca un script con lógica condicional por componente.

## Bloque 4 — Hot-reload

- [ ] `ScriptSystem` mantiene un mapa `path -> file_time_type` y compara contra `std::filesystem::last_write_time` cada cierto tiempo (throttling cada 500 ms para no spamear I/O).
- [ ] Si cambió, re-script_file sobre el mismo `sol::state` (preserva el estado del script).
- [ ] Log: `script: recargado rotador.lua`.

## Bloque 5 — Inspector: editar ScriptComponent

- [ ] `InspectorPanel`: agregar sección ScriptComponent con `InputText` del path + botón "Recargar".
- [ ] Si el archivo no existe o tiró error la última vez, mostrar el error en rojo.

## Bloque 6 — Script de ejemplo + demo

- [ ] `assets/scripts/rotator.lua`:
  ```lua
  function onUpdate(self, dt)
    self.transform.rotationEuler.y = self.transform.rotationEuler.y + 45 * dt
  end
  ```
- [ ] Modificar `buildInitialTestMap` o agregar un botón "Agregar rotador" que cree una entidad suelta con `ScriptComponent{rotator.lua}`. Verificar que rota en el Viewport.

## Bloque 7 — Tests

- [ ] `tests/test_lua_bindings.cpp`: smoke test del ScriptSystem sin GL:
  - Crear Scene + Entity + ScriptComponent.
  - Script inline que modifica el Transform.
  - Correr system.update una vez y verificar que el Transform cambió.
- [ ] Test de hot-reload con un archivo temporal: escribir v1, cargar, escribir v2, re-update, verificar que se aplicó la nueva lógica.

## Bloque 8 — Cierre

- [ ] Recompilar, tests verdes, editor demuestra el cubo rotando por script Lua.
- [ ] Actualizar `docs/HITOS.md`, `docs/DECISIONS.md`, `docs/ESTADO_ACTUAL.md`.
- [ ] Commits atómicos en español.
- [ ] Tag `v0.8.0-hito8` + push.
- [ ] Crear `docs/PLAN_HITO9.md` (audio básico con miniaudio).

---

## Qué NO hacer en el Hito 8

- **No** exponer TODA la API del motor a Lua. Solo lo mínimo para comportamiento de entidades. AssetManager / Render / file I/O quedan fuera por ahora.
- **No** convertir sistemas del motor (RenderSystem, PhysicsSystem) a Lua. El doc técnico es explícito: Lua solo para gameplay, C++ para performance.
- **No** agregar debugger de Lua. Hot-reload + `print` alcanzan para Hito 8.
- **No** integrar LuaRocks. Solo la stdlib de Lua.
- **No** cargar scripts .moddle empaquetados. Path directo a `.lua` es suficiente.

---

## Decisiones durante implementación

_(llenar a medida que aparezcan)_

---

## Pendientes que quedan para Hito 9 o posterior

_(llenar al cerrar el hito)_
