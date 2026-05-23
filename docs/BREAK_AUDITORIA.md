# BREAK · Auditoría de integridad y salud de código (pre-Fase 3)

> **Fecha:** 2026-05-23 · **Alcance:** todo `src/` tras cierre de `v2.0.0` (86 hitos).
> **Tipo:** *break* de consolidación — no agrega features, salda deuda antes de planear Fase 3.
> **Regla del dev:** "limpieza y auditoría del código, mejores prácticas, nada de espagueti
> ni archivos enormes. Cuando esto esté resuelto, recién vemos la Fase 3."
>
> Este documento es **ejecutable por el agente**. Reúne dos auditorías:
> - **Parte A — Integridad (placebos):** features serializadas/expuestas que NO están
>   cableadas de punta a punta al runtime. Verificado: UI escribe → serializa simétrico →
>   runtime lee → tiene efecto.
> - **Parte B — Salud de código:** tamaños de archivo, funciones gigantes, duplicación,
>   inversión de capas, higiene.
>
> **Veredicto global:** integridad **alta** y código **sano y bien mantenido**. De ~60
> features auditadas hay **1 placebo crítico**; el resto son huecos de UI/save-game o
> deuda de mantenibilidad acotada. **Nada amerita reescritura.**

---

## Instrucción de arranque para el agente (copiar/pegar)

> Cerramos Fase 2 con `v2.0.0`. Antes de la Fase 3 hacemos un **break de consolidación**: no
> se agregan features, se salda deuda de integridad y de salud de código. Tu tarea es ejecutar
> este documento (`docs/BREAK_AUDITORIA.md`) completo.
>
> **Cómo trabajar:**
> 1. Leé el documento entero, en especial "Cómo ejecutar este break" y "Auditado y OK — NO TOCAR".
> 2. Ejecutá los items en el **orden de las 3 tandas** del final (Tanda 1 integridad → Tanda 2
>    salud → Tanda 3 cosmético). Dentro de cada tanda, un item por vez.
> 3. **Un item = un commit** (`fix(break-N): ...` o `refactor(break-N): ...`). Después de cada
>    item: compilá, corré la suite (debe seguir en `1078/11119` verde) y agregá el test que el
>    item pide.
> 4. Los refactors de salud (Parte B) son **sin cambio de comportamiento**. Si algún output
>    observable cambia, frená y reportá.
> 5. Cambios al formato `.moodsave`/`.moodmap` (A1/A2): bump de versión + upgrader, nunca romper
>    archivos viejos en silencio.
> 6. **NO toques** nada de la sección "Auditado y OK".
> 7. Cuando termines todo: tag `v2.0.1-break-auditoria`, actualizá `ESTADO_ACTUAL.md` con el
>    cierre del break, y reportá item por item qué hiciste (qué cambió, qué tests agregaste).
>    Recién entonces planeamos la Fase 3.

---

## Cómo ejecutar este break (leer antes de tocar nada)

- **Un item por commit.** Mensaje: `fix(break-N): <qué>` (placebos) / `refactor(break-N): <qué>` (salud).
- Tras cada item: **compilar + suite verde (`1078/11119`)** y **agregar el test que el item pida**.
- Los `archivo:línea` son del estado `v2.0.0`. Si no matchean, buscá por el **símbolo citado**
  (nombre de campo/función), no asumas posiciones.
- **Refactors de salud = sin cambio de comportamiento.** Si un refactor cambia output
  observable, frená y reportá: algo se entendió mal.
- **Cambios de formato de `.moodsave`/`.moodmap`:** bump de versión + upgrader. Nunca
  romper archivos viejos en silencio.
- **NO tocar** lo de la sección "Auditado y OK" al final. Está verificado.
- Orden recomendado al final del documento.

---
---

# PARTE A — Integridad (placebos)

## A1 · [CRÍTICO] El inventario de runtime NO se persiste en `.moodsave`

