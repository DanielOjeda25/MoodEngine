"""
Diagnose GLB: imprime el arbol de nodos con `det(M_local)` y `det(M_world)`.

Valores negativos indican mirrors baked (scale negativo) que invierten el
winding visible en el render. Usado antes de `flatten.py` para confirmar
si hace falta corregir.

Uso:
    python tools/glb/diag.py <input.glb>
"""
import argparse
from pathlib import Path

import numpy as np
import pygltflib

from common import node_local_matrix


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("input", type=Path)
    args = ap.parse_args()

    gltf = pygltflib.GLTF2().load(str(args.input))

    def walk(idx, parent_M, depth=0):
        node = gltf.nodes[idx]
        local = node_local_matrix(node)
        world = parent_M @ local
        det_local = np.linalg.det(local[:3, :3])
        det_world = np.linalg.det(world[:3, :3])
        marker = " [HAS MESH]" if node.mesh is not None else ""
        flip = ""
        if det_local < 0:
            flip += " <-- LOCAL det<0 (mirror)"
        if det_world < 0:
            flip += " <-- WORLD det<0 (winding flipped)"
        name = node.name or f"node{idx}"
        print(f"{'  ' * depth}{name}  local_det={det_local:+.4e}  world_det={det_world:+.4e}{marker}{flip}")
        for child in (node.children or []):
            walk(child, world, depth + 1)

    scene = gltf.scenes[gltf.scene or 0]
    print(f"=== {args.input} ===")
    print(f"Scene root nodes: {scene.nodes}")
    print(f"Total nodes: {len(gltf.nodes)}, total meshes: {len(gltf.meshes)}\n")
    for root in scene.nodes:
        walk(root, np.eye(4))


if __name__ == "__main__":
    main()
