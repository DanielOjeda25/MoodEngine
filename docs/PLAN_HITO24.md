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

- [ ] Tipos C++: `engine/scripting/ExposedProperty.{h,cpp}` con `enum class Type { Number, Bool, String, Vec3 }` + `struct Property { name, type, defaultValue (variant), currentValue (variant) }`.
- [ ] `ScriptComponent` gana `std::vector<ExposedProperty> exposedProps` (descubierto al cargar) + `std::unordered_map<std::string, ExposedValue> overrides` (editable).
- [ ] `LuaBindings`: registrar `engine` table con `engine.exposed(name, default)`. Implementación:
  - Si el `name` ya está en `sc.overrides`, devolver el override (convertido a sol::object).
  - Si no, guardar `(name, type, default)` en `sc.exposedProps` (deduplicado por nombre) y devolver el `default`.
  - Tipo se infiere del tipo del `default` Lua: `number/bool/string/table` (table de 3 numbers = vec3).
- [ ] Tests headless: script con `engine.exposed("speed", 5.0)` registra la prop al correr; devuelve 5.0 sin override; devuelve override si está seteado.

## Bloque 2 — Inspector dinámico

- [ ] `InspectorPanel`: en la sección Script, debajo del path + botón Recargar, listar `exposedProps`. Por cada prop:
  - `Type::Number` → `DragFloat`.
  - `Type::Bool` → `Checkbox`.
  - `Type::String` → `InputText`.
  - `Type::Vec3` → `DragFloat3` (o ColorEdit si el nombre contiene "color").
- [ ] Editar un campo escribe en `sc.overrides[name]` y dispara `sc.loaded = false` para forzar reload del script en el próximo frame con el nuevo valor.
- [ ] Botón "Reset" por prop que borra el override (vuelve al default del script).

## Bloque 3 — Aplicar overrides en runtime

- [ ] `ScriptSystem`: al cargar el script, antes de `safe_script_file`, NO settear nada — los overrides los lee `engine.exposed` desde `sc.overrides`.
- [ ] El valor que `engine.exposed` devuelve depende del estado de `sc.overrides[name]` en el momento de la llamada. Como la llamada está en el chunk top-level (afuera de `onUpdate`), corre cada vez que el script se re-carga.
- [ ] Hot-reload por mtime sigue funcionando — los overrides persisten porque viven en el ScriptComponent, no en el sol::state.

## Bloque 4 — Persistencia en `.moodmap`

- [ ] `SavedEntity` extendido con `script_overrides: { name: value }` opcional.
- [ ] `EntitySerializer`/`SceneLoader`: lee/escribe el map. Tipos serializables: number, bool, string, vec3. Cualquier otro se ignora con warning.
- [ ] `k_MoodmapFormatVersion` bump a 8 (o lo que toque). Back-compat: archivos viejos sin `script_overrides` cargan con `overrides` vacío (defaults).

## Bloque 5 — Tests + cierre

- [ ] `tests/test_exposed_properties.cpp`: round-trip de overrides + binding semantics + inferencia de tipos.
- [ ] Smoke manual: `rotator.lua` modificado para usar `engine.exposed("speed", 45.0)`. Spawnear 3 rotadores, editar el speed de cada uno desde el Inspector, ver 3 velocidades distintas. Guardar, cerrar, reabrir → valores persistidos.
- [ ] Commits atómicos: `feat(scripts)`, `feat(editor)`, `test(scripts)`.
- [ ] Tag `v0.24.0-hito24`.
- [ ] Crear `docs/PLAN_HITO25.md`.
- [ ] Actualizar `ESTADO_ACTUAL.md`, `HITOS.md`, `DECISIONS.md`.

---

## Decisiones a tomar

- [ ] **Inferencia de tipos**: en V1 inferir del Lua `type()` del default. Si más adelante se quiere forzar (ej. integer vs float), agregar `engine.exposed("name", default, "int")` con tipo explícito como tercer arg.
- [ ] **Vec3 como tabla `{x, y, z}` o como `Vec3.new(x, y, z)`?** V1: tabla simple `{1.0, 0.5, 0.2}` (acceso por índice) — más rápido de escribir y assimilable a JSON. Si el script lo necesita como `glm::vec3`, el binding lo convierte.
- [ ] **¿Qué pasa si el script CAMBIA el set de exposed props entre reloads?** (dev edita el `.lua`, agrega `engine.exposed("new_prop", 0)`). El override anterior se mantiene si el nombre coincide; props que ya no aparecen se borran del Inspector pero NO del overrides map (tal vez vuelven más tarde). Aceptable para V1.

---

## Riesgos identificados

- **Reload del script al cambiar override**: forzar `sc.loaded = false` re-corre el chunk top-level. Si el script tiene side effects al cargar (escribe globals, spawnea entidades, etc.), se ejecutan otra vez. V1 ignora este edge case.
- **Inferencia de tipo para `nil` default**: `engine.exposed("foo", nil)` no tiene tipo. Reportar warning + skipear la prop.
- **Tipos no soportados**: tablas anidadas, funciones, userdata. Reportar warning + skipear.
