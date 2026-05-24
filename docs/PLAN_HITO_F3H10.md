# PLAN F3H10 — Ampliar `ComponentClipboard` + kits del convert_entity_modal

**Estado:** Planeado (tercer hito de Sub-fase 3.2 "Inspector + Hierarchy pulidos", arranca tras `v2.9.0-fase3-hito9`).
**Predecesor:** F3H9 (EntityType model + paste Tier 1 + popup remake + Material Inspector Blender-style).
**Origen:** dos backlogs explícitamente diferidos por el dev en F3H9 (anotados en memorias `project_convert_modal_followup` + `project_component_clipboard_expand`):
- El Hierarchy "Copiar valores" muestra grisado con tooltip "pendiente F3H10+" en types no-Tier-1 (Mesh/Audio/Brush/Vehicle/Camera/Environment + bases compuestos Dialog/ItemPickup).
- El `convert_entity_modal` solo tiene 4 kits (NPC / Item / PointLight / DirLight) — faltan el resto de EntityTypes.

---

## Qué siente el usuario

**Hoy (post-F3H9):**
- El dev tiene un AudioSource con todos sus parámetros tuneados (volumen, pitch, rolloff). Quiere copiar EXACTAMENTE eso a otra entity con audio. Click derecho en Hierarchy → "Copiar valores de AudioSource" → **grisado**. Tooltip dice "pendiente F3H10+". El dev tipea los 4 valores a mano. Frustración.
- El dev tiene un cubo placeholder y quiere convertirlo en un Brush. Click derecho → "Cambiar tipo de entidad…" → modal solo le ofrece NPC/Item/PointLight/DirLight. Brush no está. El dev tiene que borrar el cubo y crear un brush nuevo. Workflow roto.

**Post-F3H10:**
- Click derecho en cualquier entity → "Copiar valores de X" funciona para los 8 types adicionales. Paste cross-entity preserva config.
- Modal "Cambiar tipo" lista TODOS los EntityType disponibles (incluido Brush, Mesh, Vehicle, Audio, Camera, Environment, ParticleEmitter, ForceField, Trigger solo). Cada kit agrega el base + setea el `entityType` correcto.

---

## Realidad técnica (qué sí / qué no)

**Sí en F3H10:**

### Parte A — Extender `ComponentClipboard` a 8 types más

Patrón establecido en F3H9 (4 funciones tocar por type) → escala linealmente:

| componentKey | Componente | Riesgo |
|---|---|---|
| `mesh_renderer` | `MeshRendererComponent` | Bajo — material refs son `MaterialAssetId` (números, válidos cross-entity en mismo proyecto). Skinned skipea. |
| `audio_source` | `AudioSourceComponent` | Bajo — leaf simple (volume / pitch / clipPath). |
| `dialog` | `DialogComponent` | Bajo — `dialogPath` (string) + flags. |
| `item_pickup` | `ItemPickupComponent` | Bajo — `itemPath` (string) + quantity + flag. |
| `brush` | `BrushComponent` | Medio — faces tienen `materialIndex` que apunta al array de materials del owner. Si destino tiene distinta lista, los índices pueden quedar fuera de rango. Doc-only warning. |
| `vehicle` | `VehicleComponent` | Bajo — `configPath` + `dirty=true`. El reload del config viene del path. |
| `camera` | `CameraComponent` | Bajo — fov/near/far. Stub hoy pero serializable. |
| `environment` | `EnvironmentComponent` | Bajo — skyboxPath + fog + tonemap. F2H86 ya lo persiste. |

Para cada uno: agregar key const + `supportedKeys()` set + `componentNameKey()` branch + `entityHasComponent()` branch + `applyPayload()` branch + applier privado + test case. Aprovechar `serializeEntityToJson` + `parseEntityFromJson` para reusar el schema sin reescribir serialización.

### Parte B — Ampliar kits del `convert_entity_modal`

Modal hoy en `renderConvertEntityModal()` (F2H57 + F3H9 patch) ofrece 4 kits con `kitButton(label, desc, alreadyHas)`. Pattern repetible — agregar 9 kits nuevos:

| Kit | Componente(s) base | EntityType resultado |
|---|---|---|
| Brush vacío | `BrushComponent` | `Brush` |
| Mesh (drop después) | `MeshRendererComponent` con `missingMeshId()` | `Mesh` |
| Vehicle stub | `VehicleComponent` con `configPath` vacío | `Vehicle` |
| Fuente de audio | `AudioSourceComponent` | `Audio` |
| Cámara | `CameraComponent` | `Camera` |
| Entorno | `EnvironmentComponent` | `Environment` |
| Emisor de partículas | `ParticleEmitterComponent` | `ParticleEmitter` |
| Campo de fuerza | `ForceFieldComponent` | `ForceField` |
| Trigger solo | `TriggerComponent` | `Trigger` |

**Comportamiento sigue siendo aditivo** (no destructivo) — decisión D1 abajo.

### Parte C — Tests

- 8 test cases nuevos en `test_component_clipboard.cpp` (uno por type Tier 2): serialize → apply → roundtrip preserva.
- Smoke test del convert_modal: aplicar cada kit y assertear que el `entityType` queda seteado + el base component existe.

**NO en F3H10:**

