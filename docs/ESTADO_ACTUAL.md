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

## 0.1. Último hito de feature — F2H82 (2026-05-23) — **Sub-fase 2.7**

**Modal Importar vehículo + Live tuning + bake GLB + anti-roll + polish.** Tag `v1.73.0-fase2-hito82`. Detalle completo en [`hitos/F2H82.md`](hitos/F2H82.md). Séptimo hito de Sub-fase 2.7.

Convierte el pipeline de vehículos de "editar JSON a mano" a "drop-in del `.glb` desde el editor". Cierre de la deuda de **integrar los autos del backlog** declarada en F2H81 y mucho más arriba.

**Lo que entregó**:
- **Anti-roll bars (Jolt built-in)**: `JPH::VehicleAntiRollBar` en eje delantero (FL↔FR) y trasero (RL↔RR), `mStiffness=3000`, idéntico concepto a Wheel Collider de Unity / Chaos de Unreal. Cero código nuevo de física — solo poblar `VehicleConstraintSettings::mAntiRollBars`. Memoria `feedback_no_reinventar_rueda` aplicada.
- **CoM bajo en autos del backlog**: Tesla `mass_center_override_mm.y` 514→**150**, armor-car 569→**200**. Truco arcade GTA SA/Burnout/NFS: bajar el CoM debajo del centro físico reduce drásticamente el vuelco sin tocar la física.
- **Drag preview con thumbnail 3D**: `BeginDragDropSource` dibuja `Image((ImTextureID)tex, 48x48)` antes del label en cada card del tab Vehículos. Antes solo veías texto suelto.
- **Modal Importar vehículo** (`AssetBrowserPanel_ImportVehicle.cpp` NUEVO ~400 LOC): botón *+ Importar...* → `pfd::open_file` → `analyzeVehicleMesh` → form en `CollapsingHeader`s (Análisis: scale + 4 ruedas con role/pos/radius + `Invertir adelante/atrás`/`izq/der`; Identidad: nombre+clase combo; Motor/Frenos/Suspensión/Chasis con tooltips por campo) → `saveImportedVehicle` (copia `.glb` a `assets/vehicles/<slug>/`, hornea scale si !=1, `writeVehicleConfigFile`, `rescan` + `m_reloadRequested`). UX: `BeginChild(-footerH)` para que **Guardar/Cancelar siempre estén visibles**, `NoResize`, errores rojos con `TextWrapped`.
- **Live tuning del VehicleComponent** en Inspector (`InspectorPanel_Vehicle.cpp`): `AssetManager::getMutableVehicleConfig(id)` devuelve el config compartido. `CollapsingHeader "Live tuning"` con combo de presets (cambia clase → `applyPresetToConfig` copia físicos preservando geometría) + DragFloats para mass/torque/maxRPM/brake/handbrake/maxSteer/steerLerp/damping/CoM/frictLong/frictLat. Cualquier cambio → `veh.dirty=true` y el `VehicleSystem` reaplica al body Jolt next frame.
- **Borrar vehículo por clic derecho**: `BeginPopupContextItem` con `MenuItem("Eliminar...")` → modal de confirmación → `fs::remove(.moodvehicle)` (el `.glb` queda).
- **Bake del scale en GLB root node** (decisión clave): primer intento de persistir `mesh_scale` en runtime descartado por dos convenciones de unidades en el proyecto. Definitivo: hornear el scale en el root node del `.glb` copiado (cirugía del chunk 0 JSON, **chunk 1 BIN se preserva verbatim**). Soporta **ambas formas** del transform de glTF: `scale: [sx,sy,sz]` (multiplica componentes) **y `matrix` 4x4** (multiplica indices 0,1,2,4,5,6,8,9,10 del 3x3 + 12,13,14 traslación; skipea 3,7,11,15). Sin soporte de matrix no entraban modelos Sketchfab/FBX→glTF (~50% de la web).
- **`meshScale` end-to-end (back-compat)**: aunque el bake es definitivo, se preserva `VehicleConfig::meshImportScale` para `.moodvehicle` legacy; aplicación en `DemoSpawners_Drop` (chassis) + `VehicleSystem` (wheel-entities). El **writer ya no escribe** `mesh_scale` post-F2H82.
- **Migración del skip de wheel-entities** (`VehicleWheelMarker { chassisHandle, wheelIndex }` en `Components_Physics.h`): el `SceneSerializer` skipeaba por tag matcheando `wheel_FL/FR/RL/RR`, fallaba con autos importados cuyas ruedas tienen tags reales del modelo (`f_t_l`, `RUEDA_*`); migrado a `e.hasComponent<VehicleWheelMarker>()`. `ScenePick`/`HierarchyCollect` también lo consumen. **Side effect en tests**: `test_scene_serializer_lighting_physics.cpp` (caso F2H70.3 H) actualizado para añadir el marker — refleja el contrato nuevo, no afloja el test.

