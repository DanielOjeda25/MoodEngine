# Plan — F2H22: Reorganización UX del editor (workspaces orientados a tareas)

> **Leer primero:** `ESTADO_ACTUAL.md` (F2H21 cerrado), `PENDIENTES.md`
> entrada *"Pase de polish UX general del editor"* (charlado tras F2H21:
> *"a futuro deberemos mejorar toda la UI para hacerla más fácil de
> entender"*). El dev al validar F2H21 confirmó que la decisión de
> priorizar este hito antes del 4-viewport Hammer es correcta:
> **"Va, mientras quede bien ordenado"**.

---

## Decisión de scope

El plan F2 original no tiene un hito explícito para "rework UX
workflow del editor" — está implícito en sub-fase 2.7 (UI/UX final del
editor, F2H41-F2H44, ~6+ meses). El feedback del dev tras F2H21 indica
que **el orden actual de los workspaces y panels confunde el flow de
trabajo cotidiano**, lo que justifica adelantar parte del scope.

F2H22 ataca **3 fricciones específicas** identificadas en uso:

1. **Nombres de workspaces no comunican qué hacer ahí**
   (`Layout / Scripting / Profile / Materials`). El dev no sabe a qué
   workspace ir para una tarea.
2. **Todos los panels están dockeados en todos los workspaces**, solo
   cambia *dónde*. El dev abre `Modelar` y ve `MaterialEditor`,
   `ScriptEditor`, `Performance HUD`, etc. — ruido visual.
3. **Tools de edición están solo en menús** (Mover/Rotar/Escalar via
   teclas W/E/R no descubribles, primitivas de Brush bajo `Brush > Añadir`,
   Face mode bajo tecla 3 reservada). No hay toolbar visible.

F2H22 NO toca:
- Estética (colores / fonts / theme — lo dijiste explícito).
- Refactor del WorkspaceManager / Dockspace / EditorUI core.
- Onboarding visual / tour interactivo.
- 4-viewport Hammer (movido a F2H23).
- Node graph del Material Editor (movido a F2H24+).

## Filosofía de diseño

- **Workspaces = tareas, no categorías**: cada workspace tiene un nombre
  que describe **qué hacés ahí**, no qué contenido muestra.
- **Visibilidad por workspace**: cada workspace solo deja visibles los
  panels que su tarea necesita. El resto sigue accesible vía menú **Ver**
  (toggle), pero NO se renderean por default. Reduce el ruido visual.
- **Toolbar lateral con iconos** en el viewport: descubribilidad de tools
  (Move/Rotate/Scale + primitivas Brush + Face mode toggle).
  Saca lo crítico del menú al ojo.
- **Back-compat estricto**: `.moodproj` viejos con nombres `Layout` /
  `Scripting` / `Profile` / `Materials` migran al cargar a
  `Modelar` / `Programar` / `Optimizar` / `Materiales` preservando el
  `iniLayout` custom del dev. Sin pérdida de configuración.

## Decisiones arquitectónicas

| # | Decisión | Valor |
|---|---|---|
| 1 | Renames | `Layout` → `Modelar`, `Scripting` → `Programar`, `Profile` → `Optimizar`, `Materials` → `Materiales`. |
| 2 | Visibility default por workspace | `Modelar`: Viewport, Hierarchy, Inspector, AssetBrowser. `Programar`: ScriptEditor, Console, LuaApi, Hierarchy, Inspector, Viewport (chico). `Optimizar`: Viewport, Performance, Console. `Materiales`: MaterialEditor, AssetBrowser, Viewport, Inspector. |
| 3 | Persistencia | El `iniLayout` del workspace activo se sigue capturando en cada switch. Si el dev abre un panel "extra" en `Modelar` (ej. Console), persiste en SU `iniLayout` para ese workspace. La visibility default solo aplica al **primer** activación o al pedir reset. |
| 4 | Toolbar lateral | Vertical, anclada al borde izquierdo del Viewport. Iconos: `T` (translate), `R` (rotate), `S` (scale), `B+box` (add box brush), `B+cyl` (cylinder), `M` (face mode toggle). Iconos = letras grandes con tooltip (sin font icons todavía). |
| 5 | Back-compat | `WorkspaceManager::setWorkspaces` mapea nombres viejos → nuevos al deserializar. Si el `.moodproj` ya tiene los nuevos nombres, no toca. Si tiene una mezcla, deja los nuevos como están y solo migra los viejos. |
| 6 | Tests | Tests del rename/migración via `setWorkspaces` con nombres viejos. La toolbar no es testeable sin GL — validación visual. |
| 7 | Scope explícitamente fuera | 4-viewport Hammer (F2H23), node graph (F2H24+), iconos image-based (font icons + iconos custom — diferido). |

