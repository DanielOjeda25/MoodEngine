"""
Verify GLB world-space AABB y reporta si cumple convencion del engine.

Convencion (ver docs/asset_conventions.md):
  - 1 unit = 1 metro
  - +Y up, +Z forward
  - Sin det<0 en el grafo de nodos

Uso:
    python tools/glb/verify.py <input.glb>
"""
import argparse
from pathlib import Path

import numpy as np
import pygltflib

from common import node_local_matrix, world_aabb


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("input", type=Path)
    args = ap.parse_args()

    print(f"Verifying {args.input.name}")
    gltf = pygltflib.GLTF2().load(str(args.input))
    blob = gltf.binary_blob()

    overall_min, overall_max = world_aabb(gltf, blob)
    dims = overall_max - overall_min

    print(f"World AABB min: [{overall_min[0]:7.3f}, {overall_min[1]:7.3f}, {overall_min[2]:7.3f}]")
    print(f"World AABB max: [{overall_max[0]:7.3f}, {overall_max[1]:7.3f}, {overall_max[2]:7.3f}]")
    print(f"Dimensions:     X={dims[0]:.3f}m  Y={dims[1]:.3f}m  Z={dims[2]:.3f}m")
    print(f"Magnitude (longest axis): {dims.max():.3f}m")

    # Warnings basicos contra la convencion
    warnings = []
    if dims.max() < 0.1:
        warnings.append("Modelo muy chico (<10cm). Probablemente falte escalar (ej en cm o mm).")
    if dims.max() > 100.0:
        warnings.append("Modelo muy grande (>100m). Probablemente escala fuera de la convencion 1u=1m.")

    # Vehicle heuristic: ¿el eje largo es Z? (+Z forward)
    long_axis = int(np.argmax(dims))
    axis_name = "XYZ"[long_axis]
    if axis_name != "Z":
        warnings.append(
            f"Eje largo del AABB es {axis_name}, no Z. Si esto es un vehiculo, "
            f"probablemente necesite `reorient.py` (engine espera forward = +Z).")

    # det<0 en algun node?
    def find_flipped(idx, parent_M, out):
        node = gltf.nodes[idx]
        M = parent_M @ node_local_matrix(node)
        if np.linalg.det(M[:3, :3]) < 0:
            out.append(node.name or f"node{idx}")
        for child in (node.children or []):
            find_flipped(child, M, out)

    flipped = []
    for root in gltf.scenes[gltf.scene or 0].nodes:
        find_flipped(root, np.eye(4), flipped)
    if flipped:
        warnings.append(
            f"{len(flipped)} node(s) con det<0 detectados (mirrors baked). "
            f"Corregir con `flatten.py`. Primeros: {flipped[:5]}")

    if warnings:
        print("\nWARNINGS:")
        for w in warnings:
            print(f"  - {w}")
    else:
        print("\nOK: cumple convencion del engine.")


if __name__ == "__main__":
    main()
