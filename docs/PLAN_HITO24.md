# Plan — Hito 24: Exposed properties Lua

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 23 cerrado).
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Objetivo

Hacer scripts Lua **reusables como componentes**. Hoy un script tiene constantes hardcoded:

```lua
local DEG_PER_SEC = 45.0
function onUpdate(self, dt)
    self.transform.rotationEuler.y = self.transform.rotationEuler.y + DEG_PER_SEC * dt
end
```

Para tener 3 entidades rotando a velocidades distintas se necesitan 3 archivos `.lua` o un solo script + un parámetro editable per-entity. Este hito agrega la segunda opción.

API Lua:

```lua
-- Declaracion: el engine guarda (name, default) y devuelve el override
-- de la entidad si lo hay.
local speed = engine.exposed("speed", 45.0)

function onUpdate(self, dt)
    self.transform.rotationEuler.y =
        self.transform.rotationEuler.y + speed * dt
end
```

Inspector renderiza un campo "speed" por entidad con el valor actual; editarlo re-corre el script con el nuevo override. Persiste en `.moodmap` per-entity.

No-goals: tipos complejos (entity refs, listas anidadas, structs). V1 soporta number / bool / string / vec3.

---

## Criterios de aceptación

### Automáticos

- [ ] Compila sin warnings nuevos.
- [ ] Tests: el binding `engine.exposed("name", default)` registra metadata + devuelve override; sin override devuelve default.
- [ ] Tests: round-trip de overrides en `.moodmap` (persiste y restaura).
- [ ] Suite total ≥ 192.

### Visuales

- [ ] Modificar `assets/scripts/rotator.lua` para usar `engine.exposed("speed", 45.0)`.
- [ ] Inspector: la sección Script muestra una sub-sección "Exposed properties" con el campo speed editable (DragFloat).
- [ ] Editar el speed cambia la rotación visiblemente sin recargar el editor.
- [ ] Cerrar/reabrir el proyecto preserva el valor editado.

---

## Bloque 1 — `engine.exposed` binding + descubrimiento

- [x] Tipos C++: `engine/scripting/ExposedProperty.{h,cpp}` con `enum class Type { Number, Bool, String, Vec3 }` + `struct Property { name, type, defaultValue (variant), currentValue (variant) }`.
- [x] `ScriptComponent` gana `std::vector<ExposedProperty> exposedProps` (descubierto al cargar) + `std::unordered_map<std::string, ExposedValue> overrides` (editable).
- [x] `LuaBindings`: registrar `engine` table con `engine.exposed(name, default)`. Implementación:
  - Si el `name` ya está en `sc.overrides`, devolver el override (convertido a sol::object).
  - Si no, guardar `(name, type, default)` en `sc.exposedProps` (deduplicado por nombre) y devolver el `default`.
  - Tipo se infiere del tipo del `default` Lua: `number/bool/string/table` (table de 3 numbers = vec3).
- [x] Tests headless: script con `engine.exposed("speed", 5.0)` registra la prop al correr; devuelve 5.0 sin override; devuelve override si está seteado.

## Bloque 2 — Inspector dinámico

- [x] `InspectorPanel`: en la sección Script, debajo del path + botón Recargar, listar `exposedProps`. Por cada prop:
  - `Type::Number` → `DragFloat`.
  - `Type::Bool` → `Checkbox`.
  - `Type::String` → `InputText`.
  - `Type::Vec3` → `DragFloat3` (o ColorEdit si el nombre contiene "color").
- [x] Editar un campo escribe en `sc.overrides[name]` y dispara `sc.loaded = false` para forzar reload del script en el próximo frame con el nuevo valor.
- [x] Botón "Reset" por prop que borra el override (vuelve al default del script).

## Bloque 3 — Aplicar overrides en runtime