**Síntoma:** ítems recogidos en Play (pickup con E, `inventory.add`, rewards de quest tipo
`Item`) **desaparecen** al cargar la partida. Parece funcionar dentro de una sesión (el
Quest Log avanza) pero no sobrevive guardar/cargar. Es el placebo de mayor impacto real.

**Cadena rota:**
- UI/diseño crea inventario: `InspectorPanel_Inventory.cpp` (drop del Item Browser). **OK.**
- Persistencia estática `.moodmap`: `EntitySerializer.cpp:367-401` / `SceneLoader.cpp:357-385`.
  **OK** — pero es estado de *diseño*, no de partida.
- Runtime muta: `ItemPickupSystem.cpp:123-124`, `LuaBindings_Inventory.cpp:219-240`. **OK.**
- **FALTA save-game:** `SaveLoad.h:91-104` (`struct SaveData`) no tiene campo de inventario;
  `SaveLoad.cpp:40-110` (`save`) y `PlayerApplication_SaveLoad.cpp:362-458`
  (`captureCurrentState`) no capturan ningún `InventoryComponent`.

**Acción:**
1. Agregar a `SaveLoad::SaveData` (`SaveLoad.h:91`) un `std::vector<InventorySnapshot>`
   (tag + entries `path`+`qty`+`slot`), reusando el patrón `SavedInventory` de
   `SceneSerializer.h:287`.
2. Serializar en `SaveLoad.cpp::save` (~L107) y parsear en `load` (~L247).
3. Capturar en `captureCurrentState` (`PlayerApplication_SaveLoad.cpp:455`) iterando
   `forEach<TagComponent, InventoryComponent>`; restaurar por tag en `applyLoadedSave` (~L359).
4. Bump `k_supportedVersion` 3→4 (`SaveLoad.cpp:12`) + upgrader para v3 (inventario vacío).
5. **Test:** `test_saveload_inventory.cpp` — add 3 items en runtime, save, mutar, load,
   assert que el inventario vuelve al snapshot guardado (no al `.moodmap`).

## A2 · [MEDIO] `dialog.vars` (GameState::dialogVars) NO se persiste en `.moodsave`

**Síntoma:** flags de diálogo (`dialog.set_var`/`has_var`) se pierden al guardar. Un NPC que
"te recuerda" o un Talk a medio camino se resetea post-load. Mitigado en parte porque el
progreso de *objectives* sí se restaura.

**Evidencia:** set/get en memoria OK (`LuaBindings.cpp:324-339`, `GameState.cpp:22`); quests
Talk/Reach evalúan `dialog.has_var` (`QuestSystem.cpp:74-77`); `SaveLoad` no toca `dialogVars`.
Documentado como YAGNI en `PENDIENTES.md:129`.

**Acción:** agregar `std::unordered_map<std::string,std::string> dialogVars` a `SaveData`,
capturar/restaurar `GameState::dialogVars()` **en el mismo bump 3→4 que A1** (hacer A1+A2
juntos). Quitar la nota de `PENDIENTES.md:129`. **Test:** extender `test_saveload_inventory.cpp`
con set_var → save → load → has_var.

## A3 · [MEDIO] El auto-avance de quests depende de que exista ≥1 entidad con ScriptComponent

**Síntoma:** un mapa con quests pero sin ninguna entidad-script → objetivos Collect/Talk/Reach
**nunca auto-completan** y rewards nunca se aplican. El Quest Log se ve congelado → parece
placebo. Además el evaluator queda apuntando a la `sol::state` del último script cargado
(acoplamiento frágil).

**Evidencia:** `QuestSystem::tick` usa `g_evaluator`; si es nullptr `allDone=false` siempre
(`QuestSystem.cpp:239-248`); `applyRewards` early-return si `g_executor` nullptr (`:38-39`).
El único setter es `setupQuestBindings` (`LuaBindings_Quest.cpp:266,285`), llamado **una vez
por entidad-script** (`ScriptSystem.cpp:37`). `tick` sí corre incondicional
(`PlayerApplication_Frame.cpp:533`).

