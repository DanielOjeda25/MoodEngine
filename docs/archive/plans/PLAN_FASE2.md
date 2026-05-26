# MoodEngine — Plan Técnico de Fase 2

> **Propósito de este documento:** definir el plan completo de Fase 2 de MoodEngine, desde la consolidación arquitectónica hasta los sistemas avanzados de juego. Este es el documento maestro que el agente de código debe consultar para cada hito de Fase 2.
>
> **Filosofía de Fase 2:** calidad máxima, escalabilidad a futuro, sin atajos. Tiempo no es restricción. Cero deuda técnica acumulada.
>
> **Estado al inicio de Fase 2:** v1.0.0 cerrado tras 42 hitos de Fase 1. Motor con PBR + IBL + shadows + Forward+ + animación esquelética + Jolt physics + Lua scripting + editor con undo/redo + MoodPlayer standalone + packaging. 23.443 líneas en `src/`, 7.271 en tests, 319 casos / 6.613 aserciones pasando.
>
> **Convención de numeración:** Fase 2 reinicia el contador de hitos. La nomenclatura es **Fase 2 — Hito N** (abreviado **F2H1**, **F2H2**, etc.). Los tags Git siguen el esquema `v1.X.0-fase2-hitoN`, donde `v1.X.0` corresponde al hito cerrado y va escalando hasta el cierre de Fase 2 con `v2.0.0`.

---

## ÍNDICE

1. Visión y principios de Fase 2
2. Arquitectura objetivo de Fase 2
3. Sub-fases y agrupación de hitos
4. Roadmap completo (44 hitos: F2H1 - F2H44)
5. Detalle del Fase 2 — Hito 1 (arrancamos por acá)
6. Decisiones técnicas mayores de Fase 2
7. Stack adicional (librerías nuevas)
8. Métricas de calidad continuas

---

## 1. Visión y principios de Fase 2

### Visión

Fase 2 transforma MoodEngine de "motor funcional con v1.0.0" a **motor de calidad profesional comparable a Source/Godot en sus funcionalidades core**, escalable a juegos serios y con un pipeline de contenido moderno asistido por AI.

### Principios

**Calidad sobre velocidad.** Cada hito se cierra con código limpio, tests, documentación, y commit del agente que pueda leer otro humano dentro de 2 años y entenderlo.

**Cimientos antes que techo.** No se agregan features sobre arquitectura que aún no soporta. Cualquier hito que requiera reorganización previa, primero reorganiza.

**Profile, don't guess.** Toda decisión de performance se justifica con números medidos por Tracy, no con intuición.

**Documentación viva.** Cada hito actualiza `DECISIONS.md`, `HITOS.md`, `ESTADO_ACTUAL.md`, y si introduce features visibles para usuarios, también `docs/USER_GUIDE/`.

**Arquitectura escalable de fábrica.** Toda feature nueva se diseña pensando en que el motor crezca a 100K+ entidades, mapas de varios kilómetros cuadrados, equipos de varios desarrolladores trabajando en el mismo proyecto.

**i18n desde F2H4 en adelante.** Todo string nuevo del editor pasa por la función `T(key)`. Sin excepciones. Esto evita la deuda de tener que volver a barrer todo después.

---

## 2. Arquitectura objetivo de Fase 2

### Reorganización de capas

La arquitectura actual tiene `engine/` como una sola carpeta gigante de 11K líneas. Fase 2 la subdivide así:

