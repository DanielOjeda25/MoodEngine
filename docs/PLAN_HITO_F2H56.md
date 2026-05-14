# PLAN HITO F2H56 — TBD (Render polish continuación)

> **Estado**: STUB — pendiente decidir scope con el dev (2026-05-14).
> **Tag previsto**: `v1.43.0-fase2-hito56`.
> **Origen**: segundo hito de **Sub-fase 2.6 — Render polish**. F2H55 cerró bloom; al cerrar el dev mencionó que su escena de testing improvisada **no proyecta sombras** sobre la caja blanca. Eso quedó como reactive fix candidate pero no entra en F2H55. Hay 2 opciones razonables para F2H56:

---

## 🎮 Pregunta para el dev (en mecánicas — leer esto primero)

Después del bloom, ¿por dónde seguimos?

### Opción A — Investigar por qué tu escena de testing no tiene sombras

**Qué cambia para el jugador**: cuando ponés objetos en una escena nueva, las sombras dinámicas se ven sin que tengas que configurar nada extra. Hoy parece que falta o un directional light por default, o el `casts shadows` está apagado, o el `MeshRenderer` no recibe sombras. Investigación rápida (~1-2h) + fix puntual.

**No es un hito grande** — son ~3 bloques. Útil si la falta de sombras te molesta inmediatamente y querés que la próxima caja de testing tenga sombra sin pelear con el setup.

### Opción B — AO (sombras de rincón) por SSAO o FidelityFX CACAO

**Qué cambia para el jugador**: los rincones, las grietas, donde un objeto toca el piso — se oscurecen sutilmente. Los objetos dejan de "flotar" — se sienten apoyados en el mundo. Es el siguiente eslabón del polish después de bloom según el orden planeado en F2H55.

**Es un hito grande** — probablemente 8-10 bloques (~12-15h). Implica un G-buffer pass adicional o sample del depth buffer, blur bilateral, integración con el pipeline de iluminación existente. Decisión técnica abierta: implementar SSAO propio (port de Filament) vs adoptar **AMD FidelityFX CACAO** (más calidad, mantenido por AMD, MIT — re-evaluación que se difirió de F2H55).

### Opción C — Color grading (LUT-based "look")

**Qué cambia para el jugador**: el mismo mapa puede sentirse cálido (atardecer Red Dead), frío (pantano Witcher), saturado (Borderlands), o lavado tipo cine (Last of Us). Le da MOOD a cada mapa más allá del bloom. Encaja literal con el nombre del motor.

**Tamaño**: medio (~6-8 bloques, 8-10h). Sumar un LUT 3D al composite del post-process, slider de intensidad, UI para cargar `.cube` files. Sin librería externa necesaria.

### Opción D — Algo completamente distinto

Si ya estás cansado del render polish y querés volver a gameplay (combate, AI de NPCs), decímelo y pivot el roadmap.

---

## Mi recomendación

**Opción A primero** (~1-2h reactive fix) y después **B o C** según mood. La sombra es lo que más te molesta ahora — resolverlo te deja con un editor que "se ve completo" antes de seguir con polish profundo. AO/color grading son hitos grandes, mejor encararlos con la base ya pulida.

---

## Cuando confirmes la opción

Cuando elijas (A/B/C/D + comentarios), reemplazo este stub por un PLAN_HITO_F2H56.md completo con:
- Bloques de ejecución A-N
- Criterios de aceptación
- NO entra (scope explícito)
- Riesgos identificados