- [x] `ScriptSystem`: al cargar el script, antes de `safe_script_file`, NO settear nada — los overrides los lee `engine.exposed` desde `sc.overrides`.
- [x] El valor que `engine.exposed` devuelve depende del estado de `sc.overrides[name]` en el momento de la llamada. Como la llamada está en el chunk top-level (afuera de `onUpdate`), corre cada vez que el script se re-carga.
- [x] Hot-reload por mtime sigue funcionando — los overrides persisten porque viven en el ScriptComponent, no en el sol::state.

## Bloque 4 — Persistencia en `.moodmap`

- [x] `SavedEntity` extendido con `SavedScript` opcional ({path + overrides}). Ver `EntitySerializer.h` schema.
- [x] `EntitySerializer`/`SceneLoader`: lee/escribe el map. Tipos serializables: number, bool, string, vec3. Cualquier otro se loguea con warning y se skipea.
- [x] `k_MoodmapFormatVersion` bump a 8. Back-compat: archivos viejos sin `script` cargan sin ScriptComponent (igual que antes).
- [x] **Fix arrastrado:** `SceneSerializer::save` filtraba entidades por componente (MeshRenderer/Light/RigidBody/Environment); ahora también incluye Script con path no-vacío. Sin esto, una entidad cuya razón de ser es llevar un script (rotator demo, etc.) se descartaba al guardar.

## Bloque 5 — Tests + cierre

- [x] `tests/test_exposed_properties.cpp`: round-trip de overrides + binding semantics + inferencia de tipos. 8 casos nuevos, suite total **200/5287**.
- [x] Smoke manual: rotator demo + Inspector edita el speed; el cambio se ve en vivo; guardar/cerrar/reabrir preserva valores.
- [x] Commits atómicos: ver historia (`feat(scripts)` Bloques 1-3 — `eb61844` previo a este hito; `feat(serialization)` Bloque 4 — `0dede43`; `fix(serialization)` filtro — `5c26d17`; `test(scripts)` — `b50da2a`). Polish del mapa/skybox como fix reactivo aparte (`61cc6e8`).
- [x] Tag `v0.24.0-hito24`.
- [x] Crear `docs/PLAN_HITO25.md`.
- [x] Actualizar `ESTADO_ACTUAL.md`, `HITOS.md`, `DECISIONS.md`.

---

## Decisiones tomadas

- [x] **Inferencia de tipos**: V1 infiere del Lua `type()` del default (number → Number, bool → Bool, string → String, table de 3 numbers → Vec3). Si más adelante se quiere forzar tipos (integer vs float, etc.) se agregaría `engine.exposed("name", default, "int")` con tipo explícito.
- [x] **Vec3 como tabla `{x, y, z}`**: V1 usa tabla simple `{1.0, 0.5, 0.2}` (acceso por índice) — más rápido de escribir y assimilable a JSON. Si el script lo necesita como `glm::vec3`, el binding lo convierte (los overrides en C++ usan `glm::vec3` directamente via `ExposedValue` variant).
- [x] **Set de exposed props variable entre reloads**: se redescubren cada carga (clear de `sc.exposedProps` antes del re-run del top-level). Overrides persisten en `sc.overrides` con el mismo nombre — props que ya no aparecen se ocultan del Inspector pero NO se borran (vuelven si el dev re-agrega el `engine.exposed` con el mismo nombre).
- [x] **Persistencia incluye el path del script**: el plan inicial decía sólo `script_overrides`, pero sin el path no hay a quién atarle los overrides. Decidido `script: { path, overrides }` como bloque atómico — alinea con el patrón de `mesh_renderer`/`light`/etc.

---

## Riesgos identificados

- **Reload del script al cambiar override**: forzar `sc.loaded = false` re-corre el chunk top-level. Si el script tiene side effects al cargar (escribe globals, spawnea entidades, etc.), se ejecutan otra vez. V1 ignora este edge case.
- **Inferencia de tipo para `nil` default**: `engine.exposed("foo", nil)` no tiene tipo. Reportar warning + skipear la prop.
- **Tipos no soportados**: tablas anidadas, funciones, userdata. Reportar warning + skipear.
