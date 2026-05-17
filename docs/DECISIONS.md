# Log de decisiones técnicas — Fase 2

Registro cronológico de decisiones arquitectónicas no triviales tomadas
durante la **Fase 2** (F2H1 en adelante). Formato por entrada: contexto,
decisión, razones, alternativas descartadas, condiciones de revisión.

> **Decisiones de Fase 1 (Hito 0 → Hito 42)** archivadas en
> [`archive/DECISIONS_Fase1.md`](archive/DECISIONS_Fase1.md). Split
> aplicado en F2H26 cierre (2026-05-08) cuando este documento superó
> las 4000 líneas.

---

## 2026-05-16: F2H59 cierre — Primitivas clásicas + reorg UX (modal Crear entidad + toolbar flotante)

**Contexto:** pivot temporal de Sub-fase 2.6 (render polish) a UX del editor entre F2H58 (color grading) y F2H6X (siguiente render polish), motivado por el dev que durante el tour visual de F2H58 detectó que estaba usando un Box brush aplastado como suelo y pidió la primitiva Plano + primitivas clásicas adicionales. La conversación escaló a UX reorg general. Tag `v1.46.0-fase2-hito59`. Hito mediano (Bloques A-G).

**Decisión clave 1 — Modal "+ Crear Entidad" como punto único de entrada para crear geometría.** Pre-F2H59 las primitivas vivían en 3 lugares: menú top-level "Brush > Añadir" (11 items), Toolbar lateral (Box + Cylinder), y modal "+ Crear Entidad" (que solo manejaba meshes importados). F2H59 consolida los 3 en el modal con TabBar (Meshes del proyecto / Primitivas).

- **Razón:** workflow Hammer/SFM consistente. 1 click en panel Escena → modal → click en primitiva. El dev no tiene que aprender 3 caminos distintos para crear geometría. Convención que ya empezamos a usar en F2H57 ("+ Crear Entidad" como punto único de entrada SFM-style).
- **Alternativa descartada:** mantener las primitivas en el menú Brush para "Hammer-purists" + el Toolbar como "atajos rápidos". Rechazada porque genera tres puntos de mantenimiento UI sincronizados. Si emerge demanda futura de atajos rápidos a primitivas comunes, agregamos un atajo de teclado o configurable.
- **Trade-off documentado:** el dev pierde 1 click vs el menú directo (modal → tab → click vs menú → click). Aceptable porque el modal también ofrece "Importar..." + "Empty" en el mismo flow.

**Decisión clave 2 — Toolbar como overlay flotante sobre el viewport, estilo Blender / Unity / Unreal moderno / Godot 4.** El panel Toolbar pre-F2H59 era una ventana dockable lateral con Box/Cylinder + gizmo modes + Face toggle. F2H59 lo migra a una sub-window ImGui flotante en la esquina superior-izquierda de la imagen del viewport (background transparente alpha 0 + border 0 + NoBackground), 4 botones icon-only: Mover / Rotar / Escala / "F".

- **Razón:** convención industria moderna. Hammer 2004 usa toolbar lateral fijo (paradigma MFC de Windows), pero Blender / Unity 2024 / Unreal 5 / Godot 4 usan overlay flotante porque libera espacio dockable para paneles más útiles (Hierarchy / Inspector / Asset Browser). El espacio horizontal del editor es escaso.
- **Limitación documentada:** el overlay actual NO es movible, NO es context-aware del workspace. F2H60 candidate evalúa overlay context-aware (botones específicos según workspace: narrative vs map_editor vs gameplay) y posiblemente movible con SetWindowPos.
- **Toolbar como panel queda dead-code linkeable** — no en `m_panels` pero el struct `m_toolbar` sigue declarado. Cleanup completo (eliminar `Toolbar.h/.cpp`) diferido por minimal-risk; si emerge demanda futura de reactivarlo como panel, está intacto.

**Decisión clave 3 — Background del overlay totalmente transparente** (alpha 0 + border 0 + flag `NoBackground`). Pedido explícito del dev: *"le podemos dejar el fondo transparente para que solo sean botones flotantes?"*.

- **Razón:** estética Blender — los botones de tools "flotan" sobre el viewport sin marco visual que los separe. Mejor integración visual con el render 3D detrás.
- **Bug pre-detectado:** con solo `SetNextWindowBgAlpha(0.0f)` quedaban líneas finas del frame de la sub-window. Fix: `PushStyleVar(WindowBorderSize, 0.0f)` + flag `NoBackground` explícito redundante con el alpha pero protege si el alpha falla por theme. Pedido del dev: *"se sigue viendo unas lineas finas"*.

**Decisión clave 4 — "F" como label del botón Face toggle, no `ICON_FA_VECTOR_SQUARE`.** Pedido explícito del dev: *"creo que deberia ser F de faces, ya que solo se usa en la edicion de texturas"*.

- **Razón:** la letra F como label es semánticamente clara (Face = F). El icono cuadrado FA es genérico y se confundía con Box / Quad. En el overlay flotante con 4 botones chicos (36×36 px), claridad > consistencia con icon set FA.
- **Aplicabilidad:** homogeneización general del sistema de iconos del editor agendada como follow-up F2H60+. Por ahora el botón F es la excepción minimal.

**Decisión clave 5 — Footer del modal con vocabulario universal de engines** ("Importar..." + "Empty"). Pre-F2H59 los labels eran "Importar desde archivo..." + "Crear vacía (placeholder)" — verbosos y específicos del workflow. F2H59 los acorta + adopta términos universales.

- **Razón:** "Empty" es vocabulario universal — Unity Empty GameObject, Unreal Empty Actor, Godot Empty Node. El dev que viene de cualquier engine moderno reconoce inmediatamente el botón. "Importar..." con elipsis es convención UI estándar para "abre file picker" (Unity Import / Unreal Import / Godot Import). Pedido explícito del dev: *"al termino que me referia era a Empty"*.
- **Trade-off i18n:** "Empty" queda en inglés también en `es.json` (no se traduce a "Vacío"). Convención: términos universales del vocabulario engine quedan en inglés (matchea expectativa del dev que viene de tutoriales en inglés). Si emerge demanda de full translation, evaluar.

**Decisión clave 6 — Toro skipped en v1.** Las primitivas pedidas eran Plano / Quad / Cono / Cápsula / Toro. Toro NO entró.

- **Razón técnica:** un toro NO es geométricamente convexo (tiene un agujero en el medio), y el sistema CSG de MoodEngine asume brushes convexos para todas las operaciones (boolean, picking, clipping). Implementar Toro como brush único es imposible sin refactor del CSG core.
- **Workaround documentado:** spawn 2 cilindros (outer + inner) + Brush > Operaciones Booleanas > Resta del inner. 3 clicks pero da un anillo CSG editable.
- **Alternativa diferida:** implementar Toro como N segmentos curvos auto-spawneados + group. Scope mayor, requiere primitive groups (no implementado). F2H6X+ evalúa cuando se vea modificadores Blender-style para booleans.

**Decisión clave 7 — Cápsula = Sphere dodecaédrica estirada Y 2×.** No es cápsula técnica (cilindro + 2 hemisferios con paredes laterales rectas) sino elipsoide alargado.

- **Razón:** trade-off de simplicidad. La cápsula como `makeSphereBrush(scale Y=2)` es 0 código nuevo. La cápsula técnica requiere nueva función `makeCapsuleBrush()` con geometría híbrida (cilindro central + 2 calotas hemisféricas preservando convexidad). v1 cubre el caso común "personaje proxy / pildora / pilar redondeado".
- **Revisión:** si emerge demanda de cápsula con paredes laterales rectas (gameplay con player controller cápsula), follow-up implementa `makeCapsuleBrush()` real.

**Decisión clave 8 — Brushes son geometría estática del mapa, NO objetos físicos dinámicos. Source/Hammer paradigm explícito agendado a F2H60.** Durante el tour el dev creó un cubo brush, le agregó RigidBody Dynamic, dio Play y no cayó. Causa raíz: el mesh cache del brush se construye con `worldMatrix` incrustado en los vértices y solo se rebuilda si `bc.dirty=true` — al cambiar `t.position` el cache queda desactualizado. El body de física Jolt SÍ cae internamente, pero el visual no se mueve.

- **Decisión arquitectónica (F2H60):** adoptar explícitamente la separación que Source/Hammer 2004 hizo con Half-Life 2 (Havok Physics). **Brushes** = estructura estática del mapa (paredes, pisos, columnas) — nunca dinámicos. **Meshes (props)** = objetos del juego (cajas, barriles, NPCs) — pueden ser Static / Kinematic / Dynamic libremente. Source separó porque BSP/CSG no se lleva bien con simulación rígida — la geometría de un brush se materializa al compile-time del mapa, no a runtime per-frame.
- **Implicancias en MoodEngine F2H60:** rename UI "Brush" → "Estructura" / "Geometría del nivel" (el término técnico "brush" queda solo en código + docs). Tab "Primitivas" del modal Crear Entidad se divide en 2 sub-secciones: **Estructura** (los 11 brushes actuales) + **Objetos** (5-6 meshes procedurales: Cubo, Esfera, Cilindro, Cápsula, Cono — generados en runtime con vertices/normales/UVs puros, soportan física dinámica nativamente). Warning en Inspector si el dev pone RigidBody Dynamic sobre un Brush — slider Type Dynamic queda disabled con tooltip.
- **Cita del dev:** *"que es mejor usar meshes o brushes? para nuestro editor me refiero, porque me gusta lo de los brush, por ahi confunde el termino o su definicion pero si es util"*. Tras explicación del Source paradigm: *"sigamos esa direccion"*.
- **Alternativa descartada:** refactor del render de brushes para que rebuilden el mesh cache cada frame si tienen RigidBody Dynamic (1 línea de código: `bc.dirty = (rb.type == Dynamic);`). Rechazada porque mezcla los paradigmas — el dev podría poner RigidBody Dynamic sobre brushes complejos (boolean trees, polígonos arbitrarios) y el rebuild per-frame del mesh cache impactaría performance. La separación Source es más limpia conceptualmente.

---

## 2026-05-16: F2H58 cierre — Color grading LUT-based + consolidación Environment + UX polish

**Contexto:** tercer hito de Sub-fase 2.6 (Render polish): bloom (F2H55) → SSAO (F2H56) → **color grading (F2H58)** → god rays / shadow polish (F2H6X+). Tag `v1.45.0-fase2-hito58`. Hito mediano que creció de 7 bloques planeados (A-G) a 10 (A-J + fix lateral) por feedback iterativo del dev durante el tour visual.

**Decisión clave 1 — Pre-tonemap LUT (convención Unity URP) sobre post-tonemap (Unreal).** El plan original (PLAN_HITO_F2H58 línea 171) ya documentaba las dos escuelas. v1 va con pre-tonemap clamp `[0,1]` por simplicidad del color space: la LUT opera en el rango que cualquier herramienta de cine (Photoshop, GIMP, DaVinci) le pasa al colorista por default.

- **Razón:** Unity URP usa el mismo path y es el flow más documentado entre los motores open-source que portamos shaders (Filament, Godot 4). Cambiar a post-tonemap requeriría re-arquitectar la posición del pase en el pipeline y posiblemente convertir las LUTs existentes.
- **Alternativa diferida:** si emerge feedback de coloristas de que las LUTs de DaVinci Resolve (que asumen input post-tonemap) no quedan bien aplicadas pre-tonemap, evaluar switch. **Mitigación:** los 4 LUTs sample que shippeamos (`identity`, `cinema_warm`, `matrix_cool`, `noir_high_contrast`) están generados con `tools/gen_luts.py` operando sobre la identity table — son coherentes con el path pre-tonemap del shader y se pueden regenerar trivialmente si se cambia el approach.

**Decisión clave 2 — Color grading default OFF, a diferencia de bloom/SSAO.** Bloom y SSAO en F2H55/F2H56 quedaron default ON con valores razonables porque sin ellos la escena se ve "plana" — ambos efectos aportan grounding visual incluso con intensidades bajas.

- **Razón:** color grading sin LUT (con la identidad) es no-op. Con una LUT cargada el efecto cambia el look entero de la escena — eso es una decisión de art direction que el motor no debe imponer. Engine-grade significa proveer defaults sensatos, no opiniones de director artístico. El dev del juego elige el look conscientemente.
- **Trade-off de discoverability:** el dev podría no enterarse de que existe color grading si nunca prende el checkbox. **Mitigación:** los presets en el dropdown (Cálido / Frío / Noir) son visualmente obvios y nombrados con la mood asociada — eso da exposure UX al feature sin imponer el efecto por default.

**Decisión clave 3 — Path field como mecánica interna + preset dropdown como UX.** El dev pidió explícitamente: *"lo de LUT no lo veo viable, a menos que tenga pressets incluidos"*. Pre-feedback la UI mostraba un `InputText` con el path lógico (ej. `luts/cinema_warm.png`) — UX técnica más que de director artístico.

- **Razón:** el dev del juego piensa en términos de "look" ("quiero un atardecer cálido", "quiero un noir") no de paths de archivo. Pre-feedback el flow obligaba a explorar `assets/luts/` con el file picker para descubrir qué hay; post-Bloque H el flow es 1 click en el dropdown.
- **El path sigue siendo la mecánica de persistencia** — el `.moodmap` guarda `colorGradingLutPath: "luts/cinema_warm.png"` y los devs pueden inspeccionar/versionar/editar manualmente. El preset dropdown es resolución bidireccional: lee el path del componente y lo matchea contra los built-ins; si no matchea, muestra "Personalizado: \<filename\>". Best of both worlds.
- **Alternativa descartada:** ocultar el path completamente y usar IDs internos del preset (`PresetId::CinemaWarm`). Rechazada porque ata el formato del .moodmap a un enum que rompería si en el futuro removemos un built-in. El path tiene la ventaja de ser self-documenting y forward-compatible.

**Decisión clave 4 — Reset per-section sin undo en v1.** Los 5 botones `⟲ Restablecer` (uno por sección Sky+Fog, Tonemap, Bloom, SSAO, Color Grading) asignan los campos desde una instancia `kEnvDefaults` default-constructed.

- **Razón:** snapshot multi-campo undoable requeriría un `ResetSectionCommand` custom que serializa el `EnvironmentComponent` entero pre-reset y deserializa en undo. Es complejidad significativa para una operación claramente intencional (el dev clickea explícitamente).
- **Trade-off documentado:** si el dev resetea por error, no hay Ctrl+Z. **Mitigación:** los edits individuales sobre los sliders post-reset SÍ son undoable (mismo `pushEditIfDone` pre-existente). En el peor caso el dev reescribe los valores a mano — fricción aceptable para no inflar v1.
- **Revisión:** si emergen reportes de "reseteé y perdí mi config", priorizar `ResetSectionCommand` como sub-hito puntual.

**Decisión clave 5 — Post-Process wrapper con `ImGui::Indent/Unindent` sobre TreeNode anidado nativo.** Bloque I reorganiza el Inspector a 2 headers top-level (Sky+Fog y Post-procesado) con los 4 sub-pases (Tonemap / Bloom / SSAO / Color Grading) dentro de Post-procesado.

- **Razón:** ImGui no anida `CollapsingHeader` nativamente con un solo chevron — usar `TreeNode` anidado da un look-and-feel diferente (chevron doble) que no matchea la convención visual del resto del Inspector. `CollapsingHeader` plano + `Indent/Unindent` da el efecto visual de subordinación sin alterar el control de colapso.
- **Patrón Unity Volume / Unreal PPV:** ambos usan algún tipo de indent visual o group headers — F2H58 elige la opción más simple compatible con el resto del panel.
- **Aplicabilidad a otros paneles:** si emerge la necesidad de jerarquía visual similar en otros sub-paneles del Inspector (Mesh con submesh details, Animation con timeline groups), el patrón `Indent/Unindent` queda como template reusable.

**Decisión clave 6 — IDs únicos por sufijo `##envreset_<id>` para los 5 botones reset.** Bug descubierto al primer tour: los 5 botones con mismo label visible "Restablecer" colisionaban en el ImGui ID interno → solo uno respondía al click.

- **Razón:** convención ImGui — labels visibles iguales necesitan IDs distintos. Las opciones son: (a) labels visibles diferentes ("Restablecer Sky+Fog" / "Restablecer Bloom" — verboso), (b) sufijos `##id` que ImGui usa internamente sin mostrar (mantiene texto visible limpio), (c) `PushID/PopID` por sección (más invasivo). Elegimos (b) por mínima fricción.
- **Generalización:** cualquier helper que pinte widgets con label visible idéntico debe tomar un `idSuffix` parameter — patrón aplicable a futuros helpers del Inspector.

**Decisión clave 7 — Fix MenuBar pre-existente incluido en F2H58 (bug de F2H57).** Durante el tour Bloque F el editor crashea con assert ImGui `EndMenuBar`. Root cause: F2H57 Bloque E borró un `EndMenu()` del menu Help junto con el submenu Demos eliminado. El bug NO se manifestó en F2H57 porque el test de ese hito no hizo click sobre Help.

- **Decisión:** fix de una línea (`ImGui::EndMenu();` en `MenuBar.cpp:256`) incluido en F2H58 en vez de hito separado. **Razón:** sin el fix, F2H58 no es testeable visualmente (el crash bloquea el tour). Hito de fix-only para una línea es overhead burocrático. **Mitigación:** commit separado con prefijo `fix(F2H57 followup):` para que git blame del MenuBar atribuya correctamente el origen del bug y del fix.
- **Aplicabilidad futura:** patrón de "fix lateral de hito anterior incluido en hito siguiente con commit separado" queda como precedente — preferible a inflar el hito siguiente con tag separado para una línea de fix.

---

## 2026-05-15: F2H57 cierre — Workflow Crear+Convertir entidad estilo Hammer/SFM

**Contexto:** pivot temporal de Sub-fase 2.6 (render polish) a UX del editor entre F2H56 y F2H58, motivado por 3 bugs UX detectados durante el tour visual de F2H56. Tag `v1.44.0-fase2-hito57`. Dev validó: *"lo demas anda perfecto, me gusta como esta"* tras el followup del modal SFM + welcome centering.

**Decisión clave 1 — Workflow SFM-style sobre Hammer puro.** El plan original era abrir directo el file picker del SO (convención Hammer Editor). El dev rechazó esa UX cuando vio la primera versión: *"creo que el crear entidad deberia darme la opcion de usar un mesh que este dentro del sistema, por ejemplo el hammer usa el modelo base de freeman… te cuesta leer lo que hay internamente"*. Pivot a Source Film Maker: el modal muestra primero la lista de meshes ya importados al proyecto, con botón secundario para importar uno nuevo desde el SO.

- **Razón:** el caso común del dev no es "tengo un FBX nuevo en mi escritorio", es "tengo 10 FBX ya en mi proyecto, quiero reusarlos". SFM optimiza ese flujo. El file picker del SO sigue accesible pero deja de ser el path por default.
- **Alternativa descartada:** popup con 3 opciones (Vacía / Desde modelo en el proyecto / Desde archivo del SO) antes de abrir el modal. El dev pidió eliminarla: *"que directamente al dar click en crear entidad, ya habra el modal interno"*. La razón es UX: 1 click directo al modal es menos fricción que click → popup → click → modal.
- **Final UX:** click en `+ Crear Entidad` → modal SFM con lista de meshes + dos botones de acción al pie ("Importar desde archivo..." + "Crear vacía (placeholder)") + "Cerrar". Punto único de entrada.

**Decisión clave 2 — Kits del modal Convertir son aditivos no-destructivos.** Convertir una entidad agrega los componentes del kit sin remover los existentes. Si la entidad ya tiene `DialogComponent`, el botón "NPC con diálogo" queda disabled.

- **Razón:** modo destructivo con confirm ("te voy a sacar X / Y / Z componentes — ¿confirmás?") es más predecible pero también más annoying. Aditivo permite stack: el dev puede aplicar "Item pickeable" + "Luz puntual" + "NPC con diálogo" en la misma entidad sin cerrar el modal entre clicks.
- **Trade-off de undo:** el paso convert NO es undoable en v1. Snapshot pre/post de componentes requeriría serializar la entidad completa (vía `EntitySerializer::serialize`) antes y restaurar el JSON en undo. Diferido por scope. **Mitigación:** edits individuales post-convert SÍ son undoable via `InspectorEditCommand` F2H32, así que sacar manualmente un componente del kit que aplicaste por error sigue siendo trivial.
- **Revisión:** si emergen reportes de "apliqué un kit por error y tuve que sacar 3 componentes a mano", priorizar snapshot undoable como sub-hito puntual.

**Decisión clave 3 — Demos removal minimal-risk.** Eliminamos solo las entries del menú `Ayuda > Demos` en `MenuBar.cpp` (~80 líneas). Los `DemoSpawners_*.cpp` quedan como dead code en `CMakeLists.txt` para no romper helpers compartidos como `ensureDemoIntroDialogExists`.

- **Razón:** el cleanup completo (borrar los .cpp + extraer helpers compartidos a sitio nuevo) infla la diff con cambios que pueden romper algo lateral. Engine-grade prioriza minimizar regresión por hito. El dead code no tiene cost runtime — solo compila y queda sin caller.
- **Alternativa diferida:** sub-hito de cleanup completo de los DemoSpawners + extracción de los helpers a `EditorHelpers` o equivalente, una vez confirmado que ningún consumer interno los necesita.
- **Revisión:** flagueado como tech-debt en `MoodEngine`. Si emerge demanda (alguien intenta agregar otro demo o un test rompe), atacar entonces.

**Decisión clave 4 — Welcome modal recentra cada frame (`ImGuiCond_Always` + `WorkPos + WorkSize/2`).** Pre-F2H57 el welcome usaba `ImGuiCond_Appearing` con `viewport->GetCenter()`. En pantallas >720p el viewport del primer frame todavía no tiene su tamaño definitivo → modal queda off-center.

- **Razón:** el costo de recalcular center cada frame es despreciable (suma de 2 floats + dos puntos flotantes) y elimina la categoría completa de bugs de "modal off-center cuando la ventana del OS cambia de tamaño".
- **WorkPos + WorkSize sobre `GetCenter()`:** WorkSize excluye la menubar — sin esto el modal queda visualmente alto (porque el GetCenter incluye el espacio de menubar). WorkPos cubre el caso de multi-viewport ImGui (cuando un docking layout mueve el work area).
- **Aplicabilidad a otros modales:** si emerge el mismo bug en convert/pick mesh modals, repetir el mismo patrón. v1 los deja con `ImGuiCond_Appearing` porque arrancan después del welcome (viewport ya estable).

**Lecciones cruzadas para futuros pivots UX:**
- **Tour visual del dev > validación headless cuando se trata de UX**: las 3 issues que motivaron F2H57 ninguna era detectable por suite de tests — emergieron porque el dev usó el editor por 20 minutos para crear una escena de testing del bloom de F2H55. Pattern: priorizar tour visual como criterio de aceptación de cierres de hitos que tocan render/editor.
- **El plan inicial del hito no sobrevive al primer feedback real**: PLAN_HITO_F2H57.md draft tenía Hammer file picker como Bloque B. Tras la primera demo el dev pidió SFM-style + popup intermedio + después pidió eliminar el popup. El plan se refinó en flight. Engine-grade no significa "spec inmutable" — significa "cada cambio del plan se documenta en DECISIONS".

---

## 2026-05-14: F2H56 cierre — SSAO + depth-texture FB + per-scene settings

**Contexto:** segundo hito de **Sub-fase 2.6 — Render polish** (F2H55 = bloom). Continúa el orden de impacto visual planeado: bloom → AO → color grading → god rays. Tag `v1.43.0-fase2-hito56`. Dev validó: *"el SSAO funciona bien"* — esquinas y debajo de objetos se oscurecen sutilmente al default; subiendo intensity a 3.0 el efecto se vuelve marcado.

**Decisión clave 1 — Port de Filament/Godot vs adoptar AMD FidelityFX CACAO.** Mismo dilema que tuvimos con bloom F2H55, pero esta vez SÍ existe una lib externa real (CACAO de AMD, MIT, mantenida, alta calidad). Elegimos **portar** igual.

- **Razón:** consistencia con la filosofía aplicada en F2H55 (no-reinventar = portar de open-source comprobado, no agregar dependencia externa). El port de Filament/Godot 4 cubre el caso base (~150 líneas GLSL) y CACAO sería overkill para v1 cuando ni siquiera sabemos si el dev necesita más calidad.
- **Alternativa diferida:** CACAO como upgrade futuro si emerge calidad pobre. El swap sería un sub-hito puntual (reemplazar el `ssao.frag` + ajustar uniforms; el wireup C++ no cambia porque el shader interfaz es la misma).
- **Trade-off documentado:** SSAO básico de Filament tiene noise inherente que el blur 4x4 disimula parcialmente — visualmente queda "OK" pero no "premium". Si el dev pide "se ve raro / noisy", swap a CACAO.

**Decisión clave 2 — Depth attachment como textura solo en modo HDR de OpenGLFramebuffer.** El FB principal de scene render (HDR RGBA16F) ahora crea su depth como `GL_TEXTURE_2D` con formato `GL_DEPTH24_STENCIL8` en lugar de `GL_RENDERBUFFER`. LDR FB (viewport final que muestra ImGui) mantiene renderbuffer.

- **Razón:** SSAO necesita samplear depth desde un fragment shader → requiere textura. LDR FB no necesita samplear su propio depth (solo se usa para Z-test al renderizar, y al final el `glColorTextureId` es lo que ImGui muestra).
- **Trade-off de memoria:** ~24-32 MB extra a 1920x1080 (textura vs renderbuffer ambos son lo mismo en memoria, el cambio es de tipo de objeto OpenGL, no de tamaño). Sin overhead en runtime.
- **Compatibilidad:** Shadow pass usa su propio FB (no `OpenGLFramebuffer`), no se afecta. Ortho viewports usan LDR, mantienen renderbuffer.
- **Revisión:** si en el futuro algún pass LDR necesita samplear depth, agregar flag al constructor de `OpenGLFramebuffer` para forzar textura en LDR. No emerge caso de uso todavía.

**Decisión clave 3 — SSAO multiplica el color HDR final, no solo el término ambient del PBR shader.** Esto es **conscientemente incorrecto físicamente** pero simplifica drásticamente la integración v1.

- **Razón pragmática:** integrar AO en el PBR shader (`pbr.frag`) implica agregar un sampler nuevo + branch para AO opcional + cambiar el flow de iluminación. Tocar el shader crítico del PBR es riesgoso (regresiones en todas las escenas). El composite separado en SSAOPass mantiene el cambio aislado.
- **Consecuencia visual:** la luz directa también recibe oclusión (las sombras de cualquier directional light se OSCURECEN MÁS donde hay AO), lo cual no es físicamente correcto — solo el ambient/indirect debería ocluirse. En la mayoría de escenas la diferencia visual es sutil (el ojo no nota el over-darkening).
- **Cuándo refactorizar:** si emerge complaint del dev sobre "los objetos se ven planos / lavados en zonas con AO" — síntoma de over-darkening. El refactor sería pasar el AO texture al PBR shader como uniform + multiplicar solo `iblContribution + ambient * ao` antes del directional lighting. Hito propio (~2-3h).

**Decisión clave 4 — Half-res AO buffer en lugar de full-res.** Los 2 FBs internos de SSAO (raw + blurred) son la mitad del ancho/alto del scene FB. El composite es full-res.

- **Razón:** convención industria — 16 samples por pixel a full-res es caro (~1080p × 16 samples = 32M texture reads por frame solo para SSAO). El blur 4x4 que viene después disimula la pérdida de resolución del downsample. Filament, Unreal, Godot, todos hacen half-res por default.
- **Trade-off:** en patrones de muy alta frecuencia (líneas finas, texto en el mundo), la resolución de AO puede notarse. No emerge caso de uso real.
- **Revisión:** si el dev pide "AO más definido en bordes finos", agregar opción full-res al SSAOPass. Improbable para una escena de juego típica.

**Decisión clave 5 — Defaults SSAO ON con intensity=1.0.** Mismo criterio que bloom en F2H55.

- **Razón:** engine-grade no toca semánticas de gameplay pero SÍ provee defaults visuales sensatos. SSAO al default añade "peso" a los objetos sin estilo intrusivo. El dev del juego apaga el slider si quiere look plano vintage.
- **Alternativa descartada:** default OFF — descartado por la misma razón que bloom (el dev quería "lo visual que podemos sacar bien" — defaults aspiracionales).

**Bugs UX detectados durante el tour (no scope F2H56, flagueados para F2H57):**

- **Vista SIDE (ZY) ortho dibuja brushes con eje invertido.** Arrastrar izquierda-a-derecha mapea al revés en la matemática del editor. Probable causa: confusión de signo en la conversión screen-coords → world-coords del eje Z en la vista lateral. Investigación dirigida al `OrthoViewportPanel.cpp` cuando lleguemos al hito.
- **Falta "Crear Entidad" button.** El editor no expone un workflow directo para crear entidades — el dev cita Hammer Editor (Source Engine) como referencia: botón "Create Entity" → elegir tipo + importar modelo opcional → editar propiedades en Inspector. Workflow actual obliga a spawn vía Demos del menú Ayuda, lo cual es muleta.
- **Demos como muleta a eliminar.** Por la falta del workflow anterior, los Demos del menú Ayuda son la única forma práctica de poblar una escena nueva. Pendiente eliminarlos una vez exista el workflow real (F2H57). Mantenerlos hasta entonces para que el editor sea usable.

Estos 3 ítems componen el scope de **F2H57 — Workflow de creación de entidades estilo Hammer + fix SIDE ortho + remove demos**. Pivot temporal de la Sub-fase 2.6 (render polish) a UX del editor antes de continuar con color grading (F2H58) / god rays (F2H59).

---

## 2026-05-14: F2H55 cierre — Bloom (glow) + Environment per-scene (apertura Sub-fase 2.6 Render polish)

**Contexto:** primer hito de **Sub-fase 2.6 — Render polish** post-cierre de Sub-fase 2.5 (Diálogos / Inventario / Quests, F2H53). El dev pidió "lo visual que podemos sacar bien" — entre 4 candidatos (bloom / AO / color grading / god rays) eligió bloom por mayor impacto inmediato + scope acotado. Tag `v1.42.0-fase2-hito55`. F2H54 quedó **skip** (laptop-only descartado por divergencia con desktop al cerrar F2H53).

