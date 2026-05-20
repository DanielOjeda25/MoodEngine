# asset_conventions — Convenciones de assets de MoodEngine

> Documento canónico de cómo deben venir los assets que se importan al
> engine. Inspirado en Unity (Project Settings → Player → 1 unit = 1m)
> y en la convención glTF 2.0. Sigue los mismos principios que Unreal
> (cm en lugar de m) pero unificamos en **metros** para evitar conversión
> mental constante.

---

## 1. Sistema de coordenadas y unidades

| campo         | valor                          |
| ------------- | ------------------------------ |
| Unidad        | **1 unit = 1 metro**           |
| Up            | **+Y**                         |
| Forward       | **+Z**                         |
| Right         | **+X**                         |
| Handedness    | Right-handed (estándar glTF)   |

**Por qué metros y no cm o pies arcade**: las specs reales de los assets
(autos, NPCs, edificios) vienen en metros en datasheets; tunear física a
escala real evita "feel arcade-floppy" como en GTA SA pre-escala. Ver
[memoria de medidas industriales](../C:/Users/Daniel/.claude/projects/c--Users-Daniel-Documents-GitHub-MoodEngine/memory/feedback_medidas_industriales.md).

**Cómo verificarlo en Blender** antes de exportar: el modelo del DeLorean
DMC-12 debe medir ~4.22m en el eje largo (eje Z post-export glTF). Si
mide 422 o 0.422, está en cm o dm — escalar antes de exportar o usar
`tools/glb/scale.py`.

---

## 2. Origin del modelo

