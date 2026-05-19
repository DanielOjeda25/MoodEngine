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

## 0.1. Último hito de feature — F2H67 (2026-05-19)

**Vehicle physics estilo GTA San Andreas.** Tag `v1.54.0-fase2-hito67`. Detalle completo en [`hitos/F2H67.md`](hitos/F2H67.md). **Cierra plan original F2H25** dentro de Sub-fase 2.4 (Física avanzada).

**Stack completo end-to-end**: `VehicleConfig` puro (header sin Jolt) con `makeDefaultSA()` baked GTA-SA-style (chassis 1500 kg con CoM bajo Y -0.20m, tracción alta lateral 1.4/longitudinal 1.6, motor 500 Nm @ 4000 RPM, 5 marchas + 1 reversa, brake 1500/handbrake 4000 Nm, steering 35° lerp 4/s, 4WD) → `VehicleComponent` + `VehicleSeatComponent` persistencia aditiva (configPath persiste; runtime handles + input no persisten; mapas pre-F2H67 cargan igual) → `PhysicsWorld_Vehicle.cpp` wrapper sobre `JPH::VehicleConstraint` + `WheeledVehicleController` con engine/transmission/differentials/4 wheels (createVehicle/setInput/readState con chassis + 4 wheel world matrices + speed + grounded + gear + RPM/applyImpulse) → `VehicleSystem` dedicado en `src/systems/physics/` hookeado en `EditorScene::tickPhysics` (materialize lazy con dirty flag + push input + sync poses chassis/4 wheels). **Ensamblaje multi-entity**: 1 entity chassis con `VehicleComponent` + 4 child entities con tags fijos `Wheel_FL/FR/RL/RR` auto-encontradas por tag al primer tick. **Sub-mesh selector nuevo** en `MeshRendererComponent.subMeshName` + `SubMesh.name` poblado desde `aiMesh->mName` por MeshLoader → las 5 entities comparten UN solo FBX (`sedan.fbx`) renderizando cada una su sub-mesh (`body`, `wheel-front-left/right`, `wheel-back-left/right`); las wheels rotan independiente del chassis con la suspensión + steering en runtime. **Lua API** `vehicle.set_input/get_speed/is_grounded/gear/rpm/respawn` lookup por tag. **Mount/dismount con F** (radio 3m): detección por `forEach<VehicleComponent, TransformComponent>` para encontrar el más cercano; mount switchea input dispatch (WASD → vehicle component) + activa chase cam; dismount teleporta player al costado del auto + reset input. **Chase cam orbital third-person**: reusa `FpsCamera` — mouse rota yaw/pitch, position re-derivada cada frame como `chassisPos - cam.forward() * 5m + (0, 1.5m, 0)`. **Asset `.moodvehicle` JSON aditivo**: nuevo type en AssetManager (slot 0 = `makeDefaultSA()` lazy-init, parsea campos opcionales con fallback a defaults SA, `vehicle::isValid()` chequea antes de cachear). **InspectorPanel_Vehicle** con InputText para configPath + display runtime + botón rematerializar. **Debug overlay 3D bajo F1**: OBB azul chassis + cruz verde por wheel + vector amarillo velocidad + líneas grises chassis→wheels. **Modelo CC0 shipado**: Kenney Car Kit `sedan.fbx` (129 KB) con 5 sub-meshes nombrados confirmados. **Demo project precargable** en `c:/tmp/MoodDemo_F2H67/` con `.moodproj`, mapa, vehicle assets, README con teclas. **Suite 1023/10203 verde** (+23 cases vs F2H66, +60 asserts).

---

## 0.2. Hito anterior — F2H66 (2026-05-18)

**Ragdolls auto-build sobre `JPH::Ragdoll`.** Tag `v1.53.0-fase2-hito66`. Detalle completo en [`hitos/F2H66.md`](hitos/F2H66.md). Cierra plan original F2H24 dentro de Sub-fase 2.4 (Física avanzada).

