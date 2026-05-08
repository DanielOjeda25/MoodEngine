# Log de decisiones técnicas — Fase 2

Registro cronológico de decisiones arquitectónicas no triviales tomadas
durante la **Fase 2** (F2H1 en adelante). Formato por entrada: contexto,
decisión, razones, alternativas descartadas, condiciones de revisión.

> **Decisiones de Fase 1 (Hito 0 → Hito 42)** archivadas en
> [`archive/DECISIONS_Fase1.md`](archive/DECISIONS_Fase1.md). Split
> aplicado en F2H26 cierre (2026-05-08) cuando este documento superó
> las 4000 líneas.

---

## 2026-05-08: F2H28 — Editor de mapas 4-viewport (split en 2 hitos, fondo negro, snap solo de display)

**Problema:** F2H28 originalmente quería entregar **todo** el editor estilo Hammer en un solo hito: layout 4-viewport + render orto wireframe + grid 2D + click-select + block tool (dibujar rectángulo en orto → crear brush) + drag-edit (mover brushes desde orto con grid snap) + vertex/edge edit. Estimación: ~12 bloques con bugs cruzados (cada feature de edición depende del render + selection + grid; un bug en un layer rompe los otros).

**Decisión:** **Split explícito en 2 hitos**. F2H28 entrega los **fundamentos**: layout + render + grid + click-select + snap visual. F2H29 entrega las **3 features de edición**: block tool, drag-edit, vertex/edge edit. El snap step expuesto por F2H28 (`m_hammerSnapStep` cycleable con Ctrl++/Ctrl+-) se aplica al delta del drag en F2H29.

**Razones:**
- **Bug isolation**: el render multi-viewport + grid + click cross-viewport ya es código nuevo crítico. Sumar drag-edit (que muta el Transform del brush en vivo viendo update en las 4 vistas) duplica la superficie de bugs.
- **MVP visual primero**: el dev puede USAR el workspace inmediatamente para navegar/seleccionar; las features de edición vienen después con esa base ya validada.
- **Patrón ya usado**: F2H25 (cull overlap) + F2H26 (runtime-load) splittearon dos features tightly-coupled del mismo dominio CSG. Mismo approach acá.

**Alternativas descartadas:**
- **Hito grande único de 12+ bloques**: bugs cruzados retrasan todo, no hay punto intermedio para validar visualmente con el dev. Descartado.
- **Diferir block tool a F2H30+**: los 3 (block, drag-edit, vertex) son tightly-coupled — comparten el manipulador 2D del orto + el snap. Splittear más fino agrega ceremonia sin separar dominios reales. Descartado.

---

**Sub-decisión 1 — Label castellano "Editor de mapas" (no "Hammer"):**

Plan original llamaba al workspace "Hammer". Cambio durante implementación: alineamos con la convención F2H22 (workspaces orientados a TAREAS — "Layout", "Programar", "Materiales") y usamos label "Editor de mapas". Internamente seguimos hablando del estilo Hammer/Source como inspiración técnica, pero el dev ve el nombre de la tarea.

**Razones:** consistencia con los otros 3 workspaces; "Hammer" es referencia que solo entiende quien conoce Source SDK; "Editor de mapas" describe qué hace el dev cuando entra ahí.

---

**Sub-decisión 2 — Fondo NEGRO en lugar de gris claro `#C8C8C8`:**

Plan original especificaba paleta Valve+GMod con fondo gris claro `#C8C8C8` (mimic Hammer original). Cambio durante validación visual: dev pidió *"quiero cambiar el fondo gris por negro"* — el wireframe celeste GMod `#6CC1E5` resalta mucho mejor sobre negro que sobre gris.

Ajustes acompañantes: grid menor cambió de `#7A7A7A` (visible sobre gris claro) a `(40,40,40)` (sutil sobre negro, no compite con el wireframe). Grid mayor `#F58220` (naranja Valve) preservado — pop sobre negro mejor que sobre gris.

**Razones:** el plan era guess; la validación visual es la fuente de verdad. Fondo negro es la convención de muchos editores modernos (Unreal Editor wireframe, Houdini network views) y subjetivamente más cómodo para sesiones largas.

---

**Sub-decisión 3 — `OrthoCamera` en `editor/panels/scene/`, NO en `engine/scene/cameras/`:**

Plan original sugería poner `OrthoCamera` en `engine/scene/cameras/Camera.h` (junto al `EditorCamera` orbital y `FpsCamera` del player). Decisión durante implementación: ponerlo en `editor/panels/scene/OrthoCamera.h` junto al `OrthoViewportPanel` que la usa.

**Razones:**
- `OrthoCamera` es **solo del editor** — el `MoodPlayer` no la necesita (no tiene workspace orto).
- Acoplamiento mínimo: el SceneRenderer NO conoce `OrthoCamera` directamente (recibe `panOffset` + `worldHeight` como params plain). Si en el futuro hace falta usarla en `engine/`, mover es trivial.
- Evita layering issues: `engine/` no debe depender de `editor/`. Si `OrthoCamera` viviera en engine y luego algún partial de engine la incluyera, romperíamos la regla.

---

**Sub-decisión 4 — `pickEntityFromRay` como helper público + `pickEntity` delega:**

Bloque F necesitaba picking ortográfico (rayos paralelos). Tres approaches posibles:

1. **Duplicar el loop**: copiar el `forEach<TransformComponent>` con AABB/sphere tests a una función nueva. Descartado: 25 líneas duplicadas, drift garantizado.
2. **Refactor con `unproject lambda`**: pasar a `pickEntity` un functor que arma el rayo. Descartado: API rara, hard de testear.
3. **Extraer helper público `pickEntityFromRay(scene, origin, dir, assets)`** y hacer que `pickEntity` (la versión perspective con view+proj+ndc) sea un wrapper que arma el rayo y delega. **Elegido.**

**Razones:** los tests existentes de `pickEntity` siguen verde sin cambios (mismo loop interno). El nuevo helper queda público y reusable para futuros picking sintéticos (ej. raycast desde script Lua, F2H30+).

---

**Sub-decisión 5 — Snap step solo de display en F2H28, aplicado a drag en F2H29:**

Plan original: `m_hammerSnapStep` cycleable con Ctrl++/Ctrl+- + label "Grid: Nu". Validación durante implementación: el dev preguntó *"luego habrá el snap to grid?"* — confirmando que el snap visual NO actúa sobre movements aún.

**Decisión:** F2H28 expone el valor (UI + uniform del grid shader) pero NO lo aplica al delta del drag (porque drag-edit es F2H29). El handler Ctrl++/Ctrl+- vive en `EditorApplication.cpp::processEvents` con guard de workspace activo.

**Razones:** mantiene F2H28 puramente visual + de selección. La aplicación al drag entra como sub-bloque natural de F2H29 (`pos = round(pos / snap) * snap`). Sin riesgo de regresión: F2H29 solo necesita LEER el valor existente.

---

**Condiciones de revisión:**
- Si el costo del render orto (~3x CPU del frame perspectivo cuando workspace activo es "Editor de mapas") emerge como cuello, optimizar con frustum culling per-viewport o reducir ortos a 2 (top + frontal) configurable.
- Si el dev pide volver al fondo gris (rechazo subjetivo del negro), revertir; los colores quedan como constantes nombradas en `SceneRenderer_Ortho.cpp` para hacer el cambio trivial.
- Si F2H29 (drag-edit) revela que `OrthoCamera` necesita state adicional (ej. clipping plane near/far user-configurable), promoverla a `engine/` cuando ese state aparezca, NO antes.

---

## 2026-05-03: Reorganización arquitectónica de `src/` por dominios (F2H1)

**Problema:** post-v1.0.0 el árbol `src/engine/` era flat con 11K líneas en sub-carpetas planas (`render/`, `scene/`, `physics/`, etc.). Cualquier hito futuro de Fase 2 que aporte 3-5K líneas adicionales (CSG, dialog, quest, material node-graph) iba a contaminar más esa estructura. Antes de empezar a sumar features, reorganizar.

**Decisión:** subdividir `engine/`, `systems/` y `editor/` por sub-dominio explícito. Plan completo en `PLAN_FASE2.md` sección 2. Los movimientos se hicieron bloque por bloque con commits atómicos:

- **Bloque A — `engine/render/`**: split en `rhi/` (interfaces), `backend/opengl/` (impl GL — único lugar que incluye `glad/gl.h`), `pipeline/` (Fog, LightGrid, math), `resources/` (Mesh/Material assets), `scene_renderer/` (coordinador). 5 sub-commits.
- **Bloque B — `engine/scene/`**: split en `core/` (Scene, Entity, Cameras), `components/` (Components.h), `serialization/` (movido desde `engine/serialization/`), `queries/` (ScenePick/ViewportPick). 4 sub-commits.
- **Bloque C — `engine/physics/`**: PhysicsWorld a `world/`, placeholders para `components/` `character/` `queries/`.
- **Bloque D — `engine/animation/audio/scripting/assets/`**: cada uno subdividido. 4 sub-commits.
- **Bloque E — `engine/world/`**: GridMap+Pathfinding a `grid/`, placeholders `csg/` (F2H9+) + `streaming/` (Fase 3).
- **Bloque F — `engine/game/`**: subdivisión en manifest/overlay/state + placeholders dialog/quest/inventory + nueva carpeta `engine/i18n/` (F2H5).
- **Bloque G — `systems/`**: subdividido por dominio: render/physics/animation/ai/particles/audio/light/scripting.
- **Bloque H — `editor/`**: split en `application/` (EditorApplication + 6 partials), `ui/` (Dockspace/MenuBar/StatusBar/EditorUI), `panels/{scene,assets,debug,world}/` por categoría. 3 sub-commits.

**Razones:**
- **Cabe en mente.** Cada subcarpeta es un dominio identificable; nuevas features tienen lugar obvio.
- **Reglas de dependencia aplicables.** Con `backend/opengl/` aislado, el "único lugar con glad/gl.h" se vuelve operacional, no aspiracional.
- **Placeholders `.gitkeep` para hitos futuros.** Cuando F2H9-F2H16 agregue brushes 3D, hay carpeta esperándolos. Idem dialog/quest/inventory en F2H29-F2H31.
- **Zero regression.** Cada sub-bloque cierra con la suite intacta (319/6613). Editor + MoodPlayer compilan limpios y se ven idénticos a v1.0.0.

**Trade-offs:**
- Se pierde: profundidad de paths más larga (ej. `engine/scene/serialization/SceneSerializer.h` vs `engine/serialization/SceneSerializer.h`). Aceptable: el IDE autocompleta y el cambio de path se hizo con sed bulk, no a mano.
- Se gana: superficie de "engine flat" desaparece. Cualquier dev que llega al repo en F2H10+ encuentra `engine/world/csg/` directamente y sabe dónde editar.

**Revisar si:** algún sub-bloque queda con 1 archivo solo durante mucho tiempo (puede colapsarse). Por ahora todos justifican existencia futura (placeholders documentados con `.gitkeep`).

## 2026-05-03: Tracy adoptado como profiler oficial (F2H2)

**Contexto:** F2H2 abre la sub-fase de optimización de Fase 2. Antes de empezar a optimizar (frustum culling F2H3, LOD F2H4, batching futuro) hay que saber **dónde** se va el tiempo de frame. Sin profiler real, las decisiones serían intuición.

**Decisión:** integrar **Tracy v0.11.1** como profiler. Cliente linkeado al ejecutable vía CPM (target `Tracy::TracyClient`), controlado por la opción CMake `MOOD_PROFILE` (default ON). Macros propias `MOOD_PROFILE_FRAME / SCOPE / FUNCTION / PLOT` en `src/core/Profiler.h` que expanden a Tracy cuando ON y a `do{}while(0)` cuando OFF (coste cero en builds finales).

Instrumentación inicial: 10 zonas en `EditorApplication::run` (eventos / UI / física / scripts / animación / nav / partículas / audio / render / present) + 8 sub-zonas dentro de `SceneRenderer::renderScene` (shadow / skybox / light grid / PBR static / PBR skinned / particles / debug / post-process). Plots de FPS y entity count.

**Razones:**
- **Cliente embedded ≠ servidor externo.** El cliente vive linkeado, el servidor es `tracy-profiler.exe` portable que se conecta por TCP. Con server cerrado la overhead es ~1-2% del frame; con server abierto ~5-8%. En Release con `MOOD_PROFILE=OFF` las macros desaparecen del binary.
- **Granularidad real**: Tracy mide por zona con stddev / min / max — no es una vista global tipo "frame ms". Con 3163 frames de captura ya tenemos `mean / max / stddev` por cada zona.
- **`tracy-csvexport.exe`** permite extraer los % de zona a CSV sin la GUI — útil cuando hay mismatch de versión o headless. Esta sesión lo usó: el `tracy-profiler.exe` que el dev bajó tenía protocolo distinto (mismatch), pero `tracy-capture.exe` capturó el `.tracy` y `tracy-csvexport.exe` lo decodificó. La data del baseline final salió de ese pipeline.
- **CMake CPM** lo trae en una línea, sin dependencia adicional al sistema.

**Alternativas consideradas:**
- **Optick**: similar feature set, menos mantenido. Tracy tiene más adopción y mejor visualización.
- **`std::chrono` ad-hoc**: sin overhead pero sin granularidad por zona ni vista temporal — solo da un total. Insuficiente para identificar cuellos.
- **Profilers nativos (Visual Studio Profiler, Superluminal)**: excelentes pero requieren instalación + licencia / setup. Tracy es portable y lib propia, va con el repo.
- **PIX / RenderDoc**: orientados a GPU, no a CPU + frame-level. Complementarios pero distintos.

**Trade-offs:**
- Se pierde: tamaño binario crece ~500KB con Tracy linkeado en Debug. Aceptable en builds de desarrollo, irrelevante en Release con `MOOD_PROFILE=OFF`.
- Se gana: cuellos identificables por zona con stddev. Para F2H2 inmediatamente útil: `PBR::staticPass` se queda con el 70.74% del frame con 836 cubos = decisión clara para F2H3.

**Lección aprendida (mismatch de versión):** el cliente linkeado y el `tracy-profiler.exe` deben ser **exactamente la misma versión**. Bajar el ZIP del tag exacto (v0.11.1 en este caso). El protocolo TCP cambia entre versiones y el server muestra "Protocol mismatch" al conectar. Si pasa, fallback al pipeline `tracy-capture.exe` → `tracy-csvexport.exe`.

**Revisar si:** Tracy deja de ser mantenido (improbable a corto plazo, proyecto activo) o si una migración a Vulkan/D3D12 requiere features de profiling GPU más profundas (entonces evaluar PIX o RenderDoc en paralelo, no como reemplazo).

## 2026-05-03: Frustum culling plano, no jerárquico (F2H3)

**Contexto:** F2H2 midió que `PBR::staticPass` se llevaba el 70.74% del frame con 836 cubos. F2H3 ataca eso descartando entidades fuera del frustum antes del draw call. La pregunta: ¿implementación plana (loop linear sobre todas las entidades) o jerárquica (BVH / octree / quadtree espacial)?

**Decisión:** **plana** para v1, con **API diseñada para que el upgrade interno a estructura jerárquica sea transparente** para los callers. El loop de PBR static itera por entidad como antes, agrega 3 líneas: calcular AABB world, test contra frustum, `continue` si fuera. El header `Frustum.h` expone `aabbVisible` y `worldAabb` en el estilo header-only del motor; los callers no iteran por dentro de una estructura — la API es "test este AABB contra este frustum".

**Razones:**
- **No es cuello todavía.** El test AABB-vs-frustum con truco p-vertex es 1 dot product por plano → 6 dots × ~1 ns = 6 ns por entidad. Para 836 entidades = 5 µs totales. Para 10K entidades = 60 µs. Para 100K = 0.6 ms. Cuando el dato medido diga que el loop linear es cuello, agregamos BVH como hito propio. Hoy no.
- **Costo de implementación.** Un loop de 3 líneas vs un BVH (~300-500 LOC con build, refit en cambios de transform, traversal por frustum, tests). El BVH además agrega risk de bugs en refit incorrecto cuando un transform cambia.
- **API extensible.** Hoy `aabbVisible(aabb, frustum)` se llama por entidad. Mañana, si reemplazamos el loop por `bvh.queryFrustum(frustum) → [aabbs visibles]`, ese cambio queda aislado en `SceneRenderer::renderScene` — el resto del código no se entera.
- **Profile don't guess.** Filosofía Fase 2 (ver `docs/PLAN_FASE2.md`): no optimizar antes de medir. F2H3 nace con datos, no con intuición. El próximo nivel también nacerá así.

**Alternativas consideradas:**
- **BVH dinámico desde día 1**: rechazado por scope. ~5x más código, más superficie de bugs, sin necesidad inmediata.
- **Octree estático**: no encaja con el modelo de mundo dinámico (entidades se mueven con scripts y physics). Refit constante haría perder la ventaja.
- **Spatial hashing (grid uniforme)**: alternativa simple al BVH; postergado para evaluar si emerge como necesidad real.

**Scope explícito de F2H3:**
- Cull solo en `PBR::staticPass`. Shadow + skinned passes excluidos.
  - **Shadow**: cada light tiene su propio frustum (CSM o radius esfera para points). Implementarlo correcto es ~2x el código de v1, y el baseline F2H2 dice que `ShadowPass` ni aparece en top 10 zonas. ROI bajo.
  - **Skinned**: sub-1% del baseline. Cuando emerja en un baseline futuro lo agregamos.
- AABB world-space calculado **on-the-fly cada frame** (sin caché por dirty-flag). Costo medido ~30 ns por entidad, despreciable. Caché entra solo si una medición lo justifica.
- Test conservador: descarta solo cuando los 8 vértices del AABB están del lado negativo de un mismo plano (truco p-vertex). Falsos positivos posibles (AABB que pasa pero no se ve), falsos negativos NO. Es la garantía correcta — preferimos dibujar de más a "que se note un mesh faltando".

**Trade-offs:**
- Se pierde: rendimiento extra que un BVH daría a >100K entidades (cuando ese escenario aparezca).
- Se gana: 3 líneas de código + 12 tests + API estable. Rendimiento medido x13.3 cuando el viewport está vacío de mesh (escena 10K, cámara apuntando lejos).

**Validado en producción** (testtrace2.tracy, 1162 frames, GTX 1660):
- `PBR::staticPass` 70.74% → 15.73% del frame (promedio mezcla escenarios).
- Max de la zona: 2574 ms → 251 ms (10x menos varianza en peor caso).
- `PBR::CulledStatic` plot reporta hasta 835/836 entidades descartadas.
- Costo del culling: <<1% del frame, no entra en top 10 zonas.

**Revisar si:** una medición futura muestra que el loop linear (no el cull en sí) es cuello — escenario probable solo a >100K entidades visibles, lo cual el motor todavía no soporta por otros cuellos pendientes (LOD F2H4, batching futuro).

## 2026-05-03: F2H4 = instancing (swap con LOD original) + decisiones MVP de instancing

**Contexto:** `PLAN_FASE2.md` originalmente colocaba F2H4 = LOD. Cuando llegó el momento de implementarlo después de F2H3, los datos medidos del baseline F2H2 mostraban que el cuello real era **`PBR::staticPass` 70.74% del frame, mean 42.5 ms / 836 entidades = 50 µs por cubo de 12 tris**. Eso es CPU-bound en draw call submission, no GPU-bound en triangle processing. LOD ataca lo segundo; instancing ataca lo primero. Si hubiéramos seguido el plan literal, F2H4 LOD habría dado mejora marginal (los cubos primitivos ya tienen 12 tris, no hay LOD que reducir) y dejado el cuello real intacto.