**Decisión clave 1 — No-librería externa para bloom: portar shaders open-source en lugar de adoptar SDK.** Bloom no tiene "lib plug-and-play" en la industria — Unreal Engine, Unity HDRP, Godot 4, id Tech, Frostbite todos lo implementan inline porque debe estar pegado al pipeline del motor (formatos HDR específicos, orden de passes, mip chain de FBs). El algoritmo (downsample con Karis + upsample tent + composite) viene de una presentación de Sledgehammer/Activision 2014 ("Next Generation Post Processing in Call of Duty Advanced Warfare", Jorge Jimenez) — es el estándar de facto desde hace 10+ años. Lo que cambia entre motores es el tuning, no la matemática.

- **Razón:** integrar una lib externa terminaría siendo más trabajo que las ~80 líneas de GLSL. Y "no reinventar" en este dominio significa **portar de referencia open-source comprobada** (Godot MIT, Filament Apache 2.0) — no escribir de cero.
- **Alternativas evaluadas:** (a) **AMD FidelityFX SDK** — descartado para bloom: el SDK trae CAS/FSR/CACAO pero no bloom como módulo discreto. Re-evaluable cuando lleguemos a AO en F2H56 (CACAO sí es código AMD mantenido). (b) **bgfx examples** — sólo reference code, no library. (c) **Adoptar bgfx o Filament completos** — descartado: implicaría reemplazar el motor entero (RHI propio). (d) **Reinventar from scratch** — descartado: alta probabilidad de bug, sin upside.
- **Atribución en código:** headers de los 4 shaders mencionan Godot 4 / Filament + algoritmo COD AW 2014. Documenta que NO es trabajo propio + da pista al próximo agente sobre dónde buscar si necesita modificar.
- **Revisión:** si Godot/Filament evolucionan su algoritmo (ej. dual-Kawase vs mip-chain), re-evaluar el port.

**Decisión clave 2 — Settings de bloom (y futuros polish) per-mapa en `EnvironmentComponent`, NO global.** Extender el componente existente con 4 campos nuevos en lugar de inventar un struct `EnvironmentSettings` separado.

- **Razón:** el motor YA tiene `EnvironmentComponent` con skybox + fog + exposure + tonemap + IBL intensity (F2H15/F2H18). Bloom es semánticamente el mismo dominio ("cómo se ve este mapa"). Sumar un struct paralelo violaría YAGNI + duplicaría serialización + duplicaría UI. Per-mapa permite que la cueva tenga bloom alto + el desierto bajo (mood diferencial).
- **Consecuencia futura:** F2H56 (AO) suma `ssao*` al mismo componente. F2H57 (color grading) suma `colorGrading*`. F2H58 (god rays) suma `godRays*`. El componente crecerá ~12-16 campos más durante Sub-fase 2.6 — aceptable mientras siga siendo conceptualmente coherente. Si emerge presión, split a subcomponente (p.ej. `PostFXComponent`) con migración aditiva.
- **Alternativa descartada:** global setting en `config.json` o `UserSettings` — apaga el caso de uso principal (variedad mood por mapa). Per-cámara override quedó para Sub-fase 3 si emerge demanda (cinemáticas custom).

**Decisión clave 3 — Defaults aditivos en JSON: solo persistir campos que difieren del default.** Cuando `EntitySerializer` escribe el `environment` block, los 4 campos bloom solo aparecen si difieren de su default. El parser lee con `je.value(key, default)` — campos ausentes resultan en el mismo valor que campos presentes con el default explícito.

- **Razón:** mapas `.moodmap` pre-F2H55 round-tripean SIN ensuciarse con los 4 campos nuevos. Sólo los mapas que el dev edita activamente con bloom custom acumulan los campos en disco. Mantiene los diffs de git limpios.
- **Patrón establecido:** mismo enfoque que `ibl_intensity` (Hito 18) — ese campo también se persiste sólo si `!= 1.0`. F2H55 extiende el patrón a 4 campos más.
- **Trade-off:** si el dev EXPLÍCITAMENTE pone `bloomIntensity = 0.6` (igual al default), no se persiste — el .moodmap no refleja la "intención de set". Aceptable mientras el default sea estable. Si emerge presión, refactor a "siempre persistir todos".

**Decisión clave 4 — Cero regresión visual con bloom apagado: si `apply()` falla o intensity=0, post-process lee directo del scene FB.** El `endFrame()` del SceneRenderer mantiene dos rutas:

- **Razón:** F2H55 toca el flujo crítico del frame (post-process). Una regresión silenciosa donde "sin bloom" se ve distinto a "antes de F2H55" sería un bug difícil de detectar. La regla *"con bloom apagado, idéntico al frame pre-F2H55"* es testeable visualmente por el dev.
- **Implementación:** `BloomPass::apply` retorna bool. `endFrame` solo apunta `postProcessSrc` al `m_bloomFb` si el apply retornó true. Si bloom está deshabilitado, no se invoca al pass; `postProcessSrc` queda en `m_sceneFb` directo.
- **Bug descubierto durante tour por NO seguir esta regla en v1:** primera implementación cambiaba `postProcessSrc = m_bloomFb` ANTES de invocar `apply()` — si apply early-returnaba (mip chain no construido), el FB destino quedaba con contenido stale → pantalla negra. Fix aplicado durante el tour del dev.

**Decisión clave 5 — Defaults bloom ON (no OFF).** Cuando el dev crea un proyecto nuevo o agrega `EnvironmentComponent`, bloom arranca enabled con intensity=0.6.

- **Razón:** engine-grade no toca semánticas de gameplay pero SÍ provee defaults visuales sensatos. Igual que el motor arranca con ACES tonemap default (no None) o IBL intensity 1.0 (no 0). El dev del juego que quiera look plano vintage apaga el slider — pero el motor no DEBE servir "indie sin pulir" como default.
- **Revisión:** si emerge complaint del dev sobre que el bloom "interfiere" con su look, agregar flag en UserSettings global para apagar el default.

---

## 2026-05-14: F2H53 cierre — Quest System engine-grade (schema + state machine + tick + Browser/Editor + Lua + HUD + persistencia)

**Contexto:** cierra el Bloque 2 (Quests) del `PLAN_SUBFASE_2_5.md`. Sub-fase 2.5 Bloque 1 (Inventario) ya estaba completo con F2H52; F2H53 abre y cierra el sistema de quests aprovechando las primitivas de F2H48 (dialog vars) y F2H52 (inventory bindings). Tour visual validado por dev: *"funcionó todo"*.

**Decisión clave 1 — Predicates declarativos sobre primitivas existentes (NO sistema de eventos custom).** Los 3 predicate types (`Collect`, `Talk`, `Reach`) se compilan a strings Lua que consumen primitivas YA registradas: `inventory.count('items/x') >= N` (F2H52) y `dialog.has_var('foo')` (F2H48). El motor NO inventó un sistema de eventos paralelo "quest.event_listen(...)".

- **Razón:** engine-grade strict. La cantidad de items en el inventario YA es queryable; las dialog vars YA son el event log del juego. Sumar un tercer mecanismo de eventos sería redundancia + más superficie de bug. El dev expresa cualquier condición complejas con `CustomLua` (escape hatch).
- **Alternativas descartadas:** (a) Sistema de eventos pub/sub propio (`quest.on_event("kill_enemy", ...)`) — descartado: el motor no conoce "enemy"; sería otra capa de game-specific. (b) Predicates como C++ función (`std::function<bool(...)>`) — descartado: requeriría re-compilación cada vez que el dev cambia un predicate. Strings Lua tienen hot-reload gratis.
- **Revisión:** si emerge un caso de uso que el `CustomLua` no cubre cómodamente (ej. "matar 5 enemigos de tipo X en menos de 30s"), bumpear el schema con un cuarto type. La mayoría de quest types en Skyrim/Witcher caen en alguno de los 4 actuales.

**Decisión clave 2 — `LuaEvaluator` + `LuaExecutor` inyectables (NO sol::state directo en el motor).** El `QuestSystem` corre el `tick()` y necesita evaluar predicates + aplicar rewards-como-código-Lua. NO importa sol2 directamente. Tiene 2 `std::function` inyectados que el script host (LuaBindings) instala apuntando a su propia sol::state.

- **Razón:** mismo patrón que `DialogSystem::setEvaluator/Executor` (F2H48). Permite que `mood_tests` testee el QuestSystem headless sin sol2 (los tests inyectan mocks). Mantiene el `engine/quest/` libre de incluir sol2 en su header.
- **Lifetime trap descubierto en F2H53:** los hooks globales (`g_evaluator`, `g_onComplete`, etc.) capturan referencias/punteros a la `sol::state` del ScriptSystem. Si la state muere antes que los hooks (orden default de destrucción), al terminar el proceso `~std::function` destruye una `sol::function` colgante → `lua_unref` sobre VM muerto → SIGSEGV. **Fix**: llamar `QuestSystem::clearHooks()` + `Inventory::Hooks::clearAll()` ANTES de `m_scriptSystem.reset()` en `~EditorApplication` y `~PlayerApplication`. Mismo bug pattern que F2H52 J — ahora documentado como invariante de shutdown.

**Decisión clave 3 — Identificación por path lógico en persistencia (NO por id).** El schema `.moodsave` v3 guarda `quests[].path` (`"quests/q_fetch.moodquest"`) en lugar del `QuestAssetId` numérico.

- **Razón:** paridad con `SavedInventory` de F2H51 I. Los IDs son volátiles entre runs — dependen del orden de `loadQuest` del AssetManager (primero en cargarse = id 1, etc.). El path es estable mientras el archivo exista.
- **Consecuencia:** si el dev borra un `.moodquest` entre save y load, ese quest aparece como "huérfano" en log warn y se skipea silencioso en restore. NO crash. Mismo trade-off que `SavedInventory`.

**Decisión clave 4 — `QuestSystem::restore(...)` separado del lifecycle normal.** En lugar de re-usar `start()` + setear progreso, hay una API dedicada `restore(id, state, objectiveDone, am)` que:
- Inserta directo al `g_active` sin chequear validez de transición.
- NO dispara hooks (es restauración, no transición).
- Trunca/padea `objectiveDone` si el asset cambió entre save y load.

- **Razón:** semánticamente diferente. `start()` es "el jugador empieza un quest" (dispara cinematic, SFX, etc. via hooks); `restore()` es "el game state se está reconstruyendo" (silencioso). Mezclarlos llevaría a callbacks disparándose al cargar partida — UX espantosa.
- **No expuesto a Lua:** sólo callable desde C++ (`SaveLoad::load → applyLoadedSave`). Si el dev hace `quest.start("...")` desde Lua, dispara los hooks normalmente.

**Decisión clave 5 — Re-start permitido tras `Failed`, bloqueado tras `Active`/`Complete`.** Cuando el dev llama `quest.start(path)` y el quest ya está registrado, el comportamiento depende del state actual.

- **Razón:** retry tras fallar es UX común en RPGs ("¡puedes intentar la misión otra vez!"). Re-start tras complete sería duplicar progreso (one-shot por design). Active doble es no-op evidente.
- **Revisión:** si emerge un caso de uso para "repeatable quest" (daily quests, side jobs), agregar flag `repeatable` al schema y permitir re-start tras Complete cuando esté activado.

**Decisión clave 6 — Tracker HUD reutiliza el widget `objective_text` (NO nuevo widget).** El widget `drawObjectiveText` (F2H41) ahora tiene 2 modos: Quest Tracker (preferido si hay tracked) vs Legacy text (fallback F2H41).

- **Razón:** compatibilidad con scripts existentes que usen `hud.set_widget("objective_text", false)` o `hud.setObjective("...")`. El nombre del widget en el registry queda igual; sólo cambia el render interno según el state del QuestSystem.
- **Alternativa descartada:** widget nuevo `quest_tracker` separado — implicaría que el dev tenga DOS widgets que pueden estar enabled simultáneamente y compitiendo por la misma posición top-left.

**Decisión clave 7 — Quest Log panel como widget separado con toggle por tecla (NO ventana ImGui::Begin standalone).** El panel se dibuja con `ImDrawList` directo en el viewport overlay (paridad con `inventory_panel` de F2H52). Toggle por **J** (convención RPG Skyrim/Witcher "Journal").

- **Razón:** consistencia con los demás widgets HUD. No queremos ventanas ImGui flotantes en Play Mode — el HUD es overlay no-interactivo via mouse capture. Click sobre quest = mouse no capturado en Play (porque el panel está en `widget_enabled["quest_log_panel"]` que el dev togglea con J y respeta `isInputBlocked`).
- **Limitación v1:** sin scroll. Si hay más de ~10 quests visibles, los siguientes quedan fuera del panel. Migrar a ImGui::Begin + ScrollRegion cuando emerja necesidad.

---

## 2026-05-12: F2H52 cierre — Inventory runtime (pickup + HUD + Lua + container split + renderer override)

**Contexto:** cierra el Bloque 1 (Inventario) del `PLAN_SUBFASE_2_5.md`. F2H51 entregó autoría + state + persistencia; F2H52 cierra el lado runtime. Sub-fase 2.5 Bloque 1 ✅ completo.

**Decisión clave 1 — Hooks Lua único callback por evento (no veto, sí after-success).** Los 3 hooks (`on_pickup` / `on_drop` / `on_use`) se sobrescriben con warn si el dev registra dos veces. Motor completa la operación primero (add/remove) y DESPUÉS dispara el callback como "after-success notification". El dev NO puede vetar — si necesita veto, hace `inventory.has` check antes de la acción.

- **Razón:** simplicidad > composability en v1. Patrón identico a `DialogSystem::setEvaluator/Executor` (F2H48.1). Si emerge necesidad de N callbacks, sumar `add_listener(...)` aparte sin deprecar el setter único.
- **Trade-off aceptado:** el dev tiene que decidir en su Lua dónde meter toda su lógica de pickup (en un solo callback). Para hot reload, el sol::function vieja queda colgando hasta que el nuevo script se cargue + se re-registre.

**Decisión clave 2 — `inventory.use(entity, path)` NO auto-consume.** El motor invoca el hook `on_use(entity, path)` y vuelve. Es el callback del dev quien decide si llama `inventory.remove(...)` adentro (caso poción) o no (caso arma equipable). Idem `inventory.drop` desde el HUD widget: motor remueve + spawnea pickup + invoca `on_drop` notificación.

- **Razón:** engine-grade strict. El motor no conoce "poción se consume al usar". El sample `inventory_demo.lua` muestra el patrón: `USE_HANDLERS` map por path con `health_potion` haciendo `inventory.remove(ent, path, 1)` adentro, y `iron_sword` NO (caso equipar).
- **Alternativa rechazada:** flag `consume_on_use` en el `.mooditem`. Hubiera obligado al motor a saber qué hacer post-consume (¿devuelve un wrapper vacío? ¿restaura HP automáticamente?). Más simple ceder al script.

**Decisión clave 3 — `Inventory::spawnPickupInWorld` helper compartido (3 callsites → 1).** Misma lógica de "crear entity con Transform + MeshRenderer + Trigger 0.5³ + ItemPickupComponent" se necesitaba en: Lua binding `inventory.spawn_pickup`, drop del HUD widget, drag-drop del Item Browser al viewport. Extraída a `engine/inventory/ItemSpawn.h/cpp`.

- **Razón:** evitar drift entre los 3 callsites cuando el schema del pickup evolucione. Mesh derivado del item: `model_path` → loadMesh + createMaterialsForMesh; sino `icon_path` → cubo + textura icon como albedo; sino default. El dev puede actualizar el helper en un lugar.
- **Tests:** `test_item_spawn.cpp` con 8 tests cubre creación exitosa, qty custom, scene/assets null, item inexistente, Trigger halfExtents, multi-spawn.

**Decisión clave 4 — Container split visualmente FlatList para ambos lados, sin importar el `mode` del state.** Cuando `hud.open_container(entity)` se llama, el widget renderea 2 columnas (player izq, container der) usando FlatList visual. La layout config del container's `InventoryComponent.state.mode` se IGNORA en el render del split.

- **Razón:** el loot UX (cofre/comercio/drop pile) rara vez quiere el grid spatial del container. Casos como "open chest in RE4 → see chest's grid" son raros; lo común es "open chest → see flat list of items, drag to player".
- **Limitación documentada:** si el dev quiere split visual respetando el mode del container, registra su renderer custom via `inventory.set_renderer`. El motor expone primitivas; el dev decide presentación.

**Decisión clave 5 — `HudState::container_open` bool + `container_target` u32 (NO sentinel value).** Descubierto bug en testing: `entt::entity` 0 es un handle VÁLIDO (la primera entity creada). No podemos usar `container_target = 0` como sentinel "no container". Refactor a dos campos: `bool container_open` + `u32 container_target`. El widget chequea ambos.

- **Razón:** sin el bool, el primer `hud.open_container(player)` (caso degenerado pero válido) hubiera sido indistinguible de "no hay container abierto". El bool es cero overhead + máxima claridad.
- **Pattern replicable:** cualquier campo que store un entt handle como "opcionalmente seteado" debería usar un bool paralelo, no un valor sentinel.

**Decisión clave 6 — `inventory.set_renderer(callback)` cede TODO al dev (modes + tooltip + context menu skipeados).** Cuando el dev registra un renderer custom, el motor no dibuja nada del default — el callback recibe `(player, container_or_nil)` y se encarga.

- **Razón:** engine-grade extremo. No tendría sentido mezclar "el motor dibuja la mitad y el dev la otra". Cede o no cede — sin estados intermedios.
- **Limitación v1 documentada:** sin bindings ImGui mínimos en Lua, el callback solo puede dibujar usando bindings del dev (UI custom Dead Space-style). Cuando emerja necesidad de devs sin sus propios bindings → hito de "bindings ImGui mínimos para Lua".

**Decisión clave 7 — Drop horizontal (forward proyectado al plano XZ), no diagonal.** Primera versión hizo `cameraPos + cameraForward * 1.8`. Si el jugador miraba hacia abajo (cosa común al ver su propio inventario), forward apuntaba al piso → item atravesado. Fix: proyectar al plano horizontal (`forward.y = 0` + normalize) antes de multiplicar. Y offset -1.1 desde camera height.

- **Razón:** UX consistente independiente del pitch de la cámara. El jugador SIEMPRE ve el item caer enfrente, mire donde mire.
- **Alternativa rechazada:** raycast contra el piso para apoyar el item exactamente al nivel. Sobre-engineering para v1; si el piso es irregular, el item flotará un poco — aceptable para un sistema de inventario, no es un physics-driven drop.

**Decisión clave 8 — Bug fix descubierto en tour M: `MousePos = -FLT_MAX` machacado cada frame.** Código viejo de F2H41 forzaba `io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX)` cada frame con condición `Play && !paused`. La condición NO consideraba inventory_panel abierto, así que cuando el dev abría Tab durante Play, el cursor era liberado por updateCameras pero machacado por beginFrame — ImGui no recibía mouse, ni siquiera el botón Stop respondía.

- **Fix:** nuevo helper `GameState::isInputBlocked()` returns `paused() || widget_enabled["inventory_panel"]`. Reemplaza el chequeo de `!paused()` en beginFrame + el de `paused()` en updateCameras de editor + player. Generalizable: cualquier overlay UI futuro (trade menu, quest log, etc.) se suma al helper sin tocar los 4 callsites.
- **Lección aprendida:** cuando un fix lateral (F2H41) cambia comportamiento global del input, dejar siempre un helper centralizado en lugar de inlinear el chequeo. El inlining causó que F2H52 no anticipara la interacción.

---

## 2026-05-12: F2H51 cierre — Inventario engine-grade (autoría + state + persistencia)

**Contexto:** Bloque 1 del `PLAN_SUBFASE_2_5.md`, sexto hito real de Sub-fase 2.5. F2H50 cerró el flow narrativo end-to-end con NPC tangible; F2H51 abre el sistema de inventario (Bloque 1 del plan macro, pedido del dev: *"recuerda la idea es crear una base solida para que a futuro cualquiera pueda crear su sistema de conversaciones, misiones, o inventario y asignar a modelos 3D"*). Split editor/runtime aplicado: F2H51 = autoría + state + persistencia; F2H52 = runtime (pickup + HUD + Lua).

**Decisión clave 1 — Engine-grade strict: motor sin semántica hardcoded de "weapon"/"potion"/"armor".** El schema `.mooditem` tiene `tags` (`std::vector<std::string>` libre) + `stats` (`std::map<string, float>` libre). El motor NO interpreta ni "damage" ni "heal_amount" — solo los almacena. Cada dev del juego define su propia ontología.

- **Razón:** principio #2 del marco estratégico de Sub-fase 2.5 (*"Sin semántica hardcodeada de gameplay"*). Devs futuros de juegos distintos (RPG/shooter/walking sim/etc) configuran su propia semántica sin recompilar.
- **Tensión con UX:** el dev nuevo no sabe qué stats ponerle a un arma. Resuelto en Bloque K post-validación con **plantillas** (dropdown "Plantilla" en `+ Nuevo Item` con `Vacío`/`Arma`/`Poción`/`Armadura`/`Quest item`/`Objeto`). Las plantillas son presets del editor (`applyTemplate` en anonymous namespace de `ItemBrowserPanel.cpp`) — el motor sigue sin conocerlas. Como punto de partida, no semántica.
- **Alternativa rechazada:** hardcodear categorías como enum en el schema. Hubiera roto principio #2 + obligado a tocar C++ cada vez que un dev quiere una categoría nueva ("alchemy_reagent", "spell_focus", "trinket"). Imposible engine-grade.

**Decisión clave 2 — Split editor/runtime F2H51 vs F2H52 (mismo patrón F2H47→F2H48).** F2H51 cierra: ItemAsset schema, AssetManager loader, InventoryState pure logic, InventoryComponent, Item Browser, Property Editor, Inspector section, persistencia `.moodmap`, workspace "Gameplay". F2H52 hará: ItemPickupComponent, HUD widget, Lua bindings, integración con DialogScriptHost.

- **Razón:** checkpoint natural de validación del schema antes de atarlo al runtime. Si el runtime emerge con requisitos extras del schema, bumpear `.mooditem` v1→v2 cuesta poco antes de tener saves reales. Hitos mantenibles (~10-12h cada uno) en lugar de uno gigante (~20h).
- **Validado en Bloque K:** el dev pudo probar el flow end-to-end de autoría (crear, editar, persistir items + inventarios en entidades + roundtrip save/load) sin necesitar el runtime de pickup. Validación temprana.

**Decisión clave 3 — 3 layout modes (FlatList / Grid2D / EquipmentSlots) en v1, NO solo FlatList.** Resistí la tentación de hacer solo FlatList para v1 y agregar los otros cuando emerjan.

- **Razón:** PLAN_SUBFASE_2_5 sección 1.3 los lista como fundacional. Implementar solo FlatList sería deuda inmediata cuando F2H52 HUD widget necesite Grid2D estilo Resident Evil para el caso "inventario tetris" + EquipmentSlots para el caso "RPG slots de armadura". Costo extra ~3-4h vale la pena.
- **Default:** FlatList max_items=20. Universal, se entiende sin contexto del género del juego.
- **Trade-off polimorfismo simple:** la lógica de `add`/`remove`/`placeAt` tiene 3 ramas (switch sobre `LayoutMode`). Encapsulado en `InventoryState`. Si emerge complejidad real, refactor a `std::variant` + visitor.

**Decisión clave 4 — Paths-no-ids en persistencia `.moodmap` (mismo patrón F2H50 AnimatorComponent).** El `SavedInventoryEntry` persiste `itemPath` (string lógico) y NO `itemId` (`ItemAssetId`).

- **Razón:** los IDs no son estables entre sesiones (dependen del orden de loads del AssetManager). Persistir el path lógico (`assets.itemPathOf(id)`); al cargar, `assets.loadItem(path)` lo re-resuelve y reconstruye el `Entry`.
- **Bump aditivo:** mapas pre-F2H51 cargan sin componente (`std::optional<SavedInventory>` ausente — no se auto-agrega). Sin regression.
- **Limitación conocida:** si el dev borra un `.mooditem` entre save y load, el path persistido pierde info (queda como `__empty_item`). Mismo patrón F2H50 — sin caso real todavía.

**Decisión clave 5 — `InventoryComponent` agregado al whitelist de `SceneSerializer::save`.** Antes el whitelist solo incluía Mesh/Light/RigidBody/Environment/Script/Particle. Una entidad con solo `InventoryComponent` no se persistía.

- **Razón:** principio engine-grade — un entity puede ser un chest/container/vendor SIN mesh visible (logic-only entity). No asumir que el inventario implica un visual.
- **Trade-off:** abre la puerta a entidades "fantasmas" (logic-only) en el .moodmap. Acepto — es la dirección correcta. Si emergen issues (entidades huérfanas), se filtran en futuro.

**Decisión clave 6 — `slot_size > 1x1` IGNORADO en v1 del `InventoryState`.** El schema persiste `slot_size {width, height}` pero la lógica de `add`/`placeAt` para Grid2D asume cada item ocupa 1 cell.

- **Razón:** el packing rectangular real (Resident Evil 4 inventory style) requiere algoritmo de "bin packing" que no es trivial. Implementarlo bien suma ~3-4h adicionales para un caso de uso que ningún demo necesita todavía. YAGNI v1.
- **Schema persiste el campo** para roundtrip — cuando v2 lo implemente, no rompe assets existentes. Documentado en hint del Property Editor: *"Sólo se respeta en layout grid_2d. v1 ignora width/height (cada item ocupa 1 cell)."*

**Decisión clave 7 — 3 fixes UX post-validación del dev (Bloque K).** El dev validó visualmente el flow + reportó 4 issues. Decisiones:

- **Issue 1 — checkbox "Usar i18n key" confuso:** el dev no entendía qué hacía. **Fix:** tooltip al hover sobre los checkboxes name/description que explica el use-case (juego multi-idioma vs mono-idioma). Mantengo el checkbox (el motor soporta i18n, eso es engine-grade); no lo escondo porque sería romper esa capability.
- **Issue 2 — i18n del Property Editor:** los strings UI ya se traducen (es.json/en.json). El dev preguntó si traducción automática (Google Translate / DeepL). **Respuesta diferida:** las grandes empresas (Riot/Blizzard/Nintendo) NO usan auto-translation para texto in-game — usan plataformas pro (Crowdin/Lokalise/Phrase) con traductores humanos. Para MoodEngine queda como hito propio "Localization Pipeline" en Sub-fase 3 (~6-8h): dev escribe en su locale → editor auto-genera la i18n key → UI lista keys sin traducir para que un humano complete los otros locales. NO entra en F2H51. Agregado a `PENDIENTES.md`.
- **Issue 3 — InputFloat `%.3f` mostraba "20.000" leído como "20000":** el separador decimal de Windows en español es coma, y `%.3f` siempre rellena 3 decimales. **Fix:** `%g` smart-format ("20" si entero, "12.5" si decimal real).
- **Issue 4 — dev pedía categorías predefinidas tipo "armas/pociones/atuendos":** ver Decisión 1. Resuelto con plantillas en `+ Nuevo Item` que pre-pueblan tags + stats típicos. Engine-grade preservado.

**Alternativas descartadas (no entran en F2H51)**:
- **Iconos en cards del Item Browser:** requieren wireup AssetManager texture + ImTextureID handle. Costo ~1h. Diferido — el dev no lo pidió y es nice-to-have.
- **3D preview del modelo en Property Editor:** requiere Bloque 0.2 del PLAN_SUBFASE_2_5 (widget reusable de render-to-texture con cámara orbital). Diferido a hito propio cuando emerja Material Editor pro.
- **Drag-source desde Item Browser → spawn pickup en Viewport:** requiere `ItemPickupComponent` (F2H52). Lógico que vaya juntos.

**Revisión:** si en F2H52 emerge un requisito que invalida alguna de estas decisiones (ej. el runtime de pickup necesita un campo nuevo en el schema), bumpear `.mooditem` v1→v2 con fallback en `fromJson`.

---

## 2026-05-11: F2H50 cierre — Demo narrativa end-to-end + persistencia AnimatorComponent + regen materiales auto

**Contexto:** F2H49 cerró el pipeline de animaciones standalone (FBX anim-only). F2H50 cierra el Bloque 2.5 del `PLAN_SUBFASE_2_5.md` atando esas animaciones al sistema de diálogo + character controller para validar el flow narrativo end-to-end con un NPC tangible (dev: *"que sentido tiene crear un sistema de dialogo, sino tenemos a quien asignar"*).

**Decisión clave 1 — Auto-attach de sibling `anim_*.fbx` al spawn de mesh skinneado:** cuando el dev dropea un FBX con skeleton en el viewport, el motor escanea la carpeta padre del mesh y enchufa todos los `anim_<alias>.fbx` siblings al `AnimatorComponent.externalClips` automáticamente, defaulteando `clipName = "idle"` si existe ese alias.

- **Razón:** sin esto, el dev tiene que drag-drop manualmente cada clip desde el Inspector tras spawnear un rig. Para un demo de 3-6 clips por personaje es tedioso. La convención de filename (`anim_*`) ya estaba documentada en `assets/characters/README.md`.
- **Generalización (no exclusivo de Mixamo):** funciona con cualquier rig (`assets/heroes/hero/hero.fbx` + `anim_*.fbx` siblings). Convención por filename, no por carpeta.
- **No-op para FBX sin siblings (Fox.glb, Kenney props):** la lista queda vacía → comportamiento Hito 19 preservado (primer clip embedded).
- **Default `clipName = "idle"`:** asume convención que "idle" es la pose neutral. Para chars sin idle, queda `""` (primer embedded, típicamente T-pose para Mixamo "With Skin").

**Decisión clave 2 — Condición de auto-add de AnimatorComponent al spawn relajada a solo `hasSkeleton()`:** antes era `hasSkeleton() && !animations.empty()`. Ahora cualquier rig con esqueleto recibe AnimatorComponent + SkeletonComponent.

- **Razón:** el log existente `"(skinned + animator)"` ya usaba la condición floja (visible incluso para meshes sin embedded anims), creando inconsistencia entre log y comportamiento. Más importante: con el auto-attach (decisión 1), un rig sin embedded anims + con sibling `anim_*.fbx` puede igual usar esos clips externos. La condición vieja le negaba el AnimatorComponent y perdía esos clips.
- **Trade-off:** rigs con skeleton + sin clips de ningún tipo tienen AnimatorComponent inerte. Cero costo runtime (el AnimationSystem retorna early si no hay clip activo).

**Decisión clave 3 — `SavedAnimator` agrega persistencia completa del `AnimatorComponent` en el `.moodmap`:** bump aditivo del schema. Persiste `clipName`, `speed`, `playing`, `loop`, y la lista `externalClips` como pares `{alias, path}`.

