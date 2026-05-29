"""Helpers compartidos para los gen_*_texture.py de la library/.

F3H30: pack base de 5 texturas HL1-style procedurales (concrete_wall,
concrete_floor, dirt_ground, metal_panel, brick_old). Cada material
tiene su algoritmo propio pero comparten:
  - Resolucion 256x256
  - Seamless tileable (offset + blend)
  - Cuantizacion a paleta indexed (16-32 colores)
  - Salida a assets/textures/library/<name>.png

NO es un framework — solo helpers chicos. Cada script sigue mandando
su main() y dibuja su propio look.
"""

from __future__ import annotations

import random
from pathlib import Path

import numpy as np
from PIL import Image


TEX_SIZE = 256


def repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def library_path(name: str) -> Path:
    out = repo_root() / "assets" / "textures" / "library" / f"{name}.png"
    out.parent.mkdir(parents=True, exist_ok=True)
    return out


def seeded_rng(seed: int) -> tuple[random.Random, np.random.Generator]:
    """RNG determinista para que los PNGs sean reproducibles."""
    return random.Random(seed), np.random.default_rng(seed)


def make_tileable(img: Image.Image, blend: int = 32) -> Image.Image:
    """Hace una imagen seamless mezclando bordes opuestos.

    Para cada par (columna_izq[i], columna_der[i]) y analogamente para
    filas, mezclamos linealmente para que en el borde mismo los pixels
    coincidan exactamente (== promedio de ambos), y se vuelvan al
    original a `blend`px de distancia. Resultado: bordes seamless al
    tilear.
    """
    arr = np.array(img, dtype=np.float32)
    h, w = arr.shape[:2]

    # Bordes izquierdo/derecho.
    for i in range(blend):
        alpha = i / blend          # 0 en borde, 1 a blend px adentro
        mix = 0.5 * (1.0 - alpha)  # 0.5 en borde, 0 a blend px adentro
        left = arr[:, i].copy()
        right = arr[:, w - 1 - i].copy()
        arr[:, i] = left * (1.0 - mix) + right * mix
        arr[:, w - 1 - i] = right * (1.0 - mix) + left * mix

    # Bordes top/bottom (aplicado sobre el arr ya modificado, mantiene
    # consistencia en las esquinas).
    for i in range(blend):
        alpha = i / blend
        mix = 0.5 * (1.0 - alpha)
        top = arr[i, :].copy()
        bot = arr[h - 1 - i, :].copy()
        arr[i, :] = top * (1.0 - mix) + bot * mix
        arr[h - 1 - i, :] = bot * (1.0 - mix) + top * mix

    arr = np.clip(arr, 0, 255).astype(np.uint8)
    return Image.fromarray(arr, mode=img.mode)


def quantize_to_palette(img: Image.Image, n_colors: int = 16) -> Image.Image:
    """Convierte a paleta indexed con N colores via median-cut.

    El look HL1 viene de paleta limitada — 16 colores es suficiente
    para concrete/metal/dirt, 32 para brick (mas variacion tonal).
    """
    if img.mode != "RGB":
        img = img.convert("RGB")
    return img.quantize(colors=n_colors, method=Image.Quantize.MEDIANCUT)


def gaussian_noise(size: int, mean: float, sigma: float,
                   rng: np.random.Generator) -> np.ndarray:
    """Ruido gaussiano 2D clipped a [0, 255]."""
    arr = rng.normal(loc=mean, scale=sigma, size=(size, size))
    return np.clip(arr, 0, 255)


def smooth(arr: np.ndarray, passes: int = 1) -> np.ndarray:
    """Box-blur 3x3 manual, sin scipy. Usado para suavizar ruido."""
    out = arr.astype(np.float32)
    for _ in range(passes):
        out = (
            out
            + np.roll(out, 1, axis=0) + np.roll(out, -1, axis=0)
            + np.roll(out, 1, axis=1) + np.roll(out, -1, axis=1)
            + np.roll(np.roll(out, 1, axis=0), 1, axis=1)
            + np.roll(np.roll(out, -1, axis=0), 1, axis=1)
            + np.roll(np.roll(out, 1, axis=0), -1, axis=1)
            + np.roll(np.roll(out, -1, axis=0), -1, axis=1)
        ) / 9.0
    return out