**Decisión:** **swap F2H4 ↔ F2H5**. Este F2H4 = "Instancing del pase opaco estático", F2H5 = "LOD" (postergado). El plan F2H4 LOD no se descarta — sigue siendo necesario cuando aparezca contenido con meshes de alto poly (Fox.glb, CesiumMan.glb, props complejos del catálogo Kenney). Hoy no hay con qué medirlo útil.

**Validación medida (test3.tracy, 4993 frames):**
- `PBR::instancedPass` mean **0.88 ms** vs `PBR::staticPass` baseline F2H2 mean **42.5 ms** = **~48x más rápido por draw**.
- Escena 10K (836 cubos): antes 4 FPS / 836 draws, ahora **60 FPS / 3 draws** (vsync cap; el rendering ya no es el cuello).
- Escena 100K (8336 cubos): antes congelaba el editor, ahora **10.4 FPS / 3 draws / 82K tris** = editable. F2H3 + F2H4 en cadena hicieron viable un escenario que el motor literalmente no soportaba.
- Costo del helper `groupByBatch` + culling: <<3% del frame con 836 cubos. Despreciable.

**Decisiones MVP del instancing (sub-decisiones, valen revisarse cuando emerjan):**

1. **Re-upload del VBO de instancias cada frame con `glBufferData` orphan-then-fill**: el más simple. Para 836 mat4 = ~50 KB, transferencia en microsegundos. Probable que resista hasta 10K-20K instancias visibles. Cuando emerja (medición futura), upgrade a *persistent mapped buffers* o *triple buffering* sin tocar la API pública (`OpenGLInstanceBuffer::upload`).
2. **Sin caché de matrices model**: las recalculamos cada frame en `groupByBatch`. Para 836 entidades = micro-segundos. Cuando 100K+ entidades estáticas justifiquen caché, agregar dirty-flag en `TransformComponent`. La API de `groupByBatch` no cambia.
3. **Reglas estrictas de batcheable**: 1 submesh + `materials.size() ≤ 1`. Multi-submesh y materiales mixtos caen al path no-instanced del F2H3. Conservador para v1 — extender el grouping a "batch por submesh" cuando contenido real lo justifique.
4. **Shader instanced separado** (`pbr_instanced.vert` + reuso de `pbr.frag`): no usamos `#define INSTANCED` con un solo .vert porque mezcla código con/sin atributos en el mismo archivo dificulta debug de VAO state. Los dos `.vert` son menos LOC totales que el toggle.

**Trade-offs del enfoque MVP:**
- Se pierde: rendimiento extra que persistent mapped + scene caching darían a >100K entidades.
- Se gana: ~700 LOC totales (helper + RHI + shader + cableo + tests). API limpia y extensible. **Cuello del 70% medido en F2H2 → 2.5%** con esta inversión.

**Lección aprendida:** atacar lo que se mide, no lo que el plan original asumió. F2H2 fue exactamente para esto — para que F2H3+F2H4 entren con datos. Si hubiéramos seguido el plan literal, habríamos hecho LOD primero (mejora marginal) y descubierto el cuello real recién después.

**Revisar si:**
- El upload del VBO emerge como cuello (medible en escenas con cambios bruscos de cuántas entidades son visibles por frame).
- Una medición muestra que el helper `groupByBatch` es cuello (reupload cada frame con 100K+ entidades).
- Aparece contenido real con multi-submesh frecuente que justifique extender el grouping a "batch por submesh".

## 2026-05-03: F2H5 = virtualización ImGui Hierarchy + lección sobre predicciones de % de mejora

**Contexto:** F2H4 destapó el cuello `UI::draw` 19% del frame con 8336 entidades. F2H5 según `PLAN_FASE2.md` era LOD; segundo swap consecutivo (F2H4 ya había swapeado con LOD original). El plan F2H5 predijo que virtualizar el panel Hierarchy bajaría `UI::draw` de 19% a <2%, llevando el FPS de 10.4 a 12-15.

**Decisión:** seguir adelante con F2H5 = virtualización Hierarchy. Refactor con `ImGuiListClipper`, helper `collectHierarchyEntries` extraído a `HierarchyCollect.cpp` para testing.

**Resultado medido (CSV + repetición de medición sin Tracy):**
- 100K_full_view: 96 → 90.5 ms / 10.4 → 11.0 FPS = **~6% mejora** (no el 25-40% predicho).
- 100K_no_view: 89.9 → 86.5 ms / 11.1 → 11.5 FPS = **~4% mejora**.

**Por qué falló la predicción:** el plan asumía que `UI::draw` (19% del frame) era **dominantemente** el panel Hierarchy. La realidad: ese 19% se reparte entre Hierarchy + Inspector (con muchos componentes seleccionados) + AssetBrowser + Performance HUD + gizmos + tooltips. Atacar solo el Hierarchy mejora una fracción, no el total.

**Lo que F2H4 destapó realmente** (visible al cerrar F2H5): el cuello con 8336 entidades NO es un panel específico — es **scene iteration distribuida**. Multiples sistemas (`Animation/Script/Nav/Particle/Audio/Trigger`) hacen `scene.forEach<...>` cada frame, sumando costo lineal por entidad. F2H5 no atacaba eso.

**Aprendizaje aceptado para futuros hitos:**
1. **Las predicciones de % de mejora con escenas grandes son frágiles**. Tracy mide zonas con nombre fijo — `UI::draw` engloba todos los panels, no separa por panel. Antes de predecir mejora, instrumentar sub-zonas (`UI::Hierarchy::draw`, `UI::Inspector::draw`, etc.) para tener attribución real.
2. **Atacar lo medido, no lo asumido** (lección F2H4 reaplica). Sin sub-zonas en F2H4 no podíamos saber qué fracción del 19% era cada panel.
3. **Una mejora del 6% sigue siendo positiva** y el código del refactor es correcto: el patrón ListClipper queda como template para `AssetBrowser`, `Inspector` listas, etc. cuando crezcan.

**Decisión de continuar (no revertir):**
El refactor en sí es código limpio y reusable. No introduce regresiones (suite 345/6736). El comportamiento visual es idéntico. **Cerrar F2H5 con la mejora real documentada** > mantenerlo abierto buscando más speedup.

**Aclaración crítica de scope (anotada al cerrar F2H5):**
Todas las mediciones de F2H2-F2H5 son en **build Debug + Tracy ON**. MSVC Debug agrega 5-10x overhead vs Release optimizado. Antes de invertir más hitos en optimización CPU, **medir Release** es el paso obligatorio. Predicción (sin medir aún): 100K_full_view pasaría de 11 FPS / 90 ms (Debug) a 45-60 FPS / 16-20 ms (Release).

**El stress 100K es patológico, no representativo:**
8336 cubos primitivos individuales. Mapas reales tienen muchos más triángulos pero muchas menos entidades:
- HL2 nivel típico: 500K-2M tris en 500-1500 entidades (geometría del nivel = pocos meshes grandes + props sueltos).
- Skyrim escena: 1-5M tris en 200-500 entidades.
- Doom Eternal encuentro: 10-50M tris en 1000-3000 entidades.

F2H3 + F2H4 cubren ambos casos: instancing para props repetidos, frustum cull para mesh grande no visible. **El motor con 100-300 entidades reales en una escena va a estar perfecto** — el stress test 8336 es un test de extremo, no un escenario de producto.

**Revisar si:**
- Una medición de Release muestra que el motor en producto sigue siendo limitado (improbable).
- Otros panels (Inspector, AssetBrowser) emergen como cuellos en escenas reales y justifican aplicar el mismo patrón ListClipper.
- Aparece la necesidad de instrumentar sub-zonas Tracy por panel para tener attribución correcta antes de futuros hitos UI.

## 2026-05-03: F2H6 LOD con cache en disco — cimiento sólido vs parche

**Contexto:** F2H6 retoma el LOD original (era F2H4 plan, postergado dos veces — primero por instancing F2H4, después por virtualización F2H5). El dev preguntó explícitamente "¿esto va a ser parche o cimiento sólido?" antes de implementar. Las decisiones siguen lo que hacen Unity, Unreal y Godot.

**Decisión 1 — Persistencia con cache en disco (NO regeneración pura):**

Lo que hacen los motores serios:
- Unity LOD Group → persiste en `.meta` del asset → load instant.
- Unreal Mesh Editor → persiste en `.uasset` → load instant.
- Godot auto-genera al import → persiste en `.scn` → load instant.

Implementación: `LodCache` lateral al formato `.moodmap` en `assets/.cache/lods/<hash>.moodlod`. FNV-1a 64-bit del logical path como nombre del archivo. Header binario con magic `MLOD` + version + mtime + size del source para invalidación. Borrar el dir `.cache/` no rompe nada — solo fuerza re-generación al próximo arranque.

**Razones:**
- **Tiempo de arranque**: regenerar 50 meshes complejos al cargar un proyecto = 5-30 seg de spinner. Persistir = abre instantáneo después del primer arranque.
- **Determinismo**: lo que ve el dev en el editor === lo que ve el jugador.
- **Workflow futuro**: cuando alguien quiera importar LOD custom de Blender, la API ya está lista (sub-hito futuro).
- **Si meshoptimizer hace algo feo**, el dev puede borrar el cache y regenerar (operativo, no ideal — UI editor es hito futuro).

**Por qué cache lateral, no schema bump:** bumpear `.moodmap` para guardar LODs implicaría migración + back-compat para todos los proyectos existentes. El cache es privado del workspace, no afecta proyectos compartidos. Si emerge necesidad de "guardar LODs en el proyecto" (ej. compartir LODs custom con el equipo), entonces sí schema bump como hito propio.

**Decisión 2 — Per-MeshAsset con default global (NO ranges hardcoded):**

Lo que hacen los motores serios: todos (Unity LOD Group, Unreal Mesh Editor, Godot) permiten configurar los rangos **por mesh**. Razón: un personaje hero usa LOD 0 hasta más lejos; un ladrillo de fondo usa LOD 2 desde cerca. Hardcoded global no escala.

Implementación: campo `lodDistances{30, 80}` en `MeshAsset` (default global). Override per-mesh disponible programáticamente. UI editor para cambiarlo es hito futuro — por ahora el dev edita los defaults o el código.

**Por qué hardcoded global hubiera sido parche:** ata el motor a un escenario "uniforme" que no existe en juegos reales. Cuando aparezca el primer mesh que necesita rangos custom (probable: un personaje hero visible desde lejos en FPS), tendríamos que re-arquitectar.

**Decisión 3 — Skinned meshes saltean LOD generation en v1:**

Reducir vértices en un mesh skinned implica re-mapear los bone weights consistentemente (cada vértice tiene 4 índices + 4 pesos que apuntan al esqueleto). meshoptimizer trabaja sobre vertex positions; los bone weights del vertex eliminado tendrían que redistribuirse a sus vecinos. Implementación correcta es scope grande con risk de bugs visuales (animación que se rompe en LOD 1/2).

Para v1: skinned siempre usa LOD 0. Postergado a hito propio cuando emerja un benchmark con >50 personajes skinned simultáneos. Hoy con CesiumMan + Fox como únicos skinned del catálogo, el cuello no aparece.

**Decisión 4 — Auto-gen al loadMesh, no en el editor manualmente:**

Workflow: el dev arrastra un `.glb` al proyecto, los LODs se generan automáticamente, se cachean en disco, se ven inmediatamente. Sin pasos manuales. Si el resultado de meshoptimizer no le gusta, edita el source y re-arrastra (mtime cambia, cache se invalida).

Alternativa rechazada: UI editor para "Generar LOD" / "Importar LOD custom" / barras visuales de threshold (estilo Unity LOD Group). Es scope grande para v1 — UX nice-to-have pero no esencial. Hito futuro cuando los workflows reales lo justifiquen.

**Trade-offs:**
- Se pierde: control fino sobre cada LOD (no podés "tocar" el resultado de meshoptimizer en el editor todavía). UX de Unity LOD Group con barras y screen-space size visual.
- Se gana: workflow zero-config. El dev importa un mesh y "simplemente funciona". Cache persiste, así que después de la primera vez no hay penalty de tiempo. API queda preparada para los hitos futuros sin re-arquitectar.

**Validación medida**: pendiente con escena de meshes complejos. Los stress patológicos actuales (cubos primitivos) no se benefician del LOD porque ya están en el mínimo (12 tris). Cuando entre contenido real (Fox.glb ~1500 tris, kenney_survival props ~500-2000 tris cada uno), spawnar 50+ instancias a distintas distancias mostrará el speedup esperado: LOD 1 = 750/250-1000 tris, LOD 2 = 225/75-300 tris.

**Lo que NO sería cimiento sólido (rechazado):**
- ❌ Regenerar siempre al load (sin cache): tiempo de arranque crece linealmente, ningún motor profesional hace esto.
- ❌ Schema `.moodmap` bump para guardar LODs: contamina el formato del proyecto, dificulta back-compat.
- ❌ Sin override per-mesh: ata el motor a un escenario uniforme irreal.
- ❌ Hardcoded de "siempre 3 LODs": Unity/Unreal soportan hasta 8 niveles. v1 usa 3 (0/1/2) con espacio para extender (basta agregar `lod3Submeshes` y otro threshold).

**Revisar si:**
- Una medición real con meshes complejos muestra que los ratios 50%/15% no son los óptimos (probable que ajustemos por dominio: characters quizás 60%/30%, props 50%/15%, vegetación 40%/10%).
- El cache crece mucho (>500MB en proyectos grandes) y hace falta política de evicción.
- Aparece la necesidad de un sub-hito UI para "Editor de LOD" (custom LODs importados, override per-mesh visual, sliders de threshold).
- Meshes con multi-submesh + materiales distintos por submesh emergen como caso común y necesitan extensión del flujo (hoy multi-submesh CAE al fallback no-instanced del F2H3, sigue funcionando pero sin LOD).

## 2026-05-04: Multi-mapa intra-proyecto (F2H8) + bug arquitectónico de "tags auto-generados"

**Contexto:** el `.moodproj` soporta `maps[]` desde Hito 6 pero el editor solo opera sobre `defaultMap`. El "Save As" del menú Archivo era un stub explícito ("no implementado, requiere UI multi-mapa"). El dev eligió cerrar la deuda con la opción C (multi-mapa intra-proyecto) en lugar de "Save Project As" (copiar carpeta entera, diferido a hito futuro).

**Decisión 1 — Scope multi-mapa**: F2H8 implementa el CRUD completo de mapas dentro de un proyecto: New, SaveAs, Open, SetDefault, Delete. Sin schema bump (todo lo necesario ya existía). Sin back-compat de proyectos viejos (el dev confirmó que están borrados). UX bajo `Archivo > Mapa` con submenu listando `project.maps[]`.

**Decisión 2 — Helper `MapsManager` PURO**: la lógica de "lista + default + current con invariantes" vive en `editor/project/MapsManager.{h,cpp}`, sin acoplarse a UI ni a disco. Invariantes que mantiene: ≥1 mapa, default + current dentro de la lista, dedup por `generic_string` (paths con separadores distintos = mismo path). Testeado con 13 cases en `tests/test_maps_manager.cpp`.

**Decisión 3 — Snapshot de mapas en EditorUI**: para que `MenuBar` pueda dibujar el submenu sin acoplarse al `Project` directamente, `EditorUI` mantiene un snapshot `(maps[], currentMap, defaultMap)` que `EditorApplication::syncMapsSnapshot()` refresca tras cada operación de mapas. Patrón consistente con `setProjectMapsSnapshot` similar al que usa el `WorkspaceManager` en F2H7.

**Decisión 4 — Bug arquitectónico de "tags auto-generados" descubierto durante el testing**: durante la validación visual de F2H8 emergió que cuando el dev movía el `Floor` (el piso del mapa) y switcheaba entre maps, el editor freezaba. Diagnóstico con datos del `.moodmap` confirmó que el Floor se duplicaba: `rebuildSceneFromMap` creaba un Floor default cada vez, y `applyEntitiesToScene` aplicaba el Floor del JSON sobre eso. Resultado: 2 planos 48×48 superpuestos → fillrate brutal → freeze.

**Fix arquitectónico**: el helper de `applyOneEntity` que reemplaza entidades con mismo tag al cargar (introducido en el fix de Tile_X_Y modificados) se extiende para cubrir TODOS los "tags auto-generados por rebuildSceneFromMap":
- `Floor` (entidad única).
- `Tile_X_Y` (entidad por celda del grid).

Lista hardcoded en `applyOneEntity`. Otros tags (Multi, CajaFisica, etc.) siguen permitiendo duplicados — necesario para `CreateEntityCommand::execute` (redo de comandos batch).

**Lección aprendida**: cualquier entidad creada automáticamente por el motor (no por el user) y que pueda ser modificada por el editor necesita persistencia + reemplazo en el load. El concepto "tag auto-generado" es la primera abstracción que aparece — si emergen más casos (e.g. `Skybox`, `LightProbe`), agregar al patrón.

**Test arquitectónico**: el caso del Floor está cubierto en `test_save_load_full_roundtrip.cpp` con un test que simula el flow exacto del editor (rebuildSceneFromMap → applyEntitiesToScene) y valida que hay un solo Floor con la position del JSON. Si alguien rompe el patrón en el futuro, el test falla con mensaje claro.

**Trade-offs:**
- Se gana: workflow estándar de IDE (varios mapas por proyecto, switching), bug fix del Floor que era prácticamente bloqueante para usar multi-mapa.
- Se pierde: Save Project As (copiar carpeta entera) sigue stub. Diferido — emergencia baja.

**Revisar si:**
- Aparecen escenas con multi-submesh + materiales mixtos modificados (caso similar a Tile pero para entidades arbitrarias).
- El user empieza a usar muchos mapas (>10) y la UI del submenu necesita scrolling/categorías.
- "Save Project As" emerge como necesidad real (workflow de "duplicar proyecto para experimentar").

## 2026-05-04: CI/CD con GitHub Actions (F2H10)

**Contexto:** post-F2H8 el proyecto tiene 396 tests + 8 hitos de Fase 2 cerrados. Sin CI, cualquier regresión accidental podía durar varios commits sin detectarse. Sub-fase 2.2 (CSG / editor de niveles real) viene grande — 2-3 meses estimados — entrar con red de seguridad activa es responsabilidad básica.

**Decisión 1 — Solo Windows MSVC**: el dev tiene como directiva durable que "multiplataforma Linux está fuera de scope" (memoria del proyecto). El plan original de Fase 2 mencionaba "Linux como test de portabilidad", pero eso nunca fue prioridad real. Si emerge necesidad cross-platform, agregar Linux como matrix entry es trivial; mientras tanto, no agrega valor a un proyecto solo del dev.

**Decisión 2 — Sin Dependabot**: el repo es propiedad personal del dev, sin colaboradores externos. Las deps CPM están pinneadas con `GIT_TAG` exactos (Tracy v0.11.1, meshoptimizer v0.21, etc.). Updates de deps van a hacerse de forma deliberada cuando emerja necesidad concreta — no como notificación automática semanal que solo agregaría ruido.

