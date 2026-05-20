"""
Center GLB origin on the model's vertical center (Y axis).

Aplica un offset Y a todas las posiciones para que el centro vertical del
AABB quede en Y=0. Usado para que los modelos sigan la convencion "origin
en el centro" (compatible con `pivotYOffset` del engine).

NOTE: el engine MoodEngine absorbe cualquier convencion de origin via
`TransformComponent.pivotYOffset` calculado desde el AABB del MeshAsset.
Este script solo es util si queres uniformidad entre tus assets.

Uso:
    python tools/glb/center_y.py <input.glb> [--in-place]
"""
import argparse
import struct
from pathlib import Path

import pygltflib


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("input", type=Path)
    ap.add_argument("--in-place", action="store_true")
    args = ap.parse_args()

    out = args.input if args.in_place else args.input.with_suffix(".centered.glb")

    gltf = pygltflib.GLTF2().load(str(args.input))
    blob = bytearray(gltf.binary_blob())

    y_min, y_max = float("inf"), float("-inf")
    pos_accs = set()
    for mesh in gltf.meshes:
        for prim in mesh.primitives:
            pos = prim.attributes.POSITION
            if pos is None:
                continue
            pos_accs.add(pos)
            acc = gltf.accessors[pos]
            if acc.min and acc.max:
                y_min = min(y_min, acc.min[1])
                y_max = max(y_max, acc.max[1])

    y_center = (y_min + y_max) / 2.0
    print(f"AABB Y: min={y_min:.4f}  max={y_max:.4f}  center={y_center:.4f}")
    print(f"Applying Y offset: {-y_center:+.4f}")

    for pos_idx in pos_accs:
        acc = gltf.accessors[pos_idx]
        bv = gltf.bufferViews[acc.bufferView]
        base = (bv.byteOffset or 0) + (acc.byteOffset or 0)
        stride = bv.byteStride or 12
        new_min_y = float("inf")
        new_max_y = float("-inf")
        for i in range(acc.count):
            off = base + i * stride
            x, y, z = struct.unpack_from("<fff", blob, off)
            y -= y_center
            struct.pack_into("<fff", blob, off, x, y, z)
            new_min_y = min(new_min_y, y)
            new_max_y = max(new_max_y, y)
        if acc.min and acc.max:
            acc.min[1] = new_min_y
            acc.max[1] = new_max_y

    gltf.set_binary_blob(bytes(blob))
    gltf.save_binary(str(out))
    print(f"Saved {out}")


if __name__ == "__main__":
    main()
