# Estado actual del proyecto — handoff para el agente

> **Propósito:** dar al próximo agente el estado exacto del proyecto, comandos que sabemos que funcionan, y el siguiente paso concreto. Leer esto **antes** de `MOODENGINE_CONTEXTO_TECNICO.md`.
>
> **Convención de tamaño** (AUDIT-1, 2026-05-17): este documento mantiene SOLO el último hito cerrado + último audit + pendientes. Los entries históricos viven en [`hitos/F2H<N>.md`](hitos/) y en el git history (tags). No se acumulan "0bis" acá.

---

## 0. Último cierre — AUDIT-3 (2026-05-17)

**Cierre de la tanda inicial de audits.** Tag `v1.49.3-audit-3`. Reporte completo en [`audits/AUDIT_3.md`](audits/AUDIT_3.md).

**Resumen consolidado de la tanda (AUDIT-1 + 2 + 3, ~10h totales):**

```
                      AUDIT-1   AUDIT-2   AUDIT-3
HARD cap violations:    5         2         0
Layer violations:       —         2         0
Pure helpers extracted: 0         0         1 + 7 tests
```

**Qué entregó AUDIT-3:**

- **Layer fixes mecánicos**: `Workspace` struct movido a `engine/project/`, `i18n` a `core/i18n/`. 55 archivos updateados. 0 violaciones cross-layer ahora.
- **Split `tests/test_scene_serializer.cpp`** 934 → 302 LOC (core) + 210 (lighting/physics) + 399 (gameplay) + 39 (helpers). Patrón `_helpers.h` para fixtures de test compartidas.
- **Pure helper `pickRayFromNdc`** + 7 unit tests headless (`core/math/Ray.h` + `tests/test_ray.cpp`). 5 call sites refactorizados, ~42 LOC de duplicación eliminada.
- **Split `EditorUI.h`** 836 → 483 LOC (-42%). Inline bodies movidos a 4 archivos `EditorUI_<Family>.inl` por categoría (Spawn / Tools / Entity / Project). HARD cap = 0.

**Próximo audit**: solo si emerge dolor concreto. No agendar audits vacíos por cumplir la cadencia.

---

## 0.1. Último hito de feature — F2H70.3 (2026-05-20)

**Vehicle Browser + drag-and-drop de vehículos al viewport.** Tag `v1.60.0-fase2-hito70-3`. Detalle completo en [`hitos/F2H70-3.md`](hitos/F2H70-3.md). Cierra el loop de autoría del sistema data-driven `.moodvehicle`.

**Lo que entregó (Bloque F)**:
- **Tab "Vehiculos" en el Asset Browser**: `VehicleEntry` + scan recursivo de `assets/vehicles/**/*.moodvehicle`. Metadata legible (`metadata.name` + `body.mass_kg` + `engine.horsepower`) parseada del JSON al escanear → muestra nombre + `[kg, HP]`. Drag-source emite payload `MOOD_VEHICLE_ASSET` (path lógico, buffer 256B null-terminated).
- **Drag-and-drop al viewport** (consistente con mesh/prefab/script/item): `ViewportPanel` acepta el payload → `m_pendingVehicleDrop`; `EditorApplication::processViewportVehicleDrop()` hace `pickTile` + crea entity con `TransformComponent` + `MeshRendererComponent` (mesh de `VehicleConfig.meshPath`) + `VehicleComponent(configPath, dirty=true)` + undo.
- **`.moodvehicle` self-contained** (estilo Source): `VehicleConfig.meshPath` (de `body.mesh_path`) → arrastrar un solo `.moodvehicle` basta para un auto completo. `VehicleConfig.displayName` (de `metadata.name`) → tag legible del entity ("DeLorean DMC-12" en vez del filename stem).
- **Drop dedicado en el Inspector** (vía secundaria, reasignar config): un `InputText` no funciona como `BeginDragDropTarget` (consume el drag); se usa un botón "Soltar .moodvehicle aquí", patrón de `InspectorPanel_Animation`.
- **Grounding del spawn**: el drop replica `SceneLoader` (`tf.pivotYOffset = VehicleSystem::chassisRenderYOffset(e, assets)` + `pivotYawOffsetDeg = meshYawOffsetDeg`). Sin esto el auto atravesaba el piso en Editor mode.