- **Razón:** pre-F2H50 el AnimatorComponent NUNCA se serializaba — el SceneLoader auto-agregaba uno con defaults (`clipName=""`, `playing=true`, `loop=true`) cuando el mesh tenía skeleton + embedded anims. Eso significaba que editar `anim.speed = 2.0` en el Inspector, save, load → speed back to 1.0. Más crítico: los `externalClips` agregados via drop o via inspector se perdían en cada save.
- **Path persistido vs ID:** los `AnimationClipAssetId` no son estables entre sesiones (dependen del orden de loads del AssetManager). Persistimos el path lógico (`assets.animationClipPathOf(clipId)`); al cargar, `loadAnimationClip(path)` lo re-resuelve. Mismo patrón que MeshRenderer.meshPath.
- **`externalBindCache` NO se persiste:** runtime state que `AnimationSystem` regenera al primer evaluate via `bindClipToSkeleton`. Mismo criterio que `cachedDialogId` (F2H48.1) — IDs cacheados no sobreviven sesiones.
- **`time` NO se persiste:** siempre arranca en 0 al cargar. Convención "respawn" de motores 3D. Si emerge necesidad de "remember anim time" (ej. cinemática pausable), agregar como flag.
- **Fallback path para mapas pre-F2H50:** si el JSON no trae `animator`, el SceneLoader cae al auto-add con primer embedded — same behavior as Hito 19. Cero regression.

**Decisión clave 4 — Regen lazy de materiales auto en SceneLoader cuando el slot tiene path vacío:** descubierto al validar el flow save→load del demo F2H50. Los X/Y Bot aparecían magenta tras un roundtrip.

- **Causa:** el EntitySerializer (línea ~86) nukea cualquier path que empiece con `__` a string vacío (handling de paths internos). Eso incluye los `__runtime#<id>` que `createMaterialsForMesh` asigna a materiales auto-generados con diffuse color de F2H49.1. El SceneLoader al ver path vacío caía a `missingMaterialId()` (magenta).
- **Fix:** en SceneLoader, cuando un slot tiene path vacío Y la mesh tiene auto-materials disponibles, regenerar lazy via `createMaterialsForMesh(meshId)`. Eso preserva textures embedded del FBX (Fox.glb) + diffuse colors (X/Y Bot) sin perderlos en el roundtrip — y sin tener que serializar el material como JSON inline (overhead).
- **Determinismo:** `createMaterialsForMesh(meshId)` es determinístico para un mesh dado — el resultado es función de los `materialAlbedoTextures` + `materialAlbedoColors` del MeshAsset. Reload del mismo mesh produce los mismos materiales.

**Decisión clave 5 — Demo scene minimalista (sin walls CSG):** el handler genera SOLO el NPC con sus componentes. Sin floor brushes, sin walls, sin lighting custom.

- **Razón:** filosofía engine-grade — el `.moodmap` es DATA. Hardcodear walls en el generator obliga al dev a editarlas si quiere distinta arquitectura. Mejor: minimal scaffold que el dev decora editando el .moodmap como cualquier mapa. La tile floor implícita es suficiente para el demo.
- **Alternativa descartada:** room rectangular con 4 walls + floor brush + RigidBody Static. Hubiera requerido también integración con el "compile map" flow para que las walls colisionen. Scope creep para el demo.

**Decisión clave 6 — Helper `ensureDemoIntroDialogExists` extraído del handler F2H47:** refactor del `processSpawnDialogDemoRequest` para reuso entre "Cargar diálogo demo" (que abre el DialogEditor) y "Cargar demo narrativo" (que lo usa como dependencia del NPC, sin side effects de UI).

- **Razón:** evitar copy-paste de ~50 LOC de generación de `.mooddialog`. El helper devuelve `bool` (existía ya / se creó OK / falla) para que ambos handlers tomen decisiones de continuar.

**Alternativas descartadas:**

- **Walls CSG + RigidBody Static en el demo (room cerrada):** scope creep + complica el flow (los brushes no colisionan automáticamente; necesitan "compile map"). Sin ganancia para el demo.
- **Animator state machine / blending en F2H50:** prematuro. El demo solo necesita un loop de idle. Locomotion blending (idle ↔ walk ↔ run) emerge cuando el player se mueva como char skinneado, no como capsule.
- **Serializar materiales runtime como JSON inline:** considerado para fixear la magenta de X/Y Bot. Descartado en favor de la regen lazy — JSON inline duplica datos del FBX (que ya tiene la info en `aiMaterial`), y obliga a versionar el formato del material en el `.moodmap`. La regen es declarativa y determinística.
- **Preservar path original cuando un clip standalone está missing al save:** requiere extender `AnimatorComponent.externalClips` de `pair<alias, id>` a `tuple<alias, path, id>`. Limitación que aparece solo si un asset referenciado fue borrado entre save y load. Sin caso real todavía — backlog si emerge.
- **Drop-on-viewport para anim clips:** nice-to-have UX. Drag clip → soltar sobre la entidad bajo el cursor → add a su `externalClips`. Requiere extender picking del viewport para detectar entity durante drag activo. Backlog.

**Condiciones de revisión:**

- Si emerge un caso donde el dev quiere "remember anim time" entre save/load (ej. cinemática que pausa + retoma), agregar `time` al SavedAnimator.
- Si la convención `anim_<alias>.fbx` se vuelve restrictiva (ej. dev quiere `walk_north.fbx` sin prefijo), agregar una segunda heurística (file inspection para "es anim-only") como complemento.
- Si los rigs futuros tienen embedded anims que el dev quiere usar EN LUGAR de siblings con mismo alias, agregar un toggle "embedded prioritario" o pedir al dev que use aliases distintos para los siblings.

---

## 2026-05-11: F2H49 cierre — Animaciones standalone (FBX anim-only) + bind pass + tab Animations + Inspector external clips

**Contexto:** el plan original de F2H49 era "Demo characters Mixamo + escena narrativa completa" (Bloque 2.5 del `PLAN_SUBFASE_2_5.md`). Al intentar cargar los `*.fbx` de Mixamo descargados a `assets/characters/`, descubrimos que el motor no soportaba clips de animación sin malla adjunta (caso "Without Skin" de Mixamo, 1 FBX por clip). Sin esto el NPC se quedaba congelado en T-pose durante el diálogo. F2H49 se re-scopea a habilitar el pipeline standalone; la demo narrativa pasa a F2H50.

**Decisión clave 1 — Cache externo (`externalBindCache` en `AnimatorComponent`) vs mutar `track.boneIndex`:** el bind pass calcula `outRemap[i] = skel.boneIndex(clip.tracks[i].boneName)` para cada track. El remap NO se escribe sobre `clip.tracks[i].boneIndex` (que queda permanentemente en `-1` para clips standalone) — se guarda en un `unordered_map<AnimationClipAssetId, vector<int>>` por entidad.

- **Razón vs mutar el clip:** mutar haría el clip skeleton-específico. Caso de uso central de F2H49: el mismo `anim_idle.fbx` se reusa entre player y npc (mismo `mixamorig:*` naming pero esqueletos físicamente distintos en memoria). Si mutamos el primer bind contamina al segundo personaje.
- **Razón vs vivir en una global por path:** el cache vive en el componente porque la invalidación es por-entidad (si una entidad swappea su mesh con un skeleton incompatible, solo SU cache es inválido). Cache global por clip-asset no podría discriminar.
- **Trade-off aceptado:** cache duplicado entre N entidades con mismo (clip, skeleton). Despreciable — 1 entry × ~50 ints × N entidades es trivial.
- **Invalidación:** responsabilidad del consumidor cuando swappea mesh (`anim.externalBindCache.clear()`). El runtime no detecta automáticamente — chequear `bones.size()` mid-frame por cada entidad sería caro y el caso de mesh-swap mid-frame es raro.

**Decisión clave 2 — 2 funciones libres en `AnimationClip.h` (`bindClipToSkeleton` + `evaluateClipWithRemap`) vs nuevo método en `AnimationClip`:** las funciones quedan como inline en el namespace, no como miembros del struct.

- **Razón:** mantener `AnimationClip::evaluate()` con su contrato Hito 19 intacto (lee `track.boneIndex` interno). Agregar un parámetro opcional `remap` mutaría su firma y obligaría a fixear callers downstream.
- **Bonus:** las funciones libres siguen el patrón existente de `samplePosition/sampleRotation/sampleScale` del mismo header — naturalmente extensible.
- **Header-only:** ambas funciones son inline, sin dependencias nuevas. Testeables en `mood_tests` sin enlazar `AnimationSystem`.

**Decisión clave 3 — `resolveActiveClip` devuelve `ClipResolution { clip*, remap* }` en lugar de mutar `AnimatorComponent`:** la función signature pasa de `const AnimationClip*` a un struct con 2 punteros (clip + remap opcional).

- **Razón:** keep `update()` con un único punto de decisión sobre qué `evaluate*` llamar. Si retornara solo el clip, habría que duplicar el lookup del remap en `update()`.
- **`remap=nullptr` semantica:** "este es un clip embedded, usá el `evaluate()` legacy". `remap=&cacheEntry` → "este es standalone, usá `evaluateClipWithRemap`". Reglas claras.

**Decisión clave 4 — Drop target en Inspector, NO en Viewport:** el `MOOD_ANIMCLIP_ASSET` payload se acepta solo desde el botón "Arrastrá un clip aquí" dentro del Inspector.

- **Razón:** un clip standalone no es entidad espacial — no se puede "spawnear" en el mapa. Necesita un personaje al que animar. Drop en viewport no tiene semántica clara (¿crear entity nueva? ¿asignar a la entity bajo el cursor?).
- **Alternativa diferida:** drop sobre la entidad bajo el cursor → add a `externalClips` de esa entity. Es nice-to-have UX, queda en backlog. Requiere extender el picking del viewport para detectar entity durante drag activo.

**Decisión clave 5 — Alias auto-derivado del filename con strip `anim_` prefijo:** al droppear `characters/player/anim_walk.fbx`, el alias defecto es `walk` (no `anim_walk` ni el path completo).

- **Razón:** el alias es lo que el gameplay/scripts usan para pedir reproducción (`animator.play("walk")`, `clipName = "walk"`). Mantenerlo corto y semántico mejora ergonomía. La convención `anim_<accion>.fbx` está documentada en `assets/characters/README.md`.
- **Defensa anti-colisión:** si el alias defecto ya existe en `externalClips`, NO se agrega (tooltip "alias ya en uso"). El user puede renombrar el conflicto primero.

**Decisión clave 6 — Filtro `anim_*.fbx` en tab Meshes (convención de nombre, no análisis de contenido):** la heurística para separar meshes de animaciones es el prefijo del filename, no inspeccionar el FBX.

- **Razón:** análisis de contenido (chequear si `aiScene::mNumMeshes==0`) sería más robusto pero requiere abrir el archivo cada scan — caro y se ejecuta varias veces durante el ciclo de vida del editor. Convención de nombre es O(1) sobre el path.
- **Trade-off aceptado:** un FBX sin animaciones llamado `anim_static.fbx` aparecería incorrectamente en Animations. El nombre es contractual con la convención del README — devs que ignoren la convención son responsables.

**Decisión clave 7 — `AI_SCENE_FLAGS_INCOMPLETE` ignorado en el standalone loader:** el chequeo original `if (... AI_SCENE_FLAGS_INCOMPLETE != 0 ...) abort()` se removió.

- **Razón:** assimp setea ese flag para FBX "Without Skin" porque no hay geometría — pero las animaciones están bien parseadas. El flag es informativo, no error. El check causó que los 6 clips iniciales devolvieran `[0 tracks, 0.00s]`. Quitar el check fue suficiente — `scene == nullptr` y `mRootNode == nullptr` siguen siendo guards reales.
- **Documentado in-line** en el cpp para que nadie reintroduzca el chequeo "porque el mesh loader lo tiene".

**Decisión clave 8 — Shipping de X/Y Bot demo rigs en el repo + `.gitignore` con excepciones:** los rigs Mixamo X Bot + Y Bot + sus 6 clips (~7.5 MB) se commitearon al repo.

- **Razón:** out-of-the-box working characters. Un dev clonando el repo tiene personajes funcionales sin tener que registrarse en Mixamo + descargar manualmente. El motor "tiene contenido" desde el primer clone.
- **`.gitignore` mantiene regla genérica `**/*.fbx`** (los FBX "With Skin + textures embedded" pueden pesar >100 MB, sobre el límite de GitHub) **pero exceptúa** `!assets/characters/player/*.fbx` y `!assets/characters/npc/*.fbx`. Devs que dropeen sus propios FBX en `hero/`, `enemy/`, etc., siguen gitignored.
- **Licencia:** X Bot / Y Bot son redistribuibles bajo el uso libre de Mixamo. README documenta esto.

**Alternativas descartadas:**

- **Texturas embedded del FBX al cargar:** los X/Y Bot se ven magenta sin texture map. Limitación pre-existente del `MeshLoader` (no extrae `aiScene::mTextures`). Queda en PENDIENTES — ataque cuando emerja necesidad real o cuando F2H50 lo pida visualmente.
- **Persistencia de `externalClips` en `.moodmap`:** deferred. Ningún demo lo usa todavía; F2H50 lo va a empujar cuando el NPC viva en escena persistida.
- **Animation state machines / blending (clip A → clip B con cross-fade):** scope creep para F2H49. F2H50/F2H51 si emerge necesidad real (ej. locomoción idle ↔ walk ↔ run).
- **`useRootMotion` flag por clip:** todos los clips de F2H49 son In Place (locomoción controlada por `CharacterController`). Si emerge cinemática o finishers con root motion, agregar como flag opcional por clip.
- **Bumping schema version de `.moodmap`:** `AnimatorComponent` no se serializa todavía con los campos nuevos. Cuando se sume persistencia (F2H50+), bumpear schema `.moodmap` aditivo.

**Condiciones de revisión:**

- Si el bind pass se vuelve hot path (N personajes × M clips × K frames), considerar skeleton fingerprinting (bone count + hash de nombres) para detectar mesh swap sin pedir al consumidor que lo gestione.
- Si emerge un caso donde el alias debería resolver dinámicamente (ej. múltiples versiones por género/edad: `walk_male`/`walk_female`), considerar un nivel de indirección — pero antes verificar si el caso real lo necesita.
- Si Mixamo decide cambiar la convención `mixamorig:*` o agregar prefijos extra, la búsqueda por `boneName` cae sin warning (todos los tracks quedan `-1`). Agregar un warn de "0 tracks bindeados" después del primer evaluate sería defensivo.

---

## 2026-05-10: F2H48.1 cierre — Serialización DialogComponent + DialogScriptHost (condition_lua reales pre-F2H49)

**Contexto:** tras cerrar F2H48 listé 4 deudas como "diferidas" (inyección hooks Lua reales, persist dialogVars en save, mouse-clic en choices, persist DialogComponent en .moodmap). El dev preguntó: *"Y NO DEBERIAMOS HACER ESTO ANTES DE LO DE LA DEMO DE MIXAMO?"*. Triage honesto en respuesta identificó 2 bloqueantes para una demo F2H49 creíble (serialización + hooks Lua) y 2 YAGNI legítimos (save vars, mouse-clic). El dev confirmó scope completo (B): *"me gustaria que la demo sea completa como la B"*. F2H48.1 es mini-hito (~3h, sin tag mayor — bump patch a `v1.36.1`).

**Decisión clave 1 — Sol::state dedicada del host (NO reusar la per-entity de ScriptSystem):** nueva `DialogScriptHost::g_state` (singleton namespace). Bindings minimal: `dialog` (vars + read-only state) + `hud` (interact_prompt + getters HP/mag/reserve) + `log`. Sin `self`, sin `physics`, sin `engine.exposed`.

- **Razón vs reusar la sol::state de un ScriptComponent específico:** los `condition_lua`/`on_select_lua` son del DIALOG, no de un entity. Si los corro contra el sol::state del NPC, las globales del script del NPC (`hp`, `pathFinding`, etc.) contaminan el sandbox del dialog — y peor: si el NPC NO tiene script, no hay sol::state contra qué evaluar.
- **Razón vs sol::state global por ScriptSystem:** ScriptSystem maneja una sol::state por entity (cada script Lua aislado, decisión F2H8 del plan original). Agregar una "global" rompe esa convención + mezcla responsabilidades.
- **Trade-off aceptado:** los dialog hooks NO pueden acceder a `self.transform` ni `engine.exposed` directamente. Si necesitan eso, el dev usa `dialog.set_var` desde el script del NPC + `dialog.get_var` desde el hook. Patrón clásico de event-bus con state intermedio.

**Decisión clave 2 — NO exponer `dialog.start/advance/continueNext/stop` desde el host:** las funciones de control de flujo del DialogSystem viven solo en la tabla `dialog` de los scripts de entity (LuaBindings.cpp F2H48 Bloque E), NO en el host del F2H48.1.

- **Razón:** si un `on_select_lua` llama `dialog.advance(1)` mientras estamos en medio de `DialogSystem::advance(0)` (el que dispara el hook), entramos en recursión + state machine inconsistente. El host debe ser READ + WRITE de vars, NO control de flujo.
- **Caso futuro:** si un dev pide "saltar al nodo X desde un on_select_lua" (skip directo sin link), agregar `dialog.jump_to(nodeId)` que difiere la transición al final del frame. v1 no lo necesita.

**Decisión clave 3 — Wrap defensivo `"return (" + expr + ")"`:** los `condition_lua` se escriben como expresiones (`dialog.get_var('x') == 'true'`) sin `return` explícito. El host wrappea automáticamente para que el dev no tenga que escribir `"return dialog.get_var(...)"` en cada choice.

- **Razón:** ergonomía. Las expresiones de condition leídas por humanos parecen condiciones (`a == b`), no statements (`return a == b`). Sol2 requiere `return` para capturar el valor.
- **Edge case:** si el dev escribe `"return x"` literal, el wrap produce `"return (return x)"` que es parse error → la choice no aparece (fail-safe loggea). Aceptable — esos casos son raros y el log dirige al dev.

**Decisión clave 4 — Fail-safe a `false` en errores de evaluate:** si el `condition_lua` tiene parse error o runtime error, la choice NO aparece (return false del evaluator). Log a `script` channel para que el dev vea.

- **Razón:** filosofía defensiva. Una choice con condition rota es probablemente "no debería estar visible". Mostrarla y permitir avanzar a un nodo basura es peor que esconderla. El warn en el log es alta señal: el dev ve el error inmediatamente.
- **Alternativa descartada:** mostrar la choice + ejecutar igual el `on_select_lua` rota cuando la elijan → side effects impredecibles. NO.

**Decisión clave 5 — `DialogScriptHost::reset()` NO toca `GameState::dialogVars()`:** la reset del host tira la sol::state (eventuales globals del juego que el dev haya creado) pero las vars del dialog persisten.

- **Razón:** consistencia con la decisión F2H48 #2 (vars sobreviven entre dialogs). Si un `Save/Load` del juego empuja vars al disco (deferred), reset del host NO debería borrarlas.

**Decisión clave 6 — `cachedDialogId` NO se persiste en `.moodmap`:** solo `dialogPath + autoStartOnInteract` van al JSON. El `cachedDialogId` arranca en 0 al load — el DialogInteractSystem llama `loadDialog(dialogPath)` la primera vez que el player entra al trigger.

- **Razón:** los IDs del AssetManager no son estables entre sesiones (depende del orden de loads). Persistir un ID viejo apuntaría a slots equivocados. Recompoer via `loadDialog(path)` es robusto + barato (cache hit tras el primer load).

**Decisión clave 7 — `dialogPath` vacío → NO se serializa el componente:** mismo patrón que `ScriptComponent::path`. Si el dev agrega un `DialogComponent` pero no le asigna path, el JSON queda limpio.

- **Razón:** evitar contaminación del .moodmap con componentes inertes. Un dev que olvida configurar el path no debería ver "dialog: {}" en el archivo.

**Alternativas descartadas:**

- **Persistir `dialogVars` en `.moodsave` como parte de F2H48.1:** YAGNI confirmado por el dev — la demo F2H49 no usa save/load. Sumar cuando emerja "guardia recuerda al cargar partida".
- **Mouse-clic sobre choices del HUD:** YAGNI — teclas 1-9 HL2-style son válidas estilísticamente. Sumar tras feedback de usabilidad post-F2H49.
- **Exponer físicas (`physics.raycast` etc.) en el host:** scope creep — los hooks de dialog NO deberían hacer raycasts. Si emerge caso real, agregar.
- **Cargar el host una vez por NPC en lugar de global:** complica + scope (init+teardown por NPC), sin ganancia (las vars son globales igual).
- **Persistir `cachedDialogId` directo:** rompe entre sesiones (IDs no son estables — depende del orden de loads del AssetManager).

**Condiciones de revisión:**

- Si un dev pide acceso a `physics.raycast` desde un `on_select_lua` (ej. "si el player está a < 2m del NPC, opción extra"), agregar el binding al host con cuidado de no abrir surface innecesario.
- Si emerge necesidad real de `dialog.jump_to(nodeId)` desde un hook (sin link), implementar con deferred transition (queue + flush al final del frame).
- Si los errores de `condition_lua` se vuelven comunes y el log es ruidoso, considerar un "modo estricto" toggleable que muestre las choices con error en rojo (para developer feedback durante demo time).
- Si un dev escribe `condition_lua` con efectos colaterales (`dialog.set_var(...)` dentro), el sandbox lo permite pero es mala práctica. Documentar en la convención del schema (no enforced).

---

## 2026-05-10: F2H48 cierre — Dialog runtime + HUD HL2-style + Lua bindings + DialogComponent

**Contexto:** continuación natural de F2H47 — el editor producía `.mooddialog` pero no había sistema que los interpretara en Play Mode. Bloque 2.3 + 2.4 del `PLAN_SUBFASE_2_5.md` fusionados (runtime y Lua bindings van de la mano). El dev confirmó 3 decisiones pre-implementación que abajo se detallan.

**Decisión clave 1 — DialogSystem como singleton namespace (no clase instanciable):** mismo patrón que `Mood::GameState::*`. El motor es single-threaded y una sola conversación activa a la vez basta para v1 (player no puede hablar con dos NPCs simultáneamente).

- **Razón vs clase con múltiples instancias:** simplifica el acceso desde cualquier punto (HUD widget, Lua bindings, DialogInteractSystem, char controller lock) sin pasarse el puntero. El patrón `GameState` ya está consolidado en el motor.
- **Trade-off aceptado:** si en el futuro el juego necesita "dialog en background con NPC X mientras player camina hacia NPC Y", habrá que refactorizar a clase con N instancias. v1 no lo necesita.

**Decisión clave 2 — `DialogSystem` sin deps de sol2 (callbacks `std::function`):** los hooks `LuaEvaluator/LuaExecutor/NodeEnterHook/ChoiceHook` son `std::function` inyectables. El módulo en `engine/dialog/DialogSystem.h/cpp` NO incluye `<sol/sol.hpp>`. Los tests headless lo prueban end-to-end sin Lua.

- **Razón:** mismo principio que `Graph` (F2H46) y `DialogAsset` (F2H47) — el state model es puro, la integración con Lua vive en `LuaBindings.cpp`. Testeable + se puede usar el sistema desde C++ directo (ej. tutorial in-engine que avanza dialogs sin script Lua).
- **Trade-off aceptado:** los `condition_lua`/`on_select_lua` declarados en los choices del DialogAsset NO se ejecutan en v1 — no hay sol::state inyectado. Los choices se asumen siempre disponibles. Para v2 se necesita una sol::state global del juego (deferred hasta caso real).

**Decisión clave 3 — Variables del dialog persisten en `GameState::dialogVars()`:** map global `unordered_map<string, string>` accesible desde Lua (`dialog.set_var/get_var/has_var/clear_vars`). Sobreviven entre dialogs.

- **Razón vs session-only (clear al stop):** habilita "el NPC recuerda que ya hablaste". Patrón clásico en RPG (Skyrim, Fallout, BG3): la state del mundo persiste entre conversaciones.
- **Persistencia en save:** **NO en v1** — vive solo en memoria. Sumar al `SaveLoad.cpp` cuando emerja necesidad real ("el guardia recuerda al cargar partida"). Sin urgencia.
- **Stringly-typed (value = string):** simplifica binding Lua + JSON serialization futuro sin variant. Si el script necesita int o bool, hace `tonumber()` / `s == "true"` — convención aceptable para v1.

**Decisión clave 4 — Trigger de start = `DialogComponent.autoStartOnInteract` + Lua escape hatch:** un NPC en escena con `DialogComponent` + `TriggerComponent` auto-muestra "[E] Hablar" cuando player entra al trigger; al apretar E, `DialogSystem::start` arranca con el `.mooddialog` apuntado. Como alternativa, `dialog.start("path")` desde Lua arranca cualquier dialog desde donde sea (cinemáticas, custom triggers, scripted events).

- **Razón vs (a) "solo Lua" o (b) "solo auto-trigger":** combinación cubre 80% de casos (NPCs estándar) con cero código + escape hatch para los casos custom. Patrón usado en HL2 (entity NPCs vs scripted_sequence).
- **`autoStartOnInteract=true` por default:** opt-out cuando el dev quiere control total via Lua.
- **`cachedDialogId` en runtime para evitar `loadDialog` por frame:** el primer trigger carga, el resto reusa. Reseteado cuando se cambia el path desde Inspector.

**Decisión clave 5 — HUD widget `dialog_box` HL2-style con choices numeradas 1-9 (no mouse clic):** caja inferior centrada (max 800px o 70% del ancho), NPC text con wrap, "1) opción / 2) opción" listadas en amarillo HL. Tecla 1-9 = `advance(idx)`. Tecla E (cuando no hay choices) = `continueNext`.

- **Razón vs clic con mouse:** simplifica v1 sin handling de hover/click del DrawList (el widget se renderea con `ImDrawList::AddText`, no es interactivo). Patrón HL2/Mass Effect Classic = teclado numérico. Si en el futuro el dev pide mouse clic, agregar `ImGui::IsMouseClicked + AABB test` sobre cada choice — incremento pequeño.
- **Cap visual 9 choices:** suficiente para 99% de los casos (típicamente 2-4). Si emerge un caso con 10+, falla por design — split en sub-dialogs.

**Decisión clave 6 — Char controller lock via flag `GameState::dialogActive()`:** WASD/jump/crouch ignorados mientras dialog activo. Mouse-look queda libre (el player puede mirar al NPC con naturalidad mientras conversa). Editor + Player ambos respetan el flag.

- **Razón vs (a) input completamente desactivado (mouse incluido) o (b) congelar physics entero:** mouse-look libre es ergonómico (jugadores quieren mirar al NPC sin perder agencia visual). Congelar physics entero rompe simulaciones de fondo (NPCs patrullando, partículas, etc.) — innecesario.
- **Reseteo en `GameState::reset()`:** al salir de Play Mode el flag se limpia automáticamente (preservación de invariantes del editor).

**Decisión clave 7 — Workspace Narrativa con viewport 3D (no NarrativeIntro placeholder):** pedido del dev tras tour visual de F2H47. Layout final 3 columnas: Viewport (3D) 30% izq / Dialog Editor 40% centro / col der vertical con Node Inspector arriba + Dialog Browser abajo. NarrativeIntro removido del default (sigue accesible via `Ver`).

- **Razón:** Sub-fase 2.5 va a producir NPCs reales (F2H49). El workspace puro de canvas era abstracto — el dev necesita ver el NPC en escena mientras edita su dialog (posicionar trigger, validar anim, etc.). Patrón estándar de editores narrativos (RenPy con preview, Inkle con scene viz).
- **Trade-off aceptado:** menos espacio para el canvas del Dialog Editor (40% vs 100% previo). Si los grafos crecen, el dev puede expandir el panel con drag — el layout es solo el default.
- **Persistencia por workspace:** cada workspace guarda su propio `iniLayout` en `captureCurrentLayout` al switch + restaura via `LoadIniSettingsFromMemory` al volver. Confirmado al dev: layouts independientes — cambios en Narrativa NO afectan a Layout.

**Decisión clave 8 — `DialogComponent` en popup Add Component (categoría Logic):** descubrible como componente normal del Inspector, junto a Script + NavAgent. Descripción i18n: "Linkea un asset .mooddialog; auto-arranca al interactuar el player si hay un Trigger."

- **Razón:** sin esto, asignar dialog a una entity requeriría Lua o hacks. El popup Add Component es el flujo discoverable estándar.
- **Categoría Logic:** vive con Script + NavAgent — todos cubren behavior/data del entity vs Render/Physics/Audio/World.

**Alternativas descartadas:**

- **DialogSystem como clase con N instancias:** YAGNI v1 (no hay caso de uso).
- **Inyectar `LuaEvaluator/LuaExecutor` desde una sol::state global ya en F2H48:** scope creep — requiere crear infra de sol::state global del juego compartida (no existe hoy). Deferred hasta que un caso real lo pida (probablemente F2H50+ con inventory + quest condition predicates).
- **Persistir `dialogVars` en `.moodsave` desde F2H48:** YAGNI — el demo de F2H49 no lo necesita. Sumar cuando emerja.
- **Mouse clic sobre choices:** complica el v1 sin valor crítico. Teclado numérico es estándar HL2.
- **Workspace con tabs Viewport+Dialog en center:** menos usable que side-by-side. El dev prefiere ambos visibles.
- **Inspector general en Narrativa:** no entra — el workspace se enfoca en canvas + 3D. El dev puede abrir Inspector via `Ver` si lo necesita puntualmente.

**Condiciones de revisión:**

- Si emerge necesidad de dos conversaciones simultáneas (NPC patrullaje + cinemática), refactorizar DialogSystem a clase con N instancias.
- Si los `condition_lua`/`on_select_lua` empiezan a tener uso real en demos/juegos del dev, inyectar `LuaEvaluator/LuaExecutor` con sol::state global dedicada.
- Si el dev valida un caso donde `dialogVars` deben sobrevivir entre Save/Load (guardian recuerda al cargar), agregar serialización al `SaveLoad.cpp`.
- Si emerge un grafo de dialog con 10+ choices en un solo nodo, agregar overflow (scroll en el HUD) o forzar split en sub-dialogs (decisión de pipeline).
- Si el dev pide mouse-clic sobre choices en el HUD (más ergonómico para gamepad? touch?), agregar handler de `IsMouseClicked + AABB test` por choice.
- Si el workspace Narrativa con viewport 3D se siente apretado en monitores chicos (<= 1600px), reconsiderar tabs en center.

---

## 2026-05-10: F2H47 cierre — Dialog Editor (autoría: schema + visual editor + inspector + browser)