- **Rework destructivo del convert_modal** (borrar bases viejos al cambiar de type) — decidido descartar en F3H9 por riesgo de undo. Backlog futuro si emerge fricción real.
- **Renombrar "Cambiar tipo" a "Aplicar kit"** — la opción descartada en F3H9 sigue descartada (el dev mantuvo el nombre actual).
- **Tier 3 components** (RigidBody / Joint / Ragdoll / Cloth / Script / Inventory / Animator) — assets-deep o ecosystem-heavy, scope distinto. Quedan pending para más adelante.
- **Atajos de teclado** Ctrl+C / Ctrl+V — siguen reservados para futuro entity-clipboard (copy/paste de entities enteras).

---

## Bloques

### A — Extender `ComponentClipboard` (8 types)

- 1 commit por type (8 commits chicos) o 1 commit grande con los 8. Decisión al implementar — probable los 8 juntos porque el patrón es repetitivo y el risk es bajo.
- Validar con la UI: el "Copiar valores" del Hierarchy debería automáticamente dejar de grisar esos types (el helper consulta `isSupported`).

### B — Brush con caveat de material indices

- Agregar warning en la doc + tooltip en el menu "Copiar valores de Brush" cuando el destino tiene distinta lista de materials. UX: si materialIndex > destination.materials.size() en cualquier face, clampear a 0 + log warning. Trade-off: paste no falla silenciosamente, pero las caras puede que queden con material default.

### C — Ampliar kits del convert_modal

- 9 kits nuevos siguiendo el patrón existente. i18n keys gemelas (`editor.convert_modal.kit.<x>` + `.<x>_desc`).
- Verificar que cada kit setea correctamente el `entityType` post-add (mismo patrón que los 4 actuales).
- Smoke test del convert: aplicar cada kit → assertear componente base presente + `entityType` correcto.

### D — Validación visual end-to-end

- Probar copy/paste para los 8 types nuevos: copiar AudioSource → paste sobre otra entity con audio (homogeniza), paste sobre entity sin audio (paste-as-new agrega).
- Probar los 9 kits nuevos: cubo placeholder → convertir a Brush, Mesh, Vehicle, Audio, Camera, Environment, ParticleEmitter, ForceField, Trigger.
- Verificar que `inferFromEntity` para los entities recién kit-aplicados devuelve el type correcto.

---

## Decisiones (pre-implementación)

### D1 — Mantener convert_modal aditivo (no destructivo) — confirmar

**Contexto:** El convert_modal hoy AGREGA componentes sin borrar los viejos. Si convertís Luz → NPC, queda Luz + NPC mezclados (el `entityType` cambia a NPC pero el LightComponent sigue colgando).

**Decisión propuesta:** confirmar lo que F3H9 decidió — **dejarlo aditivo**. Razones:
- Undo destructivo es riesgoso — borrar un componente con todos sus tuneados sin que el dev pueda recuperar fácil es mala UX.
- El popup "Add Component" filtrado por type ya impide combinaciones absurdas para entidades NUEVAS. Las viejas mezcladas son responsabilidad del dev.
- Si el dev quiere "limpiar" después de un convert, hace click derecho en los componentes que no le sirven y los borra a mano (con undo individual).

**Revisar si:** emerge demanda real del dev por behavior destructivo. Probable F3H11+.

### D2 — Brush + Vehicle paste cross-project — invalidar

**Contexto:** Brush refs material indices del owner. Vehicle ref `configPath` (string). En cross-entity dentro del mismo proyecto, ambos OK. Cross-project (clipboard persiste entre proyectos): material indices apuntarían a materials que no existen en el nuevo proyecto.

**Decisión propuesta:** mantener el invariante de F3H9 (D2 de aquel hito) — clipboard NO persiste cross-project. Cerrar proyecto → clipboard vacío. Por ahora, paste mismo-proyecto solamente.

### D3 — Test smoke del convert modal: unitario o end-to-end

**Contexto:** F3H9 no tiene tests del convert_modal (es UI). ¿Vale meter smoke tests headless en F3H10?

**Decisión propuesta:** **unitario sobre las lambdas de kit** — extraer cada lambda de kit a una función nombrada (`applyNpcKit(Entity)`, `applyBrushKit(Entity)`, etc) y testear que post-call el `entityType` y el componente base son los esperados. Skip de la parte ImGui (no testeable headless sin ImGui::TestEngine).

---

## Tamaño estimado

**Mediano** (~3-5h). Distribución:
- Parte A (clipboard 8 types): 1.5-2h. Patrón mecánico, riesgo bajo.
- Parte B (Brush caveat): 30 min. Solo doc/tooltip + clamp.
- Parte C (9 kits convert_modal): 1-1.5h. Patrón existente, copy-paste con tuning.
- Parte D (validación visual + tests): 1h.

---

## Cierre del hito

- [ ] Suite verde (+8 tests clipboard + 9 tests convert_modal kits).
- [ ] Hierarchy "Copiar valores" deja de grisar para los 8 types nuevos.
- [ ] convert_modal muestra TODOS los EntityType disponibles + cada kit aplica el componente base + setea el `entityType` correcto.
- [ ] Validación visual end-to-end por el dev.
- [ ] Tag `v2.10.0-fase3-hito10`.
- [ ] Update `ESTADO_ACTUAL.md`, `HITOS.md`, `DECISIONS.md`. Crear `PLAN_HITO_F3H11.md` cuando se decida el próximo bloque de Sub-fase 3.2 (probable: Undo coverage audit del Inspector, F3H11-F3H13 según `PLAN_FASE3.md`).
