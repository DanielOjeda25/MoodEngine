# Estado actual del proyecto — handoff para el agente

> **Propósito de este documento:** dar al próximo agente (Claude Code en Antigravity) el estado exacto del proyecto, comandos que ya sabemos que funcionan, y el siguiente paso concreto. Leer esto **antes** de `MOODENGINE_CONTEXTO_TECNICO.md` para saber dónde estamos parados.

---

## 1. ¿Dónde estamos?

**🚀 Fase 2 — F2H37 cerrado: FontAwesome icons en el resto del editor + polish UX general.**
Tag: `v1.27.0-fase2-hito37`.
Verificado visualmente por dev: MenuBar (los 6 menus + workspace tabs + Play/Stop con icon), Hierarchy (icon FA por tipo + multi-select polish naranja/amarillo/gris), VisGroupsPanel (consolidado al helper compartido), Inspector (icons en headers de 13 components), AssetBrowser (icons en 6 tabs), Console (icons por nivel + level filter toggles), StatusBar (FPS/mode/sub-mode con icons). Editor arranca sin asserts. Tour visual end-to-end OK.

**🏁 Hammer Editor cerrado funcional al 100% + iconos FontAwesome en TODO el editor + polish UX consolidado.** 35/44 hitos de Fase 2.

**Decisiones clave de F2H37:**
- **Hito unificado**: fusiona "extender FontAwesome al resto del editor" (post-F2H36) + "Pase de polish UX general continuo" (anotado desde F2H21+F2H22). Mismos paneles tocados → un solo hito sin doble pasada. Validado por dev al pedir el scope unificado.
- **Header `IconHelpers.h` separado de `IconsFontAwesome6.h`**. Razón: `IconsFontAwesome6.h` queda como "tabla pura de macros UTF-8" sin dependencias de scene/components; `IconHelpers.h` agrega los helpers que dispatchean sobre presencia de componentes (depende de `Components.h`/`Entity.h`). Inline en el header — no requiere .cpp porque es un switch puro.
- **`iconForEntity(Entity)` consolida `entityIconStr` duplicado** que vivía en HierarchyPanel + VisGroupsPanel pre-F2H37. Mismo orden de prioridad (MeshRenderer > Brush > Light > Audio > Script > Trigger > Camera > Particle > sin componente). Cualquier hito futuro que agregue un nuevo component type renombra/extiende este helper en un solo lugar.
- **Polish multi-select del Hierarchy con 3 colores**: naranja (active), amarillo claro (en seleccion pero no active), gris (hidden por VisGroup). Pre-F2H37 las secundarias se veían iguales al ImGui default — el dev tenia que abrir el Inspector para saber cuál era la primary.
- **Console level filter con 6 toggles SmallButton** en lugar de un dropdown. Razón: el dropdown obliga a un click + open + click; los toggles permiten activar/desactivar varios niveles con clicks puntuales. Cada toggle muestra el icon + tinte coloreado cuando ON, tenue cuando OFF — el estado se ve sin abrir nada. Estado persistido en `m_levelEnabled[6]` del header del panel (no en `.moodproj` — es un toggle ergonomico de sesión, default a "mostrar todos").
- **Rows del AssetBrowserPanel sin icon-por-row**. Razón: cada tab es type-pure (todos meshes en Meshes), agregar icon a cada row sería redundante con el icon del tab. El audit inicial lo proponía pero al implementar emergió que era ruido visual.
- **InspectorPanel_Brush.cpp upgrade TextDisabled → SeparatorText** durante el pase. Razón: los demás partials usan SeparatorText (F2H23 convention); el `TextDisabled("Brush (CSG)")` original era inconsistente. Fix gratis al estar tocando el header.
- **Fix lateral em-dash tofu en Welcome modal**: el carácter `—` (U+2014, General Punctuation) no está en ProggyClean ni en mi `k_iconRange` (FA Private Use Area). Pre-F2H36 ya se veía como `?` pero nunca se notó hasta el tour visual del Bloque I. Fix mínimo: reemplazar por `-` (hyphen-minus, U+002D, Basic Latin). Cambiar la default font a Lato (que ya está en `assets/ui/fonts/`) sería scope mayor — hito propio si emerge.
- **NO se agrega icon-por-tipo en cada row del AssetBrowser** (audit inicial lo sugería). Razón: cada tab es type-pure, redundante.
- **Width del botón Play/Stop bumped 64→80 px** para acomodar `ICON_FA_PLAY " Play"` / `ICON_FA_STOP " Stop"`.

**Implementación (F2H37 Bloques A-J en commits):**

- **Bloque A**: plan en [`archive/plans/PLAN_HITO_F2H37.md`](archive/plans/PLAN_HITO_F2H37.md).
- **Bloques B-H unificados**: TTF asset ya existía (F2H36) → solo extender header + crear helper + aplicar a 7 paneles + polish.
- **Fix lateral em-dash** (Bloque I validación): 3 ocurrencias en `EditorUI.cpp`.
- **Bloque I**: build + validación visual end-to-end con dev.
- **Bloque J (este commit)**: docs + tag.

**Pendientes conocidos** (post-F2H37):
- **TBD — definir con el dev**. Opciones:
  - Sub-fase 2.5 gameplay (diálogos / quests / inventario).
  - HUD procedural/minimalista del MoodPlayer (interés expresado post-F2H35).
  - Otra sub-fase del PLAN_FASE2 (optimización / runtime).
- **Cambiar default font ImGui a Lato** (existe en assets pero no se carga): nice-to-have. Resolvería el tofu del em-dash y mejoraría legibilidad general. Pero requiere revisar todo el editor para verificar que el spacing/alineamiento siguen OK con la métrica nueva. Hito propio si emerge.
- Validación full del Player con compiledMesh: deuda menor heredada de F2H26.

**Próximo paso**: **TBD — definir con el dev**.

### F2H36 (anterior, ya cerrado)

**🚀 F2H36 cerrado: FontAwesome icons en toolbars del editor de mapas.**
Tag: `v1.26.0-fase2-hito36`.
Verificado visualmente por dev: los 17 botones del Toolbar lateral (Mover/Rotar/Escala/Box/Cilindro/Cara) + MapEditorTopBar (Selecc./Bloque/Pincel/Clip/Objeto/Vertex/Edge/Cara/Snap V/Nombres/Carve) renderean con icono FA + label castellano. Editor arranca sin asserts. Mini-hito chico que cierra deuda arrastrada desde F2H22.

**🏁 Hammer Editor cerrado funcional al 100% + iconos en toolbars del workspace de mapas.** 34/44 hitos de Fase 2.

