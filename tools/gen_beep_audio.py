"""Genera el clip demo del motor (Hito 9 Bloque 5).

Salida: assets/audio/beep.wav (tono senoidal 440 Hz, 0.5 s, 16-bit mono,
44.1 kHz). Se reproduce en loop desde la entidad demo "AudioSource demo"
creada por el botón del menú Ayuda. En Play Mode, caminar cerca/lejos
debería subir/bajar el volumen por la atenuación 3D de miniaudio.

Usa stdlib (modulo wave + math). Amplitud al 30% del fondo para evitar que
moleste al debuggear; un asset serio vendría de un artista.
"""

from pathlib import Path
import math
import struct
import wave


SAMPLE_RATE = 44100
DURATION_SEC = 0.5
FREQ_HZ = 440.0
AMPLITUDE = 0.3           # fraccion del rango 16-bit
CHANNELS = 1
SAMPLE_WIDTH_BYTES = 2


def main() -> None:
    num_samples = int(SAMPLE_RATE * DURATION_SEC)
    max_int = 32767
    amp_int = int(AMPLITUDE * max_int)
    two_pi_f_over_sr = 2.0 * math.pi * FREQ_HZ / SAMPLE_RATE

    # Fade-in/out de 5 ms para evitar clicks al bucle. Muestras 0..fade y
    # (N-fade)..N escalan linealmente con la envelope.
    fade_samples = int(0.005 * SAMPLE_RATE)

    frames = bytearray()
    for i in range(num_samples):
        s = math.sin(i * two_pi_f_over_sr)
        env = 1.0
        if i < fade_samples:
            env = i / float(fade_samples)
        elif i > num_samples - fade_samples:
            env = (num_samples - i) / float(fade_samples)
        sample = int(s * env * amp_int)
        frames += struct.pack("<h", sample)

    out = Path(__file__).resolve().parent.parent / "assets" / "audio" / "beep.wav"
    out.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(out), "wb") as w:
        w.setnchannels(CHANNELS)
        w.setsampwidth(SAMPLE_WIDTH_BYTES)
        w.setframerate(SAMPLE_RATE)
        w.writeframes(bytes(frames))
    print(f"Escrito {out}")


if __name__ == "__main__":
    main()