**Aclarado en validación (no-bugs)**: la colisión de vehículos solo se simula en Play mode (el engine solo hace `physicsWorld->step()` en Play; **F1** solo togglea debug-draw de AABBs, no habilita colisión). Copias spawneadas antes del hito (prefijo `Vehicle_`) que quedaron guardadas en un `.moodmap` conservan ese tag; drops nuevos usan `metadata.name`.

**Pendiente** (a retomar, no scope ahora): **split-by-node de wheels** (rotación visual independiente + auto-derivar `attach_y`) + **pipeline de packs multi-auto** — el dev tiene un archivo con 24 autos estilo DeLorean a escala ~1.4cm (autoreados en otra unidad pese a `scale=1`); construir `tools/glb/split.py` (separar por nodo) + `--scale` para hornear 1u=1m → 24 `.glb` + 24 `.moodvehicle`.

---

## 0.2. Hito previo — F2H70.2 (2026-05-20)

**Tuning físico del vehicle (damping + spawn elevation + frame consistente).** Tag `v1.59.0-fase2-hito70-2`. Detalle completo en [`hitos/F2H70-2.md`](hitos/F2H70-2.md). Cierra los 3 bugs físicos que F2H70.1 dejó pendientes.

**Lo que entregó**:
- **Bloque C — chassis damping configurable**: pre-fix el chassis estaba hardcoded a `mLinearDamping=0.05` (sim-floppy, rodaba infinito al soltar W). `chassisLinearDamping` + `chassisAngularDamping` (default 0.3 arcade) leídos desde `body.linear_damping` / `angular_damping` en el `.moodvehicle`, aplicados a `JPH::BodyCreationSettings`.
- **Bloque B — spawn elevation spring-aware**: helper `wheelRestCompression(WheelConfig) = g/(2π·f)²` (mass-independent) sumado al `pivotYOffset` en `VehicleSystem::chassisRenderYOffset`. `SceneLoader` usa el helper centralizado (aplica también en Editor mode). Resuelve el brinco visual al iniciar Play.
- **Bloque D5 — mesh yaw offset data-driven**: `body.mesh_yaw_offset_deg` (número) o `body.mesh_forward_axis` (`"+Z"/"-Z"/"+X"/"-X"`) en el `.moodvehicle`. `TransformComponent::pivotYawOffsetDeg` runtime-only post-multiply `Ry` (visual-only, no afecta física). DeLorean declara `mesh_yaw_offset_deg: 180` → **moodmap limpio** (workaround `rotationEuler: [0,180,0]` de F2H70.1 eliminado). Drag-and-drop friendly: subís .glb, si controles salen invertidos editás un campo.
- **Bloque E — S-key brake-stick + HUD/logs**: edge-detect en S decide brake-vs-reverse UNA vez al press (patrón GTA/Forza). HUD polish + debug logs de edges/speed.
- **Grounding final**: el spring-aware arregló el brinco pero no el float en equilibrio (chassis center a 0.85m, modelo half-height 0.568m → base flotaba 0.285m). Fix data-driven `attach_y_mm: -300 → -15` (`-(half_height - spring_rest - radius)`) en ambos ejes. Validado en Editor mode: DeLorean apoya en piso.

**Suite 1029/10227 verde**. **Pendiente a F2H70.3**: Vehicle Browser UI (Bloque F) + split-by-node de wheels (Bloque H, permitiría auto-derivar `attach_y`).

---

## 0.3. Hito previo — F2H70.1 (2026-05-20)

**Sistema data-driven de vehículos (estilo Source/Valve).** Tag `v1.58.0-fase2-hito70-1`. Detalle completo en [`hitos/F2H70.md`](hitos/F2H70.md). Plan archivado en [`archive/plans/PLAN_HITO_F2H70.md`](archive/plans/PLAN_HITO_F2H70.md).

**Pivot mid-hito**: el plan inicial era "fix 3 bugs sistémicos del VehicleSystem". El dev objetó verbatim *"qué pasa si mañana yo agrego 10 autos más? tener los valores hardcodeados, no lo veo realmente viable"* cuando propuse hardcodear specs reales del DMC-12 en `makeDefaultSA()`. Pivotamos a sistema data-driven completo estilo Source Engine (`scripts/vehicles/<car>.txt`).

**Lo que entregó**:

- **Bloque A — auto-spawn-height vía `TransformComponent.pivotYOffset`**: runtime-only field; `worldMatrix()` aplica `position.y + pivotYOffset` antes del translate. `VehicleSystem` materialize lazy + `SceneLoader::applyOneEntity` (también Editor mode sin Play) setean `tf.pivotYOffset = -mesh->aabbMin.y`. Post-tick resta el offset antes de escribir al TC → `tf.position` queda "raw" (lo que el dev escribe en moodmap), `worldMatrix()` lo re-aplica transparente. Patrón Unity/Unreal.
- **Bloque B — quat sync sin gimbal**: `glm::quat rotation` + `bool useQuaternion` en TC. `VehicleSystem::writeWorldMatrixToTransform` extrae quat con `glm::quat_cast(mat3(world))` sin pasar por euler intermedio → resuelve gimbal lock cerca de rotaciones singulares (180° en Y). `InspectorPanel_Transform` refresca euler desde quat para display; edit manual resetea `useQuaternion=false`.
- **Bloque C — Schema `.moodvehicle` v2 axle-based estilo Source**: `body / axle_front / axle_rear / engine / brakes / steering` con units dev-friendly (mm, kg, HP, Nm @ RPM) — conversión interna al SI. Axles expandidos a wheels FL+FR / RL+RR con `attachLocal` derivado de `offset_z_mm + ±track_mm/2`. Presets `transmission: "5-speed-manual" / "4-speed-auto" / "6-speed-manual"` cargan gear ratios típicos. `brakes.deceleration_target_mps2` deriva brakeTorque via física. `schemaVersion: 2` triggea el nuevo parser; v1 sigue funcionando.
- **Bloque D — Convenciones de assets + `tools/glb/` versionado**: `docs/asset_conventions.md` (1u=1m, +Z forward glTF, origin libre absorbido por engine, sin det<0, naming `wheel_FL/FR/RL/RR`). 6 scripts genéricos en `tools/glb/` (scale/flatten/center_y/reorient/verify/diag) promocionados desde `c:/tmp/` con argparse + common.py compartido + README pipeline. `reorient.py` nuevo (rota yaw + hornea posiciones + normales).
- **Bloque E — `assets/vehicles/delorean/delorean_dmc12.moodvehicle`** con specs reales DMC-12 stock (1230 kg, 130 HP, 208 Nm @ 2750 rpm, redline 5500, 5-speed manual + reverse, RWD, dimensions [1853, 1136, 4220] mm = match exacto AABB del modelo, track 1588mm front/rear, wheelbase 2410mm).
- **Bloque G — Rename `makeDefaultSA() → makeFallbackGenericSedan()`** (9 archivos, 47 occurrences). Sentinel `__default_vehicle_sa → __fallback_generic_sedan`. Refleja el rol real: fallback genérico con warn log cuando se usa, no "default DeLorean-specific". Specs reales viven en el `.moodvehicle`, no en C++.

**Validación visual**: hitbox del chassis Jolt ahora encaja con el modelo (mismas dimensiones), wheels físicas alineadas con las visuales del .glb. Moodmap demo extendido a camino largo 60m × 8m con NPC al final del recorrido.

**Issues residuales agendados a F2H70.2** (no son scope del schema parsing): (1) **frame inverso post-reorient** — cuando reorienté el .glb a +Z forward, W/A/D quedaron invertidos (cámara FPS del seat mount asume convención vieja). Workaround pragmático: revertí el .glb + uso `rotationEuler: [0, 180, 0]` en moodmap del DeLorean. (2) **chassis flota 7-10cm** sobre el piso por Jolt spring settle del spawn — fix: `pivotYOffset` debe considerar `wheel_radius + spring_rest_length`, no solo `-aabbMin.y`. (3) **momentum indefinido** sin acelerador — el schema v2 no expone `chassis.linear_damping` ni `angular_damping`; agregar y propagar a `JPH::BodyCreationSettings`.

**Lección operacional crítica** (memoria `feedback-build-validar-siempre`): durante la sesión el dev abrió `MoodEditor.exe` Release con timestamp del **10 de mayo** (9 días antes de los commits de Bloques A+B). Cuando reportó "modelos enormes + sin texturas", asumí regresión de mis cambios; en realidad el binario ni los contenía. Diagnóstico llevó 30+ minutos hasta comparar timestamps. **Tests verdes ≠ feature funcionando**: cualquier cambio C++ debe ir seguido de rebuild + abrir editor + validar visual antes de marcar bloque completo.

**Suite 1029/10227 verde** (sin tests nuevos — el refactor es backward-compat con v1 + integration test coverage indirecto via VehicleConfig validator).

---

## 0.4. Hito previo — F2H69 (2026-05-19)