**Decisiones clave de F2H36:**
- **FontAwesome 6 free solid (`fa-solid-900.ttf`, ~417 KB)** descargada del repo oficial `FortAwesome/Font-Awesome` rama `6.x` al path `assets/ui/fonts/`. Asset estático commiteado al repo (no se regenera, no requiere build step).
- **Header `IconsFontAwesome6.h` con subset de ~15 macros**, no el header full de ~2000. Macros encoded en UTF-8 con escapes hex (`"\xef\x86\xb2"`) para no depender de la code page del source MSVC. Agregar un icono nuevo en hitos futuros = extender este header explícitamente (no autoimport).
- **Merge con default font (ProggyClean) usando `0.0f` como SizePixels**, no `13.0f`. Razón: ImGui 1.92 introdujo asserts que verifican que el merge use el mismo "reference size" que la dst font (implicit en `AddFontDefault`). Pasar 13.0f explicit triggera assert. Patrón confirmado en `imgui-src/docs/FONTS.md`.
- **GlyphMinAdvanceX dropeado** (override de mono-spacing). Razón: ImGui 1.92 tira otro assert si combinas glyph advance overrides + size 0.0f. Default rendering es suficiente — los iconos quedan al alto natural alineados con el texto.
- **Width del Toolbar bumped 72→92 px** para acomodar icon + label castellano sin truncar (`ICON_FA_CIRCLE " Cilindro"` no entra en 72 px).
- **Scope explícitamente acotado a los 2 toolbars del workspace "Editor de mapas"**. MenuBar / Hierarchy / Inspector / AssetBrowser / Console / StatusBar / paneles VisGroups/Material/Script quedan diferidos a F2H37 dedicado. Decisión alineada con la convención "un hito = un dominio acotado". Validado por dev al pedir continuar: *"deberemos integrarlos en otras areas del proyecto para que todo sea equitativo"*.

**Implementación (F2H36 Bloques A-E en commits feat + cierre):**

- **Bloque A**: plan en [`archive/plans/PLAN_HITO_F2H36.md`](archive/plans/PLAN_HITO_F2H36.md).
- **Bloques B+C unificados**: TTF asset + `IconsFontAwesome6.h` + merge en `EditorApplication_Init.cpp` + iconos en `Toolbar.cpp` + `MapEditorTopBar.cpp`.
- **Bug fix iter 2 — ImGui 1.92 asserts**: dropear PixelSnap typo (era PixelSnapH) + pasar 0.0f size al merge + dropear GlyphMinAdvanceX. 3 fixes acoplados resueltos en una sola iteración tras lanzar el editor 2 veces.
- **Bloque D — validación visual**: dev confirma iconos OK en los 17 botones, sin tofu.
- **Bloque E (este commit)**: docs + tag.

**Pendientes conocidos** (post-F2H36):
- **F2H37 — extender FontAwesome al resto del editor** (próximo, pedido explícito del dev): MenuBar (Archivo / Editar / Ver / Ayuda + tabs Layout/Scripting/...), Hierarchy con icon-por-tipo de entity (cube=mesh, lightbulb=light, music=audio, video=camera, etc.), Inspector con icons en headers de componentes, AssetBrowser con icons por tipo de asset (image=textura, music=audio, file-code=script, etc.), Console con icons por nivel (info/warn/error), StatusBar, paneles VisGroups/Material/Script. Plan a redactar al arrancar.
- **HUD del juego procedural/minimalista** (interés expresado post-F2H35): MoodPlayer con HUDs estilo Mirror's Edge / Doom Eternal vía ImGui DrawList o shaders custom. Diferido tras F2H37.
- Validación full del Player con compiledMesh: deuda menor heredada de F2H26.

**Próximo paso**: **F2H37 — extender FontAwesome al resto del editor**. Plan en `docs/PLAN_HITO_F2H37.md` cuando arranquemos.

### F2H35 (anterior, ya cerrado)

**🚀 F2H35 cerrado: Polish editor (UX viewport + Hammer-style visual).**
Tag: `v1.25.0-fase2-hito35`.
Verificado por dev end-to-end: editor arranca maximizado con dockspace balanceado (sin "Restablecer layout" manual); brushes en VisGroup tintan su color en los 3 ortos; point entities por tipo (Light amarillo / Audio naranja / Trigger verde / Camera azul / Particle violeta) con cubitos en orto + iconos 2D en perspective + labels flotantes con tag (toggle "Nombres" persistido); face picking muestra hover preview cyan antes de clickear; gizmo Rotate constante en pantalla (igual que Translate/Scale).

**Decisiones clave de F2H35:**
- **Editor maximizado via `SDL_GetDesktopDisplayMode` + `SDL_WINDOW_MAXIMIZED`** en lugar de 1280x720 + maximize async. Razón: `SDL_GetWindowSize` en el primer frame devolvía 1280x720 stale, el rebuild del Dockspace usaba ese WorkSize, los splits se persistían como offsets absolutos al ini → dockspace descuadrado al maximizar la ventana real. Crear directamente al display garantiza dimensiones correctas desde el frame 0.
- **Stamp `k_IniLayoutStamp = 1` per-proyecto.** El bumpear `imgui_layout_v3.ini` cubre el ini global, pero los `.moodproj` también guardan `iniLayout` por workspace (F2H7). Stamp invalida los iniLayouts persistidos cuando el dockspace builder cambia (mismo patrón que el ini global). Ausente = legacy 0 → invalidar y rebuild fresh con WorkSize correcto. Sin esto los proyectos viejos seguían descuadrados aunque la ventana arrancara maximizada.
- **Tint VisGroup color solo en wireframe orto, NO en perspective.** Perspective ya renderea PBR completo (no wireframe excepto outlines de selected); tintar el wireframe del VisGroup tendría sentido solo en orto donde todo es wireframe. Mantiene consistencia: tipo va al perspective via icon, organización (VisGroup) va al orto via wireframe color.
- **Cubitos point entity en orto: tamaño FIJO (r=0.4)**, NO proporcional al snap. Razón: con snap=64 los cubitos inflaban la vista; con snap=1 desaparecían. `r=0.4` matchea `k_iconPickRadius=0.6` de ScenePick para que el área visible coincida con el área pickable (sin sorprender al dev "lo veo pero no lo agarro").
- **Hover preview de face picking en cyan brillante** `(0.10, 0.95, 1.00)`. Probado: blanco tenue inicial desaparecía sobre texturas claras (probado contra textura blanca tilada del proyecto del dev). Cyan saturado contrasta con amarillo (active selected) y naranja (secondary selected) — el dev distingue hover-preview vs ya-seleccionado a primera vista.
- **Gizmo Rotate constante en pantalla = Translate/Scale.** Pre-F2H35 era `0.6 * max(localAabb)` world-space (F2H30 Bloque D) — al alejar la cam se hacía chico mientras los handles de Translate/Scale se mantenían a 60 px. Fix: `worldRadius = TARGET_PX / pixelsPerWorld` derivado de `(h/2) / (camDistance * tan(fovY/2))`. Target 70 px (mayor que k_armLen=60 para que el ring rodee los handles sin solaparse).
- **Toggle "Nombres" default ON, persistido por proyecto.** Default OFF (Hammer original) sería más alineado con la convención clásica, pero el dev pidió explícitamente *"labels default On"*. Persistencia opcional en `.moodproj` solo si != default (mismo patrón que coyoteWindowSec del Hito 40 G — los `.moodproj` viejos no se ensucian).
- **Detalles por tipo dentro del cubito orto descartados.** En iteración inicial agregué rayos/X/diagonal/frustum/burst dentro del cubo para diferenciar tipos sin label. Feedback dev: *"demasiado grande, como lo hace el hammer editor de valve?"*. Hammer Source clásico usa cubito chico + label de texto. La diferenciación fina viene del label (nombre de la entidad), no de la forma del icono — eso queda para Hammer-Source 2 / Hammer++ (FontAwesome icons, hito futuro).

**Implementación (F2H35 Bloques A-G en 9 commits feat/fix + cierre):**

