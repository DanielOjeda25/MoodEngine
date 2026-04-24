# Plan — Hito 9: Audio básico

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, sección 4.17 de `MOODENGINE_CONTEXTO_TECNICO.md` (miniaudio).
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Objetivo

Dotar al motor de **reproducción de audio** sin ceremonia: el editor carga un `.wav`/`.ogg`/`.mp3` del AssetBrowser y una entidad con `AudioSourceComponent` lo reproduce. Meta del hito:
- `miniaudio` como backend (single-header, igual que stb).
- `AudioClip` como segundo tipo de asset del motor (el primero fue `Texture`).
- `AudioSourceComponent { clipId, volume, loop, play_on_start, is3D }` en `Components.h`.
- `AudioSystem` que inicializa el device una vez y por frame:
  - Arranca clips marcados para auto-play.
  - Sincroniza posición 3D leyendo `TransformComponent` de entidades con `is3D = true`.
- Reproducción básica demostrable: botón "Play clip" en el Inspector reproduce. Una entidad con `play_on_start = true` + `is3D = true` se oye más fuerte a medida que el player se acerca en Play Mode.

No-goals del hito: listener configurable (por ahora = cámara Play), reverb/efectos, streaming de archivos grandes, Lua bindings del audio.

---

## Bloque 0 — Pendientes arrastrados

- [ ] Revisar pendientes del Hito 8 que puedan bloquear: ninguno esperado (Scripting está autocontenido, no hay deuda hacia audio).

## Bloque 1 — miniaudio + AudioDevice

- [ ] Bajar `miniaudio.h` (versión estable más reciente) a `external/miniaudio/` y commitear. Target CMake INTERFACE (solo include path, como stb). Implementación en una única TU `src/engine/audio/miniaudio_impl.cpp` con `#define MINIAUDIO_IMPLEMENTATION`.
- [ ] `src/engine/audio/AudioDevice.{h,cpp}`: wrapper RAII sobre `ma_engine` (API high-level de miniaudio: device + mixer + decoder + positional).
  - Constructor inicializa device default (mono/stereo autodetect, sample rate default).
  - Destructor cierra limpio.
  - `play(clip, volume, loop, is3D, worldPos) -> SoundHandle`: oneshot o loop, posicional si `is3D`.
  - `stop(SoundHandle)`, `setListener(worldPos, forward, up)`.
  - Errores de inicialización loguean al canal nuevo `audio` y desactivan el device (null object) — el resto del motor no debe crashear si falta audio.
- [ ] Canal de log `audio` nuevo en `core/Log.{h,cpp}` (misma receta que `script`).
- [ ] Smoke test en `tests/test_audio.cpp`: cargar device null + reproducir clip desde memoria sin crashear. No toca hardware real (el runner de CI podría no tener sonido).

## Bloque 2 — AudioClip como tipo de asset

- [ ] `src/engine/audio/AudioClip.{h,cpp}`: wrappe a `ma_sound` pre-decodificado (decodificar completo en RAM; para el hito, sin streaming). Path lógico + ma_sound owned + duración + canales.
- [ ] Extender `AssetManager` con una tabla paralela a la de texturas: `std::vector<std::unique_ptr<AudioClip>> m_audioClips` + `std::vector<std::string> m_audioPaths` + `loadAudio(path) -> AudioAssetId`. Slot 0 reservado para `audio/missing.wav` (beep silencioso o tono corto generado con Python/Pillow análogo a `missing.png`).
- [ ] Factory inyectable análoga a `TextureFactory`, para testear sin hardware.
- [ ] Convención: los clips se leen de `assets/audio/` (VFS sandbox: no salir del directorio raíz del proyecto/motor).

## Bloque 3 — AudioSourceComponent + AudioSystem