**Contexto:** Segundo hito real de Sub-fase 2.5 (Bloque 2 del plan `PLAN_SUBFASE_2_5.md`). Construye el primer editor de contenido sobre la infra del node-graph framework de F2H46. Entrega herramienta de autoría completa end-to-end (schema + asset + 3 paneles del editor + sample demo) pero NO el runtime — el state machine del dialog que corre en Play Mode + HUD widget + Lua bindings + DialogComponent quedan para F2H48. Split editor/runtime deliberado para validar el schema antes de atar el runtime.

**Decisiones técnicas clave:**

- **Schema v1 con un solo tipo de nodo (`dialog_line`).** Razón: simplicidad — los hooks `condition_lua` y `on_select_lua` por choice cubren el 80% de los casos prácticos. Tipos `condition` (nodo de branching puro), `action` (nodo de hook sin texto), `jump` (saltar a otro `.mooddialog`) emergen como necesidad real cuando un caso de juego concreto los pida. Alternativa descartada: incluir los 4 tipos en v1 — schema más rico pero la mayoría no se usaría, y agregar nodos en v2 es non-breaking.

- **Auto-sync invariante: N choices == N output sockets.** Razón: el dev edita choices en el Inspector contextual; el grafo debe reflejar visualmente que un nodo con 3 opciones tiene 3 outputs (uno para cada destino). Si el dev tuviera que agregar/borrar sockets manualmente en paralelo al array de choices, sufriría desync errors. Implementación: `Asset::writeLine()` re-crea sockets para matchear `choices.size()` (o exactamente 1 "continue" si vacío), borrando los sobrantes (cascade de links incidentes via `Graph::removeSocket` que tuve que agregar como extensión a F2H46). Alternativa descartada: dejarlo manual + validator que detecte el desync — propenso a frustrar al dev con errores constantes.

- **Toggle text_key vs text_literal por línea (y por choice).** Razón: prototyping flow real — el dev escribe el texto inline al armar el diálogo y promueve a i18n key después cuando el contenido se estabiliza. Forzar i18n key desde el día 1 friccionaría el prototyping; forzar literal eliminaría la opción de localización. El toggle deja al dev elegir cuándo promover. UI: checkbox "Usar i18n key" en el Inspector — al togglear, el valor previo se mueve al otro campo (no se pierde). Alternativa descartada: dos campos visibles siempre — clutter visual y ambiguo cuál gana en runtime.

- **Cycles permitidos pero reportados como Warning (no Error).** Razón: algunos diseños de diálogo (loops controlados por flags Lua, "el NPC vuelve al menú principal hasta que el jugador elija X") usan ciclos intencionalmente. Bloquearlos como Error frustraría esos casos. Reportar como Warning informa al dev sin impedir guardar. Detección via DFS iterativa con coloring blanco/gris/negro (back edge = ciclo). Alternativa descartada: análisis estático que descarte ciclos "escapables" — feature de v2 cuando los ciclos sean comunes.

- **Save explícito v1 (no auto-save).** Razón: el dev necesita control durante experimentación — auto-save sobrescribe el archivo mientras el dev prueba ideas que quizás revertirá. Auto-save con debounce + undo a-disco es feature de polish para v2.

- **Path absoluto `<cwd>/assets/dialogs/` para el Browser.** Razón: convención del editor existente (Material Editor F2H42 usa `<cwd>/assets/materials/`). El cwd se setea al directorio del proyecto cuando se abre uno; reusar el patrón existente evita inventar abstractions del filesystem específicas para Dialog.

- **Sample demo generado programáticamente en lugar de shipped como archivo.** Razón: hacer commit de un `demo_intro.mooddialog` agregaría un asset al repo con texto pre-cocinado, que después habría que mantener traducido + sincronizado con cambios de schema. Generarlo en `processSpawnDialogDemoRequest()` (DemoSpawners_Basic.cpp) usa el API real del Asset y queda automáticamente up-to-date con cualquier evolución del schema. El archivo se persiste a disco la primera vez para que reabrir el demo no lo regenere (el dev podría haberlo editado y queremos preservar sus cambios).

- **Split editor (F2H47) vs runtime (F2H48).** Razón: checkpoint de validación. Si encerramos editor + runtime en un solo hito mega, cambios al schema durante el desarrollo del runtime requerirían re-trabajar el editor. Con split, F2H47 estabiliza el schema (con tests), F2H48 implementa el runtime sabiendo qué consume. Aceptado por el dev pre-implementación.

- **Window IDs estables (sin `###` ni i18n) para que matchee DockBuilder.** Lección de F2H46 aplicada: el window name pasado a `ImGui::Begin` es lo que `DockBuilderDockWindow` hashea. Si el title se traduce, el hash cambia y el dock no se aplica. Convención: `name()` de cada panel retorna inglés estable; el contenido del panel sí es i18n.

- **Inspector contextual como panel separado (no inline en Editor).** Razón: separación clara de concerns — el Editor maneja el grafo, el Inspector maneja los campos del nodo seleccionado. Beneficio adicional: el Inspector puede docked a la derecha del Editor (consistente con el flujo Inspector/Hierarchy del editor general). Alternativa descartada: campos del nodo dentro de un ImGui::CollapsingHeader en el Editor mismo — agrega clutter al canvas y ocupa espacio que va a ser premium cuando los grafos crezcan.

- **Sandbox queda como Debug-only, NO default del workspace Narrativa post-F2H47.** Razón: el Sandbox era placeholder de F2H46; con F2H47 el workspace tiene un editor real. Mantener el Sandbox visible confundiría al dev sobre cuál es la herramienta principal. Sigue accesible vía Ver > Debug para inspección del framework subyacente — utilidad permanente para futuros devs que extiendan node-graph.

**Alternativas descartadas explícitamente:**

- **Tipos de nodo `condition`/`action`/`jump` en v1**: descartado por scope. Hooks Lua en choices cubren los casos prácticos.
- **Voiceover sync (highlight de palabras con audio timing)**: descartado por scope. customData ya tiene `audio` path; v2 puede agregar timing data si emerge necesidad.
- **Animation wait_for flag**: el customData tiene `animation` pero v1 solo dispara; no espera a que termine. Polish de v2.
- **Theming custom del grafo (paleta narrativa)**: herencia de F2H46 sin urgencia hasta que el grafo se vuelva confuso visualmente.
- **String tables dedicadas para gameplay (vs reusar i18n actual)**: descartado en v1. Las keys de dialog viven en el mismo `assets/i18n/{en,es}.json` que el editor — funciona, no agrega infra.
- **AssetManager::loadDialog**: descartado para F2H47. Se agrega en F2H48 cuando el runtime necesita resolver paths logicos para DialogComponent. F2H47 usa I/O directo de filesystem.

**Condiciones de revisión:**

- Si emerge necesidad real de tipos `condition`/`action`/`jump`: agregar en F2H48 o hito propio antes de que los devs externos los pidan ad-hoc.
- Si auto-sync de choices causa pérdida de links accidental (el dev borra un choice y pierde el link a un nodo importante): considerar agregar warning de confirmación antes del remove en el Inspector.
- Si el Browser con muchos diálogos (>50) se vuelve lento al refresh: paginar o filtrar.
- Si emerge demand por preview en-vivo del diálogo dentro del editor (sin entrar a Play Mode): agregar como feature de F2H48 una vez que el runtime exista.

---

## 2026-05-10: F2H46 cierre — node-graph framework (integración `imgui-node-editor`) + workspace "Narrativa"

**Contexto:** Primer hito real de Sub-fase 2.5 (Bloque 0.1 del plan `PLAN_SUBFASE_2_5.md`). Pre-requisito de los Dialog Editor (F2H47) y Quest Editor (F2H4X) que vienen — comparten arquitectura de grafo, implementar UN framework reutilizable evita duplicación. Estimación inicial ~8h en 8 bloques (A-H); realizado en ~1 sesión con 3 fixes técnicos reactivos al feedback del dev durante validación visual.

**Decisiones técnicas clave:**

- **Lib externa `thedmd/imgui-node-editor` vs framework propio.** Decisión confirmada con dev pre-implementación: usar la lib. Razones: ~10x menos esfuerzo (~1 hito integración vs ~3-5 propio), battle-tested (usado en herramientas Unreal-like), header-only-ish, license MIT. Trade-off: control limitado del look-and-feel (theming custom es feature futura), dependencia externa nueva (aceptable — sigue el patrón de imgui/glm/EnTT/Lua/Jolt/etc en `_deps/`). Alternativa descartada: framework propio sobre ImGui DrawList — control total pero scope explotaría fácil con feature creep (animaciones, snap, multi-select, etc) que la lib ya da gratis.

- **Pinned a commit master post-v0.9.3** en lugar del tag v0.9.3 (Aug 2023). Razón: v0.9.3 usa APIs ImGui ya removidas en docking branch actual (`ImRect::Floor`, `ImGui::GetKeyIndex`). Master tiene los fixes. Cuando aparezca v0.9.4+ tag estable, migrar — por ahora pin a hash 2025-era para reproducibilidad. Documentado en CMakeLists.txt.

- **Patch idempotente al header de la lib via CMake `file(READ/WRITE)` en configure-time.** `imgui_extra_math.inl` define `operator==/!=/operator*(float, ImVec2)` para ImVec2 que colisionan con el ImGui docking branch que ya los provee (`imgui.h:3038-3054`). Patch envuelve el bloque problemático en `#ifndef IMGUI_DEFINE_MATH_OPERATORS_IMPLEMENTED` con guard `MOOD_NE_OPS_GUARD`. Idempotente — re-config detecta el guard y no reaplica. Alternativa descartada: fork del repo + maintenance overhead, o aplicar patch via `PATCH_COMMAND` en CPM (más frágil con git apply en Windows). El `file(REPLACE)` directo es portable + idempotente + auto-documentado en el log de configure.

- **Separación dura state vs rendering: `engine/nodegraph/Graph.h` puro + `NodeGraphEditor.h` con pImpl.** Razón: Dialog/Quest van a serializar grafos a JSON. La serialización no debe depender de ImGui. Tests unitarios del data model no necesitan ImGui linkeado en `mood_tests`. Mismo patrón que F2H39 GameState ↔ GameOverlay. El pImpl en `NodeGraphEditor` evita que clientes del header incluyan `imgui_node_editor.h` ni ImGui — solo `Graph.h`. Alternativa descartada: una sola clase mezclando state + draw — perdés testabilidad y el header se contamina.

- **IDs propios `u32` con `k_invalid = 0` como sentinel, monotonicos sin reuso post-delete.** Razón: simpler than tombstones / generational indices; `u32` cabe en el `uintptr_t` que usa la lib internamente para sus NodeId/PinId/LinkId (cast directo en helpers). Generadores `m_next*Id` se serializan en JSON para que cargar un grafo + agregar nuevos nodos siga IDs frescos (no reusa IDs de nodos borrados). Alternativa descartada: indices en arrays — frágil ante deletes y reordenamientos.

- **5 reglas de `canConnect` estrictas v1.** (1) ambos sockets existen, (2) kinds correctos (Output → Input, no Output→Output ni Input→Output), (3) typeTag idéntico (sin coerciones / casts entre tipos), (4) no duplicar link existente con mismo from+to, (5) un input acepta solo 1 link entrante (outputs pueden fan-out a N). Razón: los editores específicos (Dialog/Quest) van a definir su propia taxonomía de typeTag — coerciones automáticas en el framework crearían ambigüedad. v2 podría agregar matriz de compatibilidad si emerge necesidad real. Test coverage exhaustivo de las 5 reglas en `test_nodegraph.cpp`.

- **`draw()` no-mutating + retorna `std::vector<EditorEvent>`.** Razón: desacopla el wrapper del sistema de undo/redo. El caller (Sandbox panel, eventualmente Dialog/Quest Editor) traduce eventos a Commands y los pushea al `HistoryStack` para undo. Alternativa descartada: mutar el grafo dentro de draw — el wrapper necesitaría conocer HistoryStack, agregando dependencia transitiva. Eventos suficientes para v1: NodeMoved, NodeDeleted, LinkCreated, LinkDeleted. AddNode no es event (la lib no dispara "create node" implícito — el caller lo dispara explícito via botón/menu y construye directo el `AddNodeCommand`).

- **Sync `graph → lib` unconditional cada frame cuando `!ne::IsActive()`.** Razón: garantizar que un undo de MoveNodeCommand revierta la pos visualmente. La lib mantiene su propio storage de posiciones; si después de `Command::undo()` el graph.position cambia pero la lib sigue mostrando la pos vieja → bug visual. La gate `!IsActive()` evita override durante un drag activo (en ese caso la lib es source of truth temporal hasta drag-end). Alternativa descartada: track explícito de "graph mutated externally" con dirty flags — más invasivo y propenso a perder un evento. Polling con epsilon-compare es trivial.

- **Window titles de paneles nuevos = `name()` directo (English estable, sin `###StableID`).** Razón: convención con el resto del editor (Inspector, Console, Performance — ninguno traduce su title bar). `DockBuilderDockWindow(window_name)` hashea el string completo — debe matchear con lo que `ImGui::Begin` pasa. Si usaramos `"Título###StableID"` traducido, las traducciones romperían el hash → dock no se aplicaría. Solución: title fijo, contenido traducido. Decisión post-fix tras dev reportar "ambos siguen flotando" — el ###suffix initial design no matcheaba con DockBuilder lookup.

- **Workspace "Narrativa" SIN paneles de scene/3D.** Cita del dev (post-validación 1): *"si se trata de narrativa no tiene sentido que muestre paneles que no tienen sentido"*. Workspace contiene SOLO `NodeGraphSandbox` (izq 70%) + `NarrativeIntroPanel` (der 30%). Inspector, Asset Browser, Console quedan accesibles desde Ver pero no son default. Razón: el workspace tiene que ser dedicado a su tarea, no un cajón de sastre.

- **`io.ConfigDebugHighlightIdConflicts = false` global.** Razón: el debug-check nuevo de ImGui 1.92 da false-positives con imgui-node-editor (la lib manipula su propio ID stack internamente). Nuestra suite de 8580 assertions cubre lo que el check detectaría en código nuestro. Desactivar globalmente vs por-panel: el flag se chequea cada widget submission, suprimirlo local fue insuficiente porque la alarma se dispara en otra fase del frame. Trade-off aceptado: perder el check de debug para code real nuestro — los tests + revisiones del agente mitigan.

- **`NarrativeIntroPanel` como placeholder explicativo.** Cita del dev al ver el Sandbox por primera vez: *"como esto conecta con el 3D, como creo yo conversaciones o que puedo crear con esto, no entiendo aun"*. Sin un panel que explique que F2H46 es **infra** y los editores reales vienen en F2H47+, el workspace parece roto / abstractamente inútil. Panel con título en amarillo HL + body explicando + roadmap de F2H47/F2H4X/F2H4Y + footer engine-grade philosophy. Se reemplaza/oculta cuando los editores reales aterricen — temporal pero crítico para handoff a la próxima sesión / dev nuevo.

**Alternativas descartadas explícitamente:**

- **Framework propio sobre ImGui DrawList**: descartado pre-implementación. Justificado por scope (~3-5 hitos extra) sin ganancia clara para v1.
- **v0.9.3 tag estable de imgui-node-editor**: descartado tras encontrar APIs ImGui removidas. Master post-fixes es la opción viable hasta que aparezca v0.9.4+.
- **Coerciones automáticas entre typeTags de sockets**: descartado en v1. Tipos estrictos forzan decisiones explícitas del dev del editor específico (Dialog/Quest). Agregar coerciones más tarde si emerge necesidad.
- **Sync bidireccional lib ↔ graph cada frame**: descartado. Source of truth claro (graph siempre, lib durante drag). Bidireccional crearía race conditions.
- **Theming custom del grafo en F2H46**: descartado por scope. El default de la lib es funcional; theming a la paleta Valve es feature de polish — se agrega cuando los Dialog/Quest Editor lo pidan.
- **Menu contextual para "Add Node" en el canvas**: descartado en v1. Los botones del toolbar (+Source/+Process/+Sink) cubren v1; el contextual menu es feature de ergonomía que aterriza con Dialog Editor cuando los tipos de nodo emerjan.

**Condiciones de revisión:**

- Si la lib master rompe build en update futuro: pinear a commit anterior verificado, o evaluar fork.
- Si el patch al `imgui_extra_math.inl` falla por cambio upstream del header (renombran o cambian estructura del bloque): adaptar el `string(REPLACE)` o eliminar el patch (la versión upstream eventualmente arregla el conflicto en su lado).
- Si emerge necesidad de coerciones entre typeTags (ej. socket "int" conecta a "float"): agregar matriz de compatibilidad en `canConnect`.
- Si la performance con grafos grandes (>500 nodos) se vuelve cuello: profilear, considerar lazy rendering / culling fuera del viewport.
- Si los Dialog/Quest Editor descubren que el evento-based dispatch del wrapper es restrictivo (ej. necesitan interceptar drag mid-action): agregar callbacks adicionales en NodeGraphEditor, sin romper la API de `draw()`.

---

## 2026-05-10: Sub-fase 2.5 — commitment estratégico (scope Pro Tools + filosofía engine-grade)

**Contexto:** Post-F2H45 el dev confirma Sub-fase 2.5 (diálogos / quests / inventario) como next-up. Pre-implementación arranca conversación estratégica: el dev quiere alineamiento del agente con la filosofía del motor antes de tocar código. Esta entrada documenta los compromisos de diseño que aplican a TODA la sub-fase (no a un hito específico) y que cualquier decisión técnica futura en la sub-fase debe respetar. **NO es decisión de implementación** — es marco de diseño no-negociable que precede a la implementación.

**Decisiones estratégicas clave:**

- **Filosofía: motor que crea juegos, no juego concreto.** Cita verbatim del dev: *"este sistema que haremos ahora es el más importante, me refiero a que debe ser la base para que a futuro cualquier dev al crear su juego pueda utilizar este sistema de inventario, quests, y diálogos, para sus juegos, osea debe ser versátil y realmente amigable de usar... aún no estamos creando un juego, estamos creando el motor que va a crear juegos"*. **Implicación dura**: cualquier decisión que solo encaje con un género (RPG / shooter / walking sim / metroidvania) o que asuma una semántica de gameplay (XP / mana / HP como conceptos del motor) es **bug de diseño** y debe rechazarse. El motor solo conoce contenedores genéricos; el dev del juego registra los recursos y semántica que su juego usa.

- **Scope nivel B (Pro Tools), no nivel A (mínimo viable).** Cita verbatim: *"B, aunque nos lleve días"*. Significa: Dialog Editor como node-graph visual interconectado (estilo Unreal Blueprints / `ink`), Quest Editor como flowchart, Item Browser con 3D preview rotable. **NO** listas de nodos con dropdowns, **NO** lista plana de objectives, **NO** Asset Browser plano. **Costo aceptado**: la sub-fase pasa de ~3 hitos (scope A en `PLAN_FASE2.md:285-303`, ahora obsoleto) a ~15-20 hitos. El dev explícitamente acepta el costo en tiempo a cambio de calidad engine-grade.

- **8 principios de diseño no-negociables** (extraídos de la conversación y aplicables a TODOS los hitos de la sub-fase):
  1. **Data-driven, no code-driven**: NPCs / quests / items se crean sin tocar código del motor. Todo es asset visualmente editable (`.mooddialog`, `.moodquest`, `.mooditem`).
  2. **Sin semántica hardcodeada de gameplay**: API genérica `stats.add("xp", 100)` donde `"xp"` es string que el dev registra, no un campo `stats.xp` del motor.
  3. **Hooks Lua sobre primitivas**: `on_quest_complete(id, callback)` — el callback hace lo que ese juego quiera. Patrón ya consolidado con triggers + scripts.
  4. **Default + override en rendering**: widget HUD default funcional + `dialog.set_renderer(lua_callback)` para juegos con HUD propio.
  5. **Composabilidad con sistemas existentes**: quest objectives son predicados genéricos contra estado del motor (`item_count`, `flag_set`, `area_entered`, `counter_at_least`). No tipos específicos como "kill 10 wolves".
  6. **Editor visual real, no JSON a mano**: cada sistema tiene su panel de editor visual. Si el dev abre `.json` crudo para crear contenido, fallamos.
  7. **State vs rendering separados**: patrón F2H39 `GameState` ↔ `GameOverlay` — lógica en módulos puros sin deps gráficas → `mood_tests` los testea sin ImGui. Aplica a Dialog/Quest/Inventory state.
  8. **Engine-agnostic respecto al género**: si la API solo sirve para RPG o solo para shooter, es bug.

- **Orden propuesto de bloques** (pendiente confirmar con dev al arrancar la sub-fase): Bloque 0 (infra compartida: node-graph framework + 3D preview widget) → Bloque 1 (inventario) → Bloque 2 (diálogos) → Bloque 3 (quests) → Bloque 4 (soporte producción: stats genéricos + save v3 + template/sample project + docs developer-facing). **Recomendación inventario primero**: diálogos y quests dependen de "tengo X item" para condicionales — empezar por items evita hardcodear flags `game.has_key` que después hay que migrar. Detalle completo en [`PLAN_SUBFASE_2_5.md`](PLAN_SUBFASE_2_5.md).

- **Bloque 0 (infra compartida) primero, no por dentro de cada sistema.** Razón: Dialog Editor + Quest Editor van a compartir arquitectura de node-graph. Implementar UN framework reutilizable (pan/zoom/snap, sockets, conexiones, save/load) y consumirlo desde los 2 editores. Alternativa descartada: implementar diálogos primero con su propio grafo, refactor cuando llega quests — más rework. Mismo razonamiento para el 3D preview widget (Item Browser ahora + Material Editor pro version después).

- **Decisión técnica abierta para el Bloque 0**: node-graph framework propio sobre ImGui DrawList vs adoptar `imnodes` / `imgui-node-editor`. Pros del propio: control total sobre look-and-feel + integración estética con el resto del motor + no agregar dependencia. Pros de adoptar: ~10x menos esfuerzo + battle-tested. **Pendiente evaluar al arrancar Bloque 0** — documentar pros/cons con código de muestra antes de elegir.

**Alternativas descartadas explícitamente:**

- **Scope A (mínimo viable)**: descartado por el dev. *"B, aunque nos lleve días"*.
- **Implementar los 3 sistemas en paralelo**: descartado por scope. Cada uno es un proyecto considerable; secuencial permite que las decisiones del primero informen al siguiente (ej. cómo se hace el editor visual del inventario informa cómo se hace el de diálogos).
- **Empezar por diálogos**: descartado. Items condicionan diálogos via predicados — si arrancamos por diálogos hardcodearíamos flags que después hay que migrar al sistema de inventario real.
- **Hardcodear "XP", "level", "mana" como campos del Stats system**: descartado. Cada juego tiene su modelo (XP / soul / experiencia / nada). Genérico key-value.
- **Diferir el editor visual para una v2**: descartado. Sin editor visual el sistema NO es engine-grade. Si el dev tiene que tocar JSON crudo para crear NPCs, fallamos.

**Condiciones de revisión:**

- Si en cualquier punto de la sub-fase emerge una decisión técnica que viola alguno de los 8 principios, **detenerse** y pedir confirmación al dev. Los principios son no-negociables salvo intervención explícita.
- Si el scope estimado de ~15-20 hitos resulta muy bajo (ej. el node-graph framework solo toma 4-5 hitos), no recortar features — extender la sub-fase. El dev priorizó calidad sobre velocidad.
- Si emerge un caso donde un género específico (ej. shooter sin diálogos) no necesita un sistema, eso no justifica simplificar el sistema. La opcionalidad ya está garantizada (un juego no usa un sistema que no enchufa) — la generalidad sigue siendo necesaria.
- Cuando se cierre cada hito de la sub-fase, anotar en `DECISIONS.md` las decisiones técnicas específicas, pero referenciar esta entrada como marco general.

---

## 2026-05-10: F2H45 cierre — deudas pre-Sub-fase 2.5 (AddComponentCommand undoable + Lato Player con tildes + Console tooltip i18n)

**Contexto:** tras cerrar F2H44, el dev pidió revisar `PENDIENTES.md` para clasificar deuda real vs feature diferida y arreglar las deudas reales antes de arrancar Sub-fase 2.5: *"no me gustan las deudas futuras, quiero arreglar lo necesario ahora antes de pasar a otra cosa"*. Tres deudas identificadas como reales (consistencia/sweep incompleto): (1) AddComponentCommand sin undo (toda otra acción del Inspector es undoable, romper esa expectativa para Add Component generaba dissonance), (2) font del MoodPlayer atrasada en ProggyClean desde F2H38 + por consecuencia `es.json` sin tildes desde F2H43 (paridad forzada), (3) Console tooltip multilínea con icons FA inline que el subagente de F2H43 dejó sin envolver. Otras ~12 entradas del PENDIENTES.md fueron clasificadas como features diferidas (no son deuda) y NO entraron al hito.

**Decisión clave 1 — AddComponentCommand type-erased en lugar de N templates específicos:** el comando vive como una sola clase `AddComponentCommand` con dos `std::function<void(Entity&)>` (`add` + `remove`) que el helper templated `makeAddComponentCommand<T>(entity, label)` rellena capturando T en las closures. El callsite del popup pasa de `[e]() mutable { e.addComponent<X>(); }` a `[](Entity en, std::string lbl) { return makeAddComponentCommand<X>(en, std::move(lbl)); }`.

- **Razón vs `template<T> class AddComponentCommand`:** una clase templated generaría 11 instanciaciones del comando completo en el binario (con sus virtuales). Type-erase es 1 clase + 11 sitios de captura templated triviales (~3 LOC cada uno). El cost extra de los `std::function` es ~32 bytes por comando + heap alloc de la closure — irrelevante para una operación que el dev hace raramente vs los sliders del Inspector.
- **Razón vs implementar como `EditPropertyCommand<bool>` con setter que add/remove:** EditPropertyCommand está pensado para edits de UN campo de UN componente existente. Crear/destruir el componente entero es semánticamente otro animal — un comando dedicado tiene mejor `name()` ("Agregar componente: Luz") y separa el log del edit del log del add.
- **Sin snapshot del estado pre-add:** si el dev hace add → edita campos del componente → undo, los EditPropertyCommand de los edits quedan más arriba en el stack y se deshacen primero (LIFO). El AddComponentCommand solo necesita add/remove con valores default — los campos editados quedan capturados en sus propios commands. Probado en `test_add_component_command.cpp` con la interacción Add+Edit+Undo.
- **Trade-off aceptado:** redo de un Add (tras undo) recrea con valores default. Si el dev había editado campos en el ciclo original, los EditPropertyCommand del redo los re-aplican en orden — funciona correctamente, pero requiere que la HistoryStack mantenga los edits posteriores en el redo stack (lo hace por design — push() limpia el redo, pero un solo undo+redo no toca el redo).

**Decisión clave 2 — Lato en Player con mismo patrón que F2H38, sin FA merge:** el MoodPlayer ahora carga `LatoLatin-Regular.ttf` 15px en `PlayerApplication_Init.cpp` con range Basic Latin + Latin-1 Supplement + General Punctuation subset. FA (FontAwesome) NO mergeada al atlas.

- **Razón sin FA:** el HUD del Player usa DrawList procedural (F2H39+) — círculos, barras, anillos, hexágonos calculados con math, sin sprites ni iconos. El Pause menu del Player no usa FA tampoco (validado con grep `ICON_FA` en `src/player/`). Cargar FA agregaría ~400KB al atlas para nada. Si en el futuro un menú del Player necesita FA (improbable), agregarlo en su momento.
- **Razón mismo size 15px que el editor:** coherencia visual al transicionar Editor → Play Mode. Pre-F2H45 había salto de tamaño + estilo (ProggyClean 13px bitmap → Lato 15px smooth) que era inconsistente.
- **Sweep tildes habilitado:** con Lato cargado, los `á/é/í/ó/ú/ñ` rendean correctamente. Subagente delegado para sweep mecánico de ~75 palabras corregidas en ~60 valores de `es.json` (fundamentalmente sustantivos `acción/selección/configuración`, palabras del HUD `MUNICIÓN/MENÚ`, `Diseño/Español/Añadir/tamaño`). Términos técnicos en inglés (`Inspector/Hierarchy/Brush/MeshRenderer/workspaces`) intactos por convención del motor.

**Decisión clave 3 — Console tooltip: keys i18n para texto, icons FA en código:** el tooltip de leyenda log levels en `ConsolePanel.cpp` ahora se construye con `std::string` concat en C++:

```cpp
const std::string body =
    I18n::T("editor.panel.console.help.header") + "\n"
    "  " ICON_FA_BUG " " + I18n::T("editor.panel.console.help.level.trace") + "\n"
    ... (5 lines más) ...
    "\n" + I18n::T("editor.panel.console.help.footer");
ImGui::SetTooltip("%s", body.c_str());
```

- **Razón vs poner los icons en el JSON:** los macros `#define ICON_FA_X "\xef\x86\x88"` son secuencias UTF-8 cuyos bytes son válidos en JSON (después de `JSON.parse`), pero serían frágiles a editar y a la vista del traductor son tofu. Mantenerlos en código asegura que el traductor solo edita texto plano (la parte que SÍ entiende su idioma) y que un eventual cambio de los iconos FA no requiere tocar el JSON.
- **Patrón replicable:** este approach (1 key por segmento de texto + concat con bytes literales en código) es la solución estándar para cualquier tooltip / mensaje con icons inline. Quedó documentado en `_comment` de los JSONs como precedente.

**Decisión clave 4 — botón "Recompute mesh" eliminado, no escondido en grupo "Debug avanzado":** a pedido del dev tras tour visual del Inspector. Era debug breadcrumb del Hito 12+ que forzaba `bc.dirty=true` por si alguna mutación del brush no marcaba dirty automáticamente.

- **Razón vs esconderlo:** ninguna mutación del brush deja `dirty=false` por error en práctica (validado por 50+ commits desde F2H12 sin reportes). Esconderlo en un TreeNodeEx("Debug") agrega ruido visual y mantiene código muerto. Eliminar es más limpio.
- **Recuperable si emerge necesidad:** el campo `bc.dirty` sigue público; si en el futuro un developer necesita el escape hatch, puede agregarlo en una rama de debug local con 4 LOC.

**Alternativas descartadas:**

