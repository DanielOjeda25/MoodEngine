# MoodEngine — Plan Técnico de Fase 3

> **Propósito de este documento:** definir el plan completo de Fase 3, una **fase de pulido**. No se agregan sistemas nuevos: se madura y se da control al usuario sobre lo que ya existe. Este es el documento maestro que el agente de código debe consultar para cada hito de Fase 3.
>
> **Filosofía de Fase 3:** *nada hardcodeado, todo editable, todo consistente*. El motor le da al dev libertad sobre cada parámetro del editor y del runtime. Lo que hoy es un magic number en código, mañana vive en un panel de Settings.
>
> **Estado al inicio de Fase 3:** v2.0.0 cerrado (88 hitos de Fase 2) + break `v2.0.2-break-deferreds` cerrado (cleanup de 12 demos historicos + 5 spawners legacy + HudState.ammo, ~-920 LOC). Suite 1087/11182 verde. Sin items abiertos.
>
> **Convención de numeración:** Fase 3 reinicia el contador de hitos. Nomenclatura **Fase 3 — Hito N** (abreviado **F3H1**, **F3H2**, etc.). Tags Git `v2.X.0-fase3-hitoN` escalando hasta el cierre de Fase 3 con `v3.0.0`.

---

## ÍNDICE

1. Visión y principios de Fase 3
2. La regla "nada hardcodeado"
3. Sub-fases y agrupación de hitos
4. Roadmap completo (~27 hitos: F3H1 – F3H27)
5. Detalle del Fase 3 — Hito 1 (arrancamos por acá)
6. Decisiones técnicas mayores de Fase 3
7. Métricas de calidad continuas

---

## 1. Visión y principios de Fase 3

### Visión

Fase 3 transforma MoodEngine de "motor funcional con 88 features" a **motor profesional con UX cuidada y control total del usuario sobre cada parámetro**. Al final de Fase 3, un dev que abre MoodEngine por primera vez puede customizar atajos, defaults de spawn, theme, snap, profiling — todo desde la UI, sin tocar código ni recompilar.

### Principios

**Pulido, no features.** *Regla dura.* Si un hito agrega un sistema nuevo, no es Fase 3 — esperá a Fase 4. Si un hito mejora algo que ya existe (UX, performance, configurabilidad, consistencia), es Fase 3.

**Nada hardcodeado.** Todo valor de comportamiento (defaults, límites, colores, atajos, intervalos) debe ser editable por el dev sin recompilar. Ver sección 2.

**Consistencia.** Si dos panels hacen lo mismo, lo hacen igual. Si dos shortcuts tienen el mismo verbo, comparten la misma tecla. Auditar inconsistencias acumuladas en Fase 1-2.

**UX-first.** Cada hito empieza con la pregunta "¿qué fricción tiene el dev hoy?". Si la respuesta es "ninguna", el hito no es prioridad.

**Cero regresiones.** Suite verde al cerrar cada hito. Validación visual en el editor para todo cambio de UI (no solo tests).

**Documentación viva.** Igual que Fase 2: `DECISIONS.md`, `HITOS.md`, `ESTADO_ACTUAL.md`, `PLAN_HITO_F3H<N>.md`.

---

## 2. La regla "nada hardcodeado"

La regla más importante de Fase 3. Cuando aparezca un magic number, un default, un límite, o un color en el código, evaluar dónde debería vivir:

| ¿Es esto? | Vive en |
|---|---|
| Default per-proyecto (gravity, target FPS, spawn position, snap default) | `.moodproj` + Panel "Project Settings" |
| Preferencia per-instalación (atajos, theme, font size, autosave interval) | `UserSettings` global + Panel "Preferences" |
| Estado de escena (luces, env, audio buses) | `.moodmap` (ya pasa hoy) |
| Valor per-entidad/component | Inspector field (ya pasa hoy) |

**Las constantes solo justifican vivir en código cuando:**
1. Son matemáticas (PI, conversiones grados↔rad).
2. Son magic numbers de algoritmos con justificación documentada (epsilon GGX 0.05 para evitar NaN, threshold de Forward+ tile size).
3. Son límites duros del runtime (max bodies de Jolt, max lights por tile).

**Si dudás: default = configurable.**

**Ejemplos vivos hoy a migrar (catalogación completa en F3H3):**
- `handleAddPointLight`: pos=(0,4,0), radius=12, intensity=1.5, color=tibia
- `handleAddDirectionalLight`: dir=(-0.3,-1.0,-0.2), castShadows=true
- `processSpawnLightStress`: 8×8 grid, spacing=2.5m, radius=3.5m, intensity=1.2
- `processSpawnStressTris`: targets 10K/100K/500K/1M hardcoded en MenuBar
- `EnvironmentComponent` defaults (skyboxPath, fog, bloom, ssao)
- Theme colors del editor (palette::*)
- Font sizes del HUD widgets
- Autosave interval (hoy: 0, no existe)
- Atajos de teclado (F2 quedó deferred desde Fase 2)