**Suite 1074/11103 verde**. Validado en vivo: Tesla + armor-car en curvas cerradas sin volcar, DELOREAN.glb de Sketchfab importado a tamaño correcto, live tuning de masa en Play persiste al re-spawnear, drag preview con miniatura, clic-derecho-Eliminar.

Cerrado por el dev: *"lo logrado hasta ahora esta bien, cerremos aca para terminar con esto y luego en la fase 3 veremos como mejorar esto"*.

**Diferidos a F2H83 / Fase 3**:
- **Extracción de texturas embebidas a disco** al importar (estándar Unity gLTFast / Unreal glTF Importer): hoy modelos Sketchfab tipo BTTF DeLorean entran con materiales rosa-grid porque el loader cae a `missingMaterial`. F2H83 dedicado.
- **Limitación conocida**: re-importar un `.glb` mientras el editor tiene esa entity cargada → Windows file lock. Workaround: borrar la entity antes, o usar otro nombre.

**Pendientes de 2.7 (en orden acordado con el dev)**:
- **F2H83 — extracción de texturas embebidas + asset extraction pipeline**.
- **Refactor diferido (riesgo render)**: los 3 archivos grandes del hot path (renderScene / overlay 3D / EditorApplication.h god-class) — hito dedicado con validación visual. Anotado en [BACKLOG.md § 4](BACKLOG.md).
- **Atajos de teclado configurables** (era F2H42 del plan original, nunca hecho — hoy hardcodeados): keybindings + UI + persistencia + presets.
- **Cierre Fase 2 + `v2.0.0`**: suite verde, docs al día, release notes, recap, planning Fase 3.
- Menores: Ctrl+S "guardar como", unificar Undo en Material/Item/Quest. **Tutorial in-app**: diferido por el dev a post-Fase 2.

---

## 0.1ante4. Hito previo — F2H81 (2026-05-22) — **Sub-fase 2.7**

**Preview de animaciones + Asset Browser visual + Inspector claro + break de auditoría.** Tag `v1.72.0-fase2-hito81`. Detalle completo en [`hitos/F2H81.md`](hitos/F2H81.md). Sexto hito de Sub-fase 2.7.

**Lo que entregó** (resumen):
- **Preview de animaciones** (`AnimationPreviewRenderer`) con NPC de Mixamo; hover-to-play.
- **Asset Browser 100% visual**: Vehículos/Materiales/Scripts/Prefabs/Audio con cards.
- **Inspector más claro**: tarjetas plegables, Plegar/Expandir todo, Quitar componente undoable, MeshRenderer con "Technical details" colapsado.
- **Break de auditoría**: `AssetBrowserPanel.cpp` 924→395, `EntitySerializer.cpp` 819→504, `Components.h` 936→25 (agregador). DRY de `lowerExt()`.

**Suite 1061/11018 verde**. Validado en vivo — *"todo ok"*.

---

## 0.1ante3. Hito previo — F2H80 (2026-05-22) — **Sub-fase 2.7**

**Miniaturas 3D en "Crear Entidad" + Asset Browser.** Tag `v1.71.0-fase2-hito80`. Detalle en [`hitos/F2H80.md`](hitos/F2H80.md). `MeshThumbnailRenderer` (render-once cacheado por mesh, reusa PBR+IBL) → grilla de cards 3D en Crear Entidad + Asset Browser; primitivas en 3D (vía brush CSG), luces con ícono, modal no-resize.

