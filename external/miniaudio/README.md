# miniaudio (vendored)

Single-header audio library de David Reid (dominio publico / MIT-0).

- **Version pinneada**: 0.11.21 (2023-11-15)
- **Upstream**: https://github.com/mackron/miniaudio
- **Docs**: https://miniaud.io/docs

## Como actualizar

```bash
curl -sSL -o external/miniaudio/miniaudio.h \
    https://raw.githubusercontent.com/mackron/miniaudio/<TAG>/miniaudio.h
```

Actualizar luego este README con el nuevo tag y probar que `tests/test_audio.cpp` siga pasando.

## Integracion CMake

Target INTERFACE `miniaudio` expone el include path. La implementacion vive en una unica TU `src/engine/audio/miniaudio_impl.cpp` con `#define MINIAUDIO_IMPLEMENTATION`.