- **Lua scripts traducibles:** identificada como deuda real pero descartada del scope F2H45 — solo aplica a demos actuales (`hud_demo.lua`), no hay scripts gameplay reales. Esperar a que emerjan.
- **Validación full Player con compiledMesh:** no es deuda, es validación pendiente al primer empaquetado real del dev. Sin código que escribir hoy.
- **Implementar AddComponentCommand templated por cada T (sin type-erase):** scope inflado innecesariamente (11 instanciaciones × ~80 LOC cada una = ~900 LOC vs ~80 LOC del header-only type-erased actual).
- **Cargar FA en Player por si acaso:** YAGNI. El HUD procedural no lo necesita; agregar peso al atlas sin uso real es deuda preventiva.
- **Esconder "Recompute mesh" detrás de un toggle "Debug avanzado" en lugar de eliminar:** agrega complejidad de UI por código que nunca se usó. Eliminar > esconder.

**Condiciones de revisión:**

- Si emerge un caso real donde un brush queda con mesh stale sin marcar `dirty=true`, recuperar el botón "Recompute mesh" como single-LOC (`if (ImGui::Button("Recompute")) bc.dirty = true;`) — no requiere reabrir el hito.
- Si el dev percibe overhead notable de los `std::function` en AddComponentCommand al hacer add masivos (improbable — add no es operación de tight loop), refactorizar a templated por T.
- Si en el futuro se agrega un componente que NO sea default-constructible (improbable, todos los actuales lo son), `makeAddComponentCommand<T>` falla en compilación — requeriría sobrecarga adicional con args explícitos.
- Si el subagente cometió errores en el sweep de tildes que solo se ven en uso real (ej. tildes innecesarias en una palabra técnica que el dev espera sin tilde), corregir esa palabra puntual con `Edit` — no requiere hito propio.

---

## 2026-05-10: F2H44 cierre — polish onboarding UX (sin docs)

**Contexto:** tras cerrar F2H43 (i18n), el dev pidió evaluación crítica honesta de la UI desde la perspectiva de un dev de juegos nuevo. La auditoría identificó 5 gaps de descubribilidad/UX que limitaban la primera impresión del editor: (1) workspaces con nombres ambiguos al usuario nuevo, (2) sin "Add Component" en Inspector — el dev solo podía agregar componentes via demos hardcoded del menú Help, (3) demos enterrados en `Ayuda > Demos` (primera vista del editor era dockspace vacío), (4) VisGroups sin onboarding contextual del concepto Hammer/Source, (5) outline AABB de meshes seleccionados invisible (cubo unitario hardcoded que se perdía dentro de meshes grandes como Fox.glb). Bloque originalmente planeado #5 (USER_GUIDE/* + README + GIF) fue descartado por el dev: *"seguiremos agregando cosas que luego vamos a terminar cambiando"* — docs externos rotarían más rápido que la implementación.

**Decisión clave 1 — outline AABB usa `MeshAsset::aabbMin/aabbMax` real (no cubo unitario hardcoded):** pre-F2H44 el outline en perspectiva (introducido en F2H13) tenía un comentario *"compromise historico, no usa el AABB real del MeshAsset"* — usaba `glm::vec3(-0.5f, 0.5f)` para todos los meshes. Con un mesh de ~3m de largo (Fox.glb), el outline era un cubito de 1m³ centrado en el origen del transform: invisible al ojo. Fix: `else if (sel.hasComponent<MeshRendererComponent>())` lee el AABB real via `m_assetManager->getMesh(mr.mesh)`. Para entidades sin mesh ni brush (Light/Audio/Trigger/Camera/ParticleEmitter), AABB chico fijo 0.5m³ alrededor del origen → SIEMPRE hay feedback visual de selección (consistencia con orto que ya hacía esto desde F2H35).

- **Razón vs OBB rotado:** el AABB se sigue calculando world-space tomando 8 corners locales y transformándolos por `t.worldMatrix()`. Eso da un OBB real (rotado), no un AABB axis-aligned al mundo. El nombre "AABB" en el código es del input (local space del mesh), no del output renderizado.
- **Razón AABB chico fijo para point entities:** uniformidad visual. Si un dev clickea una luz puntual, espera ver UNA forma marcada — no un gizmo flotante sin contexto. 0.5m³ es suficiente para verse pero pequeño para no taparlas.

**Decisión clave 2 — Add Component popup sin AddComponentCommand undoable:** el botón "+ Agregar Componente" al final del Inspector abre un popup con search + lista agrupada (Render/Physics/Audio/Logic/World) de los 11 componentes agregables. El click llama directamente `e.addComponent<X>()` sin pasar por la HistoryStack del undo system.

- **Razón vs implementar AddComponentCommand:** consistencia con el patrón actual del motor — los demos del menú `Ayuda > Demos > Agregar luz puntual demo` (etc.) también hacen `e.addComponent` directo sin command. Crear un AddComponentCommand templated por cada uno de los 13 component types + serialización del estado pre-add para el undo es scope significativo (~200 LOC) para un caso que el dev puede revertir manualmente: clickear el icono X del header del componente (que el Inspector renderiza como parte del CollapsingHeader). Anotado como deuda futura por si emerge presión real.
- **Trade-off aceptado:** el dev no puede deshacer un Add Component con Ctrl+Z. Tiene que removerlo manualmente. Aceptable porque la operación es "creativa" (no destructiva) y reversible en 1 click.

**Decisión clave 3 — workspaces con ID ASCII estable separado del label visible:** pre-F2H44 el `Workspace.name` era a la vez el ID persistido en `.moodproj` y el label visible en la pestaña (`"Programar"`/`"Materiales"`/`"Editor de mapas"`). Cuando F2H43 introdujo i18n del editor, intentar traducir esos labels rompía la identidad del workspace (la persistencia en `.moodproj` esperaba el nombre español) — quedó documentado como deuda en F2H43. F2H44 separa: `name` ahora es ID ASCII estable (`"layout"/"scripting"/"materials"/"map_editor"`), label visible viene de `T("workspace.<id>")`.

- **Razón ID ASCII (no más enum):** `.moodproj` se persiste como JSON. Strings ASCII estables son grep-friendly + immune a re-renames de un enum. Si en el futuro se agrega un workspace `"animation"`, no requiere migrar nada del schema — solo agregar la key `workspace.animation` al JSON i18n.
- **Migración robusta:** `migrateWorkspaceName` extendida cubre 3 generaciones: F2H7 (inglés original, ej `"Scripting"→"scripting"`), F2H22 (español task-oriented, ej `"Programar"→"scripting"`), F2H44 (IDs ASCII, passthrough). Proyectos viejos abren sin perder iniLayouts custom del usuario.
- **Decisión paralela: NO eliminé el campo `name` por uno explícito `id`:** habría requerido tocar Workspace.h + ProjectSerializer (schema bump) + re-deserialización de proyectos viejos. El campo `name` sigue funcionando como ID — solo cambio su semántica vía comentario en el header. Refactor mecánico en lugar de cirugía profunda.

**Decisión clave 4 — Material Editor: eliminado modo TwoColumns, layout siempre Vertical:** pre-F2H44 el panel tenía layout adaptativo: `>= 540px` = 2 columnas (controles izq | preview der), `< 540px` = vertical (preview arriba). El modo TwoColumns estaba roto en práctica: `ImGui::Columns` no sincroniza alturas, y al docking en otros lados (workspace `materials` lo ensancha) el preview quedaba flotando en el medio del panel desconectado del label "Preview" — el dev reportó *"si se mueve el panel, ya se rompe"*.

- **Razón eliminar vs arreglar:** arreglar requería sincronizar alturas manualmente (`ImGui::SetCursorPosY` calculado) o migrar a `ImGui::Tables` (refactor mayor). Eliminar es más simple y robusto: ~20 LOC menos, layout vertical funciona en cualquier ancho/dock sin sorpresas.
- **Trade-off aceptado:** en monitores muy anchos no se puede ver preview + controles lado a lado. Marginal: el panel se usa en bursts cortos (ajustar metallic, ver preview, repetir), el scroll vertical en cualquier ancho es aceptable.

