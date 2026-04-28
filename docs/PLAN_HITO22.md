# Plan — Hito 22: Workflow de scripting Lua

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 21 cerrado).
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Objetivo

Cerrar el gap UX que tiene hoy el motor entre "Lua corre" y "el dev escribe scripts cómodo". La infra del Hito 8 funciona: `ScriptComponent`, hot-reload por mtime, bindings (`self.transform`, `hud`, `log`), Inspector con InputText + Recargar + lastError. Pero hoy el dev:

- Tiene que escribir el path del `.lua` a mano en el Inspector (no hay browser de scripts).
- No tiene forma rápida de crear un script nuevo (alt-tab a VS Code, mkdir, edit, paste path).
- Si un script tira error, lo ve como string en el Inspector — no en su contexto en el archivo.
- No hay referencia visible de qué bindings existen (`self.tag`, `hud.setHp`, etc.).

Este hito ataca esos 4 puntos.

No-goals: debugger interactivo, autocomplete real, exposed properties (eso es Hito 23+ — ver Candidato B en versión anterior de este doc).

---

## Criterios de aceptación

### Automáticos

- [ ] Compila sin warnings nuevos.
- [ ] Tests: `AssetBrowserPanel` lista scripts; `processViewportScriptDrop` agrega `ScriptComponent` a la entidad bajo el cursor.
- [ ] Suite total >= 179 (sin regresiones).

### Visuales

- [ ] Asset Browser tiene una sección "Scripts" con los `.lua` de `assets/scripts/`.
- [ ] Drag de un script al viewport sobre una entidad la asigna como su `ScriptComponent`. Highlight amarillo durante el drag (mismo patrón del drop de Material).
- [ ] `Ayuda > Nuevo Script...` crea `assets/scripts/<nombre>.lua` con template y lo selecciona en el browser.
- [ ] Panel "Lua API" con tabs por tabla (`self`, `hud`, `log`) listando los miembros disponibles.
- [ ] (Opcional, si entra en alcance) Panel "Script Editor" con `ImGuiColorTextEdit`: editar el script seleccionado, Ctrl+S guarda y dispara hot-reload.

---

## Bloque 1 — Scripts en el Asset Browser

- [ ] `AssetBrowserPanel`: scan `assets/scripts/*.lua` igual que hoy hace con texturas/audio/meshes/prefabs/materials. Sección colapsable nueva, con el path lógico + line count.
- [ ] Drag source con payload `MOOD_SCRIPT_ASSET` (el path lógico como string).
- [ ] Refrescar la lista al mismo tiempo que el resto (post hot-reload).

## Bloque 2 — Drop de script al viewport

- [ ] `ViewportPanel::AssetDragKind::Script` + `consumeScriptDrop()`. Highlight 3D durante el drag estilo Material drop (OBB amarillo sobre la entidad bajo el cursor).
- [ ] `EditorApplication::processViewportScriptDrop()`: pickEntity → `addOrReplace<ScriptComponent>(path)`. markDirty.
- [ ] Test headless: drop simulado agrega el componente.

## Bloque 3 — "Nuevo Script" en el menú

- [ ] `Ayuda > Nuevo Script...` (o `Archivo > Nuevo Script...`, definir cuál). Abre `pfd::save_file` con default name + filtro `*.lua`.
- [ ] Si el path está dentro de `assets/scripts/`, escribir un template:
  ```lua
  -- Auto-generado por MoodEngine.
  function onUpdate(self, dt)
      -- self.transform.position.x = self.transform.position.x + dt
  end
  ```
- [ ] Refrescar Asset Browser para que el script aparezca.

## Bloque 4 — Panel "Lua API"

- [ ] Nuevo `LuaApiPanel` en `editor/panels/`. Lista hardcoded inicial (sincronizada manualmente con `LuaBindings.cpp`):
  - `self.tag` (string, read-only)
  - `self.transform.position/rotationEuler/scale` (vec3, mutable)
  - `log.info(msg)` / `log.warn(msg)`
  - `hud.setHp/setAmmo/setPaused` / `hud.getHp/getAmmo/getPaused`
- [ ] Tabs por tabla con un mini-snippet de uso por método.
- [ ] Agregado al dockspace por default.

## Bloque 5 — (stretch) Mini editor de código in-place

- [ ] Evaluar si `ImGuiColorTextEdit` (https://github.com/BalazsJako/ImGuiColorTextEdit) entra como dep CPM (license check + tamaño).
- [ ] Si entra: nuevo `ScriptEditorPanel`. Al seleccionar un script en el Asset Browser, se abre acá. Ctrl+S guarda al disco, lo cual dispara el hot-reload del ScriptSystem (sin código adicional).
- [ ] Error highlighting: la línea del último `lastError` del ScriptComponent se pinta en rojo.

**Si Bloque 5 no entra**: dejar como pendiente del Hito 23 — los 4 bloques anteriores ya entregan el grueso del workflow.

## Bloque 6 — Tests + cierre

- [ ] Tests headless del scan de scripts y del drop processor.
- [ ] Smoke manual del flujo completo: nuevo script → drag a entidad → Play Mode → verificar que corre.
- [ ] Actualizar `MOODENGINE_CONTEXTO_TECNICO.md` sección 10 con el estado del scripting workflow.
- [ ] Commits atómicos en español: `feat(editor)`, `feat(scripts)`, `test(scripts)`.
- [ ] Tag `v0.22.0-hito22`.
- [ ] Crear `docs/PLAN_HITO23.md` con candidatos (siguiente: exposed properties Lua, AI/pathfinding, save/load, networking).
- [ ] Actualizar `ESTADO_ACTUAL.md` y `HITOS.md`.

---

## Decisiones a tomar

- [ ] **¿"Nuevo Script" en `Ayuda` o en `Archivo`?** Mi voto: `Archivo` (es creación de asset). Ver consistencia con "Nuevo Proyecto" / "Guardar como prefab".
- [ ] **¿`ImGuiColorTextEdit` cabe?** Verificar antes de Bloque 5. Si no, dejar como Hito 23 sin drama.
- [ ] **¿Drop de script va a Hierarchy también o solo Viewport?** V1: solo Viewport (consistente con el resto de drops). Hierarchy queda para un hito de "drop a entidad por nombre" si aparece pedido.

---

## Riesgos identificados

- **`addOrReplace<ScriptComponent>` no existe en `Entity`**: hoy el editor solo agrega via `addComponent`. Si la entidad ya tiene `ScriptComponent`, `addComponent` puede duplicar o tirar excepción según entt config. Validar y si hace falta agregar el helper.
- **Hot-reload re-ejecuta el script**: cuando el dev guarda desde el editor, el ScriptSystem detecta el cambio de mtime y re-corre `onUpdate`. Verificar que el flow no pierde el estado del Animator/RigidBody.
- **Path absoluto vs lógico en `pfd::save_file`**: si el usuario elige un path fuera de `assets/scripts/`, el ScriptComponent no lo va a encontrar (AssetManager busca paths relativos a `assets/`). Validar y forzar dentro de `assets/scripts/` (mover el archivo si hace falta).