---

## 3. Sub-fases y agrupación de hitos

Cuatro sub-fases ordenadas por **dependencia** (no por importancia). La 3.1 es prerequisito conceptual de las otras tres.

```
Sub-fase 3.1 — El editor te respeta (defaults configurables)   F3H1  - F3H7    🏁
Sub-fase 3.2 — Inspector + Hierarchy pulidos                   F3H8  - F3H13   🏁
Sub-fase 3.3 — Asset Browser de verdad                         F3H14 - F3H19
Sub-fase 3.4 — Viewport pro + Performance + Feedback           F3H20 - F3H24
```

> **Consolidación post-F3H18 (2026-05-26):** la Sub-fase 3.4 original tenía 8 hitos (F3H20-F3H27). Tras revisar oportunidades de unir hitos que comparten infraestructura, el dev autorizó **consolidación agresiva a 5 hitos**:
> - F3H21 = ex-F3H21 (cámaras numpad) + ex-F3H22 (modos visualización) → "Viewport pro".
> - F3H22 = ex-F3H23 (Profiler) + ex-F3H24 (Stats overlay) → "Performance feedback".
> - F3H23 = ex-F3H25 (Console mejorada) + ex-F3H26 (Toasts) → "Comunicación al dev".
> - F3H24 = ex-F3H27 (Crash recovery + autosave) sin cambios.
>
> La numeración bumpa: F3H20 (Snapping) queda; F3H21 (Viewport pro); F3H22 (Performance feedback); F3H23 (Comunicación al dev); F3H24 (Crash recovery). Total Fase 3 = 24 hitos (era 27).

**Por qué ese orden:** 3.1 construye la infraestructura de Project Settings + User Preferences. Las sub-fases 3.2-3.4 *consumen* esa infraestructura: cuando 3.2 quiera agregar "hot reload de shaders" debe respetar la pref "auto-reload on save" del usuario; cuando 3.4 agregue snapping, los snap defaults viven en Project Settings.

**Sin 3.1 primero**, las sub-fases siguientes acumularían deuda nueva (más hardcodes a la pasada).

---

## 4. Roadmap completo

### Sub-fase 3.1 — El editor te respeta (F3H1 - F3H7)

**Norte de la sub-fase:** crear los *lugares donde editar*. Project Settings (per-proyecto) + User Preferences (per-instalación). Migrar los hardcodes más visibles como caso de uso.

**F3H1 — Project Settings panel + persistencia `.moodproj`.**
Panel dedicado (`Edit > Project Settings...`) con tabs (General, Spawn Defaults, Rendering, Physics). Esqueleto + persistencia. Detalle en `PLAN_HITO_F3H1.md`.

**F3H2 — User Preferences panel + persistencia `UserSettings`.**
Panel dedicado (`Edit > Preferences...`) con tabs (General, Appearance, Shortcuts placeholder, Editor Behavior). Esqueleto + persistencia. Tema dark/light persistido per-user.

**F3H3 — Auditoría hardcoded values + catalogación.**
Grep sistemático del código por magic numbers / default-position / hardcoded-color. Spreadsheet (markdown table) con cada hit + clasificación (project/user/inspector/dejar-en-code). Output: tickets concretos para F3H4-F3H7.

**F3H4 — Migración spawn defaults (luces + brush + env).**
`handleAddPointLight`, `handleAddDirectionalLight`, `handleAddEnvironment`, primitivas de brush → leer defaults de Project Settings en vez de hardcode. Inspector debe reflejar "from Project Settings" cuando el valor es el default.

**F3H5 — Spawn inteligente (posición contextual).**
Reemplazar `pos=(0,4,0)` etc. por: spawn point configurable (cursor 3D, frente a cámara editor, selección actual, origen). Configurable per-tipo desde Project Settings.

**F3H6 — Atajos de teclado configurables.**
*Era F2H42, quedó deferred desde Fase 2.* Panel Preferences > Shortcuts con todos los atajos editables. Conflict detection. Persistencia. Default = layout actual.

**F3H7 — Migración constantes UI (theme, fonts, density).**
Tema dark/light real, font size editable, layout density (compact/comfortable), padding/spacing del editor. Todo configurable, defaults sensatos.

### Sub-fase 3.2 — Inspector + Hierarchy pulidos (F3H8 - F3H13)