**Decisión clave 5 — USER_GUIDE/* + README + GIF descartados:** era el bloque 5 propuesto en mi auditoría inicial. El dev rechazó textualmente: *"seguiremos agregando cosas que luego vamos a terminar cambiando"*. Razón sólida: el motor sigue evolucionando hacia sub-fase 2.5+ (diálogos/quests/inventario). Documentar workflows ahora generaría docs que rotan más rápido que la implementación. Hito propio post-Fase 2 cuando el motor estabilice y los workflows queden congelados.

**Decisión clave 6 — Welcome modal carga "Personaje animado" (Fox.glb) como demo, no Stress Scene:** opciones consideradas: (a) Stress Scene completa, (b) Shadow demo, (c) Personaje animado, (d) Combo Floor + columna + luz + Fox + trigger. Elegido (c).

- **Razón:** visual fuerte sin saturar. Stress Scene (200 cubos + 64 luces + esferas + Fox + CesiumMan + fuego + trigger) intimida y ralentiza editores low-end al primer arranque. Shadow demo es estático (poco impactante). Combo es excesivo para "primera impresión". Personaje animado: 1 entidad, animación visible inmediatamente, demuestra render + animación + assets sin sobrecargar.

**Decisión clave 7 — subagente NO usado en F2H44:** a diferencia de F2H43 (255 keys en 23 archivos = caso textbook de delegación), F2H44 fueron 5 cambios pequeños y heterogéneos que no encajaban en un patrón mecánico. Cada bloque requirió judgment calls puntuales (categorías Add Component, choice del demo, mappings de migración workspaces, layout del Material Editor, gates del Ctrl+wheel). Hacer todo en main context fue lo correcto.

**Alternativas descartadas en F2H44:**
- **AddComponentCommand undoable:** ver decisión 2.
- **Top-level menú "Ejemplos" en MenuBar:** descartado a favor del botón en Welcome modal. El menú top-level requiere otro click + menos descubrible (un dev nuevo no abre "Ejemplos" antes de saber qué es). El botón en Welcome es VISIBLE inmediatamente al primer arranque.
- **Migrar workspaces a `Workspace { id, name }` con campos separados:** ver decisión 3 (paralela). Demasiado refactor para ganancia chica. Mantener un solo campo `name` (semánticamente ID ahora) es más simple.
- **Material Editor con `ImGui::Tables` en lugar de `Columns`:** Tables sincroniza heights mejor pero es más verbose y el problema (preview separado del label) se resuelve sin él. Hito futuro si emerge presión real (ej. necesidad de mostrar preview + controles + node-graph).
- **Tooltip toolbar tools:** mi crítica original incluía esto pero al verificar el código (`Toolbar.cpp` línea 29-31) ya tenía tooltips wired desde F2H36+F2H43. Falsa alarma; no se hizo nada.

**Condiciones de revisión:**
- Si emerge necesidad de undo del Add Component → implementar AddComponentCommand templated genérico + serializar el estado pre-add (default values del componente).
- Si los workspaces crecen a >6 (ej. animation/timeline/profile) → considerar moverlos a `assets/workspaces/*.json` con definiciones declarativas (icono, ID, layout default).
- Si el dev en algún momento habilita docs externos (post-Fase 2) → empezar por GIF demo de 30s (motor en acción, primer arranque a juego corriendo) + docs/USER_GUIDE/GETTING_STARTED.md (5 pasos: open → spawn brush → texturizar → physics → script).

---

## 2026-05-10: F2H43 cierre — sistema de i18n completo (Editor + HUD + Player)

**Contexto:** el plan F2 original tenía F2H5 (i18n del editor, ~500 strings) + F2H34 (i18n de gameplay, diálogos/items/quests) que nunca se hicieron — la deuda se acumuló durante 40 hitos. F2H41 unificó los strings del HUD a inglés *"sienta baseline para futura selección de idioma"* con la promesa explícita de hacer i18n después. F2H43 ataca los dos hitos originales fusionados en uno: infra + barrido completo del editor + barrido del HUD/Player + Player MainMenu.

**Decisión clave 1 — API namespaced `Mood::I18n::T("key")` (no clase singleton):** mismo patrón que `Mood::Log::engine()` y el nuevo `Mood::UserSettings::*`. Funciones libres en namespace, estado privado en `namespace { ... }` del .cpp. La función se llama `T()` (corta a propósito porque se usa 500+ veces). Variante con interpolación `T("key", args...)` usa `fmt::runtime` + `fmt::format` para placeholders `{}` estilo Python/Rust.

- **Razón vs `gettext` lib externa:** una dependencia menos. La app no necesita `_()`, plurales, contextos, ni catalogos compilados `.mo` — un `unordered_map<string, string>` por idioma alcanza para los ~500 strings actuales.
- **Razón vs clase singleton instanciable:** las clases singleton requieren `I18n::instance()->T(...)` que es ruidoso visualmente. La función libre `Mood::I18n::T(...)` lee como llamada a función pura.
- **Razón warn-once por key faltante:** spammar el log con cada frame de render que pase por una key faltante haría el log inutil. `unordered_set<string> s_warnedKeys` registra los warns ya emitidos; reseteable al `setLanguage()`.

**Decisión clave 2 — keys flat con dot notation (no nested JSON):** `"editor.menu.file": "Archivo"` en lugar de `{"editor": {"menu": {"file": "Archivo"}}}`. El JSON se carga a un `unordered_map<string, string>` lineal con la key como literal completo. La dot notation es solo convención de naming para grep + agrupación visual.

- **Razón vs JSON nested:** lookup directo O(1) en map plano sin traversal recursivo. Diff visual más fácil (cada key en su línea, no anidamiento). Permite que `_comment` (metadata) coexista con keys reales sin estructura especial.
- **Trade-off:** más verboso al editar (todas las keys del namespace `editor.menu.file.*` están dispersas alfabéticamente, no agrupadas por sub-objeto). Mitigado: agrupamos manualmente con líneas en blanco entre namespaces.

**Decisión clave 3 — diccionarios SIN tildes en `es.json`:** `MUNICION` en vez de `MUNICIÓN`, `RESISTENCIA` en vez de `RESISTENCIA` (correcto), `PROXIMO` en vez de `PRÓXIMO`. Reemplazos sistemáticos: `á→a / é→e / í→i / ó→o / ú→u / ñ→n / ¿→? / ¡→!`.

- **Razón:** la font del MoodPlayer (ProggyClean default ImGui, charset Basic Latin solo) no cubre Latin-1. Los caracteres con tilde se renderizarían como tofu (`?`) durante gameplay. El editor (Lato F2H38) sí soporta Latin-1, pero mantenemos paridad para tener UNA fuente de verdad consistente.
- **Mitigación de la deuda:** cuando se cambie el Player a Lato (deuda anotada en pendientes desde F2H38), agregar tildes es un find-and-replace mecánico en `es.json` sin tocar código. Hito chico cuando emerja presión visual (un usuario hispanohablante reportando que el HUD se ve raro).

**Decisión clave 4 — persistencia GLOBAL del idioma (`%APPDATA%\MoodEngine\settings.json`), no per-proyecto:** `core/UserSettings.cpp` con un singleton namespaced minimalista. Editor + Player ambos llaman `UserSettings::init() + I18n::init(UserSettings::language())` al arrancar. Path resolver usa `std::getenv("APPDATA")` (Windows) con fallback al cwd.

- **Razón vs per-proyecto en `.moodproj`:** el idioma es preferencia del USUARIO, no del proyecto. Un colaborador hispanohablante abriendo un proyecto creado por un brasileño NO debería ver portugués. La preferencia es de la persona, debe persistir entre proyectos.
- **Razón abre la puerta:** futuro `settings.json` puede crecer con `theme`, `recent_files_limit`, `editor_font_size`, etc. sin tener que reinventar la infra.
- **Trade-off:** el `.moodproj` no recuerda el idioma del autor. Si el dev cierra el proyecto y lo reabre meses después en otra máquina con otro idioma activo, ve la UI en el otro idioma. Aceptable: las traducciones son consistentes (no se "pierde" información), solo cambian las labels visuales.

**Decisión clave 5 — workspace names NO traducidos en F2H43:** `Layout`/`Programar`/`Materiales`/`Editor de mapas` quedan sin envolver en `T()`. Razón: viven en `WorkspaceManager.cpp` con el nombre como ID persistido en `.moodproj` (la selección del workspace activo se guarda como string `"name": "Editor de mapas"`). Traducirlos requiere refactor: separar nombre INTERNO (ID estable, ej `"map_editor"`) del label MOSTRADO (`T("workspace.map_editor")`). Diferido a hito propio chico cuando emerja necesidad.

**Decisión clave 6 — subagente para barrido masivo del editor:** 23 archivos / 255 keys con misma transformación mecánica = caso textbook de delegación según la regla global del repo (>15 archivos misma transformación = subagente). Estrategia: bloque C1 (MenuBar, ~60 keys) hecho manual primero como piloto del patrón → bloque C2 con subagente con la convención ya validada (naming, qué traducir, qué no, IDs internos de ImGui, iconos FA fuera del JSON, etc).

**Alternativas descartadas en F2H43:**
- **`gettext` o `ICU`:** dep externa innecesaria para 500 strings. Plurales, contextos, catalogos `.mo` compilados → overengineering para el scope actual.
- **i18n via Qt `tr()`:** Qt no es nuestro UI framework (usamos ImGui). El `tr()` de Qt está acoplado al meta-object system.
- **Default Inglés:** descartado. El dev trabaja en español, arrancar en su idioma evita un click extra al iniciar el editor.
- **Selector de idioma en menu top-level Settings:** descartado. No tenemos panel Settings y crearlo solo para el idioma sería overkill. `Ver > Idioma` es donde el dev mira primero.
- **Lua scripts traducibles en F2H43:** descartado por scope. Los string literals dentro de `assets/scripts/*.lua` (ej. `hud_demo.lua` con `"Demo: explore the test map"`) requieren un binding Lua adicional `T("...")` + sweep de los .lua. Hito chico futuro cuando aparezca uno con scripts gameplay reales (no demos).
- **Hot-reload del JSON al cambiar el archivo en disco:** descartado. Útil para developers de traducciones pero overhead innecesario para el caso de uso (devs editan JSON + reinician editor para ver cambios).

**Condiciones de revisión:**
- Si aparece presión de un usuario hispanohablante reportando tofu en el HUD del Player → ejecutar el fix Lato Player (anotado pendiente desde F2H38) + agregar tildes a `es.json`.
- Si el catálogo de keys crece a >2000 (ej. con scripts Lua + diálogos + items + quests de Sub-fase 2.5) → considerar partir el JSON por dominio (`editor.json` / `hud.json` / `gameplay.json`) o migrar a un formato compilado (binary keyset map) para arranque más rápido.
- Si emerge necesidad de un 3er idioma (portugués, inglés UK, etc) → solo agregar `pt.json` / `en_uk.json` y entradas en el menú. La infra ya soporta arbitrarios via `enum class Language` extensible.
- Si emerge necesidad de plurales (ej. "1 entity" vs "5 entities" en HUD), evaluar agregar lib externa (gettext) o resolver inline con condicional en el caller (overhead chico).

---

## 2026-05-10: F2H42 cierre — optimización runtime (shadow caching + VSync toggle)

**Contexto:** F2H39 original "optimización runtime" diferido para PC de escritorio (GTX 1660 / Ryzen 5 5600G del baseline F2H2-F2H6) — el dev movió el trabajo a la desktop. Tracy Profiler v0.11.1 ya integrado vía CPM con `MOOD_PROFILE` cmake option (estaba OFF por default en Release; F2H42 lo activa). Stress scene generada por nuevo handler `processSpawnFullStressSceneRequest` que dispara los 8 spawners individuales: 200 cubos + 64 point lights + 9 esferas PBR + shadow demo + Fox + CesiumMan + fuego + trigger Lua = 285 entidades / 17K tris.

**Decisión clave 1 — shadow map caching por hash de escena (no dirty flags):** hash FNV-1a 64 incremental cada frame de (`lightDir` + `sceneCenter` + `sceneRadius` + por entidad con MeshRenderer: `position/rotationEuler/scale/mesh id`). Si coincide con frame anterior y `m_shadowMapValid`, skip `m_shadowPass->record(...)` y reusa la GLTexture existente. Resultado: ShadowPass::record cae de 13.4 ms/frame a **278 μs/frame (-98%)**, cache hit rate **99.996%** en escena estática. Hash overhead: 32 μs por frame.

- **Razón hash vs dirty flags:** el ECS no tiene dirty flags por componente. Implementarlos requeriría hookear todos los call sites que mutan transforms (Inspector, gizmos drag, Lua scripts, `AnimationSystem::update`, `Physics::updateRigidBodies`, undo/redo commands, deserialización de saves). Hash es O(N) con N = entidades MeshRenderer — barato (32 μs en stress de 285 entidades) y captura **cualquier** mutación sin tocar call sites. Si la escena crece a >5000 meshes, re-evaluar hash incremental por chunk + mark-dirty desde mutators críticos.
- **Razón FNV-1a vs xxHash o std::hash:** FNV-1a es 4 líneas de código sin dependencias, suficientemente bueno para detectar cambios (no es crypto). std::hash<glm::vec3> no existe out-of-the-box. xxHash sería más rápido pero requiere lib externa para una optimización marginal.
- **Edge case off→on:** si `shadowEnabled` pasó de false a true entre frames, `m_shadowMapValid=false` aunque el hash coincida. Sin esto, reactivar luz direccional con escena idéntica reusaría una shadow map vacía (nunca se generó).

**Decisión clave 2 — VSync toggle en Performance panel (no en menú global):** método `Window::setVSync(bool)` + getter `vsyncEnabled()` + checkbox "VSync (60fps cap)" en `PerformanceHudPanel`. Patrón request/consume (`consumeVsyncToggleRequest` → EditorApplication aplica via Window). Sync inicial del checkbox con estado real del Window en `EditorApplication_Init.cpp` (cubre edge case del driver que rechace vsync al crear contexto).

- **Razón Performance panel vs menú global:** el dev mide FPS / frame_ms / draws acá. Agregar el toggle en el mismo panel lo hace descubrible cuando ya está midiendo. Patrón consistente con resto del editor (request flags consumidos por EditorApplication, no callbacks).
- **VSync ON por defecto:** mantenemos el default histórico. El toggle es para medir, no para producción. Sin VSync el GPU corre al 100% en escenas pequeñas tirando energía y calor sin razón.

**Decisión clave 3 — skybox reorder probado y revertido:** hipótesis era que dibujar skybox post-PBR con `depth=1 LEQUAL` solo pintaría píxeles donde la geometría no escribió, reduciendo overdraw del fragment shader. Test4 (17913 frames sin VSync): empate dentro del ruido (~744 fps reorder vs ~776 fps original).

- **Por qué no funcionó:** el "1.14 ms del skybox" en test3 NO era trabajo del fragment shader (que es trivial: 1 sampler read del cubemap o 2 atan/asin del equirect). Era **GPU sync** absorbido por el scope (CPU-side stall esperando que la GPU termine trabajo encolado). Mover el skybox al final solo redistribuye el sync entre Skybox/swapBuffers/endFrame; el costo total no baja.
- **Por qué revertir:** sin ganancia medible + rompe el comentario doc del SkyboxRenderer.h ("Llamar PRIMERO en el frame para que la escena escriba encima") + convención estándar de motores (skybox-first es el patrón estable). Mantener el cambio agregaría carga cognitiva sin retorno.

**Decisión clave 4 — cierre temprano del hito a 780 FPS de headroom:** test3 (sin VSync, post-shadow-cache) muestra stress scene a **780 FPS / 1.27 ms-frame**. Sistemas gameplay combined <0.2 ms. Skybox 1.14 ms (real cost). ImGui ~0.4 ms. PBR pases <0.1 ms cada uno. **17x headroom sobre el baseline pre-fix.** No tiene sentido invertir más esfuerzo cuando el motor está sobrado para contenido real. Optimizaciones diferidas (GPU timestamp queries, CSM cascadas, frustum cull shadow pass): hito propio si emerge presión real (escenas >1000 meshes, target VR, mobile port).

**Alternativas descartadas en F2H42:**
- **GPU timestamp queries (`glQueryCounter` con `GL_TIMESTAMP`):** descartado por scope. Mediría el costo GPU real en lugar del CPU stall, pero requiere buffering 2-3 frames + manejo async + lectura diferida. ~2-3 horas de trabajo para diagnóstico que con 17x headroom no se necesita. Hito propio si emerge presión.
- **CSM (Cascaded Shadow Maps):** descartado. La shadow map actual de 2048x2048 + bounding sphere fijo (radius=30m al origen) es suficiente para escenas tipo Hito 20. CSM aporta calidad en escenas grandes / outdoor amplios — no es el caso aún.
- **Frustum culling del shadow pass:** descartado. Con cache 99.996% hit rate, el costo del record es prácticamente cero. Frustum cull sumaría complejidad para optimizar el 0.004% de los frames donde sí se renderiza.
- **Pre-renderizar skybox a cubemap LDR cacheado:** descartado. El cubemap mode YA usa cubemap (no es procedural). El equirect sí podría convertirse a cubemap on-load, pero el costo del shader (2 atan + 1 textureLod) es trivial y la "mejora" de 1ms es ruido a 780 fps.

**Condiciones de revisión:**
- Si la escena crece a >5000 meshes con MeshRenderer, re-evaluar el hash O(N) — podría ser >100 μs y empezar a pesar.
- Si target shifts a VR (90+ fps requeridos x 2 ojos = 180 fps eye) o mobile (30 fps target con thermal throttling), volver a evaluar GPU side: timestamp queries + CSM + Forward+ tile culling más agresivo.
- Si emerge necesidad de profile post-mortem en producción, agregar export Tracy automático al cerrar el editor.

---

## 2026-05-09: F2H41 cierre — 5 widgets HUD diferidos + 3 fixes laterales + i18n unificado

**Contexto:** F2H39 dejó explícitamente diferido un paquete de 5 widgets HUD adicionales (CompassBar / ObjectiveText / KillFeed / Stamina / CRT scanline) — la lista de la "wishlist" del dev (Half-Life base + Doom/Fallout/Metro/CoD). F2H41 ataca ese pendiente directo, manteniendo el contrato del framework establecido en F2H39: agregar widgets toca máximo 4 lugares (función `drawXxx`, registry, HudState, bindings Lua si necesita). Hito mediano (~3h, ~750 LOC neto) que entrega los 5 widgets + 3 fixes reactivos al feedback del dev durante validación + unificación i18n del HUD.

**Decisiones técnicas clave:**

- **Único cambio en arquitectura del framework: `HudContext` gana `glm::vec3 cameraForward`.** Razón: CompassBar necesita yaw del player. Default `vec3(0,0,-1)` para tests que no inicializan cam. Callers (`EditorApplication::drawGameOverlay` y `PlayerApplication_Frame::endFrame`) leen `m_playCamera.forward()` y lo pasan. Alternativa descartada: leer cam global o singleton — pasar explícito vía context es testeable y libre de orden de init.

- **CompassBar yaw via `atan2(forward.x, -forward.z)`, no `FpsCamera.yaw` interno.** Razón: el compass es UI derivada — `forward` es lo que el player ve, derivar el yaw de ahí garantiza consistencia visual con la cámara real. Si emerge inconsistencia (ej. cam vertical pura donde `forward.x≈0 && forward.z≈0`), el widget abortea con guard `if (std::abs(f.x) < 1e-6f && std::abs(f.z) < 1e-6f) return`. Convención: `yaw=0=N=-Z`, `90=E=+X`, `180=S=+Z`, `270=W=-X`. Tickmarks cada 15° con cardinales destacados (1.5x altura + texto). Rango visible ±90° (180° total).

- **CRT scanline default OFF, los demás 12 default ON.** Razón: efecto retro divisivo (Pip-Boy) que no encaja con todo proyecto. `widget_enabled` initializa con `crt_scanline=false`; toggle desde Lua via `setWidget("crt_scanline", true)`. Patrón explícito: el widget se dibuja siempre que esté en el registry y enabled — el opt-out vía init del map preserva el contrato de F2H39 (registry hardcoded, toggle dinámico).

- **StaminaBar bypass si `max_stamina<=0`.** Razón: gameplay sin stamina (proyectos puzzle/walking sim) no debería consumir área de pantalla con una barra inútil. Default `100/100` para que el demo y proyectos típicos arranquen con la barra visible. Alternativa descartada: requerir `setWidget("stamina_bar", false)` explícito — feo por default; el bypass es semántico correcto.

- **KillFeed con `KillEntry { string text, ImU32 color, float ttl }` + lifetime 4s.** Razón: longer lifetime que `pickup_queue` (2.5s) porque el dev quiere ver kills sin perderlos en combate intenso. Cap visual 5 entradas. Color custom permite diferenciar enemy types/headshots/etc — `pushKillColored(text, r, g, b)` desde Lua. Patrón ttl idéntico a F2H39 pickup_queue (decremento con `dt`, popea en `<=0`, fade in/out por edges) — consistencia interna.

- **Helpers de los 4 widgets con state mutable viven en `GameState::*`, no `GameOverlay::*`.** `pushKill / pushKillColored / setObjective / clearObjective / setStamina / setMaxStamina / etc` se definen en GameState por la misma razón que F2H39: LuaBindings linkea contra GameState (puro state, sin ImGui), pero NO contra GameOverlay (depende de ImGui). Mover los helpers a GameState los hace invocables desde Lua sin arrastrar ImGui al modulo de tests `mood_tests.exe`. Patrón establecido en F2H39, F2H41 lo respeta.

- **Lua bindings expandidos en la misma tabla `hud`, no nueva tabla.** 10 funciones nuevas (`setStamina`, `setMaxStamina`, `getStamina`, `getMaxStamina`, `setObjective`, `clearObjective`, `getObjective`, `pushKill`, `pushKillColored`) sobre las 24 ya existentes (Hito 20 + F2H39). Razón: scripts pre-F2H41 siguen funcionando sin cambios; nuevos pueden mezclar libre. Una sola tabla simplifica la API mental. Alternativa descartada: tabla nueva `hud_combat` o `hud_v2` — segmentaría confusamente.

**3 fixes laterales descubiertos en validación visual con dev:**

- **Hover/pick spurious en Hierarchy panel durante Play Mode.** Síntoma observado: cada vez que el dev miraba un elemento en el viewport durante Play, el ítem correspondiente del Hierarchy se marcaba hovered/seleccionado. Causa: `SDL_SetRelativeMouseMode` captura el cursor para FPS look pero ImGui sigue viendo la posición OS warpeada al centro del viewport — y el Hierarchy panel está debajo del centro en muchos layouts, así que ImGui interpreta cada frame que el mouse "está sobre" la primera entry del Hierarchy. Fix: `io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX)` en `EditorApplication::beginFrame` cuando `m_mode == EditorMode::Play && !GameState::paused()`. Off-screen pos → ImGui no hovera nada. Cuando paused, ImGui vuelve a ver pos real (cursor visible). Aplica solo al Editor — el Player no tiene panels.

- **Caminata "muy lenta con pasitos pegados".** Feedback dev: *"siento que avanzo muy lento, los pasos están muy pegados uno del otro"*. Causa: walkSpeed 4.0 m/s + headbob 5 Hz daba stride visual ~80 cm por paso = denso/agitado. Fix: `k_walkSpeed 4.0→5.5 m/s` (paso normal humano ~5 km/h ≈ 1.4 m/s; FPS convencional 5-6 m/s para feel ágil), `k_crouchSpeed 2.0→3.0`, `k_bobFreq 5.0→3.5 Hz` (paso ~1.6 m a 5.5 m/s = stride realista), `k_bobAmp 0.04→0.05 m`. Aplicado en Editor + Player (paridad — ambos usan mismas constantes en `EditorPlayMode.cpp` y `EditorScene.cpp`). Decisión: estos números son tuning dependiente del feel — quedan ajustables por proyecto en futuro hito (config en `.moodproj`).

- **Spawn no centrado.** Feedback dev: *"en que ubicacion spawnea el usuario? no debería aparecer en el punto 0,0 para que siempre esté en el centro del mapa?"*. Causa: legacy hardcoded `(-4.5, 1.6, 7.5)` heredado del placeholder buildTestMap (esquina del 8x8 grid). Fix: cambio a `(0, 1.6, 0)` en 4 lugares: `EditorApplication.h:m_playCamera` default, `EditorPlayMode.cpp:exitPlayMode` reset, `PlayerApplication.h:m_playCamera` default, `PlayerApplication_SaveLoad.cpp:applyLoadedSave` reset. Convención del motor: spawn al centro del mapa. Para mapas con spawn custom (ej. proyectos con `SpawnPoint` entity), F2H futuro agregará lookup en runtime; por ahora `(0,1.6,0)` es el default sano.

**i18n unificado a inglés** post-feedback dev *"no se si he visto cosas en español e ingles mescladas, no es lo ideal, si a futuro tenemos seleccion de idioma no es lo ideal"*:

- **Decisión: unificar todo el HUD a inglés** (no a español). Razón: convención FPS HUD ya dominante en el codebase pre-fix (HEALTH / AMMO / STAMINA / PAUSED / RESERVE inglés mientras OBJETIVO / CONTINUAR / OPCIONES estaban en español). Inglés es lingua franca de games + matchea estética HL/CoD/Doom inspiración. Strings cambiados: `OBJETIVO:`→`OBJECTIVE:`, `CONTINUAR`→`CONTINUE`, `OPCIONES`→`OPTIONS`, `"Salir al editor"`→`"EXIT TO EDITOR"` (EditorPlayMode), `"Salir al menu"`→`"EXIT TO MENU"` (PlayerApplication_Frame), demo Lua `"Demo: explorar el mapa de pruebas"`→`"Demo: explore the test map"`, `"[E] Levantar item demo"`→`"[E] Pick up demo item"`.
- **Comments del código + log lines siguen en español.** Razón: son dev-facing, no user-facing. La unificación cubre solo strings que aparecen en el HUD/UI del jugador.
- **Sienta baseline para futura selección de idioma** — los string literals serán keys de translation table (`tr("OBJECTIVE")` o similar) sin tener que tocar lógica del HUD. El refactor cuando se implemente i18n será mecánico: wrap de literals en `tr()` + JSON con traducciones por idioma.

**Alternativas descartadas explícitamente:**

- **Vector dinámico de widgets en lugar de array hardcoded:** descartado en F2H41 (igual que F2H39). 13 widgets stable; vector dinámico agregaría runtime registration sin ganancia hasta que aparezca un caso real (ej. plugins / mods que registren widgets propios).
- **HUD diegetic 3D incluido en F2H41:** descartado por scope. Requiere FPS arms (mesh + animator del brazo) que no existen aún. Hito propio mayor futuro.
- **Mini-map / Radar incluidos en F2H41:** descartado por scope. Requiere render-to-texture topdown del mundo cercano — diferente arquitectura del HUD que es pure DrawList. Hito propio mediano futuro.
- **Themes alternativos (Doom saturado / Fallout verde) incluidos en F2H41:** descartado por scope. Requiere theme runtime + bindings Lua. Hito chico propio futuro.
- **Sound feedback (hit marker sound, low HP heartbeat):** descartado por scope. Requiere audio bindings en GameState — fuera del paquete HUD visual.
- **Compass con objective markers proyectados a screen-space:** descartado en v1. Requiere world-space objectives con coordenadas — agregar después si emerge necesidad real (gameplay con waypoints).
- **Implementar i18n completo en F2H41:** descartado. F2H41 unifica strings (baseline) pero translation table queda como hito propio cuando emerja la necesidad real (dev quiere shipping multi-idioma). Sin urgencia ahora.

**Condiciones de revisión:**

- Si emerge feedback de que el HUD necesita más widgets stable (>20), considerar refactor del registry a vector dinámico con plugin pattern.
- Si CompassBar yaw derivation tiene jitter (especialmente cam casi vertical), agregar damping o switch a `FpsCamera.yaw` interno.
- Si CRT scanline a 3 px de espaciado se vuelve cuello en pantallas 1440p+, aumentar spacing a 4-5 px o convertir a shader post-process.
- Si el walk speed 5.5 m/s se siente muy rápido para un proyecto específico, agregar `walk_speed` al `.moodproj` config (ya hay precedente en F2H40 con coyote/jump buffer windows per-proyecto).
- Si emerge proyecto multi-idioma, agregar el sistema i18n con translation table + lookup en HUD strings (ya unificadas).

---

## 2026-05-09: F2H40 cierre — Fix físicas Floor scale-RigidBody desync

**Contexto:** Bug físicas descubierto durante validación de F2H39. El dev no podía pararse en el Floor para ver los widgets dinámicos del HUD porque caía infinito tras enlargar el piso vía Inspector. Pre-F2H40 cuando el `Transform.scale` de una entidad con `RigidBody` cambiaba (Inspector / gizmo / script), el `RigidBody.halfExtents` no se sincronizaba — el visual cambiaba pero la colisión Jolt quedaba al tamaño del body inicial. Mini-hito chico (~45 min, ~80 LOC neto) que cierra el bug + previene futuros desync similares.

**Decisiones técnicas clave:**

- **Auto-sync `halfExtents = Transform.scale * 0.5` solo para Box bodies.** Razón: en Box, el campo `halfExtents` es realmente la mitad del tamaño en 3 ejes — sincronizar con `t.scale * 0.5` es semánticamente intuitivo y matchea cómo el codebase crea bodies (ver `EditorScene.cpp:104` Floor, `:142` Tile, `DemoSpawners` físicas demo). Sphere/Capsule tienen significados distintos para `halfExtents.x` (radio en Sphere; halfHeight en Capsule), no escalan uniformemente desde un `Transform.scale` potencialmente no-uniforme. Si el dev quiere cambiar radius/height de Sphere/Capsule, lo edita en el Inspector field directamente y el caso 2 de abajo lo sincroniza.

- **Re-sync via `setBodyHalfExtents` existente, no destroy+recreate.** `PhysicsWorld::setBodyHalfExtents` (introducido en hitos previos) llama Jolt `BodyInterface::SetShape` que recrea el collider preservando pose + velocity + contacts del body. Importante para Dynamic bodies escalados mid-frame (script Lua o gizmo en Editor Mode con un cube físico). Alternativa descartada: destroy + recreate body — pierde la pose actual + velocidad, lo que rompe Dynamic bodies en movimiento.

- **Cache `lastSyncedHalfExtents` en `RigidBodyComponent`** para detectar cuándo invocar el resync. Inicializado en `vec3(0)` para forzar primer sync inmediato post-materialización. Updated después de cada `setBodyHalfExtents` exitoso. NO se serializa (estado runtime puro). Alternativa descartada: dirty flag `bool needsResync` que el Inspector / gizmo prendan al editar — requiere hookear todos los puntos de mutación del Transform/halfExtents (Inspector partials, gizmo, script bindings); más invasivo que el polling con epsilon compare.

- **Pase de re-sync corre cada frame en `updateRigidBodies`** después del materializar inicial. Overhead negligible (epsilon compare de 3 floats por entity con RigidBody, ~30 ns); a 1000+ entities con RigidBody el costo total queda <30 µs por frame. Si emerge presión de perf en escenas extremas, mover a un dirty-flag system.

- **Aplica también a `PlayerApplication::updatePhysics`** (paridad con Editor). Razón: scripts Lua pueden mutar `Transform.scale` en Play Mode (ej. growing/shrinking effect); saves cargados pueden tener Floor con scale enlargado y el body se recrea con halfExtents derivados de scale del JSON correctamente — pero si el dev guardó un scale modificado en editor pre-F2H40, el sync recién ocurre en F2H40. Patrón idéntico al Editor.

- **Lógica auto-sync ANTES del re-sync condicional**, no en orden inverso. Razón: el caso (a) (Box scale change) muta `rb.halfExtents`; el caso (b) (resync al body) compara `halfExtents` contra `lastSyncedHalfExtents`. Si la lógica corriera en orden inverso, el caso (b) primero observaría el `halfExtents` viejo (pre-mutación) y no detectaría el cambio que el caso (a) iba a aplicar el siguiente frame — un frame de lag visible. Con el orden actual, cualquier cambio se aplica el mismo frame.

**Alternativas descartadas explícitamente:**

- **Sync Transform.position / rotation al body en runtime para Static bodies**: descartado para F2H40 por scope. Ya hay pattern para Dynamic bodies (`step` lee del body al Transform); el caso opuesto (Editor mueve un Floor en Editor Mode → body se actualiza) no estaba en el bug original. Si emerge necesidad real (ej. dev mueve Floor con gizmo y luego entra a Play y el body queda en posición vieja), agregar como hito chico — patrón análogo: comparar `lastSyncedPosition` y llamar `setBodyPosition`.
- **Opt-out flag** `bool autoSyncBoxToScale = true` en RigidBodyComponent: descartado en v1. La convención en todo el codebase actual es `halfExtents = scale*0.5` para Box bodies. Si emerge un caso de uso intencional (ej. Box visual deformado pero colisión cilíndrica más chica), agregar el flag entonces.
- **Refactor del PhysicsSystem a event-driven** (Inspector dispatcha "transform changed" event que el sistema atiende): descartado por scope. Polling con epsilon compare es más simple y suficiente para cualquier escena razonable.

**Condiciones de revisión:**

- Si emerge un demo / entity-creator del codebase con `halfExtents != scale*0.5` intencional (Box body con tamaño distinto al visual), agregar el opt-out flag.
- Si en escenas con 1000+ Box bodies el polling se vuelve cuello del frame (>0.1 ms), refactor a dirty-flag pattern: el Inspector + gizmo + script bindings setean `needsResync=true` al mutar Transform/halfExtents.
- Si emerge necesidad de sync Transform.position/rotation a Static bodies (dev mueve Floor en Editor → body en posición nueva), agregar como hito chico siguiendo el mismo patrón de `lastSyncedPosition`.
- Si Jolt actualiza la API y `BodyInterface::SetShape` deja de existir o cambia comportamiento (preserva pose vs re-spawnea), revisar `PhysicsWorld::setBodyHalfExtents` y este sync flow.

---

## 2026-05-09: F2H39 cierre — HUD framework extensible + paquete inicial estilo HL/Doom/Fallout

**Contexto:** El F2H39 original era "optimización runtime", pospuesto a hito futuro porque el dev está en notebook (Iris Xe) y el baseline de profiling de F2H2-F2H6 se hizo en su desktop (GTX 1660 / Ryzen 5 5600G) — números no comparables. PERFORMANCE.md además explícitamente concluye que sub-fase 2.1 cumplió: *"el motor está listo para contenido real"*. F2H39 pivota al pendiente "HUD del juego procedural/minimalista" anotado post-F2H35. Dev confirma estética **Half-Life como inspiración base** del motor + influencias Doom/Doom Eternal/Fallout/Metro/CoD: *"abarcar todas las posibles cosas de HUD que necesitaremos para futuro, y que se puedan ir agregando más"*. Hito mediano (~3h, ~700 LOC neto) que entrega framework extensible + 8 widgets default + bindings Lua + bug fix lateral.

**Decisiones técnicas clave:**

- **Widget pattern como struct simple, no clase con vtable.** `HudWidget { const char* name, void (*draw)(HudContext&) }` en array hardcoded ordenado back-to-front. Razón: ~10 widgets → vtable overhead innecesario; función pura es testeable + sin estado oculto. Alternativa descartada: `class HudWidget { virtual void draw() = 0; }` con registry dinámico de unique_ptr — mayor complejidad sin ganancia para 10 widgets stable.

- **Helpers de mutación de HudState viven en `GameState`, no `GameOverlay`.** `triggerHitMarker / triggerDamageFlash / pushPickup / clearInteractPrompt` se movieron de `GameOverlay::*` (donde inicialmente los puse) a `GameState::*` durante implementación, tras encontrar **link error en `mood_tests.exe`**: LuaBindings linkea contra GameState (puro state) pero NO contra GameOverlay (depende de ImGui). Pasar los helpers a GameState (sin deps gráficas) los hace invocables desde Lua sin arrastrar ImGui al modulo de tests. Costo: división conceptual entre state mutation (GameState) vs rendering (GameOverlay) — limpia, no convoluta.

- **`PickupNotification` usa `ttl` countdown, no `spawnTime` absoluto.** Refactor durante el fix anterior: la versión inicial usaba `spawnTime = ImGui::GetTime()` en `pushPickup`, pero al moverse a GameState (sin ImGui) hay que sustituir. Patrón ttl: `pushPickup` setea `ttl=2.5f`, el overlay decrementa `n.ttl -= dt` cada frame y popea cuando ttl<=0. Fade in/out se computa de `(k_lifetime - ttl)` para age in y `ttl` directo para age out. Limpio + sin clock dependency.

- **Lua bindings expandidos en la misma tabla `hud`**, no nueva tabla. 18 funciones nuevas (`setMaxHp`, `setMag`, `setMaxMag`, `setReserve`, `setInteractPrompt`/`clearInteractPrompt`/`getInteractPrompt`, `showHitMarker`, `flashDamage(x,y)`, `pushPickup`, `setWidget(name, on)`, `isWidgetEnabled`) preservando las 6 originales del Hito 20 (`setHp`/`setAmmo`/`setPaused` + getters). Razón: scripts pre-F2H39 siguen funcionando sin cambios; nuevos pueden mezclar libre. State-based (no draw calls directos) — el patrón existente escala perfecto al framework expandido. Alternativa descartada: tabla nueva `hud2` o `hud_ext` — segmentaría la API confusamente.

- **Paleta Half-Life hardcoded en `palette` namespace** (no theming runtime). Razón: F2H39 v1 enfoca arquitectura + paquete inicial; theming es scope secundario. Si emerge necesidad real (modo Doom Eternal saturado, modo Fallout monocromo verde), agregar `HudTheme` struct + `setTheme` binding como hito propio. Mientras tanto, el dev edita `palette::*` constexprs si quiere ajustar.

- **`PixelSnapH = false` en Lato + `true` en FA** (heredado de F2H38). HUD usa el mismo atlas, los icons FA salen con snap-to-pixel y el texto Lato smooth. Sin override en GameOverlay.

- **Pause menu rediseño Doom-style con chevrons en hover**: 4 líneas naranjas externas marcando las 4 esquinas del rect del botón. Solo visibles en hover — efecto "geometric Doom" sin saturar el feel. Alternativa descartada: glow shader para el botón hovered — requiere shader pass, scope mayor.

- **SaveLoad: campos nuevos opcionales solo se serializan si difieren del default** (ej. `if (d.hud.max_hp != 100) j["hud"]["max_hp"] = d.hud.max_hp`). Razón: saves chicas para gameplay basico — un save de demo no incluye 7 keys nuevas si los valores son default. Patrón mismo que `coyoteWindowSec` del Hito 40 G y `showEntityLabels` de F2H35.

- **State transient (timers, queue) NO se persiste** en SaveLoad. Razón: no tiene sentido restaurar un hit marker a medio fade tras load. Si emerge necesidad (ej. continuidad cinematográfica de un game over con damage vignette frozen), agregar después.

- **Fix lateral SceneLoader: auto-RigidBody al Floor cargado sin uno.** Descubierto durante validación F2H39 (dev no podía pararse para ver los widgets dinámicos — caía infinito). Causa: proyectos guardados pre-Hito 12 (cuando se introdujo `RigidBodyComponent`) no tenían el body serializado en el Floor, y `EntitySerializer` solo lo agrega si está en el JSON. Fix: en `SceneLoader::applyOneEntity`, si la entidad cargada tiene tag `Floor` o `Tile_X_Y` y NO tiene RigidBody, auto-add un Static Box con `halfExtents = scale * 0.5`. Solo aplica a auto-generadas para no contaminar entities user-creadas que omitan colisión a propósito. Ataca el caso de "loaded project sin RigidBody"; el caso "new project con scale modificado" sigue abierto (ver pendientes).

- **Bug físicas conocido fuera de scope**: cuando el dev cambia `Transform.scale` del Floor en Inspector o vía gizmo, el `RigidBody.halfExtents` no se sincroniza — el visual cambia pero la colisión no. Player puede caer fuera del body. **NO atacado en F2H39** porque está fuera del dominio HUD; documentado como pendiente para hito propio futuro. Fix razonable: en `Inspector_Transform.cpp` o `EditorScene::updateRigidBodies`, detectar el delta `Transform.scale` desde la última creación del body y re-crear o `setBodyShape` con halfExtents proporcional.

**Alternativas descartadas explícitamente:**

- **Hito de optimización runtime sobre la notebook** (Iris Xe): descartado. PERFORMANCE.md baseline está en GTX 1660 — números no son comparables. Posponer al desktop es sensato.
- **HUD shader-based (post-process custom para vignette/scanlines)** en lugar de ImGui DrawList: scope mayor — requiere render pipeline custom para el HUD. F2H39 v1 mantiene DrawList para velocidad de iteración + zero-asset. Si emerge necesidad de fidelidad mayor (CRT scanlines genuinos con chromatic aberration), hito propio futuro.
- **HUD diegetic 3D ahora**: descartado. Requiere FPS arms (mesh + animator del brazo del player) primero — eso es hito propio mayor de gameplay/3D, no HUD overlay.
- **CompassBar / ObjectiveText / KillFeed / etc en F2H39**: descartado por scope. F2H39 entrega arquitectura + paquete inicial; los demás se agregan extendiendo el registry de widgets.
- **Themes alternativos** (Doom saturado / Fallout verde): descartado por scope. Paleta hardcoded HL en v1.

**Condiciones de revisión:**

- Si el dev pide más widgets de la lista diferida (CompassBar, KillFeed, etc.), agregar en hitos chicos siguiendo el patrón de los 8 widgets de F2H39.
- Si emerge necesidad de HUD diegetic 3D (Pip-Boy / muñequera Metro), abrir hito propio: requiere FPS arms primero.
- Si el dev cambia de paleta (ej. Doom Eternal saturado para un nivel específico), agregar `HudTheme` runtime + binding Lua `hud.set_theme(name)`.
- Si los widgets crecen >15 y el array hardcoded se vuelve molesto, refactor a `std::vector<HudWidget>` con `registerWidget()` API. Mientras esté <15, hardcoded es claro.
- Si emerge presión del bug físicas Floor scale-RigidBody desync, abrir hito propio en domain "engine/physics" — el fix natural es en `EditorScene::updateRigidBodies` detectando cambio de Transform.scale.

---

## 2026-05-09: F2H38 cierre — Default font ImGui a Lato

**Contexto:** F2H37 cerró el polish UX + iconos FA con un fix lateral para el em-dash tofu del Welcome modal (síntoma de que ProggyClean — el default font de ImGui — no cubre General Punctuation). El asset `LatoLatin-Regular.ttf` ya estaba en `assets/ui/fonts/` desde antes pero nunca se había cargado. F2H38 promueve Lato a default y consolida los benefits: legibilidad mejorada (especialmente en Console text-heavy), coverage Unicode más amplio, side effect de resolver problemas de tofu para cualquier punctuation natural del español. Mini-hito chico (~30 min, ~25 LOC neto en un solo archivo).

**Decisiones técnicas clave:**

- **Lato Latin Regular a 15px** como tamaño base. Razón: ProggyClean es bitmap monoespaciada de 13px diseñada para terminales (legible en pixel-art, pero pixely y sin kerning para UIs modernas). Lato es sans-serif TTF diseñada para pantalla con kerning correcto. 15px matchea VSCode / JetBrains / convención IDE moderna. Bumpeable a 14 / 16 si validación visual lo pidiera, pero el dev confirmó *"todo se ve perfecto"* al primer arranque.

- **Custom GlyphRanges custom para Lato**: `{0x0020, 0x00FF, 0x2010, 0x2027, 0}`. Cubre Basic Latin + Latin-1 Supplement + subset de General Punctuation (em-dash U+2014, en-dash U+2013, hyphen variants U+2010-U+2015, ellipsis U+2026, comillas curvas U+2018-U+201D). Razón: el dev escribe en español; los caracteres naturales del idioma (acentos, ñ, comillas curvas) deben renderear sin esfuerzo. Pre-F2H38 el em-dash era tofu — F2H38 lo resuelve para todos los strings, no solo el del Welcome modal. Alternativa descartada: `GetGlyphRangesDefault()` (Basic Latin + Latin-1 solo) — dejaría el tofu del em-dash sin resolver.

- **FA merge a 13px explicit**, no `0.0f` implicit. Razón: ImGui 1.92 tira `IM_ASSERT((font->Flags & ImFontFlags_ImplicitRefSize) == 0)` si la primary y la merge no comparten convención de reference size. F2H36 resolvió pasando `0.0f` al merge porque `AddFontDefault` es implicit; F2H38 con Lato a 15.0f explicit obliga a la FA también explicit. 13px = ~85% del texto Lato = icons proporcionales al cuerpo del texto (regla común en design systems: icons al 80-90% del text size para que se vean "del mismo peso visual" sin dominar).

- **`PixelSnapH = false` en Lato**, `true` en FA. Razón: Lato es sans-serif diseñada para anti-aliasing smooth — snap a pixel rompe el rendering a tamaños no enteros y deshabilita kerning sub-pixel. FA monocromática se beneficia del snap (icons crisp en sus tamaños base). Diferencia es visible en hover state y al zoom.

- **NO se revierte el fix em-dash de F2H37** (`—`→`-` en `EditorUI.cpp`). Razón: con Lato cargado, el em-dash ahora rendea correctamente, pero el fix `-` queda en su lugar como approach más robusto: el hyphen-minus U+002D es universal Latin (cualquier font lo cubre), mientras que depender de Lato para U+2014 reintroduciría tofu si alguien cambia la font default en el futuro. Costo del fix mínimo (3 ocurrencias, sin pérdida de información semántica).

- **NO se carga la font del MoodPlayer**. Razón: el Player usa su propio `PlayerApplication_Init.cpp` con su `AddFontDefault()` separado. Cargar Lato allí también requiere replicar la lógica — scope adicional sin valor inmediato (el Player no es text-heavy en runtime, sus paneles son Main Menu + HUD scripts). Si emerge presión de coherencia visual Editor↔Player, fix chico posterior siguiendo el mismo patrón.

- **NO se carga Lato Bold / Italic / SemiBold**. Razón: ImGui no usa font weights nativamente (`ImGui::Text` no tiene un `bold` parameter); el bold se simula con color alfa o `PushFont`. Cargar variantes adicionales agrega ~150 KB cada una al binary sin ganancia clara. Si emerge necesidad real (ej. Inspector quiere bold para headers), agregar variante específica en hito propio.

**Alternativas descartadas explícitamente:**

- **Inter / Roboto / Source Sans Pro / SF Pro / otra sans-serif**: descartado. Lato ya estaba en assets desde antes (heredado, escogido en algún momento previo); cambiar de pack ahora introduce 150-200 KB nuevos sin valor agregado. Lato es decisión "buena suficiente" — todas son sans-serif para UI bien rasterizadas.
- **Cargar Lato con backend FreeType** (mejor rasterizado que stb_truetype default): scope mayor — requiere CPM/build de FreeType, link, y el atlas builder de ImGui debe enchufarse a backend custom. Diferido si emerge presión de calidad de rendering en sizes pequeños.
- **Tamaño 14px o 16px**: descartado tras validación. 15px funciona bien en el monitor del dev (notebook Iris Xe + desktop GTX 1660). Si en pantallas 4K el texto se ve chico, agregar global `io.FontGlobalScale` toggle por DPI — diferido.
- **Font fallback chain (Lato → Noto Color Emoji → CJK)**: scope mayor, no necesario. El editor es interface en español con icons FA — no requiere CJK / emoji ahora.

**Condiciones de revisión:**

- Si el dev pide variantes (Bold para headers de Inspector, Italic para tooltips), agregar hito propio. El patrón es trivial pero requiere decidir mapeo (qué widgets usan qué variante).
- Si emerge necesidad de DPI scaling (4K monitors), agregar `io.FontGlobalScale` toggle desde el menu Ver / settings.
- Si el dev pide cambiar a Inter / otra font, regenerar `IconsFontAwesome6.h` no necesario (FA es independiente). Solo cambiar el TTF + el `AddFontFromFileTTF` path.
- Si MoodPlayer comparte init de ImGui en el futuro (hipotético refactor de "PlayerEditorBase"), Lato se cargaría una sola vez. Mientras tanto, fix chico aparte si emerge.

---

## 2026-05-09: F2H37 cierre — FontAwesome icons en el resto del editor + polish UX general (hito unificado)

**Contexto:** Tras cerrar F2H36 (icons en los 2 toolbars del workspace "Editor de mapas"), el dev pidió expandir al resto del editor: *"deberemos integrarlos en otras areas del proyecto para que todo sea equitativo"*. Al revisar PENDIENTES.md emergió que había además un "Pase de polish UX general continuo" anotado desde F2H21+F2H22 (Inspector drop targets, Hierarchy multi-select feedback, Console level filter, StatusBar layout/colores) — los mismos paneles tocados. Decisión: **fusionar ambos pendientes en F2H37** para evitar doble pasada sobre los mismos archivos. Hito multi-bloque (~5h, ~400 LOC distribuidos en ~15 archivos).

**Decisiones técnicas clave:**

- **Hito unificado deliberadamente**, no dos hitos separados. Razón: ambos pendientes tocan los mismos paneles (Inspector, Hierarchy, AssetBrowser, Console, StatusBar). Hacerlos por separado implicaría abrir InspectorPanel_*.cpp dos veces (una para icons, otra para drop targets), recompilar dos veces, validar visualmente dos veces. Atacar todo junto en un hito reduce churn y evita conflicts de merge entre los dos pasadas. Validado por dev al pedir el scope unificado.

- **Header `IconHelpers.h` separado de `IconsFontAwesome6.h`.** Razón: `IconsFontAwesome6.h` debe quedar como tabla pura de macros UTF-8, sin dependencias de scene/components — eso permite incluirlo desde código que no toca entities (ej. `MenuBar.cpp` solo usa los macros). `IconHelpers.h` agrega los helpers que dispatchean sobre presencia de componentes (`iconForEntity(Entity)`), que sí dependen de `Components.h`/`Entity.h`. Inline en el header — no requiere .cpp porque es un switch puro sobre presencia de componentes.

- **`iconForEntity(Entity)` consolida `entityIconStr` duplicado** que vivía en HierarchyPanel + VisGroupsPanel pre-F2H37. Mismo orden de prioridad (MeshRenderer > Brush > Light > Audio > Script > Trigger > Camera > Particle > sin componente). Razón: el helper local en cada panel se justificaba pre-F2H37 (cada uno mapeaba a `[X]` ASCII), pero al migrar a FA quedaba claro que el mapping es idéntico — si alguien renombra/agrega un component type, hay que actualizarlo en N panels. Helper compartido = un solo punto de cambio. Costo: agregar un nuevo entity type ahora requiere extender el header en lugar de cada panel — bajo, gana coherencia.

- **Polish multi-select del Hierarchy con 3 colores distintos** (naranja=active, amarillo=secundaria, gris=hidden) en lugar de seguir con el ImGui default selected highlight. Razón: pre-F2H37, el dev tenía que abrir el Inspector para saber cuál de las N entities seleccionadas era la primary (la que el Inspector edita por default en single-component flow). Distinguirlas en la lista con color = feedback visual sin click adicional. Color elegido: naranja-amarillo dentro del mismo "warm spectrum" para que no parezcan tipos completamente distintos (ambos son "selected", solo cambia el rol). Pasado test contra el outline naranja de overlay (consistencia cross-panel).

- **Console level filter con 6 SmallButton toggles**, no un dropdown / combo. Razón: un dropdown obliga a 3 clicks (open + click + close) cada vez que cambia el filtro; los 6 toggles independientes permiten ON/OFF de cada nivel con un click puntual sin abrir/cerrar UI. Cada toggle muestra el icon FA del nivel + color del nivel (activo) o tinte tenue (off) — el estado se ve sin tooltip. Trade-off: usa más espacio horizontal en la toolbar que un combo (~6×24=144 px vs ~80 px del combo). Aceptable: la toolbar de Console ya tenía espacio (Limpiar + Auto-scroll + filter input + (?)). Estado en `m_levelEnabled[6]` del header del panel — NO en `.moodproj`. Razón: es un toggle ergonomico de sesión, no una preferencia persistente del proyecto. Default = todos true (mostrar todo).

- **Tabs del AssetBrowserPanel reciben icon, rows NO.** Razón: cada tab es type-pure (todos meshes en Meshes, todos audio en Audio). Agregar el mismo icon en cada row dentro del tab sería ruido visual sin valor agregado. El audit inicial sugería ambos; al implementar emergió la redundancia. Rows mantienen el "displayName + metadata" original limpio.

- **Tab "Texturas" sigue mostrando grid de thumbnails** (no list con icon). Razón: el thumbnail es el feedback visual canonico para texturas — agregar icon `IMAGE` al lado redundaría con el preview. Solo el tab itself recibe icon.

- **InspectorPanel_Brush.cpp `TextDisabled("Brush (CSG)")` → `SeparatorText(ICON " Brush (CSG)")`** durante el pase. Razón: los demás partials del Inspector usan `SeparatorText` (F2H23 convention); el `TextDisabled` original era inconsistente y se notaba en el tour visual. Upgrade gratis al estar tocando el header.

- **InspectorPanel_Particles.cpp mismo upgrade**: `TextDisabled("Particle Emitter")` → `SeparatorText(ICON " Particle Emitter")`. Mismo patrón.

- **InspectorPanel_Internal.h centraliza el include de `IconsFontAwesome6.h`**, no cada partial. Razón: 11 partials hubieran requerido 11 ediciones de include, todas idénticas. Internal.h ya es include compartido por convención (F2H24); agregar un include más mantiene el patrón y reduce churn.

- **Fix lateral em-dash tofu en Welcome modal** (descubierto en Bloque I): el carácter `—` (U+2014, General Punctuation block) no está en ProggyClean (default font de ImGui, cubre ASCII + Latin-1) ni en mi `k_iconRange = {0xE005, 0xF8FF}` (Private Use Area de FA). Pre-F2H36 ya se veía como `?` pero nunca se notó. Fix mínimo: reemplazar `—` por `-` (hyphen-minus U+002D, Basic Latin) en `EditorUI.cpp` (3 ocurrencias). Alternativa descartada: cambiar la default font a Lato (que ya está en `assets/ui/fonts/`) — scope mayor, requiere revisar todo el editor para verificar spacing/alineamiento. Hito propio si emerge necesidad real.

- **Width del botón Play/Stop bumped 64→80 px** para acomodar `ICON_FA_PLAY " Play"` y `ICON_FA_STOP " Stop"`. Sin esto el "Stop" se cortaba.

- **StatusBar mode indicator: `ICON_FA_PLAY` para Play / `ICON_FA_PEN_TO_SQUARE` para Editor.** Razón: `PEN_TO_SQUARE` matchea el icon del menú "Editar" del MenuBar (consistencia cross-panel). Para devs daltonicos, el shape del icon es un refuerzo accesible al color rojo/azul claro existente.

**Alternativas descartadas explícitamente:**

- **Hito separado para FontAwesome (F2H37) y otro para polish UX (F2H38)**: descartado por overlap de paneles. Doble churn sin valor agregado.
- **Cambiar default font a Lato durante F2H37**: descartado, scope mayor. El em-dash tofu es una excepción rara, no un problema sistémico (el resto del UI usa ASCII + Latin-1 que ProggyClean cubre).
- **Header IconHelpers.h con dispatchers para todos los tipos** (entity + asset + log level + workspace): descartado. `iconForEntity` se consolida porque pre-existía duplicación; `iconForAsset` no se usa fuera de AssetBrowserPanel y no hay duplicación que consolidar. `iconForWorkspace` queda local en MenuBar.cpp por la misma razón. YAGNI.
- **Dropdown de level filter en Console**: descartado por UX (3 clicks vs 1 click por toggle).
- **Persistir level filter en `.moodproj`**: descartado. Es ergonomia de sesión, no preferencia del proyecto. Si emerge necesidad real, agregar `Project::consoleLevelEnabled[6]` siguiendo el patrón de `showEntityLabels` (F2H35).

**Condiciones de revisión:**

- Si el dev pide cambiar el default font a Lato (más legible para mucho texto, ej. Console con muchos logs), abrir hito propio. El em-dash tofu se resolvería como side effect.
- Si emerge un component type nuevo (ej. `NavMeshComponent`), extender `iconForEntity` en `IconHelpers.h` con el nuevo case + agregar el macro al header subset.
- Si emerge necesidad de iconos color (multi-tone SVG-style) — FA6 free solid es monocromo. Cambio de pack es scope mayor.
- Si los toggles del level filter se vuelven cuello de botella visual (ej. el dev quiere un "solo errors" macro), agregar shortcut keys (1-6 con modifier) o un macro button "Solo errores" / "Resetear filtros". Diferido.

---

## 2026-05-09: F2H36 cierre — FontAwesome icons en toolbars del editor de mapas

**Contexto:** F2H22 cerró un pase de polish UX que dejó como deuda explícita "iconos image-based del Toolbar" (las labels eran texto castellano corto: `Mover`/`Rotar`/`Escala`/...). El dev expresó interés post-F2H35 con feedback como *"no tenemos iconos para usar para gizmo, en blender es como esto..."*. Mini-hito chico (~30 min, ~50 LOC) que cierra esa deuda agregando FontAwesome 6 free solid al atlas de ImGui y aplicándola a los 17 botones de los 2 toolbars del workspace "Editor de mapas".

**Decisiones técnicas clave:**

- **FontAwesome 6 free solid (`fa-solid-900.ttf`) commiteada al repo en `assets/ui/fonts/`.** Asset estático del repo oficial `FortAwesome/Font-Awesome` rama `6.x`, ~417 KB. Alternativas descartadas: (1) CPM/FetchContent de la zip release de FontAwesome — no hay distribución oficial CMake-friendly, agregaría build step frágil; (2) hand-rolled SVG icons via ImGui DrawList — escala lineal con cantidad de icons, alto costo por icono; (3) Lucide / Material Symbols / IcoMoon — más nuevos pero menos cobertura del legacy FA4/5 codepoints que muchas guías documentan. FA6 es el estándar de facto, estable y autocontenido.

- **Header `IconsFontAwesome6.h` con subset de ~15 macros**, no el header full de ~2000. Macros encoded en UTF-8 con escapes hex (`"\xef\x86\xb2"` etc.) para no depender de la code page del source MSVC (warning C4566). Incrementar el subset al agregar features nuevas con icon es un cambio explícito + visible en code review, no un import implícito que crece sin control. Trade-off: agregar un nuevo icono requiere computar el escape hex UTF-8 a mano (3 bytes para codepoints 0x0800-0xFFFF). Bajo costo, alta legibilidad.

- **Merge con default font (ProggyClean) usando `0.0f` como `SizePixels` en el merge**, no `13.0f`. Razón: ImGui 1.92 introdujo asserts que verifican que el merge use el mismo "reference size" que la dst font (que es implicit cuando se usa `AddFontDefault()`). Pasar 13.0f explicit triggera `IM_ASSERT((font->Flags & ImFontFlags_ImplicitRefSize) == 0)`. Patrón documentado en `imgui-src/docs/FONTS.md` ("Merge font and icons" example).

- **`GlyphMinAdvanceX` dropeado** (que originalmente seteé a 13.0f para mono-spacing del icon column). Razón: ImGui 1.92 tira un segundo assert si combinas glyph advance overrides + `SizePixels = 0.0f` ("Specifying glyph offset/advances requires a reference size to base it on"). Default rendering es suficiente — los iconos quedan al alto natural de la fuente, alineados con el texto del label. La consistencia visual no se nota en uso real con icons mezclados con label castellano.

- **Width del Toolbar bumped 72→92 px** para acomodar `ICON " Cilindro"` sin truncar. `ICON_FA_CIRCLE` + espacio + 8 chars de label en font 13px = ~85 px. Margen de 7 px para padding interno. Valores menores cortaban el label y dejaban el botón con texto mochado.

- **Scope explícitamente acotado a Toolbar + MapEditorTopBar** (los 2 toolbars laterales del workspace "Editor de mapas"). Quedan FUERA y se difieren a F2H37 dedicado: MenuBar, Hierarchy con icon-por-tipo de entity, Inspector con icons en headers de componentes, AssetBrowser con icons por tipo, Console con icons por nivel, StatusBar, paneles VisGroups/Material/Script. Decisión consciente alineada con la convención previa "un hito = un dominio acotado". El dev validó la decisión al pedir el resto: *"deberemos integrarlos en otras areas del proyecto para que todo sea equitativo"* — confirmación de que el approach incremental es correcto, no que el F2H36 quedó incompleto.

- **NO se carga la `LatoLatin-Regular.ttf` que ya existe en `assets/ui/fonts/`.** El dev nunca la activó (ImGui usa default ProggyClean). Cambiar la default font global = scope mayor (revisar todo el editor para verificar que el spacing / alineamiento siguen OK con la nueva métrica) y NO es lo que pidió F2H22 (que pedía iconos, no cambiar la font del editor). Si se decide en el futuro promover Lato como default, hito propio.

**Alternativas descartadas explícitamente:**

- **Embedded TTF en el binary** (via `xxd -i` o `bin2c` para no depender de runtime file): el TTF es 417 KB, se commite una vez al repo, el `mood_runtime_files` target ya copia `assets/` automáticamente. Embedding agregaría build step + complica cargar variantes en el futuro.
- **Mantener el header completo de ~2000 macros** (descargado del repo `juliettef/IconFontCppHeaders`): contamina autocompletado de VS, hace ruido en grep, agrega ~150 KB de overhead de preprocessing por cada TU que lo incluya. El subset acotado se extiende explícitamente cuando hace falta.
- **Push del icon como override del label en `toolButton`** (helper que recibe icon + label separados y los concat internamente): el helper es shared entre Toolbar y MapEditorTopBar pero el call-site sigue siendo el que sabe qué icon usar, así que pasar `ICON_FA_CUBE " Box"` directo es más explícito y permite que el formateo varíe (icon-only / icon-then-label / label-only) sin tocar el helper.

**Condiciones de revisión:**

- Si en F2H37 (extensión al resto del editor) el subset crece a ~50+ icons, considerar adoptar el header `IconsFontAwesome6.h` upstream del repo `juliettef/IconFontCppHeaders` con `#define IGFD_USE_QUICK_PATHS_AND_TYPES` o similar para acotar lo que entra al preprocessor. Mientras estemos < 30 macros, el header propio es más simple.
- Si ImGui actualiza a 1.93+ y cambia el patrón de reference size, revisar este merge. El comentario en `EditorApplication_Init.cpp` apunta al CHANGELOG y FONTS.md como referencia.
- Si el dev pide cambiar a otro icon pack (Lucide, Material Symbols), el cambio principal es regenerar `IconsFontAwesome6.h` (renombrar a `IconsLucide.h` o similar) + reemplazar el TTF + actualizar los call-sites con los macros nuevos. ~30 min de trabajo.
- Si emerge necesidad de iconos color (multi-color SVG-style), FA6 free solid es monocromo — habría que mover a Twemoji / Noto Color Emoji o equivalente, scope mayor.

---

## 2026-05-09: F2H35 cierre — Polish editor: UX viewport + Hammer-style visual

**Contexto:** F2H35 agrupa varios items chicos identificados al validar F2H34 (editor maximizado, toggle wireframe en perspective descartado por dev) + paquete "Hammer-style visual polish" anotado post-F2H33 (tint VisGroup color, color por tipo de entity, labels point entities, pulir face picking). Mini-hito multi-bloque (~1 sesión, ~700 LOC) que cierra deuda de UX visual del editor sin tocar la math/lógica del CSG ni la del scene graph.

**Decisiones técnicas clave:**

- **Editor arranca al tamaño REAL del display, no 1280x720 + maximize async.** `SDL_CreateWindow(SDL_WINDOW_MAXIMIZED)` encola el resize asíncrono — `SDL_GetWindowSize` en el primer frame devuelve los valores que pasamos al CreateWindow (1280x720 stale), no las dimensiones reales maximizadas. El primer rebuild del Dockspace usaba ese WorkSize, los splits con ratio se calculaban a esa resolución y luego ImGui los persistía como offsets ABSOLUTOS al ini. Cuando la ventana ya era 1920x1057 (maximizada real), los offsets stale dejaban panels descuadrados. **Fix:** `SDL_GetDesktopDisplayMode(0, &dm)` antes de CreateWindow para crear directo al tamaño del display + flag `WindowSpec::maximized` para que el WM marque "restaurar" y respete la taskbar. Garantiza dimensiones correctas desde el frame 0.

- **Stamp `k_IniLayoutStamp = 1` per-proyecto en `.moodproj`.** El bumpear `imgui_layout_v2.ini → v3.ini` cubre el ini global de ImGui, pero los `.moodproj` también guardan `iniLayout` por workspace (F2H7). Stamp invalida los iniLayouts persistidos cuando el dockspace builder cambia significativamente. Ausente = legacy 0 (pre-F2H35) → `ProjectSerializer::load` descarta los iniLayouts y deja `iniLayout=""`, forzando rebuild fresh con WorkSize correcto al primer activado del workspace. Sin esto los proyectos viejos seguían descuadrados aunque la ventana arrancara correcta.

- **Tint VisGroup color SOLO en wireframe orto, no en perspective.** Perspective renderea PBR completo (no wireframe excepto outlines de selected); tintar el wireframe del VisGroup tendría sentido solo en orto donde TODO es wireframe. Mantiene consistencia: tipo va al perspective via icon (Bloque D), organización (VisGroup) va al orto via wireframe color (Bloque C). Selection sigue ganando: brush selected = naranja Hammer, override sobre el VisGroup color.

- **Cubitos point entity en orto: tamaño FIJO (r=0.4), no proporcional al snap.** Iteración inicial usaba `snapStep * 0.5` — con snap=64 los cubos eran de 32 unidades de lado, inflaban la vista; con snap=1 desaparecían. Después probé `snapStep * 0.15` clampeado pero seguía siendo demasiado al snap=64. Decisión final: tamaño fijo `r=0.4` que matchea `k_iconPickRadius=0.6` de ScenePick (el área visible coincide con el área pickable, sin sorprender al dev "lo veo pero no lo agarro"). Hammer Source clásico también usa cubitos de tamaño fijo (~8 unidades hammer).

- **Detalles internos elaborados en cubos orto (rayos/X/diagonal/frustum/burst) DESCARTADOS.** Iteración inicial agregó shapes distintivos por tipo dentro del cubo. Feedback dev: *"demasiado grande, como lo hace el hammer editor de valve?"*. Hammer Source clásico = cubito chico + label de texto. La diferenciación fina viene del label (Bloque E), no de la forma. Esto deja el código simpler (1 sola línea `drawAabb` por cubo) y delega la diferenciación al texto que es más legible.

- **Hover preview de face picking en CYAN brillante** `(0.10, 0.95, 1.00)`, NO blanco tenue. Iteración inicial usaba `(0.85, 0.85, 0.95)` — testeado contra textura blanca tilada del proyecto del dev: el blanco desaparecía sobre fondos claros. Cyan saturado contrasta con amarillo (active selected) y naranja (secondary selected) — el dev distingue hover-preview vs ya-seleccionado a primera vista, sobre cualquier textura.

- **Gizmo Rotate constante en pantalla = Translate/Scale.** Pre-F2H35 (F2H30 Bloque D) era `gizmoRingRadius = 0.6 * max(localAabb)` clamped a 0.5 — radio en world-space que se hacía chico al alejar la cam. Translate/Scale ya usaban `k_armLen = 60.0f` píxeles (constante en pantalla). Inconsistencia: alejabas cam, los handles seguían pero el ring se hacía minúsculo. **Fix:** derivar `worldRadius = TARGET_PX / pixelsPerWorld` con `pixelsPerWorld = (h/2) / (camDistance * tan(fovY/2))` — análogo a la proyección perspectiva. Target 70 px (ligeramente mayor que `k_armLen` para que el ring rodee los handles sin solaparse).

- **Toggle "Nombres" default ON, persistido por proyecto.** Hammer original tiene este toggle OFF por default (menú "Map > Show Helpers"). Decisión del dev: *"labels default On"*. Persistencia opcional (`Project::showEntityLabels = true` + `ProjectSerializer::save` solo emite si != default) para no ensuciar `.moodproj` viejos con campos nuevos. Mismo patrón que coyoteWindowSec del Hito 40 G.

- **Pickable extendido a Trigger/Camera/Particle en ScenePick.** Bug pre-existente del F2H17 detectado al implementar Bloque D: `pickEntityFromRay` solo soportaba Light y Audio en la rama sphere-pick — Trigger/Camera/Particle caían al `return` sin pickable. El dev tenía que ir al Hierarchy del workspace Layout para seleccionarlos. Fix extiende la condición a los 5 tipos (mismo `k_iconPickRadius=0.6`). Encajó dentro de F2H35 porque salió a la luz validando el Bloque D.

**Alternativas descartadas explícitamente:**

- **Toggle wireframe/render shading en perspective viewport** (parte original del plan F2H35 Bloque B): descartado por el dev — *"olvdalo, ya tenemos wireframe en el editor de mapas"* (los 3 ortos del workspace "Editor de mapas" ya son wireframe via F2H28). Implementarlo requería tocar el render pipeline + glPolygonMode global + UI overlay con botones — alto costo, bajo valor agregado.
- **Detección automática de WorkSize cambio + rebuild dockspace defensivo**: alternativa al stamp `k_IniLayoutStamp` para invalidar iniLayouts stale. Descartado porque rompería la persistencia del layout custom del dev (cualquier resize manual disparaba reset). Stamp es más predecible.
- **Iconos elaborados por tipo dentro del cubito orto** (rayos sun, X audio, frustum camera, etc.): descartado por feedback dev. Hammer Source clásico = cubito + label. Hammer-Source 2/Hammer++ usa SVG icons via FontAwesome — eso queda como hito futuro propio.

**Condiciones de revisión:**

- Si el render pipeline cambia significativamente y los splits del dockspace requieren bumpearse, incrementar `k_IniLayoutStamp` (mismo patrón del bump del ini global).
- Si emerge necesidad real de iconos image-based en el toolbar (FontAwesome merge), abrir hito propio para no contaminar este — el dev ya lo mencionó como pendiente desde F2H22.
- Si el dev pide hover preview en orto (no solo perspective), agregar — el helper `pickFace` ya está disponible, solo falta el wireup del cursor del orto al render pass.

---

## 2026-05-09: F2H34 cierre — Multi-face material drop (capitaliza refactor F2H33)

**Contexto:** F2H33 introdujo `selectedFaceIndices` (vector de N caras seleccionables via Shift+click), pero el handler de drop en `DemoSpawners_Drop.cpp` quedó pre-F2H33 — solo aplicaba la textura/material al `activeFaceIndex()`. F2H34 capitaliza ese refactor extendiendo el flow para que el drop afecte a las N caras seleccionadas en una sola operación undoable. Es un mini-hito (~1 sesión, ~250 LOC) que cierra una deuda explícita anotada en `PENDIENTES.md` post-F2H33.

**Decisiones técnicas clave:**

- **Extender el command existente, no agregar uno nuevo.** `EditBrushFaceMaterialCommand` cambió `u32 faceIndex` + `u32 oldFaceMatIndex` + `u32 newFaceMatIndex` por `vector<u32>` paralelos, con un constructor 1-cara que wrappea al multi rellenando vectores de tamaño 1. Alternativa descartada: crear `EditBrushFacesMaterialCommand` (plural) — duplicaba 90% del código y forzaba a las llamadoras a elegir entre dos clases con la misma intención. El wrapper preserva los 7 tests del F2H19 sin modificación.

- **Snapshot único de `bc.materials` compartido entre N caras.** Cuando el material es nuevo, un solo `push_back` y todas las caras del set apuntan al mismo slot. Sin esto, cada cara podría duplicar el slot en `apply` inflando `bc.materials` y desincronizando el snapshot post con el real → undo deja state inconsistente. Confirmado por test "aplica el mismo slot a 3 caras".

- **Validación atómica de faceIndices en `apply` (todo o nada).** Si un solo faceIndex está fuera de rango (> `faces.size()`), el command retorna sin mutar nada. Garantiza que un command corrupto nunca deje el brush en estado parcial mid-undo o mid-redo. Cubierto por test específico.

- **Helper `tryAssignMaterialToSelectedFaces` en namespace anónimo del .cpp**, no en EditorApplication header. La lógica solo existe en el flow de drop (texture + material drop comparten 100%). Exponerla en el header propagaría dependencias innecesarias. El helper devuelve `nullptr` cuando no aplica (object mode, brush distinto del active, set vacío) → llamadora cae al flow object-mode sin if extra.

- **Label dinámico singular/plural** según `selectedFaceIndices.size()`: "Asignar textura a cara" vs "Asignar textura a caras". Ediciones del Editar > Deshacer informan correctamente la magnitud de la operación.

- **Deferral consciente de "abrir maximizado" + "toggle wireframe/render" a F2H35.** Durante validación de F2H34 el dev pidió ambos features. Decisión: NO contaminar el commit de cierre F2H34 con scope creep — abrir F2H35 como mini-hito UX viewport propio. Patrón consistente con la convención previa (un hito = un dominio).

**Alternativas descartadas explícitamente:**

- **Push de N commands single-cara** (uno por cara seleccionada): undo/redo serían N pasos, mala UX. El user clickeó UNA vez (drop), el undo debe ser UNA pulsación.
- **Soportar materiales DISTINTOS por cara en una sola op**: poco probable (un drop = un material). Si emerge necesidad futura, extender el command (ya tiene la estructura `vector` lista para variar `newFaceMatIndices` libremente).

**Condiciones de revisión:**

- Si emerge necesidad de aplicar materiales DISTINTOS a caras distintas en una sola op (ej. paint mode con paleta), el command ya soporta `newFaceMatIndices` heterogéneo nativamente — solo falta UI.
- Si el cap de undo/redo del HistoryStack se vuelve apretado con N caras grandes (cada multi-face drop = 1 entry, no N), no hay riesgo previsible. El F2H32 ya tenía operaciones similares con snapshots grandes (BooleanOpCommand) sin problemas.

---

## 2026-05-09: F2H33 cierre — VisGroups + multi-select caras + texture alignment (Hammer cerrado funcional al 100%)

**Contexto:** F2H33 es el último hito antes de cerrar el editor estilo
Hammer en su totalidad funcional (31/44 hitos de Fase 2). Combina 3
features que Hammer 4 tiene y MoodEngine no tenía: VisGroups
(organización), multi-select de caras (selección productiva), y texture
alignment (Align/Fit/Justify del Face Edit Sheet). Tras F2H33, el dev
queda libre de elegir entre sub-fase 2.5 gameplay o seguir con polish UI.

**Decisiones técnicas clave:**

- **Schema bump `.moodmap` v13→v14 aditivo, mismo patrón que F2H26.**
  Array opcional top-level `visgroups: [{id, name, color, hidden}]` +
  campo opcional `visgroupId: u64` por SavedEntity y SavedBrush. El
  loader v14 acepta mapas v13 (array vacío + sin membership) sin
  migración irreversible — al guardar un mapa v13 abierto se persiste
  como v14 sin pérdida. `checkFormatVersion` rechaza v15+ (forward-incompat).

- **VisGroups planos (no jerárquicos), membership 1-a-N.** Hammer 4 es
  así. Una entity pertenece a 0 o 1 grupo (lookup O(1) al renderizar
  sin iterar membership múltiple). Sub-grupos diferidos como deuda
  futura si emerge necesidad real — refactor a `unordered_set<u64>` si
  hace falta. El componente `VisGroupMembershipComponent` opcional con
  `groupId == 0` reservado como sentinel "sin grupo" (preferimos
  ausencia de componente a presencia con 0).

- **Player ignora VisGroups (convención Hammer).** En Hammer 4, los
  VisGroups son herramienta del editor — el `.bsp` final incluye toda
  la geometría sin importar grupos ocultos. Decisión: `applyEntitiesToScene`
  con `useCompiledMesh=true` (path Player) skipea `scene.resetVisGroups()`
  y NO agrega `VisGroupMembershipComponent`. Agregamos parámetro
  `applyVisGroupMembership=true` (default true para callers como
  `DeleteEntityCommand::undo` que sí necesitan preservar membership). Sin
  esto, un mesh oculto en editor también desaparecía al probar en Player
  — bug confuso. Ahora la convención queda explícita: VisGroups son
  productividad del dev, no contenido del juego.

- **Hide gates en 3 lugares: render, picking, Hierarchy grayed.** El
  render (`groupByBatch`, brushPass, skinned pass) skipea entities en
  grupos hidden — no se encolan al frame. ScenePick las skipea — no se
  pueden seleccionar via click viewport. Hierarchy las muestra en gris
  claro `(0.55, 0.55, 0.55)` — el dev necesita verlas para sacarlas del
  grupo, pero queda claro que están "apagadas". Scripts y physics de
  entities ocultas SIGUEN activos (alineado con Hammer — VisGroup no es
  "disable", es "ocultar en viewport"). Si emerge necesidad real,
  agregar flag `disableScripts` separado al VisGroup.

- **Refactor `SelectionSet` breaking-internal: campo → método derivado.**
  Pre-F2H33: `i32 activeFaceIndex` (F2H17) era field público accedido
  como `set.activeFaceIndex`. Para multi-select, refactoreamos a
  `std::vector<i32> selectedFaceIndices` + método `i32 activeFaceIndex()
  const { return selectedFaceIndices.empty() ? -1 : back(); }`.
  Invariante mantenido por los helpers (`setSingleFace`, `toggleFace`,
  `add` que limpia caras al cambiar de active brush): el último del
  vector es siempre la "active" (= primary para single-face ops).
  Migrados ~7 call-sites de field access a método (`set.activeFaceIndex`
  → `set.activeFaceIndex()`); writes a `set.activeFaceIndex = -1`
  cambiados a `set.selectedFaceIndices.clear()`. Tests adaptados +
  6 tests nuevos para los helpers de multi-face. Tradeoff: un breaking
  cambio interno por un API más expresivo. Aceptable porque el dominio
  cambió (single → multi).

- **Active face en amarillo, secundarias en naranja Half-Life.**
  Pre-F2H33 el highlight de cara era naranja uniforme (F2H17). Con
  multi-select N caras todas naranjas no distinguen cuál es la primary
  (la que se mostraba en el header del Face Edit Sheet). Decisión:
  iterar `selectedFaceIndices` y dibujar la última (= active) con
  outline + fill amarillos `(1.00, 0.95, 0.10)`, las demás con el
  naranja Half-Life original. El dev distingue de un vistazo cuál cara
  manda en single-face ops (drop material, ediciones que solo aplican
  a la primary).

- **Face picking robust: pickEntity primero, pickFace después.** Issue
  pre-F2H33 (F2H17 original): `pickFace` solo testeaba contra el brush
  active. Si el dev clickeaba una cara de OTRO brush en Face Mode, el
  faceHit fallaba → caía al `pickEntity` → cambiaba el active → limpiaba
  las caras seleccionadas → segundo click necesario. Con multi-select
  esto rompía el flow de Shift+click cross-brush. Fix: `pickEntity`
  primero (encuentra cualquier brush hovered), `pickFace` contra ese
  brush. Si `sameBrush` + Shift → toggle; sameBrush + sin modifier →
  single; brush distinto → `replaceWithSingle` + `setSingleFace` (no
  toggle: cambiar de brush no acumula caras del brush viejo, sería
  confuso). Resuelve el feedback del dev *"la seleccion es como
  dificil... debo rotar y probar varias caras"*.

- **Reuso de `EditBrushUVCommand` para alignment ops.** El comando ya
  capturaba snapshot completo del brush (todas las caras + sus axisU/V/
  scale/offset/rotation/lockToWorld). Eso significa que aplicar 1 op
  a 1 cara o N caras y pushear el comando funciona sin extensión —
  el snapshot post incluye los cambios de todas las caras tocadas.
  Decisión: NO crear comando nuevo `EditFaceUVCommand`. Tradeoff: el
  command label dice "Editar UV scale" sin distinguir 1 vs N caras
  — aceptable. Los labels específicos del Inspector (`Editar UV scale
  (cara)` / `(N caras)`) los pone el caller via param `label`.

- **"Treat as one face" con axis heterogéneos: warn + aplica.** El
  algoritmo de bounding rect compartido asume que las N caras
  seleccionadas pueden proyectarse al mismo sistema (axisU, axisV) de
  la primary. Si las caras tienen normales muy distintas (ej. arriba +
  costado de un cubo), los axisU/V también difieren y la proyección no
  es geométricamente coherente. Heurística simple: chequear `dot(face.
  uAxis, primary.uAxis)` y vAxis; si < 0.99 (= ángulo > ~8°), log warn
  pero aplicar igual. Hammer 4 hace lo mismo — el dev sabe lo que está
  haciendo cuando activa el checkbox con caras de planos distintos. No
  bloquear la op; solo informar.

- **Refactor CMake colateral: race condition pre-existente.** Pre-F2H33,
  cada exe (MoodEditor + MoodPlayer) tenía sus propios `add_custom_command
  POST_BUILD` con `copy_directory` para shaders/ y assets/ apuntando a
  `$<TARGET_FILE_DIR:exe>/shaders` y `/assets`. Como ambos exes salen
  al mismo `build/Debug/`, `TARGET_FILE_DIR` evalúa al mismo path
  → cuando se compilaban en paralelo (`/m`), ambos POST_BUILD escribían
  a la misma carpeta destino → race condition de `copy_directory` con
  archivos parcialmente escritos. Aparecía intermitente como `MSB3073`
  en MoodPlayer.vcxproj. Fix: nuevo `add_custom_target(mood_runtime_files
  ALL)` con los 3 copies (shaders + assets + SDL2.dll) centralizados;
  `add_dependencies(mood_runtime_files MoodEditor MoodPlayer)` garantiza
  que ambos exes terminan antes del deploy. Compilación de los .cpp de
  ambos exes sigue paralela; solo el deploy es secuencial. Es un fix
  pre-existente que no le tocaba a F2H33 estrictamente, pero apareció
  durante el primer build con el código nuevo y lo cerramos acá.

- **Bug fix `Ctrl++` en teclados 80% sin numérico (layout español).**
  Pre-F2H33 el handler del snap step cycleable de F2H28 aceptaba
  `SDLK_EQUALS` (US/UK: `=` con shift produce `+`) y `SDLK_KP_PLUS`
  (numpad). En layout español la tecla a la derecha de Ñ manda
  `SDLK_PLUS` directo (sin shift). Fix: agregar `SDLK_PLUS` al handler.
  Reportado por el dev al validar Bloque B; aprovechamos el contexto
  para arreglarlo dentro de F2H33.

**Razones:**

- **Paquete unificado (VisGroups + multi-face + alignment) en 1 hito**
  en lugar de 3 separados: comparten dominio (face polish del editor de
  mapas), comparten infra (SelectionSet refactor habilita tanto multi-
  face del Bloque C como el iter del alignment del Bloque D), y cierran
  juntos el "Hammer cerrado funcional 100%" de forma cohesiva. Spliteando
  en 3 hitos hubiera diluido la narrativa.
- **Reuso máximo**: schema bump aditivo (mismo patrón que F2H26),
  comandos undoable (mismo patrón que F2H19 `EditScriptComponentCommand`),
  panel layout (mismo patrón que `HierarchyPanel` con `IPanel::name()`/
  `visible` flag + `applyDefaultVisibilityForWorkspace`), helpers de UV
  (reusan `Csg::collectFaceWorldPolygon` de F2H17), comando UV reusa
  `EditBrushUVCommand` que ya soporta multi-cara nativo.
- **5 commits feat/fix + 1 docs**: granularidad útil para git blame
  (cada bloque cierra una capa). Bloque B es grande pero coherente
  (todo VisGroups end-to-end); bloque C agrupa el refactor SelectionSet
  + handler face picking; bloque D agrupa math + UI; el fix lateral
  del snap es un commit propio porque no es F2H33 strictly.

**Alternativas descartadas:**

- **VisGroups jerárquicos (sub-grupos)**: Hammer 4 es plano. Sub-grupos
  agregaría tree drag UI + propagación de hide/show (¿propagar a hijos?
  ¿nesting de visibility?) + schema más complejo. No agrega valor
  80/20. Diferido.
- **Comando nuevo `EditFaceUVCommand` para alignment**: el snapshot
  completo de `EditBrushUVCommand` ya cubre multi-cara nativamente.
  Comando nuevo sería duplicación.
- **Drag entities desde Hierarchy a VisGroup**: scope mayor (DnD ImGui
  custom). Menú contextual "Asignar selección al grupo" cubre el flow
  básico. Diferido.
- **Multi-face material drop dentro de F2H33**: requiere extender
  `EditBrushFaceMaterialCommand` a multi-cara (vector de snapshots).
  Estaba en el plan pero lo difirimos para no inflar el hito —
  deuda explícita en `PENDIENTES.md`.

**Condiciones de revisión:**

- Si el dev pide sub-grupos VisGroup → refactor mayor (tree storage +
  UI + propagación). Ahí evaluamos si vale el costo.
- Si emerge un escenario donde "Treat as one face" con axis heterogéneos
  produce resultados inaceptables (no solo "raros") → refactor a "rotar
  axisU de las caras secundarias para alinear con la primary antes de
  computar el rect compartido". Por ahora el warn + apply alcanza.
- Si los VisGroups crecen > 100 grupos típicos por proyecto → reemplazar
  el scan lineal de `findVisGroup` por `unordered_map<u64, VisGroup>`.
  Improbable en uso típico de mapping.

---

## 2026-05-09: F2H32 cierre — geometry tools (clip 2-click + carve UI Hammer-style)

**Contexto:** F2H32 cierra 2 de las 3 herramientas core de geometría
de Hammer Editor (Hollow se deja como deferido). Tras F2H31 (selection
+ visual polish), el dev priorizó cerrar el Hammer en su totalidad
antes de pasar a sub-fase 2.5 gameplay.

**Decisiones técnicas clave:**

- **Clip plane derivada de 2 clicks + view-perpendicular del orto.**
  En orto, 3 clicks no pueden definir un plano 3D (todos coplanares
  en el view plane = degenerado). Convención Hammer: la línea entre
  los 2 clicks + extrusión sobre el `forwardAxis` del orto define el
  plano. Implementación: `normal = cross(forwardAxis_orto, lineDir_world)`;
  validación de `||cross|| > kPlaneEpsilon` (línea no paralela al
  forward); `d = -dot(normal, p1)`. La regla "elige el lado positivo
  o negativo" queda determinada por el orden de los clicks, lo cual
  es UX aceptable (si el lado equivocado se conserva, `Ctrl+Z` + `T`
  cycle + Enter de nuevo).

- **Clip = agregar plane a Brush::faces (no clipping geométrico).**
  Convención CSG del motor: las normales de las caras del brush
  apuntan AFUERA, el interior es la intersección de half-spaces
  negativos. Para conservar el lado "Front" (positivo del clipPlane):
  agregar `BrushFace { -clipPlane.normal, -clipPlane.distance }` —
  el interior queda del lado positivo del clipPlane original. Para
  "Back": agregar el plano tal cual. `isBrushValid` filtra resultados
  degenerados (plano fuera del brush, brush queda sin volumen). La
  AABB se recompute via `computeBrushAabb`. **No** se hace polygon
  clipping explícito (Sutherland-Hodgman) — el brush mantiene la
  representación implicit-by-planes.

- **`BooleanOpCommand` extendido con kind=Clip + bSnapshot vacío.**
  Pre-F2H32 el command asumía dos brushes (A + B). Para clip hay 1
  brush (A) y un plano. Decisión: no crear `ClipBrushesCommand`
  separado; en lugar de eso, agregar `Clip` al enum + skipear
  destroy/recreate cuando `bSnapshot.tag.empty()`. Mismo patrón
  reutilizable para carve (donde los carvers no se destruyen, así
  que tampoco hay B que recrear). Trade-off: leve acoplamiento
  semántico (un BooleanOpCommand de "Clip" no tiene B), pero
  evita duplicar el flow snapshot+execute+undo.

- **Carve = subtract iterativo + carvers preservados.** Algoritmo:
  `fragments = [A_world]`; por cada carver B con AABB intersect:
  `fragments = union de subtract(fragment, B)`. Si A queda
  completamente consumido (todos los fragmentos vacíos tras un
  carver), break temprano. Carvers se preservan en el scene
  (estilo Hammer — el dev decide después qué hacer con ellos).
  Push `BooleanOpCommand` con kind=Subtract y bSnapshot vacío
  (los carvers no necesitan recreación en undo).

- **Carve broadphase por AABB.** Sin esto, escenas con muchos
  brushes hacen N² test booleanos por click (cada subtract es ~300
  LOC de geometría). Mitigación: solo brushes cuyo AABB world
  intersecta el AABB del active entran al loop. Ya tenemos
  `intersects(aabb, aabb)` en core/math/AABB.h.

- **Sin keyboard shortcut para carve.** Operación destructiva +
  silenciosa (no hay sesión preview); obligar click explícito en el
  botón "Carve" del toolbar evita accidentes. La tecla `C` queda
  libre. Trade-off: ligeramente más fricción para quien la use mucho;
  aceptable para v1.

- **UX hints visibles via `setStatusMessage`.** Pre-iter el clip y
  el carve solo logueaban warns; el dev no los veía mientras testeaba
  (consola separada). Fix: `setStatusMessage` muestra el hint en el
  status bar inferior — visible siempre que falten pre-condiciones
  (sin selección, sin intersección, plano degenerado, etc.). Misma
  estrategia que el pincel poligonal (F2H30 Bloque C).

- **No hay schema bump.** El clip/carve es spawn dinámico de brushes;
  el undo via `BooleanOpCommand` snapshot. F2H33 traerá el bump
  v13→v14 para VisGroups.

**Razones:**

- **Paquete unificado** (clip + carve en 1 hito) en lugar de 2 hitos:
  comparten dominio (boolean ops sobre brushes con UI Hammer-style),
  comparten el patrón de snapshot+spawn+command, y se validan juntos
  con escenas similares. F2H32 cierra Hammer + carve sin sobrecarga.
- **Reuso máximo**: `Csg::subtract` (F2H12) + `BooleanOpCommand`
  (F2H12) + `brushAabbWorld` (expuesto en F2H31) + `setStatusMessage`
  (F2H30 Bloque C). Solo `clipBrushByPlane` (math chica ~50 LOC)
  + handlers + UI nuevo.
- **3 commits feat (A plan + B+C unificado + D cierre)**: simpler que
  4 porque B y C comparten el patrón de snapshot+command. El commit
  unificado documenta los 2 bloques.

**Alternativas descartadas:**

- **Polygon clipping explícito en clip tool** (Sutherland-Hodgman
  sobre las caras del brush): innecesario — la representación
  implicit-by-planes ya hace el clip "gratis" al agregar la nueva
  cara. Más simple + correcto.
- **`ClipBrushesCommand` separado**: redundante con `BooleanOpCommand`
  + skip de tag vacío. Decisión: extender el existente.
- **Hollow tool en F2H32**: scope incremental sobre carve (sintáctico
  azúcar de "carve A con un brush más chico interior"). Diferido — no
  es típico del flow básico.
- **Multi-brush clip simultáneo (N brushes selectos al confirmar)**:
  ya soportado en el código actual — itera `targets` y cada uno se
  splittea con el mismo plano. Validado mentalmente; no testeado
  exhaustivamente.
- **Clip tool en perspectiva 3D**: gizmo manipulable para mover el
  plano libre sería scope mucho mayor. Hammer no lo tiene.
- **Keyboard shortcut para carve** (ej. `Ctrl+Shift+C`): operación
  destructiva con efecto silencioso (sin sesión preview). El botón
  explícito es la decisión segura. Si emerge pedido del dev, agregar.

**Condiciones de revisión:** si el dev pide hollow tool, multi-brush
carve simultáneo (N A's), clip en perspectiva 3D, o un shortcut para
carve. Si el flow descubre bugs en escenas con muchos brushes
(broadphase O(N) podría ser slow → considerar spatial hash).

---

## 2026-05-08: F2H31 cierre — productivity selection + visual polish (marquee + group transform + snap-to-vertex + frustum + coords cursor)

**Contexto:** F2H31 cierra las brechas de productividad y feedback
visual del editor de mapas estilo Hammer. Tras F2H30 (que cerró el MVP
funcional), el dev evaluó qué falta vs. Hammer real: marquee select,
snap-to-vertex, clip tool, carve UI, VisGroups, texture alignment,
frustum + coords. Decisión: split en 3 hitos (F2H31/32/33) para no
inflar el scope.

**Decisiones técnicas clave:**

- **Tool selector mutually exclusive vs. modifier-based.** Antes el
  block tool fireba siempre con drag en empty space; agregar marquee
  competía por el mismo input. Opciones: (a) tool selector radio
  (Hammer-style); (b) modifier (Shift/Ctrl distingue). Decisión:
  selector radio en la sección "Herramienta" del toolbar lateral —
  alineado con Hammer Editor real, default = Select. Eliminada la
  ambigüedad: cada drag sabe qué hacer según el tool activo.
- **Marquee hit-test "any corner inside" del AABB world.** Para cada
  entidad, proyectar los 8 corners del AABB world al ndc del orto;
  si CUALQUIER corner cae dentro del rectángulo, hit. Más liberal
  que "todos los corners adentro" — alineado con Hammer (cualquier
  overlap selecciona). Trade-off: brushes muy alargados pueden
  seleccionarse con un marquee pequeño que toque solo una esquina.
  Aceptable — es lo que hace Hammer.
- **Group transform reusa infra existente.** El `OrthoDragSession::
  startPositions` desde F2H29 Bloque B ya iteraba `set.selected` al
  populate (no solo el clicked); el comportamiento de "mover N
  entidades juntas" emerge naturalmente al llenar `set.selected` con
  N via marquee. Cero código nuevo de movimiento, solo cambio de
  data. `MultiEditTransformCommand` cubre el push undoable.
- **Snap-to-vertex toggle global (no per-tool).** Un solo flag
  `m_snapToVertexEnabled` afecta pincel + block tool corners + rubber
  band. El dev no quiere "snap-to-vertex solo en pincel" o variantes
  per-tool — uniforme alinea expectativa: si está on, todo lo que
  snappearia al grid prueba primero contra vertices.
- **Snap-to-vertex broadphase por AABB world.** Sin esto, escenas con
  cientos de brushes hacen N² (cada brush enumerar V vertices ×
  cursor cada frame). Mitigación: solo brushes cuyo AABB world
  expandido por threshold contiene el cursor entran al inner loop.
  Si emerge slow en escenas con > 500 brushes, refactor a spatial
  hash. Threshold ndc 0.02 (~8 px screen) generoso.
- **Auto-close del pincel al clickear vertex 1.** Pedido implícito del
  dev tras observar el bug *"cuando cierro todos los puntos y aprieto
  enter, no lo crea"* — porque clickeaba vertex 1 de vuelta generando
  `vertex N == vertex 1` → polígono degenerado → rechazo. Fix: si
  `pointsWorld.size() >= 3` y el click cae dentro de 1mm de
  `pointsWorld[0]`, llamar `closePolygonDraw` directo (skipear el
  push del vertex duplicado). Coexiste con cierre vía Enter
  (Blender-style). Mental model alineado con editores 2D clásicos
  (Photoshop pen tool, Illustrator).
- **Frustum a "look-ahead" 4u en lugar del near-plane real.** El
  near-plane real (~0.1m) es invisible al render (rect colapsa). Un
  rect a distancia 4u en la dirección forward de la cam con dimensión
  proporcional al fovY y aspect del 3D viewport es claramente visible
  y orientable. El dev percibe "qué mira la 3D cam" desde los 3
  ortos. 4 líneas tenues `(0.6, 0.55, 0.2)` desde camPos a las
  esquinas indican el tronco del frustum.
- **Camera basis extraída de la transpose del 3x3 del view matrix.**
  EditorCamera no exposa right/up/fwd directamente. El view matrix
  tiene esas básicas en sus filas (porque view = R^T * T(-eye)). La
  transpose del 3x3 superior-izquierdo da camera-to-world rotation;
  las columnas resultantes son los ejes en world. Sin agregar API
  nueva al EditorCamera. Forward = -row(2) (convención RH OpenGL).
- **Coords cursor solo cuando hovered.** El `m_liveCursor.hovered` ya
  lo reportaba el panel desde F2H30 Bloque C (para el rubber band del
  pincel). Reutilizado: cuando hovered, formatear y mostrar
  `(x, y, z)` debajo del label de la vista. Sin overhead extra
  cuando el cursor está fuera del panel.

**Razones:**

- **Paquete unificado** (5 features en 1 hito) en lugar de 5 hitos:
  todos comparten dominio (productividad + feedback visual del orto)
  y se validan juntos. Marquee + group transform es 1 feature
  conceptual ("seleccionar y mover varios"); snap-to-vertex toca
  pincel + block tool en paralelo; frustum + coords son polish
  visual del orto.
- **3 commits feat (A plan + B+C+D unificado + E cierre)**: simpler
  que 4 commits porque B/C/D entrelazan (snap-to-vertex modifica
  pincel que tiene auto-close fix; frustum coexiste con todo en
  EditorRenderPass). El commit unificado documenta los 3 bloques.
- **Sin schema bump**: F2H31 es state in-memory + UI overlays. F2H33
  hará el bump v13→v14 para VisGroups.

**Alternativas descartadas:**

- **Marquee con modifier (Shift+drag)** en lugar de tool selector:
  rompe el mental model de Hammer donde tools son radio. Descartado.
- **Snap-to-vertex per-tool**: añade complejidad UI (3 toggles) sin
  uso real (el dev quiere un único concepto "estar snappeando al
  vertex").
- **Frustum con near + far + 8 corners + 12 edges**: ruidoso, no
  aporta info útil. Descartado en favor de "rect de mira".
- **Coords cursor en formato custom** (notación científica para
  valores grandes, etc.): premature; `%.1f` cubre el caso común.
  Si emerge necesidad, agregar.
- **Snap-to-vertex extendido a vertex/edge edit (bloque 2.4e)**:
  scope similar pero el caso de uso es distinto (mover un vertex y
  pegarlo a otro vertex). Diferido — no bloquea el flow actual.
- **Marquee Alt-modifier (remove)**: scope incremental, agregar si
  el dev lo pide.

**Condiciones de revisión:** si el dev pide marquee Alt-remove, snap-
to-vertex en vertex edit, frustum más detallado (near + far), o coords
en otra unidad. Si emerge slow en escenas con muchos brushes
(snap-to-vertex N²), refactor a spatial hash.

---

## 2026-05-08: F2H30 cierre — vertex/edge edit + pincel poligonal + W/E/R double-tap modal (3 iteraciones de atajos)

**Contexto:** F2H30 entregó 4 features unificadas como paquete polish UX
del editor de mapas estilo Hammer:
1. Vertex/edge edit con snap absoluto WORLD + rebasing al cierre.
2. Brush poligonal "pincel" + toolbar lateral "Map Tools".
3. Gizmo rotate proporcional al AABB del brush.
4. Atajos modales (esquema final tras 3 iteraciones de feedback).

**Decisiones técnicas clave:**

- **Snap WORLD-space (no LOCAL) en vertex/edge edit.** El grid del
  workspace orto vive en world coords; snappear el delta_local
  inevitablemente desfasaba el resultado para brushes con
  `tf.position != 0`. Implementación: `pivotWorldStart = worldMat *
  pivotLocalStart`; snap del `pivotWorldNew`; reconvertir delta a
  local via `inverse(R) * effDeltaWorld`. **Solo snappear los ejes que
  el dev MOVIO** (`|deltaWorld[i]| > 1e-4`): si snappeás todos,
  brushes con coords no-grid-aligned saltan al grid en el primer
  drag aunque ese eje no se haya tocado.

- **Rebasing del centroide al cierre del drag.** Al modificar planos
  via vertex edit, el centroide del AABB nuevo del brush se aleja
  del `tf.position` original — el gizmo aparece "lejos del brush".
  Fix: en el `dragEnd`, `newCentroidLocal = bc.brush.localAabb.center()`,
  trasladar todos los planos por `-newCentroid`, `tf.position += R *
  newCentroid`. Mismo patrón que F2H12 boolean ops resolvió con
  `snapshotResultWorld`. Ahora el gizmo siempre aparece en el centro
  visual del brush.

- **Dedupe de clicks consecutivos en pincel.** Dos clicks que
  snapeaban a la misma celda del grid generaban un polígono
  degenerado (vértices A → A) y `closePolygonDraw` cancelaba con un
  warn que el dev no veía → percibía "la figura desaparece". Fix:
  skipear el segundo click si está a < 1mm del último, con log
  explícito `[pincel] click duplicado (mismo grid cell) — ignorado`.
  Trade-off: esto solo cubre duplicados *consecutivos*; un polígono
  con vertice 1 = vertice N se sigue rechazando con warn de
  "polígono degenerado". Aceptable.

- **Gates anti-conflicto pincel ↔ vertex/edge ↔ block tool.** Cuando
  el dev activa el pincel:
  - `togglePolygonDrawMode()` setea `m_subMode = Object` +
    `activeFaceIndex = -1` (sin esto, vertex/edge markers seguían
    dibujados encima del pincel + el bloque 2.4e disparaba edits
    accidentales del brush selecto al primer click).
  - El bloque 2.4e (vertex/edge edit) chequea `!m_polyDraw.active`.
  - El bloque 2.4d (block tool LMB) ya tenía el guard.
  Razón: tres consumidores potenciales del LMB en empty space del
  orto. Sin priorización explícita, un click "puro" del pincel
  podía consumirlo el panel como drag (>4 px de jitter) y disparar
  el block tool o el vertex edit en paralelo.

- **Toolbar lateral "Map Tools" como columna derecha.** Pedido
  original del dev: *"hagamos uno superior"*; mutó a *"prefiero
  columna lado derecho"* tras ver que botones horizontales se
  aplastan en 36 px. Ancho 10% del workspace, botones verticales
  32 px alto, highlight del activo + tooltips con shortcut.

- **Gizmo rotate radio proporcional.** `radius = max(0.6 *
  max(localAabb.size()), 0.5)`. Cubre BrushComponent (via
  `bc.brush.localAabb`) y MeshRendererComponent (via
  `MeshAsset::aabbMin/Max`). **No multiplicado por `tf.scale`**:
  simpler, suficiente para el caso común. Si emerge bug con brushes
  rotados + escalados asimétricamente, refactor a OBB world-space.

- **Atajos: 3 iteraciones por feedback explícito del dev.** Plan
  original: G/R/S puros (Blender) + W/E/R Maya conviven. Iter 2: dev
  pidió *"ya no usaremos los shortcuts de maya sino los de blender
  como base"* + *"el de grab eliminalo"* → removidos W/E/R, R = modal
  Rotate, S = modal Scale. Iter 3: dev pidió hibrido double-tap → W
  = Translate gizmo; E single = Scale gizmo / E doble = modal Scale
  uniforme; R single = Rotate gizmo / R doble = modal Rotate libre.
  G y S removidos. Estado final: **sin shortcuts cruzados** — cada
  tecla tiene un único significado. Detección double-tap via state
  `GizmoKeyTapState { lastKey, lastPressTime }` con window 0.4s
  (default Windows double-click).

- **Cuadrado central de uniform-scale gizmo eliminado.** Pedido del
  dev *"asi solo se usa la S"* → mutó a *"asi nos desacemos de la
  S"* (uniform scale via E doble en lugar de S). Como nadie usa el
  cuadrado, removido. Los 3 arrows per-axis siguen.

**Razones:**

- **Paquete unificado** (4 features en 1 hito) en lugar de 4 hitos
  individuales: las 4 comparten dominio (manipulación geométrica
  desde orto + atajos del editor) y se validan juntas. Un hito
  separado por feature sería ceremonia sin valor.
- **Scope cerrado en 4 commits feat (B/C/D + cierre)**: alineado con
  el ritmo de F2H28 / F2H29.
- **Visual feedback del modal** (anillo amarillo + línea
  cursor→centro) por pedido del dev *"que aparezca ese circulo para
  rotar"*. Cosmetic pero importante para entender qué hace el modal.

**Alternativas descartadas:**

- **Snap LOCAL para vertex edit**: dejaba brushes con `tf.position
  != 0` desfasados — el dev lo notó al primer test.
- **Modal G permanece** (translate via cursor): removido por pedido
  explícito del dev.
- **GizmoKeyTapState con timer cosa-loca**: simplificado a `lastKey
  + lastPressTime` + ventana 0.4s. Menos código.
- **Visual de modal con línea punteada del plan original**: cambiado
  a línea sólida + anillo (más visible). Trivial revertir si emerge
  preferencia.
- **Snap-to-vertex** (snap a vertices del scene en lugar de al
  grid): scope mayor + interaction model distinto. Diferido.
- **Marquee select en orto** (rectángulo de selección sobre múltiples
  brushes): scope mayor + se solapa con el block tool en empty
  space. Diferido — no bloquea el modeling flow.

**Condiciones de revisión:** si el dev pide marquee select, snap-to-
vertex, brush poligonal cóncavo, o modal G (translate cursor-driven).
Si emerge preferencia por "rotate sobre view-axis" en lugar de Y
default + axis lock, ajustar el modal Rotate.

---

## 2026-05-08: F2H29 — Block tool + drag-edit en ortos (descope tarde, paquete polish a F2H30)

**Problema:** F2H29 plan original (escrito al cerrar F2H28) prometía 3 bloques de edición en ortos: drag-edit (Bloque B), block tool (Bloque C), vertex/edge edit (Bloque D). Tras implementar y validar B+C, el dev probó el flujo y emergió scope nuevo: gizmo rotate no proporcional al brush (bug pre-existente F2H13), pedido de atajos Blender-style `S/R/G` modal con cursor + línea punteada, pedido de "pincel" poligonal (clicks sobre vertices del grid hasta cerrar mesh — feature distinta del Bloque D vertex/edge edit).

**Decisión:** **descopear el Bloque D + cerrar F2H29 con B+C** + crear F2H30 como paquete polish unificado que junta:
1. Vertex/edge edit (Bloque D diferido).
2. Brush poligonal "pincel" (feature nueva).
3. Gizmo rotate proporcional al AABB del brush (bug pre-existente).
4. Atajos Blender `G` / `R` / `S` modal (feature nueva).

Razones:
- F2H29 ya entrega valor completo y validado por el dev: drag-edit funciona, block tool funciona, gizmo en posición correcta, preview en 4 vistas. El dev puede modelar mapas básicos sin Inspector.
- Bloque D (vertex/edge edit) tiene **alta superficie de bugs** (mover vertice = mutar 3 planos + validar `isBrushValid` post + revertir si rompe) y **bajo valor inmediato** vs. los 3 pedidos polish que el dev hizo explícitamente.
- Los 4 ítems forman un hito coherente de UX-polish del editor de mapas. Splittear más fino agrega ceremonia sin separar dominios reales.
- Decisión explícitamente confirmada por el dev: *"si el ctrl y funciona, y me gusta ese plan siguelo"*.

**Alternativas descartadas:**
- **Forzar Bloque D ahora**: scope creep, rompería el commit de validación incremental con un bloque alto-riesgo. Descartado.
- **Hito separado por cada feature nueva** (F2H30 vertex, F2H31 brush poligonal, F2H32 gizmo, F2H33 atajos): ceremonia sin valor; las 4 features comparten dominio (manipulación de geometría en orto + atajos del editor) y se validan juntas.

---

**Sub-decisión 1 — DragState pulse-style en `OrthoViewportPanel`:**

El panel emite 2 estructuras al caller:
- `DragState { active, justEnded, ndcStart, ndcCur }` — `active=true` mientras LMB-down + delta > 4 px, `justEnded=true` un frame al soltar.
- `ClickSelect { pending, ndc }` — sólo dispara si delta < 4 px.

Mutuamente excluyentes: drag o click, nunca ambos en el mismo frame.

**Razones:** preserva el flow del Bloque F de F2H28 (click-select sin tocar) + agrega drag sin nuevo conflicto. La struct `DragState` separa `active` (continuo durante el drag) de `justEnded` (pulso) — el caller usa `active` para mover en vivo y `justEnded` para pushear el command.

---

**Sub-decisión 2 — Snap al delta, no a posición absoluta (drag-edit):**

```
pos_new = startPos + round((cur_world - start_world) / snap) * snap
```

vs. la alternativa:
```
pos_new = round((startPos + delta) / snap) * snap
```

**Razones:** la 1ra fórmula preserva el offset original del brush respecto al grid (si arrancó desalineado, sigue desalineado pero se mueve en pasos de `snap`). La 2da forzaría al brush a alinearse al grid global en cada move, desplazándolo bruscamente al primer drag. Convención Hammer.

**Block tool usa la opuesta**: snap a las ESQUINAS del rectángulo dibujado (no al delta). Razón: el brush nuevo arranca alineado al grid, lo cual ES lo deseado (no preserva offset previo porque no había brush previo).

---

**Sub-decisión 3 — `pickEntityFromRay` como helper público en ScenePick:**

Bloque B necesitaba pick orto con rayo paralelo. 3 approaches:
1. Duplicar el loop forEach. Descartado: drift garantizado.
2. Refactor con functor que arma el rayo. Descartado: API rara.
3. **Extraer helper público** `pickEntityFromRay(scene, origin, dir, assets)` y hacer que `pickEntity` perspective sea wrapper que arma el rayo con `invVP * (ndc, ±1)` y delega. **Elegido.**

**Razones:** los tests existentes de `pickEntity` siguen verde sin cambios. El helper queda reusable para futuros picking sintéticos (Lua raycast, vertex picking en F2H30, etc).

---

**Sub-decisión 4 — Preview AABB en 4 vistas via debugRenderer:**

Plan original especulaba con preview "AABB cyan via debugRenderer durante drag — visible en perspectiva 3D" y mencionaba que en los 2 ortos extra **NO** aparecería (renderOrthoView no usa debugRenderer). El dev al validar pidió ver el preview en los 3 ortos también: *"creo que deberia haber un boton que active la capacidad de dibujar y que se vea en el wireframe porque seguramente van a crear en el wireframe y luego acomodar en el 3d"*.

**Decisión:** la sesión `OrthoBlockToolSession` guarda `previewMin/Max`. `EditorRenderPass.cpp` re-encola el AABB en `debugRenderer` ANTES de cada `renderOrthoView`. `renderOrthoView` flushea el debugRenderer al final con sus matrices. Cada flush limpia la cola → cada vista renderiza su set queue-eado.

**Razones:** reusa la infra del debug renderer existente (shader + VAO ya cargados). Cero código nuevo de líneas/shaders. Costo: 1 drawAabb call extra por orto + 1 flush extra → trivial.

**Alternativas descartadas:**
- **Overlay 2D en ImGui drawlist en el panel orto fuente**: solo aparece en 1 orto, no resuelve el pedido del dev.
- **Mesh de líneas dedicado**: requiere shader + VBO transient. Overkill para 1 AABB.

---

**Sub-decisión 5 — Color celeste GMod para preview (no cyan):**

Plan original usaba cyan `(0.2, 0.9, 1.0)` para el preview AABB (copy-paste de los drop highlights del editor). Dev al validar: *"porque es cyan? no controlabamos colores de valve y celeste garry'smod?"*.

**Decisión:** preview en celeste GMod `(108, 193, 229)` — mismo RGB que el wireframe regular del orto (`k_wireframeColor` en `SceneRenderer_Ortho.cpp`). Visualmente coherente: lo que el dev ve mientras dibuja es el mismo color que tendrá el brush al materializarse.

---

**Sub-decisión 6 — `spawnBoxBrushAt` rebasea brush a local space (fix de origen):**

Bug detectado en validación: el block tool spawneaba el brush con `tf.position = (0, 1, 0)` (hardcoded en `spawnBrushEntity`) pero la geometría tenía la translation baked-in (porque el block tool pasaba `T(center) * S(dims)` directo a `makeBoxBrush`). Resultado: gizmo en `(0, 1, 0)` y mesh visible lejos.

**Decisión:** `spawnBoxBrushAt` descompone el transform en `center + dims`, construye el brush con sólo `S(dims)` (local space), y override `tf.position = center` post-spawn.

**Razones:** mismo problema y misma fix que F2H12 boolean ops resolvió con `snapshotResultWorld` (rebase planos a local space tras computar centroid). Sin esta fix, drag-edit del brush spawneado se rompe (gizmo no agarrable).

---

**Sub-decisión 7 — Welcome modal fix lateral en `EditorUI` ctor:**

Bug pre-existente reportado por dev al validar F2H29: la pantalla Welcome modal mostraba panels en posiciones stale (residuo del último workspace de la sesión previa via `imgui_layout_v2.ini` auto-loadeado).

**Decisión:** `EditorUI` ctor llama ahora `m_dockspace.requestRebuildForCurrentWorkspace()` después de `applyDefaultVisibilityForWorkspace(initialWs)`. El primer frame ignora el ini stale y construye fresh.

**Razones:** sin proyecto cargado no hay personalización del dev para esta sesión. Cuando carga proyecto, `setWorkspaces` restaura los iniLayout custom del `.moodproj` y este rebuild queda overriden — sin pérdida real.

**Alternativa descartada:** hide del dockspace mientras Welcome está activo. Más invasivo (cambia el flow de render principal); fix actual es 1 línea.

---

**Condiciones de revisión:**
- Si el descope tardío del Bloque D resulta ser un patrón repetido (más hitos splitteados a mitad de implementación), revisar el alcance de los planes iniciales — quizás están siendo demasiado ambiciosos. Por ahora 1 caso aislado; aceptable.
- Si F2H30 (gizmo polish + atajos Blender + brush poligonal + vertex/edge) emerge como hito grande (>1 semana), considerar splittear: F2H30 = vertex/edge + brush poligonal; F2H31 = gizmo polish + atajos Blender.
- El cambio de paleta a celeste GMod (no cyan) deja documentado el principio: usar paleta del workspace (Valve+GMod) para overlays del workspace mismo. Si emerge necesidad de overlays con colores nuevos (ej. preview de boolean op), agregar al `paleta del workspace` consciente.

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