**Trigger NPC debug + pipeline glTF multi-node + DeLorean swap.** Tag `v1.57.0-fase2-hito69`. Detalle completo en [`hitos/F2H69.md`](hitos/F2H69.md). Cierra el bug conocido que F2H68 dejó abierto.

**Lo que entregó**: **Bloque A — fix sensor sleeping**: `JPH::Body::IsSensor()` auto-fuerza `mAllowSleeping=false` solo para bodies Dynamic. Los sensores Kinematic (caso del NPC sensor del demo) heredan default `true` y entran a sleep tras ~5s sin movimiento → Jolt no emite `OnContactAdded` para sensores dormidos (optimización broadphase). Fix en `PhysicsWorld::createBody`: si `isSensor==true` force `mAllowSleeping=false` independiente del MotionType (pattern Unity/Unreal para triggers permanentes). `impactSpeedThreshold` default 4.0→1.0 m/s para feel arcade-ish coherente con el tuning `makeDefaultSA()` (atropellar caminando funciona). **Bloque B — cleanup logs**: removidos los `[F2H69-DEBUG]` post-validación de `OnContactAdded` (sub-threshold sensor + ENCOLA) y `RagdollSystem::tick` (drain + per-event traces + TRANSITION). **Bloque C — pipeline glTF multi-node (consolidation)**: `MeshLoader.cpp` para `.gltf`/`.glb` aplica las node transforms acumuladas root→owner a vertex position + normal (`aiMatrix3x3` 3x3 con renormalize) + recalcula AABB local; FBX intacto (convención Kenney/Mixamo trae vertices baked al world). **Sin split-by-node**: consolidation only (1 MeshAsset con vertices pre-transformados), split real diferido a F2H70 si emerge demanda. **Bloque D — DeLorean swap**: deletes `assets/dmc_delorean/*` (Sketchfab v1 multi-archivo) + `assets/vehicles/banshee_sa/*` (Kenney F2H67; tuning SA-style queda en C++ `makeDefaultSA()`). Nuevo `assets/vehicles/delorean/delorean.glb` (293 KB single-file con texturas embedded) procesado headless con scripts `pygltflib` (no Blender): scale x1.86 (modelo 2.27m → 4.22m real DMC-12), flatten de matrices al vertex data + reverse winding/flip normales en 7 de 17 nodos con `det(M)<0` (mirror baked del export 3DS Max), origin centrado en su eje Y. Verificación headless walking del árbol confirma **4.220 × 1.853 × 1.136 m** (match exacto DeLorean real). `vehicle_demo.moodmap`: `configPath: ""` → fallback automático a `makeDefaultSA()` (drop del `.moodvehicle` redundante). **Lateral**: título de ventana del editor limpiado a `"MoodEngine Editor"` (sin `"v0.6.0-dev (Hito 6)"` que confundía). **Suite 1029/10227 verde** (sin tests nuevos — F2H69 es mostly fix runtime + asset integration).

**Pendientes diferidos a F2H70** (3 bugs sistémicos del VehicleSystem identificados al validar visualmente el DeLorean): (1) **auto-spawn-height** — engine debe consultar AABB del mesh en `VehicleSystem::tick` para offset Y en vez de `position.y` ajustado asset-specific en moodmap. (2) **quat sync sin gimbal** — `extractEulerAngleXYZ` sufre gimbal lock con rotaciones 180°+; fix con quaternion opcional en `TransformComponent` o `worldMatrix` cached. (3) **split-by-node de wheels** — `MeshLoader` opt-in `splitByNode` flag + wheel meshes con nombres canónicos (`wheel_FL` / `_FR` / `_RL` / `_RR`) que `VehicleSystem` sincronice con `st.wheelWorlds[i]` para rotación visual independiente del chassis. Cuando F2H70 cierre, salen del moodmap `position.y=0.568` y `rotationEuler=[0,180,0]`. El dev explicitó durante la sesión que **workarounds asset-specific = bug del engine, no del asset** — los modelos deben drop-in si cumplen convención industrial (1u=1m, origin centrado, sin mirrors baked, facing forward).

---

## 0.5. Hito previo — F2H68 (2026-05-19)

**Auto-ragdoll por impacto (infra completa).** Tag `v1.55.0-fase2-hito68`. Detalle completo en [`hitos/F2H68.md`](hitos/F2H68.md).

