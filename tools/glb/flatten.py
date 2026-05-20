"""
Bake all node transforms into vertex data; reset node matrices to identity.

Equivalente headless a "Apply All Transforms" en Blender. Si una matriz
acumulada tiene det<0 (mirror), invierte el winding del index buffer y
flippea las normales para que el backface culling siga viendo la cara
correcta.

Uso:
    python tools/glb/flatten.py <input.glb> [--in-place]
"""
import argparse
from pathlib import Path

import numpy as np
import pygltflib

from common import (
    collect_mesh_world_transforms,
    read_vec3_array,
    reverse_winding,
    write_vec3_array,
)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("input", type=Path)
    ap.add_argument("--in-place", action="store_true")
    args = ap.parse_args()

    out = args.input if args.in_place else args.input.with_suffix(".flat.glb")

    print(f"Loading {args.input}")
    gltf = pygltflib.GLTF2().load(str(args.input))
    blob = bytearray(gltf.binary_blob())

    mesh_refs = collect_mesh_world_transforms(gltf)
    multi = {m: lst for m, lst in mesh_refs.items() if len(lst) > 1}
    if multi:
        print("WARNING: meshes referenced by multiple nodes; using first transform:")
        for m, lst in multi.items():
            print(f"  mesh[{m}] used by {len(lst)} nodes")

    processed_pos = set()
    processed_nrm = set()
    processed_idx = set()
    flipped = 0

    for mesh_idx, refs in mesh_refs.items():
        M, _ = refs[0]
        M3 = M[:3, :3]
        det = np.linalg.det(M3)
        needs_flip = det < 0

        for prim in gltf.meshes[mesh_idx].primitives:
            pos_idx = prim.attributes.POSITION
            nrm_idx = getattr(prim.attributes, "NORMAL", None)
            idx_idx = prim.indices

            if pos_idx is not None and pos_idx not in processed_pos:
                processed_pos.add(pos_idx)
                positions = read_vec3_array(gltf, blob, pos_idx)
                homog = np.hstack([positions, np.ones((positions.shape[0], 1))])
                write_vec3_array(gltf, blob, pos_idx, (M @ homog.T).T[:, :3])

            if nrm_idx is not None and nrm_idx not in processed_nrm:
                processed_nrm.add(nrm_idx)
                normals = read_vec3_array(gltf, blob, nrm_idx)
                transformed = (M3 @ normals.T).T
                if needs_flip:
                    transformed = -transformed
                lens = np.linalg.norm(transformed, axis=1, keepdims=True)
                lens = np.where(lens < 1e-12, 1.0, lens)
                write_vec3_array(gltf, blob, nrm_idx, transformed / lens)

            if needs_flip and idx_idx is not None and idx_idx not in processed_idx:
                processed_idx.add(idx_idx)
                reverse_winding(gltf, blob, idx_idx)
                flipped += 1

    print(f"Processed {len(mesh_refs)} meshes; flipped winding+normals on {flipped} mesh(es).")

    for node in gltf.nodes:
        node.matrix = None
        node.translation = None
        node.rotation = None
        node.scale = None

    gltf.set_binary_blob(bytes(blob))
    gltf.save_binary(str(out))
    print(f"Saved {out}")


if __name__ == "__main__":
    main()