---

## 0.1ante2. Hito previo — F2H79 (2026-05-21) — **Sub-fase 2.7**

**Pulido de modales + hover circular + remake del Welcome.** Tag `v1.70.0-fase2-hito79`. Detalle completo en [`hitos/F2H79.md`](hitos/F2H79.md). Cuarto hito de Sub-fase 2.7.

**Lo que entregó**:
- **Modal Preferencias** (`MenuBar`): título traducido en la **titlebar** (`### ` mantiene el ID estable) + botón **X** vía `&m_prefsOpen`.
- **Top bar estilo Unity**: se quitaron las tabs de workspace estilo Blender; **Play/Stop centrado** (verde/rojo) separado de la navegación, y **selector de workspace = botón hamburguesa ☰** a la derecha que abre un popup (6 workspaces) **debajo** del botón (no lo tapa). `ICON_FA_BARS` añadido al subset.
- **Hover circular de la X** (parche CMake idempotente a `imgui_widgets.cpp`): `CloseButton`/`CollapseButton` usan `AddRectFilled` hardcodeado → `string(REPLACE)` en configure-time a `AddCircleFilled`, marca `MOOD_CIRCLE_CLOSE`. Aplica a **todas** las ventanas/modales.
- **Remake del Welcome** (`drawWelcomeModal`): estilo splash Blender — banner (nombre+versión) + 2 columnas **EMPEZAR** / **RECIENTES**; ⊗ de quitar sin fondo (hover gris), "Limpiar inexistentes" al footer, texto de bloqueo eliminado. Sigue **sin X** (no se entra sin proyecto). i18n `start_header`/`recents_header`.

**Suite 1061/11018 verde**. Validado en vivo (X+título, Play, hamburguesa, hover circular, Welcome).

**Pendientes de 2.7** (en orden): **F2H80 — thumbnails 3D** en "Crear entidad" (grid de cards con preview render-to-texture, reusando `MaterialPreviewRenderer`); luego mismo grid en Welcome/Asset Browser. Más atrás: Ctrl+S "guardar como", unificar Undo en Material/Item/Quest, atajos configurables, escala de UI, y el bump a `v2.0.0` (cierre Fase 2).

---

## 0.1ante. Hito previo — F2H78 (2026-05-21) — **Sub-fase 2.7**

**Ctrl+S contextual + ajuste de redondeo.** Tag `v1.69.0-fase2-hito78`. Detalle completo en [`hitos/F2H78.md`](hitos/F2H78.md). Tercer hito de Sub-fase 2.7.

**Lo que entregó**:
- **Ctrl+S contextual**: `IPanel::consumesSaveShortcut()` (virtual nuevo, default false) lo declaran los 4 editores guardables (script/shader/item/quest), respaldado por `m_windowFocused` (seteado en su render vía `IsWindowFocused`). El handler global de Ctrl+S (evento SDL en `EditorApplication`) recorre los panels `visible`: si alguno consume el atajo → NO guarda proyecto (el editor enfocado se guarda solo en su render); si no → `ProjectAction::Save` como siempre. Script/Shader ya tenían self-save; Item/Quest lo ganan (`saveToDisk()` compartido por botón + atajo). Item/Quest exigen `!m_loadedPath.empty()`.
- **Ajuste de redondeo** (`EditorThemes::applyMetrics`): el redondeo global de F2H76 se veía mal en regiones hijas/scrollbars/tabs → `ChildRounding/ScrollbarRounding/TabRounding = 0`; se mantienen ventanas(6)/popups(6)/frames(4)/grabs(4).

**Suite 1061/11018 verde**. Validado con el badge "● Sin guardar" (F2H77) como delator.

**Pendientes de 2.7** (en orden acordado): pulido de modales (título en titlebar + botón X de cerrar + remake del Welcome estilo Blender + reubicar el Play button), thumbnails 3D en Asset Browser/recientes (render-to-texture). Más atrás: Ctrl+S de "guardar como", unificar Undo en Material/Item/Quest, atajos configurables, escala de UI, y el bump a `v2.0.0` (cierre Fase 2).

---

