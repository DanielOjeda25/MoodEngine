# tools/glb — Preprocesado headless de modelos GLB

Scripts Python para normalizar modelos GLB al ingreso al engine. Equivalentes
headless a operaciones de Blender ("Apply Scale", "Apply All Transforms",
"Object Rotation", "Origin to Geometry"), pensados para CI / pipelines
batch / fix asset-specific sin abrir Blender.

## Requisitos

```
pip install pygltflib numpy
```

## Pipeline recomendado

Dado un GLB recien bajado:

```bash
# 1. Diagnostico inicial — chequea AABB, eje largo, mirrors
python tools/glb/verify.py <model.glb>

# 2. Si AABB esta en cm/mm, escalarlo a metros (DeLorean = 4.22m largo)
python tools/glb/scale.py <model.glb> <factor> --in-place

# 3. Si verify reporto det<0 en algun node → bake transforms + fix winding
python tools/glb/flatten.py <model.glb> --in-place

# 4. Si verify reporto "eje largo es X" → rotar para que sea Z (+Z forward)
python tools/glb/reorient.py <model.glb> --yaw-deg 90 --in-place

# 5. Diagnostico final
python tools/glb/verify.py <model.glb>
```

## Scripts disponibles

| script        | proposito                                              |
| ------------- | ------------------------------------------------------ |
| `verify.py`   | AABB + chequeo de convencion (escala, forward, det<0)  |
| `diag.py`     | dump del arbol de nodos con dets local/world           |
| `scale.py`    | bake uniform scale a vertex positions + node trans     |
| `flatten.py`  | bake todas las matrices de node + corrige winding      |
| `center_y.py` | recentra origin al centro vertical del AABB            |
| `reorient.py` | rota yaw (default 180°) sobre Y y hornea               |
| `common.py`   | helpers compartidos (no es ejecutable)                 |

Ver el docstring de cada script para argumentos completos.

## Convencion del engine

Ver [`docs/asset_conventions.md`](../../docs/asset_conventions.md) para la
lista completa. Resumen:

- 1 unit = 1 metro
- +Y up, +Z forward (glTF 2.0 standard)
- Origin libre (el engine absorbe el offset Y via `pivotYOffset`)
- Sin det<0 en el grafo de nodos
- Naming canonico de wheels: `wheel_FL`, `wheel_FR`, `wheel_RL`, `wheel_RR`