```
src/
├── core/                          # Sin cambios mayores
│   ├── log/
│   ├── math/
│   ├── time/
│   └── memory/
│
├── platform/                      # Sin cambios mayores
│   ├── window/
│   ├── input/
│   ├── filesystem/
│   └── vfs/
│
├── engine/                        # Subdividido fuerte
│   ├── render/
│   │   ├── rhi/                   # Render Hardware Interface (interfaces)
│   │   ├── backend/
│   │   │   └── opengl/            # Implementación OpenGL
│   │   ├── pipeline/              # Forward+, deferred (futuro), passes
│   │   ├── passes/                # ShadowPass, PostProcessPass, etc.
│   │   ├── resources/             # Mesh, Texture, Shader, Material assets
│   │   ├── debug/                 # Debug renderer
│   │   └── scene_renderer/        # Coordinador
│   │
│   ├── scene/
│   │   ├── core/                  # Entity, Scene
│   │   ├── components/            # Transform, MeshRenderer, Camera, Light, etc.
│   │   ├── serialization/         # Scene/Entity/Prefab serializers
│   │   └── queries/               # Picking, raycasting de escena
│   │
│   ├── physics/
│   │   ├── world/                 # PhysicsWorld
│   │   ├── components/            # RigidBody, Trigger, Constraint
│   │   ├── character/             # Character controller
│   │   └── queries/               # Raycast, sweep
│   │
│   ├── animation/
│   │   ├── skeleton/
│   │   ├── clips/
│   │   └── animator/
│   │
│   ├── audio/
│   │   ├── device/
│   │   ├── clips/
│   │   └── sources/
│   │
│   ├── scripting/
│   │   ├── runtime/               # Lua state, hot-reload
│   │   ├── bindings/              # API expuesta
│   │   └── exposed/               # Exposed properties
│   │
│   ├── assets/
│   │   ├── manager/
│   │   ├── loaders/               # MeshLoader, TextureLoader, etc.
│   │   └── primitives/
│   │
│   ├── world/                     # CSG, GridMap, BSP futuro
│   │   ├── grid/                  # Sistema actual
│   │   ├── csg/                   # NUEVO en F2H9-F2H16
│   │   └── streaming/             # Futuro Fase 3
│   │
│   ├── game/                      # Sistemas de juego (engine-side)
│   │   ├── manifest/
│   │   ├── overlay/
│   │   ├── state/
│   │   ├── dialog/                # NUEVO F2H29
│   │   ├── quest/                 # NUEVO F2H30
│   │   └── inventory/             # NUEVO F2H31
│   │
│   ├── packaging/
│   ├── saving/
│   └── i18n/                      # NUEVO F2H4
│
├── systems/                       # Subdividido por dominio
│   ├── render/                    # PostProcessPass, ShadowPass, SkyboxRenderer
│   ├── physics/                   # PhysicsSystem, TriggerSystem
│   ├── animation/                 # AnimationSystem
│   ├── ai/                        # NavSystem, NUEVO BehaviorSystem
│   ├── particles/
│   ├── audio/
│   ├── light/
│   └── scripting/
│
├── editor/                        # Subdividido por categoría
│   ├── application/               # EditorApplication y partials
│   ├── ui/                        # Dockspace, MenuBar, StatusBar
│   ├── panels/
│   │   ├── scene/                 # Hierarchy, Inspector, Viewport
│   │   ├── assets/                # Browser, MaterialEditor, ScriptEditor
│   │   ├── debug/                 # Console, LuaApi, Performance
│   │   └── world/                 # MapEditor, BrushTools, TriggerTools
│   ├── commands/
│   ├── tools/                     # Gizmos, brush tools
│   └── overlay/
│
├── player/                        # Sin cambios mayores
│
└── game/                          # Vacío, para juegos
```

**Regla de oro:** ningún `.cpp` excede 500 líneas idealmente, 800 como límite duro. Cuando uno crece más, se divide.

### Reglas de dependencias

Mantener las capas existentes (sección 5.2 del documento técnico maestro) pero reforzar:

- `engine/render/backend/opengl/` es el **único** lugar que incluye `glad/gl.h`.
- `engine/render/rhi/` define interfaces puras, sin includes de OpenGL.
- `editor/` puede usar `engine/` pero `engine/` nunca incluye nada de `editor/`.
- `player/` puede usar `engine/` pero nunca `editor/`.
- `systems/` puede usar `engine/` pero `engine/` nunca incluye `systems/`.