- [ ] `Components.h`: `AudioSourceComponent { AudioAssetId clip; float volume = 1.0f; bool loop = false; bool playOnStart = true; bool is3D = false; SoundHandle handle{}; bool started = false; }`.
- [ ] `src/systems/AudioSystem.{h,cpp}`:
  - Constructor recibe `AudioDevice&`.
  - `update(Scene&, dt, listenerPos, listenerForward, listenerUp)`: por cada entidad con `AudioSourceComponent`:
    - Si `playOnStart && !started`: disparar `device.play(clip, volume, loop, is3D, position)` y guardar `handle`. Marcar `started`.
    - Si `is3D`: actualizar posición del sound handle con la del `TransformComponent`.
  - `setListener(...)` llamado cada frame con la cámara activa (Play mode = `FpsCamera`, Editor mode = `EditorCamera` target).
  - `clear()` para cuando `rebuildSceneFromMap` limpia el registry (igual patrón que ScriptSystem).
- [ ] Integración en `EditorApplication`:
  - Member `std::unique_ptr<AudioDevice> m_audioDevice` + `std::unique_ptr<AudioSystem> m_audioSystem`.
  - Actualizar `m_audioSystem->update(...)` cada frame (antes o después de `m_scriptSystem->update`, elegir la convención).
  - `m_audioSystem->clear()` llamado desde `rebuildSceneFromMap`.

## Bloque 4 — Inspector + AssetBrowser para audio

- [ ] `InspectorPanel`: sección AudioSource con:
  - Dropdown del clip (lista audio del AssetManager).
  - Sliders `volume` (0..1), toggles `loop`/`playOnStart`/`is3D`.
  - Botón `Play` que dispara una reproducción one-shot (útil para preview sin entrar a Play Mode).
- [ ] `AssetBrowserPanel`: pestaña/sección nueva "Audio" que lista los clips cargados. Por ahora solo texto (path + duración); drag & drop a entidades vendrá en hito posterior.

## Bloque 5 — Demo funcional

- [ ] `assets/audio/beep.wav` generado con Python (script en `tools/gen_beep_audio.py`, 440 Hz × 0.5 s, 16-bit mono).
- [ ] Modificar el mapa de prueba (o un botón demo en `Ayuda`) para crear una entidad con `AudioSourceComponent{ beep.wav, loop=true, is3D=true, playOnStart=true }` en una esquina. En Play Mode, acercarse con WASD debería volumen up.
- [ ] Log: `audio: Device inicializado (sample_rate=..., channels=...)` al arrancar; `audio: Reproduciendo 'audio/beep.wav' (3D)` al spawnear entidad.

## Bloque 6 — Tests

- [ ] `tests/test_audio.cpp`: smoke con device null (miniaudio soporta backend "null" — decode-only) y factory mock de `AudioClip`. Cubrir:
  - Carga de clip por `AssetManager::loadAudio` (fallback al slot 0 si falla).
  - `AudioSystem::update` arranca clips con `playOnStart=true` y pone `started=true`.
  - `clear()` invalida handles.

## Bloque 7 — Cierre

- [ ] Recompilar, tests verdes, demo audible (dev verifica en su máquina con sonido).
- [ ] Actualizar `docs/HITOS.md`, `docs/DECISIONS.md`, `docs/ESTADO_ACTUAL.md`.
- [ ] Commits atómicos en español.
- [ ] Tag `v0.9.0-hito9` + push.
- [ ] Crear `docs/PLAN_HITO10.md` (importación de modelos 3D con assimp).

---

## Qué NO hacer en el Hito 9

- **No** exponer audio a Lua todavía. `audio.play(...)` desde scripts entra cuando haya un script demo que lo necesite (Hito 10+).
- **No** streaming de archivos grandes (clips tipo música). Todo en RAM por ahora; miniaudio soporta streaming pero complica el lifecycle.
- **No** efectos (reverb, EQ, filters). Solo mix + positional básico.
- **No** múltiples listeners. Un solo listener = cámara Play.
- **No** persistir `AudioSourceComponent` en `.moodmap`. Mismo motivo que `ScriptComponent` en Hito 8: Scene authoritative viene en Hito 10+.
- **No** formato propio empaquetado de audio. Sólo `.wav`/`.ogg`/`.mp3` que miniaudio decodifica directo.

---

## Decisiones durante implementación

_(llenar a medida que aparezcan)_

---

## Pendientes que quedan para Hito 10 o posterior

_(llenar al cerrar el hito)_
