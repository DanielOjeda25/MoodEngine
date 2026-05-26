# PLAN F3H16 — Hover preview ampliada (Asset Browser)

**Estado:** **A DEFINIR** (arrancar tras F3H15).
**Predecesor:** F3H15 (mejoras del MaterialPreviewRenderer — cache disco compartido).
**Origen:** `PLAN_FASE3.md` Sub-fase 3.3 lista "Hover preview ampliada".

---

## Avance de Sub-fase 3.3

```
F3H14 ✅ — Mejoras MeshThumbnailRenderer (cache disco + resolución + gradiente)
F3H15 ✅ — Mejoras MaterialPreviewRenderer (cache disco compartido + reusos)
F3H16 –  — ⬅ próximo: Hover preview ampliada
F3H17 –  — Drag&drop con feedback visual
F3H18 –  — Validador de assets rotos
F3H19 –  — Rename con cascada
```

---

## Norte

`PLAN_FASE3.md` declara:
> **F3H16 — Hover preview ampliada.**
> Hover prolongado (> N ms, configurable) sobre asset → tooltip grande con preview ampliado + metadata. Estilo Substance Designer.

**Mecánica del editor (cómo se siente):** el dev mueve el cursor sobre un thumb del Asset Browser. Espera 500-700 ms con el cursor quieto. Aparece un tooltip grande (384×384 o 512×512) con el preview ampliado del asset + metadata textual (path completo, dimensiones, slot count, etc). Mover el cursor cierra el tooltip al instante.

---

## Scope candidato

### Trabajo principal

1. **Detección de hover prolongado** (no inmediato): `ImGui::IsItemHovered()` + tracker de duración. Cuando `HoveredIdTimer > 0.5s` (configurable en UserSettings), gatillar el tooltip ampliado.

2. **Render del tooltip grande**:
   - **Opción A**: rerenderizar el asset a un FBO grande (384×384). Latencia visible al primer hover, pero la imagen es nítida.
   - **Opción B**: usar el thumb cacheado (128) y escalarlo via ImGui::Image. Inmediato pero pixelado.
   - **Opción C** (preferida): segundo cache disco con prefix size mayor (ej. `mesh_<hash>_384.png`). El sistema F3H14/F3H15 ya soporta múltiples sizes — pedir `thumbnailFor(meshId)` con override de size genera/cachea el 384 a demanda. Inmediato en hovers subsiguientes.

3. **Metadata textual** en el tooltip:
   - **Meshes**: path, vertex count, submesh count, bounding box (lo que `MeshAsset` exponga).
   - **Materiales**: path, albedo tint, metallic/roughness/ao base, count de texture maps.
   - **Texturas** (si se quiere expandir): path, resolución, formato.

4. **Configuración**:
   - `UserSettings.editor.hoverPreviewDelayMs` (int, default 500, clamp 0-3000). Slider en User Preferences.
   - **Decisión**: ¿Tamaño del tooltip ampliado también configurable? Probable que sí, default 384.

### Decisiones a tomar al arrancar

1. **Opción A/B/C del render del tooltip** — recomiendo C (cache disco múltiples sizes, ya soportado).
2. **Scope cobertura**: ¿solo meshes + materiales (siguen los hitos previos), o expandir a texturas/animaciones/prefabs en el mismo hito?
3. **Trigger del hover**: ¿solo desde el Asset Browser, o también desde el Inspector (Material slot del MeshRenderer, mesh dropdown, etc)?
4. **API del renderer**: ¿`thumbnailFor(id, sizeOverride)` con parámetro opcional, o método separado `thumbnailLargeFor(id)`?

---

## Trabajo NO trivial

- **First-hover latency**: si C se usa, el primer hover de cada asset gatilla un render 384×384 que el dev "siente" como hitch. Mitigación: precomputar al boot del Asset Browser en background — pero introduce thread pool. Backlog si molesta.
- **Tooltip multi-display**: si el dev tiene 2 monitores y el Asset Browser está al borde, el tooltip podría salirse de pantalla. ImGui maneja esto automáticamente (clamp), validar comportamiento.
- **Metadata para Brush/Vehicle/Scripts**: campos heterogéneos. Diseñar `AssetMetadata` struct uniforme o branches per-tipo.

---

## Alternativas a F3H16

### B) F3H17 (Drag&drop con feedback visual)

Saltarse F3H16 e ir directo a drop zones destacadas + cursor cambia al arrastrar. Mejora UX inmediata; F3H16 es polish, F3H17 es funcional.

### C) Backlog UX gaps (memoria `backlog-ux-gaps-editor`)

- Spawn de ForceField/Cloth desde el menú "+ Crear Entidad" / Hierarchy.
- Workflow "agregar sonido al activar mesh".

---

## Recomendación

Yo (Claude) sugiero **opción A — Hover preview ampliada** siguiendo el orden del plan original:
1. Sub-fase 3.3 continúa lineal.
2. F3H14+F3H15 dejaron toda la infra del cache disco lista — F3H16 es "pedir a la cache un size más grande". Bajo riesgo.
3. Mejora UX visible inmediato.

**Preguntas al dev cuando arranque F3H16:**
1. ¿Confirmás A o querés B/C?
2. ¿Tamaño default del tooltip ampliado (256 / 384 / 512)?
3. ¿Cobertura: solo Asset Browser, o también Inspector slots?

---

## Lo que NO toca F3H16

- F3H17 (Drag&drop): hito propio.
- F3H18 (Validador): hito propio.
- F3H19 (Rename con cascada): hito propio.
- Sub-fase 3.4 (Viewport pro): F3H20+.
