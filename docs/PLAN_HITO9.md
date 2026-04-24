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

- [x] Revisar pendientes del Hito 8 que puedan bloquear: ninguno esperado (Scripting está autocontenido, no hay deuda hacia audio).

## Bloque 1 — miniaudio + AudioDevice

- [x] Bajar `miniaudio.h` (versión estable más reciente) a `external/miniaudio/` y commitear. Target CMake INTERFACE (solo include path, como stb). Implementación en una única TU `src/engine/audio/miniaudio_impl.cpp` con `#define MINIAUDIO_IMPLEMENTATION`.
- [x] `src/engine/audio/AudioDevice.{h,cpp}`: wrapper RAII sobre `ma_engine` (API high-level de miniaudio: device + mixer + decoder + positional).
  - Constructor inicializa device default (mono/stereo autodetect, sample rate default).
  - Destructor cierra limpio.
  - `play(clip, volume, loop, is3D, worldPos) -> SoundHandle`: oneshot o loop, posicional si `is3D`.
  - `stop(SoundHandle)`, `setListener(worldPos, forward, up)`.
  - Errores de inicialización loguean al canal nuevo `audio` y desactivan el device (null object) — el resto del motor no debe crashear si falta audio.
- [x] Canal de log `audio` nuevo en `core/Log.{h,cpp}` (misma receta que `script`).
- [x] Smoke test en `tests/test_audio.cpp`: cargar device null + reproducir clip desde memoria sin crashear. No toca hardware real (el runner de CI podría no tener sonido).

## Bloque 2 — AudioClip como tipo de asset

- [x] `src/engine/audio/AudioClip.{h,cpp}`: wrappe a `ma_sound` pre-decodificado (decodificar completo en RAM; para el hito, sin streaming). Path lógico + ma_sound owned + duración + canales.
- [x] Extender `AssetManager` con una tabla paralela a la de texturas: `std::vector<std::unique_ptr<AudioClip>> m_audioClips` + `std::vector<std::string> m_audioPaths` + `loadAudio(path) -> AudioAssetId`. Slot 0 reservado para `audio/missing.wav` (beep silencioso o tono corto generado con Python/Pillow análogo a `missing.png`).
- [x] Factory inyectable análoga a `TextureFactory`, para testear sin hardware.
- [x] Convención: los clips se leen de `assets/audio/` (VFS sandbox: no salir del directorio raíz del proyecto/motor).

## Bloque 3 — AudioSourceComponent + AudioSystem

- [x] `Components.h`: `AudioSourceComponent { AudioAssetId clip; float volume = 1.0f; bool loop = false; bool playOnStart = true; bool is3D = false; SoundHandle handle{}; bool started = false; }`.
- [x] `src/systems/AudioSystem.{h,cpp}`:
  - Constructor recibe `AudioDevice&`.
  - `update(Scene&, dt, listenerPos, listenerForward, listenerUp)`: por cada entidad con `AudioSourceComponent`:
    - Si `playOnStart && !started`: disparar `device.play(clip, volume, loop, is3D, position)` y guardar `handle`. Marcar `started`.
    - Si `is3D`: actualizar posición del sound handle con la del `TransformComponent`.
  - `setListener(...)` llamado cada frame con la cámara activa (Play mode = `FpsCamera`, Editor mode = `EditorCamera` target).
  - `clear()` para cuando `rebuildSceneFromMap` limpia el registry (igual patrón que ScriptSystem).
- [x] Integración en `EditorApplication`:
  - Member `std::unique_ptr<AudioDevice> m_audioDevice` + `std::unique_ptr<AudioSystem> m_audioSystem`.
  - Actualizar `m_audioSystem->update(...)` cada frame (antes o después de `m_scriptSystem->update`, elegir la convención).
  - `m_audioSystem->clear()` llamado desde `rebuildSceneFromMap`.

## Bloque 4 — Inspector + AssetBrowser para audio

- [x] `InspectorPanel`: sección AudioSource con:
  - Dropdown del clip (lista audio del AssetManager).
  - Sliders `volume` (0..1), toggles `loop`/`playOnStart`/`is3D`.
  - Botón `Play` que dispara una reproducción one-shot (útil para preview sin entrar a Play Mode).
- [x] `AssetBrowserPanel`: pestaña/sección nueva "Audio" que lista los clips cargados. Por ahora solo texto (path + duración); drag & drop a entidades vendrá en hito posterior.

## Bloque 5 — Demo funcional