- **Bloque A** (`a7c6484`): plan archivado.
- **Bloque B** (`cdf1cff` + `d0ea5e2`): editor maximizado + 3 fixes layout descuadrado.
- **Fix lateral** (`1734537`): VisGroupsPanel ColorPicker live-preview.
- **Bloque C** (`befdd29`): tint wireframe brushes por VisGroup color en ortos.
- **Bloque D** (`88bdba8`): paleta por tipo + iconos 2D perspective + cubitos orto + fix pickable Trigger/Camera/Particle.
- **Bloque E** (`d5c8da1`): labels point entities + toggle "Nombres" + feedback selección orto.
- **Bloque E.5** (`db1823a`): fix gizmo Rotate proporcional a cam.
- **Bloque F** (`4cf5e89`): hover preview face picking cyan.
- **Persistencia toggle** (`c44383f`): `Project::showEntityLabels` opcional.
- **Bloque G** (este commit): docs + tag.

**Pendientes conocidos** (post-F2H35):
- **Iconos image-based del Toolbar** (FontAwesome merge): pendiente desde F2H22, ahora reforzado por el feedback de iconos de point entities. Bajo riesgo, alto impacto visual. Probable hito chico futuro.
- **Hover preview en orto** (no perspective): F2H35 Bloque F solo cubre perspective; los ortos ya usan wireframe color del Bloque C que es feedback razonable, pero pintar la cara hovered cyan en orto requeriría pasarle el cursor del orto al pickFace y dibujar overlay específico. Diferido — emerge si el dev lo pide.
- **VisGroups jerárquicos / drag desde Hierarchy / lock**: features avanzadas Hammer 4. Diferidas hasta que emerja necesidad real.
- Validación full del Player con compiledMesh: deuda menor heredada de F2H26.

**Próximo paso**: **TBD — definir junto al dev**. Hammer Editor cerrado funcional al 100% + polish viewport + visual style. Opciones a considerar:
- Sub-fase 2.5 gameplay (diálogos / quests / inventario).
- Otra sub-fase del PLAN_FASE2 (optimización / runtime / polish UI general).
- Hammer-Source 2 polish (FontAwesome icons + posibles improvements).

### F2H34 (anterior, ya cerrado)

**🚀 F2H34 cerrado: Multi-face material drop.**
Tag: `v1.24.0-fase2-hito34`.
Verificado por dev: face mode → shift+click N caras → drop textura o material → las N caras reciben el material en una sola operación.

**Decisiones clave de F2H34:**
- **Sin command class nuevo, extender el existente.** `EditBrushFaceMaterialCommand` cambió `u32 faceIndex` + `u32 oldFaceMatIndex` + `u32 newFaceMatIndex` por `vector<u32>` paralelos. Constructor 1-cara preservado como wrapper que rellena vectores tamaño 1 — back-compat total con call-sites pre-existentes y los 7 tests del F2H19. Razón: sumar `EditBrushFacesMaterialCommand` (plural) sería duplicar 90% del código y forzar a las llamadoras a elegir entre dos clases con la misma intención.
- **Snapshot único de `bc.materials` compartido entre N caras.** Cuando el material es nuevo, un solo `push_back` al vector y todas las caras del set apuntan al mismo slot. Sin esto cada cara podría duplicar el slot al apply, inflando `bc.materials` y rompiendo el undo (el snapshot post se desincroniza con el real).
- **Validación atómica de faceIndices en `apply`** (todo o nada): si un solo faceIndex está fuera de rango, no muta nada. Garantiza que un command corrupto nunca deje el brush en estado parcial. Cubierto por test "faceIndex fuera de rango en una cara → no muta nada".
- **Helper `tryAssignMaterialToSelectedFaces` en namespace anónimo** del `DemoSpawners_Drop.cpp`, no en EditorApplication. Razón: la lógica solo existe en el flow de drop (texture + material drop comparten 100%); poner el helper en el header de EditorApplication la expondría a otros call-sites que no la necesitan. Devuelve `nullptr` cuando no aplica → llamadora cae al flow object-mode existente sin if extra.
- **Label dinámico singular/plural** según `selectedFaceIndices.size()`. "Asignar textura a cara" vs "Asignar textura a caras" en Editar > Deshacer.
- **NO se incluyó fix de "abrir maximizado" ni toggle wireframe/render** en este commit: el dev pidió no contaminar el cierre de F2H34 con cambios fuera de scope. Ambos quedan en F2H35 como mini-hito UX viewport propio.

**Implementación (F2H34 en 2 commits feat + 1 commit docs/cierre):**

- **Commit `<hash>` — feat(editor): EditBrushFaceMaterialCommand multi-cara**: extensión a vectores + constructor 1-cara wrapper + validación atómica + 5 tests nuevos.
- **Commit `<hash>` — feat(editor): drop textura/material aplica a multi-cara**: helper `tryAssignMaterialToSelectedFaces` + refactor de los 2 call-sites (texture + material drop) + label dinámico singular/plural.
- **Commit `<hash>` — docs(F2H34 cierre)**: HITOS / ESTADO / DECISIONS / PENDIENTES.

**Pendientes conocidos** (post-F2H34):
- **F2H35 — UX viewport polish (mini-hito)**: (1) abrir editor maximizado por defecto (~one-liner SDL flag); (2) toggle wireframe/render shading en viewport con botones overlay estilo Blender (~1-2h, toca render pipeline + un flag de mode). Identificado en validación F2H34.
- **Resto de "Hammer-style visual polish"** (mismo mini-hito o aparte): tint del wireframe por color del VisGroup, color por tipo de entity, labels arriba de point entities.

**Próximo paso**: **F2H35 — UX viewport polish**. Empezar por el fix de maximizado (trivial) + decidir si el toggle wireframe/render entra en el mismo hito o se separa.

### F2H33 (anterior, ya cerrado)

**🚀 F2H33 cerrado: Organización + face polish (VisGroups + multi-select de caras + texture alignment).**
Tag: `v1.23.0-fase2-hito33`.
Verificado por dev: VisGroups crear/asignar/toggle/eliminar funciona end-to-end; persistencia v13→v14 round-trip OK; face mode con multi-select via Shift+click pickea cualquier brush hovered en 1 click + active en amarillo / secundarias en naranja; alignment ops funcionan single y multi.