Cualquier violación se trata como bug y se arregla en el mismo hito.

---

## 3. Sub-fases y agrupación de hitos

Fase 2 se divide en **7 sub-fases temáticas**. Cada sub-fase es un cuerpo coherente de trabajo. Las sub-fases tienen orden estricto: 2.1 antes que 2.2, etc.

| Sub-fase | Tema | Hitos | Estimación |
|---|---|---|---|
| **2.1** | Consolidación profunda | F2H1 - F2H8 | 4-6 semanas |
| **2.2** | Editor de niveles serio (CSG) | F2H9 - F2H16 | 2-3 meses |
| **2.3** | Renderer y materiales next-gen | F2H17 - F2H22 | 1-2 meses |
| **2.4** | Física y gameplay físico avanzados | F2H23 - F2H28 | 1-2 meses |
| **2.5** | Sistemas de juego (RPG primitives) | F2H29 - F2H34 | 1-2 meses |
| **2.6** | Pipeline de contenido AI | F2H35 - F2H40 | 1-2 meses |
| **2.7** | UI/UX final del editor | F2H41 - F2H44 | 3-4 semanas |

**Total:** 44 hitos, **6-9 meses** de trabajo estimado.

---

## 4. Roadmap completo de Fase 2

### Sub-fase 2.1 — Consolidación profunda (F2H1 - F2H8)

**F2H1 — Reorganización arquitectónica.**
Subdividir `engine/` y `editor/` según la estructura de la sección 2. Mover archivos sin cambiar funcionalidad. Tests deben pasar exactamente igual. CMakeLists actualizados. Detalle completo en sección 5 de este documento.

**F2H2 — Tracy + benchmark sistemático de polígonos.**
Integrar Tracy. Crear escenas de stress-test (10K, 100K, 500K, 1M triángulos). Medir FPS en GTX 1660. Documentar en `docs/PERFORMANCE.md`. Identificar top 5 cuellos de botella.

**F2H3 — Frustum culling.**
Implementar frustum culling jerárquico. Cada `MeshRendererComponent` tiene su AABB en world space. Antes del draw call, test de visibilidad contra los 6 planos del frustum de cámara. Medir mejora en escena con 1000+ entidades.

**F2H4 — LOD system.**
`MeshAsset` puede tener múltiples niveles (LOD 0/1/2/3). Selección automática por distancia. Editor permite cargar LODs manualmente o auto-generar con meshoptimizer (nueva dep). Hot-swap en runtime.

**F2H5 — i18n completo del editor.**
Sistema `i18n` en `engine/i18n/`. Archivos `lang/en.json`, `lang/es.json`. Función `Mood::T(key)`. Selector de idioma en Settings. Barrer **todos** los strings hardcoded del editor (estimado 500+). Inglés como idioma fallback.

**F2H6 — Reorganización UI editor por categorías.**
Menú Ver agrupa paneles en categorías: Scene, Assets, Debug, World. Cada panel tiene un icono. Toolbar superior con botones de acceso rápido. Layout por defecto profesional con `imgui.ini` baseline commiteado.

**F2H7 — Documentación pública.**
README con video/GIF de demo. `docs/USER_GUIDE/GETTING_STARTED.md`, `docs/USER_GUIDE/CREATING_FIRST_GAME.md`, `docs/USER_GUIDE/SCRIPTING_GUIDE.md`. `docs/API/LUA_API.md` referencia completa. `docs/CONTRIBUTING.md` real.

**F2H8 — CI/CD con GitHub Actions.**
Workflows: build en Windows + Linux (que sirve como test de portabilidad), suite de tests automática en cada PR, badge de status en README, release automático cuando se taguea. Setup de Dependabot para dependencias.

### Sub-fase 2.2 — Editor de niveles serio (F2H9 - F2H16)

