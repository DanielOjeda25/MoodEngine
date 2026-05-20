"""
Bake uniform scale into GLB vertex positions + node translations.

Equivalente headless a "Apply Scale" en Blender. Patron industrial standard.

Uso:
    python tools/glb/scale.py <input.glb> <factor> [--in-place]

Si no se pasa --in-place, escribe a <input>.scaled.glb (no toca el original).
"""
import argparse
import struct
from pathlib import Path

import pygltflib


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("input", type=Path, help="Path al GLB")
    ap.add_argument("factor", type=float, help="Factor de escala uniforme (ej 1.86)")
    ap.add_argument("--in-place", action="store_true",
                    help="Sobreescribir el GLB de entrada (default: escribir a .scaled.glb)")
    args = ap.parse_args()

    out = args.input if args.in_place else args.input.with_suffix(".scaled.glb")

    print(f"Loading {args.input}  scale x{args.factor}")
    gltf = pygltflib.GLTF2().load(str(args.input))
    blob = bytearray(gltf.binary_blob())

    scaled = set()
    for mesh in gltf.meshes:
        for prim in mesh.primitives:
            pos_idx = prim.attributes.POSITION
            if pos_idx is None or pos_idx in scaled:
                continue
            scaled.add(pos_idx)
            acc = gltf.accessors[pos_idx]
            bv = gltf.bufferViews[acc.bufferView]
            base = (bv.byteOffset or 0) + (acc.byteOffset or 0)
            stride = bv.byteStride or 12
            new_min = [float("inf")] * 3
            new_max = [float("-inf")] * 3
            for i in range(acc.count):
                off = base + i * stride
                x, y, z = struct.unpack_from("<fff", blob, off)
                x *= args.factor
                y *= args.factor
                z *= args.factor
                struct.pack_into("<fff", blob, off, x, y, z)
                new_min = [min(new_min[0], x), min(new_min[1], y), min(new_min[2], z)]
                new_max = [max(new_max[0], x), max(new_max[1], y), max(new_max[2], z)]
            acc.min = new_min
            acc.max = new_max

    for node in gltf.nodes:
        if node.translation is not None:
            node.translation = [v * args.factor for v in node.translation]
        if node.matrix is not None:
            m = list(node.matrix)
            m[12] *= args.factor
            m[13] *= args.factor
            m[14] *= args.factor
            node.matrix = m

    gltf.set_binary_blob(bytes(blob))
    gltf.save_binary(str(out))
    print(f"Saved {out}")


if __name__ == "__main__":
    main()
