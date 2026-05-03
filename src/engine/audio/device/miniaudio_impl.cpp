// Unica translation unit que materializa los simbolos de miniaudio.
// Cualquier otro .cpp que necesite la API incluye <miniaudio.h> sin definir
// MINIAUDIO_IMPLEMENTATION (igual patron que stb_image).

// Tunings que ayudan al footprint del build:
//   MA_NO_DEVICE_IO  - NO, lo necesitamos para playback real.
//   MA_NO_DECODING   - NO, queremos decodificar wav/ogg/mp3.
//   MA_NO_ENCODING   - SI, no grabamos.
//   MA_NO_GENERATION - SI, no generamos tonos procedurales en runtime.
//
// Sin tocar MA_NO_FLAC/MP3/WAV por default miniaudio ya los incluye, que es
// lo que queremos.
#define MA_NO_ENCODING
#define MA_NO_GENERATION

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