**Decisión 3 — Cache agresivo de CPM deps**: la primera build sin cache puede tardar 15-25 min descargando + compilando 8 deps grandes. Con `actions/cache@v4` y key derivada del hash de `CMakeLists.txt` + `cmake/CPM.cmake`, builds posteriores caen a 2-4 min. La key se invalida automáticamente si una dep cambia de tag. Cache adicional de build artifacts (`CMakeFiles` + `*.obj` + `*.lib`) acelera más cuando solo cambia código del repo.

**Decisión 4 — Build solo en Debug**: CI corre el mismo build que el desarrollo del dev. Release se difiere si emerge necesidad de validación de optimizaciones automáticas (ya hicimos Release manual en F2H5 testing visual y todo OK). Doble build (Debug + Release) duplica el tiempo de CI sin valor proporcional para v1.

**Decisión 5 — Release auto al taguear sin assets pre-compilados**: el workflow `release.yml` con trigger en `v*.*.*` crea un GitHub Release y usa el message del annotated tag como body (con `git tag -l --format='%(contents)'`). Esto da historial visible público sin trabajo manual. **Sin assets pre-compilados** porque hoy nadie consume el motor como binary — el dev clona y compila. "Binary releases" es hito propio cuando el motor sea consumible standalone (probable post-Fase 2 cuando exista MoodPlayer estable).

**Decisión 6 — `concurrency: cancel-in-progress`**: pushes rápidos seguidos (3 commits seguidos en main) no deben acumular 3 builds en cola — solo el último importa. Esto ahorra minutos del plan free de Actions y da feedback más rápido.

**Trade-offs:**
- Se pierde: validación automática en Linux/Mac, releases con binarios, alertas de updates de deps.
- Se gana: red de seguridad real contra regresiones, badge público de status, releases con historial visible y notes consistentes.

**Lección aprendida del primer build CI**: el cache se llena en la primera build (15-25 min). Si en el futuro alguien forkea el repo o agrega un nuevo branch, la primera build ahí también va a ser lenta. Aceptable como cost una sola vez por entorno.

**Revisar si:**
- Emerge un colaborador externo o un fork con PR — Dependabot puede tener sentido entonces.
- Aparece necesidad de validar Release automatizada (regresiones que solo aparecen con optimizaciones MSVC).
- "Binary releases" se vuelve necesidad real (cuando el motor sea producto consumible).
- Multiplataforma emerge como necesidad real (probable solo si el proyecto se vuelve open-source con tracción).

## 2026-05-04: CSG a mano (no manifold/Carve), brush implícito como representación canónica (F2H11)

**Contexto:** primer hito de sub-fase 2.2 (Editor de niveles serio estilo Hammer / TrenchBroom). Necesitamos elegir la representación de los brushes 3D y el algoritmo para convertirlos en mesh renderable. Tres approaches viables:

1. **Brush implícito + plane clipping a mano**: cada brush son N planos (uno por cara), la geometría sale de intersectar tripletes de planos. Algoritmo ~1500 LOC bien documentadas en "Real-Time Collision Detection" (Ericson) cap 5 + TrenchBroom source.
2. **manifold** (Google, MIT, lo usa Blender 4.x): library state-of-art, exact arithmetic, paralelo. Trabaja sobre mesh triangulada, no sobre brush implícito.
3. **Carve / libcsg**: abandonadas (Carve último commit 2014), pesadas, no las recomienda nadie hoy.

**Decisión:** **CSG a mano con brush implícito** (opción 1). Reusar libs existentes (glm para math, meshoptimizer para weld futuro en F2H16, nlohmann/json para persistencia, EnTT para components) pero el algoritmo CSG core es código propio.

**Razones (críticas para el roadmap):**

- **Lock-to-world UVs (F2H14)**: feature que define editores tipo Hammer. La textura no se deforma al mover el brush porque las UVs se calculan desde el plano global (no desde vértices triangulados). Solo posible si la representación canónica es por-plano. Con manifold (mesh triangulada) tendríamos que reconstruir esto a mano por encima — más trabajo, no menos.
- **Edición no destructiva**: arrastrás una cara, los planos se actualizan, la geometría se regenera deterministamente. Con mesh triangulada, mover una cara requiere reconstruir índices vecinos.
- **Operaciones booleanas limpias (F2H12)**: clipping de polígonos contra planos del otro brush ~500 LOC. Los brushes son convexos por construcción (trivial dado que son intersecciones de half-spaces), entonces los algoritmos se simplifican mucho.
- **Mapas livianos**: 100 brushes = 600 planos en JSON, no 100K vértices.
- **Approach industria estándar**: TrenchBroom (Quake/Half-Life mapping pro), Hammer (Source/Source 2), Godot CSGShape3D — todos a mano. Blender usa manifold pero Blender no es editor de niveles tipo brush, trabaja siempre con mesh triangulada y no necesita planos por cara.

**Alternativas descartadas:**
- **manifold**: por la pérdida de lock-to-world. Mantener manifold como escape hatch en hito futuro si surge un problema duro de robustez numérica que el clipping a mano no resuelva.
- **Carve / libcsg**: abandonadas.
- **CGAL**: industry-grade pero LGPL/GPL parcial, build pesadísimo, overkill para el scope.

**Convención del Plane** (durable, compartida con frustum culling):
`dot(normal, p) + distance = 0` define el plano.
`signedDistance(plane, p) = dot(plane.normal, p) + plane.distance`:
- > 0 → p del lado de la normal.
- < 0 → p del lado opuesto.
- ≈ 0 → p sobre el plano.

Para CSG, la `normal` apunta hacia AFUERA del brush. Un punto pertenece al brush ⇔ `signedDistance ≤ kPlaneEpsilon` en TODOS los planos. `kPlaneEpsilon = 1e-4f` da resolución sub-milimétrica sin caer en ruido de float (validado por tests de planos casi paralelos / coincidentes).

**`Plane.h` promovido** desde `engine/render/pipeline/Frustum.h` (donde lo introdujo F2H3) a `core/math/Plane.h`. CSG (subsystem en `engine/world/csg/`) no debe depender de `engine/render/pipeline/`.

**`BrushComponent` con ownership de `unique_ptr<IMesh>`** (excepción al patrón POD non-owning de Components.h): los brushes son geometría editable runtime, no encajan en el modelo asset-loaded-from-disk del AssetManager. Para no contaminar el header común, vive en su propio `engine/scene/components/BrushComponent.h`. `AssetManager::createDynamicMesh(verts, attrs)` expone la `MeshFactory` interna para crear IMesh runtime no persistidas en cache.

**Look "blank gris" para brushes sin material**: `albedoTint=0.7, roughness=0.85, useAlbedoMap=false`. NO caer al material slot 0 (missing.png) — ese es warning visible para meshes con material faltante, no para brushes que conscientemente no tienen material todavía (F2H14 los va a tener per-cara real).

**Schema `.moodmap` v9 → v10**: nuevo array top-level `brushes[]` paralelo a `entities[]`. `BrushComponent` se excluye de `entities[]` para evitar doble-persistencia. Mapas v9 sin `brushes[]` se cargan con lista vacía (back-compat aditiva). En F2H14 vamos a bumpear a v11 cuando el material pase de global por-brush a per-cara con UVs.

**Trade-offs:**
- Se pierde: cero deps nuevas, pero ganamos ~2000 LOC propias que mantener en lugar de ~100 LOC de bindings a manifold. Cualquier bug numérico en plane clipping es nuestro problema.
- Se gana: control total sobre el approach que matchea exactamente el roadmap (lock-to-world, multi-edit per-cara, compilación al guardar). Cero dependencia que pueda morir o cambiar API.

**Lección aprendida**: la elección de representación canónica define qué features son baratos vs caros más adelante. Manifold es excelente para **modelado** (Blender), pero el editor de niveles tipo Hammer es un workflow distinto — los brushes no son meshes editadas vértice por vértice, son volúmenes definidos por restricciones (planos). Elegir la abstracción correcta de entrada evita pelearle al motor en cada hito siguiente.

**Refs canónicas:**
- "Real-Time Collision Detection" (Christer Ericson, 2005) cap 5 — planes y intersection tests.
- TrenchBroom source: https://github.com/TrenchBroom/TrenchBroom — `lib/vm/` para math, `common/src/Model/Brush*.cpp` para el algoritmo.
- Quake / Half-Life Hammer 4 — referencia histórica del workflow de mapping.

**Revisar si:**
- Emerge un caso de robustez numérica que el clipping a mano no resuelva (brushes con caras casi coplanares en posiciones extremas) → considerar manifold como helper interno.
- F2H12 (booleanos) revela que el clipping puro es insuficiente para operaciones complejas y la suite tipo BSP es necesaria → pivot al approach Quake/Hammer clásico (BSP tree).
- Lock-to-world UV (F2H14) resulta innecesario en la práctica del dev → relajar a UVs per-vertex y reconsiderar manifold.

## 2026-05-04: CSG booleanos destructivos via plane clipping (F2H12)

**Contexto:** segundo hito de sub-fase 2.2 (CSG). Decidir cómo modelar las operaciones booleanas Subtract / Union / Intersect entre brushes. Dos approaches dominantes:

1. **Destructivo** (Hammer / TrenchBroom / Quake): la op reemplaza los brushes input por el resultado. Commit definitivo, undoable via comando.
2. **No-destructivo / Modifier stack** (Blender Boolean modifier): los brushes input siguen vivos y editables; la op se "stackea" como modifier que recalcula la geometría cada frame.

**Decisión:** **destructivo**. Las ops `subtract` / `unionOp` / `intersectOp` devuelven `std::vector<Brush>` (cardinalidad 0-N), el editor reemplaza A y B por el resultado, y `BooleanOpCommand` captura snapshots `SavedBrush` para undo/redo.

**Razones:**

- **Workflow Hammer/TrenchBroom**: el dev viene del modelo "Quake-style mapping" (referencia explícita en F2H11). Ese flow es destructivo: arrastrás un brush B sobre A, "Subtract", aparecen los pedazos. Ctrl+Z restaura. Es el comportamiento mental que el user espera.
- **Simplicidad de implementación**: ~600 LOC + 27 tests vs estimado ~1500 LOC + state runtime para modifier stack (recalcular cada frame, manejar dependency graph entre brushes, gestión de cache, hot-reload del modifier al editar input).
- **Suficiente para v1**: ningún juego tipo Quake / Half-Life / CS usa modifier stacks. El user no pidió no-destructividad explícitamente.
- **Undoability via comando estándar**: el patrón `BooleanOpCommand` (con snapshots por tag, no por handle) reusa la infra del HistoryStack existente sin caso especial.

**Alternativas descartadas:**
- **Modifier stack tipo Blender**: ~2x más código, complejidad de ciclos / dependencias entre brushes (qué pasa si A se subtractea de B y B se subtractea de C — invalidación en cadena), no encaja con el workflow de mapping. Diferido a hito propio si emerge necesidad real (ej. el dev dice "necesito poder editar A después de hacer subtract").
- **Boolean ops via libs externas (manifold, Carve)**: ya descartado en F2H11 (decisión durable de "CSG a mano").

**Algoritmo: plane clipping puro a mano.**
- `subtract(A, B)`: half-space carving plano por plano. Mantener un `remainder` que arranca como A; para cada plano `P_i` de B, generar `frag_i = remainder ∪ {flip(P_i)}` y conservar si tiene volumen, después remplazar `remainder ← remainder ∪ {P_i}`. Los `frag_i` juntos forman `A \ B` como descomposición convexa. Edge case: planos coincidentes con caras de A se skipean.
- `intersectOp(A, B)`: brush con `A.faces ∪ B.faces` (dedup por kPlaneEpsilon), validado por `isBrushValid` (≥4 vertices únicos del brush via tripletes filtrados por inside-test).
- `unionOp(A, B)`: composición vía `(A subtract B) ∪ {B}` con casos especiales para contención total y disjunto.

Reusa todos los tipos puros de F2H11 (`Plane`, `Brush`, `BrushFace`, `intersectThreePlanes`, `signedDistance`, `kPlaneEpsilon=1e-4f`). Cero deps nuevas.

**Snapshot pattern para undoability:** `BooleanOpCommand` captura `SavedBrush` por valor (no `Entity` handles). Razón: tras execute/undo los handles cambian (entidades destruidas y recreadas), pero los snapshots permiten reconstruir desde tag/transform/faces. Mismo patrón que `CreateEntityCommand` de Hito 27. La función `recreateBrushEntity` interna duplica parte del código de `SceneLoader::applyEntitiesToScene` para no acoplar el comando al loader.

**Bug fix durable — centroide del resultado:** en la primera validación los brushes resultantes salían con `transform.position=(0,0,0)` mientras la geometría estaba en world space → el gizmo aparecía en el origen del mundo. Fix: `snapshotResultWorld` setea `position = AABB.center()` (centroide en world) y rebasea los planos a local space via `d_local = d_world + dot(n, centroid)`. **Lección durable**: cualquier op CSG futura que produzca brushes resultantes debe seguir esta convención (transform al centroide + planos en local) para que el gizmo y la edición posterior se sientan naturales.

**UX mínima en F2H12 — menú combobox:** `Archivo > Mapa > Boolean > {Subtract, Union, Intersect} > <brush B>` con submenu cascading listando los demás brushes del mapa como B. **NO multi-selección por Shift+click** todavía: requiere refactor del modelo de selección (Hierarchy + Viewport + Inspector + Gizmos) que es F2H15 del plan original. Promovido a hito siguiente porque sin multi-select el flow es awkward.

**Trade-offs:**
- Se pierde: capacidad de "ajustar A después de hacer subtract" sin reverter (modifier stack lo permitiría).
- Se gana: simplicidad, alineación con workflow Hammer, undoability limpia, suficiente para los próximos hitos de la sub-fase 2.2 (cilindros, UVs, face mode).

**Lección aprendida del scope:** el menú con combobox como B es subóptimo pero **funcional**. El user lo identificó al validar y propuso multi-select como mejora. Resistir el scope creep de "hagamos multi-select dentro de F2H12" — multi-select afecta Hierarchy, Viewport, Inspector, Gizmos, render outline; es claramente hito propio. F2H12 cierra con UX awkward y el siguiente hito hace el flow natural de Blender/Hammer.

**Revisar si:**
- El user pide explícitamente no-destructividad (workflow real lo demanda).
- Edge cases de robustez numérica emergen en uso real con brushes asimétricos / muchas caras.
- F2H13 (primitivas extendidas) revela que el algoritmo no escala a brushes con 32+ caras (cilindros) — profile y considerar BSP-style optimization.

## 2026-05-06: SelectionSet como modelo puro + isBrushValid robustecido (F2H13)

**Contexto:** tercer hito de sub-fase 2.2. **Promovido del F2H15 original** porque al validar F2H12 emergió que el flow del menú Boolean con combobox (heredado de selección singular) era awkward. La selección visual primero, ops booleanas después, es el flujo natural Hammer/Blender.

### Decisión 1 — `SelectionSet` como modelo puro testeable

**Problema:** la mayoría del editor (Inspector, Gizmo, comandos del HistoryStack, Viewport overlays) dependía de `EditorUI::selectedEntity()` returnando un `Entity`. Refactorear todo eso a "operar sobre N entidades" sería un sprint propio y no es lo que F2H13 pide — solo necesitamos que **multi-click funcione** y que las ops booleanas escalen.

**Decisión:** `SelectionSet` con dos campos: `vector<Entity> selected` + `Entity active`. La `active` es la entidad "primaria" — la última clickeada, la que el Inspector muestra, la que el Gizmo afecta. Las demás `selected` son contexto adicional para ops batch (Boolean cascade, futuro multi-delete, etc.).

**Back-compat first:** `selectedEntity()` devuelve `m_selectionSet.active`. `setSelectedEntity(e)` hace `replaceWithSingle(set, e)`. Toda la base de código que asumía selección singular sigue funcionando. Esta API "fachada" desacopla los callsites del modelo interno y permite migrar a multi-edit gradual en hitos futuros.

**Header-only con helpers libres:** `editor/selection/SelectionSet.h` define `add`, `remove`, `toggle`, `replaceWithSingle`, `clear`, `contains` como funciones libres. Sin métodos en el struct → testeable como POD, sin acoplamiento a EnTT más allá del `Entity` opaque handle. Invariantes garantizados por los helpers (no por el struct).

**Invariantes durables** (verificados por tests):
- `selected.empty() ⇔ active == Entity{}`.
- `active != Entity{} ⇒ contains(set, active)`.
- `selected` sin duplicados (mismo handle aparece a lo sumo una vez).

**Política para `remove(set, e)` cuando `e` es la `active`:** el nuevo active es el ÚLTIMO elemento del set tras la remoción (o `Entity{}` si quedó vacío). "Último" = la mental model de "active = más recientemente clickeada de las que quedan".

### Decisión 2 — Click semantics estilo Blender

- **Plain click** → `replaceWithSingle(set, e)` (set queda con solo `e`).
- **Shift+click** → `toggle(set, e)` (si está → quitar; si no → agregar y `setActive`).
- **Ctrl+click** → `add(set, e)` (agrega si no estaba; siempre `setActive`).
- **Click en vacío** (sin modifier, no hit) → `clear(set)`.
- **Click en vacío** (con modifier) → no-op (preserva el set actual).

Aplica en Hierarchy panel (`ImGui::GetIO().KeyShift / KeyCtrl`) y Viewport picking (`SDL_GetKeyboardState`).

**Trade-off vs TrenchBroom:** TrenchBroom usa Ctrl+click para toggle (no Shift) en algunas builds. Elegimos Shift por alineación con Blender (más reciente, más usado por dev moderno). Si emerge fricción, agregar opción configurable.

### Decisión 3 — Boolean ops cascade con preserveB

**Subtract**: cascade real por A. Cada `A_i ≠ active` se reemplaza por sus pedazos `subtract(A_i, active)`. La `active` (B = "tool brush") se preserva. Ejemplo Hammer: agarras 5 cubos para hacer 5 huecos en una pared con la herramienta "tool".

**Union / Intersect**: requiere exactamente N=2 brushes. Razón: la operación consume **ambos** brushes (no preserva el "tool"), y cascadear con N>2 es semánticamente ambiguo (¿izquierda-asociativo? ¿qué hacer si una iteración devuelve N>1?). La cardinalidad del resultado puede ser ≥1, todos brushes nuevos. Cascade real de Union/Intersect = hito futuro si emerge necesidad concreta.

**Si N>2 con Union/Intersect**: warning en log + no-op (no aplica nada, no rompe estado).

### Decisión 4 — Outline diferenciado vía debug renderer (no shaders)

`EditorRenderPass.cpp` itera el set y dibuja 12 líneas (corners de AABB) con el debug renderer existente. Ventajas vs uniform de shader PBR:
- **Sin tocar shaders**: cero superficie de regresión.
- **Tunear visualmente sin rebuild de shaders**: cambiar colores y line width es 1 línea de C++.
- **Funciona uniforme** entre MeshRenderer (corners unitarios) y BrushComponent (corners de `bc.brush.localAabb`).

**Color choices durables:**
- `active` = `(1.0, 0.35, 0.0)` naranja saturado Blender.
- `selected` no-active = `(0.95, 0.95, 0.2)` amarillo claro.
- `glLineWidth` 2px → 3px global. Afecta también triggers OBB, drop highlights, navigation paths — todos ganan visibilidad.