**Acción:** mover el registro de evaluator/executor a un host dedicado con su propia
`sol::state`, independiente de las entidades-script (paralelo a `DialogScriptHost.cpp`).
Inicializar una vez en `PlayerApplication_Init.cpp` y en el entry a Play del editor. Verificar
lifetime vs `QuestSystem::clearHooks()` en scene-unload. **Test:** `test_quest_no_script_entity.cpp`
— escena con 1 quest Collect y CERO scripts; agregar item por API; tick; assert completa.

## A4 · [MEDIO] `RagdollComponent` no tiene Inspector

**Síntoma:** `totalMass`/`limbRadius`/`useGravity`/`spawnImpulse` se serializan y el runtime
los consume, pero **no hay sección de Inspector** para editarlos (solo JSON a mano).

**Evidencia:** serial OK (`EntitySerializer.cpp:457-466` / `_Parse.cpp:265-272` /
`SceneLoader.cpp:403-410`); consume OK (`RagdollSystem.cpp:152-153,174-175,213-216`); falta UI
(`InspectorPanel.cpp:96-104` despacha todos menos Ragdoll).

**Acción:** crear `InspectorPanel::renderRagdollSection(Entity)` con el patrón de
`InspectorPanel_Physics.cpp` (`DragFloat` totalMass 1–300, limbRadius 0.01–0.5, `Checkbox`
useGravity, `DragFloat3` spawnImpulse). Declarar en `InspectorPanel.h` (~L58) + agregar
`if (e.hasComponent<RagdollComponent>()) renderRagdollSection(e);` tras `InspectorPanel.cpp:98`.
No requiere `dirty`.

## A5 · [MEDIO] `RigidBodyComponent::isSensor` no tiene widget en el Inspector

**Síntoma:** `isSensor` (trigger físico estilo Unity `isTrigger`) llega a Jolt y hace
round-trip completo, pero el Inspector no lo expone (solo JSON/código).

**Evidencia:** serial OK (`EntitySerializer.cpp:125` / `_Parse.cpp:78` / `SceneLoader.cpp:180`);
consume OK (`PhysicsWorld.cpp:299` `mIsSensor`); falta UI (`InspectorPanel_Physics.cpp:22-108`).

**Acción:** en `InspectorPanel_Physics.cpp` tras el bloque friction (~L104), `ImGui::Checkbox`
sobre `rb.isSensor` vía el mismo `EditPropertyCommand<...>` que type/shape (efecto al próximo
Play, consistente con el diseño). Marcar `m_editedThisFrame = true`.

## A6 · [MEDIO] Shader Graph cae a PBR en mallas instanced/skinned sin avisar

**Síntoma:** un material con `shaderGraphPath` en malla skinned/instanced se renderiza con PBR
estándar, ignorando el grafo, **sin warning**. El camino estático sí funciona.

**Evidencia:** pipeline estático real (`ShaderGraphEditorPanel.cpp:122` → `AssetManager_Material.cpp:100`
→ `SceneRenderer_Render.cpp:430-435`); fallback a PBR para instanced/skinned
(`RenderBatching.cpp:95-98`, `SceneRenderer_Render.cpp:645-648`; el cache v1 solo conoce `pbr.vert`).

**Acción (mínima):** warning en el inspector del material (o `ShaderGraphEditorPanel`) cuando
se asigna a un MeshRenderer skinned/instanced: "shader graph no soportado en este tipo de malla,
usando PBR". (Soporte real = hito de Fase 3.)

## A7 · [BAJO] `CameraComponent` — sliders editables que no hacen nada

**Síntoma:** el Inspector muestra `fovDeg`/`nearPlane`/`farPlane` editables, pero no se
serializan ni afectan el render (el path usa `m_editorCamera.fovDeg()`). Ya documentado como
stub en `Components_Render.h:182-184`.

