# PLAN F3H9 — Copy/Paste de components entre entidades

**Estado:** Planeado (segundo hito de Sub-fase 3.2 "Inspector + Hierarchy pulidos", arranca tras `v2.8.0-fase3-hito8`).
**Predecesor:** F3H8 (multi-edit del Inspector — Light Tier 1).
**Origen:** Plan maestro [`PLAN_FASE3.md`](PLAN_FASE3.md) Sub-fase 3.2 — *"F3H9 — Copy/Paste de components. Click derecho en header de component → Copiar / Pegar valores / Pegar como nuevo. Cross-entity y cross-proyecto."*

---

## Qué siente el usuario

El dev tiene una luz `PointLight_A` con color rojo, intensity 5, radius 10. Quiere copiar EXACTAMENTE esa configuración a `PointLight_B` que está hoy en defaults (white / 1 / 5).

Hoy: tiene que abrir el Inspector de A, anotar los 3 valores, abrir el Inspector de B, escribirlos uno por uno. Lento + propenso a tipear mal.

Después de F3H9:
1. Click derecho en el header "Light" del Inspector con A seleccionado → menú con "Copiar valores".
2. Click en B en la Hierarchy.
3. Click derecho en el header "Light" → "Pegar valores".
4. B queda con color rojo, intensity 5, radius 10 — todos los campos del componente copiados de A.

Caso variante (cross-entity sin Light pre-existente):
1. A tiene Light. B tiene solo Transform + MeshRenderer (no tiene Light).
2. Click derecho en "Light" del Inspector de A → "Copiar valores".
3. Click derecho en header de cualquier componente de B (o en zona libre del Inspector) → "Pegar como nuevo componente".
4. B gana un `LightComponent` con los valores de A.

---

## Realidad técnica (qué sí / qué no)

**Sí en F3H9:**
- **Clipboard interno** (no Windows clipboard) para components: estructura `CopiedComponent { typeName, jsonPayload }` en `EditorUI` (singleton). Se llena al "Copiar", se consume al "Pegar". Persiste mientras el editor esté abierto; se invalida al cerrar.
- **Serialización via JSON** (reusar `EntitySerializer`). El payload es el mismo formato que el `.moodmap`, así que copy/paste de componente == roundtrip dentro del editor. Cero código nuevo de serialización per-component.
- **Click-derecho en header de componente** abre menú contextual con:
  - `Copiar valores` (siempre disponible).
  - `Pegar valores` (gris si el clipboard está vacío o tipo distinto).
  - `Pegar como nuevo` (gris si el clipboard está vacío; aparece sobre headers de OTROS componentes o en zona libre del Inspector).
- **Componentes soportados**: TODOS los que `EntitySerializer` ya maneja (Light, MeshRenderer, RigidBody, Trigger, Joint, Ragdoll, Vehicle, Audio, Animator, ParticleEmitter, ForceField, Cloth, Script, Inventory, Brush — Brush con caveats per material indices). Transform y Tag NO (son nucleo de entidad, no de componente — copiar Transform no tiene UX claro; Tag ya se edita inline).
- **Undoable**: paste de valores existentes → `EditPropertyCommand<T>` por field (o un nuevo `PasteComponentCommand` que envuelve N fields). Paste como nuevo → `AddComponentCommand<T>` (ya existe desde F2H45) + setea los fields.
- **Cross-entity** (mismo proyecto): el clipboard vive en `EditorUI`. Si el dev cierra el proyecto y abre otro, el clipboard se invalida (probable — decisión D2).
- 2-3 keys i18n nuevas (menu items).
- 4-6 tests del clipboard + apply logic.

**NO en F3H9:**
- **Cross-process** (copiar de un editor a otro corriendo): requiere serialización al Windows clipboard. Scope distinto, baja prioridad.
- **Cross-proyecto** persistente (clipboard que sobrevive cerrar editor): out of scope. Si emerge demanda → hito futuro con archivo `.moodclipboard` en `%APPDATA%`.
- **Múltiples componentes en clipboard** (copiar Light + MeshRenderer juntos): clipboard es un slot único por ahora.
- **Atajos de teclado** (Ctrl+C / Ctrl+V dentro del Inspector): los atajos globales de Ctrl+C/V están reservados para futuro entity-clipboard (más útil). F3H9 entra solo via menú contextual.
- **Copy/Paste de transforms/tags**: explícitamente excluidos del menu (D3 abajo).

