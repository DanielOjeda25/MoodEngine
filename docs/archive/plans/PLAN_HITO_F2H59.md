# PLAN HITO F2H59 — Primitivas Plano + clásicas (suelo, etc.)

> **Estado**: DRAFT pendiente arranque (2026-05-16).
> **Tag previsto**: `v1.46.0-fase2-hito59`.
> **Origen**: pedido del dev al cierre de F2H58 tras detectar que estaba usando una Box brush aplastada como suelo durante el tour visual de Color Grading. Cita verbatim: *"la base es un cuadrado comprimido, no tenemos la primitiva de un plano? deberiamos tener… estaria bien que tengamos las primitivas clasicas para no modelar todo desde la base"*.

---

## 🎮 Mecánicas (lo que vivís como dev)

### Las primitivas que hoy existen

Click derecho en el menú `Brush > Añadir`:
- Box (caja)
- Cylinder (cilindro)
- Sphere (esfera)
- Pyramid (pirámide)
- Wedge (cuña)
- Prism triangular
- Prism hexagonal

**Falta lo más común para empezar una escena: el Plano** (suelo). Para hacer un suelo hoy hay que añadir una Box y aplastarla a Y≈0.05 manualmente — 3 pasos cuando debería ser 1.

### Cómo se aplica en MoodEngine post-F2H59

- Click en `Brush > Añadir > Plano` → spawn de un brush rectangular 10m × 10m × 0.05m centrado en el origen del workspace.
- Las dimensiones siguen siendo editables vía gizmo + Inspector (igual que los otros brushes — el "plano" es una preset de Box con dimensiones por default ya optimizadas para "suelo").

### Convención industria

- **Unity**: GameObject > 3D Object > Plane (10x10m flat) + Quad (1x1m flat).
- **Unreal**: Place Actor > Basic > Plane (200x200uu = ~2x2m). Hay también un "Floor" actor con BSP plane.
- **Godot**: 3D > PlaneMesh + MeshInstance3D. Default 2x2m.
- **Hammer Editor / Source**: no tiene "plano" como primitiva separada — usa una box muy fina (mismo workaround que MoodEngine pre-F2H59).

MoodEngine combina la flexibilidad CSG de Source con la conveniencia UX de Unity/Unreal/Godot. El plano como primitiva separada en el menú es solo un **preset de Box con dimensiones óptimas para el caso común**.

---

## 🧱 Bloques de ejecución

### Bloque A — Primitiva Plano

- **`ProjectAction` nuevo**: `AddPlaneBrush` en `EditorMode.h` / `EditorProjectActions.cpp`.
- **Handler**: reusa la lógica existente de `AddBoxBrush` pero con `BrushPrimitive::makeBox(10.0f, 0.05f, 10.0f)` (dimensiones default).
- **Menú**: entry "Plano" en `MenuBar.cpp::editor.menu.brush.add` arriba de Box (más común que Box puro).
- **i18n key**: `editor.menu.brush.plane` → "Plano" / "Plane".

**Estimado**: 30 min. Es puramente UX glue sobre código existente.

**Criterio de aceptación**: dev click en Brush > Añadir > Plano → spawn de un brush 10x0.05x10 centrado. Editable normalmente. Round-trip al `.moodmap` correcto (es un brush Box estándar bajo el capó).

---

### Bloque B — (opcional) Primitivas adicionales clásicas

Evaluar si vale sumar:
- **Cono**: usable para spotlights cosméticos, tiendas de campaña, sombreros, etc.
- **Cápsula**: usable para personajes proxy, pilares redondeados.
- **Toro**: usable para anillos, donuts decorativas, marcos circulares.
- **Quad** (1x1m flat): la versión "small" del Plano — útil para sprites/billboards.

Cada uno requiere generación de geometría en `BrushPrimitive::makeXxx(...)`. Cono y cápsula son los más útiles a corto plazo; toro y quad son nice-to-have.

**Decisión deferida al arranque** según el alcance que pida el dev: si el Plano cubre el 90% del use case, frenamos en Bloque A + cierre. Si pide la batería completa, hacemos B con cono + cápsula como mínimo.

**Estimado**: 30 min por primitiva nueva.

---

### Bloque C — Cierre

HITOS + ESTADO + DECISIONS + plan archivado + tag `v1.46.0-fase2-hito59`. ~30 min.

---

## Total estimado

**Bloque A solo**: ~1h. **Bloque A + Bloque B con 2 primitivas**: ~2h. Hito chico.

---

## NO entra en F2H59

- Modelado paramétrico avanzado (LODs, blueprints estilo Unreal). Las primitivas son brushes CSG simples.
- Library de primitivas custom (kitbash sets, modular kits). Sub-fase posterior si emerge demanda.
- Snapping inteligente (snap-to-grid avanzado, vertex snap). El gizmo existente cubre el caso común.
- Pintado de texturas por face (texture face editor estilo Hammer). Hito separado.

---

## Riesgos identificados

- **Default size del Plano**: 10x10m es razonable para escenas exteriores. Si emerge que los devs hacen interiores chicos (cuartos 5x5m) y el plano default 10x10 es overkill, ajustar.
- **Plano flotando por la altura 0.05m**: el centro Y del brush queda a 0.025m por encima del piso real. Considerar spawn con `transform.position.y = -0.025f` para que la TOP cara quede exactamente en Y=0. Decisión final al implementar.
- **Eventos de selección post-spawn**: el flow existente de `AddBoxBrush` ya hace auto-select de la entidad nueva. Reusarlo sin tocar — si rompe algo en el plano específicamente, investigar.
