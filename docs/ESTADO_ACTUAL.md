# Estado actual del proyecto — handoff para el agente

> **Propósito:** dar al próximo agente el estado exacto del proyecto, comandos que sabemos que funcionan, y el siguiente paso concreto. Leer esto **antes** de `MOODENGINE_CONTEXTO_TECNICO.md`.
>
> **Convención de tamaño** (AUDIT-1, 2026-05-17): este documento mantiene SOLO el último hito cerrado + último audit + pendientes. Los entries históricos viven en [`hitos/F2H<N>.md`](hitos/) y en el git history (tags). No se acumulan "0bis" acá.

---

## 0. Último cierre — AUDIT-3 (2026-05-17)

**Cierre de la tanda inicial de audits.** Tag `v1.49.3-audit-3`. Reporte completo en [`audits/AUDIT_3.md`](audits/AUDIT_3.md).

**Resumen consolidado de la tanda (AUDIT-1 + 2 + 3, ~10h totales):**

```
                      AUDIT-1   AUDIT-2   AUDIT-3
HARD cap violations:    5         2         0
Layer violations:       —         2         0
Pure helpers extracted: 0         0         1 + 7 tests
```

**Qué entregó AUDIT-3:**

- **Layer fixes mecánicos**: `Workspace` struct movido a `engine/project/`, `i18n` a `core/i18n/`. 55 archivos updateados. 0 violaciones cross-layer ahora.
- **Split `tests/test_scene_serializer.cpp`** 934 → 302 LOC (core) + 210 (lighting/physics) + 399 (gameplay) + 39 (helpers). Patrón `_helpers.h` para fixtures de test compartidas.
- **Pure helper `pickRayFromNdc`** + 7 unit tests headless (`core/math/Ray.h` + `tests/test_ray.cpp`). 5 call sites refactorizados, ~42 LOC de duplicación eliminada.
- **Split `EditorUI.h`** 836 → 483 LOC (-42%). Inline bodies movidos a 4 archivos `EditorUI_<Family>.inl` por categoría (Spawn / Tools / Entity / Project). HARD cap = 0.

**Próximo audit**: solo si emerge dolor concreto. No agendar audits vacíos por cumplir la cadencia.

---

## 0.1. Último hito de feature — F2H62 (2026-05-17)

**Shader Graph runtime + migración a imnodes.** Tag `v1.49.0-fase2-hito62`. Detalle completo en [`hitos/F2H62.md`](hitos/F2H62.md). Validado por dev: *"creo que esta bien"*.

**Cierra Sub-fase 2.3 (Renderer) del plan original Fase 2 al 100%** (PBR + bloom + SSAO + CSM + SSR + shader graph).

---

## 1. Estado del plan

### Sub-fases cerradas

- **Sub-fase 2.1** (cimientos): F2H1-F2H10. ✅ Tag `v1.1.7-fase2-hito10`.
- **Sub-fase 2.2** (CSG editor): F2H11-F2H22. ✅ Hammer-style cerrado.
- **Sub-fase 2.3** (Renderer) del plan original: Hito 17 (PBR) + F2H55 (bloom) + F2H56 (SSAO) + F2H60 (CSM) + F2H61 (SSR) + F2H62 (shader graph). ✅ **cerrada con AUDIT-1**.
- **Sub-fase 2.5** (gameplay loop): F2H43-F2H53 (i18n + dialogs + inventory + quests). ✅ Tag `v1.41.0-fase2-hito53`.

### En curso

- **Sub-fase 2.6** (Render polish): F2H55, F2H56, F2H58, F2H59, F2H60, F2H61, F2H62 cerrados. AUDIT-1 cerrado.

### Próximo

- **F2H63 — Materiales translúcidos / alpha blending** (transparencia: render pass separado post-opaque + sort back-to-front + alpha output del fragment + UI material toggle Opaque/Translucent). Pedido del dev al cierre F2H62. Plan stub en [`PLAN_HITO_F2H63.md`](PLAN_HITO_F2H63.md).

---

## 2. Entorno de build — lo que realmente funciona

### Toolchain real

- **Visual Studio 2022 Community** → MSVC 14.44 + SDK Windows 11 + CMake 3.31 bundleado. **Es el que usamos.**

Ruta clave:
```
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat
```

Cargar entorno MSVC desde PowerShell:
```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 && <tu_comando_aqui>'
```