**Esta es la sub-fase de la transformación grande.** Construye un editor de mapas estilo Hammer/TrenchBroom: CSG con brushes 3D, operaciones booleanas, texturizado por cara.

**F2H9 — CSG arquitectura base.**
Diseño del sistema CSG. Tipos `Brush`, `BrushFace`, `Plane`, `BrushOperation`. Primer brush primitivo: Box arbitraria (no AABB) con orientación. Renderizado en viewport. Persistencia en `.moodmap` (schema bump).

**F2H10 — CSG operaciones booleanas.**
Operaciones Union, Subtract, Intersect entre brushes. Algoritmo de clipping de planos. Generación de mesh resultante. Tests con casos sintéticos (caja menos esfera, etc.).

**F2H11 — CSG primitivas extendidas.**
Cilindro, prisma triangular/hexagonal, esfera (poliédrica), pirámide, wedge. Cada primitiva con parámetros editables (resolución, segmentos).

**F2H12 — Texturizado por cara con UV editor.**
Cada `BrushFace` tiene material + parámetros UV (offset, scale, rotation, lock-to-world). UV editor visual en panel dedicado. Lock-to-world hace que la textura no se deforme al mover el brush.

**F2H13 — Brush selection y multi-edit.**
Click selecciona brush, Shift+click multi-selecciona. Gizmos para mover/rotar/escalar brush. Multi-edit aplica cambios a selección. Selección de cara individual con sub-modo "Face Mode".

**F2H14 — Compilación brush → mesh optimizada.**
Al guardar el mapa, todos los brushes se compilan a una mesh estática unificada con caras internas eliminadas, vertices soldados, e índices generados. Esta mesh es lo que se renderiza en runtime. Brushes individuales solo existen en el editor.

**F2H15 — Sistema de entidades en mapas estilo Hammer.**
"Entidades de mapa": light, trigger, spawn point, info_player_start, env_sound. Diferentes a entidades del scene tree porque están atadas al mapa. Inspector dedicado para sus propiedades.

**F2H16 — Visgroups y layers.**
Agrupar brushes en "visgroups" para ocultar/mostrar conjuntos. Layers para organizar lógicamente (geometría, detalle, gameplay). Filtrado del Hierarchy por layer.

### Sub-fase 2.3 — Renderer y materiales next-gen (F2H17 - F2H22)

**F2H17 — Material editor con node-graph (visual).**
Panel dedicado con grafo de nodos. Nodos básicos: TextureSample, ColorConstant, ScalarConstant, Multiply, Add, Mix, Normal, Output. Conexiones por drag. Preview esférico en el mismo panel. Persistencia en `.material` con schema visual.

**F2H18 — Shader graph runtime compilation.**
El node-graph genera código GLSL en runtime. Cache de shaders compilados por hash del grafo. Hot-reload del material cuando se edita el grafo.

**F2H19 — SSAO (Screen-Space Ambient Occlusion).**
Pase post-render que oscurece esquinas y contactos. Toggle en Inspector de Environment. Calidad ajustable (samples, radio).

**F2H20 — SSR (Screen-Space Reflections).**
Reflexiones screen-space para superficies metálicas/líquidas. Fallback a IBL cuando no hay datos válidos.

**F2H21 — Bloom HDR avanzado.**
Threshold + downsample + upsample con kernels gaussianos. Toggle y intensidad por Environment.

**F2H22 — Cascade Shadow Maps (CSM).**
Si no estaba ya. 3-4 cascadas para shadows del directional light a distintas distancias. Mejor calidad sin perder rendimiento.

### Sub-fase 2.4 — Física y gameplay físico avanzados (F2H23 - F2H28)

**F2H23 — Jolt constraints (joints).**
Hinge (bisagra: puertas, ruedas), Point (pin), Distance (cuerda), Slider (riel), Fixed (pegado). Componente `ConstraintComponent` con tipo enum. Editor de constraints visual en el viewport.