**Trade-off**: el outline actual no respeta depth-test "ver detrás del objeto" tipo Blender silhouette. Si emerge, agregar pase con `glDepthFunc(GL_GREATER)` y color más tenue. Suficiente para v1.

### Decisión 5 — `isBrushValid` exige AABB no-degenerada (bug fix durable)

**Bug detectado en validación visual de F2H13**: `subtract(A, B)` con brushes disjuntos generaba múltiples "copias planas" de A en lugar de una sola copia 3D. Causa: el algoritmo de plane clipping iteraba sobre los planos de B y para cada uno donde A satisfacía el flipped half-space (ej. A está en `y ≤ 0.5` cuando flipped(+Y) dice "y ≤ 0.5"), generaba un fragment "remainder ∪ {flipped}". Si la restricción extra reducía el remainder a un cuadrilátero coplanar (sin volumen), `isBrushValid` lo aceptaba porque solo contaba "≥ 4 vertices únicos" — y un cuadrilátero plano tiene 4 vertices.

**Fix:** `isBrushValid` ahora exige adicionalmente que la AABB del brush tenga `size > kPlaneEpsilon` en los 3 ejes. Sin esto, brushes 2D (cuadriláteros, polígonos coplanares) pasan el check pero rompen `buildBrushMesh` (que asume volumen 3D para fan triangulation).

**Lección durable:** "vertices ≥ N" no es check suficiente para brush 3D. Cualquier futuro algoritmo que produzca brushes (más operaciones booleanas, primitivas con vertices casi coplanares, etc.) debe pasar por `isBrushValid` que ya garantiza volumen real.

### Decisión 6 — `uniqueResultTag` con tags reservados intra-batch

**Bug detectado**: cuando una op booleana generaba N brushes resultantes, todos recibían el mismo tag (`Brush_Union_01`) porque `uniqueResultTag` solo verificaba contra entidades **vivas** del scene. Los snapshots aún no creados como entidades no se contaban.

**Fix:** parámetro `const vector<string>& reserved` que el caller mantiene durante la generación de los snapshots. Cada llamada agrega el tag generado a `reserved` y lo pasa a la siguiente. Sin acoplar a estado global ni tabla de tags vivos.

**Patrón aplicable** a cualquier futuro batch que cree N entidades con tags auto-generados (duplicate, paste-multiple, etc.).

### Decisión 7 — Multi-edit del Inspector diferido

Cuando hay multi-selección, el Inspector muestra solo la `active` + un disclaimer "+N entidad(es) adicional(es) seleccionada(s) — solo se edita la activa". Multi-edit (editar property X en N entidades a la vez) requiere refactor de los comandos `EditPropertyCommand` que hoy capturan un `Entity` específico — no es trivial. Diferido a hito futuro si emerge necesidad concreta. El `DeleteEntityCommand` ya soporta multi-target desde Hito 27.

**Trade-offs:**
- Se pierde: editar transform de 5 brushes a la vez ("alinear todos al grid"); editar material de N props a la vez.
- Se gana: foco en lo que F2H13 prometió (selección visual + Boolean cascade) sin meterse en refactor de comandos. Multi-edit es claramente "feature siguiente" — el dev puede pedirlo si lo necesita.

**Revisar si:**
- El dev pide editar transform / material de N entidades a la vez (probable post-F2H17 cuando aparezcan más entidades en mapas grandes).
- Box-select / lasso-select del viewport emergen como necesidad real (probablemente al manejar 50+ brushes).
- El SelectionSet escala mal a 1000+ entidades (improbable; el outline de 12000 líneas para 1000 brushes es ~negligible vs el cost del overlay 2D).

## 2026-05-06: Primitivas como datos puros + sphere=dodecaedro + cylinder=16 segments (F2H14)

**Contexto:** cuarto hito de sub-fase 2.2 (CSG). Era F2H13 en el plan original (primitivas extendidas), renumerado +1 por el adelanto de multi-selección como F2H13. F2H15-F2H17 también renumeran +1.

### Decisión 1 — Primitivas como funciones libres, no clases

**Problema:** decidir cómo representar 5 tipos de primitivas (cilindro, prisma, esfera, pirámide, wedge) en el subsystem CSG. Approach OOP tradicional sería: clase abstracta `Primitive` con métodos `toBrush()`, `getDefaultParams()`, etc. + 5 subclases.

**Decisión:** **funciones libres `make*Brush(matrix, params...)`** que devuelven un `Csg::Brush` standalone. Sin herencia, sin polimorfismo, sin enum runtime "PrimitiveType".

**Razones:**

- **Coherencia con F2H11**: `makeBoxBrush` ya existía como función libre. Las nuevas primitivas siguen exactamente el mismo patrón.
- **Una vez creado, una primitiva es solo "un brush más"**: render, persistencia, ops booleanas, gizmo, picking — todo opera sobre `Csg::Brush` sin saber qué primitiva era originalmente. **No hay estado runtime de "esto es un cilindro"** después de la creación.
- **Cero schema bump del `.moodmap`**: como las primitivas no tienen identidad post-creación, el formato v10 (que persiste `Brush` genéricos como arrays de planos) sirve sin cambios. Nuevas primitivas en hitos futuros tampoco van a requerir schema bumps.
- **Escala trivialmente**: agregar cono / cápsula / torus poliédrico = 1 función nueva. Sin tocar el dispatch de render, persistencia, picking, etc.
- **Tests más limpios**: cada primitiva = 1 archivo de test puro, sin mocks de jerarquía de clases.

**Trade-off:** se pierde la capacidad de "editar parámetros" después del spawn (cambiar segments=16 a 32 en un cilindro existente). En CSG con planos esto requeriría regenerar el brush completo; el approach actual delega a "edición de planos individuales en F2H16 (face mode)" o "delete + spawn nueva con otros parámetros". Aceptable.

**Patrón aplicable** a futuros generadores: primitives de F2H14, primitivas de mapa-entity de F2H17 (lights, triggers visuales), etc.

### Decisión 2 — Sphere como dodecaedro inscripto (12 caras), no UV-sphere

**Decisión:** `makeSphereBrush` devuelve un dodecaedro regular inscripto en esfera de radio 0.5 — 12 caras pentagonales planas.

**Razones:**

- **Convexidad por construcción**: el dodecaedro es convexo, encaja directo con el approach brush implícito. Una UV-sphere (típica de gráficos) NO es convexa por ser una mesh triangulada con N×M tris.
- **Mismo enfoque que TrenchBroom**: la "sphere" de TrenchBroom también es poliédrica (~ icosaedro). Es lo que el dev histórico de Hammer/Quake espera ver.
- **12 caras es suficiente para uso típico**: detail props, columnas redondeadas, etc. Si se necesita más resolución, hito futuro agrega `makeIcosphereBrush(subdivisions=1)` con 80 caras (1 nivel de subdivisión del icosaedro).
- **Boolean ops escalables**: `subtract(box, sphere)` con 12 caras es submilisegundo. Una sphere de 32+ caras (alta resolución) sería ~10x más lenta — diferido hasta que emerja el caso de uso.

**Geometría**: las normales son las 12 direcciones canónicas del icosaedro dual `(0, ±a, ±b)`, `(±a, ±b, 0)`, `(±b, 0, ±a)` con `a = 1/√(1+φ²)`, `b = φ/√(1+φ²)`, `φ` = razón áurea. Distance fija a `-0.5` para inscribir en unit sphere de radio 0.5.

### Decisión 3 — Cylinder default 16 segments

**Decisión:** `makeCylinderBrush` con default `segments=16`. Permite override pero el editor no lo expone (UI fixed defaults en F2H14, params dinámicos = hito futuro).

**Razones:**

- **Matchea TrenchBroom default**.
- **Visualmente "redondo enough"** para mapping FPS / exploration sin caer en facetado obvio.
- **Performance aceptable**: cylinder de 16 segments = 18 caras (16 lat + 2 caps). Subtract contra otro brush de 18 caras es ~324 ops del algoritmo plane clipping = submilisegundo en Debug build.
- **Consistencia entre cylinder y prisma**: prism triangular = cylinder con 3 segments; prism hexagonal = cylinder con 6. Mismo helper `buildPrismaticBrush` interno → cero duplicación.

**Trade-off:** un cilindro de 16 segments tiene un perímetro discreto, no curva real. En distancias cercanas al jugador esto puede ser visible. Si emerge feedback visual, agregar variante `makeCylinderBrush(matrix, 32)` es trivial — el algoritmo ya lo soporta.

### Decisión 4 — Wedge canónico con base cuadrada

**Decisión:** `makeWedgeBrush` produce una rampa con base cuadrada `[-0.5, +0.5]^2` en X-Z, altura máxima en `z=-0.5` (atrás) y altura cero en `z=+0.5` (adelante). 5 planos: `+X`, `-X`, `-Y` (base), `-Z` (atrás), e inclinado `(0, sqrt(2)/2, sqrt(2)/2)`.

**Razones:**

- **Forma canónica de Hammer**: el "wedge" o "ramp" en Quake es exactamente esta forma — un prisma triangular acostado.
- **Útil sin booleanos**: escaleras (1 wedge), rampas (1 wedge), techos inclinados (1 wedge rotado). Las alternativas serían "subtract de un box con un wedge invisible" — más cara computacional y conceptualmente.
- **Plano inclinado con normal `(0, +y, +z)`**: hacia "arriba-adelante", consistente con la convención "rampa que sube hacia atrás".

**Trade-off:** un wedge "no canónico" (ej. base triangular en lugar de cuadrada) requiere `makePrismBrush(matrix, 3)` rotado. Aceptable — el set de primitivas cubre la geometría común de mapping; casos exóticos van por boolean ops.

### Decisión 5 — Pyramid cuadrada (4+1 caras), no triangular ni hexagonal

**Decisión:** `makePyramidBrush` es **siempre** pirámide cuadrada (base 4 vertices, 4 caras laterales convergentes a la cima + 1 cap base). No hay parámetro `sides`.

**Razones:**

- **Mental model "pirámide" = cuadrada (egipcia)**: el dev no espera "pirámide hexagonal". Si emerge necesidad, `makeConeBrush(matrix, sides=4..N)` lo cubre.
- **Las 4 caras laterales tienen geometría exacta computable a mano**: normales `(±lx, ly, 0)` y `(0, ly, ±lx)` con `lx = 1/√1.25`, `ly = 0.5/√1.25`.
- **Distinción semántica con cylinder**: cilindro/prisma tienen N caras laterales paralelas al eje Y. Pirámide tiene N caras laterales **convergentes** a la cima.

**Trade-off:** `makeConeBrush` (variante con cima en lugar de cap top) sería una primitiva nueva — pendiente como hito futuro si emerge necesidad.

### Decisión 6 — Bug fix durable: gizmo rotate/scale para BrushComponent

**Bug detectado en validación visual:** el gizmo (EditorOverlay) solo permitía Translate sobre brushes. Causa: el filtro `selected.hasComponent<MeshRendererComponent>()` decidía si mostrar rotate/scale; brushes sin MeshRenderer caían a translate-only.

**Fix durable:** el filtro ahora chequea **`MeshRendererComponent || BrushComponent`** (renombrado a `hasGeometry` para claridad). Mismo cambio en `InspectorPanel::showRotScale`. **Lección durable:** cualquier nueva forma de "geometría visible" en hitos futuros (ej. `MapEntityComponent` para lights/triggers visuales en F2H17) debe extender este filtro o emergerá el mismo bug.

**Patrón aplicable:** centralizar el chequeo de "tiene geometría" en un helper `bool hasGeometryFor(const Entity&)` cuando aparezca el 3er tipo de geometría (probable F2H17 con map entities).

### Decisión 7 — Educar al user sobre Hammer vs Blender, no implementar Modifier Stack

**Contexto:** durante validación visual de F2H14 emergió frustración del dev al ver Union de 2 prismas con overlap parcial → ~10 piezas convexas, no "una sola forma fusionada". El dev expresó "no es como blender, o yo estoy mal entendiendo algo".

**Decisión:** **mantener el approach destructivo Hammer** (decisión durable de F2H12). NO implementar Blender Modifier Stack en este hito ni adelantar F2H17 (compilación brush → mesh estática). Educar al dev sobre la diferencia fundamental.

**Razones:**

- **CSG con brushes convexos NO PUEDE representar formas cóncavas como un solo objeto**. La unión de 2 convexos overlapping parcial es matemáticamente cóncava → debe descomponerse en N convexos o ser una mesh triangulada (Blender approach).
- **El dev pidió Hammer-style explícitamente** al arrancar sub-fase 2.2 ("hagamos destructivo como hammer editor"). La frustración emergente es educacional, no un cambio de requirement.
- **F2H17 ya está planeado** y resuelve el issue: al guardar el mapa, todos los brushes se compilan a UNA SOLA mesh triangulada con vertex weld + caras internas culled. **Visualmente vas a ver una sola forma** post-F2H17.
- **Adelantar F2H17 ahora retrasa F2H15 (texturizado UV) y F2H16 (face mode)**, que son features más urgentes para el workflow básico de mapping.

**Trade-off:** el user va a seguir viendo pedazos al hacer Union/Intersect hasta F2H17. Aceptable — la mayoría del workflow es Subtract (hacer huecos), donde la descomposición no se nota tanto visualmente porque los pedazos son geométricamente coherentes.

**Lección durable**: cuando el approach matemático del motor diverge del mental model del user, **explicar la divergencia es la solución correcta** (no rebuild el approach). El path natural es "el plan original ya cubre esto en F2H17" — y eso vale más que un fix ad-hoc que rompe la coherencia destructiva.

**Revisar si:**
- Emergen primitivas con casos numéricos patológicos (vertices casi-coplanares en pirámide rotada extremo, sphere con caras casi-paralelas).
- F2H15 (UV editor) revela que las primitivas no-cuadradas tienen problemas específicos de texturizado (esperable: cilindros con 16 segments tendrían UV "stitching" en cada cara).
- El dev pide variantes (cono, cápsula, torus poliédrico, hemisphere) — todas son `make*Brush` nuevas sin tocar el core.

## 2026-05-06: UVs computed-not-stored + lock-to-world por-cara + UI global-en-brush diferida a face mode (F2H15)

**Contexto:** quinto hito de sub-fase 2.2 (CSG). Materializa el feature que justificó el approach brush implícito de F2H11 (vs manifold mesh-based): **lock-to-world UVs**.

### Decisión 1 — UVs computed-not-stored

**Decisión:** las UVs por vertex no se almacenan en `BrushFace`. Cada cara guarda los **parámetros** de UV (`uAxis`, `vAxis`, `uvOffset`, `uvScale`, `uvRotation`, `lockToWorld`); las UVs se computan en `buildBrushMesh` desde esos params para cada vertex.

**Razones:**
- **Cero vertex data extra**: una cara con 100 vertices no almacena 100 UVs duplicadas — solo los 6 params.
- **Edición instantánea**: cambiar `uvScale` no requiere recalcular vertex data — solo invalida el mesh cache. El SceneRenderer rebuildea en el siguiente frame.
- **Persistencia compacta**: el `.moodmap` v11 guarda 6 floats por cara, no N×2 floats por vertex.
- **Lock-to-world barato**: un solo flag por cara cambia el cómputo de proyección; sin lock-to-world, el código nunca evalúa `worldMatrix * pLocal`.

**Trade-off:** UVs requieren rebuild del mesh cuando cambian. Aceptable porque cualquier cambio de UV viene del Inspector (humano = no-realtime).

### Decisión 2 — `lockToWorld` como flag por-cara, no por brush

**Decisión:** `lockToWorld` es campo de `BrushFace`, no de `BrushComponent`.

**Razones:**
- **Granularidad necesaria**: en F2H17 (face mode) el dev va a poder activar lock-to-world solo en algunas caras (ej. piso con textura world-locked + paredes con textura local-locked).
- **Modelo escalable**: el cache `anyFaceLockToWorld` en `BrushComponent` es un summary computado, no la fuente de verdad.
- **UI global por brush en F2H15** sigue siendo trivial — el toggle aplica a todas las caras a la vez. Per-cara emerge naturalmente cuando hay selección de cara (F2H17).

**Trade-off:** un poco más de memoria por cara (1 byte). Negligible.

### Decisión 3 — Tangent basis canónico al construir, override por edición posterior

**Decisión:** las primitivas (`makeBoxBrush`, `makeCylinderBrush`, `makeSphereBrush`, `makePyramidBrush`, `makeWedgeBrush`, `makePrismBrush`) inicializan los UV params con `defaultTangentBasis(normal)` para `uAxis`/`vAxis`. Resto en defaults sensatos (offset 0, scale 1, rotation 0, lockToWorld false).

**Razones:**
- **UVs alineadas out-of-the-box**: el dev spawnea un cilindro y la textura ya se ve correctamente proyectada en cada cara — no requiere edición.
- **Reusable para subtract/union/intersect**: los brushes resultantes de booleans pueden usar `defaultTangentBasis` para sus caras nuevas.
- **Estable**: `defaultTangentBasis` depende solo de la normal, mismo input → mismo output.

**Algoritmo de `defaultTangentBasis`:**
```
helper = (|normal.y| > 0.9) ? (1, 0, 0) : (0, 1, 0)
uAxis = normalize(cross(helper, normal))
vAxis = normalize(cross(normal, uAxis))
```
Switching axis cuando la normal está cerca del eje Y evita cross product con magnitud cero.

**Trade-off:** cerca de la transición (normal con `|y|` ≈ 0.9) los `uAxis`/`vAxis` flippean, lo que puede causar UVs discontinuas entre caras adyacentes. Mitigación: tests con normales patológicas; si emerge en uso real, agregar smoothing o cachear tangent basis al construir el brush.

### Decisión 4 — UV editor en F2H15 = global por brush; per-cara real = F2H17 (Face Mode)

**Decisión:** el UV editor del Inspector aplica los sliders a TODAS las caras del brush a la vez en F2H15. Per-cara real (con selección visual de cara individual estilo Hammer) se difiere a hito propio.

**Contexto:** durante validación visual el dev preguntó "qué pasa con las UV por caras? no faltaba más?". Se le ofrecieron 3 opciones:
- **A**: cerrar F2H15 sin per-cara, F2H17 = face mode con selección visual.
- **B**: workaround sin face mode — dropdown "Cara 0..N" en Inspector + sliders editando solo esa cara por índice (~30 min trabajo).
- **C**: adelantar face mode a F2H16, postergar HistoryStack cleanup.

**Decisión del dev: A** ("lo quiero igual que el hammer, agregalo como hito propio"). Rechazó workarounds parciales — quiere la cosa real cuando llegue.

**Razones:**
- **Selección visual de cara** requiere infraestructura propia: raycast contra polígonos individuales (no contra AABB del brush), sub-modo "Face Mode" del editor (toggle estilo Blender), render outline distinto solo de la cara seleccionada, comandos undoable per-cara, posible multi-selección de caras.
- **Workaround dropdown sería desechable**: cuando llegue F2H17, el flow de "cara seleccionada por índice" se reemplaza completo. El dev prefiere esperar a tener la UX final.
- **F2H15 entrega los cimientos**: estructura `BrushFace` per-cara + cómputo per-cara + persistencia per-cara. Ya está listo para que F2H17 solo agregue el UI visual.

**Trade-off:** durante F2H15-F2H16, los UV params se editan globalmente por brush. Aceptable como UX intermedia.

### Decisión 5 — Schema bump `.moodmap` v10 → v11 con back-compat aditiva + recompute de tangent basis