**F3H8 — Multi-select edita N entidades a la vez.**
Shift+click + Ctrl+click en Hierarchy. Inspector muestra valores comunes (o `—` para mixed). Editar aplica a todas.

**F3H9 — Copy/Paste de components.**
Click derecho en header de component → Copiar / Pegar valores / Pegar como nuevo. Cross-entity y cross-proyecto.

**F3H10 — Undo coverage audit + fixes.**
Auditar qué acciones NO son undoable y arreglar (probable lista: cambios de Project Settings, asset rename, drag-edit en orto). Suite de regresión.

**F3H11 — Hot reload sin restart.**
Shaders (existe parcial: F2H25). Scripts Lua (existe: hot reload del watcher). Materiales (.material). Editar un asset en disco → cambio visible en viewport sin cerrar el editor. Eliminar la fricción documentada en `[[project_shader_runtime_sync]]`.

**F3H12 — Búsqueda en Hierarchy + Asset Browser.**
Ctrl+F filtra en vivo. Por nombre, por tipo de componente, por tag. Tecla Esc limpia.

**F3H13 — "Reset to default" en cada Inspector field.**
Botón ↺ pequeño al lado de cada field. Vuelve al valor del Project Settings (no al hardcoded del código). Visualización clara cuando un valor está overrideado vs default.

### Sub-fase 3.3 — Asset Browser de verdad (F3H14 - F3H19)

**F3H14 — Thumbnails de meshes (preview 3D).**
Render off-screen de cada `.moodmesh` cargado. Cache en `<proyecto>/.cache/thumbs/`. Lazy generation. Resolución configurable en Preferences.

**F3H15 — Thumbnails de materiales (esfera PBR).**
Render off-screen de una esfera con el material aplicado + lighting estándar. Mismo cache + lazy pattern.

**F3H16 — Hover preview ampliada.**
Hover prolongado (> N ms, configurable) sobre asset → tooltip grande con preview ampliado + metadata. Estilo Substance Designer.

**F3H17 — Drag&drop con feedback visual.**
Drag de asset al viewport: cursor cambia + drop zone destacada. Drop al Inspector: highlight del field compatible. Cancel con Esc.

**F3H18 — Validador de assets rotos (panel).**
Panel "Asset Validator" dedicado. Lista: materiales con texturas faltantes, prefabs con script borrado, meshes con esqueleto huérfano, `.moodmap` con refs muertas, **assets demo inertes del cleanup v2.0.2**. Click → ir al asset / fix automático cuando aplique.

**F3H19 — Rename con cascada.**
Renombrar `.png` actualiza refs en `.material`. Renombrar `.lua` actualiza refs en `ScriptComponent`. Confirmación modal con lista de archivos afectados.

### Sub-fase 3.4 — Viewport pro + Performance + Feedback (F3H20 - F3H24)

> **Consolidación 2026-05-26:** original tenía 8 hitos (F3H20-F3H27). Ver índice arriba para el mapeo ex→nuevo. Lo que sigue es el plan consolidado.

**F3H20 — Snapping configurable.**
Grid snap (existe parcial), vertex snap (existe: F2H31C), ángulo snap (15°/45°/90°), face-align (orientar a normal). Defaults en Project Settings. Toggle visual en toolbar.

**F3H21 — Viewport pro: Cámaras numpad + Modos de visualización.**
*(Une el ex-F3H21 Cámaras + ex-F3H22 Modos visualización — ambos son herramientas del viewport para workflow del dev. Aunque no comparten infra técnica directa, comparten contexto UX y caen naturalmente en la misma sesión.)*
- **Cámaras guardadas**: Ctrl+Numpad N = guardar pose actual de cámara editor. Numpad N = ir a esa pose. Persistencia per-`.moodmap`. Igual que Blender.
- **Modos de visualización**: Wireframe / overdraw (heatmap de fragmentos) / lighting only / albedo only / normal map. Toggle en toolbar. Útil para debug de assets y perf.

**F3H22 — Performance feedback: Profiler + Stats overlay.**
*(Une el ex-F3H23 Profiler + ex-F3H24 Stats overlay — comparten métricas runtime FPS/drawcalls/tris/mem GPU. Stats es "vista mínima del Profiler".)*
- **Profiler in-engine**: Panel con timing por subsistema (Render / Physics / Scripts / Animation / Audio). GPU markers básicos. Frame graph (últimos N frames como histograma). N configurable. Tracy ya integrado desde F2H2.
- **Stats overlay**: overlay con FPS, drawcalls, triángulos, mem GPU/CPU, lights activas, entities. Cada widget toggleable desde Preferences. Estilo Quake `r_speeds`.