**F2H24 — Ragdolls.**
Esqueleto + bodies + constraints automáticos. Toggle "ragdoll mode" en runtime cuando un personaje muere. Persistencia de estado.

**F2H25 — Vehicle physics.**
Jolt Vehicle Constraint. Vehículos con chasis + 4 ruedas, suspensión, tracción, frenado, dirección. Componente `VehicleComponent`. Demo: car drivable en el viewport.

**F2H26 — Force fields y zonas físicas.**
Volúmenes que aplican fuerza a los bodies dentro: viento, gravedad alterada, agua (buoyancy básica), explosiones puntuales (impulso radial).

**F2H27 — Triggers avanzados.**
Shapes adicionales: esfera, cápsula, mesh (triángulos arbitrarios). Filtros por tag/layer. Eventos extendidos: `on_trigger_enter_with_velocity`, `on_trigger_overlap_count`. Triggers que afectan físicas (no solo notifican).

**F2H28 — Cloth y soft body básico.**
Cloth simulado con masa-resorte, no con Jolt soft bodies (que son experimentales). Banderas, capas. Soft body básico para objetos deformables (gelatinas, bolsas).

### Sub-fase 2.5 — Sistemas de juego (F2H29 - F2H34)

**F2H29 — Sistema de diálogos.**
`DialogTree` asset con nodos (texto + opciones). Editor visual de árboles. Variables persistentes (`game.has_key=true`). Portrait + audio por línea. Panel "Dialog Editor" en el editor.

**F2H30 — Sistema de misiones / quests.**
`Quest` asset con: descripción, objetivos (subtasks), recompensas (XP, items, dialog flags), estado (Available/Active/Complete/Failed). Editor visual. UI in-game de quest log.

**F2H31 — Sistema de inventario.**
`ItemAsset` con: nombre, icono, descripción, tipo (consumible/arma/key/quest), stats. `InventoryComponent` con grid 2D estilo Resident Evil o lista. Pickup/drop API en Lua.

**F2H32 — Sistema de stats / RPG primitives.**
`StatsComponent` con HP, stamina, mana custom. Modifiers (buffs/debuffs) con duración. Combat: damage types, resistencias. Levels y XP curves.

**F2H33 — Save game extendido.**
Bumpea `.moodsave` a v3 con dialog state, quest state, inventory, stats. API Lua para `game.save()` y `game.load()`. Auto-save por trigger.

**F2H34 — Localización de strings de juego.**
Extender el i18n del F2H5 a strings de gameplay (no solo editor). Diálogos, items, quests todo localizable. Interface para que el juego elija idioma desde menú.

### Sub-fase 2.6 — Pipeline de contenido AI (F2H35 - F2H40)

**F2H35 — Mixamo importer dedicado.**
Detector de rigs de Mixamo (humanoides estándar). Auto-mapping a esqueleto canónico de MoodEngine. Importación de packs de animaciones (idle, walk, run, jump, attack). Demo: NPC con suite completa Mixamo.

**F2H36 — Blender MCP server setup.**
Documentar setup. Scripts Python en `tools/blender/` para tareas comunes: importar mesh de MoodEngine, exportar a glTF con presets, validar geometría. Integración Claude Code → Blender vía MCP.

**F2H37 — Sistema de armas procedurales.**
`WeaponAsset` modular: cañón + culata + mira + mod. Cada parte con stats (daño, recoil, accuracy). Generador de combinaciones aleatorias con seeds reproducibles. Visualización 3D de partes acopladas.

**F2H38 — Generador procedural de props.**
Tipos: rocas (variantes de mismo tipo), vegetación (hierbas, arbustos), barriles (texturas variantes). Pipeline Claude+Blender para generar packs.

**F2H39 — Pipeline Blender → MoodEngine automatizado.**
Watch script en `tools/blender_watcher/`. Detecta cambios en archivos `.blend` de un proyecto MoodEngine, los re-exporta a glTF, hot-reload en MoodEditor abierto.

