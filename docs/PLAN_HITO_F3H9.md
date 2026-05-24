# PLAN F3H9 — Copy/Paste de components + EntityType model (Blender/Hammer-style)

**Estado:** **CERRADO** — `v2.9.0-fase3-hito9` (segundo hito de Sub-fase 3.2 "Inspector + Hierarchy pulidos").
**Predecesor:** F3H8 (multi-edit del Inspector — Light Tier 1).
**Origen:** Plan maestro [`PLAN_FASE3.md`](PLAN_FASE3.md) Sub-fase 3.2 — *"F3H9 — Copy/Paste de components"*.

**SCOPE EXPANDIDO** (decidido post-validación visual del dev del paste inicial):
El dev pidió **bundle**: copy/paste + modelo de **tipo de entidad** estilo Blender/Hammer + remake del Add Component popup + **rework Blender-style del Material Inspector** (bundle adicional decidido tarde — el dev pidió incorporarlo en F3H9 al ver la "lista infinita" de material slots en un mesh complejo). Razones documentadas en DECISIONS.md.

El paste solo (versión inicial entregada) ya funciona — pero "Pegar valores" via click-derecho del Inspector resultó poco intuitivo y reveló problemas más profundos: (a) "Quitar componente" de la luz base de una entity Light no tiene sentido (para borrar la luz, borrás la entity), (b) un NPC no debería poder tener Environment (componentes incompatibles con el tipo), (c) el popup de Add Component crece sin filtrado, (d) los material slots se rendereaban en lista vertical infinita (UX pobre en meshes complejos).

**Solución arquitectónica**: introducir `EntityType` (= Blender Object Type / Hammer entity class / Unreal Actor class) como atributo fijo de cada entity. El type define cuáles son los componentes BASE (no quitables) y filtra qué extensiones tienen sentido. La operación "cambiar de type" usa la modal `convert_entity_modal` existente desde F2H57 (rework dejado a F3H10+ — ver [[convert-modal-followup]]).

---

## Lo que el usuario obtiene (cerrado)

1. **Tipo de entidad explícito** — cada entidad tiene un `Tipo: …` visible arriba del Inspector. Spawneada con su type correcto (Light desde "+ Crear Entidad → Luz puntual", Vehicle al droppear un `.moodvehicle`, Brush al usar Brush Tool, etc).
2. **Componente base protegido** — "Quitar componente" del LightComponent en una entity Light está deshabilitado con tooltip explicando que para borrar la luz, borrás la entity. Tile (auto-gen) tiene TODO el Remove deshabilitado (read-only).
3. **Add Component filtrado por type** — la Luz solo te ofrece Script/RigidBody/AudioSource. El NPC NO te deja agregar Environment. El Vehículo NO te deja agregar Light. Generic (sin tipo) acepta todo.
4. **Popup Add Component con submenús** — cuando el search está vacío, las categorías son `BeginMenu` desplegables (Render/Physics/Audio/Logic/World). Cuando hay query, vuelve a lista plana (más rápido buscar).
5. **Copy/Paste de componente individual** (Tier 1: Light/Trigger/ForceField/ParticleEmitter) — click derecho en el header del componente en el Inspector → "Copiar valores" / "Pegar valores" / "Pegar como nuevo".
6. **Hierarchy "Copiar valores"** — click derecho sobre la entity en el panel Escena → "Copiar valores de <Tipo>" (lee el base component según EntityType). Tier 1 funciona; resto grisado con tooltip honesto "pendiente F3H10+".
7. **Material Inspector Blender-style** — un mesh con N materiales ya no rendea N paneles verticales. Lista compacta arriba (ListBox max 4 visibles), panel completo del slot seleccionado debajo (drop target + albedoTint/metallic/roughness/ao + Shader + Blending).
8. **Back-compat con `.moodmap` pre-F3H9** — el SceneLoader infiere el EntityType de los componentes presentes (Brush > NPC > Pickable > Environment > Light > Camera > ParticleEmitter > ForceField > Audio > Trigger > Vehicle > Mesh > Generic). Mapas viejos no rompen.

