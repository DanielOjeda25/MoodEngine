# Plan — F2H7: Workspaces estilo Blender + menú Ver categorizado

> **Leer primero:** `ESTADO_ACTUAL.md` (F2H6 cerrado, sub-fase 2.1
> avanzada), `PLAN_FASE2.md` sección 4 (F2H6 plan original = "Reorg
> UI editor por categorías", absorbido aquí + ampliado),
> `DECISIONS.md` (entradas anteriores de Fase 2).

---

## Objetivo

Transformar el editor en una herramienta tipo Blender con **workspaces**:
tabs en el top bar que cambian el layout completo de paneles. Mismo
proyecto, misma escena, misma selección — distinta vista optimizada
para la tarea actual (Layout / Scripting / Profile / Materials).

Adicionalmente: integrar la **categorización del menú "Ver"** que
estaba en el plan original (F2H6 plan), para que togglear panels
individuales dentro del workspace activo sea fácil de encontrar.

**Esto es un cambio cualitativo de UX**, no una optimización. Mejora
cómo trabaja el dev día a día.

---

## Filosofía de diseño

Sigue el modelo de Blender:
- **Workspaces son parte del proyecto** (`.moodproj`) — cada proyecto
  puede customizar.
- **Workspace = layout serializado de ImGui** (qué panels visibles,
  dónde, en qué tamaño).
- **Defaults hardcoded** los primeros 4 (Layout / Scripting / Profile /
  Materials). El user los modifica libremente, pero NO puede crear
  nuevos en v1 (UI para "Nuevo workspace" / "Eliminar" / "Duplicar"
  queda para F2H8+ si emerge necesidad).
- **Auto-save**: al switchear de workspace, el layout actual se captura
  al workspace que estaba activo. Las modificaciones persisten sin
  pasos manuales.
- **Sin back-compat** del `.moodproj`: el dev confirmó que ya borró los
  proyectos viejos. Schema nuevo limpio.

---

## Decisiones arquitectónicas (firmadas con el dev al inicio)

| # | Decisión | Valor |
|---|---|---|
| 1 | Scope de workspaces | Por proyecto (en `.moodproj`) |
| 2 | Workspaces default | 4: Layout / Scripting / Profile / Materials |
| 3 | Crear/eliminar custom en v1 | NO (F2H8+) |
| 4 | Persistencia de modificaciones | SÍ (auto-save al switch) |
| 5 | Iconos en tabs | NO en v1 (texto solo) |
| 6 | Default activo | "Layout" |
| 7 | First-run sin proyecto | Cargar los 4 defaults globales |
| 8 | Back-compat de proyectos viejos | NO necesario (proyectos viejos borrados) |

---

## Bloques

### A — Plan F2H7 (este archivo)

Documento del hito. Cierra al commit inicial.

### B — `Workspace` struct + `WorkspaceManager` puro

`src/editor/workspace/Workspace.h`:
```cpp
struct Workspace {
    std::string name;            // "Layout", "Scripting", ...
    std::string iniLayout;       // ImGui ini serializado (texto)
    bool isDirty{false};         // marcado al editar; reset al save
};
```

`src/editor/workspace/WorkspaceManager.{h,cpp}`:
- `std::vector<Workspace> m_workspaces;`
- `int m_activeIndex{0};`
- `void setActiveByIndex(int)` — guarda el actual + carga el nuevo.
- `int activeIndex() const`.
- `Workspace& activeWorkspace()`.
- `usize count() const`.
- `void captureCurrentLayout(const std::string& iniBuffer)` — actualiza
  el iniLayout del workspace activo desde un buffer ImGui.
- Helpers de persistencia (json read/write).

PURO: no toca ImGui directo. Las llamadas a
`SaveIniSettingsToMemory`/`LoadIniSettingsFromMemory` viven en el
caller (UI layer) — el manager solo guarda/devuelve los strings.

### C — UI tabs en top bar

`src/editor/ui/WorkspaceTabBar.{h,cpp}` (nuevo) o integrado en `MenuBar`.

- Fila propia debajo del menú principal (estilo Blender).
- `ImGui::BeginTabBar` con un tab por workspace.
- Estado activo visualmente distinguido (highlight).
- Click en tab → `WorkspaceManager::setActiveByIndex` + propagación
  del swap del layout a `Dockspace` / `EditorUI`.

### D — Switching: capture + load del layout ImGui

Lógica del swap:

1. Antes de cambiar:
   - `const char* current = ImGui::SaveIniSettingsToMemory(&size);`
   - `m_workspaceManager.captureCurrentLayout(std::string(current, size));`
2. Cambiar `m_activeIndex`.
3. Después de cambiar:
   - `ImGui::LoadIniSettingsFromMemory(m_workspaces[idx].iniLayout.c_str(), m_workspaces[idx].iniLayout.size());`
   - ImGui re-aplica el layout (panels visibility + dock state).

**Crítico**: el switch debe ocurrir **fuera** del frame de ImGui (entre
`ImGui::Render()` y el siguiente `NewFrame()`), porque
`LoadIniSettingsFromMemory` resetea estado. Implementación: marcar un
"pending switch" y aplicarlo en el frame siguiente.

### E — 4 default workspaces con layouts predefinidos

Helper en `WorkspaceManager` para construir los 4 defaults la primera
vez (cuando se abre un proyecto sin workspaces):

- **Layout**: viewport central grande + Hierarchy izq + Inspector der +
  AssetBrowser inf. **Default activo**.
- **Scripting**: ScriptEditor central grande + Console inf + viewport
  esquina pequeña + Hierarchy esquina pequeña.
- **Profile**: PerformanceHUD overlay + Console grande + viewport
  central.
- **Materials**: MaterialEditor central + AssetBrowser izq + viewport
  der + Inspector der.

Los layouts se construyen con `ImGui::DockBuilder` API
(`<imgui_internal.h>`). API es semi-internal pero estable; la usan
docenas de proyectos open-source como referencia.

### F — Persistencia: `.moodproj` schema bump

`src/engine/scene/serialization/ProjectSerializer.{h,cpp}`:

- Schema nuevo: agregar `workspaces` como array de objetos
  `{name, ini_layout}` al JSON del proyecto.
- Al cargar: si el array existe, popular `WorkspaceManager`. Si no,
  poblar con los 4 defaults.
- Al guardar: capturar el layout del workspace activo + serializar
  todos al JSON.
- **Sin back-compat**: si encuentra un `.moodproj` sin `workspaces`,
  asumir defaults (no es error). Proyectos viejos borrados = nadie
  carga un schema antiguo.

### G — Menú "Ver" categorizado

`MenuBar.cpp`:

Reemplazar el loop plano por submenús anidados:

```
Ver
├─ Scene
│  ├─ Hierarchy
│  ├─ Inspector
│  └─ Viewport
├─ Assets
│  ├─ AssetBrowser
│  ├─ MaterialEditor
│  └─ ScriptEditor
├─ Debug
│  ├─ Console
│  ├─ LuaApi
│  └─ Performance HUD
├─ World (placeholder, vacío hasta F2H10+ CSG)
├─ ─────────────
└─ Restablecer layout actual al default
```

Nuevo método en `IPanel`: `virtual const char* category() const = 0;`
con default `"Scene"`. Cada panel sobrescribe (3-5 LOC por panel).

El toggle de visibility opera sobre el workspace activo — la modificación
persiste vía auto-save al cambiar workspace.

### H — Tests doctest del `WorkspaceManager`

`tests/test_workspace_manager.cpp`:

- Construcción default: 4 workspaces, activo = 0.
- `setActiveByIndex(2)` cambia el activo. `activeIndex()` devuelve 2.
- `captureCurrentLayout("layout-A")` luego `setActiveByIndex(1)` luego
  `setActiveByIndex(0)` → el iniLayout del workspace 0 es "layout-A".
- Out-of-bounds index ignorado (no crash).
- Round-trip JSON: serializar 4 workspaces con datos diversos →
  deserializar → comparar.
- Loading vacío: si JSON no tiene workspaces, popular con los 4
  defaults.

Suite esperada: 358 → 365+ test cases.

### I — Documentación + cierre

- Actualizar `docs/HITOS.md`: marcar F2H7 [x].
- Actualizar `docs/ESTADO_ACTUAL.md`: sección 1 reescrita con F2H7
  cerrado, próximo paso = F2H8 (docs públicas o toolbar polish).
- Entrada en `docs/DECISIONS.md` (2026-05-XX): "Workspaces estilo
  Blender — diseño con WorkspaceManager puro + ImGui ini serializado".
- Commit y tag: `v1.1.5-fase2-hito7`.

---

## Suite

Cambios aditivos (nuevo módulo workspace + categorización del menú
existente). Tests del manager son puros (sin ImGui). Suite esperada al
cierre: **365+/6850+** sin regresiones.

## Riesgos

- **`ImGui::DockBuilder` API es `<imgui_internal.h>`**: estable pero
  doc escasa. Mitigación: copiar patrones de proyectos open-source que
  ya lo usan (referencia en el código del bloque E).
- **Switching de layout durante un frame**: `LoadIniSettingsFromMemory`
  resetea state. Requiere marcar "pending switch" y aplicar
  fuera del frame ImGui. Mitigación: pattern documentado.
- **Schema bump del `.moodproj`**: el dev confirmó que borró proyectos
  viejos, no hay back-compat. Si más adelante alguien tiene un
  proyecto pre-F2H7, cargar = "no tiene workspaces" → poblar con
  defaults. Sin error.
- **Save/load del layout puede no preservar visibility 100%**: ImGui
  serializa el dock state pero el flag `IPanel::visible` es nuestro.
  Mitigación: serializar también el array de visibility en cada
  workspace, restaurar al switch.
- **El editor abierto sin proyecto**: cargar los 4 defaults globales
  (no del proyecto). Cuando se abre un proyecto, sus workspaces lo
  sobrescriben.

---

## Criterios de cierre

- [ ] `Workspace` struct + `WorkspaceManager` puro implementados.
- [ ] UI tabs visibles + funcionales en el top bar.
- [ ] Switching cambia el layout completo (viewport size, panels
      visibility, dock positions).
- [ ] 4 workspaces default con layouts predefinidos.
- [ ] Modificaciones manuales (drag de panel, toggle visibility) se
      auto-guardan al switchear.
- [ ] Persistencia: el layout sobrevive al cerrar y reabrir el proyecto.
- [ ] Menú "Ver" categorizado por carpeta (Scene / Assets / Debug /
      World).
- [ ] 7+ tests nuevos en `test_workspace_manager.cpp`, todos pasando.
- [ ] Suite global sin regresiones (365+/6850+).
- [ ] `docs/HITOS.md`, `ESTADO_ACTUAL.md`, `DECISIONS.md` actualizados.
- [ ] Tag `v1.1.5-fase2-hito7` pusheado.
