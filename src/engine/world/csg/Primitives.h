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

#include <vector>  // F2H30 Bloque C: makePrismBrushFromPolygon recibe vector.

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

/// @brief F2H59: Brush conico: N planos laterales triangulares que
///        convergen al apex `(0, +0.5, 0)` + 1 cap base en `y=-0.5`
///        con radio 0.5. Similar a `makeCylinderBrush` pero con
///        radio top = 0.
///
///        Util para spotlights cosmeticos, tiendas de campana,
///        sombreros, arboles low-poly.
///
///        @param segments Cantidad de caras laterales (>=3).
///                         Default 16 (matchea cylinder).
Brush makeConeBrush(const glm::mat4& worldFromLocal,
                     u32 segments = 16,
                     u32 materialIndex = 0);

/// @brief F2H30 Bloque C: brush prismatico desde polígono arbitrario.
///        Toma una secuencia de N puntos en LOCAL space (sobre un
///        plano perpendicular a `axisIndex`: 0=X, 1=Y, 2=Z) y los
///        extrude `height` unidades en ambos sentidos del eje (mitad
///        para arriba, mitad para abajo) generando N+2 caras (N
///        laterales + cap superior + cap inferior).
///
///        Asume polígono CONVEXO en orden CCW visto desde +axis.
///        Si no es convexo o tiene < 3 puntos, devuelve un Brush
///        vacío (faces.empty()) — el caller debe validar antes.
///
///        Util para el "pincel poligonal" del workspace "Editor de
///        mapas": dev clickea N puntos en una orto + Enter cierra
///        el polígono + esto materializa el brush.
Brush makePrismBrushFromPolygon(const std::vector<glm::vec3>& localPoints,
                                  f32 height,
                                  u32 axisIndex,
                                  u32 materialIndex = 0);

/// @brief F2H30 Bloque C: validacion de convexidad de un polígono
///        2D (proyectado al plano perpendicular a `axisIndex`).
///        Devuelve true si todos los cross products consecutivos
///        tienen el mismo signo (convexo) — solo en orden CCW. Para
///        CW devuelve false (el caller debe invertir el orden).
///
///        Tambien rechaza polígonos con < 3 puntos o con vertices
///        coincidentes consecutivos.
bool isConvexPolygonCCW(const std::vector<glm::vec3>& points,
                         u32 axisIndex);

} // namespace Mood::Csg
