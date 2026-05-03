# Arquitectura de MoodEngine

> Documento vivo. La fuente de verdad detallada es `MOODENGINE_CONTEXTO_TECNICO.md` y `PLAN_FASE2.md`. Aquí se resume lo esencial para alguien que llega por primera vez al repo.

## Visión

MoodEngine es un motor gráfico 3D propio con editor visual integrado, escrito en C++17. Objetivo: juegos FPS como primer caso de uso, RPG más adelante. Calidad visual objetivo: AAA 2010-2015.

## Principios

- **Separación estricta entre editor y runtime.** El runtime puede correr sin el editor.
- **Separación estricta entre motor y juego.** El motor expone una API; el juego la consume.
- **Una sola aplicación, dos modos.** `MoodEditor.exe` corre en Editor Mode o Play Mode. `MoodPlayer.exe` solo Play.
- **Abstracción de API gráfica (RHI).** `engine/render/backend/opengl/` es el único lugar que incluye `glad/gl.h`. El resto del motor es agnóstico de OpenGL.
- **Entity-Component-System** como modelo de escena (EnTT detrás de una fachada propia).
- **Carpetas como dominios.** Cada subsistema vive en una carpeta dedicada con sub-carpetas internas (resources, queries, components, etc.). Soft-cap 500 líneas / `.cpp`, hard-cap 800.

## Estructura del código (post-F2H1)

```
src/
├── core/                      # Núcleo: log, math, tiempo, tipos
├── platform/                  # SDL2, ventana, VFS
│
├── engine/
│   ├── render/
│   │   ├── rhi/               # Interfaces puras (IRenderer, IShader, ITexture, ...)
│   │   ├── backend/opengl/    # Implementación GL (único include de glad/gl.h)
│   │   ├── pipeline/          # Fog, LightGrid (Forward+), PbrMath, ShadowMath
│   │   ├── passes/            # (placeholder; passes viven hoy en systems/render/)
│   │   ├── resources/         # MeshAsset, MaterialAsset
│   │   ├── debug/             # (placeholder para IDebugRenderer abstracto)
│   │   └── scene_renderer/    # SceneRenderer (coordinador del frame)
│   │
│   ├── scene/
│   │   ├── core/              # Scene, Entity, EditorCamera, FpsCamera
│   │   ├── components/        # Components.h (Tag/Transform/MeshRenderer/Light/...)
│   │   ├── serialization/     # Scene/Entity/Prefab/Project serializers
│   │   └── queries/           # ScenePick, ViewportPick
│   │
│   ├── physics/
│   │   ├── world/             # PhysicsWorld (wrapper Jolt)
│   │   ├── components/        # (placeholder F2H23+)
│   │   ├── character/         # (placeholder)
│   │   └── queries/           # (placeholder; raycast vive hoy en world/)
│   │
│   ├── animation/
│   │   ├── skeleton/          # Skeleton.h
│   │   ├── clips/             # AnimationClip.h
│   │   └── animator/          # (placeholder)
│   │
│   ├── audio/
│   │   ├── device/            # AudioDevice + miniaudio_impl
│   │   ├── clips/             # AudioClip
│   │   └── sources/           # (placeholder)
│   │
│   ├── scripting/
│   │   ├── runtime/           # (placeholder; ScriptSystem vive hoy en systems/)
│   │   ├── bindings/          # LuaBindings (sol2 → Lua)
│   │   └── exposed/           # ExposedProperty (Hito 24)
│   │
│   ├── assets/
│   │   ├── manager/           # AssetManager
│   │   ├── loaders/           # MeshLoader (assimp) + stb_impl
│   │   └── primitives/        # PrimitiveMeshes
│   │
│   ├── world/
│   │   ├── grid/              # GridMap + Pathfinding (A*)
│   │   ├── csg/               # (placeholder F2H9-F2H16: brushes 3D)
│   │   └── streaming/         # (placeholder Fase 3)
│   │
│   ├── game/                  # Sistemas engine-side de juego
│   │   ├── manifest/          # GameManifest (game.json del MoodPlayer)
│   │   ├── overlay/           # GameOverlay (HUD + pause menu)
│   │   ├── state/             # GameState (HudState + paused flag)
│   │   ├── dialog/            # (placeholder F2H29)
│   │   ├── quest/             # (placeholder F2H30)
│   │   └── inventory/         # (placeholder F2H31)
│   │
│   ├── packaging/             # PackageBuilder (empaquetado standalone)
│   ├── saving/                # SaveLoad (.moodsave)
│   └── i18n/                  # (placeholder F2H5: T(key) + lang/*.json)
│
├── systems/                   # Sistemas que iteran scene cada frame
│   ├── render/                # PostProcessPass, ShadowPass, SkyboxRenderer
│   ├── physics/               # PhysicsSystem, TriggerSystem
│   ├── animation/             # AnimationSystem
│   ├── ai/                    # NavSystem
│   ├── particles/             # ParticleSystem
│   ├── audio/                 # AudioSystem
│   ├── light/                 # LightSystem
│   └── scripting/             # ScriptSystem
│
├── editor/
│   ├── application/           # EditorApplication + partials (Overlay, PlayMode, etc.)
│   ├── ui/                    # EditorUI, Dockspace, MenuBar, StatusBar
│   ├── panels/
│   │   ├── scene/             # Hierarchy, Inspector, Viewport, InspectorEditTracker
│   │   ├── assets/            # Browser, MaterialEditor, ScriptEditor
│   │   ├── debug/             # Console, LuaApi
│   │   └── world/             # (placeholder F2H9+: MapEditor, BrushTools)
│   ├── commands/              # HistoryStack + ICommand + EditTransform/Delete/Create/Property
│   ├── tools/                 # (placeholder: gizmos extraibles, brush tools)
│   └── overlay/               # (placeholder: refactor del overlay si crece)
│
├── player/                    # MoodPlayer (runtime standalone)
└── game/                      # (vacío, para juegos del usuario)
```