**Decisiones clave de F2H33:**
- **Schema bump aditivo v13→v14** mismo patrón que F2H26 v12→v13: array opcional `visgroups` top-level + campo opcional `visgroupId` por entity y brush. Mapas v13 cargan como v14 sin pérdida (array vacío + sin membership).
- **VisGroups planos (no jerárquicos)** alineados con Hammer 4 — sub-grupos diferidos si emerge demanda real. Membership 1-a-N (entity en máximo 1 grupo); refactor a `unordered_set<u64>` si emerge.
- **Player ignora VisGroups**: convención Hammer = VisGroups son del editor, el build final incluye todo. `applyEntitiesToScene(useCompiledMesh=true)` skipea `resetVisGroups` y no agrega `VisGroupMembershipComponent`. Si el dev oculta una torre para trabajar y luego prueba en Player, la torre se ve igual.
- **Refactor `SelectionSet`** breaking-internal: campo `i32 activeFaceIndex` (F2H17) → `std::vector<i32> selectedFaceIndices` + método `activeFaceIndex()` derivado de `back()`. Invariante: el último del vector es siempre la "active" (= primary para ops single-face). 7 call-sites migrados.
- **Face picking robust** (fix UX descubierto en validación): pre-F2H33 el pickFace solo testeaba el brush active → clickear otro brush requería 2 clicks. Fix: `pickEntity` primero, `pickFace` contra el brush hovered, replace + single si distinto del active. Resuelve la queja del dev *"la seleccion es como dificil... debo rotar y probar varias caras"*.
- **Active face en amarillo, secundarias en naranja**: distingue visualmente cuál es la primary cuando hay multi-select. Antes era todo naranja Half-Life uniforme.
- **Reuso de `EditBrushUVCommand` para alignment**: ya capturaba snapshot completo del brush, soporta multi-cara nativamente sin command nuevo.
- **"Treat as one face" con axisU/V heterogéneos** = log warn + aplica igual: heurística `dot(face.axis, primary.axis) < 0.99` detecta sistemas distintos. El resultado puede salir feo pero es decisión consciente del dev (Hammer 4 hace lo mismo).
- **Refactor CMake colateral**: race condition pre-existente en `add_custom_command POST_BUILD` de MoodEditor + MoodPlayer (ambos hacían `copy_directory` a la misma `Debug/`). Fix: `add_custom_target(mood_runtime_files ALL)` centralizado con `add_dependencies` para serializar el deploy sin perder paralelismo en compilación.
- **Bug fix `Ctrl++` en teclados ES sin numérico**: pre-F2H33 solo aceptaba `SDLK_EQUALS` (US/UK) y `SDLK_KP_PLUS` (numpad); en layout español la tecla a la derecha de Ñ manda `SDLK_PLUS`. Fix agregar al handler.

**Implementación (F2H33 Bloques A-E en 5 commits feat/fix + cierre):**

- **Bloque A (commit `<hash>`)**: plan en [`archive/plans/PLAN_HITO_F2H33.md`](archive/plans/PLAN_HITO_F2H33.md).
- **Bloque B (commit `<hash>`)**: VisGroups + schema bump + render/pick gates + grayed Hierarchy + Player skip + CMake refactor.
- **Bloque C (commit `<hash>`)**: multi-select de caras (Shift+click) + face picking robust + Inspector multi-cara.
- **Bloque D (commit `<hash>`)**: texture alignment Hammer-style (Align/Fit/Justify L/R/T/B + Treat as one face).
- **Fix lateral (commit `<hash>`)**: SDLK_PLUS para Ctrl++ del snap step en teclados ES sin numérico.
- **Bloque E (este commit)**: docs + tag.

**Pendientes conocidos** (post-F2H33):
- **Hammer-style visual polish** (sub-bloque candidato a hito chico futuro): tint del wireframe por color del VisGroup (~30 LOC, alto valor visual), color por tipo de entity (luz=amarillo, audio=naranja, etc.), labels arriba de point entities, mejoras al face picking (puede haber casos borde donde el rayo no pega). Diferidos como paquete.
- **Multi-face material drop**: el handler de `DemoSpawners_Drop.cpp` sigue aplicando solo al face active. Para que aplique a las N caras seleccionadas hay que extender `EditBrushFaceMaterialCommand` a multi-cara. Diferido — emerge si el dev lo pide.
- **VisGroups jerárquicos / drag desde Hierarchy / auto-asignar a current group / lock de VisGroup**: scope mayor, diferidos hasta que emerja necesidad.
- Validación full del Player con compiledMesh: deuda menor heredada de F2H26.

**Próximo paso**: **TBD — definir junto al dev**. Hammer Editor está cerrado funcional al 100%. Opciones a considerar:
- Sub-fase 2.5 gameplay (diálogos / quests / inventario).
- Hammer-style visual polish (sub-bloque del polish UX continuo).
- Otra sub-fase del PLAN_FASE2 (optimización / runtime / polish UI general).

### F2H32 (anterior, ya cerrado)

**🚀 F2H32 cerrado: Geometry tools (clip tool 2-click + carve UI Hammer-style).**
Tag: `v1.22.0-fase2-hito32`.
Verificado por dev: clip splittea 1 brush en Front/Back/Both según `T` cycle; carve resta brush activo por todos los intersectantes; Ctrl+Z restaura en ambos casos. UX hints visibles cuando faltan pre-condiciones.

**Decisiones clave de F2H32:**
- **Clip plane perpendicular al view del orto**: 2 clicks en orto definen una línea (no 3 clicks que serían coplanares en el view plane = degenerado). El plano se construye con `normal = cross(forwardAxis_orto, lineDir_world)` — perpendicular al forward (en el view plane) AND a la línea. Convención Hammer.
- **Clip = agregar plane a Brush::faces**: para Front, agregar `BrushFace { -clipPlane.normal, -clipPlane.distance }` (el interior queda del lado positivo del clipPlane original — convención de normales del motor: outward). Para Back, agregar el plano tal cual. `isBrushValid` filtra resultados degenerados (plano fuera del brush).
- **`BooleanOpCommand` reusado para clip y carve**: extendido el enum con `Clip` + skip de destroy/recreate cuando `bSnapshot.tag.empty()`. Sin command nuevo. Pasa para carve también (kind=Subtract con bSnapshot vacío porque los carvers no se destruyen).
- **Carve = subtract iterativo**: `fragments = [A]`; por cada carver B: `fragments = union de subtract(fragment, B)`. Si A queda completamente consumido → fragments vacío → solo destroy de A (sin spawn). Carvers preservados (Hammer).
- **Carve broadphase por AABB**: solo brushes cuyo AABB intersecta el del active entran al loop. Sin esto N² test booleano por click.
- **Sin keyboard shortcut para carve**: operación destructiva; obligatorio click explícito en el botón del toolbar para evitar accidentes (tecla `C` queda libre).
- **UX hints visibles via `setStatusMessage`**: clip + carve setean mensajes claros en el status bar cuando faltan pre-condiciones (sin selección, sin intersección, etc.). El log warn existente no era visible al dev mientras testeaba.
- **Sin schema bump**: spawn de fragments es state in-memory; el undo via `BooleanOpCommand` snapshot.

**Implementación (F2H32 Bloques A-D en 3 commits):**

- **Bloque A (commit `b1c9963`)**: plan en [`archive/plans/PLAN_HITO_F2H32.md`](archive/plans/PLAN_HITO_F2H32.md).
- **Bloques B+C unificados (commit `4c8d560`)**: clip + carve + UX hints + extensión a BooleanOpCommand. Partials nuevos: `EditorProjectActions_Clip.cpp` + `EditorProjectActions_Carve.cpp`.
- **Bloque D (este commit)**: docs + tag.