Stack completo de auto-ragdoll por contacto: `physics_internal::ContactListener` registrado en `PhysicsWorld::Impl` + mapa `BodyID → entt::entity` mantenido por PhysicsSystem (RigidBody) / VehicleSystem (chassis) / RagdollSystem (parts) con auto-cleanup en destroy. `OnContactAdded` calcula `closingSpeed = vrel.Dot(normal)`, si > threshold encola evento `{victimBodyId, ±normal*impulseMag, closingSpeed}` con mutex. `RagdollSystem::tick` drena la cola pre-materialize: si la víctima tiene `RagdollComponent::Animated`, transiciona a Ragdolling con spawnImpulse. **Sensor bodies** (Unity `isTrigger` pattern): `RigidBodyComponent::isSensor` aditivo serializado como `is_sensor: true` → `JPH::BodyCreationSettings::mIsSensor`. Banshee tuning SA-style: maxTorque 500→800 Nm, brakeTorque 1500→4500 Nm, handbrakeTorque 4000→6000. 6 tests headless del API. Vehicle render: 1 entity por mesh-part (sin sub-mesh selector). **Bug conocido cerrado en F2H69**: el NPC sensor del demo no transicionaba — era el sleeping de sensores Kinematic.

---

## 0.6. Hito previo — F2H67 (2026-05-19)

**Vehicle physics estilo GTA San Andreas.** Tag `v1.54.0-fase2-hito67`. Detalle completo en [`hitos/F2H67.md`](hitos/F2H67.md). **Cierra plan original F2H25** dentro de Sub-fase 2.4 (Física avanzada).

**Stack completo end-to-end**: `VehicleConfig` puro (header sin Jolt) con `makeDefaultSA()` baked GTA-SA-style (chassis 1500 kg con CoM bajo Y -0.20m, tracción alta lateral 1.4/longitudinal 1.6, motor 500 Nm @ 4000 RPM, 5 marchas + 1 reversa, brake 1500/handbrake 4000 Nm, steering 35° lerp 4/s, 4WD) → `VehicleComponent` + `VehicleSeatComponent` persistencia aditiva (configPath persiste; runtime handles + input no persisten; mapas pre-F2H67 cargan igual) → `PhysicsWorld_Vehicle.cpp` wrapper sobre `JPH::VehicleConstraint` + `WheeledVehicleController` con engine/transmission/differentials/4 wheels (createVehicle/setInput/readState con chassis + 4 wheel world matrices + speed + grounded + gear + RPM/applyImpulse) → `VehicleSystem` dedicado en `src/systems/physics/` hookeado en `EditorScene::tickPhysics` (materialize lazy con dirty flag + push input + sync poses chassis/4 wheels). **Ensamblaje multi-entity**: 1 entity chassis con `VehicleComponent` + 4 child entities con tags fijos `Wheel_FL/FR/RL/RR` auto-encontradas por tag al primer tick. **Sub-mesh selector nuevo** en `MeshRendererComponent.subMeshName` + `SubMesh.name` poblado desde `aiMesh->mName` por MeshLoader → las 5 entities comparten UN solo FBX (`sedan.fbx`) renderizando cada una su sub-mesh (`body`, `wheel-front-left/right`, `wheel-back-left/right`); las wheels rotan independiente del chassis con la suspensión + steering en runtime. **Lua API** `vehicle.set_input/get_speed/is_grounded/gear/rpm/respawn` lookup por tag. **Mount/dismount con F** (radio 3m): detección por `forEach<VehicleComponent, TransformComponent>` para encontrar el más cercano; mount switchea input dispatch (WASD → vehicle component) + activa chase cam; dismount teleporta player al costado del auto + reset input. **Chase cam orbital third-person**: reusa `FpsCamera` — mouse rota yaw/pitch, position re-derivada cada frame como `chassisPos - cam.forward() * 5m + (0, 1.5m, 0)`. **Asset `.moodvehicle` JSON aditivo**: nuevo type en AssetManager (slot 0 = `makeDefaultSA()` lazy-init, parsea campos opcionales con fallback a defaults SA, `vehicle::isValid()` chequea antes de cachear). **InspectorPanel_Vehicle** con InputText para configPath + display runtime + botón rematerializar. **Debug overlay 3D bajo F1**: OBB azul chassis + cruz verde por wheel + vector amarillo velocidad + líneas grises chassis→wheels. **Modelo CC0 shipado**: Kenney Car Kit `sedan.fbx` (129 KB) con 5 sub-meshes nombrados confirmados. **Demo project precargable** en `c:/tmp/MoodDemo_F2H67/` con `.moodproj`, mapa, vehicle assets, README con teclas. **Suite 1023/10203 verde** (+23 cases vs F2H66, +60 asserts).