## Capas y reglas de dependencia

```
┌─────────────────────────────┐
│   editor/        player/    │  ← UI del editor / runtime standalone
├─────────────────────────────┤
│           systems/          │  ← Sistemas ECS por dominio
├─────────────────────────────┤
│           engine/           │  ← Render, scene, physics, audio, scripting, assets, ...
│  ┌──────────────────────┐   │
│  │  engine/render/rhi/  │   │  ← Interfaces puras (sin GL)
│  │  engine/render/      │   │
│  │   backend/opengl/    │   │  ← Único lugar con glad/gl.h
│  └──────────────────────┘   │
├─────────────────────────────┤
│           platform/         │  ← SDL2, ventana, VFS, filesystem
├─────────────────────────────┤
│           core/             │  ← Logging, math, tiempo, tipos
└─────────────────────────────┘
```

**Reglas operativas:**
- `engine/render/backend/opengl/` es el **único** lugar que incluye `glad/gl.h`.
- `engine/render/rhi/` define interfaces puras, sin includes de OpenGL.
- `editor/` puede usar `engine/` y `systems/`, pero `engine/` y `systems/` nunca incluyen nada de `editor/`.
- `player/` puede usar `engine/` y `systems/`, pero nunca `editor/`.
- `systems/` puede usar `engine/`, pero `engine/` nunca incluye `systems/`.

Cualquier violación se trata como bug y se arregla en el mismo hito.

## Loop principal

```
while running:
    processEvents()
    if Play Mode: scene.update(dt); physics.step(dt); audio.update(dt)
    else:         editor.update(dt)
    sceneRenderer.renderScene(scene, camera)
    if Editor Mode: editor.drawUI()
    present()
```

## Extensiones de archivo propias

- `.moodproj` — proyecto
- `.moodmap` — mapa / escena
- `.moodprefab` — plantilla reutilizable de entidad
- `.moodsave` — estado runtime de juego (Hito 38)
- `.material` — material PBR (Hito 17)

## Estado actual (post-F2H1)

Estructura reorganizada según el plan de Fase 2 (sub-fase 2.1). El motor pasa de `engine/` flat a una estructura jerárquica por dominio que escala a las próximas 43 sub-tareas (CSG, materiales node-graph, dialog/quest/inventory, Mixamo importer, etc.). 319 tests / 6613 aserciones pasando. Editor + MoodPlayer ejecutan idéntico a v1.0.0 (zero regression).
