"""Bake IBL (irradiance + prefilter) desde un cubemap PNG (Hito 17 Bloque 3).

Para cada cubemap del repo (default: assets/skyboxes/sky_day) genera:

1. Irradiance map (32x32 por cara, 6 PNGs) — convolucion difusa de la
   esfera completa, integrada con cosine weighting. Sample en el shader
   con la normal de la superficie da el termino diffuse IBL.

2. Prefilter map (mip chain 128 -> 8 = 5 niveles, 6 PNGs por nivel) —
   convolucion especular usando GGX importance sampling, parametrizada
   por roughness. Mip 0 es roughness=0 (mirror), mip N-1 es roughness=1
   (totalmente difuso). Sample en el shader con `R = reflect(-V, N)`
   y `lod = roughness * (N-1)` da el termino specular IBL (mitad del
   split-sum; la otra mitad es la BRDF LUT).

Salida en `assets/ibl/<skybox_name>/`:
    irradiance/{px,nx,py,ny,pz,nz}.png   (32x32 RGBA8)
    prefilter/mip_<N>/{px,nx,py,ny,pz,nz}.png  (decreciente de 128 a 8)

Uso: `python tools/bake_ibl.py [skybox_dir]`. Default: sky_day.
Requiere numpy + Pillow.

Notas:
- LDR (RGBA8) por simplicidad. El cubemap fuente es PNG 8-bit, no HDR;
  upgrade a `.hdr`/`.exr` queda como pendiente futuro (cuando aparezcan
  cubemaps con highlights >1.0 que ameriten el rango dinamico).
- Sample count modesto (256 para irradiance, 256 para prefilter) para
  tiempos razonables (~30s total). Subir a 1024 cuando aparezcan
  artefactos de banding.
"""

import sys
import time
from pathlib import Path

import numpy as np
from PIL import Image


# Tamanos de salida.
IRRADIANCE_SIZE = 32
PREFILTER_BASE_SIZE = 128
PREFILTER_NUM_MIPS = 5  # roughness 0, 0.25, 0.5, 0.75, 1.0

# Sample counts (Monte Carlo).
IRRADIANCE_SAMPLES_PHI = 64    # 64 * 32 = 2048 samples por texel
IRRADIANCE_SAMPLES_THETA = 32
PREFILTER_SAMPLES = 256


# Orden de las caras (matchea OpenGL GL_TEXTURE_CUBE_MAP_POSITIVE_X + offset).
FACE_NAMES = ["px", "nx", "py", "ny", "pz", "nz"]


def load_cubemap(skybox_dir: Path) -> np.ndarray:
    """Devuelve array (6, H, W, 3) en [0, 1] linear."""
    faces = []
    for name in FACE_NAMES:
        img = Image.open(skybox_dir / f"{name}.png").convert("RGB")
        arr = np.asarray(img, dtype=np.float32) / 255.0
        faces.append(arr)
    sizes = {f.shape[:2] for f in faces}
    if len(sizes) != 1:
        raise ValueError(f"Cubemap caras con sizes distintos: {sizes}")
    return np.stack(faces, axis=0)


def sample_cubemap(cubemap: np.ndarray, dirs: np.ndarray) -> np.ndarray:
    """Sample bilineal del cubemap dado un array de direcciones (..., 3).

    Devuelve un array con shape `dirs.shape[:-1] + (3,)`.
    """
    flat_dirs = dirs.reshape(-1, 3)
    abs_d = np.abs(flat_dirs)
    max_axis = np.argmax(abs_d, axis=1)  # 0=X, 1=Y, 2=Z

    out = np.zeros((flat_dirs.shape[0], 3), dtype=np.float32)
    size = cubemap.shape[1]

    for axis in range(3):
        for sign in (1.0, -1.0):
            mask = (max_axis == axis) & (
                np.sign(flat_dirs[:, axis]) == sign
            )
            if not np.any(mask):
                continue
            face_idx = axis * 2 + (0 if sign > 0 else 1)
            d = flat_dirs[mask]
            ax = d[:, axis]
            # uv depende del axis y sign segun la convencion OpenGL del
            # cubemap. Tablas estandar (ej. glTF spec):
            #   +X (axis=0,+):  uc = -z,  vc = -y,  ma = x
            #   -X (axis=0,-):  uc =  z,  vc = -y,  ma = -x  (=-x positivo)
            #   +Y (axis=1,+):  uc =  x,  vc =  z,  ma = y
            #   -Y (axis=1,-):  uc =  x,  vc = -z,  ma = -y
            #   +Z (axis=2,+):  uc =  x,  vc = -y,  ma = z
            #   -Z (axis=2,-):  uc = -x,  vc = -y,  ma = -z
            if axis == 0:
                uc = -np.sign(ax) * d[:, 2]
                vc = -d[:, 1]
                ma = np.abs(ax)
            elif axis == 1:
                uc = d[:, 0]
                vc = np.sign(ax) * d[:, 2]
                ma = np.abs(ax)
            else:  # axis == 2
                uc = np.sign(ax) * d[:, 0]
                vc = -d[:, 1]
                ma = np.abs(ax)
            u = 0.5 * (uc / ma + 1.0)
            v = 0.5 * (vc / ma + 1.0)
            # Bilinear sample.
            fx = u * (size - 1)
            fy = v * (size - 1)
            x0 = np.clip(np.floor(fx).astype(np.int32), 0, size - 1)
            x1 = np.clip(x0 + 1, 0, size - 1)
            y0 = np.clip(np.floor(fy).astype(np.int32), 0, size - 1)
            y1 = np.clip(y0 + 1, 0, size - 1)
            tx = (fx - x0)[:, None]
            ty = (fy - y0)[:, None]
            face = cubemap[face_idx]
            c00 = face[y0, x0]
            c10 = face[y0, x1]
            c01 = face[y1, x0]
            c11 = face[y1, x1]
            top = c00 * (1.0 - tx) + c10 * tx
            bot = c01 * (1.0 - tx) + c11 * tx
            out[mask] = top * (1.0 - ty) + bot * ty
    return out.reshape(dirs.shape[:-1] + (3,))