---

## 0.7. Hito anterior — F2H66 (2026-05-18)

**Ragdolls auto-build sobre `JPH::Ragdoll`.** Tag `v1.53.0-fase2-hito66`. Detalle completo en [`hitos/F2H66.md`](hitos/F2H66.md). Cierra plan original F2H24 dentro de Sub-fase 2.4 (Física avanzada).

**Stack completo end-to-end**: `RagdollLayout` puro (header sin Jolt) que mapea esqueletos Mixamo (`mixamorig:*`) a 14 bodies con Hinge en codos/rodillas y SwingTwist en hombros/caderas/spine/cuello, masa por volumen del capsule normalizada a `totalMass` → `RagdollComponent` (state Animated/Ragdolling, totalMass, limbRadius, useGravity, spawnImpulse) persistencia aditiva en `.moodmap` (sin schema bump; `state` no persiste — siempre arranca Animated al cargar) → `PhysicsWorld_Ragdoll.cpp` con createRagdoll/destroy/readPose/applyImpulse sobre `JPH::Ragdoll` + `GroupFilterTable` para self-collision-off + `JPH::Skeleton` espejo para `Stabilize()` → `RagdollSystem` dedicado en `src/systems/physics/` (transición Animated→Ragdolling: build layout + skinning→meshGlobals→worldXforms + createRagdoll + impulse al torso; sync runtime: readRagdollPose → mesh-space → skinning con `inverseBind`, bones no-ragdolleados heredan del parent más cercano) → Lua `ragdoll.enable("Tag", {x,y,z})` + `ragdoll.is_ragdolling("Tag")` patrón HL2 (no vuelve a animation; recargar mapa = reset) → debug overlay 3D bajo F1 (capsules naranjas wireframe + líneas amarillas parent→child para jerarquía de constraints) → sample mapa `assets/maps/ragdoll_demo.moodmap` con NPC Mixamo + RagdollTrigger + script Lua que llama `ragdoll.enable` al entrar el player. **Polish crítico post-validación**: race fix en `AnimationSystem` — antes pisaba `skel.skinningMatrices` cada frame incluso con `playing=false` (la pose congelada se sigue evaluando), pisando al `RagdollSystem`. Fix: guard al inicio del lambda que skipea entities con `state == Ragdolling`. Sin este fix, el ragdoll se creaba en Jolt pero el mesh seguía visualmente en idle. **Split preparatorio `PhysicsWorld.cpp`** (850→611 LOC) en 4 archivos: core + Constraints + Ragdoll + Internal PIMPL. **Suite 1000/10092 verde** (+13 cases, +86 asserts).

---

## 0.8. Hito anterior — F2H65 (2026-05-18)

**Jolt constraints (Hinge / Distance / Point).** Tag `v1.52.0-fase2-hito65`. Detalle completo en [`hitos/F2H65.md`](hitos/F2H65.md). Abre Sub-fase 2.4 (Física avanzada) del plan original.

**Stack completo end-to-end**: `PhysicsWorld` API para los 3 tipos de constraint (Hinge/Distance/Point) → `JointComponent` con runtime cleanup + `dirty` flag → Inspector con combo type + drag-drop entity desde Hierarchy (payload `MOOD_ENTITY` reusable para futuros component-entity links) + DragFloat3 pivotLocal + campos condicionales por type → persistencia `.moodmap` aditiva (sin schema bump) con target body B referenciado por **TAG** (handles entt no son estables entre sesiones; patrón paths-no-ids del Animator/Inventory) → debug overlay 3D con anchor + línea pivot→target + flecha de eje para Hinge (colores por tipo bajo F1) → sample mapa `assets/maps/physics_joints_demo.moodmap` (puerta-péndulo Hinge + péndulo rígido Distance). **Bug fix relevante**: sentinel "sin target asignado" cambió de `0` a `kJointNoTarget = UINT32_MAX` porque raw `entt::entity{0}` es válido (primera entity creada en una scene fresca colisionaba con el sentinel). Suite 987/10006 verde (+7 cases, +68 asserts).

---

## 1. Estado del plan

### Sub-fases cerradas

