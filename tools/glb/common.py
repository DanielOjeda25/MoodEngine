"""
Common helpers for MoodEngine GLB preprocessing scripts.

Convencion del engine (ver docs/asset_conventions.md):
  - 1 unit = 1 metro
  - +Y up, +Z forward (estandar glTF 2.0)
  - Origin libre — el engine absorbe el offset Y via TransformComponent.pivotYOffset
  - Sin mirrors baked en el grafo de nodos (det(M) > 0)
  - Naming canonico de sub-meshes de vehiculos:
      chassis, wheel_FL, wheel_FR, wheel_RL, wheel_RR
"""
import struct

import numpy as np


def node_local_matrix(node):
    """Return the 4x4 local transform of a glTF node.

    glTF matrices son column-major; numpy las recibe con order='F' para
    interpretarlas como column-major y operar con la convencion habitual
    `M_world = parent @ local`.
    """
    if node.matrix is not None:
        return np.array(node.matrix, dtype=np.float64).reshape(4, 4, order='F')
    m = np.eye(4, dtype=np.float64)
    if node.scale is not None:
        m = np.diag([*node.scale, 1.0]) @ m
    if node.rotation is not None:
        x, y, z, w = node.rotation
        r = np.array([
            [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w), 0],
            [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w), 0],
            [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y), 0],
            [0, 0, 0, 1],
        ], dtype=np.float64)
        m = r @ m
    if node.translation is not None:
        t = np.eye(4, dtype=np.float64)
        t[:3, 3] = node.translation
        m = t @ m
    return m


def accessor_byte_layout(gltf, accessor):
    bv = gltf.bufferViews[accessor.bufferView]
    base = (bv.byteOffset or 0) + (accessor.byteOffset or 0)
    return base, (bv.byteStride or 12)


def read_vec3_array(gltf, blob, accessor_idx):
    accessor = gltf.accessors[accessor_idx]
    base, stride = accessor_byte_layout(gltf, accessor)
    out = np.zeros((accessor.count, 3), dtype=np.float64)
    for i in range(accessor.count):
        off = base + i * stride
        out[i] = struct.unpack_from("<fff", blob, off)
    return out


def write_vec3_array(gltf, blob, accessor_idx, data):
    accessor = gltf.accessors[accessor_idx]
    base, stride = accessor_byte_layout(gltf, accessor)
    for i in range(accessor.count):
        off = base + i * stride
        struct.pack_into("<fff", blob, off,
                         float(data[i][0]), float(data[i][1]), float(data[i][2]))
    accessor.min = [float(data[:, 0].min()), float(data[:, 1].min()), float(data[:, 2].min())]
    accessor.max = [float(data[:, 0].max()), float(data[:, 1].max()), float(data[:, 2].max())]


def reverse_winding(gltf, blob, accessor_idx):
    """Swap indices i+1 and i+2 of every triangle (invierte winding -> usado al
    aplicar una matriz con det<0 para corregir backface culling)."""
    accessor = gltf.accessors[accessor_idx]
    bv = gltf.bufferViews[accessor.bufferView]
    base = (bv.byteOffset or 0) + (accessor.byteOffset or 0)
    ct = accessor.componentType
    if ct == 5121:
        fmt = "<B"; size = 1
    elif ct == 5123:
        fmt = "<H"; size = 2
    elif ct == 5125:
        fmt = "<I"; size = 4
    else:
        raise RuntimeError(f"Unsupported index component type {ct}")
    count = accessor.count
    assert count % 3 == 0, f"Index count {count} not multiple of 3"
    for tri in range(count // 3):
        base_tri = base + tri * 3 * size
        off_b = base_tri + 1 * size
        off_c = base_tri + 2 * size
        v_b = struct.unpack_from(fmt, blob, off_b)[0]
        v_c = struct.unpack_from(fmt, blob, off_c)[0]
        struct.pack_into(fmt, blob, off_b, v_c)
        struct.pack_into(fmt, blob, off_c, v_b)


def collect_mesh_world_transforms(gltf):
    """Walk the scene graph from each root and accumulate world matrices per
    mesh. Returns dict mesh_idx -> list[(M, node_idx)]."""
    out = {}

    def walk(node_idx, parent_M):
        node = gltf.nodes[node_idx]
        M = parent_M @ node_local_matrix(node)
        if node.mesh is not None:
            out.setdefault(node.mesh, []).append((M, node_idx))
        for child in (node.children or []):
            walk(child, M)

    for root in gltf.scenes[gltf.scene or 0].nodes:
        walk(root, np.eye(4, dtype=np.float64))
    return out


def world_aabb(gltf, blob):
    """Compute world-space AABB by walking node tree."""
    overall_min = np.full(3, np.inf)
    overall_max = np.full(3, -np.inf)

    def walk(node_idx, parent_M):
        nonlocal overall_min, overall_max
        node = gltf.nodes[node_idx]
        M = parent_M @ node_local_matrix(node)
        if node.mesh is not None:
            mesh = gltf.meshes[node.mesh]
            for prim in mesh.primitives:
                if prim.attributes.POSITION is None:
                    continue
                positions = read_vec3_array(gltf, blob, prim.attributes.POSITION)
                homog = np.hstack([positions, np.ones((positions.shape[0], 1))])
                world = (M @ homog.T).T[:, :3]
                overall_min = np.minimum(overall_min, world.min(axis=0))
                overall_max = np.maximum(overall_max, world.max(axis=0))
        for child in (node.children or []):
            walk(child, M)

    for root in gltf.scenes[gltf.scene or 0].nodes:
        walk(root, np.eye(4, dtype=np.float64))
    return overall_min, overall_max
