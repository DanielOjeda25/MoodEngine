# assets/characters — pipeline FBX (Mixamo)

Drop-folder para personajes con esqueleto + animaciones. El loader
(`src/engine/assets/loaders/MeshLoader*.cpp`) usa **assimp** con la
config necesaria para tragar FBX de Mixamo sin pasos manuales en
Blender.

## Formato soportado

| Formato | Estado | Notas |
|---------|--------|-------|
| `.glb` / `.gltf` | ✅ probado | Y-up, metros, texturas embedded |
| `.fbx` (Mixamo)  | ✅ soportado | Auto-scale cm→m, pivots colapsados |
| `.fbx` (otros)   | ✅ debería | Mismos flags; reportar issue si rompe |
| `.obj`           | ✅ probado | Sin esqueleto |

## Cómo descargar de Mixamo

1. **Personaje base** (un solo archivo, con malla + T-pose):
   - Format: **FBX Binary (.fbx)**
   - Pose: **T-pose**
   - Skin: **With Skin**

2. **Clips de animación** (uno por archivo, sin malla):
   - Format: **FBX Binary (.fbx)**
   - Skin: **Without Skin**
   - FPS: **30**
   - Keyframe Reduction: **none**

## Convención de carpetas

```
assets/characters/
  player/
    player.fbx              ← personaje base con T-pose + skin
    anim_idle.fbx           ← clip idle (without skin)
    anim_walk.fbx           ← clip walking In Place
    anim_wave.fbx           ← clip standing greeting
  npc/
    npc.fbx                 ← personaje base con T-pose + skin
    anim_idle.fbx           ← clip idle (without skin)
    anim_talk.fbx           ← clip talking
    anim_look_around.fbx    ← clip look around
  textures/                 ← texturas externas que Mixamo zippea aparte
    diffuse.png
```

**Convención de nombres**:
- `<carpeta>/<carpeta>.fbx` para el rig base (skin + T-pose).
- `<carpeta>/anim_<accion>.fbx` para clips standalone (sin skin, In Place).
- Locomoción (walk/run/jump) **siempre In Place** — el `CharacterController`
  desplaza, la animación solo "pisa".

## Demos incluidos en el repo

Por default los `.fbx` están gitignorados (Mixamo "with skin + textures
embedded" puede pesar >100 MB, sobre el límite de GitHub). Excepción: los
rigs **X Bot** (player) y **Y Bot** (npc) — pesan ~7.5 MB total y son
redistribuibles bajo el uso libre de Mixamo, así que vienen commiteados
para que el motor tenga personajes funcionales out-of-the-box:

```
assets/characters/player/*.fbx   ← X Bot + 3 clips
assets/characters/npc/*.fbx      ← Y Bot + 3 clips
```

Si dropeás tus propios FBX en otras subcarpetas (`assets/characters/hero/`,
`assets/characters/enemy/`, etc.), van a quedar gitignorados igual que antes.

## Cómo carga el motor

- **Rig base** (skin + T-pose):
  `AssetManager::loadMesh("characters/player/player.fbx")` devuelve un
  `MeshAsset` con `submeshes`, `skeleton` y (si trae) la primera animación
  embebida.
- **Clips standalone** (sin skin, F2H49):
  `AssetManager::loadAnimationClip("characters/player/anim_walk.fbx")`
  devuelve un `AnimationClipAsset` con tracks por `boneName`. Al bindearse
  contra un esqueleto concreto, el `AnimationSystem` resuelve los nombres
  a índices de bone (cacheado por skeleton).

## Nombres de clips

Los nombres FBX vienen sucios (`Armature|mixamo.com|Layer0`). El loader los
**sanitiza** a un identificador estable que podés usar desde scripting:

| Raw                            | Sanitizado     |
|--------------------------------|----------------|
| `Armature\|mixamo.com\|Layer0` | `Layer0`       |
| `mixamo.com`                   | `mixamo_com`   |
| `Take 001`                     | `Take_001`     |

Como queda variable, en hito futuro habría que renombrar clips al cargarlos
(ej. via metadata del .moodmap). Por ahora podés verificar el nombre
sanitizado en los logs del editor (`[assets] MeshLoader: ... (N clips)`).

## Troubleshooting

| Síntoma | Causa probable |
|---------|---------------|
| Personaje gigante (100× tamaño) | Mixamo cm sin convertir. El motor lo maneja con `aiProcess_GlobalScale`. Si pasa, abrir issue. |
| Esqueleto rotado 90° | Pivots residuales o orientación Z-up. Verificar `importRotationEuler` del log. |
| Animación no se mueve | Nombre de bone en clip no coincide con esqueleto base. Mixamo usa `mixamorig:` como prefijo — si bajaste el clip con otro rig, no va a matchear. |
| Texturas missing | Mixamo zippea texturas en carpeta `textures/` aparte. Hay que dropearlas al lado del FBX (ver convención de carpetas). |