**Acción (elegir 1):** *Mínima* — envolver los widgets en `ImGui::BeginDisabled()` + label
"(stub)" en `InspectorPanel_Misc.cpp:37-58`. *Completa (Fase 3)* — sistema "active camera":
derivar proyección del `CameraComponent` activo + serializar los 3 campos.

## A8 · [BAJO] `catch (...)` que tragan errores de parseo JSON en silencio

**Síntoma:** un `.moodmap`/paquete corrupto falla sin avisar (return mudo).
**Evidencia:** `EditorProjectActions.cpp:68`, `PackageBuilder.cpp:95`. (Los de
`OpenGLTexture.cpp:77,102` son fallback de textura, aceptables.)
**Acción:** reemplazar por `catch (const std::exception& e)` con `Log::error(... e.what())`
antes del return, en los 2 sitios de carga de proyecto/paquete. No cambia comportamiento.

## A9 · [BAJO] Dead code cosmético

| Qué | Evidencia | Acción |
|---|---|---|
| Modal `##notimpl_modal` nunca se dispara | `MenuBar.cpp:303-306,329-335`; flag nunca true; comentario stale `MenuBar.h:3-4` | Borrar flag + bloque; actualizar comentario (hoy todos los items están cableados). |
| `DemoSpawners_*` sin trigger UI | `MenuBar.cpp:216-224` (submenú eliminado en F2H57) | Confirmar que nadie reusa `ensureDemoIntroDialogExists` y eliminar los `.cpp`. |
| `world/streaming/` vacío | 0 referencias en `src/` | No es placebo (no se expone). Comentario "no implementado" en el `.gitkeep`. |
| `HudState.ammo` legacy | `GameState.h:49` "no se usa post-F2H39", se sigue serializando | Cosmético. Opcional: quitar en el próximo bump de schema. |

---
---

# PARTE B — Salud de código

## B1 · [ALTO] Inversión de capa: `engine/` incluye `systems/` (13 includes)

**Problema:** tu regla (`ARCHITECTURE.md:130`) dice "engine/ nunca incluye systems/", pero
`engine/render/` incluye 8 cosas de `systems/render/` y `systems/light/`
(`SceneRenderer.cpp:45-52`, `SceneRenderer_Render.cpp:48-50`, `LightGrid.h:23`,
`SceneLoader.cpp:15`). Análisis del contenido:
- **NO son sistemas ECS** (FB→FB, incluyen `glad/gl.h`, cero `entt`/`Scene`): `BloomPass`,
  `SSAOPass`, `ColorGradingPass`, `PostProcessPass`, `SkyboxRenderer`. **Mal ubicados.**
- **Toque ECS leve aceptable como utilidad de pipeline:** `ShadowPass` (itera casters),
  `SSRPass` (lee G-buffer).
- **Sistema ECS legítimo (se queda):** `LightSystem` (recolecta `LightComponent`).

**Acción:** la regla del doc es correcta; lo que está mal es la ubicación.
1. Mover `BloomPass/SSAOPass/ColorGradingPass/PostProcessPass/SkyboxRenderer/ShadowPass/SSRPass`
   a **`engine/render/passes/`** (no son sistemas).
2. `LightSystem` queda en `systems/light/`. Romper `LightGrid.h:23 → LightSystem.h` extrayendo
   los POD `LightFrameData`/`PointLightData` a `engine/render/pipeline/LightData.h` (incluido
   por ambas capas).
3. `SceneLoader.cpp:15 → VehicleSystem.h`: mover la constante `chassisRenderYOffset` a un header
   de `engine/physics/`.
Resultado: **0 violaciones reales** sin tocar la regla. **Test:** la suite compila = prueba; sin
cambios de comportamiento.

## B2 · [ALTO] Hard-cap violado: `SceneRenderer_Render.cpp` (978 líneas, cap 800)

