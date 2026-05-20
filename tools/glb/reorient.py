"""
Reorient GLB rotando todos los vertex positions + normales un yaw fijo
alrededor del eje Y.

Caso de uso tipico: el modelo viene mirando a -Z (frente del auto en la
direccion negativa de Z, comun en exports FBX 3DS Max). El engine
MoodEngine (y Jolt VehicleConstraint con mForward=+Z) espera +Z forward.
Con `--yaw-deg 180` el modelo queda alineado con la convencion.

Como es una rotacion pura (det=+1) no hace falta invertir winding ni
flippear normales — solo rotar normales junto a las posiciones.

Patron industrial: equivalente headless a "Object > Apply > Rotation" en
Blender despues de un rotate manual.

Uso:
    python tools/glb/reorient.py <input.glb> [--yaw-deg 180] [--in-place]
"""
import argparse
import math
from pathlib import Path

import numpy as np
import pygltflib

from common import (
    collect_mesh_world_transforms,
    read_vec3_array,
    write_vec3_array,
)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("input", type=Path)
    ap.add_argument("--yaw-deg", type=float, default=180.0,
                    help="Yaw alrededor de +Y (grados). Default 180 = mira opuesto.")
    ap.add_argument("--in-place", action="store_true")
    args = ap.parse_args()

    out = args.input if args.in_place else args.input.with_suffix(".reoriented.glb")

    print(f"Loading {args.input}  yaw={args.yaw_deg}deg")
    gltf = pygltflib.GLTF2().load(str(args.input))
    blob = bytearray(gltf.binary_blob())

    theta = math.radians(args.yaw_deg)
    c, s = math.cos(theta), math.sin(theta)
    R = np.array([
        [c, 0.0, s, 0.0],
        [0.0, 1.0, 0.0, 0.0],
        [-s, 0.0, c, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ], dtype=np.float64)
    R3 = R[:3, :3]

    # Solo procesamos meshes referenciadas (1 vez por accessor, sin importar
    # cuantos nodes la usan — la rotacion es global).
    mesh_refs = collect_mesh_world_transforms(gltf)

    processed_pos = set()
    processed_nrm = set()

    for mesh_idx in mesh_refs:
        for prim in gltf.meshes[mesh_idx].primitives:
            pos_idx = prim.attributes.POSITION
            nrm_idx = getattr(prim.attributes, "NORMAL", None)

            if pos_idx is not None and pos_idx not in processed_pos:
                processed_pos.add(pos_idx)
                positions = read_vec3_array(gltf, blob, pos_idx)
                homog = np.hstack([positions, np.ones((positions.shape[0], 1))])
                write_vec3_array(gltf, blob, pos_idx, (R @ homog.T).T[:, :3])

            if nrm_idx is not None and nrm_idx not in processed_nrm:
                processed_nrm.add(nrm_idx)
                normals = read_vec3_array(gltf, blob, nrm_idx)
                rotated = (R3 @ normals.T).T
                lens = np.linalg.norm(rotated, axis=1, keepdims=True)
                lens = np.where(lens < 1e-12, 1.0, lens)
                write_vec3_array(gltf, blob, nrm_idx, rotated / lens)

    # Tambien rotar translations de nodos para que las posiciones de las
    # wheels (en caso de que el GLB todavia tenga node transforms no horneados
    # — ej. salida directa de scale.py sin flatten) queden consistentes.
    for node in gltf.nodes:
        if node.translation is not None:
            t = np.array([*node.translation, 1.0], dtype=np.float64)
            new_t = R @ t
            node.translation = [float(new_t[0]), float(new_t[1]), float(new_t[2])]
        # Si hay matrix completa, multiplicamos por R por izquierda.
        if node.matrix is not None:
            m = np.array(node.matrix, dtype=np.float64).reshape(4, 4, order='F')
            m = R @ m
            node.matrix = m.flatten(order='F').tolist()

    print(f"Rotated {len(processed_pos)} position accessor(s), "
          f"{len(processed_nrm)} normal accessor(s).")

    gltf.set_binary_blob(bytes(blob))
    gltf.save_binary(str(out))
    print(f"Saved {out}")


if __name__ == "__main__":
    main()
