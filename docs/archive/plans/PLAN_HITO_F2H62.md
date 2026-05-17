# PLAN HITO F2H62 — Shader Graph runtime (cierra Sub-fase 2.3)

> **Estado**: STUB pendiente arranque (2026-05-17).
> **Tag previsto**: `v1.49.0-fase2-hito62`.
> **Origen**: F2H18 del plan original Fase 2. **Cierra Sub-fase 2.3 (Renderer)** del plan original junto con F2H61 SSR. Tras este hito, Sub-fase 2.3 queda 100% completa:
>
> - F2H17 ✅ material editor lite
> - F2H18 ✅ → **F2H62 → Shader Graph runtime**
> - F2H19/F2H56 ✅ SSAO
> - F2H20/F2H61 ✅ SSR
> - F2H21/F2H55 ✅ bloom
> - F2H22/F2H60 ✅ CSM
>
> Próximo gran scope post-F2H62: Sub-fase 2.4 (Física avanzada) o el ítem 1.0 del BACKLOG (Environment entry point) como UX cleanup antes.

---

## 🎮 Mecánicas (lo que vivís como dev)

### Qué es Shader Graph runtime

Un editor visual de nodos donde el dev compone shaders arrastrando bloques (texture sample, multiply, lerp, dot product, fresnel, noise…) y conectándolos con cables, sin escribir GLSL. El resultado se compila a un shader real que el motor usa en runtime para renderizar materiales.

Convención industria:
- **Unity Shader Graph** (URP/HDRP).
- **Unreal Material Editor** (más maduro, base del workflow visual del motor).
- **Godot Visual Shader** (más simple, propósito educativo).
- **Blender Shader Nodes** (referencia de UX no-engine).

### Cómo se integraría en MoodEngine

- **Workspace nuevo o panel nuevo**: ya tenemos `imgui-node-editor` integrado (F2H46, usado por Dialog Editor + Quest Editor). El Shader Graph reusa ese mismo widget.
- **Asset nuevo**: `.moodshader` con el grafo serializado (nodos + edges + parámetros). Carga via `AssetManager::loadShaderGraph`.
- **Vinculación con material**: el `MaterialAsset` (F2H17) acepta opcionalmente un `shaderGraphPath`. Si está, el material usa el GLSL generado en lugar del PBR estándar. Sin shaderGraphPath → comportamiento pre-F2H62 (PBR clásico).
- **Compilación**: al guardar el grafo, el editor genera GLSL on-the-fly y lo manda al `OpenGLShader`. Hot-reload en runtime (mismo patrón que F2H42 hot-reload de shaders).
- **Limitaciones v1**: solo fragment shader (vertex sigue siendo el `pbr.vert` con sus variants); set fijo de nodos básicos; uniforms automáticos (no sliders custom desde el grafo); sin sub-graphs reutilizables.

### Convención industria mínima v1

- Nodo terminal **Output PBR**: 5 inputs (Albedo, Metallic, Roughness, Normal, Emissive). El generador GLSL los inyecta en el PBR shader existente como overrides de los uniforms estándar.
- Nodos generadores: **Color** (constant vec3), **Float**, **UV** (vUv), **Time** (uTime), **Sample Texture** (sampler2D + UV).
- Nodos aritméticos: **Add**, **Multiply**, **Lerp**, **Power**, **OneMinus**, **Saturate**, **Clamp**.
- Nodos vectoriales: **Dot**, **Cross**, **Normalize**, **Length**, **Reflect**.
- Nodos PBR-específicos: **Fresnel** (NdotV based), **Normal Map** (decode tangent space).

---

## 🧱 Bloques de ejecución (DRAFT — refinar al arrancar)

### Bloque A — Asset `.moodshader` + schema

- `ShaderGraph::Asset` struct con `nodes` vector + `edges` vector + `outputNode` id.
- `Node` con id + type enum (Color/Float/UV/Sample/Add/Mul/.../OutputPBR) + parámetros tipados + lista de pins de input + lista de pins de output.
- Serialización JSON aditiva (mismo patrón que `.mooddialog` / `.mooditem` / `.moodquest`).
- `AssetManager::loadShaderGraph(path) → ShaderGraphAssetId`.

### Bloque B — Generador GLSL