Único `.cpp` sobre el hard-cap. **Acción:** dividir por responsabilidad de pass: extraer el
bloque de shadow/CSM setup, el de point-light SSBO/grid, y el de material/shader-graph
resolution a archivos `SceneRenderer_Render_<Familia>.cpp` (mismo patrón que ya usás con
`_Ortho`). Objetivo: cada parte < 500 (soft-cap). **Test:** render smoke + la suite.

## B3 · [ALTO] Funciones gigantes (espagueti acotado)

Las peores (heurística por llaves):

| Líneas | Archivo:línea | Función | Acción |
|---|---|---|---|
| 560 | `EditorApplication_Run.cpp:59` | `run()` | Extraer fases: `tickHotReload()`, `pumpUiRequests()`, `pumpSpawnAndDropRequests()`, `tickSystems(dt)`. Los `process*Request` ya existen — agruparlos en un dispatcher por tabla. |
| 546 | `EditorApplication_RunInteractions.cpp:44` | `processViewportInteractions()` | Un handler por modo: `handleFacePick`, `handleObjectPick`, `handleClipClick`. |
| 478 | `..._RunInteractions_ToolModes.cpp:41` | `processOrthoToolModes()` | Idem, extraer por sub-modo (block/marquee/vertex-edit). |
| 467 | `EntitySerializer.cpp:36` | `serializeEntityToJson()` | Ver B5 (tabla de componentes). |
| 446 | `EditorPlayMode.cpp:144` | `updateCameras(f32)` | Separar editor-cam vs fps-cam vs vehicle-cam. |

**Test:** smoke de Play/Stop + spawn por request + selección/picking deben seguir verdes si
se preserva el orden. Refactor SIN cambio de comportamiento.

## B4 · [MEDIO] 22 archivos `.cpp` sobre el soft-cap (500–800)

No urgente (están bajo el hard-cap), pero el dev pidió "nada de archivos enormes". Los más
grandes y candidatos a partir junto con sus funciones de B3: `GameOverlay_Inventory.cpp` (772),
`GameOverlay.cpp` (749), `DemoSpawners_Drop.cpp` (737 — desaparece si se hace A9/demos),
`SceneRenderer.cpp` (730), `SceneLoader.cpp` (692), `ShaderGraphEditorPanel.cpp` (690),
`AssetBrowserPanel_ImportVehicle.cpp` (690), `PhysicsWorld.cpp` (671), `CompileMap.cpp` (662),
`InspectorPanel_Environment.cpp` (623). **Acción:** partir solo los que toques por otra razón
(B3/B6); no hacer un barrido masivo por el número.

## B5 · [MEDIO] Duplicación: boilerplate por tipo de asset en `AssetManager`

**Problema:** 8 familias de asset (textura/audio/mesh/prefab/material/dialog/anim/item/quest/
vehicle) clonan idéntico `loadX/getX/missingXId/xPathOf/xCount` + 3 vectores paralelos
(`AssetManager.h:362-481, 491-538`). Es la deuda estructural que crece con cada tipo nuevo.
**Acción:** plantilla `AssetRegistry<T>` (cache map + vector + paths + `loadOrGet`/`get`/
`pathOf`/`count`); reemplazar cada bloque por `AssetRegistry<Dialog::Asset> m_dialogs;` etc.
Migrar **tipo por tipo** (un commit por familia). **Test:** los unit del AssetManager (fallback
id=0, round-trip `pathOf`) cubren cada migración.

## B6 · [MEDIO] Secciones largas del Inspector

`renderEnvironmentSection` (537), `renderMeshRendererSection` (418), `renderBrushSection` (387)
son UI lineal (no espagueti de control) pero enormes. **Acción:** extraer sub-bloques a
lambdas/helpers locales (`drawSkyboxPresetCombo`, `drawFogControls`, etc.). La capa de
field-helpers ya existe en `InspectorPanel_Internal.h:41-111` — colgar ahí.

## B7 · [BAJO] `glad/gl.h` fuera de `backend/opengl/` (21 archivos)

