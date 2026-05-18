# PLAN HITO F2H64 — OIT + sombras translúcidas

> **Estado**: STUB. Arranca tras cerrar F2H63.
> **Tag previsto**: `v1.51.0-fase2-hito64`.
> **Origen**: split scope de F2H63 (2026-05-17). El dev pidió transparencia realista; F2H63 entregó la base visual (alpha + refracción + Fresnel). F2H64 entrega la **correctness técnica**: orden correcto sin sort manual, y vidrios que proyectan sombras tintadas.

---

## 🎮 Mecánicas (lo que vivís como dev)

### Qué entrega F2H64

- **Sin parpadeo en escenas con muchos translúcidos solapados**. Podés tener un cuarto con 20 botellas de vidrio una encima de otra; ninguna salta de orden frame-a-frame.
- **Vidrios proyectan sombras tintadas**: una botella verde tira una sombra verdosa en el piso. Una botella opaca tira sombra negra estándar. El motor aprende a hacer las dos a la vez.
- **Funciona sin que el dev tenga que tocar nada**: F2H63 ya tiene BlendMode + opacity + IOR. F2H64 solo cambia el algoritmo interno, la API del Inspector queda igual.

### Cómo se integra

- **OIT Weighted Blended (McGuire 2013)**: 2 render targets nuevos (`accumColor` RGBA16F, `revealage` R16F). Translucents escriben acumulados ponderados por peso heurístico (depth-based weighting). Pase de composite final mezcla con el opaco. **Reemplaza el sort manual de F2H63** — el algoritmo es order-independent.
- **Shadow atlas RGB**: la pasada de shadow gana 3 canales (R/G/B en vez de R-only depth). Translucents escriben su color × (1 - opacity) al canal. Sampling del shadow tinta la luz directional según el camino acumulado.

---

## 🧱 Bloques tentativos (DRAFT — refinar al arrancar)

### Bloque A — OIT MRT setup (~2h)

- Nuevos color attachments en el FBO main: `accumColor` RGBA16F, `revealage` R16F.
- Clear correcto al inicio del pase translucent (accum=0, revealage=1).
- `glDrawBuffers` con los 3 attachments (color, accum, revealage).

### Bloque B — Shader OIT (~3h)

- `pbr.frag` ramifica si compilado con `#define OIT_TRANSLUCENT`:
  ```glsl
  float weight = clamp(pow(min(1.0, opacity * 10.0) + 0.01, 3.0) * 1e8 * pow(1.0 - depth * 0.9, 3.0), 1e-2, 3e3);
  accumColor = vec4(rgb * opacity, opacity) * weight;
  revealage = opacity;
  ```
- Variantes pbr / pbr_skinned / pbr_instanced con la formulación OIT.
- `pbr_graph_template.frag` también — el template emite ambos paths (Opaque y OIT) y el preprocesador elige.

### Bloque C — Composite pass (~2h)

- Fullscreen shader `oit_composite.frag`:
  ```glsl
  vec4 accum = texture(uAccum, uv);
  float reveal = texture(uRevealage, uv).r;
  FragColor = vec4(accum.rgb / max(accum.a, 1e-5), reveal);
  ```
- Blendea sobre el color buffer opaco con `GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA`.
- Llamada después del translucent pass, antes de SSAO/SSR/bloom.

### Bloque D — Remover sort manual de F2H63 (~1h)

- `renderTranslucentPass` ya no ordena; itera en orden de creación.
- Tiebreaker por ID + sort obsoleto, eliminar.
- Tests de sort eliminados.

### Bloque E — Shadow atlas RGB (~3h)

- `m_shadowAtlas` cambia de `GL_DEPTH_COMPONENT24` a un MRT: depth + RGB color.
- Pase de shadow ramifica:
  - Opacos: escriben solo depth (canales RGB quedan en 1.0 = sin tinte).
  - Translucents: escriben depth + color × (1-opacity) acumulado al canal RGB usando blending multiplicativo.
- Sampling del shadow en `pbr.frag` lee depth (igual que ahora) **+** color del atlas, multiplica la luz directional.

### Bloque F — UI toggle "Proyectar sombra" en Material (~1h)

- Checkbox "Cast translucent shadow" en el Inspector → Blending. Default ON para Translucent, OFF para Additive (no tiene sentido).
- Persiste en MaterialAsset.

### Bloque G — Samples actualizados + tests (~2h)

- `sample_glass.moodshader` proyecta sombra teñida (verificable visualmente).
- Test: 3 translúcidos solapados → order-independent (mismo render con cualquier orden de iteración).
- Test: shadow atlas roundtrip con tinte.

### Bloque H — Validación visual + cierre (~1h)

- Cuarto con 20 botellas de varios colores, todas con BlendMode=Translucent. Ninguna parpadea al mover cámara.
- Botella verde sobre suelo blanco con sol directional → sombra verde tenue debajo.
- Tag `v1.51.0-fase2-hito64`.

---

## Total estimado

**8 bloques A-H, ~15h**. Hito mediano.

---

## Riesgos identificados

- **OIT weight tuning** — la heurística de McGuire requiere ajuste para que objetos cercanos a la cámara no dominen. Implementación de referencia + tests visuales.
- **Sombras translúcidas costosas** — cada translucent doble shadow draw (depth + color). Mitigación: cap visual (default 1 luz tinted, configurable).
- **Compatibilidad con CSM** (F2H60) — el cascade shadow atlas también necesita RGB. Verificar al implementar.
- **Compatibilidad con shader graph** (F2H62) — el template tiene que emitir ambos paths Opaque y OIT. Bumping de complejidad del generator.