def face_uv_to_dir(face: int, u: np.ndarray, v: np.ndarray) -> np.ndarray:
    """uv en [0, 1]^2 -> direccion 3D normalizada para la cara `face`."""
    s = u * 2.0 - 1.0
    t = v * 2.0 - 1.0
    if face == 0:    # +X
        d = np.stack([np.ones_like(s), -t, -s], axis=-1)
    elif face == 1:  # -X
        d = np.stack([-np.ones_like(s), -t, s], axis=-1)
    elif face == 2:  # +Y
        d = np.stack([s, np.ones_like(s), t], axis=-1)
    elif face == 3:  # -Y
        d = np.stack([s, -np.ones_like(s), -t], axis=-1)
    elif face == 4:  # +Z
        d = np.stack([s, -t, np.ones_like(s)], axis=-1)
    else:            # -Z
        d = np.stack([-s, -t, -np.ones_like(s)], axis=-1)
    norm = np.linalg.norm(d, axis=-1, keepdims=True)
    return d / np.maximum(norm, 1e-7)


def hammersley(num: int) -> np.ndarray:
    i = np.arange(num, dtype=np.uint32)
    bits = i.copy()
    bits = (bits << 16) | (bits >> 16)
    bits = ((bits & 0x55555555) << 1) | ((bits & 0xAAAAAAAA) >> 1)
    bits = ((bits & 0x33333333) << 2) | ((bits & 0xCCCCCCCC) >> 2)
    bits = ((bits & 0x0F0F0F0F) << 4) | ((bits & 0xF0F0F0F0) >> 4)
    bits = ((bits & 0x00FF00FF) << 8) | ((bits & 0xFF00FF00) >> 8)
    radical = bits.astype(np.float64) / 0x100000000
    return np.stack([i.astype(np.float64) / num, radical], axis=1)


def importance_sample_ggx(xi: np.ndarray, roughness: float) -> np.ndarray:
    a = roughness * roughness
    phi = 2.0 * np.pi * xi[:, 0]
    cos_t_sq = (1.0 - xi[:, 1]) / (1.0 + (a * a - 1.0) * xi[:, 1])
    cos_t = np.sqrt(np.maximum(cos_t_sq, 0.0))
    sin_t = np.sqrt(np.maximum(1.0 - cos_t_sq, 0.0))
    return np.stack([np.cos(phi) * sin_t, np.sin(phi) * sin_t, cos_t], axis=1)