**F2H40 — Validación automática de assets importados.**
Al importar mesh: validar que tiene UVs, que no tiene n-gons, que el rig es válido si tiene esqueleto, que las texturas referenciadas existen. Reportes en consola del editor.

### Sub-fase 2.7 — UI/UX final del editor (F2H41 - F2H44)

**F2H41 — Theming del editor.**
Light/dark mode. Color schemes guardables. Editor de theme custom con preview. Persistencia per-user.

**F2H42 — Atajos de teclado configurables.**
Sistema de keybindings con UI dedicada. Persistencia. Defaults estilo Unity / Source Hammer / Blender (presets).

**F2H43 — Tutorial in-app.**
Tour interactivo del editor para primer arranque. "Esto es el viewport, esto es el inspector...". Skip-able. Re-launch desde Help menu.

**F2H44 — Cierre Fase 2 + tag v2.0.0.**
Suite completa de tests pasa. Documentación completa al día. Release notes detalladas. Tag `v2.0.0`. Recapitulación del dev. Planning de Fase 3 (TBD: networking, terreno, world streaming, mod support).

---

## 5. Detalle del Fase 2 — Hito 1 (F2H1) — Reorganización arquitectónica

**Este es el primer hito de Fase 2 y arrancamos por acá.**

### Objetivo

Reorganizar `src/engine/`, `src/systems/` y `src/editor/` según la estructura de la sección 2 de este documento. **No agregar features. No cambiar comportamiento. Solo mover archivos y actualizar includes/CMakeLists.**

### Por qué primero

Tres razones técnicas duras:

