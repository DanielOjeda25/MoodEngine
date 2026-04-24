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

- [x] Revisar que no haya pendientes del Hito 7 que bloqueen scripting. Ninguno: el ECS ya estaba listo y `Scene` soporta `addComponent<ScriptComponent>`.

## Bloque 1 — Dependencias Lua + sol2

- [x] CPMAddPackage para `walterschell/Lua` (wrapper CMake de Lua 5.4; Lua upstream no trae CMake nativo). Tag `v5.4.5`. Target `lua_static`.
- [x] CPMAddPackage para `ThePhD/sol2` (header-only, versión `v3.3.0`). Target `sol2::sol2`.
- [x] Linkear ambos a `MoodEditor` y a `mood_tests`.
- [x] Smoke test en `tests/test_lua.cpp`: `sol::state` carga `x = 1+2`, llama una función Lua desde C++, reporta error de sintaxis sin crashear.

## Bloque 2 — ScriptComponent + ScriptSystem

- [x] `src/engine/scene/Components.h`: agregar `ScriptComponent { std::string path; bool loaded; std::string lastError; }`. El `sol::state` NO vive en el componente (no es copiable); vive en un mapa `entt::entity -> sol::state` en `ScriptSystem`.
- [x] `src/systems/ScriptSystem.{h,cpp}`:
  - `void update(Scene&, f32 dt)`: para cada entidad con `ScriptComponent`, si no está cargada, crea `sol::state`, registra bindings y `safe_script_file`. Luego llama `onUpdate(self, dt)` si existe.
  - API expuesta a Lua vía `usertype` de sol2: `Entity`, `TransformComponent`, `log.info/warn` (ver Bloque 3 para scope).
  - Manejo de errores: si el script tira, loguea al canal `script` y desactiva (`loaded=false` + guarda el error). No spamea.
  - **Lifecycle**: `clear()` vacía el mapa `entt::entity -> sol::state`. Llamado desde `EditorApplication::rebuildSceneFromMap` (que hace `registry.clear()`) para evitar handles muertos en el ScriptSystem.

## Bloque 3 — API C++ → Lua (SCOPE ACOTADO)

> **Decisión 2026-04-23:** Bloque 3 achicado al mínimo que el demo
> `rotator.lua` requiere. `Input` y mutaciones de escena (`createEntity`/
> `destroyEntity`) se difieren al Hito 8+ o cuando aparezca un demo que
> los necesite. Evita scope creep exponiendo API que nadie usa todavia.

- [x] `src/engine/scripting/LuaBindings.{h,cpp}` centraliza los bindings.
  - `Entity`: sólo `.tag` (read-only) y `.transform` (devuelve `TransformComponent&`). Nada más.
  - `TransformComponent`: `.position`, `.rotationEuler`, `.scale` con acceso por componente `.x/.y/.z`.
  - `Log`: tabla `log` con `info(str)` y `warn(str)` → ruteados al canal `script`.
  - `self` como global, seteado por entidad al crear el `sol::state`.
  - Libs habilitadas: base + math + string. No io/os/package (sandbox razonable).
- [x] **Diferidos (con trigger explícito):**
  - `Input` bindings (`input.isKeyDown("W")` etc.). **Trigger:** primer script de gameplay que necesite teclado.
  - `scene:createEntity(name)` / `scene:destroyEntity(entity)`. **Trigger:** primer script que necesite spawn/despawn en runtime.
  - `hasComponent<T>()` y similares. **Trigger:** cuando aparezca un script con lógica condicional por componente.

## Bloque 4 — Hot-reload

- [x] `ScriptSystem` mantiene un mapa `path -> file_time_type` y compara contra `std::filesystem::last_write_time`. Throttle: un acumulador de `dt` dispara el check cada 500 ms para no spamear I/O.
- [x] Si cambió, re-`safe_script_file` sobre el mismo `sol::state` (preserva las globals del script).
- [x] Log del canal `script`: `Recargado 'assets/scripts/rotator.lua'`.
- [x] Distinción vs "Recargar" del Inspector: el botón pone `loaded=false`, lo que fuerza un `sol::state` nuevo (reset fuerte; útil ante path change).

## Bloque 5 — Inspector: editar ScriptComponent

- [x] `InspectorPanel`: sección ScriptComponent con `InputText` del path + botón "Recargar".
- [x] Editar el path invalida el state (pone `loaded=false` + limpia `lastError`): el ScriptSystem re-crea el sol::state la próxima vez.
- [x] Si hay `lastError`, se muestra en rojo con `TextWrapped`.

## Bloque 6 — Script de ejemplo + demo

- [x] `assets/scripts/rotator.lua`:
  ```lua
  function onUpdate(self, dt)
    self.transform.rotationEuler.y = self.transform.rotationEuler.y + 45 * dt
  end
  ```