## 0.1bis. Hito previo — F2H77 (2026-05-21) — **Sub-fase 2.7**

**Pulido visual base + indicador "sin guardar".** Tag `v1.68.0-fase2-hito77`. Detalle completo en [`hitos/F2H77.md`](hitos/F2H77.md). Segundo hito de la Sub-fase 2.7.

**Origen**: tras F2H76 (redondeo, que al dev le gustó) pidió aplicar "esa forma" a otros paneles + recomendaciones UX. Se hizo una **auditoría UX completa** del editor. **Hallazgo clave: la mayoría de las quick wins ya estaban implementadas** (tooltips con atajos en ambas toolbars, pestaña Luces de F2H60, Inspector con estado vacío + hint, secciones colapsables) — el editor estaba más pulido de lo que el reporte inicial del agente explorador sugería. El hito se acotó honestamente a lo realmente faltante.

**Lo que entregó**:
- **Métricas unificadas** (`EditorThemes::applyRounding` → `applyMetrics`): suma al redondeo de F2H76 el espaciado (`WindowPadding (10,8)`, `FramePadding (8,5)`, `ItemSpacing (8,7)`, `ItemInnerSpacing (6,5)`, `CellPadding (6,5)`, `IndentSpacing 20`, `ScrollbarSize 12`, `GrabMinSize 11`). Común a los 4 temas, seteado aparte de los colores → sobrevive al theme-switch.
- **Badge "● Sin guardar"** (ámbar) en la status bar: surfacea el `m_projectDirty` que **ya existía** (el título del SO ya ponía `*` pero era fácil de no ver). `StatusBar::setProjectDirty` + `EditorUI::setProjectDirty` + sync en `EditorApplication::updateWindowTitle()` (único punto, ya se llama en cada transición de dirty — sin polling). i18n en/es.

**Fuera de scope (futuro 2.7)**: Ctrl+S contextual (guardar el panel con foco), unificar Undo en Material/Item/Quest editors, acento de marca único.

**Suite 1061/11018 verde** (sin tests nuevos — UI sin superficie headless). Validado en vivo: espaciado OK + badge aparece al editar / desaparece con Ctrl+S, *"listo"*.

---

## 0.1ter. Hito previo — F2H76 (2026-05-21) — **abrió Sub-fase 2.7**

**Preferencias + temas visuales del editor.** Tag `v1.67.0-fase2-hito76`. Detalle completo en [`hitos/F2H76.md`](hitos/F2H76.md). Primer hito de la **Sub-fase 2.7 (UI/UX final + cierre Fase 2)**. Abre la "casa de los ajustes" del editor; futuros hitos cuelgan ahí atajos de teclado, escala de UI, etc. (el bump a `v2.0.0` se reserva para el CIERRE de la sub-fase, no para este hito).

**Lo que entregó**:
- **`UserSettings`** gana campo `theme` (string id, default `"dark"`, back-compat si el JSON no lo trae).
- **`EditorThemes.{h,cpp}`** (NUEVO): registro `{id, i18nKey}` + `apply(id)`. Dark/Light = presets ImGui; **Midnight** (dark + tinte azulado/acento celeste) y **Sepia** (light + tinte cálido papel/madera) con paletas `ImVec4` custom. Diseño abierto a temas por JSON a futuro (id → función).
- **Startup** usa `EditorThemes::apply(UserSettings::theme())` (antes `StyleColorsDark` hardcodeado). **Editar → Preferencias** abre modal con combo de Tema (preview live + `save()` en cada cambio) + combo de Idioma (**centralizado** acá, removido de Ver → Idioma).
- **Esquinas redondeadas** en toda la UI (añadido en validación a pedido del dev): `applyRounding` (Window/Child/Popup/Scrollbar=6px, Frame/Grab=4px, Tab=5px), seteado aparte de los colores → común a los 4 temas + sobrevive al theme-switch.
- **i18n** en/es con paridad (`test_i18n`).

**Suite 1061/11018 verde** (sin tests nuevos — ImGui style no tiene superficie testeable headless). Validado en vivo: 4 temas con preview live + idioma centralizado + persistencia al reabrir + redondeo, *"funciona okey"*.