1. **Antes de subdividir engine/csg/, engine/dialog/, engine/quest/** en hitos posteriores, la carpeta `engine/` tiene que estar limpia. Sumar más a una carpeta de 11K líneas es contaminar más.
2. **Los hitos de CSG (F2H9-F2H16) van a generar 3-5K líneas adicionales en `engine/world/csg/`.** Hay que tener la subcarpeta `world/` lista.
3. **Tests pasan al 100% antes de tocar el primer feature.** Sirve como red de seguridad para detectar cualquier regresión introducida durante la mudanza.

### Criterios de aceptación

- [ ] Estructura de carpetas en `src/engine/`, `src/systems/`, `src/editor/` coincide con la sección 2 de este documento.
- [ ] Todos los includes actualizados a las nuevas rutas.
- [ ] `CMakeLists.txt` actualizado: globs o listas de archivos consistentes con la nueva estructura.
- [ ] `cmake --preset windows-msvc-debug` configura sin errores.
- [ ] `cmake --build build/debug --config Debug` compila sin errores ni warnings nuevos.
- [ ] Suite de tests pasa con **exactamente** los mismos 319 casos / 6.613 aserciones (zero regression).
- [ ] MoodEditor.exe y MoodPlayer.exe ejecutan y se ven exactamente igual que en v1.0.0.
- [ ] `docs/ARCHITECTURE.md` actualizado con la nueva estructura.
- [ ] `docs/DECISIONS.md` con entrada documentando la reorganización.
- [ ] Commit limpio (puede ser uno solo `refactor: reorganizar src/engine y src/editor en subcarpetas (F2H1)` o varios commits ordenados por subsistema).
- [ ] Tag `v1.1.0-fase2-hito1` al cerrar.

### Plan de ejecución sugerido

El agente debe trabajar **subsistema por subsistema** para no romper todo a la vez. Orden sugerido:

**Bloque A — `engine/render/`.**
Dividir en `rhi/`, `backend/opengl/`, `pipeline/`, `passes/`, `resources/`, `debug/`, `scene_renderer/`. Mover archivos. Actualizar includes en todo el proyecto. Compilar. Pasar tests. Commit.

**Bloque B — `engine/scene/`.**
Dividir en `core/`, `components/`, `serialization/`, `queries/`. Mismo flujo.

**Bloque C — `engine/physics/`.**
Crear subcarpeta. Mover `PhysicsWorld` actual + componentes físicos desde `Components.h`. Subdividir en `world/`, `components/`, `character/`, `queries/`.

**Bloque D — `engine/animation/`, `engine/audio/`, `engine/scripting/`, `engine/assets/`.**
Subdividir cada uno.

**Bloque E — `engine/world/`.**
Crear estructura `grid/` y mover `GridMap` actual. Crear placeholder `csg/` con `.gitkeep`.

**Bloque F — `engine/game/`, `engine/packaging/`, `engine/saving/`, `engine/i18n/`.**
Reorganizar y crear subcarpetas placeholder para hitos futuros (`dialog/`, `quest/`, `inventory/`).

**Bloque G — `systems/`.**
Subdividir por dominio.

**Bloque H — `editor/`.**
Dividir application, ui, panels (con sub-categorías scene/assets/debug/world), commands, tools, overlay.

**Bloque I — Tests + docs + tag.**
Verificar suite, actualizar `ARCHITECTURE.md` y `DECISIONS.md`, tag final.

### Reglas operativas durante F2H1

- **Una operación de mover por commit.** Esto facilita revisar y revertir si algo se rompe.
- **Compilar después de cada bloque.** No avanzar si no compila.
- **Tests pasan al final de cada bloque.** Si rompió algo, fix antes de seguir.
- **Si un archivo crece más allá de 800 líneas durante la mudanza,** se divide en el mismo hito (oportunidad de oro).
- **No tocar lógica.** Esto es estrictamente una reorganización.
- **CMakeLists puede pasar de listas explícitas a globs `file(GLOB_RECURSE ...)`** si simplifica. Usar `CONFIGURE_DEPENDS` para que CMake re-detecte cambios.

### Qué NO hacer en F2H1

- No agregar features nuevas.
- No refactorizar interfaces (ej: cambiar firmas de funciones).
- No "limpiar" código de paso (eso es deuda técnica para hitos posteriores específicos).
- No cambiar nombres de clases o tipos públicos.
- No tocar shaders.
- No tocar tests salvo para actualizar includes.

### Entregables del F2H1

1. Reorganización completa según sección 2.
2. Suite verde, motor funcionando idéntico a v1.0.0.
3. Documentación actualizada.
4. Tag `v1.1.0-fase2-hito1`.
5. Reporte al desarrollador con: cuántos archivos movidos, qué decisiones se tomaron en bordes ambiguos, cuáles fueron los includes más complicados.

### Resumen ejecutivo del F2H1 para el agente

Reorganizá `src/engine/`, `src/systems/` y `src/editor/` en subcarpetas según la sección 2 de `docs/PLAN_FASE2.md`. Trabajá bloque por bloque (render, scene, physics, animation, audio, scripting, assets, world, game/utils, systems, editor). Mové archivos, actualizá includes, compilá después de cada bloque, asegurate que los 319 tests sigan verdes. No agregues features ni cambies lógica. Commit por bloque. Al cerrar: actualizá `ARCHITECTURE.md` y `DECISIONS.md`, tag `v1.1.0-fase2-hito1`, reportá al dev.

---

## 6. Decisiones técnicas mayores de Fase 2

Lista de decisiones importantes que se tomarán en distintos hitos:

### Sobre CSG (F2H9 - F2H16)

**Algoritmo:** clipping de planos basado en BSP, similar a TrenchBroom. No usar libraries externas (Carve, libcsg) en primera iteración para mantener control total. Evaluar import de Carve o similar si performance pincha.

**Datos:** brushes son arrays de planos (representación implícita por intersección de half-spaces). Cada cara es una intersección de planos. UVs por cara con world-locked opcional.

**Compilación:** al guardar mapa, brushes → mesh estática consolidada con vertex welding y eliminación de caras internas. Mesh va a `mood_engine_lib` para que el Player la cargue sin código de CSG.

### Sobre Material Node Graph (F2H17 - F2H18)

**Generación de shader:** code-gen GLSL en runtime. Cache por hash del grafo (xxhash). Templates de vertex shader fijos, fragment shader generado del grafo.

**Persistencia:** `.material` JSON con array de nodos + array de conexiones. Versión de schema.

### Sobre Diálogos / Quests / Inventory (F2H29 - F2H32)

**Modelo:** asset-based, como meshes. Editor visual para cada tipo. Runtime carga assets y los expone a Lua.

**Persistencia de estado:** todo el estado dinámico (qué quests están activas, qué items tiene el player, qué diálogos vio) va al `.moodsave` extendido v3.

### Sobre Pipeline AI (F2H35 - F2H40)

**Mixamo:** importador específico que reconoce el rig "mixamorig:" y lo mapea a un rig canónico de MoodEngine ("mood_humanoid").

**Blender MCP:** depende del proyecto `blender-mcp` u otro equivalente activo. Setup documentado, no integrado en el motor (el motor solo carga los glTF resultantes).

**Generación procedural:** scripts Python en `tools/blender/` que Claude puede invocar. No hay AI runtime en el motor.

---

## 7. Stack adicional (librerías nuevas en Fase 2)

Nuevas dependencias a agregar vía CPM en Fase 2:

| Librería | Hito | Para qué |
|---|---|---|
| **Tracy** | F2H2 | Profiling cliente-servidor |
| **meshoptimizer** | F2H4 | Generación automática de LODs y optimización de meshes |
| **xxhash** | F2H18 | Hashing rápido para cache de shaders |
| **ImNodes** | F2H17 | Node graph editor sobre ImGui (alternativa: imgui-node-editor de Thedmd, más potente) |
| **OpenAL Soft o stay miniaudio + manual 3D** | F2H28 si hace falta | Audio posicional avanzado (a evaluar) |

**Librerías ya integradas que se sigue usando sin cambios:** SDL2, GLM, Dear ImGui, spdlog, doctest, nlohmann/json, portable_file_dialogs, EnTT, Lua, sol2, JoltPhysics, assimp, GLAD, stb.

---

## 8. Métricas de calidad continuas

Estas métricas se monitorean en cada hito de Fase 2 y se documentan en `ESTADO_ACTUAL.md`:

**Build & Test:**
- Suite de tests: número de casos / aserciones, todos verdes.
- Tiempo de compilación clean Debug: debe mantenerse <60s. Si excede, hito de optimización CMake.
- Warnings: cero en código propio (warnings de dependencias OK por ahora).

**Performance:**
- FPS en escena de stress-test (definir en F2H2): debe mantenerse o mejorar entre hitos.
- Memoria RAM en arranque del editor: registrar y monitorear.
- Tamaño del ejecutable Release: registrar y monitorear.

**Código:**
- Líneas totales en `src/` (informativo, no objetivo).
- Archivos `.cpp` excediendo 800 líneas: cero idealmente.
- Cobertura de tests (informativo): se mide manualmente al cierre de cada sub-fase.

**Documentación:**
- `DECISIONS.md` actualizado en cada hito con decisiones no triviales.
- `HITOS.md` y `ESTADO_ACTUAL.md` actualizados al cerrar cada hito.
- En sub-fase 2.5+, `USER_GUIDE/` se mantiene al día con cada feature visible para el usuario.

---

## NOTA FINAL PARA EL AGENTE

Este documento es la fuente de verdad de Fase 2 de MoodEngine. Antes de implementar cualquier hito de Fase 2, leer este documento completo y la sección específica del hito.

**Tu primera tarea es F2H1** especificado en la sección 5. Trabajá bloque por bloque, compilá después de cada bloque, asegurate que los 319 tests sigan verdes, y commit por bloque.

Al terminar F2H1, reportá al desarrollador y esperá confirmación antes de arrancar F2H2.

**Buena suerte. Construyamos Fase 2.**
