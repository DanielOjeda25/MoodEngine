# MoodEngine `v2.0.0` — Cierre de Fase 2

**Fecha:** 2026-05-23
**Tag:** `v2.0.0`
**Hito final de Fase 2:** F2H86 (`v1.77.0-fase2-hito86`).

---

## Qué es este documento

Recap de la Fase 2 al sellar `v2.0.0`. **No planea Fase 3** — el dev pidió
arrancar de cero. El backlog quedó vaciado al cierre (ver `BACKLOG.md`)
y las memorias de pendientes operativos fueron limpiadas.

---

## Tamaño de la fase

- **88 tags** de hito en Fase 2 (`v1.x.0-fase2-hito*`).
- **86 hitos numerados** + 3 audits (AUDIT-1 / AUDIT-2 / AUDIT-3) + breaks
  intermedios.
- **Suite de tests final**: 1078 casos / 11119 asserts verde.
- **7 sub-fases** completadas, todas selladas:
  - Sub-fase 2.1 — Audio + Asset pipeline.
  - Sub-fase 2.2 — Render avanzado (CSM, SSAO, Bloom, Color Grading, SSR,
    OIT).
  - Sub-fase 2.3 — Editor de mapas + brushes CSG.
  - Sub-fase 2.4 — Física avanzada (joints, ragdolls, force fields,
    vehículos, cloth).
  - Sub-fase 2.5 — Diálogos + Inventario + Quests.
  - Sub-fase 2.6 — Shader graph + ScriptComponent + NodeGraph.
  - Sub-fase 2.7 — UI/UX final + cierre.

---

## Bloques temáticos cerrados

### Render
- **PBR completo**: shading + IBL bakeado + 5 mip levels prefilter +
  BRDF LUT split-sum (Karis).
- **Shadow mapping CSM** con 4 cascadas, split-lambda configurable,
  enabled per-light via `LightComponent::castShadows`.
- **Forward+** tiled light culling con SSBO triple buffering.
- **Post-process stack**: Tonemap (ACES / Reinhard / None), Exposure,
  Bloom, SSAO, SSR, Color Grading LUT (Unity URP-style PNG 256x16).
- **OIT Weighted Blended** para translucent (F2H64).
- **Skybox + IBL swap runtime** (F2H86) — auto-detecta equirect vs cubemap
  dir, IBL bakeado offline con `tools/bake_ibl.py`.
- **Particles + Cloth + Trail** systems.

### Física (Jolt)
- **RigidBody** Static / Dynamic / Kinematic con shapes (Box, Sphere,
  Capsule, Mesh, CompoundConvex).
- **Character Controller** con friction + crouch + headbob.
- **Joints** (Hinge, Fixed, Distance, Slider, Cone, Path).
- **Ragdolls** con `JPH::Ragdoll` + auto-trigger por impacto vehicle ↔ NPC.
- **Force Fields** (Wind / Vortex / Radial / Directional).
- **Vehicles** con `JPH::VehicleConstraint`: anti-roll bars + CoM tuning +
  modal Importar GLB + Live tuning del Inspector.
- **Cloth** sample con compute-style integration.

### Gameplay
- **Diálogos** `.mooddialog` (nodos + condiciones + emoción del avatar).
- **Inventario** `.mooditem` + slot grid + drag&drop visual + UI HUD.
- **Quests** `.mooquest` + tracker + objetivos + recompensas.
- **AI / Navmesh** + pathfinding A*.
- **Triggers** con auto-events + Lua bindings.
- **GameState** + HUD modular (13 widgets en `k_widgets[]`).

### Scripting
- **Lua** via Sol2 con bindings completos (transform, physics, audio,
  particles, HUD, inventory, quest, dialog, raycast, scene queries).
- **Hot-reload** de scripts + shaders.
- **Exposed properties** Lua editables desde Inspector.
- **Shader Graph** (`.moodshader`) con cache de programas GL compilados +
  fallback transparente a PBR.

### Editor
- **Workspace switcher** (6 workspaces: scene / map_editor / scripting /
  materials / gameplay / narrative).
- **Modales** unificados (Crear Entidad / Convertir Entidad / Importar
  Vehículo / Welcome / Preferencias).