**Decisión:** v11 agrega 6 campos opcionales a cada `face` del JSON (`uAxis`, `vAxis`, `uvOffset`, `uvScale`, `uvRotation`, `lockToWorld`). Faces v10 sin estos campos cargan con defaults del struct. **El loader detecta si `uAxis/vAxis` vienen como defaults canónicos (+X/+Y) y los recomputa con `defaultTangentBasis`** desde la normal real.

**Razones:**
- **Back-compat aditiva sin migración**: faces v10 que NO tenían UV params cargan con tangent basis correcto (auto desde la normal), no con `+X/+Y` canónico que se vería mal en caras no-axis-aligned.
- **Idempotente**: si un mapa v11 se guardó con `uAxis=+X, vAxis=+Y` deliberadamente (caso raro pero posible), el loader lo recomputa al cargar — pero un re-save preserva los valores recomputados. Pequeña pérdida de info en ese caso edge; mitigación: el dev usa `defaultTangentBasis` consistentemente.

**Trade-off:** ambigüedad si el dev quiere intencionalmente `uAxis=+X, vAxis=+Y` distinto al tangent basis. Caso raro (y siempre resoluble editando los params de otro modo); aceptable.

### Decisión 6 — Bug fix durable: drop de textura/material detecta BrushComponent primero

**Bug detectado:** drop de textura/material sobre un brush en el viewport creaba un **tile-pared en el grid del suelo** en lugar de asignar al brush. Causa: `processViewportTextureDrop` y `processViewportMaterialDrop` no consideraban `BrushComponent` — solo MeshRenderer (material) o tile pick (texture).

**Fix durable:** ambos handlers chequean `BrushComponent` primero. Para texture drop además crea un material wrapper via `assets.createMaterialFromTexture(texId)` y lo asigna a `bc.material`. Solo cae al flow legacy si el cursor no está sobre un brush.

**Lección durable:** cualquier futuro tipo de "geometría visible" (F2H17 face entities, F2H18 compiled meshes) debe extender estos handlers o el bug emerge de nuevo. Mismo patrón que el `hasGeometry` de F2H14 para gizmo rotate/scale.

### Decisión 7 — Bug ABI mismatch en C++: clean rebuild requerido al cambiar size de struct persistido

**Bug detectado:** al extender `SavedBrushFace` con los 6 campos UV (de ~16B a ~72B), una unidad de compilación que crea/copia el struct usaba el layout viejo. `push_back` en el `vector<SavedBrushFace>` agregaba más de 1 elemento por iteración (terminando en 20 faces para una box de 6). Build incremental no recompiló todo el código que tocaba el header.

**Resolución:** `cmake --build ... --clean-first` forzó recompilación completa.

**Lección durable:** cualquier cambio de tamaño en structs persistidos (`SavedBrushFace`, `SavedEntity`, etc.) o públicos en headers ampliamente incluidos requiere clean rebuild para evitar corrupción de memoria. Documentar como step de validación: "tras agregar/cambiar campos a structs en headers públicos, clean rebuild de la suite + lanzar editor y verificar persistencia básica antes de validar la nueva feature".

**Trade-off:** clean rebuild toma 5-10 min vs incremental ~30s. Aceptable como step ocasional cuando se cambian structs.

**Revisar si:**
- El dev empieza a usar lock-to-world masivamente y el rebuild-on-transform-change pega en performance (probable solo con 50+ brushes lock-to-world editados al mismo tiempo).
- F2H17 (face mode) revela que la API "todo per-brush" del UV editor de F2H15 confunde al dev — refactorear UI a "cara seleccionada activa".
- Schema v11 tiene back-compat issues reportados por el dev (proyectos viejos cargan con UVs raras).

## 2026-05-06: HistoryStack Blender-style — wireup, no refactor (F2H16)

**Contexto:** sexto hito de sub-fase 2.2. Hito intermedio de "limpieza de deudas" entre F2H15 (UV editor) y F2H17 (Face Mode estilo Hammer). Insertado en el roadmap por feedback del dev: "importé cilindro, lo escalé, lo elevé, le puse 1ra textura, 2da textura, arrastré textura al suelo creando cubo, Ctrl+Z me devolvió al cilindro antes de elevarlo".

### Decisión 1 — Mantener command pattern, NO refactor a snapshot pattern

**Contexto:** Blender internamente usa snapshot pattern (memcpy del estado relevante tras cada "operator"). MoodEngine desde Hito 27 usa command pattern (`ICommand` con `execute()/undo()`). El dev preguntó "como lo hace blender? me gusta el sistema, podemos copiarlo" — pregunta legítima.

**Decisión:** mantener command pattern. NO refactor a snapshot.

**Razones:**

- **Behaviour observable es idéntico**: lo que el user **ve** en Blender (drag de slider = 1 step, "Last Operator" en UI, Ctrl+Z granular por intención) es 100% reproducible con command pattern. La diferencia es interna.
- **Refactor a snapshot pattern es sprint propio sin valor agregado real**: requeriría serializar todo el state del scene/brushes a memoria/disco tras cada acción, escalable mal con el motor actual.
- **Command pattern de MoodEngine ya es testeable y robusto**: el Hito 27 + 32 ya estableció el patrón con `pushEditIfDone`, snapshots por tag (no handles), invariantes tested en `test_history_stack.cpp`.
- **F6 "tweak last operator" de Blender** (parametrizar el último operator post-hoc) sería scope grande propio. Diferido si emerge.

**Trade-off:** los devs C++ que vienen de Blender pueden esperar snapshot pattern al leer el código. Mitigación: documentar en `Command.h` que el patrón es "Blender-style en behaviour, command-pattern en implementación".

### Decisión 2 — Auditoría exhaustiva como Bloque B

**Contexto:** la deuda no era 1 bug — eran ~8 deudas acumuladas desde Hito 5 hasta F2H15. Implementar sin auditoría llevaría a "fix this fix that" cíclico cuando emerjan más casos.

**Decisión:** delegar Bloque B completo a un subagente con prompt específico ("audit MoodEngine editor for mutations of Scene/GridMap/BrushComponent/MeshRenderer that DON'T push commands"). Output: lista concreta con file:line + categoría + comando propuesto.

**Razones:**

- **Cobertura sistémica**: el subagente buscó por `setTile`, `bc.material =`, `mr.materials`, etc. Pillar todo de una vez evita el bug de "fix uno, descubrir tres más".
- **Documentado en plan**: la lista vive en `PLAN_HITO_F2H16.md` y queda como referencia. Si emerge un futuro caso similar, se compara contra esa lista.
- **Patrón aplicable**: cualquier futuro hito de "deudas técnicas" puede usar el mismo approach (auditoría con subagente + lista concreta + plan + ejecutar).

### Decisión 3 — Captura por tag, no por handle EnTT

**Decisión:** `EditBrushMaterialCommand`, `EditBrushUVCommand`, `EditMeshRendererMaterialCommand` capturan `std::string entityTag` y buscan la entidad por tag en `execute()/undo()`. NO capturan `Entity` handle.

**Razones:**

- **Robustez ante delete/recreate del HistoryStack**: si un comando previo fue `DeleteEntityCommand → undo → recreate`, el handle EnTT cambió. Capturar por tag sobrevive a esto. Mismo patrón que `BooleanOpCommand` de F2H12.
- **Tags son estables**: garantizado por convenciones del editor (Brush_Box_NN, etc.) + UI que evita duplicados de tag al spawnear.
- **Costo: O(N) lookup** por aplicación del comando. Negligible para ~100 entidades típicas.

**Trade-off:** si dos entidades tienen el mismo tag (por bug anterior a F2H8), el comando opera sobre la primera encontrada. Aceptable porque el bug ya fue arreglado y la convención de "tag único" es invariante del editor.

### Decisión 4 — Granularidad por intención del user (drag = 1 command)

**Decisión:** los sliders del UV editor pushean **1 comando al soltar** (`IsItemDeactivatedAfterEdit`), no por cada frame del drag. El checkbox lockToWorld es push instantáneo.

**Razones:**

- **Mental model del user**: el user no piensa "moví el slider 100 px"; piensa "cambié el scale de 1 a 3". Granularidad por intención.
- **Stack legible**: 1 acción = 1 entrada en el undo stack. Sin spam de "Editar UV scale" × 100.
- **Patrón ya existing**: `pushEditIfDone` del Hito 32. F2H16 extiende a UV editor con helper local `captureSnapshotIfActivated()` + `pushCommandIfChanged(label)`.
- **`snapshotsEqual` evita ruido**: si el user clickea un slider pero no lo mueve, snapshot pre = snapshot post → no se pushea comando. Sin entradas vacías en el stack.

### Decisión 5 — StatusBar Blender-style "Último: <name>" sin timeout

**Decisión:** la statusbar muestra "Último: <command name>" persistente, refrescado cada frame leyendo `historyStack->undoName()`. Sin timeout (no se borra después de N segundos).

**Razones:**

- **Info útil constante**: el user siempre quiere saber qué Ctrl+Z va a deshacer, no solo en los 5 segundos post-acción.
- **Implementación trivial**: un getter ya existente (`undoName()` del Hito 27) + un setter en `StatusBar` + sincronización en `EditorUI::draw`. ~10 LOC.
- **No bloquea otros mensajes**: la statusbar tiene FPS, modo, message libre — el "Último: ..." va al final con su propio Separator. Cero conflicto.

**Trade-off:** si la barra se llena visualmente con un command de nombre muy largo, puede empujar otros elementos. Mitigación: nombres concisos en convención (ej. "Editar UV scale" en lugar de "Modificar el parametro UV scale del BrushComponent del brush activo").

### Decisión 6 — `snapshotsEqual` con tolerancia kPlaneEpsilon

**Decisión:** la función `snapshotsEqual(a, b)` compara los UV params componente a componente con tolerancia `kPlaneEpsilon = 1e-4f`.

**Razones:**

- **Float exact equality es frágil**: un drag de slider que termina en el mismo valor numérico puede tener un epsilon de diferencia por float math.
- **Reusa la tolerancia ya estandarizada**: `kPlaneEpsilon` de F2H11 es la tolerancia geométrica del motor. Mismo orden de magnitud para UVs.
- **No spam**: con tolerancia bit-exact, cualquier wiggle del slider creaba commands ruidosos.

### Decisión 7 — Diferir tweak-last-operator (F6 de Blender)

**Decisión:** NO implementar "F6 panel" de Blender (ajustar params del último operator post-hoc).

**Razones:**

- **Scope grande propio**: requiere parametrizar cada comando con sus params editables, panel UI dedicado, integración con el statusbar.
- **No urgente**: el user pidió "como hace Blender" pero no pidió F6 explícitamente. El behaviour de drag = 1 command + Ctrl+Z granular ya cubre 95% del flow Blender.
- **Diferido a hito propio si emerge**: en el plan post-Fase 2.2, posiblemente como UX polish.

**Revisar si:**
- El user pide F6 explícitamente al usar el editor en flow real.
- El statusbar "Último: ..." se llena demasiado visualmente con comandos largos — limitar a N caracteres con ellipsis.
- Emergen NUEVOS handlers que mutan state sin command (probable en F2H17 face mode con material per-cara) — auditar en Bloque B de cada hito posterior.

## 2026-05-06: Face Mode estilo Hammer + multi-material via slots (F2H17)

**Contexto:** F2H15 cerró UV editor con sliders aplicados a TODAS las caras del brush. El dev pidió desde el día uno que la edición fuera per-cara real — "lo quiero igual que el hammer" — y rechazó workarounds parciales (dropdown intermedio, multi-edit aproximado). F2H17 materializa eso como sub-modo del editor con selección visual de cara individual + material distinto por cara.

### Decisión 1 — Sub-modo Face con tecla 3 (Blender convention)

**Decisión:** sub-modo del editor toggle con tecla **3**. Reservar `1` (vertex) y `2` (edge) sin implementar todavía. `Esc` o `3` otra vez vuelve a Object Mode.

**Razones:**
- **El dev ya conoce la convención** (usa Blender). Usar 3 para face es la elección obvia y reduce fricción cognitiva.
- **No reinventar**: imitar Blender en lo que ya funciona. Hammer usa toolbox flotante con botones — feo y obsoleto.
- **Reservar 1/2 ahora** = no romper el muscle memory si vertex/edge mode emergen después (mapping FPS rara vez los necesita, pero está la opción abierta).

**Alternativas descartadas:**
- Tecla `Tab` (Blender real): conflictua con el cycle de focus de ImGui en el editor.
- Botón en la toolbar / menu: el dev pidió teclado-first explícitamente en F2H13.
- Modal popup "elegir modo": rompe el flow rapid-fire del mapping.

**Revisar si:**
- Vertex / Edge mode emergen como necesidad real (improbable para FPS mapping).

### Decisión 2 — Multi-material via slots (no MaterialAssetId per-cara directo)

**Decisión:** `BrushComponent.materials: vector<MaterialAssetId>` (slots indexados por `face.materialIndex`). `BrushFace.materialIndex` ya existía desde F2H11 pero no se usaba — F2H17 lo activa apuntando al vector de slots.

**Razones:**
- **Mismo patrón que `MeshRendererComponent`**: la base de código ya conoce este modelo. `MeshRenderer.materials[]` con `submesh.materialIndex` indexando el array.
- **Dedup natural**: dos caras pueden compartir el mismo MaterialAssetId sin duplicación. Slot 0 = "default del brush"; slots 1+ se agregan al asignar material distinto a una cara específica.
- **Multi-material rendering eficiente**: `buildBrushMesh` agrupa caras por slot y produce 1 submesh por slot. SceneRenderer hace 1 draw call por slot — escala bien si el dev usa pocos materiales distintos por brush (caso común).
- **Schema bump v11→v12 aditivo**: `materialPaths` array nuevo en JSON; v11 con `material` singular se sintetiza como `materials = [material]`. Mapas viejos cargan visualmente idénticos.

**Alternativas descartadas:**
- `MaterialAssetId per-face` directo (1 ID por cara): menos dedup, peor render perf (1 draw call per face en peor caso), refactor más invasivo del schema.
- Mantener material global del brush + override per-cara opcional: dos paths de código diferentes en render, peor de ambos mundos.

**Revisar si:**
- N>16 slots por brush (improbable: típicamente brushes tienen ≤4 materiales distintos). Si pasa, considerar agrupar materiales similares por shader.

### Decisión 3 — `BrushComponent` move-only (`unique_ptr<IMesh>` per slot)

**Decisión:** `BrushComponent.meshCache` cambia de `unique_ptr<IMesh>` (singular) a `vector<unique_ptr<IMesh>>`. `BrushComponent` se vuelve move-only (copy ctor `=delete`).

**Razones:**
- **1 mesh GPU por slot** — alineado con la decisión 2 (multi-material). El SceneRenderer itera el vector y bindea el material correspondiente antes de cada draw.
- **Move-only forzado por `unique_ptr`**: no se puede hacer copy del componente. Migración a `addComponent<BrushComponent>(std::move(bc))` en ~12 callsites — molesto pero refleja el modelo real (un BrushComponent es ownership de meshes runtime, no debería duplicarse).
- **Catch errors at compile time**: el `=delete` explícito hace que cualquier copia silenciosa (por ej. en lambda capture) falle la compilación con error claro.

**Alternativas descartadas:**
- `vector<shared_ptr<IMesh>>`: copy-OK pero overhead refcount + ownership semánticamente ambigua. El BrushComponent ES el dueño, nadie más.
- Mesh interleaved única con offset/count per slot (1 sola GPU buffer con sub-rangos): complica el rebuild parcial cuando cambia 1 cara, sin ganancia de perf real.

**Revisar si:**
- El refactor de move-only causa fricción en algún flow nuevo. Probablemente no — el patrón está bien establecido en EnTT.

### Decisión 4 — Picking de cara con back-face culling (regla `dot > 0`)

**Decisión:** `Csg::pickFace` filtra triángulos con `dot(worldNormal, rayDir) > 0` antes de Möller-Trumbore. Solo las caras de espalda al ray (no apuntan hacia la cámara) son skip — equivalente a back-face culling de render.

**Razones:**
- **Bug real en validación**: sin esto, click en una cara seleccionaba **la opuesta**. Razón: ambas caras paralelas (face front + back) tienen sus polígonos triangulados en el plano del brush; el ray entra por la cara visible y sale por la opuesta — Möller-Trumbore intersecta ambos triángulos y el "más cercano" puede ser la opuesta dependiendo del orden de iteración.
- **Match con render**: las caras visibles son las que apuntan hacia la cámara (back-face culling activo en el shader). Picking en world space con la misma regla mantiene WYSIWYG.
- **Cero falsos negativos**: si una cara está visible para la cámara, su `worldNormal` apunta hacia la cámara → `dot(worldNormal, -rayDir) > 0` → `dot(worldNormal, rayDir) < 0` → NO se filtra. Coincide con la convención de OpenGL.

**Alternativas descartadas:**
- "Más cercano" sin filtrar: bug confirmado por validación.
- `glReadPixels` sobre un buffer de IDs (color picking): añade pase de render dedicado, complica el pipeline para 1 feature.

**Revisar si:**
- El dev rota la cámara dentro del brush (dentro de un cuarto sólido cerrado): las normales apuntarían hacia adentro y el filtro invertiría su efecto. Para mapping eso no pasa (siempre cámara fuera de los brushes), pero documentar.

### Decisión 5 — Highlight visual: outline naranja + fill semi-transparente Half-Life

**Decisión:** outline naranja `(1.0, 0.5, 0.0)` siempre + fill `(1.0, 0.55, 0.10, 0.55)` con alpha blending + `glDepthFunc=GL_LEQUAL` + `glDepthMask=GL_FALSE`. Fill se oculta cuando `Inspector::isEditingBrushUV()=true`.

**Razones:**
- **Cyan tradicional "se vio muy pobre"** en validación con el dev (literal). Naranja Half-Life es saturado, distintivo, asociado a "selección activa" en convención FPS-mapping (Hammer original).
- **Fill semi-transparente, no sólido**: el dev lo necesita ver la textura debajo para poder editarla. Alpha 0.55 deja la textura visible pero comunica "este es el pivote".
- **`LEQUAL` + `depthMask=FALSE`**: el highlight se ve sobre la geometría sin Z-fighting, pero no escribe al depth buffer (no oculta lo que esté detrás).
- **Fill oculto en UV edit**: pedido directo del dev — "alternar entre mostrar toda la cara o no" estorbaba al editar UVs. Outline siempre porque es la confirmación visual de qué cara está activa.

**Alternativas descartadas:**
- Solo outline (sin fill): demasiado sutil cuando el brush es chico o está lejos.
- Fill sólido sin alpha: tapaba la textura, imposible editar UVs.
- Color cycling animado: efectista, distrae.

**Revisar si:**
- El dev cambia de tema visual del editor (claro/oscuro) y el naranja deja de contrastar — tema-aware highlight.

### Decisión 6 — Schema v12 back-compat aditivo + dual-write `material` legacy

**Decisión:** v12 escribe `materials` (array de paths) Y `material` (path del slot 0) en cada brush. Lectores v12 prefieren `materials` array; lectores v11 leen `material` singular y pierden la info de slots adicionales pero ven el brush con el slot 0 visualmente correcto.