- [x] Ítem de menú `Ayuda > Agregar rotador demo` (grayado sin proyecto) que dispara un request en `EditorUI::requestSpawnRotator()`. `EditorApplication` consume el request tras `ui.draw()` y crea una entidad "Rotador" con `Transform(0,4,0) + MeshRenderer(cube, gridTexture) + ScriptComponent("assets/scripts/rotator.lua")`.
- [x] **Fix reactivo**: se agregó un pase scene-driven en `renderSceneToViewport` que itera entidades con `MeshRenderer + Transform` y las dibuja. Las tiles (con tag `Tile_*`) se saltean para no overdrawear. Sin este pase, la entidad tenía `MeshRenderer` pero nadie la dibujaba (el `GridRenderer` solo conoce el grid).

## Bloque 7 — Tests

- [x] `tests/test_lua_bindings.cpp`: smoke test del ScriptSystem sin GL.
  - Script escribe `self.transform.position.x = 7.5` → update cambia el Transform.
  - Hot-reload: escribir v1, update, sleep breve, sobrescribir con v2, update con dt > 0.5 s → se detecta el mtime y el segundo update usa la nueva lógica.
  - Error de carga: script inválido → `loaded=false` y `lastError` no vacío, sin crash.
- [x] Fix de build colateral: `LUA_ENABLE_TESTING=OFF` en `CMakeLists.txt` para que el wrapper walterschell/Lua no registre `lua-testsuite` en CTest (requería `lua.exe` que desactivamos).

## Bloque 8 — Cierre

- [x] Recompilar, tests verdes (mood_tests 74/330), editor muestra el cubo rotando por script Lua.
- [x] Actualizar `docs/HITOS.md`, `docs/DECISIONS.md`, `docs/ESTADO_ACTUAL.md`.
- [x] Commits atómicos en español.
- [x] Tag `v0.8.0-hito8` + push.
- [x] Crear `docs/PLAN_HITO9.md` (audio básico con miniaudio).

---

## Qué NO hacer en el Hito 8

- **No** exponer TODA la API del motor a Lua. Solo lo mínimo para comportamiento de entidades. AssetManager / Render / file I/O quedan fuera por ahora.
- **No** convertir sistemas del motor (RenderSystem, PhysicsSystem) a Lua. El doc técnico es explícito: Lua solo para gameplay, C++ para performance.
- **No** agregar debugger de Lua. Hot-reload + `print` alcanzan para Hito 8.
- **No** integrar LuaRocks. Solo la stdlib de Lua.
- **No** cargar scripts .moddle empaquetados. Path directo a `.lua` es suficiente.

---

## Decisiones durante implementación

Entradas detalladas en `DECISIONS.md` bajo fecha 2026-04-24 (Hito 8).

- **Lua 5.4 via walterschell/Lua wrapper** en vez de integrar Lua upstream (que no trae CMake). Target `lua_static`, sin binarios ni tests del wrapper.
- **Un `sol::state` por entidad** (islas aisladas): globals de un script no cruzan entre entidades. Storage en `unordered_map<entt::entity, unique_ptr<sol::state>>` para mantener punteros estables al crecer el mapa.
- **Scope Bloque 3 achicado**: solo `Entity.tag`/`Entity.transform`, `TransformComponent`, `log.info/warn`. `Input`, `scene:createEntity`, etc. se difieren con triggers explícitos.
- **Hot-reload con throttle global 500 ms**: un solo acumulador de `dt` por sistema, no por entidad; el stat al filesystem vale poco pero es inútil hacerlo cada frame.
- **Hot-reload reutiliza el `sol::state`**: `safe_script_file` sobre el state existente preserva globals. El botón Recargar del Inspector, en cambio, hace reset fuerte (`loaded=false` → nuevo state) para cubrir el caso de cambio de path.
- **Pase scene-driven de render como fix reactivo del Bloque 6**: sin él, la entidad rotador tenía `MeshRenderer` pero nadie la dibujaba. Las tiles se filtran por prefijo de tag (`"Tile_"`). Overdraw = 0 porque el filtro las excluye. La migración "real" a render scene-driven vive en Hito 10 (assimp).

---

## Pendientes que quedan para Hito 9 o posterior

- **`Input` bindings en Lua**. **Trigger**: primer script de gameplay que necesite teclado/mouse.
- **`scene:createEntity` / `scene:destroyEntity` desde Lua**. **Trigger**: primer script que necesite spawn/despawn en runtime.
- **Acceso a más componentes desde Lua** (`MeshRenderer`, `Camera`, etc.). **Trigger**: cuando un script real lo necesite.
- **Debugger de Lua** (mobdebug o similar). **Trigger**: cuando los scripts crezcan y el debug por `print` se vuelva frustrante.
- **Persistencia de scripts en `.moodmap`**: hoy el `ScriptComponent` no se serializa (el `.moodmap` solo conoce tiles). **Trigger**: Scene authoritative (Hito 10+).
- **Prevención de leak de mtimes viejos** cuando el usuario cambia el path de un script vivo: `m_mtimes[oldPath]` queda en el mapa. Overhead despreciable hoy (3 paths ever). **Trigger**: si aparece caso de uso con cientos de paths distintos.