---

## Bloques

### A — `CopiedComponent` slot en `EditorUI`
- Struct `{ std::string typeName; nlohmann::json payload; }` + `optional<CopiedComponent>` en EditorUI.
- Setters `setCopiedComponent(typeName, payload)` + `consumeCopiedComponent()` (NO consume — peek) + `clearCopiedComponent()`.
- typeName comparable con strings (no enum) para futura forward-compat (si se agrega un componente nuevo, no rompe el clipboard de versiones viejas).

### B — Serialización via `EntitySerializer`
- Reusar `EntitySerializer::serializeComponent<T>(entity)` si existe (revisar). Si no, agregar.
- Patrón: dado un componente type + entity, escupir el JSON sub-object correspondiente.
- Mismo formato que el `.moodmap` — cero divergencia.

### C — Menú contextual en `beginComponentSection`
- En `InspectorPanel::beginComponentSection<T>` ya hay un `BeginPopupContextItem` que ofrece "Quitar componente" (F2H81).
- Agregar 2 nuevos items: "Copiar valores" + "Pegar valores" (visible si clipboard.typeName == T's typeName + componente compatible).
- Templated en T para hace `serializeComponent<T>` + `deserializeAndApply<T>`.
- Tooltip: "Copia los valores del componente al clipboard interno del editor."

### D — "Pegar como nuevo"
- Opción adicional en el popup del `Add Component` o en el header de otros componentes.
- Si `clipboard.typeName` no es el del header actual y la entidad NO lo tiene → ofrece "Pegar `<TypeName>` como nuevo componente".
- Internamente: `AddComponentCommand<T>` + setear fields del clipboard payload.

### E — Undoable
- 2 paths:
  - **Paste valores (componente existe)**: armar `MultiEditPropertyCommand` con N entries (1 por field) — O un comando `PasteComponentCommand` específico que opera por reflection sobre el JSON.
  - **Paste como nuevo**: combinar `AddComponentCommand` + setters. Probable necesite un comando compuesto `PasteAsNewComponentCommand`.
- Patrón Hito 27: tracker en `EditorUI` con `HistoryStack`.

### F — Tests
- 5-7 tests del flujo:
  - Set clipboard from Light A → assert content.
  - Paste sobre Light B → B gana los valores de A.
  - Paste sobre entidad sin Light → no-op silencioso (menu item está disabled, pero defensivo).
  - Paste como nuevo sobre entidad sin Light → componente agregado.
  - Undo del paste → restaura.
  - Clipboard se invalida al cerrar proyecto (test simulado).
  - typeName desconocido en clipboard → menu items disabled.

### G — i18n + asset sync
- 3-4 keys nuevas: `editor.inspector.context.copy_values`, `paste_values`, `paste_as_new`, `clipboard_empty_tooltip`.

### H — Validación visual
- Copy color/intensity/radius de A → paste en B → B queda idéntico a A.
- Paste como nuevo: B sin Light → copia de A → B con Light.
- Ctrl+Z → revierte.
- 2 entidades de tipos distintos (luz + cámara): copy Light → paste sobre header Camera → menu item disabled (typeName no coincide).
- Cerrar + reabrir proyecto → clipboard vacío.

---

## Decisiones a tomar (pre-implementación)

### D1 — Clipboard interno vs Windows clipboard

**Decisión:** **interno** en `EditorUI`. Razones:
- No requiere serialización a string + parsing en otra app. Cero complejidad de cross-process.
- El use case dominante es cross-entity dentro del MISMO editor.
- Windows clipboard puede ser invadido por otras apps (texto random pegado por error → menu disabled o crash). Interno es predecible.
- Si emerge demanda cross-process, agregar como sub-hito futuro (serialize a Windows clipboard como JSON text con un magic header `MOODENGINE_COMPONENT:`).

### D2 — Persistir clipboard al cerrar proyecto / cerrar editor

**Decisión:** **no persistir** — invalidar al cerrar proyecto (incluye cerrar el editor).

**Razones:**
- Use case "copio una luz, cierro el proyecto, abro otro, pego" es raro. El proyecto cambia, los assets cambian, los nombres cambian — el componente copiado puede referenciar assets que no existen en el nuevo proyecto.
- Implementación más simple sin file I/O.
- Si emerge demanda, persistir a `%APPDATA%\MoodEngine\clipboard.json` con validación al cargar.

### D3 — Qué componentes soporta y cuáles no

**Soportados** (lista F3H9): todos los del `EntitySerializer` excepto Transform y Tag.

**Excluidos explícitamente:**
- **Transform**: copy/paste de Transform completo no tiene UX claro — ¿posición absoluta? ¿solo escala? El gizmo + DragFloat3 ya cubre el caso. F3H10 podría agregar "copiar solo escala" o "copiar solo rotación" como menu items específicos si emerge necesidad.
- **Tag (name)**: el nombre se edita inline con InputText — no es un componente "rico" que valga la pena copy/paste.
- **Brush** (BrushComponent): edge case — el JSON tiene array de faces con material indices que apuntan al `materials[]` vector. Pegar entre brushes con distintas listas de materiales puede dar resultados raros. Por ahora SOPORTADO pero con advertencia en docs (no tooltip — invade el flow). Si emerge fricción, agregar guard.

### D4 — Atajos de teclado

**Decisión:** **NO** agregar Ctrl+C / Ctrl+V en F3H9.

**Razones:**
- Ctrl+C / Ctrl+V globales suelen reservarse para entity-level clipboard (copiar/pegar entidades enteras — convención Unity/Unreal). F3H9 entra solo via menú contextual para no robar esos atajos prematuramente.
- Si en el futuro hay hito de entity-clipboard, los componentes podrían usar Shift+Ctrl+C / Shift+Ctrl+V o entrar solo via menú (probable opción b).
- Sub-fase 3.2 está enfocada en UX del Inspector — no requiere atajos.

---

## Riesgos / a confirmar temprano

- **Composición con MultiSelect (F3H8)**: si el dev tiene 3 luces seleccionadas y hace "Pegar valores" sobre la activa, ¿se aplica solo a la activa o a las 3? Decisión a tomar al implementar — probable **solo activa** (paste es operación 1-a-1, no "broadcast"). Multi-paste a N entidades = trivial extensión con `MultiEditPropertyCommand` pero scope creep.

- **EntitySerializer no expone per-component**: revisar la API actual. Si solo expone entity-level, agregar `serializeComponent<T>(Entity) -> json` y `deserializeComponentInto<T>(Entity, json)`. Cero cambio al `.moodmap` schema — son helpers nuevos.

- **JSON de referencias** (assets): `MeshRendererComponent.materials[]` contiene `MaterialAssetId`s — números opacos que solo tienen sentido en el `AssetManager` del proyecto actual. Cross-entity dentro del mismo proyecto = OK (mismos IDs). Cross-proyecto sería un disaster. Reforzar D2 (no persistir) con un check al pegar: si el typeName tiene refs a assets y el proyecto activo cambió, invalidar.

- **Undo cross-component**: pegar como nuevo crea componente + setea fields. Es un solo command compuesto (`PasteAsNewComponentCommand`) que internamente combina `AddComponent` + N `SetField`. Diseñar bien para que el undo restaure el estado pre-paste (sin componente).

---

## Tamaño estimado

Hito chico-mediano (~3-4h). El grueso es:
- Wiring del menú contextual + serialize/deserialize per-component (probable existe parcialmente — confirmar).
- 2 comandos nuevos (`PasteComponentCommand`, `PasteAsNewComponentCommand`).
- Tests.

El UX es simple, el riesgo es la composición con multi-select (D D arriba).

## Cierre del hito

- [ ] Suite verde (+5-7 tests nuevos).
- [ ] Click derecho en header de Light → menu "Copiar valores" / "Pegar valores" / "Pegar como nuevo".
- [ ] Copy A → Paste B (mismo componente type) → B queda con los valores de A.
- [ ] Copy A → Paste como nuevo sobre C (sin ese componente) → C lo gana.
- [ ] Ctrl+Z restaura.
- [ ] Validación visual end-to-end.
- [ ] Tag `v2.9.0-fase3-hito9`.
- [ ] Update `ESTADO_ACTUAL.md`, `HITOS.md`, `DECISIONS.md`. Crear `PLAN_HITO_F3H10.md` (Undo coverage audit + fixes).