- **Asset Browser** con thumbnails 3D (meshes + primitivas + materiales)
  + drag&drop al viewport.
- **Inspector** con component cards plegables + remove component undoable.
- **HistoryStack** unificado (gizmo, delete, create, edit-property,
  edit-asset, add-component, brushes CSG).
- **Save As contextual** + Shift+D duplicate (F2H85).
- **Environment como entidad de primera clase** + HDRI swap runtime
  (F2H86).

### Infra
- **Layer audit** (AUDIT-1/2/3): cero violaciones cross-layer, HARD cap
  de líneas por archivo enforced.
- **AssetManager** con factory de texturas pluggable (NullTexture en
  tests headless).
- **SceneSerializer** versionado v1 → v6+ con upgrade automático.
- **PackageBuilder** standalone (`MoodPlayer`).
- **i18n** unificado en/es con `I18n::T(key, args...)`.
- **Themes** del editor (4 temas) + métricas unificadas.

---

## Limitaciones conocidas al cerrar Fase 2

Lo que **NO está en el motor** al cierre — capturado acá para evitar
sorpresas en Fase 3:

- **No hay multiplayer / networking** (excluido por scope explícito del
  dev en CLAUDE.md).
- **No hay i18n de la UI** (el `I18n::T` cubre keys pero la UI sigue
  hardcoded a Latin alphabet).
- **No hay cursores custom**.
- **Render backend = OpenGL 4.5 core** únicamente. No Vulkan / D3D12.
- **No hay runtime IBL bake** — HDRIs custom requieren `python
  tools/bake_ibl.py` offline.
- **SceneRenderer_Render.cpp** (978 LOC) no fue partido en F2H83 por
  riesgo render sin cobertura visual. Documentado en el cierre de F2H83.
- **Save As de Material / Script / Shader** quedó fuera de F2H85 (Item +
  Quest sí). Requieren refactor del AssetManager o de
  `ScriptComponent.path`.
- **Atajos de teclado configurables** (era F2H42 del plan original) —
  hardcodeados; diferido por decisión del dev.
- **Tutorial in-app** — diferido a post-Fase 2.

---

## Tags clave de Fase 2

| Tag | Hito | Resumen |
|---|---|---|
| `v1.30.0-fase2-hito40` | F2H40 | Triggers + auto-events. |
| `v1.41.0-fase2-hito53` | F2H53 | Cierre Sub-fase 2.5 (diálogo+inventario+quests). |
| `v1.49.3-audit-3` | AUDIT-3 | Layer audit final. |
| `v1.51.0-fase2-hito64` | F2H64 | OIT translucent. |
| `v1.55.0-fase2-hito68` | F2H68 | Auto-ragdoll por impacto. |
| `v1.66.0-fase2-hito75` | F2H75 | Cloth — cierre Sub-fase 2.4. |
| `v1.67.0-fase2-hito76` | F2H76 | Apertura Sub-fase 2.7. |
| `v1.73.0-fase2-hito82` | F2H82 | Modal Importar vehículo + live tuning. |
| `v1.77.0-fase2-hito86` | F2H86 | Environment first-class + HDRI swap. |

Lista completa: `git tag --list "v1.*-fase2-hito*"`.

---

## Cómo arrancar Fase 3

Cuando vuelvas a abrir el repo para Fase 3:

1. `docs/BACKLOG.md` está vacío — registrar items nuevos a medida que
   emerjan.
2. `docs/ESTADO_ACTUAL.md` queda como referencia del último estado
   pre-cierre. El primer hito de Fase 3 abrirá una nueva sección.
3. `docs/HITOS.md` mantiene el roadmap histórico — agregar
   `## Fase 3 — <Nombre TBD>` cuando se decida el rumbo.
4. Las **memorias del agente** (`~/.claude/projects/.../memory/`)
   conservan las preferencias operativas estables (auto-accept,
   commits-al-final, no-reinventar-rueda, etc.) — los pendientes
   operativos obsoletos fueron borrados.

Sin plan de Fase 3 acá por decisión del dev: *"no planees nada, solo
haz el cierre de la fase 2"*.