**De esta sesión salió una auditoría UX completa** del editor (paneles/workspaces/menús/modales/redundancias) → alimenta el plan de **F2H77** (polish UX transversal: tooltips con atajos, spacing/acento unificados, Inspector colapsable, indicador de "sin guardar", limpieza de redundancias).

---

## 0.1quater. Hito previo — F2H75 (2026-05-21) — **cerró Sub-fase 2.4**

**Cloth / telas que ondean.** Tag `v1.66.0-fase2-hito75`. Detalle completo en [`hitos/F2H75.md`](hitos/F2H75.md). Era el F2H28 original (último item de Física avanzada). Banderas/cortinas como soft body de Jolt que cuelgan por gravedad y flamean con las zonas de viento (`ForceFieldComponent` de F2H72).

**Scope acordado en mecánicas con el dev**: tela anclada por un borde + gravedad + viento, render sólido iluminado doble cara. NO choca con objetos, no es jelly/globo, no se rasga, grilla procedural NxM (no import de `.glb`).

**Lo que entregó**:
- **`ClothLayout`** (header puro, sin Jolt, testeable): grilla mass-spring (structural/shear/bend) + anclajes (None/TopEdge/TopCorners/LeftEdge) + triángulos.
- **`PhysicsWorld_SoftBody.cpp`**: wrapper sobre `JPH::SoftBody` (v5.2.0 ya viene compilado). create/destroy/readClothVertices/applyClothAcceleration. Ancladas con invMass 0; viento = aceleración sobre la velocidad de las libres.
- **`ClothComponent`** + serialización round-trip aditiva + gate del serializer (tela standalone sin mesh).
- **Mesh dinámico** (infra NUEVA — hasta F2H74 todo era estático/skinneado): `OpenGLClothRenderer` con VBO `GL_DYNAMIC_DRAW` re-subido/frame, shader `cloth.{vert,frag}` lit simple doble cara (`gl_FrontFacing`), invocado post-opaco por `SceneRenderer`.
- **`ClothSystem`**: materialize lazy + sync sim→buffer (normales/frame) + `previewRest` analítico (editor) + reset en `enterPlayMode`.
- **Editor**: *Add Component → Física → Tela* + Inspector (dimensiones/resolución/anclaje/masa/rigidez/damping/gravedad/color, i18n en/es).
- **Viento**: `ForceFieldSystem` extendido a telas + demo `cloth_demo.moodmap`.

**Fix lateral reactivo** (ajeno al cloth): el Slider constraint con `min==max` crasheaba por un assert de Jolt v5.2.0 → `createSliderConstraint` expande un epsilon si los límites son iguales (protege test de F2H71 + runtime del editor).

**Suite 1060/11015 verde.** Validado en vivo: bandera sólida + iluminada, en Play cuelga + ondea, feel "se siente bien".

**Fix reactivo post-cierre** (commit `dc7c832`): el crash al escalar un Box body por debajo del *convex radius* de Jolt (~0.05 m) quedó arreglado — `createJPHShape` clampea el half-extent + baja el convex radius para boxes finos. Test de regresión agregado. Suite 1061/11018 verde.

---

## 0.1quinquies. Hito previo — F2H74 (2026-05-21)

**Cleanup UX (menú Ver) + capa de field-helpers del Inspector + fix undo.** Tag `v1.65.0-fase2-hito74`. Detalle completo en [`hitos/F2H74.md`](hitos/F2H74.md). El dev pidió reorganización de UX + limpieza del imgui disperso + recomendación (lente Unity/Unreal).

**Auditoría primero**: paneles/workspaces ya bien organizados (21 paneles en 6 workspaces = patrón Blender/Unity Layouts/Unreal Modes); las deudas "reorg de menús" (F2H18) y "HistoryStack residual" resultaron obsoletas → memorias borradas.

