"""Genera el clip de audio fallback del motor (Hito 9 Bloque 2).

Salida: assets/audio/missing.wav (100 ms de silencio, 16-bit mono, 44.1 kHz).
Sirve como slot 0 del AssetManager para audio: si el motor intenta cargar un
`.wav` que no existe, AudioDevice devuelve este silencio en vez de crashear.

El equivalente visual es `missing.png` (patron chequered rosa/negro). Elegimos
silencio en vez de "beep" porque un beep en loop cuando hay N assets rotos
seria insoportable; el warn del canal `audio` ya avisa del fallo.

Reproducible desde la raiz del repo. Solo usa stdlib (modulo wave).
"""

from pathlib import Path
import wave


SAMPLE_RATE = 44100      # Hz
DURATION_SEC = 0.1       # 100 ms
CHANNELS = 1             # mono
SAMPLE_WIDTH_BYTES = 2   # 16-bit PCM


def main() -> None:
    num_samples = int(SAMPLE_RATE * DURATION_SEC)
    silent_frames = b"\x00\x00" * num_samples  # little-endian 0 por sample

    out = Path(__file__).resolve().parent.parent / "assets" / "audio" / "missing.wav"
    out.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(out), "wb") as w:
        w.setnchannels(CHANNELS)
        w.setsampwidth(SAMPLE_WIDTH_BYTES)
        w.setframerate(SAMPLE_RATE)
        w.writeframes(silent_frames)
    print(f"Escrito {out}")


if __name__ == "__main__":
    main()