Desde **Developer Command Prompt for VS 2022** del menú inicio, `cl` y `cmake` funcionan directamente.

### Versiones verificadas

- `cl` → `19.44.35226 for x64`
- `cmake` → `3.31.6-msvc6`
- `git` → `2.49.0.windows.1`

### Generador CMake

`Visual Studio 17 2022` (ya en `CMakePresets.json`).

---

## 3. Comandos que funcionan

Desde la raíz del repo, con entorno MSVC cargado:

```bash
# Configurar (descarga deps la primera vez, ~5 min)
cmake --preset windows-msvc-debug

# Compilar
cmake --build build/debug --config Debug

# Ejecutar
./build/debug/Debug/MoodEditor.exe

# Tests
./build/debug/Debug/mood_tests.exe

# Sizeometer (AUDIT-1)
tools/sizeometer.sh 30
```

---

## 4. Qué hacer al arrancar la próxima sesión

1. Leer este archivo entero (es chico — diseñado para eso).
2. Leer el plan del hito en curso: `docs/PLAN_HITO_F2H<N>.md` (actualmente F2H63).
3. `git status` + `git log --oneline -10` + `git tag --sort=-v:refname | head -5`.
4. Preguntar al dev: "¿seguimos con el hito en curso o pasó algo nuevo?"
5. Si arrancamos hito nuevo: refinar el plan stub con el dev → bloques A-X concretos.
6. Trabajar bloque por bloque, marcando todo en el plan + DECISIONS al aparecer.
7. Al cerrar: commit "feat(F2H<N> Bloque X)" + "docs(F2H<N> cierre)" + tag + actualizar HITOS.md (one-liner) + ESTADO_ACTUAL.md (reemplaza section 0.1, no acumular) + crear `docs/hitos/F2H<N>.md`.

---

## 5. Gotchas conocidos

1. **VS 2026 Community sin workload C++**: no usar. Solo VS 2022.
2. **SDL2 debug se llama `SDL2d.dll`** no `SDL2.dll`. POST_BUILD lo copia correcto.
3. **GLAD v2 naming**: header `<glad/gl.h>` (no `<glad/glad.h>`), source `gl.c`, loader `gladLoaderLoadGL()`.
4. **Primera config `cmake --preset` tarda ~5 min** (descarga deps SDL2/ImGui/spdlog/GLM). Incrementales son segundos.
5. **Shaders con path relativo `shaders/*.{vert,frag}`**. Funciona desde repo root y desde dir del exe (POST_BUILD copy_directory).
6. **Al editar shaders**: copiar al build dir si el exe ya está deployed (`build/debug/Debug/shaders/`). CMake solo copia en build.
7. **2 máquinas de dev**: notebook Iris Xe + desktop Ryzen 5600G + GTX 1660. En desktop forzar NVIDIA via Panel de Control.

---

## 6. Filosofía operacional (resumida)

- **Ship something**: no romper el build entre commits.
- **No implementar hitos futuros** "por adelantar trabajo".
- **Cero scope creep**: bug fixes reactivos OK, refactors planeados van a AUDIT-N.
- **Comentarios en español**, convención commits `tipo(scope): mensaje`.
- **Soft 500 / hard 800 LOC por archivo**. AUDIT detecta y splittea.
- **Preguntar al dev antes de asumir** ante ambigüedades.

---

## 7. Archivos clave por tarea

| Para... | Tocar |
|---|---|
| Añadir dependencia | `CMakeLists.txt` (bloque CPM) |
| Cambiar flags compilador | `cmake/CompilerWarnings.cmake` |
| Añadir panel del editor | `src/editor/panels/*` + `EditorUI.(h\|cpp)` |
| Registrar decisión arquitectónica | `docs/DECISIONS.md` (append-only) |
| Cerrar hito | `docs/HITOS.md` (one-liner) + `docs/hitos/F2H<N>.md` (detalle) |
| Tech debt audit | `tools/sizeometer.sh` + `docs/audits/AUDIT_N.md` |
| Cambiar log level | `src/core/Log.cpp` |

---

## 8. Histórico

Los entries de hitos anteriores a F2H62 viven inline en `docs/HITOS.md` con el formato "fat" original. Los detalles más viejos están en `docs/archive/`. El histórico completo se navega por `git log` + tags `v*.*.*`.
