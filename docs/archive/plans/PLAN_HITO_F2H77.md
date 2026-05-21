# PLAN HITO F2H77 — Pulido visual base + indicador "sin guardar"

> **Estado**: en ejecución (aprobado por el dev en sesión).
> **Sub-fase**: 2.7 (UI/UX final + cierre Fase 2). Segundo hito de la sub-fase.
> **Predecesor**: F2H76 (Preferencias + temas + redondeo).

## Origen

Tras F2H76 (redondeo de esquinas, que al dev le gustó), pidió aplicar "esa
forma" a otras pestañas/paneles y recomendaciones UX como diseñador
experimentado. Se hizo una **auditoría UX completa** del editor.

**Hallazgo importante**: la mayoría de las "quick wins" del audit **ya estaban
implementadas** — el editor está más pulido de lo que el reporte inicial
sugería. Verificado en código:

| Recomendación del audit | Realidad |
|---|---|
| Tooltips con atajos en toolbars | ✅ ya en `Toolbar.cpp` + `MapEditorTopBar.cpp` |
| Pestaña "Luces" en Crear Entidad | ✅ ya implementada (F2H60) |
| Inspector estado vacío con hint | ✅ ya (`inspector.no_selection` + hint) |
| Inspector secciones colapsables | ✅ ya (`CollapsingHeader`, ImGui recuerda estado) |
| Indicador "sin guardar" | ⚠️ parcial: el título del SO ya pone `*`, pero no se ve dentro del editor |
| Espaciado/padding unificado | ❌ solo se seteaba el redondeo |

Por eso el hito se acota a las **dos cosas genuinamente faltantes**.

## Qué siente el usuario

1. **Todo respira igual**: los paneles dejan de tener espaciados dispares; un
   padding/spacing consistente (hermano del redondeo de F2H76) da sensación de
   producto terminado.
2. **Sé si tengo cambios sin guardar de un vistazo**: además del `*` en el
   título de la ventana (fácil de no ver), aparece un punto ámbar **"● Sin
   guardar"** en la status bar mientras haya ediciones pendientes; desaparece al
   guardar (Ctrl+S).

## Bloques

### A — Métricas unificadas en el tema (HECHO)
`EditorThemes.cpp`: `applyRounding` → `applyMetrics` (suma al redondeo de F2H76
el espaciado): `WindowPadding (10,8)`, `FramePadding (8,5)`, `ItemSpacing (8,7)`,
`ItemInnerSpacing (6,5)`, `CellPadding (6,5)`, `IndentSpacing 20`,
`ScrollbarSize 12`, `GrabMinSize 11`. Común a los 4 temas (seteado aparte de los
colores → sobrevive al theme-switch).

### B — Badge "sin guardar" en la status bar
Surfacea el `m_projectDirty` que **ya existe** (`markDirty()` lo prende en cada
edición; `handleSave()` lo apaga; el título del SO ya muestra `*`). Cableado:
- `StatusBar` gana `setProjectDirty(bool)` + render de un segmento `● Sin
  guardar` ámbar (`ICON_FA_CIRCLE`) cuando hay cambios.
- `EditorUI::setProjectDirty` forwardea a la status bar.
- `EditorApplication::updateWindowTitle()` (único punto de sync — ya se llama en
  cada transición de dirty: markDirty/save/new/open/close) llama
  `m_ui.setProjectDirty(...)`. Sin polling por frame.
- i18n `editor.statusbar.unsaved` en/es.

### C — Cierre
`docs/hitos/F2H77.md`, one-liner en `HITOS.md`, sección 0.1 de `ESTADO_ACTUAL.md`,
nota en `DECISIONS.md` (audit mostró editor ya pulido; hito acotado), archivar
este plan. Tag `v1.68.0-fase2-hito77`.

## Fuera de scope (futuro de 2.7, si el dev los prioriza)
- Ctrl+S contextual (que guarde el panel con foco, no solo el proyecto).
- Unificar Undo en Material/Item/Quest editors (hoy no tienen).
- Acento de marca único (hoy cada tema ya es consistente internamente).

## Riesgos
- El espaciado más generoso podría apretar paneles muy densos (Inspector con
  muchas secciones). Validar runtime que no genere scroll excesivo; ajustar
  valores si molesta.