**Lo que entregó**:
- **Fase 1 — menú Ver**: `kCategories` tenía `World` vacío y omitía `Narrative` (paneles de diálogo invisibles en el menú). Fix a **Scene/Assets/Narrative/Gameplay/Debug**; `Item*`/`Quest*` → `Gameplay`, `NarrativeIntro` → `Narrative`.
- **Fase 2 — field-helpers** (`InspectorPanel_Internal.h`, estilo Unity `PropertyField` / Unreal `DetailsView`): `fieldDragFloat/3` + `fieldColorEdit3` colapsan el triplete *label i18n + widget + pushEditIfDone*. **25 campos** migrados en 6 partials, behaviour-preserving.
- **Fix undo muerto por `helpMarker`**: `trackPropertyEdit` lee el ID del último item; con el `helpMarker` (`TextDisabled`) entre widget y `pushEditIfDone`, el undo nunca disparaba. Reordenado en **Joint** (6 campos), **Trigger** `requiredTag`, **ForceField** `strength`. Transform NO se toca (su undo viene de `applyDeltaToSelection`; reordenar daría doble-undo).

**Suite 1046/10300 verde** (refactor de UI, validación visual).

---

## 0.2. Hito previo — F2H73 (2026-05-21)

**Triggers avanzados (filtro por tag + one-shot + enabled).** Tag `v1.64.0-fase2-hito73`. Detalle completo en [`hitos/F2H73.md`](hitos/F2H73.md). El `TriggerComponent` detectaba player + bodies pero siempre disparaba con cualquier cosa; F2H73 le agrega los filtros/modos estándar (Unreal `ActorHasTag`, Unity tag + `enabled`, `trigger_once` de Source). Flags sobre el `TriggerSystem` existente, sin reinventar nada.

**Lo que entregó**:
- **`TriggerComponent`** gana `requiredTag` (vacío = cualquier body; solo matchea el `TagComponent.name`, no afecta al player), `triggersOnPlayer` (default true), `oneShot` (default false; dispara una vez y queda muerto hasta recargar) y `enabled` (default true; master switch). Runtime nuevo `fired` (no serializado).
- **`TriggerSystem`**: gate al tope (`!enabled` y `oneShot && fired` cortan); player gated por `triggersOnPlayer`; el pre-collect de bodies guarda el `tag` de cada uno para el filtro `requiredTag`; `oneShot` se arma en el primer enter.
- **Editor**: Inspector ampliado (`InputText` requiredTag + 3 checkboxes + texto "one-shot ya disparó"), i18n en/es.
- **Serialización** round-trip aditiva; los 4 campos se escriben solo si != default (back-compat sin bump). Se agregó `TriggerComponent` al gate del `SceneSerializer` (trigger standalone sin mesh, como `Inventory`/`ForceField`).
- **Demo** `physics_triggers_demo.moodmap` (`ZonaTrigger` `required_tag="VIP"` + caja VIP dispara + caja Comun filtrada).

**Validación en vivo** (logs del `TriggerSystem` al panel Console): de dos cajas que cruzan solo la VIP disparó (`required_tag`), one-shot dispara una vez, `enabled=false` no despacha nada, player gated por `triggers_on_player`. Las 4 features confirmadas.

**Suite 1046/10300 verde**.

---

## 0.3. Hito previo — F2H72 (2026-05-21)

**Force fields / zonas de fuerza física.** Tag `v1.63.0-fase2-hito72`. Detalle completo en [`hitos/F2H72.md`](hitos/F2H72.md). Siguiente feature de Sub-fase 2.4. Referencia: Unreal `RadialForceComponent` + Unity Area Effectors.

**Lo que entregó**:
- **`ForceFieldComponent`**: zona sin colisión sólida que aplica fuerza a los `RigidBody` Dynamic adentro. `shape` (Box `halfExtents` / Sphere `radius`), `mode` (**Directional** = viento, fuerza constante en `direction` / **Radial** = explosión/atractor, fuerza centro→body, `strength>0` empuja, `<0` atrae), `strength`, `linearFalloff` (radial), `ignoreMass` (acceleration vs force), `enabled`.
- **`ForceFieldSystem`** (`src/systems/physics/`): cada frame pre-recolecta los Dynamic materializados y por cada zona habilitada aplica `addForce` a los de adentro (test OBB Box / distancia Sphere, mismo patrón que el `TriggerSystem`). Sin API nueva de física, sin tocar render.
- **Wiring**: antes del step (`updateRigidBodies`) en el loop de Play del editor + player, solo en Play.
- **Editor**: *Add Component → Physics → Force Field*, sección de Inspector (combos + condicionales + toggles, i18n), debug-draw del overlay (`F1`): zona violeta (Box OBB / Sphere 3 círculos) + flecha (Directional) / cruz radial (Radial), gris si disabled.
- **Serialización** round-trip aditiva; se agregó `ForceFieldComponent` al gate del serializer (zona standalone sin mesh).
- **Demo** `physics_forcefields_demo.moodmap` (WindZone arrastra cajas + ExplosionZone las dispara).