## Bloques

### A — Plan F2H22 (este archivo)

Cierra al commit inicial.

### B — Renames + back-compat

`src/editor/workspace/WorkspaceManager.cpp`:
- `defaultWorkspaces()` devuelve los 4 con nombres nuevos.
- `setWorkspaces()` recibe lista del `.moodproj` y mapea nombres viejos →
  nuevos antes de aceptar:
  - `Layout` → `Modelar`
  - `Scripting` → `Programar`
  - `Profile` → `Optimizar`
  - `Materials` → `Materiales`

`src/editor/ui/Dockspace.cpp`:
- `buildLayoutForWorkspace` switch case usa nombres nuevos. Acepta
  también los viejos como alias (defensivo si la migración falla por
  algo).

### C — Visibility default por workspace

Nueva función `applyDefaultVisibilityForWorkspace(name)` en `EditorUI` que
setea `panel->visible` para cada panel del manager según la tarea:

```cpp
void EditorUI::applyDefaultVisibilityForWorkspace(const std::string& name) {
    // Mapa nombre → vector<panelName visible>
    // Modelar: ["Viewport", "Hierarchy", "Inspector", "Asset Browser"]
    // Programar: ["Script Editor", "Console", "Lua API", "Hierarchy", "Inspector", "Viewport"]
    // Optimizar: ["Viewport", "Performance", "Console"]
    // Materiales: ["Material Editor", "Asset Browser", "Viewport", "Inspector"]
    // Resto = false.
}
```

Llamar desde `applyPendingWorkspaceSwitch` solo cuando `nextWs.iniLayout`
está vacío (primer activación) o tras `requestResetToDefault`.

Layouts en `Dockspace.cpp` ajustados: cada `buildXxxWorkspace` ya NO
dockea **todos** los panels — solo los visibles del workspace. Los demás
quedan flotantes (ImGui los esconde si `visible=false`).

### D — Toolbar lateral con iconos

Nuevo componente `editor/ui/Toolbar.{h,cpp}` (~150 LOC):
- `Toolbar::draw(EditorUI& ui)` dibuja un panel `ImGuiWindowFlags_NoTitleBar`
  + `NoMove` + `NoResize` anclado a la izquierda del Viewport (relativo
  al imgui.ini del workspace, no posición fija en pantalla).
- 6 botones cuadrados 32×32:
  - `T` translate → setea `m_gizmoMode = Translate` en `EditorUI` (state
    actual de gizmos del F2H13).
  - `R` rotate → `Rotate`.
  - `S` scale → `Scale`.
  - Separator.
  - `□` Add Box Brush → emite `ProjectAction::AddBoxBrush`.
  - `○` Add Cylinder Brush → `AddCylinderBrush`.
  - Separator.
  - `▦` Face mode toggle → setea `EditorSubMode::Face` (o `Object` si
    ya estaba en Face).
- Tooltips en hover con el nombre completo + atajo de teclado (W / E / R
  para gizmos, no hay aún para brushes, "3" para face mode).

Si el scope del bloque D crece (ej. font icons), partir a hito propio.
**Decisión MVP**: usar caracteres ASCII grandes (`T`/`R`/`S`/etc.) en
botones cuadrados — claros, sin deps, y dejan margen para upgrade
visual cuando emerja necesidad de polish estético.

