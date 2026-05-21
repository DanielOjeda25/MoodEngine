"""
Split wheels: centra cada mesh-nodo de rueda en su hub + lo renombra al
nombre canonico (wheel_FL / wheel_FR / wheel_RL / wheel_RR) que el
VehicleSystem espera como sub-mesh selector.

Contexto: el pipeline de F2H69 hornea (flatten) todas las node transforms al
vertex buffer, dejando las ruedas en su posicion world dentro del mesh
consolidado. Para que roten visualmente independientes del chassis, cada
rueda tiene que ser un sub-mesh con su origen en el centro del hub (asi el
TransformComponent.worldMatrix de la wheel-entity la posiciona+rota desde
cero). Este script:

  1. Detecta los nodos cuyo nombre (stripped) empieza con "wheel".
  2. Calcula el centroide world de cada uno y se lo resta a los vertices
     (origen = hub). Las normales no cambian (la traslacion no las afecta).
  3. Renombra el nodo al canonico FL/FR/RL/RR clasificando por posicion en
     espacio FISICA (rota el centroide por --yaw para deshacer el
     mesh_yaw_offset_deg del .moodvehicle; convencion engine +Z forward,
     +X right).

Uso:
    python tools/glb/split_wheels.py <input.glb> [-o out.glb] [--yaw 180]
"""
import argparse
import math
from pathlib import Path

import numpy as np
import pygltflib

from common import node_local_matrix, read_vec3_array, write_vec3_array


def classify(px, pz):
    """Devuelve el nombre canonico segun la posicion en espacio fisica.

    front = pz > 0 (engine: +Z forward), right = px > 0 (+X right)."""
    front = pz > 0.0
    right = px > 0.0
    if front and not right:
        return "wheel_FL"
    if front and right:
        return "wheel_FR"
    if (not front) and (not right):
        return "wheel_RL"
    return "wheel_RR"


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input", type=Path)
    ap.add_argument("-o", "--output", type=Path, default=None,
                    help="Default: sobrescribe el input.")
    ap.add_argument("--yaw", type=float, default=180.0,
                    help="mesh_yaw_offset_deg del .moodvehicle (default 180 "
                         "para el DeLorean Sketchfab). Usado solo para "
                         "clasificar FL/FR/RL/RR, no modifica geometria.")
    args = ap.parse_args()
    out = args.output or args.input

    gltf = pygltflib.GLTF2().load(str(args.input))
    blob = bytearray(gltf.binary_blob())

    # nodo_idx -> matriz world acumulada (para leer centroides correctos
    # aunque el grafo no estuviera flatten).
    world_of = {}

    def walk(idx, parent_M):
        node = gltf.nodes[idx]
        M = parent_M @ node_local_matrix(node)
        world_of[idx] = M
        for ch in (node.children or []):
            walk(ch, M)

    for root in gltf.scenes[gltf.scene or 0].nodes:
        walk(root, np.eye(4, dtype=np.float64))

    # yaw -> rotacion en el plano XZ para mapear mesh-space a physics-space.
    th = math.radians(args.yaw)
    cs, sn = math.cos(th), math.sin(th)

    found = []
    for idx, node in enumerate(gltf.nodes):
        if node.mesh is None:
            continue
        name = (node.name or "").strip()
        if not name.lower().startswith("wheel"):
            continue
        M = world_of[idx]
        mesh = gltf.meshes[node.mesh]
        # Centroide world del mesh (promedio de todas las primitivas).
        all_world = []
        for prim in mesh.primitives:
            if prim.attributes.POSITION is None:
                continue
            pos = read_vec3_array(gltf, blob, prim.attributes.POSITION)
            homog = np.hstack([pos, np.ones((pos.shape[0], 1))])
            all_world.append((M @ homog.T).T[:, :3])
        if not all_world:
            continue
        world_pts = np.vstack(all_world)
        centroid = world_pts.mean(axis=0)

        # Clasificar por posicion en physics-space (rotar centroide por yaw).
        mx, mz = centroid[0], centroid[2]
        px = mx * cs + mz * sn
        pz = -mx * sn + mz * cs
        canon = classify(px, pz)

        # Centrar los vertices: restar el centroide en LOCAL space del nodo.
        # Como las transforms estan flatten (identity), local == world; pero
        # por robustez convertimos el centroide world a local del nodo.
        Minv = np.linalg.inv(M)
        centroid_local = (Minv @ np.array([*centroid, 1.0]))[:3]
        for prim in mesh.primitives:
            if prim.attributes.POSITION is None:
                continue
            pos = read_vec3_array(gltf, blob, prim.attributes.POSITION)
            pos = pos - centroid_local
            write_vec3_array(gltf, blob, prim.attributes.POSITION, pos)

        node.name = canon
        found.append((name, canon, centroid))

    if len(found) != 4:
        print(f"WARN: se esperaban 4 ruedas, se encontraron {len(found)}:")
    for orig, canon, c in found:
        print(f"  {orig:36s} -> {canon}  (hub world=({c[0]:+.3f},{c[1]:+.3f},{c[2]:+.3f}))")

    gltf.set_binary_blob(bytes(blob))
    gltf.save(str(out))
    print(f"OK -> {out}")


if __name__ == "__main__":
    main()