**Suite 1042/10287 verde**.

---

## 0.4. Hito previo — F2H71 (2026-05-21)

**Slider + Fixed joints — completa la familia de constraints de Jolt.** Tag `v1.62.0-fase2-hito71`. Detalle completo en [`hitos/F2H71.md`](hitos/F2H71.md). F2H65 había dejado Hinge/Distance/Point; este cierra los 2 que faltaban, reusando el andamiaje existente (Inspector + serialización + dispatch + debug-draw).

**Lo que entregó**:
- **`JointComponent`**: enum `Type` gana `Slider=3` (prismatic — rieles/cajones/ascensores/pistones; reusa `axisLocal` como dirección del riel + `sliderLimitMin/Max` en metros) y `Fixed=4` (suelda los 6 DOF; auto-detecta la pose relativa, sin pivot).
- **`PhysicsWorld`**: `createSliderConstraint()` (`SliderConstraintSettings` + `SetSliderAxis` + `mLimitsMin/Max`) y `createFixedConstraint()` (`FixedConstraintSettings` con `mAutoDetectPoint=true`, estilo Fixed joint de Unity). Mismo molde que los 3 de F2H65.
- **Inspector** combo de 5 tipos (Slider: eje + límites; Fixed: nota) + claves i18n en `en.json`/`es.json`. **Debug-draw** (`F1`): Slider cyan con riel + topes, Fixed naranja. **Serialización** round-trip aditiva.
- **Demo** `physics_joints_demo.moodmap` ampliado 4→8 entidades (Slider "ascensor" + Fixed "caja soldada").

**Fix lateral 1 — signo de los límites del slider**: Jolt mide la posición como `(point2−point1)·axis`, así que A (dueño) moviéndose a favor del eje da valor negativo → los límites del Inspector iban al revés. Fix: invertir al pasar a Jolt → convención pública intuitiva (+límite = a favor del eje). Atrapado por un test.

**Fix lateral 2 — bodies/joints siguen la pose visual al entrar a Play** (bug **general** de física, no solo joints): `updateRigidBodies` materializa el body en su pose inicial y no reposiciona los existentes; mover una entidad ya materializada y dar Play hacía que el body (pose vieja) pisara al Transform → el objeto saltaba. Fix en `enterPlayMode`: re-sincronizar cada RigidBody a su Transform (`setBodyPositionRot`) + marcar joints dirty. Solo afecta entidades con `RigidBodyComponent`.

**Suite 1037/10273 verde**.

---

## 0.5. Hito previo — F2H70.4 (2026-05-21)

**Ruedas que rotan (split-by-node) + HUD de conducción.** Tag `v1.61.0-fase2-hito70-4`. Detalle completo en [`hitos/F2H70-4.md`](hitos/F2H70-4.md). Cierra el Bloque H que F2H70.3 dejó pendiente: las ruedas dejan de estar horneadas en el chassis y rotan/giran independientes desde la pose física de Jolt.

