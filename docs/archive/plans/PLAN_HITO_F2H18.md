# Plan — F2H18: Reorg de menús del editor

> **Leer primero:** `ESTADO_ACTUAL.md` (F2H17 cerrado), `PENDIENTES.md`
> ítem 1 ("Reorganización de menús `Archivo > Mapa`"). Este hito cubre
> exactamente ese pendiente y nada más; no entran cambios funcionales.

---

## Objetivo

Reorganizar la barra superior del editor para separar **file ops**
de **geometría** y bajar a la mitad la profundidad de clicks para
operaciones frecuentes (spawn de brush, boolean ops). Cero cambios
funcionales — solo reubicación de items existentes.

**Por qué ahora**: el menú `Archivo > Mapa` mezcla ops del proyecto
(Nuevo, Abrir, Guardar como mapa) con geometría (Añadir Brush,
Boolean) y crece sin estructura. Spawn de brush requiere 4 clicks
(`Archivo → Mapa → Añadir Brush → Box`); con la reorg pasa a 3
(`Brush → Añadir → Box`). Antes de que F2H19+ acumulen más items
en `Archivo > Mapa`, atacar la deuda.

---

## Filosofía de diseño

- **Cero refactor funcional**: los `requestProjectAction(...)`
  calls no cambian. Solo se mueve **dónde** se renderiza cada
  `MenuItem`. El dispatcher del `EditorApplication` queda intacto.
- **Top-level menus por dominio**: `Archivo` (proyecto), `Mapa`
  (file ops del mapa actual), `Brush` (geometría: añadir +
  boolean), `Editar`, `Ver`, `Ayuda`. Cada menu top-level cubre
  un dominio claro.
- **Demos fuera de Ayuda**: los 13+ items de "agregar X demo" y
  el stress test no son ayuda al usuario; van a submenu
  `Ayuda > Demos ▶` (sub-anidado, no top-level — son secundarios
  al flow de mapping serio). Ayuda queda con solo "Acerca de".
- **Sin back-compat de muscle memory**: el dev usa el editor solo
  él, así que la reorg no rompe a nadie más. Si emerge fricción,
  ajustar en hito de polish.

---

## Decisiones arquitectónicas

| # | Decisión | Valor |
|---|---|---|
| 1 | Top-level `Mapa` | Sí. Promueve el submenu actual. Solo file ops del mapa (Nuevo/Abrir/Guardar como/default/Eliminar). |
| 2 | Top-level `Brush` | Sí. Reúne `Añadir ▶` y `Boolean ▶`. Considerar renombre a `Geometría` cuando entren shapes no-brush — diferido. |
| 3 | Demos | Submenu `Ayuda > Demos ▶`. Stress test queda en el mismo submenu (es un demo más). |
| 4 | `Archivo` final | Solo: Nuevo/Abrir/Guardar/Cerrar Proyecto + Empaquetar + Nuevo Script + Guardar prefab + Salir. |
| 5 | Tests | NO se agregan — el menú es UI puro de ImGui sin lógica testeable. La validación es visual. |
| 6 | Schema bump | No aplica (sin cambios de archivo). |
| 7 | Habilitación condicional | `Mapa` y `Brush` solo activos con `ui.hasProject()` — mismo comportamiento que el submenu actual. |
| 8 | Boolean menu | `ui.drawBooleanOpMenu()` se mueve dentro de `Brush` sin tocar su impl. |

---

## Bloques

### A — Plan F2H18 (este archivo)
Definir scope, decisiones, mapping de items viejos → nuevos.

### B — Implementación reorg
Editar `src/editor/ui/MenuBar.cpp`:
- Sacar el bloque `Mapa` de dentro de `Archivo`.
- Agregar `BeginMenu("Mapa", ui.hasProject())` top-level con
  los 5 file ops del mapa.
- Agregar `BeginMenu("Brush", ui.hasProject())` top-level con
  `Añadir Brush ▶` (7 primitivas) + `ui.drawBooleanOpMenu()`.
- Mover los 13+ items de demos de `Ayuda` a
  `BeginMenu("Demos") ▶` dentro de `Ayuda`.
- Mover `Stress test poligonos ▶` al mismo `Ayuda > Demos`.
- Mantener separators y orden visual.

### C — Build + suite + validación visual
- `cmake --build build/debug --target MoodEditor mood_tests`.
- Suite debe seguir 567/8182 verde (cero cambios funcionales).
- Lanzar editor (con permiso del dev), validar:
  - Cada item del menú nuevo dispara la acción correcta.
  - `Mapa` y `Brush` están deshabilitados sin proyecto activo.
  - Boolean menu sigue funcionando con selección activa.
  - Demos siguen spawneando entidades correctamente.

### G — Cierre
- `HITOS.md`: entrada F2H18 cerrado, tag `v1.9.0-fase2-hito18`.
- `ESTADO_ACTUAL.md`: sección 1 reescrita; F2H17 desplaza a anterior.
- `DECISIONS.md`: entrada con razones de la reorg + decisión de
  separar Brush vs Geometría (diferido).
- `PENDIENTES.md`: tachar ítem 1; ítem 2 (HistoryStack residual)
  permanece para F2H19.
- Archivar `PLAN_HITO_F2H18.md` en `docs/archive/plans/`.
- Commit `feat(F2H18 Bloque B): reorg menús editor` + commit
  `docs(F2H18 Bloque G): cierre`. Tag + push tras autorización
  explícita.

---

## Lo que NO entra

- Renombre de `Brush` a `Geometría` o `Mapa` a `Niveles` (diferido).
- Toolbar lateral con iconos de brush (Hammer-style) — diferido.
- Atajos de teclado nuevos (Ctrl+B para añadir box, etc.) —
  diferido. Hoy solo Ctrl+S existe; agregar más es scope propio.
- Reorg del Inspector o del AssetBrowser — F2H18 es solo la barra
  superior.
- F2H19 (limpieza HistoryStack residual) — hito propio siguiente.
- F2H20 (mesh estática unificada) — era F2H18 original, corrido.
