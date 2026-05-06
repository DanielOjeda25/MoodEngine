#pragma once

// F2H14: primitivas CSG mas alla de la box (F2H11). Cada helper
// devuelve un Brush listo para usar con todas las ops booleanas
// (subtract / unionOp / intersectOp) — el algoritmo CSG core no
// requiere modificacion: una primitiva es solo un Brush con N
// planos.
//
// Convencion canonica:
//   - Unit cell [-0.5, +0.5]^3 antes del worldFromLocal.
//   - Eje Y = altura para cilindro / prisma / piramide.
//   - Normales hacia afuera (signedDistance > 0 = punto exterior).
//   - localAabb computada con computeBrushAabb tras la generacion.

#include "core/Types.h"
#include "engine/world/csg/Brush.h"

#include <glm/mat4x4.hpp>

namespace Mood::Csg {

/// @brief Brush cilindrico: N planos laterales radiales + 2 caps
///        top/bottom. Eje Y como altura. AABB unitaria centrada
///        en origen antes del transform.
///
///        @param segments  Cantidad de planos laterales. Default
///                          16 (matchea TrenchBroom). Minimo 3
///                          (un prisma triangular degenera ahi).
///                          Mas segments = mas redondo, mas costo
///                          en ops booleanas (O(N^2)).
Brush makeCylinderBrush(const glm::mat4& worldFromLocal,
                         u32 segments = 16,
                         u32 materialIndex = 0);

/// @brief Brush prisma N-gonal: N planos laterales radiales + 2
///        caps. Equivale a `makeCylinderBrush` con segments=N.
///        Existe como alias por claridad semantica (UI: "Prism
///        Triangular" vs "Cylinder con 3 segments").
///
///        @param sides Cantidad de lados (>=3). 3 = triangular,
///                      6 = hexagonal son los presets del editor.
Brush makePrismBrush(const glm::mat4& worldFromLocal,
                      u32 sides,
                      u32 materialIndex = 0);

/// @brief Brush esfera poliedrica: dodecaedro inscripto en una
///        esfera de radio 0.5. 12 caras pentagonales planas — es
///        una **aproximacion convexa** de la esfera, no una
///        esfera real. Mismo enfoque que TrenchBroom para
///        mantener convexidad.
Brush makeSphereBrush(const glm::mat4& worldFromLocal,
                       u32 materialIndex = 0);

/// @brief Brush piramide cuadrada: 4 caras laterales convergentes
///        a la cima `(0, +0.5, 0)` + 1 cap base en `y=-0.5` con
///        vertices `(±0.5, -0.5, ±0.5)`.
Brush makePyramidBrush(const glm::mat4& worldFromLocal,
                        u32 materialIndex = 0);

/// @brief Brush "wedge" (rampa): forma canonica con base
///        cuadrada `[-0.5, +0.5]` en X-Z, altura maxima en
///        `z=-0.5` y altura cero en `z=+0.5`. 5 planos: 2 laterales
///        en `x=±0.5`, 1 base en `y=-0.5`, 1 atras en `z=-0.5`,
///        y 1 plano inclinado conectando la arista superior trasera
///        con la inferior delantera.
///        Util para escaleras, rampas, techos inclinados.
Brush makeWedgeBrush(const glm::mat4& worldFromLocal,
                      u32 materialIndex = 0);

} // namespace Mood::Csg