- **Sub-fase 2.1** (cimientos): F2H1-F2H10. ✅ Tag `v1.1.7-fase2-hito10`.
- **Sub-fase 2.2** (CSG editor): F2H11-F2H22. ✅ Hammer-style cerrado.
- **Sub-fase 2.3** (Renderer) del plan original: Hito 17 (PBR) + F2H55 (bloom) + F2H56 (SSAO) + F2H60 (CSM) + F2H61 (SSR) + F2H62 (shader graph). ✅ **cerrada con AUDIT-1**.
- **Sub-fase 2.5** (gameplay loop): F2H43-F2H53 (i18n + dialogs + inventory + quests). ✅ Tag `v1.41.0-fase2-hito53`.

### En curso

- **Sub-fase 2.4** (Física avanzada): **F2H65 + F2H66 + F2H67 + F2H68** cerrados (Hinge/Distance/Point + Ragdolls Mixamo + Vehicle physics SA + ContactListener-driven auto-ragdoll infra). F2H68 cerrado con **bug conocido**: NPC sensor sample no transiciona end-to-end. Pendiente del plan original: force fields (F2H26), triggers avanzados (F2H27), cloth/soft body (F2H28), Slider/Fixed joints.
- **Sub-fase 2.6** (Render polish): F2H55, F2H56, F2H58, F2H59, F2H60, F2H61, F2H62, F2H63, F2H64 cerrados. AUDIT-1, AUDIT-2, AUDIT-3 cerrados.

### Próximo — F2H69 (next)

**F2H69 — Vehicle pipeline glTF multi-node + debug del trigger NPC + reemplazar sedan Kenney por modelo correctamente armado.** Acordado con el dev al cerrar F2H68 (2026-05-19). El dev pidió no reinventar la rueda; este hito ataca el bug conocido de F2H68 + entrega pipeline de modelos vehiculares al standard industry.

**Scope estimado**: 4-6h, ~3 bloques:

- **A — Debug del trigger NPC**: añadir logs temporales en `OnContactAdded` + drain en `RagdollSystem` para confirmar dónde se rompe la cadena. Hipótesis priorizadas: (1) callback no se invoca para Dynamic vs Sensor — fix: chequear `body.IsSensor()` flag y ajustar el filtro `IsDynamic()`. (2) `closingSpeed` mal proyectada — fix: log de los valores reales. (3) Timing del register tras `createBody` — fix: registrar ANTES de pushear contact desde el listener. (4) Orden de tick — fix: drain a inicio de `tick()` antes de cualquier scene.forEach.
- **B — Pipeline glTF multi-node**: extender `MeshLoader` para que detecte aiNodes con mesh children y opcionalmente genere `MeshAsset`s separados por node (cada uno centrado en su origen). Usado por el patrón estándar Unity/Unreal: 1 GLB → N entities (chassis + wheels + doors) cargadas como assets independientes. Sin sub-mesh selector. La elección "1 mesh por entity" del editor consume el FBX/GLB y deja al dev armar la entity tree.
- **C — Reemplazar sedan Kenney por modelo DCC-friendly**: usar el DeLorean GLB de Sketchfab (CC-BY, ya bajado a `assets/dmc_delorean/scene.gltf`) con escala correcta. Eliminar `assets/vehicles/banshee_sa/sedan.fbx` (el modelo Kenney compuesto incompatible con sub-mesh selector). El `.moodvehicle` y el `VehicleConfig::makeDefaultSA()` SE QUEDAN — son la "configuración" lógica del vehicle, no el asset visual.

**Sub-fases más adelante** (sin presión inmediata):

- **Sub-fase 2.4 — Física avanzada** (continúa post-F2H69): candidatos remanentes del plan original = **Force fields y zonas físicas** (F2H26), **Triggers avanzados** (F2H27), **Cloth + soft body** (F2H28), Slider/Fixed joints.
- **Sub-fase 2.6 — Pipeline AI** (F2H35-F2H40 originales): Mixamo importer cubierto parcialmente por F2H49. Pendiente: Blender MCP server, armas procedurales, generador de props, validación automática.
- **Sub-fase 2.7 — UI/UX final + cierre Fase 2** (F2H41-F2H44 originales): theming, atajos configurables, tutorial in-app, tag `v2.0.0`.

---

## 2. Entorno de build — lo que realmente funciona

### Toolchain real

- **Visual Studio 2022 Community** → MSVC 14.44 + SDK Windows 11 + CMake 3.31 bundleado. **Es el que usamos.**

Ruta clave:
```
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat
```

