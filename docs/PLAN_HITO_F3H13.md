# PLAN F3H13 — TBD (cierre Sub-fase 3.2)

**Estado:** **A DEFINIR**.
**Predecesor:** F3H12 (Undo coverage audit del Inspector).
**Origen:** `PLAN_FASE3.md` Sub-fase 3.2 menciona "Reset to default en cada Inspector field" como uno de los hitos de la sub-fase.

---

## Candidatos para F3H13 (a decidir con el dev)

### A) Reset to default per-field en el Inspector (candidato fuerte del plan original)

`PLAN_FASE3.md` Sub-fase 3.2 declara:
> **F3H13 — "Reset to default" en cada Inspector field.**

Convención Unity/Unreal: cada field tiene un botón `↺` que aparece solo cuando `current != default`. El dev ya implementó esto para Project Settings (F3H4) y User Preferences (F3H7) via helper template `resetButton<T>` — Sub-fase 3.1 lo usa para 4 secciones (Performance/Gameplay/Character/Editor).

**Trabajo estimado:**
- Llevar el helper `resetButton<T>` del `ProjectSettingsPanel` a `InspectorPanel_Internal.h` (compartido).
- Cada widget editable del Inspector recibe un reset button al lado.
- Default por componente: tomar de la construcción `{}` del componente (mismo patrón que `kEnvDefaults` que F3H12 ya usa en Environment).
- Edits via reset deben generar entrada al HistoryStack (mismo `EditPropertyCommand<T>` que F3H12 usa para combos/checkboxes).

**Trabajo NO trivial:**
- ¿Reset por field o por sección? F3H12 ya implementó "reset por sección" en Environment (6 secciones). Inspector general probablemente quiere por-field (Unity convention).
- Defaults compuestos: la dirección de una luz directional, los axisU/V de un brush face, etc. ¿Cuáles tienen un "default" sensato?
- Defaults runtime: algunos fields se inicializan desde el spawn handler (ej. capsule de player se inicializa desde `CharacterSettings`). El reset debe restaurar al default del componente o al del runtime inicial?

**Por qué cierra Sub-fase 3.2:** completa la trifecta "Inspector que el dev controla" — multi-edit (F3H8) + copy/paste (F3H9-F3H11) + undo coverage (F3H12) + reset to default (F3H13).

### B) Búsqueda en Hierarchy + Asset Browser

`PLAN_FASE3.md` también menciona:
> **F3H12 — Búsqueda en Hierarchy + Asset Browser.**
> Ctrl+F filtra en vivo. Por nombre, por tipo de componente, por tag. Tecla Esc limpia.

El plan original tenía esto como F3H12 — pero el dev priorizó undo coverage. Esta búsqueda sigue siendo deuda. Si el Hierarchy crece a >50 entities el filtrado es indispensable.

### C) Backlog UX descubierto en F3H12

Memoria `backlog-ux-gaps-editor`:
- Spawn de ForceField/Cloth desde el menú Add Entity / Hierarchy / AssetBrowser.
- Workflow "agregar sonido al mesh" (puerta con audio al activarse) — Inspector slot, evento o prefab.

El dev pidió anotar "para luego" — re-evaluar al planificar 3.3/3.4 o si se le ocurre algo más urgente al cerrar 3.2.

---

## Recomendación

Yo (Claude) sugiero **opción A — Reset to default per-field**:
1. Cierra Sub-fase 3.2 con el último bloque del plan original.
2. Aprovecha la infra que F3H12 puso (`pushAtomicEdit<T>`, helpers en Internal.h, defaults conocidos via `kEnvDefaults`).
3. Tier 1 acotado: Light, Trigger, ForceField, Particle, Cloth, RigidBody, Ragdoll, Joint, Audio (los paneles con defaults claros).
4. Tier 2 diferible: MeshRenderer (material es asset compartido, "reset" significa qué exactamente?), Brush (default per-face vs per-brush?), Vehicle (config viene del .moodvehicle, reset al asset load?), Script (overrides ya tienen el Reset SmallButton).

**Pregunta al dev cuando arranque F3H13:** ¿confirmás opción A o querés B/C?

---

## Lo que NO toca F3H13 (cualquiera sea la opción)

- Sub-fase 3.3 (Asset Browser de verdad): F3H14-F3H19 son scope diferente.
- Sub-fase 3.4 (Viewport pro + Performance): F3H20-F3H27.
- Los gaps de UX externos del memoria `backlog-ux-gaps-editor` (a menos que se elija opción C explícitamente).
- Inventory operaciones estructurales sin undo — diferido a hito propio si el dev lo reclama.
- Script Reset SmallButton del override sin undo — ídem.
