# Plan — F2H19: Limpieza HistoryStack residual

> **Leer primero:** `ESTADO_ACTUAL.md` (F2H18 cerrado), `PENDIENTES.md`
> ítem 2, `DECISIONS.md` entry 2026-05-06 "HistoryStack Blender-style —
> wireup, no refactor (F2H16)" (patrón a reusar).

---

## Objetivo

Auditar el editor post-F2H17 (multi-material) + post-F2H18 (reorg menús)
para identificar **handlers que mutan estado sin push al `HistoryStack`**
y por tanto no son undoable con Ctrl+Z. Implementar comandos nuevos +
wireup en los handlers identificados.

**Por qué ahora**: F2H17 introdujo multi-material rendering, drops
distinguen Object/Face Mode, slots de material per-cara. La auditoría
anterior (F2H16 Bloque B) cubrió el modelo de F2H15 (singular `material`
+ UV per-brush). El refactor de F2H17 puede haber dejado huecos: drop
en Face Mode con slot nuevo, mutaciones desde el Inspector para items
per-cara, etc.

Mismo approach que F2H16: **wireup, no refactor**. Mantener el command
pattern del Hito 27. Captura por tag (no handle EnTT) para robustez al
delete/recreate.

---

## Filosofía de diseño

- **Subagente recorre el editor** y produce lista concreta. No predecir
  desde el plan; auditar el código real.
- **Granularidad Blender-style**: drag = 1 command, drop = 1 command,
  toggle de checkbox = 1 command. Nombres humano-legibles.
- **Captura por tag**: comandos no almacenan `entt::entity` directo;
  almacenan el tag y resuelven por `Scene::findEntityByTag` en
  execute/undo. Robusto al recreate del HistoryStack.
- **Sin refactor del pattern**: NO migrar a snapshot pattern. NO romper
  comandos existentes. Solo agregar lo que falta y wireup.
- **Reusar helpers existentes**: `BrushUVSnapshot`, `captureBrushUV`,
  `applyBrushUV`, `snapshotsEqual` (de F2H16) si aplican; nuevos snapshots
  solo si la deuda nueva no encaja.

---

## Decisiones arquitectónicas

| # | Decisión | Valor |
|---|---|---|
| 1 | Approach | Wireup (no refactor a snapshot pattern). |
| 2 | Captura | Por tag, no handle EnTT. |
| 3 | Granularidad | Drag = 1, drop = 1, checkbox = 1 (Blender-style). |
| 4 | Tests | Round-trip por cada comando nuevo (execute → undo → estado pre = post; redo idempotente). |
| 5 | Scope | 3 deudas confirmadas (auditoría Bloque B). Cerradas: drop textura/material en Face Mode + drop de script. |
| 6 | Nombres de comando | Estilo "Asignar material a cara N", "Asignar script". |
| 7 | Fuera de scope | Acciones de menú Mapa (alineado con Blender/Unity: no se hace undo de "abrir mapa") y toggles de modo/selección. |

## Scope cerrado tras Bloque B

3 deudas con prio ≥ media confirmadas por auditoría:

1. **Drop de textura sobre cara en Face Mode** ([`DemoSpawners.cpp:645-668`](../src/editor/application/DemoSpawners.cpp)) — regresión F2H17. El branch `if (faceMode)` muta `bc.materials.push_back(newMat)` y `face.materialIndex = slot` sin push. Comando nuevo: **`EditBrushFaceMaterialCommand`**.
2. **Drop de material sobre cara en Face Mode** ([`DemoSpawners.cpp:930-950`](../src/editor/application/DemoSpawners.cpp)) — regresión F2H17. Mismo patrón que #1, mismo comando nuevo cubre ambos.
3. **Drop de `.lua` sobre entidad** ([`DemoSpawners.cpp:991-1028`](../src/editor/application/DemoSpawners.cpp)) — deuda pre-F2H17 (Hito 22). Muta `ScriptComponent.path` (o agrega componente nuevo) sin push. Comando nuevo: **`EditScriptComponentCommand`**.

Total: **2 comandos nuevos** + wireup en 3 sitios.

**Confirmado bueno (no requiere fix)**: el UV editor del Inspector en Face Mode (post-F2H17) sí pushea `EditBrushUVCommand` — F2H17 hizo el wireup correctamente. La regresión está solo en los drops.

---

## Bloques

### A — Plan F2H19 (este archivo, provisional)
Definir filosofía + decisiones marco. El scope concreto se cierra tras
Bloque B.

### B — Auditoría exhaustiva (subagente)
Subagente recorre `src/editor/` buscando:
- Mutaciones en handlers de drop (`DemoSpawners.cpp`, viewport processors).
- Mutaciones en widgets del Inspector que NO usen el patrón
  `IsItemActivated` + `IsItemDeactivatedAfterEdit` para push.
- Mutaciones nuevas introducidas por F2H17 (Face Mode, slots de material,
  picking de cara).
- Comandos existentes que asumen API vieja (`bc.material` singular vs
  `bc.materials[]` post-F2H17) y necesitan extensión.

Produce **lista concreta** con archivo:línea, descripción del bug,
comando que falta. Subir scope final del hito a este plan.

### C — Implementar comandos nuevos
Por cada deuda confirmada:
- Crear `src/editor/commands/<Nombre>Command.{h,cpp}`.
- Definir snapshot pre/post con captura por tag.
- Implementar `execute()` + `undo()` + `name()`.

### D — Wireup en handlers
Por cada handler identificado:
- Reemplazar mutación directa por `historyStack->push(make_unique<X>(...))`.
- En widgets ImGui usar el patrón `IsItemActivated` → snapshot pre +
  `IsItemDeactivatedAfterEdit` → snapshot post + push si difieren.

### E — Tests
Por cada comando nuevo:
- Round-trip (execute → undo restaura estado pre).
- Redo idempotente (execute → undo → execute = primer execute).
- Tag inexistente no crashea (warn + no-op).
- Snapshot edge cases (slot inexistente, vector vacío, etc.).

### F — Build + suite + validación visual
- `cmake --build build/debug --target MoodEditor mood_tests`.
- Suite verde (debe crecer con los tests nuevos).
- Lanzar editor (con permiso del dev), validar:
  - Drop de material en Face Mode (con slot nuevo) es undoable.
  - Cualquier widget per-cara identificado es undoable como un solo step.
  - Statusbar muestra "Último: <nombre>" actualizado.

### G — Cierre
- `HITOS.md`: entrada F2H19 cerrado, tag `v1.10.0-fase2-hito19`.
- `ESTADO_ACTUAL.md`: sección 1 reescrita; F2H18 desplaza a anterior.
- `DECISIONS.md`: entrada con razones del approach + lista de deudas
  resueltas.
- `PENDIENTES.md`: tachar ítem 2 si toda la deuda quedó cubierta;
  agregar nuevos pendientes detectados durante la auditoría.
- Archivar `PLAN_HITO_F2H19.md` en `docs/archive/plans/`.
- Commits agrupados (1 feat + 1 docs) + tag + push tras autorización.

---

## Lo que NO entra

- Refactor a snapshot pattern.
- Comandos para mutaciones que NO surjan en la auditoría.
- F6 panel de Blender (tweak last operator) — diferido desde F2H16.
- Multi-undo / branching history — diferido si emerge.
- F2H20 (mesh estática unificada) — hito propio siguiente.
