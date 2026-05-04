# Plan — F2H10: CI/CD con GitHub Actions

> **Leer primero:** `ESTADO_ACTUAL.md` (F2H8 cerrado), `PLAN_FASE2.md`
> sección 4 (este hito era F2H8 plan original = "CI/CD con GitHub
> Actions"; renumerado a F2H10 por los swaps de F2H4-F2H8 anteriores).

---

## Objetivo

Establecer un pipeline de **integración continua** que corra en cada
push a `main` y cada Pull Request. Protege contra regresiones,
da visibilidad pública del estado del proyecto vía badge, y prepara
la infraestructura para una eventual colaboración externa.

**Por qué ahora**: post-F2H8 el proyecto tiene 396 tests + 8 hitos
de Fase 2 cerrados. Sin CI, cualquier regresión accidental puede
durar varios commits sin detectarse. La sub-fase 2.2 (CSG) viene
grande — entrar con CI verde es responsabilidad básica.

---

## Filosofía de diseño

- **Solo Windows MSVC** (decisión durable del dev: "multiplataforma
  Linux fuera de scope"). Si algún día emerge necesidad cross-platform,
  agregar Linux como matrix entry.
- **Sin Dependabot** (ruido innecesario en proyecto solo; las deps
  CPM ya están pinneadas con tags exactos).
- **Cache agresivo** del directorio `_deps` de CMake / CPM para que
  builds incrementales del CI tomen 2-3 min en lugar de 10-15.
- **Suite obligatoria**: el job falla si `ctest` falla. Sin esto el
  CI es decorativo.
- **Release auto al taguear**: trigger en `git push origin v*.*.*`,
  crea GitHub Release con el message del tag como notes. Sin assets
  pre-compilados (no hace falta para v1; el dev clona y compila
  hasta que haya "binary releases" como hito propio).

---

## Decisiones arquitectónicas

| # | Decisión | Valor |
|---|---|---|
| 1 | Plataforma CI | Solo `windows-latest` (matrix vacía) |
| 2 | Compilador | MSVC (Visual Studio 2022 — viene con `windows-latest`) |
| 3 | Build mode | Debug (matchea desarrollo del dev). Release queda diferido si emerge necesidad. |
| 4 | Trigger del workflow build | `push: branches: [main]` + `pull_request: branches: [main]` |
| 5 | Cache de CPM deps | `actions/cache@v4` con key basada en hash de `CMakeLists.txt` |
| 6 | Suite obligatoria en CI | `ctest --output-on-failure -C Debug` |
| 7 | Badge | Status del workflow `build.yml` en README.md |
| 8 | Release auto | Workflow `release.yml` separado, trigger `push: tags: 'v*.*.*'` |
| 9 | Dependabot | NO en v1 |

---

## Bloques

### A — Plan F2H10 (este archivo)

Documento del hito. Cierra al commit inicial.

### B — `.github/workflows/build.yml`

Workflow principal de CI. Estructura:

```yaml
name: build
on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

jobs:
  windows-msvc-debug:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - name: Cache CPM deps
        uses: actions/cache@v4
        with:
          path: build/debug/_deps
          key: cpm-${{ hashFiles('CMakeLists.txt') }}
      - name: Configure
        run: cmake --preset windows-msvc-debug
      - name: Build
        run: cmake --build build/debug --config Debug
      - name: Test
        run: ctest --test-dir build/debug -C Debug --output-on-failure
```

Notas técnicas:
- `actions/checkout@v4`: clona el repo. `submodules: false` por
  default (nuestro repo no tiene submodules, todo via CPM).
- `windows-latest` viene con MSVC 2022 + cmake + ninja preinstalados.
- Cache key incluye `hashFiles('CMakeLists.txt')`: si una dep CPM
  cambia de tag, la cache se invalida automáticamente.

### C — Badge + sección "Build status" en README

Agregar al inicio del `README.md`:

```markdown
[![build](https://github.com/DanielOjeda25/MoodEngine/actions/workflows/build.yml/badge.svg)](https://github.com/DanielOjeda25/MoodEngine/actions/workflows/build.yml)
```

Si el repo público todavía no tiene README estructurado, agregar
sección breve "Build status" con el badge + 3 líneas de cómo
clonar/compilar localmente (no es F2H9 docs públicas todavía,
solo lo mínimo).

### D — `.github/workflows/release.yml`

Trigger en push de tags `v*.*.*`. Crea un GitHub Release con el
message del tag como body. Sin assets pre-compilados.

```yaml
name: release
on:
  push:
    tags:
      - 'v*.*.*'

jobs:
  create-release:
    runs-on: ubuntu-latest  # release creation no necesita compilar
    permissions:
      contents: write       # para crear el release
    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0    # necesita los tags
      - name: Get tag message
        id: tag
        run: |
          echo "name=${GITHUB_REF#refs/tags/}" >> $GITHUB_OUTPUT
          echo "body<<EOF" >> $GITHUB_OUTPUT
          git tag -l --format='%(contents)' "${GITHUB_REF#refs/tags/}" >> $GITHUB_OUTPUT
          echo "EOF" >> $GITHUB_OUTPUT
      - name: Create release
        uses: softprops/action-gh-release@v2
        with:
          name: ${{ steps.tag.outputs.name }}
          body: ${{ steps.tag.outputs.body }}
```

`softprops/action-gh-release@v2`: action mantenida y ampliamente
usada para crear releases automáticos sin trabajo manual.

### E — Verificación

Cuando pusheamos los workflows:
1. El primer push a main dispara `build.yml`.
2. Verificar en `https://github.com/DanielOjeda25/MoodEngine/actions`
   que el workflow corrió.
3. Si falla:
   - Si es timeout (deps tardan en bajar): aumentar `timeout-minutes`.
   - Si es path/cmake inexistente: ajustar cmd.
4. Una vez verde, push de un tag de prueba (`v1.1.7-fase2-hito10` al
   cierre) verifica el `release.yml`.

### F — Cierre + tag

- `docs/HITOS.md`: F2H10 [x].
- `docs/ESTADO_ACTUAL.md`: sección 1 reescrita con F2H10 cerrado,
  badge en README activo. F2H8 movido a "anterior".
- `docs/DECISIONS.md` (2026-05-XX): "CI con GitHub Actions —
  rationale del scope solo Windows + sin Dependabot".
- Tag: `v1.1.7-fase2-hito10`.

---

## Suite

No hay tests propios del workflow (los .yml no se testean con doctest).
La validación es **el workflow corriendo verde** en GitHub Actions.

Suite global: igual que post-F2H8 (**396/6948**) — F2H10 es
infraestructura, no agrega features al motor.

## Riesgos

- **Tiempo de la primera build en CI**: con 8 deps CPM (Tracy,
  meshoptimizer, assimp, Jolt, EnTT, sol2, lua, etc.) la primera
  build puede tardar **10-20 min**. Las siguientes con cache
  bajan a 2-5 min. Aceptable.
- **Cache invalidation incorrecto**: si la key del cache no captura
  un cambio de versión de dep, los deps quedan stale. Mitigación:
  hash de `CMakeLists.txt` (donde están los `CPMAddPackage` con
  `GIT_TAG`).
- **Tests que requieren display GL**: deberían fallar en CI headless.
  Mitigación: marcar esos tests con `--test-suite-exclude` o usar
  `xvfb` (Linux); para Windows no hay equivalente fácil. Si emerge,
  saltarlos con `[skip-ci]` en el nombre del test.
- **GitHub Actions billing**: el repo es privado del dev (probable),
  con el plan free son 2000 min/mes. Una build Windows ~5-10 min,
  ~200 builds al mes = OK.

---

## Criterios de cierre

- [ ] `.github/workflows/build.yml` cableado y corriendo verde en
      `main`.
- [ ] Cache de CPM activo (segunda build < 5 min).
- [ ] Badge en README.md verde.
- [ ] `.github/workflows/release.yml` cableado.
- [ ] Tag de cierre `v1.1.7-fase2-hito10` dispara el workflow de
      release y crea un GitHub Release con el message del tag.
- [ ] `docs/HITOS.md`, `ESTADO_ACTUAL.md`, `DECISIONS.md` actualizados.
- [ ] Tag `v1.1.7-fase2-hito10` pusheado.