**Razones:**
- **Forward compat**: si un mapa v12 cae en una build v11 antigua, no crashea ni se ve raro — la cara con slot 1+ pierde su material y cae al default, pero el brush completo sigue siendo válido.
- **Gradual migration**: el dev no necesita rewriteear sus mapas viejos. v11 abre, v12 escribe, conversión transparente.
- **Costo de bytes mínimo**: el campo legacy duplicado es 1 string corto adicional por brush. Cero impacto en performance de save/load.

**Alternativas descartadas:**
- Solo escribir `materials` (drop legacy): rompe la posibilidad de downgrade durante el desarrollo activo.
- Versión rama (v11.5 con feature flag): complica el reader sin beneficio real.

**Revisar si:**
- El dev confirma que el flujo dev-vs-prod no requiere downgrade (estamos en proyecto solo). En ese caso la próxima versión puede dropear `material` legacy.

### Decisión 7 — Cubo `brick.png` removido del mapa nuevo

**Decisión:** `EditorScene::createDefaultMap` ya no spawnea un cubo con textura `brick.png` central. `arena_16x16` arranca completamente vacío.

**Razones:**
- **Pedido directo del dev**: "el cubo que siempre aparece con textura brick.png del mapa podes eliminarlo, me molesta". Distorsiona el flow de validación visual de cualquier feature CSG (siempre hay que mover/eliminar el cubo primero).
- **Simplicidad**: un mapa vacío es el blank slate correcto para mapping. El dev spawnea lo que quiere desde menu Brush.
- **Cero pérdida funcional**: el cubo era debug-leftover del Hito 4 (cuando solo había tiles). Ya no aplica.

**Revisar si:**
- Algún test o demo asume que el mapa default tiene contenido — verificar (tests pasaron 567/567 sin tocar nada, OK).

## 2026-05-06: Reorg de menús del editor — top-level Mapa + Brush (F2H18)

**Contexto:** el menú `Archivo > Mapa` mezclaba file ops del proyecto (Nuevo, Abrir mapa, Guardar como) con geometría (Añadir Brush ▶, Boolean ▶). Spawn de un brush exigía 4 clicks con jerarquía no obvia. Ítem 1 del backlog `PENDIENTES.md` post-F2H17. F2H18 lo cubre como hito propio antes que F2H19+ acumulen más items en `Archivo > Mapa`.

### Decisión 1 — Promover `Mapa` y `Brush` a top-level

**Decisión:** dos menús nuevos top-level. `Mapa` toma los 5 file ops del mapa actual; `Brush` toma `Añadir ▶` (7 primitivas) + `Boolean ▶` (subtract/union/intersect via `drawBooleanOpMenu`). `Archivo` queda solo con file ops del proyecto + Empaquetar + Nuevo Script + Guardar prefab + Salir.

**Razones:**
- **Flatten**: spawn de brush pasa de 4 clicks a 3. Geometría es la operación dominante en mapping; merece top-level.
- **Separación de dominios**: file ops del proyecto y file ops del mapa son cosas distintas. Mezclarlas en `Archivo` confunde porque "Guardar" guarda el proyecto, no el mapa, pero `Mapa > Guardar como` guarda el mapa.
- **Cero refactor funcional**: los `requestProjectAction(...)` calls quedan idénticos. Solo se reubica el `MenuItem` que dispara cada acción.
- **Habilitación condicional preservada**: ambos top-levels deshabilitados sin proyecto activo (mismo behavior que el submenu anterior).

**Alternativas descartadas:**
- Mantener todo bajo `Archivo > Mapa`: el ítem 1 de PENDIENTES era específicamente esa queja.
- Toolbar lateral con iconos (Hammer-style): scope propio mucho más grande; el dev pidió "mejora a futuro" pero F2H18 era el quick-win de reorg.
- Renombre `Brush → Geometría`: diferido. Hoy todo el mapping es CSG-brush; el día que entren shapes no-brush (mesh import procedural, etc.) se renombra.

**Revisar si:**
- Entran shapes no-brush al editor → renombre a `Geometría`.
- El dev pide atajos rápidos por keyboard (Ctrl+B = Box, etc.) — hito propio.

### Decisión 2 — Demos a submenu `Ayuda > Demos ▶`

**Decisión:** los 13 items "Agregar X demo" + `Stress test poligonos ▶` que vivían top-level en `Ayuda` se agruparon bajo submenu `Ayuda > Demos ▶`. `Ayuda` queda con solo "Acerca de" + el submenu Demos.

**Razones:**
- **Los demos no son ayuda al usuario**: nombre no engaña pero pollutea visualmente. Algo como "Agregar particulas de fuego demo" no es help-content; es validación rápida de features.
- **Tampoco merecen top-level**: son secundarios al flow de mapping serio. Quien usa el editor en producción rara vez los toca.
- **Submenu de Ayuda**: agrupación obvia para "cosas raras del editor que no son del flow principal". Si emergen más, caben acá sin ensanchar la barra.
- **Stress test dentro del mismo Demos**: es un demo más (de poligonos masivos para benchmark) — no necesita su propio nivel.

**Alternativas descartadas:**
- Top-level `Demos`: demasiada visibilidad para algo de uso ocasional.
- Quitar los demos del editor: el dev los usa para validar regresiones rápidas. Mantenerlos accesibles, solo escondidos.
- Panel separado: ortogonal y diferido — los demos hoy son menu items rápidos, no necesitan UI dedicada.

**Revisar si:**
- Los demos crecen a >25 items → considerar agruparlos por categoría dentro de `Demos ▶` (Audio / Gameplay / Render / Stress).

### Decisión 3 — Sin tests nuevos

**Decisión:** F2H18 no agrega tests. Validación es 100% visual con el editor.

**Razones:**
- **El menú es UI puro de ImGui**: cero lógica testeable. Los `requestProjectAction(...)` ya están cubiertos por los tests del dispatcher en `EditorApplication`.
- **Mock de ImGui sería overkill**: testear que `BeginMenu("Mapa")` se llama antes que `EndMenu()` no agrega valor real.
- **Suite confirma cero regresión**: 567/8182 verde con el código nuevo — los tests existentes ya son la red de seguridad de que ningún `requestProjectAction` cambió de signature.

**Revisar si:**
- Aparecen tests de UI snapshot (raros en C++ con ImGui). Improbable.

## 2026-05-07: HistoryStack residual — auditoría con subagente + 2 comandos nuevos (F2H19)

**Contexto:** F2H17 introdujo multi-material rendering con `BrushComponent.materials` vector y drops que distinguen Object Mode (slot 0) vs Face Mode (slot existente o nuevo). El path Face Mode quedó **sin push al HistoryStack**: solo el path Object Mode estaba cubierto por `EditBrushMaterialCommand` (hard-coded a slot 0). Más una deuda pre-F2H17: drop de `.lua` sobre entidad mutaba `ScriptComponent.path` o agregaba el componente sin command. Item 2 del backlog `PENDIENTES.md` post-F2H18.

### Decisión 1 — Auditoría con subagente antes del scope

**Decisión:** Bloque B del hito = subagente (`general-purpose`) recorre `src/editor/` buscando mutaciones sin push. Scope cerrado tras la auditoría, no antes.

**Razones:**
- **Predecir desde el plan inicial subestima**: F2H16 anticipó "2-3 deudas", encontró 8. F2H19 anticipó "regresiones de F2H17", confirmó 2 ALTA + descubrió 1 MEDIA pre-F2H17 que no estaba en el radar.
- **El subagente lee código real, no asume**: verifica que `EditBrushMaterialCommand` está hard-coded a slot 0 (comentario explícito en el .cpp), que el UV editor del Inspector sí pushea correctamente, etc. Sin el subagente uno predice más por miedo o duplica trabajo.
- **Reporte estructurado por archivo:línea + prio**: facilita decidir scope (ALTA / MEDIA → entran; BAJA → confirmar con dev).
- **Misma técnica usada en F2H16 con éxito** — patrón validado.

**Alternativas descartadas:**
- Yo recorrer manualmente: aprox 30+ archivos del editor; el contexto principal se llena de file reads sin valor de razonamiento.
- Predecir desde memoria: los detalles concretos (archivo:línea) se pierden y los pendientes derivan a hipótesis.

**Revisar si:**
- El subagente reporta deudas falsas (sobre-flagging). En este hito 0 falsos positivos — los 5 hallazgos eran todos válidos, solo discrepamos en prio.

### Decisión 2 — `EditBrushFaceMaterialCommand` captura el vector `materials` completo

**Decisión:** el comando snapshotea `oldMaterials` y `newMaterials` (`std::vector<MaterialAssetId>` completos), no solo el slot afectado por el drop.

**Razones:**
- **El drop puede crear slot nuevo via `push_back`**: si `newMat` no estaba en `bc.materials`, el handler hace `materials.push_back(newMat)` y `face.materialIndex = nuevo_index`. Si el undo solo revirtiera `face.materialIndex` sin tocar el vector, quedaría un slot huérfano (material no usado por ninguna cara).
- **Tras varios undo/redo el vector divergiría**: cada execute agrega slot, cada undo no lo quita → `materials.size()` crece sin volverse atrás. Snapshot completo garantiza shape exacto en cualquier dirección.
- **Costo despreciable**: vector de N MaterialAssetId (u32) — N raramente > 4. Copia de 16 bytes en peor caso típico.
- **Robusto a casos edge**: dos drops del mismo material en caras distintas; un drop, undo, drop diferente, undo de nuevo, etc. Snapshot completo cubre todo.

**Alternativas descartadas:**
- Solo snapshot de `(faceIndex, oldFaceMatIndex, newFaceMatIndex)` + flag "did_push_back": agrega complejidad, multiple paths en undo según el flag. La captura completa del vector es estructuralmente más simple.
- Restar/agregar por delta sobre `materials`: requiere reaplicar reglas del handler en undo — duplica lógica.

**Revisar si:**
- N (slots por brush) supera ~32 con frecuencia → snapshot grande, considerar diff. Improbable: typical brushes tienen ≤4 materiales distintos.

### Decisión 3 — `EditScriptComponentCommand` con flag `hadComponent` y edge case de re-creación

**Decisión:** snapshot del comando incluye `bool hadComponent` que distingue 2 sub-casos en `undo()`:
- `!hadComponent` → undo remueve el componente (drop lo agregó).
- `hadComponent` → undo restaura `path` previo. Si alguien removió el componente entre execute y undo, lo recrea con `oldPath`.

**Razones:**
- **Drop de script tiene 2 semánticas distintas**: agregar componente nuevo vs reemplazar path. El undo correspondiente difiere fundamentalmente — sin el flag, el comando tendría que inferirlo y podría equivocarse si el state cambió externamente.
- **Edge case real**: el dev podría remover el ScriptComponent desde el Inspector (botón Remove) entre el drop y el Ctrl+Z. Si undo asume "componente está ahí" crashearía. Recrear con oldPath es la decisión menos sorprendente.
- **No snapshot de exposed props (`overrides`/`exposedProps`)**: el flow normal es "drop reemplaza el script entero"; las overrides del script anterior dejan de aplicar (son por nombre+default del script). Si el undo restaura el path viejo, las overrides se redescubren al re-cargar via mtime check.

**Alternativas descartadas:**
- Comando que infiera el flag desde state: frágil, distintos resultados según orden de operaciones.
- Reusar `EditPropertyCommand<std::string>` con setter custom: no cubre el caso "agregar componente si no existía" — distinto path en setter de get-or-add que en setter de assign.

**Revisar si:**
- El dev pide undo de Inspector edits sobre `overrides` (Hito 24 deuda) → comando dedicado `EditScriptOverrideCommand` con snapshot del map de overrides; ortogonal a este.

### Decisión 4 — Items BAJA fuera de scope (alineado con Blender/Unity)

**Decisión:** acciones del menú `Mapa` (Nuevo / Abrir / Guardar como / Set default / Eliminar) y toggles de modo/selección Face NO pushean al stack. Quedan fuera de F2H19.

**Razones:**
- **Convención de la industria**: Blender, Unity, Godot no hacen undo de "abrir mapa" ni de "cambiar de modo de edición" ni de "cambiar la selección". Son operaciones a nivel proyecto/UI, no edición del modelo.
- **Semantica del HistoryStack**: representa edits del scene (state persistido). Cargar otro mapa reemplaza el scene entero — el stack debería **resetearse** al cambiar de mapa, no acumular un "undo open map".
- **Seleccion**: Ctrl+Z después de hacer click en una cara debería deshacer la EDICIÓN previa, no la selección. Imitar Blender aquí.

**Alternativas descartadas:**
- Stack separado para selección/modo: complica la UX (qué Ctrl+Z deshace qué) sin valor real.

**Revisar si:**
- El dev pide explícitamente undo de "abrir mapa" o de selección. Improbable — el feedback ya validó que pasaron los 3 fixes core.





## 2026-05-07: F2H20 — compilación brush → mesh + export OBJ on-demand, sin persistencia ni runtime-load

**Contexto:** El plan original `PLAN_FASE2.md` entrada F2H14 ("Compilación brush → mesh optimizada") sugería: "al guardar el mapa, todos los brushes se compilan a una mesh estática unificada con caras internas eliminadas, vertices soldados, e índices generados. Esta mesh es lo que se renderiza en runtime. Brushes individuales solo existen en el editor." Tras el rerouting (Face Mode primero en F2H17, reorg menús F2H18, cleanup HistoryStack F2H19), F2H20 toma el ítem. Hay tres niveles de scope posibles:

1. **MVP**: helper puro `compileMap` + export OBJ + UI menu, sin tocar el formato `.moodmap` ni el runtime del MoodPlayer.
2. **Persistencia**: agregar `compiledMesh` al `.moodmap` JSON con schema bump (back-compat aditiva). Editor sigue usando brushes; runtime puede usar la mesh compilada si el campo está disponible.
3. **Two-paths**: editor carga brushes (edición), MoodPlayer carga **solo** la mesh compilada (sin código CSG en el runtime). Cumple el plan original al 100%.

**Decisión:** **Nivel 1 (MVP)** para F2H20. La compilación es una **vista derivada** del scene actual, accesible desde 2 menu items en `Mapa`:
- "Compilar mapa (stats)" → `pfd::message` con stats (brushes / faces totales / culled / triángulos / vertices pre-weld / unique / submeshes).
- "Exportar OBJ..." → `pfd::save_file` + `compileMap` + `writeObj` → `.obj` + `.mtl` lateral.

**Por qué MVP:**
- **Cumple el caso de uso real principal**: el dev quiere ver el resultado de cull/weld + exportar a Blender / MeshLab para iteración. Eso lo cubre el MVP completo.
- **Persistencia infla el `.moodmap`** sin beneficio inmediato (vertex data es ~50 bytes/vertex en JSON; un mapa típico de 200 brushes con 5K vertices = 250 KB extra). Si emerge necesidad de loading time en `MoodPlayer`, abrir un hito futuro con schema bump claro.
- **Two-paths duplica complejidad**: editor + player con lógicas distintas para leer el mismo formato; refactor del `SceneLoader`. Beneficio (dependency cero del runtime + load time mejorado) es real pero no urgente — los brushes se compilan rápido al cargar (`buildBrushMesh` per-brush; ~2-5 ms para mapas típicos en debug).
- **No two-paths en F2H20 mantiene la suite verde sin tocar code paths críticos**: el MoodPlayer no cambia, no hay risk de regresión.

**Cómo se implementa el MVP:**
- `engine/world/csg/CompileMap.{h,cpp}` (puro, testeable sin GL): tipos `BrushSource` / `CompiledVertex` / `CompiledSubmesh` / `CompiledMap` / `CompileStats`. Funciones `collectFaces` (por cada cara: polígono local via duplicación del helper privado de `BrushMesh.cpp`, ordenado CCW, transformado a world), `markInternalFaces` (pareja exhaustiva i<j buscando antiparalelos + `polygonsMatch` ignorando orden de vertices), `compileMap` (paso 1 descubre paths en orden, paso 2 triangula con builders paralelos + spatial hash celda eps en world). El weld matchea **position + UV + normal** — vertices coincidentes en posición pero con UV/normal distintas se mantienen separados (split estilo OBJ flat shading). El `BrushSource.materialPaths` lleva un path lógico por slot (resolución `MaterialAssetId → string` via `AssetManager::materialPathOf`); agrupación final por path lógico, no MaterialAssetId, para reproducibilidad entre sesiones.
- **Cull pareja-exacta, no overlap parcial**: dos caras se cullean solo si los polígonos coinciden vértice-a-vértice ±eps (orden libre — uno con CCW y el otro con CW visto desde la primera normal) + `dot(n_i, n_j) < -0.9999`. Cubre el caso típico (cubos pegados); overlap parcial (caras que comparten solo PARTE) requiere clipping general — diferido si emerge necesidad.
- **UV preservation respeta `lockToWorld`** igual que `buildBrushMesh`: si false, transforma vertex world → local con `inverse(worldMatrix)` antes del proyectado sobre `uAxis`/`vAxis`. Si true, la posición world es input al UV calc directamente.
- `engine/world/csg/MapExportObj.{h,cpp}`: `writeObj(compiled, path)` produce `.obj` (mtllib + o + v/vn/vt globales + bloques `usemtl` por submesh + caras `f a/a/a` con índices 1-based + offset acumulado entre submeshes) y `.mtl` lateral (1 newmtl por path distinto, material vacío `""` → `_default` con `Kd 0.8 0.8 0.8`). `sanitizeMtlName` reemplaza no-alfanumérico (excepto `_.-/`) por `_` (formato MTL no tolera espacios en `newmtl`).

**Suite resultante:** **602/8323** (+20 cases / +104 asserts vs F2H19). Tests cubren empty / 1 box (12 tris / 24 verts) / 2 separados misma material / materiales distintos (orden estable) / 2 pegados con cull (20 tris) / brush degenerado / `markInternalFaces` aislado en 3 escenarios / weld global / contenido del `.obj` con `mtllib`/`o`/`v`/`vn`/`vt`/`f`/`usemtl` correctos / indices 1-based con offset entre submeshes (`f 25/...` en el segundo).

**Validación visual del dev:** spawn 2 brushes → "Compilar mapa" mostró stats coherentes en dialog → "Exportar OBJ..." escribió a `Desktop/test/test.obj` (2 submeshes, 24 tris) → editor cerró limpiamente.

**Alternativas descartadas:**
- **Persistir la compilación en `.moodmap`** (nivel 2): infla el archivo sin beneficio inmediato. Schema bump v12 → v13 con back-compat aditiva era doable; la decisión fue no agregar ese peso hasta que emerja un caso de uso real.
- **Cargar la mesh compilada en `MoodPlayer`** (nivel 3): refactor del `SceneLoader` con dos branches (editor vs player). Riesgo de regresión + dos paths a mantener. Diferido.
- **Cull por overlap parcial via clipping general**: complejo, requiere intersección polígono-polígono. El caso simple cubre el 80% (cubos pegados); el resto puede esperar.
- **Schema versionado del OBJ con `MoodEngine F2H20` en el header**: ya está el comentario `# MoodEngine F2H20 — compiled map`. Si el dev necesita validación más estricta del export, agregar checksum o version explícita en hito futuro.

**Revisar si:**
- El dev reporta que la mesh exportada no corresponde visualmente (probable bug de UV o normal transform).
- El dev pide cargar la mesh compilada en runtime (abrir hito propio nivel 2 o 3).
- El cull de overlap parcial empieza a ser pedido en validación (mapas grandes con muchos brushes pegados parcialmente).