**F3H23 — Comunicación al dev: Console mejorada + Toasts.**
*(Une el ex-F3H25 Console + ex-F3H26 Toasts — comparten fuente, log severity pipeline. Toasts = snippet transitorio de la Console.)*
- **Console panel mejorada**: filtros por severidad (info/warn/error/debug). Search box. Click en `file:line` salta al editor de scripts/material. Botón "Copy as bug report".
- **Toasts no-modales**: notificaciones efímeras esquina inferior derecha: "Guardado", "Asset importado", "Shader compilado OK", "Project Settings actualizadas". Lifetime configurable. Estilo VSCode.

**F3H24 — Crash recovery + autosave.**
Autosave del `.moodmap` cada N minutos (configurable en Preferences, default 5). On crash, al reabrir el proyecto → modal "Recuperar última sesión?". Lock file que detecta crashes.

---

## 5. Detalle del Fase 3 — Hito 1

**Ver [`PLAN_HITO_F3H1.md`](PLAN_HITO_F3H1.md).**

Resumen: crea el panel "Project Settings" con infraestructura completa (modelo de datos + persistencia en `.moodproj` + UI + 2 fields prueba). NO migra todos los hardcodes (eso es F3H4). Foco: dejar el chasis sobre el cual los siguientes hitos van a colgar fields.

---

## 6. Decisiones técnicas mayores de Fase 3

### 6.1. ¿Cómo se versiona `.moodproj` cuando agregue settings nuevas?

**Decisión a tomar en F3H1.** Opciones:
- (a) Bump explícito del schema cada vez (`v1` → `v2` → ...). Conservador pero verbose.
- (b) Sin versión, solo defaults: nuevas keys que no existen se llenan con default; keys que ya no se leen se ignoran. Forward+backward compatible si se respeta la regla.
- (c) Versión por-sección (settings.general.v=2, settings.rendering.v=1).

Probablemente (b) con el patrón ya usado en `SaveLoad.cpp` post-v2.0.2 (ammo removal sin bump). Cerrar en F3H1.

### 6.2. ¿Project Settings overridea o se ignora cuando Inspector tiene valor explícito?

Inspector siempre gana. Project Settings define *defaults para entidades nuevas*. Una vez creada la entidad, los valores son persistentes en `.moodmap` independientemente del Project Settings.

**Implicación UX**: el botón "Reset to default" del F3H13 vuelve al Project Settings, no al hardcoded.

### 6.3. ¿UserSettings actual sirve o hay que refactorear?

`core/UserSettings` ya existe desde F2H43 (i18n). Auditar en F3H2: probablemente solo agregar más fields. Si crece > 200 LOC, considerar split por categoría (UserSettings_Appearance, UserSettings_Shortcuts).

### 6.4. Shortcuts configurables: ¿cómo manejar conflictos?

**Decisión a tomar en F3H6.** Probable: detect on save, warning visible, no bloquear (el dev puede tener razón). Per-context (modal vs viewport vs hierarchy) algunos conflictos son válidos.

---

## 7. Métricas de calidad continuas

- **Suite verde** después de cada hito: 1087/11182+ siempre, agregando tests por feature nueva.
- **Soft cap 500 / hard cap 800 LOC** por `.cpp/.h` (regla ya vigente, `[[file_size_limit]]`).
- **Cero hardcodes nuevos** a lo largo de Fase 3. Si un hito introduce un default, ese default vive en Project Settings o Preferences. Auditar en review de cada hito.
- **Cada hito agrega ≥ 1 entrada a `DECISIONS.md`** documentando elecciones no triviales.
- **Validación visual obligatoria** (Chequear: en commit) para cambios de UI. No basta con suite verde.
- **Cero regresiones de feature**: si un hito de pulido rompe algo que andaba, se rollbackea (no se mergea con regresion conocida).

---

## Apéndice — Items que NO entran en Fase 3

Para evitar scope creep. Si emerge necesidad, agendar en Fase 4.

- **Features nuevas de gameplay** (combate, IA avanzada, RPG systems extra).
- **Networking / multiplayer** (Fase 4+).
- **Backend gráfico adicional** (Vulkan, DX12) — el RHI ya está pero un solo backend (OpenGL).
- **Mobile / consoles** (port).
- **Tutorial in-app** — diferido desde Fase 2.7, sigue diferido salvo que cierre 3.1 muy rápido.
- **Editor colaborativo** (multi-user) — fuera de scope.
- **Save As de Material/Script/Shader** — diferido en F2H85, evaluar en 3.3 si emerge fricción.