- [x] `assets/audio/beep.wav` generado con Python (script en `tools/gen_beep_audio.py`, 440 Hz × 0.5 s, 16-bit mono).
- [x] Modificar el mapa de prueba (o un botón demo en `Ayuda`) para crear una entidad con `AudioSourceComponent{ beep.wav, loop=true, is3D=true, playOnStart=true }` en una esquina. En Play Mode, acercarse con WASD debería volumen up.
- [x] Log: `audio: Device inicializado (sample_rate=..., channels=...)` al arrancar; `audio: Reproduciendo 'audio/beep.wav' (3D)` al spawnear entidad.

## Bloque 6 — Tests

- [x] `tests/test_audio.cpp`: smoke con device null (miniaudio soporta backend "null" — decode-only) y factory mock de `AudioClip`. Cubrir:
  - Carga de clip por `AssetManager::loadAudio` (fallback al slot 0 si falla).
  - `AudioSystem::update` arranca clips con `playOnStart=true` y pone `started=true`.
  - `clear()` invalida handles.

## Bloque 7 — Cierre

- [x] Recompilar, tests verdes, demo audible (dev verifica en su máquina con sonido).
- [x] Actualizar `docs/HITOS.md`, `docs/DECISIONS.md`, `docs/ESTADO_ACTUAL.md`.
- [x] Commits atómicos en español.
- [x] Tag `v0.9.0-hito9` + push.
- [x] Crear `docs/PLAN_HITO10.md` (importación de modelos 3D con assimp).

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

Detalle en `DECISIONS.md` (fecha 2026-04-24 bajo Hito 9).

- **miniaudio v0.11.21 vendored single-header** en `external/miniaudio/`, misma convención que stb. INTERFACE target + `miniaudio_impl.cpp` único con `MA_NO_ENCODING` + `MA_NO_GENERATION` para reducir binary size.
- **`AudioDevice` null-safe**: si `ma_engine_init` falla (CI, hardware roto), el device queda en modo mute — `isValid()` devuelve false pero play/stop/etc. son no-ops silenciosos. El motor sigue corriendo.
- **`AudioClip` no dueño del PCM**: miniaudio tiene resource manager interno que cachea decodificaciones. AudioClip solo guarda path + metadata. `ma_sound_init_from_file` dentro del device crea instancias de playback compartiendo el PCM.
- **`AssetManager` con tabla paralela** para audio, análoga a la de texturas. `AudioAssetId` (u32, 0 = missing) separado de `TextureAssetId`. Factoría de audio inyectable para tests (default usa AudioClip real que inspecciona con ma_decoder).
- **Inspector `Reproducir` resetea `started`** en lugar de disparar play() directo: mantiene al panel desacoplado del runtime (no necesita `AudioDevice&`). El AudioSystem hace el disparo en el próximo frame.
- **`AudioSourceComponent` NO se serializa** (mismo motivo que `ScriptComponent` en Hito 8: el modelo Scene-driven no es autoritativo aún). El demo "Agregar audio source demo" spawnea la entidad programáticamente.

---

## Pendientes que quedan para Hito 10 o posterior

- **Persistencia de `AudioSourceComponent`** en `.moodmap`. Igual al `ScriptComponent` del Hito 8, espera al modelo Scene-driven (Hito 10+ con assimp / mesh imports).
- **Drag & drop de audio** desde AssetBrowser a entidades. Hoy solo texturas tienen drag&drop (Hito 6 Bloque 0). Extender `ImGui::SetDragDropPayload` con un tipo `MOOD_AUDIO_ASSET` y receiver en el Hierarchy o Inspector.
- **Lua bindings de audio**: `audio.play("audio/beep.wav")` desde scripts. No entra en Hito 9; requiere un script demo que lo justifique.
- **Reverb / filters / effects**: entra cuando aparezca diseño de sonido real (post-Hito 15 que empieza a tocar polish).
- **Streaming de música**: clips largos cargan todo en RAM por ahora. miniaudio soporta streaming con `ma_sound_init_from_file` + `MA_SOUND_FLAG_STREAM`. Trigger: cuando el motor necesite música de fondo de varios minutos.
- **Listener múltiple / configurable**: hoy un único listener = cámara activa. Para cinematics o cameras escénicas, exponer `setListener` por componente cámara.
- **AudioSystem: tracking explícito de handles por entidad**: hoy `AudioSystem::clear()` llama `AudioDevice::stopAll()`, pero si hay otros sonidos non-component-managed en el futuro, esto los mataría también. Agregar un set de handles activos por sistema.