**Lo que entregó (Bloque H)**:
- **`tools/glb/split_wheels.py`**: centra cada mesh-nodo de rueda en su hub + renombra al canónico `wheel_FL/FR/RL/RR` clasificando **por posición en espacio físico** (no por nombre original). Aplicado al `delorean.glb`.
- **`MeshRendererComponent.hideSubMeshPrefix`**: filtro exclude (complementa el include `subMeshName`). El chassis renderea todo menos los sub-meshes `wheel_*`; `SceneRenderer_Render` saltea cualquier `SubMesh` cuyo `name` empiece con el prefijo.
- **Auto-spawn de wheel-entities (`VehicleSystem`)**: 4 entities hijas que comparten `MeshAssetId` + materiales del chassis y rendean solo su sub-mesh. Spawn diferido fuera del `forEach` (crear entities durante la iteración invalida iteradores de entt). `wheelsNeedSpawn()` cubre handle-0, handle stale (delete+undo) y reload. En Play: `wheel.transform = chassisWorld * GetWheelLocalTransform(i)`.
- **`VehicleSystem::previewRest()`**: pose analítica de reposo en Editor mode (sin Play, Jolt no simula) — `wheelLocal = attachLocal - (0, restLen, 0)`. `EditorScene` lo llama en el branch no-Play.
- **Steering gradual**: rampa hacia el target (`k_steerTurnInRate=3.0`/s, `k_steerReturnRate=6.0`/s) en vez de snap `±1`. Las traseras giran más que las delanteras porque las delanteras reparten rotación entre rolido y steering (correcto).
- **Cleanup al borrar**: `EditorScene::deleteSelectedEntity` destruye las 4 wheel-entities + resetea `wheelEntities[]` antes del `DeleteEntityCommand` (sin esto quedaban ruedas huérfanas).
- **HUD de conducción**: `setDrivingHud(true)` al montar esconde `crosshair`/`health_number`/`stamina_bar`/`ammo_counter` y muestra `speedometer`. El velocímetro es un **popup anclado al mundo** que sigue al auto (`EditorPlayMode` proyecta `vtf.position + (0,1.3,0)` a pantalla → `HudState.vehicle_marker_x/y` + `onscreen`; `vehicle_speed_kmh = forwardSpeed * 3.6`).

**Fix lateral — wheel-entities no se serializan**: tenían `MeshRenderer` → entraban por el check `hasMr` y se guardaban, pero `wheelEntities[]` no persiste → recargar dejaba **8 ruedas** (4 huérfanas + 4 respawneadas). `SceneSerializer` ahora saltea las entities con tag `wheel_*` (el `VehicleSystem` las rematerializa). Eliminado el `isWheelTag` capital (código muerto F2H67). Test de regresión agregado.

**Assets movidos** (no integrados aún): `armor-car` y `tesla` a `assets/vehicles/<nombre>/` — geométricamente válidos (1u=1m, +Z forward, sin mirrors) pero les falta `.moodvehicle` + procesar ruedas (diferido al gestor de vehículos, backlog).

**Suite 1030/10233 verde** (1029 + test de regresión del serializer).

---

## 0.6. Hito previo — F2H70.3 (2026-05-20)

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

- **Sub-fase 2.4** (Física avanzada): **CERRADA** ✅ con F2H75. Incluyó F2H65 (Hinge/Distance/Point) + F2H66 (Ragdolls Mixamo) + F2H67 (Vehicle physics SA) + F2H68 (auto-ragdoll por impacto) + F2H70.2/3/4 (vehicle polish + browser + wheels) + F2H71 (Slider/Fixed joints) + F2H72 (Force fields) + F2H73 (Triggers avanzados) + **F2H75 (Cloth/soft body)**. Todos los items del plan original (F2H24-F2H28) entregados.
- **Sub-fase 2.6** (Render polish): F2H55, F2H56, F2H58, F2H59, F2H60, F2H61, F2H62, F2H63, F2H64 cerrados. AUDIT-1, AUDIT-2, AUDIT-3 cerrados.

### Próximo — a definir con el dev

Con la Sub-fase 2.4 cerrada, las direcciones candidatas (a elegir con el dev):

- **Sub-fase 2.6 — Pipeline AI** (F2H35-F2H40 originales): Mixamo importer cubierto parcialmente por F2H49. Pendiente: Blender MCP server, armas procedurales, generador de props, validación automática.
- **Sub-fase 2.7 — UI/UX final + cierre Fase 2** (F2H41-F2H44 originales): theming, atajos configurables, tutorial in-app, tag `v2.0.0`.
- **Backlog de vehículos**: integrar `armor-car` + `tesla` (faltan `.moodvehicle` + procesar ruedas), pipeline de packs multi-auto (`tools/glb/split.py` + `--scale`).

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