**Libre**. El engine absorbe cualquier convención de origin via
`TransformComponent.pivotYOffset`, calculado al cargar el `MeshAsset` como
`-aabbMin.y` (ver [VehicleSystem::chassisRenderYOffset](../src/systems/physics/VehicleSystem.cpp#L91-L102)).

Resultado: el dev puede escribir `position: [0, 0, 0]` en el moodmap y el
modelo apoya correctamente en el piso, sin importar si el origin del GLB
está en la base, en el centro vertical, o en cualquier otro lado.

Quien quiera uniformidad puede correr `tools/glb/center_y.py` para fijar
el origin al centro vertical, pero **no es requisito**.

---

## 3. Sin mirrors baked (det positivo en todos los nodes)

El grafo de nodos del GLB **no debe contener matrices con `det < 0`**.

Las exportaciones FBX → GLB de software comercial (3DS Max, Maya) a veces
bakean scales negativos para producir "espejos" en lugar de duplicar
geometría. Eso invierte el winding triangular y deja al renderer con
backface culling viendo el interior del modelo (caras "huecas").

**Cómo detectar**: `python tools/glb/diag.py <model.glb>` imprime el árbol
de nodos con `det(M)` local y world. Cualquier valor negativo es bug.

**Cómo arreglar**: `python tools/glb/flatten.py <model.glb> --in-place`
bakea las matrices a vertex data e invierte el winding + normales en los
meshes afectados.

---

## 4. Forward axis = +Z

El frente del modelo (la dirección "natural" hacia donde apunta — para un
vehículo, el morro) debe coincidir con **+Z**.

**Por qué +Z y no -Z**: glTF 2.0 estándar y Jolt `VehicleConstraint`
ambos usan `mForward = +Z` ([PhysicsWorld_Vehicle.cpp:126-127](../src/engine/physics/world/PhysicsWorld_Vehicle.cpp#L126-L127)). Mantener la
convención del estándar evita compensaciones manuales (e.g.,
`rotationEuler: [0, 180, 0]` en el moodmap) que son frágiles y se
copian/duplican por cada nuevo asset.

**Cómo detectar**: si `verify.py` reporta "Eje largo del AABB es X" y se
trata de un vehículo, probablemente venga de un export con convención
distinta (e.g., 3DS Max suele usar +Y forward, Blender +Y o -Y según el
exporter).

**Cómo arreglar**: `python tools/glb/reorient.py <model.glb> --yaw-deg 180 --in-place`
rota el modelo y hornea la rotación al vertex buffer. El yaw exacto
depende de la convención del modelo original:

| modelo viene mirando | rotar con `--yaw-deg` |
| -------------------- | --------------------- |
| `-Z`                 | `180`                 |
| `+X`                 | `-90`                 |
| `-X`                 | `90`                  |
| `+Z` (ya OK)         | (no hace falta)       |

---

## 5. Escala = metros (no cm, no mm, no "Blender units")

Tras exportar, el AABB del modelo en world space (ver `verify.py`) tiene
que estar en metros plausibles para su categoría:

| tipo            | rango razonable de dimensiones |
| --------------- | ------------------------------ |
| Vehículo sedan  | 4 – 5 m largo, 1.5 – 2 m ancho |
| Vehículo sport  | 4 – 4.5 m largo, ~2 m ancho    |
| NPC humano      | 1.6 – 2 m alto                 |
| Edificio chico  | 5 – 20 m                       |

Si el AABB reporta dimensiones absurdas (`<0.1m` o `>100m` para algo que
debería ser estándar), corregir con `tools/glb/scale.py`.

---

## 6. Naming canónico de sub-meshes

Para que los sistemas del engine puedan identificar partes del modelo por
nombre (sin heurísticas frágiles), los sub-meshes siguen nombres
canónicos:

### Vehículos

| sub-mesh  | rol                                |
| --------- | ---------------------------------- |
| `chassis` | cuerpo principal (collision shape) |
| `wheel_FL`| rueda delantera izquierda          |
| `wheel_FR`| rueda delantera derecha            |
| `wheel_RL`| rueda trasera izquierda            |
| `wheel_RR`| rueda trasera derecha              |
| `interior`| interior (driver POV, opcional)    |
| `door_FL` `door_FR` `door_RL` `door_RR` | puertas (opcional, futuro destructible) |

**Por qué nombres en lugar de tags por orden**: si el modelo viene de un
sitio donde el dev hizo el rig (Sketchfab, KitBash3D, etc.), no podemos
asumir orden. El nombre del node es el único identificador estable.

### Caracteres / NPCs

(scope F2H7X — pendiente de definir cuando se trabaje en pipeline de
characters/animaciones.)

---

## 7. Pipeline recomendado al ingresar un asset nuevo

```bash
# 1. Bajar el GLB y ponerlo en una carpeta scratch.
# 2. Diagnostico.
python tools/glb/verify.py scratch/new_car.glb

# 3. Aplicar correcciones segun lo que verify reporte:
python tools/glb/scale.py scratch/new_car.glb 1.86 --in-place   # si escala mala
python tools/glb/flatten.py scratch/new_car.glb --in-place       # si det<0
python tools/glb/reorient.py scratch/new_car.glb --yaw-deg 180 --in-place  # si forward != +Z

# 4. Verificar de nuevo.
python tools/glb/verify.py scratch/new_car.glb

# 5. Renombrar sub-meshes a convencion canonica (Blender o Asset editor).
#    Esto requiere herramienta visual; no hay script headless aun.

# 6. Mover al destino final.
mv scratch/new_car.glb assets/vehicles/new_car/new_car.glb
```

---

## 8. Lo que NO es convencion (libre)

- **Material/PBR**: el engine acepta cualquier material PBR estándar
  glTF 2.0. No hay restricción de naming en materiales.
- **Texturas**: cualquier formato glTF estándar (PNG/JPEG embebido o
  externo).
- **LODs**: aún no soportados (F2H7X+).
- **Animaciones**: aún no soportadas para vehículos. Para characters,
  cuando se implemente, definir convención de nombre de bones aparte.

---

## 9. Referencias industriales

- [glTF 2.0 spec](https://github.com/KhronosGroup/glTF/tree/main/specification/2.0) — convención +Y up, +Z forward, right-handed.
- [Unity asset import](https://docs.unity3d.com/Manual/HOWTO-importObject.html) — base del patrón "1 unit = 1 meter".
- [Source Engine `scripts/vehicles/`](https://developer.valvesoftware.com/wiki/Vehicle_scripting) — patrón data-driven que `.moodvehicle` v2 replica.
- [Jolt `VehicleConstraint`](https://github.com/jrouwe/JoltPhysics/blob/master/Jolt/Physics/Vehicle/VehicleConstraint.h) — `mForward = +Z` por default.