- Función `generateGlsl(ShaderGraph::Asset& graph) → std::string`. Walk topológico del grafo desde el output node hacia atrás, emite código GLSL con variables intermedias.
- Plantilla base que incluye el header del `pbr.frag` (uniforms IBL, lights, shadow samplers, etc.) + body autogenerado a partir del grafo + escritura final `FragColor = ...; NormalRT = ...;` (manteniendo el output MRT de F2H61).
- Mapping de tipos: `Color` → `vec3`, `Float` → `float`, `UV` → `vec2`, etc.

### Bloque C — Editor visual (panel nuevo)

- `ShaderGraphEditorPanel` (`src/editor/panels/shader/`) usando `imgui-node-editor` (ya integrado en F2H46/F2H47/F2H53).
- Drag-and-drop de nodos desde una palette lateral. Conexión de pins con cables.
- Toolbar con botones Save / Generate Preview / Open in External (mostrar el GLSL generado en read-only).
- Hot-reload: al hacer Save, el material que use ese grafo recompila su shader en runtime.

### Bloque D — Integración con MaterialAsset + InspectorPanel

- `MaterialAsset` extendido con `std::string shaderGraphPath` opcional.
- `InspectorPanel_MeshRenderer` (donde se editan materiales): dropdown "Shader" con opciones "PBR estándar" (default) o un graph cargado.
- Cuando un material usa shader graph, el inspector muestra los uniforms exposed por el grafo (futuro: sliders custom desde el grafo).

### Bloque E — Compilación on-the-fly + caché

- Cache de GLSL generado por hash del grafo (evitar recompilar si el grafo no cambió).
- Fallback al `pbr.frag` estándar si la compilación GLSL falla (con error reportado al panel).

### Bloque F — Sample shaders shipados

- 2-3 ejemplos en `assets/shaders/graphs/`:
  - `water.moodshader` (UV scroll + normal map + fresnel).
  - `hologram.moodshader` (emissive scanlines + transparency).
  - `dissolve.moodshader` (noise threshold + edge glow).

### Bloque G — Tests

- Schema roundtrip JSON.
- Generador GLSL para casos básicos (constant color, sample texture, add+multiply).
- Detección de ciclos en el grafo (los grafos no pueden tener loops).

### Bloque H — Validación visual + cierre

Dev abre el ShaderGraph editor, carga el sample `water.moodshader`, ve el water en runtime sobre un plane. Modifica el scroll speed en vivo. Save → recompila. ~30 min.

### Bloque I — Cierre

HITOS + ESTADO + DECISIONS + plan archivado + tag `v1.49.0-fase2-hito62`. ~30 min.

---

## Total estimado

**9 bloques A-I, ~20-25h**. Hito grande (probablemente el más complejo de Sub-fase 2.3). Posible split en 2 sub-hitos si emerge complejidad: F2H62a (asset + generador + sample text) y F2H62b (editor visual + integración material).

---

## NO entra en F2H62

- **Vertex shader graph** — solo fragment en v1. Vertex queda con los `pbr*.vert` actuales.
- **Sub-graphs reutilizables** — todos los nodos son inline en v1.
- **Uniforms custom desde el grafo** — los uniforms los provee el motor (uTime, uCameraPos, lights, etc.). El dev no puede exponer un slider custom desde el grafo en v1.
- **Compute shaders** — fuera del scope (Sub-fase 3+).
- **Item 1.0 del BACKLOG** (Environment entry point) — sub-hito UX separado, decidir si va antes o después de F2H62 al arrancar.

---

## Referencias

- Unity Shader Graph documentation.
- Unreal Material Editor expressions reference.
- Godot VisualShader source.
- "Building a Visual Shader Editor in C++ with ImGui" (varios blog posts; revisar al arrancar).

---

## Riesgos identificados

- **Complejidad del editor visual** — `imgui-node-editor` ya está integrado (F2H46) pero el shader graph tiene requirements distintos (validación de tipos en pin connection, ordenamiento topológico estricto, generación GLSL). Posible scope creep.
- **Performance del GLSL generado** — un grafo torpe puede generar shader ineficiente. Optimización del output (dead code elimination, constant folding) queda como mejora futura.
- **Compatibilidad con CSM/SSR/Bloom existentes** — el shader graph debe emitir el output al MRT (FragColor + NormalRT) y reusar el path de CSM sampleShadow. Documentar en la plantilla base.
- **Hot-reload de shaders cuando el grafo cambia** — el material puede estar siendo usado por entidades activas. El recompile debe ser atómico (no romper el frame en curso).