def basis_from_normal(n: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Devuelve (tangent, bitangent) ortonormales a la normal n (Frisvad)."""
    up = np.where(np.abs(n[2]) < 0.999, np.array([0.0, 0.0, 1.0]), np.array([1.0, 0.0, 0.0]))
    t = np.cross(up, n)
    t = t / max(np.linalg.norm(t), 1e-7)
    b = np.cross(n, t)
    return t, b


def bake_irradiance(cubemap: np.ndarray, out_dir: Path) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    s = IRRADIANCE_SIZE
    print(f"Irradiance {s}x{s} ({IRRADIANCE_SAMPLES_PHI}*{IRRADIANCE_SAMPLES_THETA} samples)")

    # Tabla de direcciones de muestreo en hemisferio (tangent space).
    phis = np.linspace(0.0, 2.0 * np.pi, IRRADIANCE_SAMPLES_PHI, endpoint=False)
    thetas = np.linspace(0.0, np.pi / 2.0, IRRADIANCE_SAMPLES_THETA, endpoint=False)
    pp, tt = np.meshgrid(phis, thetas)
    sample_t = np.stack([
        np.sin(tt) * np.cos(pp),
        np.sin(tt) * np.sin(pp),
        np.cos(tt),
    ], axis=-1).reshape(-1, 3)
    weights = (np.cos(tt) * np.sin(tt)).reshape(-1)
    weight_sum = float(np.sum(weights))

    for face in range(6):
        face_img = np.zeros((s, s, 4), dtype=np.uint8)
        u = (np.arange(s) + 0.5) / s
        v = (np.arange(s) + 0.5) / s
        uu, vv = np.meshgrid(u, v)
        dirs = face_uv_to_dir(face, uu, vv)  # (s, s, 3)
        flat_n = dirs.reshape(-1, 3)
        out_rgb = np.zeros((flat_n.shape[0], 3), dtype=np.float32)
        # Procesamos texel por texel para evitar arrays gigantes en memoria.
        for i, n in enumerate(flat_n):
            t, b = basis_from_normal(n)
            # tangent_to_world: world_dir = t*tangent.x + b*tangent.y + n*tangent.z
            world_dirs = (
                sample_t[:, 0:1] * t[None, :]
                + sample_t[:, 1:2] * b[None, :]
                + sample_t[:, 2:3] * n[None, :]
            )
            colors = sample_cubemap(cubemap, world_dirs)  # (N, 3)
            irr = (colors * weights[:, None]).sum(axis=0) / weight_sum
            out_rgb[i] = irr * np.pi
        out_rgb = out_rgb.reshape(s, s, 3)
        out_8 = np.clip(out_rgb * 255.0, 0.0, 255.0).astype(np.uint8)
        face_img[..., :3] = out_8
        face_img[..., 3] = 255
        Image.fromarray(face_img, mode="RGBA").save(out_dir / f"{FACE_NAMES[face]}.png")
        print(f"  cara {FACE_NAMES[face]} OK")


def bake_prefilter_mip(
    cubemap: np.ndarray, mip_size: int, roughness: float, out_dir: Path
) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    samples = hammersley(PREFILTER_SAMPLES)

    for face in range(6):
        face_img = np.zeros((mip_size, mip_size, 4), dtype=np.uint8)
        u = (np.arange(mip_size) + 0.5) / mip_size
        v = (np.arange(mip_size) + 0.5) / mip_size
        uu, vv = np.meshgrid(u, v)
        dirs = face_uv_to_dir(face, uu, vv).reshape(-1, 3)

        out_rgb = np.zeros((dirs.shape[0], 3), dtype=np.float32)
        for i, n in enumerate(dirs):
            # Aproximacion estandar Karis: V = R = N. Pierde estiramiento
            # de reflejos en grazing pero es la convencion del split-sum.
            t, b = basis_from_normal(n)
            h_tan = importance_sample_ggx(samples, roughness)  # (N, 3)
            h_world = (
                h_tan[:, 0:1] * t[None, :]
                + h_tan[:, 1:2] * b[None, :]
                + h_tan[:, 2:3] * n[None, :]
            )
            # L = reflect(-V, H) con V = N: L = 2*dot(N, H)*H - N.
            n_dot_h = h_world @ n  # (N,)
            l_world = 2.0 * n_dot_h[:, None] * h_world - n[None, :]
            n_dot_l = np.maximum(l_world @ n, 0.0)

            mask = n_dot_l > 0.0
            if not np.any(mask):
                continue
            colors = sample_cubemap(cubemap, l_world[mask])
            weights = n_dot_l[mask]
            num = (colors * weights[:, None]).sum(axis=0)
            den = float(np.sum(weights))
            out_rgb[i] = num / max(den, 1e-7)

        out_rgb = out_rgb.reshape(mip_size, mip_size, 3)
        face_img[..., :3] = np.clip(out_rgb * 255.0, 0.0, 255.0).astype(np.uint8)
        face_img[..., 3] = 255
        Image.fromarray(face_img, mode="RGBA").save(out_dir / f"{FACE_NAMES[face]}.png")
        print(f"  cara {FACE_NAMES[face]} OK")


def bake_prefilter(cubemap: np.ndarray, out_root: Path) -> None:
    print(f"Prefilter (mip chain {PREFILTER_NUM_MIPS} niveles, base {PREFILTER_BASE_SIZE}px)")
    for mip in range(PREFILTER_NUM_MIPS):
        size = max(PREFILTER_BASE_SIZE >> mip, 1)
        roughness = mip / max(PREFILTER_NUM_MIPS - 1, 1)
        roughness = max(roughness, 0.04)  # mip 0 = mirror puro
        mip_dir = out_root / f"mip_{mip}"
        print(f"  mip {mip}: {size}x{size} roughness={roughness:.3f}")
        bake_prefilter_mip(cubemap, size, roughness, mip_dir)


def main() -> None:
    skybox_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("assets/skyboxes/sky_day")
    if not skybox_dir.exists():
        print(f"ERROR: cubemap dir no existe: {skybox_dir}")
        sys.exit(1)

    name = skybox_dir.name
    out_root = Path("assets/ibl") / name
    print(f"Bakeando IBL para '{name}' -> {out_root}")

    t0 = time.time()
    cubemap = load_cubemap(skybox_dir)
    print(f"Cubemap fuente cargado: shape={cubemap.shape}")

    bake_irradiance(cubemap, out_root / "irradiance")
    bake_prefilter(cubemap, out_root / "prefilter")

    print(f"OK en {time.time() - t0:.1f}s")


if __name__ == "__main__":
    main()