**Pendientes conocidos** (post-F2H32):
- **F2H33 — Organización + face polish** (próximo, último hito para cerrar el Hammer en su totalidad funcional): VisGroups (agrupar brushes con nombre + hide/show + persistencia, schema bump v13→v14) + texture alignment del Face Edit Sheet (Align/Fit/Justify L/R/T/B + Treat as one face).
- Validación full del Player con compiledMesh: deuda menor heredada de F2H26.
- **Hollow tool** (carve A con un brush "interior" para crear paredes huecas): scope incremental sobre carve. Diferido — no típico del flow básico de Hammer.
- **Multi-brush carve simultáneo** (selección de N A's en lugar de 1): la math de subtract lo soporta; UI sólo necesita iterar. Si emerge necesidad, agregar.
- **Clip tool en perspectiva 3D**: Hammer no lo tiene; gizmo manipulable para mover el plano libre = scope mucho mayor. Diferido.

**Próximo paso**: **F2H33 — Organización + face polish (VisGroups + texture alignment)**. Plan en `docs/PLAN_HITO_F2H33.md` cuando arranquemos.

### F2H31 (anterior, ya cerrado)

**🚀 F2H31 cerrado: Productivity selection + visual polish (marquee + group transform + snap-to-vertex + frustum + coords cursor).**
Tag: `v1.21.0-fase2-hito31`.
Verificado por dev: marquee detecta N entidades en cualquier orto y group transform las mueve juntas; snap-to-vertex hace el cursor pegarse al vertex objetivo; auto-close del pincel funciona clickeando vertex 1; frustum amarillo de la 3D cam visible en los 3 ortos; coords world del cursor siguen en vivo al mouse.

**Decisiones clave de F2H31:**
- **Tool selector mutually exclusive** en lugar de modifier: Select / CreateBlock / Pincel como radio (3 botones del toolbar). Default = Select estilo Hammer. Antes el block tool fireba siempre con drag en empty space; ahora hay que clickear "Bloque" para activarlo. Mismo modelo que Hammer Editor real.
- **Marquee hit-test "any corner inside"**: para cada entidad, proyectar los 8 corners del AABB world al ndc del orto; si CUALQUIER corner cae dentro del rectángulo, hit. Más liberal que "todos adentro" — alineado con Hammer (cualquier overlap selecciona).
- **Group transform reusa infra existente**: `OrthoDragSession::startPositions` ya iteraba `set.selected` desde F2H29 Bloque B; marquee solo cambia el contenido de `set.selected`, no el flow del drag-edit. Cero código nuevo de movimiento.
- **Snap-to-vertex toggle global** (no per-tool): al activar, AFECTA pincel + block tool corners + rubber band perspectivo simultáneamente. Threshold 0.02 ndc (~8 px screen) generoso para que el snap "agarre" temprano sin requerir precisión al pixel.
- **Broadphase por AABB world expandido**: el snap-to-vertex enumera vertices solo de brushes cercanos al cursor (AABB intersect con caja de threshold). Sin esto, escenas con cientos de brushes hacen N² por frame del drag.
- **Auto-close del pincel al clickear vertex 1**: pedido implícito del dev (*"cuando cierro todos los puntos y aprieto enter, no lo crea"* — porque clickeaba vertex 1 de vuelta generando degenerado). Fix: detectar click dentro de 1mm de `pointsWorld[0]` con `>= 3` puntos → auto-close. Coexiste con el cierre vía Enter (Blender-style).
- **Frustum dibujado a distancia "look-ahead" 4u en lugar del near-plane real**: el near-plane (~0.1m) sería invisible al render. El "rect de mira" a 4u es claramente visible y orientable; el dev percibe "qué mira la 3D cam" desde los 3 ortos.
- **Coords cursor solo cuando hovered**: el `m_liveCursor.hovered` ya lo reportaba el panel desde F2H30 Bloque C (para el pincel rubber band). Reutilizado para mostrar el label `(x, y, z)` en gris claro debajo del label de la vista.

**Implementación (F2H31 Bloques A-E en 3 commits):**

- **Bloque A (commit `c98cfad`)**: plan en [`archive/plans/PLAN_HITO_F2H31.md`](archive/plans/PLAN_HITO_F2H31.md).
- **Bloques B+C+D unificados (commit `3e0e414`)**: marquee + group transform + snap-to-vertex + auto-close pincel + frustum + coords cursor + helpers `brushAabbWorld` / `meshAabbWorld` expuestos.
- **Bloque E (este commit)**: docs + tag.

**Pendientes conocidos** (post-F2H31):
- **F2H32 — Geometry tools** (próximo, siguiente del plan "cerrar Hammer en su totalidad"): clip tool (3-click define plano que splittea brushes) + carve UI (boolean subtract con flow Hammer-style).
- **F2H33 — Organización + face polish**: VisGroups (agrupar brushes con nombre + hide/show + persistencia, schema bump v13→v14) + texture alignment del Face Edit Sheet.
- Validación full del Player con compiledMesh: deuda menor heredada de F2H26.
- Snap-to-vertex aplicado a vertex/edge edit: actualmente solo en pincel + block tool. Si emerge necesidad, extender al bloque 2.4e.
- Marquee Alt-modifier (remove): scope incremental, agregar si el dev lo pide.

**Próximo paso**: **F2H32 — Geometry tools (clip + carve)**. Plan en `docs/PLAN_HITO_F2H32.md` cuando arranquemos.

### F2H30 (anterior, ya cerrado)

**🚀 F2H30 cerrado: Polish del editor de mapas (vertex/edge edit + pincel poligonal + gizmo proporcional + W/E/R double-tap modal).**
Tag: `v1.20.0-fase2-hito30`.
Verificado por dev: vertex/edge edit funciona con snap absoluto al grid + rebasing al cierre, pincel poligonal crea prismas (4 vertices distintos → brush spawneado, dedupe corrigió "figura desaparece"), gizmo rotate sigue al AABB del brush, W/E/R con double-tap dispara modal Rotate/Scale con anillo amarillo visual, click confirma + Esc cancela.

**Cambio importante**: el esquema de atajos del modal pasó por **3 iteraciones** según feedback del dev:
- **Iter 1 (plan)**: G/R/S puros estilo Blender + W/E/R Maya conviven.
- **Iter 2**: dev pide "solo Blender, no Maya" → removidos W/E/R, R = modal Rotate, S = modal Scale.
- **Iter 3 (final, validado)**: dev pide hibrido double-tap → W = Translate gizmo, E single = Scale gizmo / E doble = modal Scale uniforme, R single = Rotate gizmo / R doble = modal Rotate libre. G y S removidos. Cuadrado central de uniform-scale gizmo eliminado (la S desapareció reemplazada por E doble).

**Decisiones clave de F2H30:**
- **Vertex/edge edit con snap WORLD-space (no local)**: el grid del workspace orto vive en world; snap en local quedaba desfasado para brushes con `tf.position != 0`. Solo se snappean los ejes que el dev MUEVE (`|deltaWorld[i]| > eps`), preservando coords no-grid-aligned en ejes intactos.
- **Rebasing al cierre del drag**: `newCentroidLocal = brush.localAabb.center()`, todos los planos se trasladan por `-newCentroid`, `tf.position += R * newCentroid`. Sin esto el gizmo quedaba en la posición vieja del brush. Mismo patrón que F2H12 boolean ops resolvió con `snapshotResultWorld`.
- **Dedupe en pincel**: dos clicks que snapean a la misma celda generaban polígono degenerado y `closePolygonDraw` cancelaba sin pista — el dev percibía "la figura desaparece". Fix: skipear el segundo click si está a < 1mm del último.
- **Gates anti-conflicto pincel/vertex-edit/block-tool**: activar pincel resetea sub-modo a Object + `activeFaceIndex = -1`; bloque 2.4e (vertex/edge edit) gateado con `!m_polyDraw.active`. Sin estos guards, un click del pincel en sub-modo Vertex disparaba edit accidental del brush selecto.
- **Toolbar lateral "Map Tools"** (columna derecha del workspace, ~10% ancho): pedido del dev *"hagamos uno superior"* mutado a *"prefiero columna lado derecho"*. Botones Objeto/Vertex/Edge/Cara/Pincel apilados verticalmente con highlight del activo + tooltips con shortcut.
- **Gizmo rotate radio = `0.6 * max(localAabb.size())` clamp >= 0.5m**: cubre BrushComponent (via `bc.brush.localAabb`) y MeshRendererComponent (via `MeshAsset::aabbMin/Max`). Antes era fijo 1m world — invisible dentro de brushes grandes.
- **W/E/R + double-tap (<0.4s) hibrido Maya/Blender**: scheme final que el dev validó. State `GizmoKeyTapState { lastKey, lastPressTime }` detecta double-tap. Single-tap toggea `GizmoMode`; double-tap arranca modal con misma tecla. Sin shortcuts cruzados — cada tecla tiene un único significado.
- **Modal con axis-lock X/Y/Z**: durante el modal, presionar X/Y/Z lockea/destrabe el constraint (alineado con Blender). Click izq confirma (push `MultiEditTransformCommand`); Esc cancela (revert al startValue snapshot).
- **Visual feedback del modal**: anillo amarillo (radio = distancia cursor-centro) + línea cursor→centro durante Rotate; línea sola durante Scale. Pedido del dev *"que aparezca ese circulo para rotar"*.

**Implementación (F2H30 Bloques A-E en 4 commits):**

- **Bloque A (commit `5674812` previo + plan en `docs/PLAN_HITO_F2H30.md`)**: plan archivado en [`archive/plans/PLAN_HITO_F2H30.md`](archive/plans/PLAN_HITO_F2H30.md).
- **Bloque B (commit `03ed19a`)**: vertex/edge edit con `Csg::pickVertex/pickEdge` + `EditBrushGeometryCommand` snapshot pre/post de planos + `tf.position` + sub-modos `Vertex` (tecla 1) / `Edge` (tecla 2) en `EditorSubMode`.
- **Bloque C (commit `4db0bd0`)**: pincel poligonal con `Csg::makePrismBrushFromPolygon` + validación CCW con auto-revert + dedupe + toolbar lateral `MapEditorTopBar` + gates anti-conflicto.
- **Bloque D (commit `67611ec`)**: gizmo proporcional + W/E/R double-tap modal con `ModalShortcutState` + `GizmoKeyTapState` + visual feedback. Implementación en `EditorOverlay_Modal.cpp` separado para no inflar `EditorApplication_Run.cpp`.
- **Bloque E (este commit)**: docs + tag.

**Pendientes conocidos** (post-F2H30):
- **MVP del editor estilo Hammer está completo**: 4-viewport + click-select + grid + block tool + drag-edit + vertex/edge edit + pincel poligonal + W/E/R modal. Próximo hito **TBD** — definir junto al dev qué prioriza.
- **Validación full del Player** con compiledMesh: deuda menor heredada de F2H26.
- **Multi-selección con marquee select** en orto (clásico Hammer rectángulo de selección sobre múltiples brushes): diferido — no bloquea el modeling flow.
- **Snap-to-vertex** (snap a vertices existentes del scene, no solo al grid): nice-to-have.
- **Brush poligonal cóncavo**: requiere refactor mayor del CSG core (asume convexos).
- Los demás items del backlog "Activos sin orden definido" siguen vivos: split fino de `SceneRenderer_Render.cpp`, iconos Toolbar, polish UX panels, node-graph Material Editor.

**Próximo paso**: **TBD — definir junto al dev**. El editor de mapas tiene su MVP funcional cubierto; queda alinear sobre qué priorizar (gameplay/scripting, polish UI/UX, optimización runtime, features nuevas del editor).

### F2H28 (anterior, ya cerrado)

**🚀 F2H28: Editor de mapas 4-viewport (MVP visual + select + grid).**
Tag: `v1.18.0-fase2-hito28`.

**Decisiones clave de F2H28:** workspace "Editor de mapas" (label castellano, no "Hammer") como 4to default + dockspace 2x2 (Top XZ / Viewport / Front XY / Side ZY) + 3 FBOs LDR + render orto wireframe (color flat celeste GMod / naranja selected) sobre fondo NEGRO (cambio dev sobre plan original gris) + grid 2D shader fullscreen con AA via `fwidth` (menor `(40,40,40)`, mayor naranja Valve `#F58220`) + refactor `pickEntity` → helper `pickEntityFromRay` para rayos paralelos del orto + snap step `m_hammerSnapStep` cycleable Ctrl++/Ctrl+- con label "Grid: Nu" arriba-derecha.

**F2H27 (F6 panel) fue descartado** durante implementación — redundante con Inspector; F6-real es scope grande propio.

### F2H25 + F2H26 (anteriores, ya cerrados)

**🚀 F2H25 + F2H26: Cull overlap parcial en CompileMap + Runtime-load de mesh compilada en MoodPlayer.**
Tags: `v1.16.0-fase2-hito25` (cull overlap + UI layout v2 stamp) + `v1.17.0-fase2-hito26` (schema v13 + Player path).

**Decisiones clave de F2H25:** algoritmo BSP-style polygon clipping con Sutherland-Hodgman extendido + 3 pre-tests críticos (cara entera afuera/adentro/coplanar) + stats `culledOverlapTriangles` + `splitFragments` + UI layout v2 stamp (`imgui_layout_v2.ini`).

**Decisiones clave de F2H26:** schema bump aditivo v12→v13 con `compiledMesh` opcional + layout PBR 11 floats interleaved + componente `CompiledMeshComponent` move-only + render path nuevo `compiledMeshPass` + flag `useCompiledMesh` en SceneLoader (Player=true, Editor=false).

### F2H24 (anterior, ya cerrado)

**🚀 F2H24 cerrado + Bloque C extendido: Reorganización interna del código.**
Tags: `v1.15.0-fase2-hito24` + `v1.15.1-fase2-hito24-bloque-c`.
Verificado automático: suite doctest **610/8359** verde después de cada Bloque B.X. Verificado por LOC: ningún partial supera 500 LOC; los 5 archivos CRÍTICOS originales (5784 LOC totales) reorganizados en 1 núcleo + N partials cada uno.

**Distribución LOC final** (núcleo + partials, 5 CRÍTICOS):

| Original | LOC original | Núcleo | Partials | Max partial |
|---|---:|---:|---:|---:|
| `InspectorPanel.cpp` | 1338 | 77 | 10 (+ Internal.h) | 208 |
| `EditorProjectActions.cpp` | 1272 | 106 | 6 | 329 |
| `DemoSpawners.cpp` | 1188 | 41 | 4 (+ Internal.h) | 399 |
| `PlayerApplication.cpp` | 1160 | 111 | 3 | 484 |
| `EditorApplication.cpp` | 826 | 173 | 2 | 435 |
| `SceneRenderer.cpp` | 776 | 242 | 1 (+_Render 590) | 590 |
| `MeshLoader.cpp` | 767 | 279 | 3 (+ Internal.h) | 213 |
| `EditorOverlay.cpp` | 745 | 310 | 1 (+ Internal.h) | 460 |
| `AssetManager.cpp` | 743 | 256 | 5 | 276 |

**Cambio importante**: F2H24 cumple el pedido explícito del dev al cerrar F2H23 (*"creo que hay archivos demasiado grandes que te cuesta arreglar"*) + extendido tras pedido posterior *"esa deuda chica podes hacerla ahora"* para cubrir los 4 ALTO 700-780 LOC. Refactor puramente estructural — ningún cambio funcional, ningún cambio de API pública. El editor y el player arrancan idénticos al usuario final. Los 4 archivos ALTO (700-780 LOC: `SceneRenderer`, `MeshLoader`, `EditorOverlay`, `AssetManager`) quedan en `PENDIENTES.md` como deuda chica.

**Decisiones clave**:
- **Split por dominio funcional con archivos parciales de la misma clase**: cada partial implementa métodos privados de la clase declarada en el header. Patrón ya usado pre-F2H24 (`EditorApplication.cpp` ya tenía `EditorProjectActions.cpp` + `DemoSpawners.cpp` + `EditorOverlay.cpp` + `EditorPlayMode.cpp` + `EditorRenderPass.cpp` + `EditorScene.cpp` desde Hito 16); F2H24 lo extiende a los 5 CRÍTICOS de Fase 2.
- **API pública intacta**: solo `InspectorPanel.h` recibió 13 métodos privados nuevos (`renderTagSection(Entity)`, `renderTransformSection(Entity)`, etc.) para que el dispatch del `onImGuiRender` quede legible — esto es API privada, no rompe nada externo.
- **Helpers compartidos via header interno** (`Foo_Internal.h`): namespace `Mood::detail` con `inline` functions y templates para que cada TU tenga acceso. Patrón aplicado a InspectorPanel (`pushEditIfDone` template + `helpMarker` + `isDragActiveOfType`) y DemoSpawners (`WorldYBounds` + `rotatedAabbWorldY`).
- **Cap soft 500 / hard 800 reafirmado**: ningún partial supera 500 LOC excepto `_Frame` y `_Run` que rondan 435-484 (loops monolíticos sin sub-secciones obvias para particionar más). Aceptable.
- **Validación incremental por Bloque B.X**: build + suite verde después de cada split antes de pasar al siguiente. Permite detectar regresiones rápido. La suite 610/8359 quedó idéntica antes y después de cada bloque (refactor sin cambios funcionales).
- **Bloque C (split de ALTO 700-780 LOC) skipped por presupuesto**: 4 archivos movidos a `PENDIENTES.md` como deuda chica. F2H24 entregó los 5 CRÍTICOS — los ALTO son menos urgentes (todos < 800 LOC = bajo el hard cap).
- **Errores resueltos durante el trabajo** (no requieren mención del dev pero quedan registrados): `glm/gtx/compatibility.hpp` removido del `InspectorPanel_Brush.cpp` (extensión experimental no necesaria); `APIENTRY` reemplazado por `GLAD_API_PTR` en `EditorApplication_Init.cpp` (no depende de inclusión transitiva de `<windows.h>`); `FrameStats` requiere include de `IRenderer.h` (definición completa) en `EditorApplication_Run.cpp` además de `SceneRenderer.h` (forward-decl).

**Implementación (Bloques A-E):**

- **Bloque A — Plan F2H24 + auditoría**: subagente recorrió `src/` con `wc -l`. Confirmé números reales con conteo directo (los del subagente eran ligeramente bajos por path antiguo). Plan documentado en `docs/archive/plans/PLAN_HITO_F2H24.md`.
- **Bloque B.1 — InspectorPanel.cpp**: 1338 LOC → núcleo 77 + 10 partials + Internal.h. Tag intermedio: `1cce00a`.
- **Bloque B.2 — EditorProjectActions.cpp**: 1272 LOC → núcleo 106 + 6 partials. Tag intermedio: `05e9afe`.
- **Bloque B.3 — DemoSpawners.cpp**: 1188 LOC → núcleo 41 + 4 partials + Internal.h. Tag intermedio: `dba4aa7`.
- **Bloque B.4 — PlayerApplication.cpp**: 1160 LOC → núcleo 111 + 3 partials. Tag intermedio: `01a80ae`.
- **Bloque B.5 — EditorApplication.cpp**: 826 LOC → núcleo 173 + 2 partials. Tag intermedio: `9abc38b`.
- **Bloque C extendido — split de ALTO 700-780 LOC** (atacado tras pedido del dev): C.1 AssetManager 743→256+5 partials, C.2 EditorOverlay 745→310+_Gizmo 460+Internal.h, C.3 MeshLoader 767→279+_Skeleton 184+_Geometry 213+_Animation 68+Internal.h, C.4 SceneRenderer 776→242+_Render 590 (deuda chica aceptada: frame loop monolítico bajo hard cap). Tag `v1.15.1-fase2-hito24-bloque-c` tras el cierre.
- **Bloque D — validación**: suite 610/8359 verde tras cada Bloque B.X; build OK Debug; cap soft 500 respetado en todos los partials nuevos.
- **Bloque E — cierre**: este documento + HITOS + DECISIONS + PENDIENTES + tag `v1.15.0-fase2-hito24`.

**Pendientes conocidos** (memoria + `PENDIENTES.md`):
- **F2H25 — 4-viewport Hammer-style layout** (próximo, heredado de F2H22→F2H23→F2H24 candidato): workspace nuevo "Hammer" con dockspace en 4 cuadrantes.
- **Split de archivos ALTO** — **CERRADO en Bloque C extendido tag `v1.15.1`**. Quedan documentadas las 4 splits + la deuda chica residual en `SceneRenderer_Render.cpp` (590 LOC, sobre soft 500 pero bajo hard 800 — frame loop monolítico).
- **Iconos image-based del Toolbar** (deuda explícita F2H22): FontAwesome / IcoMoon. Hito chico futuro.
- **CompoundCommand para batch undo** (deuda parcial cerrada en F2H23 con MultiEditTransformCommand).
- **Pase de polish UX continuo (siguiente ronda)**.
- **Node-graph del Material Editor** (deuda F2H21).
- **Runtime-load mesh compilada en MoodPlayer** (deuda F2H20).
- **Cull overlap parcial** (deuda F2H20).

**Próximo paso**: **F2H25 — 4-viewport Hammer-style layout**. Workspace "Hammer" con 4 cuadrantes: 1 perspectiva 3D + 3 ortográficas top/front/side en wireframe + drag-edit entre vistas con grid snap. Tradeoff: ~4× costo CPU del frame — mitigar reusando shadow pass / light grid compartido. Reusa la infra de workspaces de F2H7+F2H22 con un `buildHammerWorkspace` nuevo en `Dockspace.cpp`.


### Hitos anteriores

Para los detalles de hitos cerrados previos a F2H24 (F2H23, F2H22, ..., F2H1, Fase 1, Hito 0-42), ver [`archive/ESTADO_HISTORICO.md`](archive/ESTADO_HISTORICO.md). Split aplicado en F2H26 cierre — este documento mantiene solo el current state + último hito cerrado.

---

## 2. Entorno de build — lo que realmente funciona

### Toolchain real instalado en la máquina del dev

- **Visual Studio 2022 Community** → tiene MSVC 14.44 + SDK Windows 11 + CMake 3.31 bundleado. **Este es el que usamos.**
- **Visual Studio 2026 Community** → instalado **sin** workload de C++. No tiene compilador. Ignorar hasta que el dev lo complete o desinstale.

La ruta clave es:

```
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat
```

### Cargar entorno MSVC desde PowerShell (el comando que funciona)

```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 && <tu_comando_aqui>'
```

Desde un **Developer Command Prompt for VS 2022** del menú inicio, `cl` y `cmake` funcionan directamente sin esto.

### Versiones verificadas

- `cl` → `Microsoft (R) C/C++ Optimizing Compiler Version 19.44.35226 for x64`
- `cmake --version` → `3.31.6-msvc6`
- `git --version` → `2.49.0.windows.1`

### Generador CMake correcto

`Visual Studio 17 2022` (ya está en `CMakePresets.json`, no tocar).

---

## 3. Comandos que ya corrieron exitosamente

Desde la raíz del repo, con entorno MSVC cargado:

```bash
# Configurar (descarga deps la primera vez, ~5 min)
cmake --preset windows-msvc-debug

# Compilar
cmake --build build/debug --config Debug
```

Resultado: `build/debug/Debug/MoodEditor.exe` (3.9 MB) + `SDL2d.dll` (4.3 MB) copiada automáticamente al mismo directorio.

Para ejecutar:

```
.\build\debug\Debug\MoodEditor.exe
```

---

## 4. Qué tiene que hacer el próximo agente

### Tarea inmediata: TBD — definir junto al dev

F2H33 está cerrado (tag `v1.23.0-fase2-hito33`). **Hammer Editor cerrado funcional al 100%.** 31/44 hitos de Fase 2.

Opciones a considerar para el próximo hito (preguntar al dev al arrancar la sesión):

1. **Sub-fase 2.5 gameplay** — diálogos, quests, inventario. Es el siguiente bloque del PLAN_FASE2 si seguimos el orden.
2. **Hammer-style visual polish** (sub-bloque chico): tint del wireframe por color del VisGroup (~30 LOC, alto valor visual), color por tipo de entity (luz=amarillo, audio=naranja), labels arriba de point entities, mejoras al face picking UX (descubierto durante F2H33 — el dev mencionó *"se puede pulir a futuro"*). Diferidos como paquete.
3. **Otra sub-fase del PLAN_FASE2** — optimización, runtime, polish UI general.
4. **Multi-face material drop** — extender `EditBrushFaceMaterialCommand` a multi-cara para que dropear textura sobre N caras seleccionadas las afecte a todas (hoy aplica solo al active).

### Flujo recomendado en esta sesión

1. Preguntar al dev qué hito atacar (TBD entre las 4 opciones de arriba).
2. Crear `docs/PLAN_HITO_F2HN.md` con bloques A-E concretos.
3. Trabajar bloque por bloque marcando en el plan al cerrar cada uno.
4. Actualizar `docs/DECISIONS.md` cuando aparezca una decisión no trivial.
5. Al final: commits atómicos en español, merge a main, tag `v1.X.0-fase2-hitoN`, actualizar este documento + `docs/HITOS.md`, archive del plan.

---

## 5. Gotchas conocidos — cosas que ya se aprendieron por las malas

1. **VS 2026 Community se instaló sin el workload de C++.** El `Developer Command Prompt for VS 2026` abre pero `cl` y `cmake` no existen. No depender de VS 2026 hasta que el dev agregue el workload o lo desinstale. Usar siempre VS 2022.
2. **El `Developer Command Prompt` normal de Windows** (sin MSVC) no tiene `cl` en PATH. Si un comando falla con "cl no se reconoce", revisar que el prompt diga "Visual Studio 2022" en el banner.
3. **SDL2 en debug se llama `SDL2d.dll`**, no `SDL2.dll`. El `add_custom_command` POST_BUILD del CMakeLists copia la variante correcta según `$<TARGET_FILE:SDL2::SDL2>`.
4. ~~**GLAD no está incluido en Hito 1.**~~ Resuelto en Hito 2: `EditorApplication.cpp` usa `<glad/gl.h>` y llama `gladLoaderLoadGL()` tras crear el contexto. GLAD y el loader interno de ImGui conviven sin conflictos porque glad2 prefija sus símbolos con `glad_`.
8. **GLAD v2 usa naming distinto de v1.** Header: `<glad/gl.h>` (no `<glad/glad.h>`). Source: `gl.c` (no `glad.c`). Loader: `gladLoaderLoadGL()` / `gladLoaderUnloadGL()`.
9. **Dos máquinas de desarrollo.** Notebook con Intel Iris Xe (GL 4.5 OK, menos performance) y desktop con AMD Ryzen 5 5600G + NVIDIA GTX 1660. En el desktop, forzar NVIDIA para `MoodEditor.exe` desde el Panel de Control NVIDIA (sino Windows puede elegir la iGPU AMD). `imgui.ini` y `build/` NO viajan por git: cada máquina genera lo suyo.
10. **Shaders se buscan con path relativo `shaders/default.vert`.** Funciona desde la raíz del repo (VS_DEBUGGER_WORKING_DIRECTORY) y desde el directorio del exe (post-build `copy_directory shaders/`). Si agregás shaders nuevos, la copia es automática.
5. **El primer `cmake --preset` tarda ~5 minutos** porque baja y compila subdeps de SDL2 + ImGui + spdlog + GLM. Ese tiempo solo ocurre la primera vez; después el build incremental es segundos.
6. **spdlog busca pthread en Windows** y sale un warning de que no lo encuentra. Es esperado y benigno en Windows.
7. **GLM tira warnings de `cmake_minimum_required` deprecated** porque su CMakeLists usa sintaxis vieja. Ignorable; no es nuestro código.

---

## 6. Recordatorios de filosofía (sección 9 del doc técnico)

- Ship something: no romper el build entre commits.
- No implementar hitos futuros "por adelantar trabajo".
- No añadir dependencias que no estén en la sección 4 del doc sin consultar.
- Comentarios en español.
- Convención de commits: `tipo(scope): mensaje` en español.
- Preguntar al dev antes de asumir ante ambigüedades.

---

## 7. Archivos clave que tocar cuando

| Para... | Tocar |
|---|---|
| Añadir una dependencia | `CMakeLists.txt` (bloque CPM) |
| Cambiar flags de compilador | `cmake/CompilerWarnings.cmake` |
| Añadir un panel al editor | `src/editor/panels/*` + `EditorUI.(h|cpp)` |
| Registrar una decisión arquitectónica | `docs/DECISIONS.md` |
| Marcar progreso de hito | `docs/HITOS.md` |
| Cambiar log level | `src/core/Log.cpp` |

---

## 8. Al arrancar una sesión nueva

1. Leer este archivo entero.
2. Leer `docs/PLAN_HITO<N>.md` del hito en curso para ver qué tareas quedan.
3. Leer `MOODENGINE_CONTEXTO_TECNICO.md` si es la primera vez.
4. `git status` + `git log --oneline -10` + `git tag --sort=-v:refname | head -5` para ver tags y cambios recientes.
5. Preguntar al desarrollador: "¿seguimos con el Hito en curso o pasó algo nuevo?"
6. Actuar según la respuesta.
