"""Genera la BRDF LUT 2D para el split-sum de Epic (Hito 17 Bloque 3).

Tabla universal (no depende del skybox): para cada (NdotV, roughness) en
[0, 1]^2, integra el termino del split-sum y guarda (scale, bias) en
canales R y G respectivamente. El shader PBR reconstruye la BRDF
especular IBL como:

    F0 * scale + bias    (donde scale, bias = textura(NdotV, roughness))

El canal B queda en 0 (ignorado por el shader). Output PNG RGBA8.

Salida: assets/ibl/brdf_lut.png (256x256, RG = (scale, bias)).

Math: Karis 2013 "Real Shading in Unreal Engine 4", Sec. 5.
Importance sampling GGX + 1024 samples per texel via Hammersley.

Reproducible: `python tools/bake_brdf_lut.py` desde la raiz del repo.
Requiere numpy + Pillow.
"""

from pathlib import Path

import numpy as np
from PIL import Image


SIZE = 256
SAMPLES = 1024
OUT_PATH = Path("assets/ibl/brdf_lut.png")


def hammersley(num_samples: int) -> np.ndarray:
    """Devuelve `num_samples` puntos de la secuencia Hammersley en [0, 1)^2."""
    i = np.arange(num_samples, dtype=np.uint32)
    # Van der Corput inverso (radical inverse base 2).
    bits = i.copy()
    bits = (bits << 16) | (bits >> 16)
    bits = ((bits & 0x55555555) << 1) | ((bits & 0xAAAAAAAA) >> 1)
    bits = ((bits & 0x33333333) << 2) | ((bits & 0xCCCCCCCC) >> 2)
    bits = ((bits & 0x0F0F0F0F) << 4) | ((bits & 0xF0F0F0F0) >> 4)
    bits = ((bits & 0x00FF00FF) << 8) | ((bits & 0xFF00FF00) >> 8)
    radical_inverse = bits.astype(np.float64) / 0x100000000
    u = i.astype(np.float64) / num_samples
    return np.stack([u, radical_inverse], axis=1)  # (N, 2)


def importance_sample_ggx(xi: np.ndarray, roughness: float) -> np.ndarray:
    """Sample direction H en tangent space para una distribucion GGX.

    xi: (N, 2) en [0, 1)^2. Devuelve (N, 3) con N = (0, 0, 1).
    """
    a = roughness * roughness
    phi = 2.0 * np.pi * xi[:, 0]
    cos_theta_sq = (1.0 - xi[:, 1]) / (1.0 + (a * a - 1.0) * xi[:, 1])
    cos_theta = np.sqrt(np.maximum(cos_theta_sq, 0.0))
    sin_theta = np.sqrt(np.maximum(1.0 - cos_theta_sq, 0.0))
    h = np.stack([
        np.cos(phi) * sin_theta,
        np.sin(phi) * sin_theta,
        cos_theta,
    ], axis=1)
    return h


def geometry_schlick_ggx_ibl(n_dot_x: np.ndarray, roughness: float) -> np.ndarray:
    """k = a^2 / 2 para IBL (en vez de (r+1)^2/8 del direct lighting)."""
    a = roughness * roughness
    k = a * a / 2.0
    return n_dot_x / (n_dot_x * (1.0 - k) + k + 1e-7)


def integrate_brdf(n_dot_v: float, roughness: float, samples: np.ndarray) -> tuple[float, float]:
    """Integra (scale, bias) para un texel del LUT.

    samples: (N, 2) puntos Hammersley, ya generados (compartido entre texels).
    """
    # V en tangent space (N = (0, 0, 1)).
    v = np.array([np.sqrt(max(1.0 - n_dot_v * n_dot_v, 0.0)), 0.0, n_dot_v])
    h_vecs = importance_sample_ggx(samples, roughness)  # (N, 3)

    # L = reflect(-V, H) = 2 * dot(V, H) * H - V
    v_dot_h = np.maximum(h_vecs @ v, 0.0)  # (N,)
    l_vecs = 2.0 * v_dot_h[:, None] * h_vecs - v[None, :]  # (N, 3)

    n_dot_l = np.maximum(l_vecs[:, 2], 0.0)
    n_dot_h = np.maximum(h_vecs[:, 2], 0.0)

    mask = n_dot_l > 0.0
    if not np.any(mask):
        return 0.0, 0.0

    g = (
        geometry_schlick_ggx_ibl(np.maximum(n_dot_v, 1e-7), roughness)
        * geometry_schlick_ggx_ibl(np.maximum(n_dot_l, 1e-7), roughness)
    )
    g_vis = (g * v_dot_h) / (n_dot_h * max(n_dot_v, 1e-7) + 1e-7)
    fc = (1.0 - v_dot_h) ** 5

    scale_terms = (1.0 - fc) * g_vis
    bias_terms = fc * g_vis

    n = float(samples.shape[0])
    scale = float(np.sum(np.where(mask, scale_terms, 0.0)) / n)
    bias = float(np.sum(np.where(mask, bias_terms, 0.0)) / n)
    return scale, bias


def main() -> None:
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)

    samples = hammersley(SAMPLES)
    image = np.zeros((SIZE, SIZE, 4), dtype=np.uint8)

    print(f"Bakeando BRDF LUT {SIZE}x{SIZE} con {SAMPLES} samples por texel...")
    for y in range(SIZE):
        # roughness en [0, 1] (eje Y del LUT). y=0 es row arriba en PNG;
        # convencion del shader: V coord arriba = roughness 1. Para que el
        # shader haga `texture(lut, vec2(NdotV, roughness))` y matchee con
        # la convencion estandar, escribimos roughness creciente con y.
        roughness = (y + 0.5) / SIZE
        roughness = max(roughness, 0.04)  # evitar a^4 = 0 (G blow up)
        for x in range(SIZE):
            n_dot_v = (x + 0.5) / SIZE
            scale, bias = integrate_brdf(n_dot_v, roughness, samples)
            image[y, x, 0] = int(round(np.clip(scale, 0.0, 1.0) * 255.0))
            image[y, x, 1] = int(round(np.clip(bias, 0.0, 1.0) * 255.0))
            image[y, x, 2] = 0  # canal libre
            image[y, x, 3] = 255
        if (y + 1) % 32 == 0:
            print(f"  ... {y + 1} / {SIZE}")

    # PNG con origen top-left por defecto. El shader debera leer con la V
    # invertida o usar `stbi_set_flip_vertically_on_load(true)` (hace el
    # OpenGLTexture estandar).
    Image.fromarray(image, mode="RGBA").save(OUT_PATH)
    print(f"OK -> {OUT_PATH}")


if __name__ == "__main__":
    main()
