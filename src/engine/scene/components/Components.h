#pragma once

// Componentes de datos para el ECS (Hito 7). Todos son POD (no logica en
// metodos); los sistemas operan sobre ellos iterando via `Scene::forEach`.
//
// Convenciones:
//   - POD + constructores convenientes donde ayudan al call-site.
//   - Ningun componente hace I/O ni toca GL.
//   - MeshRendererComponent guarda punteros no-owning; AssetManager es
//     dueno del ciclo de vida.
//
// F2H81 (auditoría): este header pasaba las 900 líneas. Se partió en 3
// headers por categoría detrás de este agregador — los call-sites siguen
// incluyendo "Components.h" y obtienen el set completo, sin cambios:
//   - Components_Render.h   : Tag, Transform, MeshRenderer, Camera, Light,
//                             Environment, PrefabLink (+ isWheelEntityTag).
//   - Components_Physics.h  : RigidBody, Joint (+ kJointNoTarget), Ragdoll,
//                             Vehicle, VehicleSeat, ForceField, Cloth.
//   - Components_Gameplay.h : Script, AudioSource, Animator, Skeleton,
//                             NavAgent, ParticleEmitter, Trigger, Dialog,
//                             Inventory, ItemPickup.

#include "engine/scene/components/Components_Gameplay.h"
#include "engine/scene/components/Components_Physics.h"
#include "engine/scene/components/Components_Render.h"