## 2026-05-07: F2H21 — Material Editor con preview esférico, scope MVP vs node-graph del plan F2H17 original

**Contexto:** El plan F2 original entrada F2H17 agrupaba "Material editor con node-graph (visual)" — node-graph + nodos básicos (TextureSample, ColorConstant, ScalarConstant, Multiply, Add, Mix, Output) + preview esférico + persistencia, todo en un solo hito. F2H18 original era el follow-up "Shader graph runtime compilation" (genera GLSL en runtime + cache por hash). Tras evaluar el costo:

- Node graph visual implementado a mano: ~500-700 LOC de UI ImGui.
- Compilación runtime del graph a GLSL + cache por hash: ~300-500 LOC.
- Refactor de `SceneRenderer` para shaders custom por material: cada graph distinto = shader distinto, rompe el batching de F2H4 + risk de regresión.

Total ~ 1-2 semanas de hito grande.

**Decisión:** **F2H21 entrega solo el componente de mayor valor inmediato — preview esférico off-screen — sin pagar el costo del node graph.** El dev ve sus cambios de tint / sliders / drop de texturas en una esfera dedicada en el mismo panel sin tener que asignar el material a una entidad del viewport. El node graph queda anotado en PENDIENTES.md como hito futuro si emerge necesidad real (probable F2H24+).

**Por qué MVP:**
- El preview esférico es ~80% del valor visual del plan original con ~20% del scope.
- Sin refactor del SceneRenderer: F2H21 solo agrega un componente lateral, riesgo de regresión casi cero.
- Reusa el shader PBR del repo en lugar de un shader custom: trade-off es setear los uniforms del Forward+ aunque no haya point lights (SSBOs vacíos con count=0).

**Implementación:**

- `engine/render/preview/MaterialPreviewRenderer.{h,cpp}` (~330 LOC):
  - FBO LDR 256x256 + depth.
  - Reusa `pbr.vert` + `pbr.frag` + `assets.primitiveSphereId()`.
  - Cámara fija frontal + rotación lenta automática del modelo sobre Y a ~22 deg/s — el plan original era cámara fija pero el dev al validar pidió rotación. Tiempo del clock monotónico, sin estado interno acumulado.
  - 1 directional light fija desde 3/4 + IBL inyectado del SceneRenderer (no duplica disk-load). Si IBL no disponible, cae a `uAmbient = 0.20` (subido del 0.05 original tras feedback "mitad oscura demasiado negra").
  - SSBOs vacíos para Forward+ bindings 2/3/4: el shader requiere los binds aunque uTilesX=1/uTilesY=1 deshabiliten el iter.
  - Sin shadow + bind dummy 2D al slot uShadowMap (algunos drivers validan sampler2DShadow aún en branches no tomados).

- `AssetManager::saveMaterial(MaterialAssetId id)` (~70 LOC):
  - Serializa al path lógico con el mismo schema que `loadMaterial` lee.
  - Solo escribe campos de textura cuando slot != 0 (preserva contrato del loader).
  - Rechaza ids fuera de rango y sentinels (`__default_material`, `__tex#<id>`, `__runtime#<id>`), VFS sin resolve, errores de I/O.

- `MaterialEditorPanel` reescrito con polish post-validación:
  - **Layout adaptativo**: >=540px → 2 columnas (controles izq | preview der); <540px → vertical (preview ARRIBA, controles abajo).
  - **Selección inicial del primer no-sentinel**: slot 0 magenta sigue accesible vía dropdown pero no es default.
  - **Texture slots con descubribilidad** (3 fixes que emergieron al validar):
    - Botón "X" reservando 28px a la derecha (antes -FLT_MIN cortaba el X fuera del panel).
    - Slots vacíos con label "(vacio - drop textura aquí)" (antes mostraban path "textures/missing.png" del fallback y confundían).
    - Tooltips + header "(?)" explicativo.
  - **Botón Guardar** con feedback verde/rojo durante 120 frames.
  - **Logs de tracking discretos** (`IsItemActivated` pre, `IsItemDeactivatedAfterEdit` log delta) — sin spam por frame. Mismo patrón F2H16 Inspector.

- `EditorApplication`: `m_materialPreview` creado post-`m_sceneRenderer` con IBL inyectado, destruido **antes** del SceneRenderer en el dtor (refs no-owning a textures que el scene manage).

**Suite resultante:** **607/8341** (+5 cases / +18 asserts en `test_material_serializer.cpp`). **Bug fix Windows-specific**: el test que leía el JSON resultante mantenía un `ifstream` con handle abierto al hacer `std::filesystem::remove` → crash silencioso (exit 9, doctest summary cortado); arreglado leyendo en scope cerrado y usando overload con `std::error_code`.

**Validación visual end-to-end del dev**: confirmó funcionamiento — esfera rotando, dropdown actualiza la esfera, sliders refrescan en vivo, drop de textura desde AssetBrowser activa `useAlbedoMap=true` automáticamente, click Guardar persiste el `.material` (probado con `acero_pulido.material` roughness 0.20 → 0.16). Reformat JSON por `nlohmann json::dump(2)` con keys alfabéticas y floats con precisión máxima — tradeoff aceptado.

**Alternativas descartadas:**
- **Node graph completo en F2H21** (scope original del plan): 1-2 semanas. Diferido.
- **Shader simplificado para preview**: duplica código del PBR sin ganar mucho.
- **Orbit cam con mouse**: nice-to-have anotado en PENDIENTES.md.
- **Schema bump del `.material`**: conservar el actual; cuando emerja node graph, ahí sí campo `graph` opcional.

**Revisar si:**
- El dev pide preview en el Inspector también (mini-preview inline cuando hay material seleccionado).
- Algún material concreto (water shader, vegetation) necesita node graph — abrir hito propio.
- El polish UX general del editor (mencionado por el dev tras F2H21: *"a futuro deberemos mejorar toda la UI para hacerla más fácil de entender"*) emerge como prioritario antes del 4-viewport — re-priorizar.

## 2026-05-07: F2H22 — UX rework (workspaces orientados a tareas + visibility default + Toolbar + AssetBrowser tabs), adelantado vs sub-fase 2.7

**Contexto:** El plan F2 original tenía la sub-fase 2.7 (UI/UX final del editor, F2H41-F2H44, ~6+ meses) para el polish ergonómico. Tras feedback explícito del dev al cerrar F2H21 (*"a futuro deberemos mejorar toda la UI para hacerla más fácil de entender"*) reafirmado al arrancar (*"mientras quede bien ordenado"*), F2H22 adelanta parte de ese scope. El 4-viewport Hammer-style layout (era F2H22 candidato charlado tras F2H20) se mueve a F2H23 — el workflow va primero, las features visuales después.

**Decisión:** **F2H22 ataca 4 fricciones específicas** identificadas en uso real, no un rewrite UX completo. Scope acotado, ~1 día de trabajo.

1. **Renames de workspaces a tareas**: `Layout → Modelar`, `Scripting → Programar`, `Profile → Optimizar`, `Materials → Materiales`. Los nombres viejos no comunicaban "esto es para hacer X".
2. **Visibility default por workspace**: cada workspace solo muestra los panels relevantes a su tarea. Antes todos los panels estaban dockeados en todos los workspaces (solo cambiaba dónde) — ruido visual.
3. **Toolbar lateral nueva**: panel `Tools` con 6 botones para gizmo modes + brushes + face mode toggle. Tools críticos descubribles al ojo, no escondidos en menús.
4. **AssetBrowser refactoreado a tabs**: 6 tabs (Texturas/Meshes/Prefabs/Materiales/Scripts/Audio) reemplazan los CollapsingHeaders apilados. La lista de meshes ya no infla el panel.

**Por qué no un rewrite completo:**
- **Lo que funciona, no se toca**: el sistema F2H7 de workspaces con `WorkspaceManager` + `Dockspace` + `iniLayout` per-workspace es sólido. F2H22 es **aditivo** (rename de strings + nuevo método de visibility + nuevo panel + refactor del onImGuiRender del AssetBrowser) — sin tocar la arquitectura.
- **El feedback del dev es iterativo**: durante la validación visual surgieron 3 fricciones más (toolbar flotante al cargar proyecto, lista de meshes exagerada, iconos abstrusos). Resueltas en bloque F polish, sin abrir hitos nuevos.

**Cómo se implementa:**

- `WorkspaceManager` con helper `migrateWorkspaceName(oldName)` aplicado en `setWorkspaces` — los `.moodproj` viejos cargan con nombres nuevos preservando `iniLayout` intacto. Sin pérdida de configuración.
- `Dockspace::buildLayoutForWorkspace` con dispatcher que acepta nombres nuevos como primary + viejos como alias defensivo (defensa en profundidad si la migración no se aplicó).
- `EditorUI::applyDefaultVisibilityForWorkspace(name)` setea `panel->visible` por workspace. Llamado desde 3 sitios:
  1. Ctor de EditorUI (arranque inicial).
  2. `applyPendingWorkspaceSwitch` cuando el workspace de destino tiene `iniLayout` vacío (primer activado en este proyecto).
  3. MenuBar "Restablecer layout" (re-aplica visibility + dock layout default).
- `editor/ui/Toolbar.{h,cpp}` (~110 LOC) panel ImGui nuevo (categoría Scene). Botones de 72×36 con texto en castellano (no letras T/R/S — el dev confirmó que no quedaban claras). Cada botón emite request al EditorUI; EditorApplication consume cada frame en `run()`. Sin acoplamiento directo al EditorApplication. La franja del Dockspace en Modelar reservó 8% del ancho para anclar el Tools a la izquierda del Hierarchy.
- `AssetBrowserPanel::onImGuiRender` reescrito a `BeginTabBar` + 6 `BeginTabItem`. Cada tab: counter de items + `BeginChild` con scroll interno. Drag&drop preservado (mismos payloads).

**Polish post-validación (3 iteraciones)**:
1. Toolbar flotante al primer arranque + ventanas flotantes vacías en el centro: causa = `imgui.ini` global persistente del último uso. Fix: en `tryOpenProjectPath`, siempre `setActiveByIndex(0)` + `applyDefaultVisibilityForWorkspace("Modelar")` + `LoadIniSettingsFromMemory("")` cuando workspace 0 sin iniLayout custom + `requestRebuildForCurrentWorkspace()`. Antes el editor recordaba el último estado; ahora siempre arranca limpio en Modelar al cargar proyecto.
2. Iconos del Toolbar abstrusos: dev pidió iconos visuales tipo Blender. Compromiso: labels en castellano (`Mover`/`Rotar`/`Escala`/`Box`/`Cilindro`/`Cara`) + tooltips. FontAwesome / IcoMoon image-based anotado como pendiente futuro (requiere mergear font binaria al init de ImGui — ~5-10 LOC + binario).
3. Lista de meshes exagerada: 84 entries (Kenney Survival Pack 82 + 4 sueltos) sobrecargaba el tab Meshes. Fix: eliminar `assets/meshes/kenney_survival/` del repo. Quedan 5 demos básicos (CesiumMan/Fox/cube_mtl/pyramid). *"Más adelante los usuarios podrán descargar sus meshes"*.

**Suite resultante:** **610/8357** (+3 cases / +16 asserts en `test_workspace_manager.cpp`). Tests cubren: migración de los 4 nombres viejos completos, mezcla nombres nuevos+viejos (solo migra los viejos), preservación de nombres custom no reconocidos.

**Validación visual end-to-end del dev:** confirmó funcionamiento — tabs de workspace en castellano, cada workspace con sus panels relevantes, Toolbar lateral en Modelar con labels claros, AssetBrowser con 6 tabs scrolleables, editor arranca siempre limpio en Modelar. Pidió cerrar el hito.

**Alternativas descartadas:**
- **Rewrite UX completo del editor** (estilo Unreal/Unity): scope masivo. Sub-fase 2.7 original lo cubre cuando el motor esté maduro. F2H22 ataca solo las fricciones identificadas en uso real.
- **Rename de strings sin back-compat**: rompe los `.moodproj` existentes con nombres viejos. Migración aplicada en `setWorkspaces`.
- **Iconos image-based en F2H22**: scope extra (font binaria + integración + diseño). Diferido — labels en castellano cubren "saber qué hace cada botón".
- **Eliminar `kenney_survival` solo del AssetBrowser** (filter UI): hack — los archivos seguirían en el repo inflándolo. Mejor borrar de raíz, dejar 5 demos, y que los usuarios traigan sus meshes (cuando F2H8+ habilite drag-drop import o similar).

**Revisar si:**
- El dev pide deshabilitar la migración (workspaces con nombres custom que coinciden con los viejos). Improbable — los 4 nombres viejos son hardcoded del editor F2H7.
- El dev pide que el editor recuerde el último workspace al cerrar/abrir (en lugar de siempre Modelar). Compromiso entre "predecible" y "respetar el flow del dev". Si emerge, agregar toggle en preferencias del proyecto.
- El polish UX general continuo identifica más fricciones (Inspector / Hierarchy / Console / StatusBar). Hito propio cuando emerja presión.

## 2026-05-07: F2H23 — pase polish UX con 5 iteraciones de feedback en vivo + multi-edit Transform agrupado

**Contexto:** Tras cerrar F2H22 (rework UX con renames de workspaces + Toolbar + AssetBrowser tabs), el dev pidió continuar el polish sobre 4 panels que F2H22 no había tocado: Inspector / Hierarchy / Console / StatusBar. F2H23 lo cubre con el patrón heredado de F2H16/F2H19 — auditoría con subagente → lista priorizada → fixes acotados.

**Decisión clave:** **scope cerrado tras Bloque B** (auditoría) + **5 iteraciones de polish post-validación** durante Bloque F. Cada iteración emergió 2-3 bugs UX nuevos que el subagente no podía predecir sin uso real (tabs cruzados, panels que se mezclan al cambiar workspace, multi-edit que no funciona desde viewport, etc.). Sin abrir hitos nuevos por cada iteración — el plan F2H23 deliberadamente dejó scope acotado para permitir 5 iteraciones sin explotar.

**Auditoría inicial (Bloque B):** 32 ítems totales (13 ALTA / 13 MEDIA / 6 BAJA). Scope cerrado en 17 (13 ALTA + 4 MEDIA críticos):

- Inspector: helpMarker `(?)` con tooltip, SeparatorText × 9 componentes, drop targets con highlight verde durante drag activo (`isDragActiveOfType`), tooltips Transform.
- Hierarchy: iconos ASCII por tipo `[M]/[B]/[L]/[A]/[S]/[T]/[C]/[P]`, hint shortcuts arriba con tooltip, warning >5000 entidades.
- Console: popup confirmación Limpiar, leyenda completa de niveles con `(?)`, input filtro ancho dinámico, counter de líneas filtradas.
- StatusBar: FPS coloreado por rango, modo Play/Editor con color, "Ultimo:" → "Ultimo comando:".

**5 iteraciones de polish (las realmente importantes):**

1. **Iter 1 — multi-edit Transform en Inspector + tabs cruzados**:
   - Inspector con multi-edit: delta del active aplicado a todas las del SelectionSet usando `IsItemActivated`/`IsItemDeactivatedAfterEdit`.
   - DragFloat3 con `FramePadding(6,6)` para clickeabilidad.
   - Cada workspace dockea solo sus panels (Script Editor ya no aparece como tab cruzado en Layout).

2. **Iter 2 — workspace Optimizar fuera + nombres + Floor**:
   - Optimizar eliminado (era para benchmark Fase 1, no flujo cotidiano del dev). 3 workspaces ahora: Layout/Programar/Materiales.
   - "Modelar" → "Layout" (revert F2H22, pedido explícito del dev).
   - Hierarchy panel name() → "Escena" (más descriptivo + castellano consistente).
   - Floor default 16×16/tile=3 → 8×8/tile=1.5 (Floor 12×12m). Antes 48×48m hacía que un brush 1m se viera diminuto.
   - WorkspaceManager filtra workspaces obsoletos al cargar `.moodproj` viejos + completa con defaults si lista incompleta.

3. **Iter 3 — bugs operacionales**:
   - SmallButton "R" en lugar de "Recargar" grande en AssetBrowser.
   - EditorCamera radio default 30 → 12m (consistente con Floor más chico).
   - **Workspace switch SIEMPRE aplica visibility default** (no solo primera vez). Antes los panels "se mezclaban" al cambiar entre workspaces porque el iniLayout custom pisaba la visibility. Tradeoff aceptado: predecible > customización persistente. La customización del dev en un workspace persiste durante la sesión actual, pero al volver desde otro workspace vuelve al default.
   - Hierarchy invierte modifiers a **Maya-style**: Shift=ADD, Ctrl=TOGGLE (antes era Blender-style Shift=toggle/Ctrl=add). Pedido del dev: *"shift seleccionar ambos y de ahí mover"*.

4. **Iter 4 — layout default columna derecha**:
   - Layout default reescrito: columna derecha unificada (Escena arriba + Inspector abajo 50/50), Viewport ocupa centro completo, Toolbar franja izquierda. Antes Escena izq + Inspector der consumían 2 columnas.
   - `finalizeGizmoDrag` aplica delta del gizmo a todas las del SelectionSet AL SOLTAR — pero las "demás" saltaban visualmente al final, no se movían en vivo.

5. **Iter 5 — los 3 bugs reales del flow viewport (la crítica)**:
   - **(a) Drag visual no en vivo**: el dev veía solo el active moverse durante el drag y las demás saltaban al soltar. FIX: snapshot `otherStarts` (entidades extra del SelectionSet) al iniciar drag con sus startValues del Field correspondiente. Cada frame del drag aplica el delta del active a TODAS las demás en vivo. Aplica a Translate / Rotate / Scale (per-axis y uniform). Helpers `readTransformField` / `writeTransformField` (con clamp para Scale).
   - **(b) Ctrl+Z no agrupado**: el HistoryStack solo recibía el active. FIX: nuevo `MultiEditTransformCommand` (`editor/commands/MultiEditTransformCommand.{h,cpp}`, ~95 LOC) que encapsula `vector<{Entity, before, after}>` compartiendo el mismo Field. `execute()`/`undo()` iteran y aplican. `isNoOp` si todas las entries son nearlyEqual. Resilience con `valid()` chequeo por entry. `finalizeGizmoDrag` construye el `MultiEditTransformCommand` cuando `otherStarts` no está vacío; sino fallback al `EditTransformCommand` single. Push del command revierte primero todos los transforms al `startValue` para que `execute()` re-aplique sin doble-aplicación.
   - **(c) Shift+click en viewport no acumulaba**: el path de pickEntity en `EditorApplication::run()` ya tenía lógica F2H13 con `keyShift` y `keyCtrl` — pero usaba el orden viejo (Blender-style). FIX: invertir a Shift=ADD / Ctrl=TOGGLE para consistencia con HierarchyPanel. Logs explícitos en cada path: "[viewport] Shift+click ADD '...' (selected=N)".
   - Validación visual end-to-end: log muestra "[viewport] Shift+click ADD 'Brush_Cyl_01' (selected=2)" + "[gizmo multi-edit] push MultiEditTransformCommand: 3 entidades, field=0/1/2" probando los 3 modos. **Dev confirmó: "funciona"**.

**Suite resultante:** **610/8359** verde sin regresiones. Tests del WorkspaceManager actualizados al schema 3-default + filtro de workspace obsoleto. UI pura (Inspector / Hierarchy / Console / StatusBar) no testeable sin GL — validación visual del dev al cierre de cada iteración.