**Stack completo end-to-end**: `RagdollLayout` puro (header sin Jolt) que mapea esqueletos Mixamo (`mixamorig:*`) a 14 bodies con Hinge en codos/rodillas y SwingTwist en hombros/caderas/spine/cuello, masa por volumen del capsule normalizada a `totalMass` → `RagdollComponent` (state Animated/Ragdolling, totalMass, limbRadius, useGravity, spawnImpulse) persistencia aditiva en `.moodmap` (sin schema bump; `state` no persiste — siempre arranca Animated al cargar) → `PhysicsWorld_Ragdoll.cpp` con createRagdoll/destroy/readPose/applyImpulse sobre `JPH::Ragdoll` + `GroupFilterTable` para self-collision-off + `JPH::Skeleton` espejo para `Stabilize()` → `RagdollSystem` dedicado en `src/systems/physics/` (transición Animated→Ragdolling: build layout + skinning→meshGlobals→worldXforms + createRagdoll + impulse al torso; sync runtime: readRagdollPose → mesh-space → skinning con `inverseBind`, bones no-ragdolleados heredan del parent más cercano) → Lua `ragdoll.enable("Tag", {x,y,z})` + `ragdoll.is_ragdolling("Tag")` patrón HL2 (no vuelve a animation; recargar mapa = reset) → debug overlay 3D bajo F1 (capsules naranjas wireframe + líneas amarillas parent→child para jerarquía de constraints) → sample mapa `assets/maps/ragdoll_demo.moodmap` con NPC Mixamo + RagdollTrigger + script Lua que llama `ragdoll.enable` al entrar el player. **Polish crítico post-validación**: race fix en `AnimationSystem` — antes pisaba `skel.skinningMatrices` cada frame incluso con `playing=false` (la pose congelada se sigue evaluando), pisando al `RagdollSystem`. Fix: guard al inicio del lambda que skipea entities con `state == Ragdolling`. Sin este fix, el ragdoll se creaba en Jolt pero el mesh seguía visualmente en idle. **Split preparatorio `PhysicsWorld.cpp`** (850→611 LOC) en 4 archivos: core + Constraints + Ragdoll + Internal PIMPL. **Suite 1000/10092 verde** (+13 cases, +86 asserts).

---

## 0.2. Hito anterior — F2H65 (2026-05-18)

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

- **Sub-fase 2.4** (Física avanzada): **F2H65 + F2H66 + F2H67** cerrados (Hinge/Distance/Point + Ragdolls Mixamo + Vehicle physics SA). Pendiente del plan original: force fields (F2H26), triggers avanzados (F2H27), cloth/soft body (F2H28), Slider/Fixed joints.
- **Sub-fase 2.6** (Render polish): F2H55, F2H56, F2H58, F2H59, F2H60, F2H61, F2H62, F2H63, F2H64 cerrados. AUDIT-1, AUDIT-2, AUDIT-3 cerrados.

### Próximo

**Decisión del dev (2026-05-18):** cerrar lo que queda del **plan original** (`PLAN_FASE2.md`) antes de atacar follow-ups. Los 3 follow-ups de F2H64 quedaron archivados en [BACKLOG.md §1.-3/-2/-1](BACKLOG.md).

Sub-fases pendientes:

- **Sub-fase 2.4 — Física avanzada** (continúa post-F2H67): siguiente candidato del plan original = **F2H68 — Force fields y zonas físicas** (plan F2H26 — volúmenes que aplican fuerza: viento, gravedad alterada, buoyancy básica, explosiones puntuales). Alternativa: **F2H69 — Triggers avanzados** (plan F2H27 — shapes adicionales sphere/capsule/mesh, filtros por tag/layer, eventos extendidos) o **F2H70 — Cloth + soft body** (plan F2H28 — banderas, capas). **Slider / Fixed joints** quedan para un sub-hito chico si emerge presión.
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
2. Leer el plan del hito en curso: `docs/PLAN_HITO_F2H<N>.md` (actualmente F2H63).
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