---

## Stages ejecutados

| # | Bloque | Estado |
|---|---|---|
| 1 | F3H9 core (paste components Tier 1 — clipboard + comandos + tests + Inspector context menu) | ✅ |
| 2 | `EntityType` enum + `EntityTypeTable` (toString/fromString, baseComponentKeys, isBaseComponent, inferFromEntity) + 17 tests | ✅ |
| 3 | Spawn paths: `entityType` seteado en handlers (Light/Environment kits + Brush + Boolean + Carve + Clip + PickModal + Drop mesh/pickable + Tile floor/grid) | ✅ |
| 4 | Persistencia: `EntitySerializer` escribe `"entity_type": "light"` + SceneLoader lee + inferFromEntity como fallback para pre-F3H9 | ✅ |
| 5 | Inspector: label "Tipo: X" arriba + "Quitar componente" disabled cuando es base / autogen (con tooltip) | ✅ |
| 6 | Inspector Add Component popup filtra por `canAddComponent(entType, componentKey)` (whitelist por type) | ✅ |
| 7 | Popup Add Component con submenús Unity-style (BeginMenu por categoría cuando search vacío; lista plana cuando search activo) | ✅ |
| 8 | Hierarchy "Copiar valores de <Tipo>" — toma el base component del EntityType; grisado con tooltip si no está en clipboard Tier 1 | ✅ |
| 9 | Material Inspector Blender-style (bundle agregado tarde — lista compacta + panel del seleccionado) | ✅ |
| Vehicle | `EntityType::Vehicle` + audit spawn handlers que quedaron sin taggear (Prefab via inferFromEntity / CreateEntity mesh-from-project / placeholder / Stress lights/cubes) | ✅ |

---

## Pendiente — diferido a F3H10+ (anotado en memorias [[convert-modal-followup]] y [[component-clipboard-expand]])

- **Ampliar kits del convert_entity_modal**: hoy solo NPC / Item / PointLight / DirLight. Faltan Brush, Mesh, Vehicle, Audio, Camera, Environment, ParticleEmitter, ForceField, Trigger solo. Decidido en F3H9 que el modal queda aditivo (no destructivo) por riesgo de undo.
- **Extender `ComponentClipboard` más allá de Tier 1**: MeshRenderer, AudioSource, Dialog, ItemPickup, Brush, Vehicle, Camera, Environment. Patrón claro (serialize + apply + isSupported + test) — placeholder honesto en UI mientras tanto ("pendiente F3H10+").
- **Componentes Tier 2 originales** (RigidBody / Joint / Vehicle / Ragdoll / Cloth / Script / Inventory / Animator) — diferidos como estaba previsto.

---

## Cierre del hito — checklist verificado

- [x] Suite verde (1164 cases / 11443 assertions — +17 EntityType + 2 Vehicle + 12 ComponentClipboard).
- [x] Spawn handlers auditados — cero entidades quedando como Generic salvo el placeholder explícito.
- [x] Inspector muestra Tipo + lock base + Add Component filtrado.
- [x] Popup Add Component con submenús cuando vacío + flat cuando hay search.
- [x] Hierarchy "Copiar valores" funciona Tier 1 + grisado honesto resto.
- [x] Material Inspector compacto en meshes con N materiales.
- [x] Validación visual end-to-end por el dev (Stages 5/6, Vehicle audit, Material UI, Hierarchy Stage 8).
- [x] Update `PLAN_HITO_F3H9.md` (este archivo), `ESTADO_ACTUAL.md`, `HITOS.md`, `DECISIONS.md`. Crear `PLAN_HITO_F3H10.md`.
- [ ] Tag `v2.9.0-fase3-hito9` (al confirmar push del dev).