Cargar entorno MSVC desde PowerShell:
```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 && <tu_comando_aqui>'
```

Desde **Developer Command Prompt for VS 2022** del menú inicio, `cl` y `cmake` funcionan directamente.

### Versiones verificadas

- `cl` → `19.44.35226 for x64`
- `cmake` → `3.31.6-msvc6`
- `git` → `2.49.0.windows.1`

### Generador CMake

`Visual Studio 17 2022` (ya en `CMakePresets.json`).

---

## 3. Comandos que funcionan

Desde la raíz del repo, con entorno MSVC cargado:

```bash
# Configurar (descarga deps la primera vez, ~5 min)
cmake --preset windows-msvc-debug

# Compilar
cmake --build build/debug --config Debug

# Ejecutar
./build/debug/Debug/MoodEditor.exe

# Tests
./build/debug/Debug/mood_tests.exe

# Sizeometer (AUDIT-1)
tools/sizeometer.sh 30
```

---

## 4. Qué hacer al arrancar la próxima sesión

1. Leer este archivo entero (es chico — diseñado para eso).
2. Leer el plan del hito en curso: `docs/PLAN_HITO_F2H<N>.md` (próximo: F2H70 — vehicle engine fixes sistémicos).
3. `git status` + `git log --oneline -10` + `git tag --sort=-v:refname | head -5`.
4. Preguntar al dev: "¿seguimos con el hito en curso o pasó algo nuevo?"
5. Si arrancamos hito nuevo: refinar el plan stub con el dev → bloques A-X concretos.
6. Trabajar bloque por bloque, marcando todo en el plan + DECISIONS al aparecer.
7. Al cerrar: commit "feat(F2H<N> Bloque X)" + "docs(F2H<N> cierre)" + tag + actualizar HITOS.md (one-liner) + ESTADO_ACTUAL.md (reemplaza section 0.1, no acumular) + crear `docs/hitos/F2H<N>.md`.

---

## 5. Gotchas conocidos

1. **VS 2026 Community sin workload C++**: no usar. Solo VS 2022.
2. **SDL2 debug se llama `SDL2d.dll`** no `SDL2.dll`. POST_BUILD lo copia correcto.
3. **GLAD v2 naming**: header `<glad/gl.h>` (no `<glad/glad.h>`), source `gl.c`, loader `gladLoaderLoadGL()`.
4. **Primera config `cmake --preset` tarda ~5 min** (descarga deps SDL2/ImGui/spdlog/GLM). Incrementales son segundos.
5. **Shaders con path relativo `shaders/*.{vert,frag}`**. Funciona desde repo root y desde dir del exe (POST_BUILD copy_directory).
6. **Al editar shaders**: copiar al build dir si el exe ya está deployed (`build/debug/Debug/shaders/`). CMake solo copia en build.
7. **2 máquinas de dev**: notebook Iris Xe + desktop Ryzen 5600G + GTX 1660. En desktop forzar NVIDIA via Panel de Control.

---

## 6. Filosofía operacional (resumida)

- **Ship something**: no romper el build entre commits.
- **No implementar hitos futuros** "por adelantar trabajo".
- **Cero scope creep**: bug fixes reactivos OK, refactors planeados van a AUDIT-N.
- **Comentarios en español**, convención commits `tipo(scope): mensaje`.
- **Soft 500 / hard 800 LOC por archivo**. AUDIT detecta y splittea.
- **Preguntar al dev antes de asumir** ante ambigüedades.

---

## 7. Archivos clave por tarea

| Para... | Tocar |
|---|---|
| Añadir dependencia | `CMakeLists.txt` (bloque CPM) |
| Cambiar flags compilador | `cmake/CompilerWarnings.cmake` |
| Añadir panel del editor | `src/editor/panels/*` + `EditorUI.(h\|cpp)` |
| Registrar decisión arquitectónica | `docs/DECISIONS.md` (append-only) |
| Cerrar hito | `docs/HITOS.md` (one-liner) + `docs/hitos/F2H<N>.md` (detalle) |
| Tech debt audit | `tools/sizeometer.sh` + `docs/audits/AUDIT_N.md` |
| Cambiar log level | `src/core/Log.cpp` |

---

## 8. Histórico

Los entries de hitos anteriores a F2H62 viven inline en `docs/HITOS.md` con el formato "fat" original. Los detalles más viejos están en `docs/archive/`. El histórico completo se navega por `git log` + tags `v*.*.*`.