### E — Tests

`tests/test_workspace_manager.cpp` (extender el existente):
- Test de migración: `WorkspaceManager` recibe lista con nombres viejos
  → guardo internamente con los nuevos.
- Test de mezcla: lista con `[Modelar, Scripting, Optimizar, Materials]`
  → solo migra los 2 viejos, los nuevos quedan intocables.
- Test de iniLayout preservado: la migración no toca el `iniLayout` —
  solo cambia el `name`.

Sin tests para la toolbar (UI pura sin lógica testeable).

### F — Validación visual + suite

- Build editor + tests.
- Suite verde, esperado **~607 → ~613 cases**.
- Validación visual del dev:
  1. Abrir un proyecto existente (con `.moodproj` que tiene los nombres viejos).
  2. Ver que los tabs de workspace en la barra superior dicen
     `Modelar | Programar | Optimizar | Materiales`.
  3. Cambiar a `Modelar` → ver SOLO Viewport + Hierarchy + Inspector +
     AssetBrowser docked. Console/LuaApi/Performance/MaterialEditor/
     ScriptEditor cerrados.
  4. Cambiar a `Programar` → ver el set adecuado de programación.
  5. Toolbar lateral con `T R S □ ○ ▦` clickeable cambia el modo / spawnea
     brushes.
  6. Reabrir el proyecto → workspaces siguen como `Modelar | Programar |
     Optimizar | Materiales` (migración persistió).

### G — Cierre

- `docs/HITOS.md`: nueva entrada F2H22 closed.
- `docs/ESTADO_ACTUAL.md`: F2H22 al top.
- `docs/DECISIONS.md`: entrada `2026-05-07` "F2H22 — workspaces orientados
  a tareas + visibility default + toolbar lateral".
- Tag: `v1.13.0-fase2-hito22`.
- `PENDIENTES.md`: F2H23 candidato = 4-viewport Hammer (heredado
  movido); diferidos no urgentes actualizados con "iconos image-based"
  como nice-to-have.

---

## Suite

Todo el código nuevo es **aditivo + back-compat**:
- Renames: cambios en strings + mapeo en setWorkspaces.
- Visibility: nuevo método en EditorUI llamado al switch.
- Toolbar: panel nuevo, no toca panels existentes.
- Layouts: ajustes a los 4 buildXxxWorkspace existentes.

Riesgo de regresión bajo. Validación crítica = abrir un proyecto viejo
y confirmar que los workspaces migran sin perder customización.

## Riesgos

- **iniLayout viejo con referencias a panels que ya no se docean**:
  ImGui los muestra flotantes por default. Solución: pedir
  `requestResetToDefault` automáticamente cuando se detecta migración
  exitosa, así el layout default nuevo aplica. **NO** lo hago
  automático en F2H22 — el dev puede hacerlo manualmente desde
  `Ver > Restablecer layout` si algo se ve raro. Documentar.
- **Toolbar tapa el viewport overlay**: el panel del toolbar va al
  costado del Viewport como otro dock; el overlay 2D (gizmos, OBB
  selección) sigue funcionando sobre el viewport — NO afectado.
- **Detección de "Modelar" en versiones viejas hardcodeadas**: alguna
  parte del código puede tener `if (name == "Layout")` hardcoded
  fuera del Dockspace. Auditar con grep.

## Criterios de cierre

- [ ] Workspaces tienen nombres nuevos en español.
- [ ] `.moodproj` viejos cargan con nombres migrados sin perder
      customización.
- [ ] Cada workspace muestra solo sus panels relevantes por default.
- [ ] Toolbar lateral con iconos `T R S □ ○ ▦` funcionando.
- [ ] Suite verde sin regresiones.
- [ ] Tag `v1.13.0-fase2-hito22` pusheado.
- [ ] `PENDIENTES.md` actualizado con F2H23 (4-viewport).