Tu regla (`ARCHITECTURE.md:128`) dice que `backend/opengl/` es el único lugar con `glad`. En la
práctica lo incluyen los render-passes, los preview renderers, el scene_renderer y los apps. Para
un motor de **un solo backend** la regla literal es demasiado estricta. **Acción (decisión, no
código urgente):** actualizar `ARCHITECTURE.md` para reflejar la realidad — "GL permitido en
`engine/render/` (passes, scene_renderer, preview) y en los apps; la abstracción RHI sigue
siendo el camino para lógica portable". Alternativa (más cara, Fase 3+): rutear todo por RHI.
Recomendación: actualizar el doc ahora, no el código.

## B8 · [BAJO] `new`/`delete` crudos

Todos son `JPH::` (Jolt) asignados a `JPH::Ref<>` (idiomático, sin leaks). Único frágil:
`PhysicsWorld_Vehicle.cpp:212` hace `new JPH::WheeledVehicleControllerSettings()` con ownership
manual hasta `:278`. Hoy correcto, pero un `return`/throw nuevo entre `:213-278` filtraría.
**Acción:** envolver en `std::unique_ptr` + `.release()` al asignar a `mController`.

## B9 · [INFO] Cosas que están bien (no tocar)

- **Naming:** consistente con la convención. Únicos lowercase son forward-decls de libs externas
  (`ma_engine`, `adl_serializer`) y el namespace intencional `vehicle::`. Sin violaciones.
- **TODO/FIXME:** la cuenta de "32" son casi todos falsos positivos de la palabra **"TODOS/TODO"**
  en español. Deuda anotada real: ~cero.
- **`std::cout` crudos: NO existen.** Los 35 `*nprintf` son a buffers locales (uso correcto); los
  2 `fprintf(stderr)` son errores fatales pre-logger. *(Corrige una observación previa errónea.)*
- **God-objects:** `EditorApplication` (.h 703/103 métodos), `PhysicsWorld` (.h 576) y
  `AssetManager` (.h 541) NO son god-objects para split — ya están particionados por familia en
  `.cpp` y su acoplamiento es inherente al rol. No refactorizar la estructura.
- **Inspector field-helpers:** ya factorizados (`InspectorPanel_Internal.h`), sin duplicación.

---
---

# Auditado y OK — NO TOCAR

Verificado punta a punta, está bien cableado / sano:
- **Render:** `EnvironmentComponent` completo (fog, exposure, tonemap, IBL, bloom, SSAO, SSR,
  color grading, CSM), `LightComponent` completo, material node-graph (camino estático).
- **Física:** los 5 tipos de constraint (Hinge/Distance/Point/Slider/Fixed, todos reales en
  Jolt), RigidBody/Cloth/ForceField/Vehicle.
- **Game:** Dialog (ramificación/condiciones/acciones), Quest state-machine + save/load de
  quests, Triggers, ItemPickup→inventory→quest collect (en runtime), GameManifest.
- **Editor/World:** CSG booleanos + `CompileMap`, tool modes + snap + gizmos, Lua bindings +
  hot-reload + eventos, Preferences, save/load de los editores de asset.
- **Higiene:** naming, RAII (`HistoryStack` con `unique_ptr`), capas (salvo B1/B7).

---

# Orden de ejecución sugerido

**Tanda 1 — integridad (lo que el usuario "cree que funciona"):**
A1 + A2 (un solo bump de schema) → A3 → A4 / A5 (UI rápida) → A8.

**Tanda 2 — salud estructural (antes de que crezca):**
B1 (mover passes, mata la inversión de capa) → B2 (split hard-cap) → B3 (funciones gigantes,
arrastra B4/B6) → B5 (AssetRegistry, tipo por tipo) → B8.

**Tanda 3 — cosmético / decisiones de doc:**
A6 (warning) → A7 → A9 → B7 (actualizar ARCHITECTURE.md).

Cerrar el break con tag `v2.0.1-break-auditoria` y actualizar `ESTADO_ACTUAL.md`. Recién
entonces planear Fase 3.