**Limpieza assets:** pack `kenney_survival` (82 meshes externos) eliminado del repo. Quedan 5 demos básicos (CesiumMan/Fox/cube_mtl/pyramid). Pedido del dev: *"5 nomás, más adelante los usuarios podrán descargar sus meshes"*.

**Alternativas descartadas:**
- **Refactor profundo de los panels** (rework completo): scope masivo. F2H23 atacó solo fricciones identificadas en uso real con auditoría dirigida.
- **MultiEdit con commits visibles solo al final** (iter 4 approach): el dev confirmó que NO funciona cuando las demás "saltan" al soltar. La versión en vivo (iter 5) es necesaria.
- **CompoundCommand genérico** para agrupar N commands cualesquiera: scope mayor que `MultiEditTransformCommand` específico para Transform. Diferido a hito futuro si emerge necesidad de agrupar otros tipos de commands (ej. crear+mover+pintar como un solo undo).
- **FontAwesome para iconos image-based**: deuda explícita F2H22 que F2H23 NO ataca. Hito chico futuro.

**Revisar si:**
- El dev encuentra más fricciones tras uso real prolongado (iter 6+): atender en hito propio si pasan las 3 iteraciones del polish.
- El dev pide que la customización de visibility por workspace persista entre switches: agregar toggle "modo predecible / modo customizable" en preferencias.
- Los archivos grandes empiezan a frenar el desarrollo: F2H24 resuelve esto (split por dominio).
- El gizmo de rotación con multi-selección revela bugs sutiles (rotación se aplica per-entity sin pivot común — esperado pero documentar si emerge confusión).

## 2026-05-08: F2H24 — split de archivos críticos >800 LOC en partials por dominio (refactor estructural sin cambios funcionales ni de API pública)

**Contexto:** F2H23 cerró con 5 iteraciones de polish UX donde el dev confirmó *"funciona"* pero también pidió explícitamente *"creo que hay archivos demasiado grandes que te cuesta arreglar, así que mejor debemos organizar, que ningún archivo tenga demasiadas líneas para que sea fácil de mantener"*. El cap soft 500 / hard 800 LOC por `.cpp/.h` ya estaba documentado en CLAUDE.md y en la memoria del proyecto, pero acumulamos 5 archivos CRÍTICOS >800 LOC durante Fase 1 y Fase 2: InspectorPanel.cpp 1338, EditorProjectActions.cpp 1272, DemoSpawners.cpp 1188, PlayerApplication.cpp 1160, EditorApplication.cpp 826. Total: 5784 LOC repartidos en archivos que el dev encontraba difíciles de mantener.

**Decisión:** F2H24 — refactor puramente estructural. Split de los 5 CRÍTICOS en archivos parciales con sufijo descriptivo (`Foo_<Dominio>.cpp`) implementando métodos privados de la **misma clase** declarada en `Foo.h`. Helpers compartidos en header interno `Foo_Internal.h` con namespace `Mood::detail`. API pública intacta. Cero cambios funcionales — el editor y el player arrancan idénticos al usuario final. Los 4 archivos ALTO (700-780 LOC) quedan en `PENDIENTES.md` para hito futuro si emerge presión.

**Razones:**
- **Cumplir cap del proyecto** (soft 500 / hard 800 LOC). F2H24 reduce los 5 CRÍTICOS para que ningún partial supere 500 LOC excepto `_Frame` y `_Run` que rondan 435-484 (loops monolíticos sin sub-secciones obvias).
- **Patrón ya validado en el repo**: `EditorApplication.cpp` ya tenía 6 partials desde Hito 16 (EditorProjectActions / DemoSpawners / EditorOverlay / EditorPlayMode / EditorRenderPass / EditorScene). F2H24 extiende el mismo patrón a InspectorPanel + DemoSpawners + PlayerApplication + EditorApplication.
- **Validación incremental**: build + suite verde después de cada Bloque B.X (5 sub-bloques, 5 commits intermedios). Permite detectar regresiones por TU sin debugar todo el split al final. La suite 610/8359 quedó idéntica antes y después de cada bloque (refactor sin cambios funcionales por construcción).
- **API pública intacta = cero riesgo de regresión externa**: solo `InspectorPanel.h` recibió 13 métodos PRIVADOS nuevos (`renderTagSection(Entity)`, etc.) para que el dispatch del `onImGuiRender` quede legible. El user-facing del editor + player + tests no ve diferencia alguna.
- **Headers internos `Foo_Internal.h` para helpers compartidos**: alternativa al namespace anónimo (que solo es visible dentro de un único `.cpp`). Patrón limpio aplicado a InspectorPanel (`pushEditIfDone` template + `helpMarker` + `isDragActiveOfType` con `inline`) y DemoSpawners (`WorldYBounds` + `rotatedAabbWorldY` con `inline`).

**Distribución LOC por archivo crítico:**

- **InspectorPanel.cpp 1338 → 11 archivos**: núcleo 77 (dispatch por `hasComponent<>`) + Internal.h + 10 partials por componente (Misc 82 = Tag+Camera+Trigger; Audio 103; Physics 106; Animation 108; Script 130; Transform 141; Light 160 = Light+Environment; MeshRenderer 177; Particles 192; Brush 208).
- **EditorProjectActions.cpp 1272 → 7 archivos**: núcleo 106 (confirmDiscardChanges + addToRecentProjects + load/saveEditorState — helpers compartidos) + _FileIO 329 (project lifecycle: new/open/save/saveAs/close/newScript) + _Package 101 (handlePackageProject único) + _Map 257 (multi-mapa: saveMapAs/newMap/openMap/setDefault/delete + sanitizeMapName + syncMapsSnapshot) + _Brush 117 (spawnBrushEntity helper + 7 handleAdd*Brush) + _Boolean 327 (snapshot helpers + buildWorldBrush + handleBooleanOp F2H12) + _Compile 108 (formatCompileStats + handleCompileMap + handleExportObj F2H20).
- **DemoSpawners.cpp 1188 → 5 archivos + Internal.h**: núcleo 41 (pushCreatedEntities) + Internal.h (WorldYBounds + rotatedAabbWorldY) + _Basic 199 (8 demos chicos: Rotator/HUD/PhysicsBox/Environment/PointLight/AudioSource/FireParticles/Trigger) + _Stress 376 (6 demos pesados: Enemy/Shadow/PbrSpheres/LightStress/AnimatedChar/StressTris) + _Prefab 214 (SavePrefab + ViewportPrefabDrop) + _Drop 399 (4 viewport drops con Face Mode awareness: Texture/Mesh/Material/Script).
- **PlayerApplication.cpp 1160 → 4 archivos**: núcleo 111 (mapWorldOrigin + buildTestMap + rebuildSceneFromMap) + _Init 224 (PlayerApplication ctor + tryLoadGameManifest + dtor) + _Frame 484 (processEvents + beginFrame + endFrame + updateCamera char controller + updateRigidBodies + run loop) + _SaveLoad 435 (drawMainMenu + applyLoadedSave + captureCurrentState + quickSave F5 + saveAs F6).
- **EditorApplication.cpp 826 → 3 archivos**: núcleo 173 (updateWindowTitle + markDirty + processEvents + beginFrame + endFrame + mapWorldOrigin + viewportAspect) + _Init 275 (glDebugCallback + ctor con SDL/GL/ImGui/sistemas/inyeccion + dtor) + _Run 435 (loop principal con dispatchers de UI requests + click-to-select Maya-style + system updates physics/scripts/triggers/animation/nav/particles/audio + render).

**Errores resueltos durante el trabajo (no requieren mención del dev pero quedan registrados):**
- **`glm/gtx/compatibility.hpp` agregado por error a `InspectorPanel_Brush.cpp`**: extensión experimental de glm que requiere `GLM_ENABLE_EXPERIMENTAL` antes del include. Eliminado — no era necesario para el código del partial.
- **`APIENTRY` undefined en `EditorApplication_Init.cpp` cuando se compilaba como TU separada**: la macro APIENTRY se define solo si `<windows.h>` se incluyó transitivamente, lo cual variaba según el orden de includes del partial. Fix definitivo: usar `GLAD_API_PTR` en lugar de `APIENTRY` para la firma del callback `glDebugCallback` — `GLAD_API_PTR` siempre lo define `glad/gl.h` (alias condicional de APIENTRY o vacío según platform). Patrón general: en partials, evitar macros que dependen de inclusión transitiva.
- **`FrameStats` undefined en `EditorApplication_Run.cpp`**: forward-declared en `SceneRenderer.h` (donde solo se usa como tipo de retorno opaco), definición completa en `IRenderer.h`. El partial necesitaba `IRenderer.h` además de `SceneRenderer.h` para que el `FrameStats stats = m_sceneRenderer->frameStats()` compilara.

**Alternativas descartadas:**
- **Refactor profundo con extracción de clases helper**: scope masivo + cambia API. F2H24 ataca solo el problema de tamaño con cambios mínimos.
- **Funciones libres en namespace anónimo dentro del partial** (en lugar de métodos privados de la clase): no pueden acceder a miembros privados (`m_ui`, `m_assets`, etc.). Solución vía `friend` declarations sería peor que agregar métodos privados al header.
- **Bloque C: split de los 4 archivos ALTO (700-780 LOC)** (`SceneRenderer`, `MeshLoader`, `EditorOverlay`, `AssetManager`): skipped por presupuesto. Bajo el hard cap 800 — menos urgentes. Movidos a `PENDIENTES.md` como deuda chica.
- **CompoundCommand genérico** para agrupar refactors en commits atómicos: F2H24 ya tiene granularidad por Bloque B.X (5 commits intermedios), suficiente para revertir si emerge regresión.

**Revisar si:**
- El dev encuentra que algún partial sigue sintiéndose grande (>500 LOC y costoso de editar): partir más fino. Candidatos probables: `_Frame` (484) y `_Run` (435).
- Los 4 archivos ALTO movidos a PENDIENTES.md (`SceneRenderer.cpp` 776, `MeshLoader.cpp` 767, `EditorOverlay.cpp` 745, `AssetManager.cpp` 743) crecen pasada la marca 800: abrir hito chico para split de los que estén sobre el cap.
- Aparece un caso donde la inclusión transitiva de macros varía entre TUs y rompe builds: estandarizar headers internos `Foo_Internal.h` para isolar dependencias macro-sensibles.
- Surge necesidad de un patrón de helpers compartidos por partials además de los 5 ya hechos: codificar el patrón `Foo_Internal.h` con namespace `Mood::detail` como convención del proyecto.

## 2026-05-08: Cull de overlap parcial via BSP polygon clipping (F2H25)

**Contexto:** F2H20 implementó la compilación brush → mesh estática con cull de **pareja exacta** (dos caras coincidentes con normales antiparalelas). Faltaba cull de **overlap parcial** — cuando una cara está parcialmente dentro de otro brush. Sin esto, mapas con brushes solapados arrastran tris invisibles dentro del volumen vecino.

### Decisión 1 — BSP-style polygon clipping (Sutherland-Hodgman extendido)

**Decisión:** algoritmo iterativo. Para cada cara F y cada brush B != A: clipear F contra los planos de B uno por uno. En cada plano, split en `above` (lado positivo = afuera del brush respecto a ese plano = output) + `below` (sigue siendo testeado contra los siguientes planos). Lo que sobrevive en `inside` tras todos los planos = adentro de B → descartar.

**Razones:**
- Polígonos convexos por construcción (`worldPolygonCcw` viene de `BrushMesh.cpp`). Sutherland-Hodgman funciona tal cual sin descomponer.
- Una cara contra un brush convexo = cara intersectada con la unión de half-spaces externos. El BSP iterativo materializa esa unión correctamente.
- Reusa infra existente: `Plane`, `signedDistance`, `kPlaneEpsilon`. Cero deps nuevos.

**Alternativas descartadas:**
- Clipping general polígono-polígono (Weiler-Atherton, Vatti): overkill para polígonos siempre convexos.
- CSG completo con Carve/manifold: refactor masivo del modelo.

### Decisión 2 — 3 pre-tests críticos antes del BSP loop

**Decisión:** pre-tests al inicio de `cullPolygonAgainstBrush`:
1. "Cara entera afuera de B" — output = poly entero sin partir.
2. "Cara entera adentro de B" — output vacío.
3. "Polígono coplanar a un plano de B" — emit en `below` (NO en `above`), saltea ese plano.

**Razones:**
- Sin pre-test 1, el BSP partía la cara en N trozos contiguos cuya unión era la cara entera (N-1 splits falsos + N draw calls innecesarios).
- Pre-test 2: descartar caras totalmente internas sin pasar por el loop O(P).
- Sin pre-test 3, cara coplanar con la pared exterior de B salía prematuramente como output cuando la sub-region central estaba dentro de B respecto a los OTROS planos.

### Decisión 3 — Stats: caras eliminadas enteras + fragmentos partidos (no "tris ahorrados")

**Decisión:** `culledOverlapTriangles` cuenta SOLO los tris de caras descartadas enteras. `splitFragments` cuenta caras partidas en >1 fragmento.

**Razones:**
- El BSP clipping puede AUMENTAR el tri count: una cara de 2 tris partida en 4 fragmentos puede generar 8 tris (cada fragmento independiente requiere su propia triangulación).
- El beneficio del cull es ÁREA visible (overdraw), no tri count global. Mapas grandes con mucho overlap se benefician en términos de overdraw.
- Reportar "tris ahorrados" sería engañoso: en muchos casos sería negativo.

### Decisión 4 — UI layout version stamp en `imgui.ini`

**Decisión:** cambiar `io.IniFilename = "imgui.ini"` → `"imgui_layout_v2.ini"`. Bumpear el sufijo cuando agreguemos paneles nuevos al dockspace.

**Razones:**
- Pedido directo del dev: "por defecto la UI sea fija, luego el usuario acomodará a su gusto".
- Tras F2H22 que agregó panel `Tools`, los `imgui.ini` viejos lo dejaban flotante al primer arranque.
- Trade-off aceptado: customizaciones en archivos viejos no se migran al bumpear. Frecuencia: 1-2 bumps por hito de UI. Aceptable.
- Sin file I/O custom: 1 línea de código.

**Alternativas descartadas:**
- ImGui Settings Handler propio que detecte versión: ~50 LOC de boilerplate por mejora marginal.

## 2026-05-08: Runtime-load de mesh compilada en MoodPlayer (F2H26)

**Contexto:** Plan original F2H14 hablaba de "brushes solo en el editor" — los brushes son herramienta de autoría; la entrega final al runtime es la mesh estática unificada. F2H20 entregó la compilación on-demand pero NO la persistía en `.moodmap` ni la consumía el Player. F2H26 cierra ese loop.

### Decisión 1 — Schema bump aditivo v12→v13, NO destructivo

**Decisión:** `compiledMesh` opcional al top-level del JSON. Mapas v12 cargan como v13 con `compiledMesh = nullopt`. Los brushes siguen persistidos siempre (para que el editor pueda re-editar).

**Razones:**
- Back-compat sin migración: cualquier `.moodmap` existente sigue cargando.
- Editor sigue editando brushes: el `compiledMesh` es OUTPUT del editor + INPUT del Player. El Editor lo escribe pero NO lo usa para render.
- Roundtrip preserva todo: brushes individuales + compiledMesh coexisten. Al guardar tras editar, el editor regenera el compiledMesh desde los brushes nuevos.

**Alternativas descartadas:**
- Reemplazar `brushes` por `compiledMesh`: rompe edición.
- Persistir compiledMesh en archivo separado: más complejidad sin beneficio claro para mapas chicos.

### Decisión 2 — Layout PBR de 11 floats interleaved (mismo que `brushSubmeshToInterleaved`)

**Decisión:** `SavedCompiledSubmesh.vertices` es `vector<f32>` con 11 floats por vertex (pos+color+uv+normal), indices ya expandidos sin EBO.

**Razones:**
- Mismo formato que `brushSubmeshToInterleaved` produce: el editor reusa el helper. Cero código nuevo de serialización.
- Compatible con shader PBR estándar: `kPbrAttrs` se reusa para BrushComponent y CompiledMeshComponent. Un solo render path.
- Color=1 fijo: el `albedoTint` del material domina; mantener color real no aporta.

**Alternativas descartadas:**
- 8 floats sin color: obligaría VAO distinto. No vale el ahorro.
- EBO con indices separados: requeriría expandir al cargar. Optamos por expandir al SAVE (una vez) para load-time mínimo en Player.

### Decisión 3 — `CompiledMeshComponent` move-only + 1 entity por mapa

**Decisión:** Componente nuevo con `vector<unique_ptr<IMesh>>` + `vector<MaterialAssetId>` paralelos. Move-only. Player crea **una sola entity** "WorldCompiledMesh".

**Razones:**
- Mismo patrón que F2H17 BrushComponent multi-material. Vector paralelos, 1 draw call por submesh.
- 1 entity por mapa: la mesh compilada es UN objeto unificado.
- Move-only por unique_ptr: evita copias accidentales.

**Alternativas descartadas:**
- Una entity por submesh: genera N entities cuando 1 alcanza. Sin beneficio.
- Reusar `MeshRendererComponent`: no aplica (MeshRenderer apunta a MeshAssetId persistido; compiled mesh es runtime-creada).

### Decisión 4 — Flag `useCompiledMesh` en SceneLoader, NO un loader separado

**Decisión:** `applyEntitiesToScene(saved, scene, assets, bool useCompiledMesh = false)`. Editor pasa default `false`; Player pasa `true`.

**Razones:**
- Una sola función, dos modos: evita duplicar código de carga de tiles/entidades/lights.
- Default `false` es seguro: callsites existentes (Editor + tests) no cambian.
- Fallback automático: Player con flag `true` pero sin compiledMesh (mapa v12 legacy) cae a procesar brushes. Transición v12→v13 transparente.

**Alternativas descartadas:**
- Dos funciones distintas: duplica código.
- Flag implícito desde `mode` global de la app: oculta la decisión, dificulta testing.

## 2026-05-08: F2H27 (F6 panel) descartado durante implementación

**Contexto:** F2H27 estaba planificado como "F6 panel estilo Blender — ajustar params del último operator post-hoc" (diferido desde F2H16). Durante implementación, recortado a versión "F6-light" con sliders Position/Rotation/Scale del último brush spawneado. El dev al verlo: *"no entiendo porque aparece esta información acá si directamente para eso tengo el inspector, es redundante"*.

**Decisión:** descartar F2H27 entero. Código removido, sin tag.

**Razones:**
- F6-light era redundante con Inspector: ya muestra Transform editable de la entidad seleccionada.
- F6 real de Blender requiere parametrizar comandos: ajustar `size`/`segments`/`materialDefault` al spawn — params que NO están en Inspector y requieren metadata por comando + UI dedicada + re-ejecución del comando con nuevos params. Scope de hito grande propio.
- Reconocer el error rápido: el dev marcó la redundancia en la primera validación. Descartar antes de invertir más en algo sin valor agregado.

**Alternativas consideradas:**
- Mantener F6-light para evitar abrir Inspector cuando el brush no está seleccionado: marginal — Ctrl+click selecciona y abre Inspector con 1 click extra.
- Implementar F6 real ahora: scope grande, no priorizado.

**Revisar si:**
- El dev pide explícitamente "ajustar params del operador post-spawn" (ej. cambiar `segments` de un cilindro tras spawnearlo): abrir hito propio con parametrización formal de comandos.


