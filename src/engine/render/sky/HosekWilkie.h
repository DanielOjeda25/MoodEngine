#pragma once

// F3H31: CPU-side helper para el modelo analitico de cielo Hosek-Wilkie
// 2012 ("An Analytic Model for Full Spectral Sky-Dome Radiance",
// SIGGRAPH 2012, L. Hosek + A. Wilkie, Charles University in Prague).
//
// Port reducido de diharaw/sky-models (MIT, 2019). Dataset original
// `hosek_data_rgb.inl` mantenido textual desde el repo upstream con
// banner de copyright intacto. Atribuciones en THIRD_PARTY_LICENSES.md.
//
// Pipeline:
//   1) `computeCoefficients(sunDir, turbidity, groundAlbedo)` evalua
//      el dataset y devuelve 10 coeficientes vec3 (A..I, Z).
//   2) Los coeficientes se suben como uniforms al shader procedural_sky.frag.
//   3) El shader aplica la formula analitica Hosek-Wilkie per-pixel.
//
// El paper no es propietario — la formula es publica. El dataset (270
// floats por canal x 3 canales x 2 albedos x 10 turbidities) viene del
// supplementary material del paper (uso libre con atribucion).

#include <glm/glm.hpp>

namespace Mood::Sky {

// Coeficientes Hosek-Wilkie listos para upload al shader como uniforms.
// 10 vec3 (uno por canal RGB) — el shader los consume directamente.
struct HosekCoefficients {
    glm::vec3 A{0.0f};
    glm::vec3 B{0.0f};
    glm::vec3 C{0.0f};
    glm::vec3 D{0.0f};
    glm::vec3 E{0.0f};
    glm::vec3 F{0.0f};
    glm::vec3 G{0.0f};
    glm::vec3 H{0.0f};
    glm::vec3 I{0.0f};
    // Z = radiance scale per canal (depende de turbidity + albedo +
    // sun elevation). El shader multiplica el resultado de hosek_wilkie()
    // por Z.
    glm::vec3 Z{0.0f};
};

// Computa los coeficientes para una configuracion de cielo dada.
//
// @param sunDirection   Direccion DESDE LA QUE viene la luz solar, en
//                       espacio mundo, normalizada. Y up. Y > 0 = sol
//                       arriba del horizonte. Y <= 0 → cielo nocturno
//                       (radiance ~ 0).
// @param turbidity      Cantidad de aerosoles + humedad. Rango [1, 10].
//                       1.0 = cielo de alta montana / atmosfera clear.
//                       2.5 = clear sky standard (default).
//                       4.0 = haze suave.
//                       8.0 = neblina / smog urbano.
// @param groundAlbedo   Albedo difuso del suelo (afecta el aporte de
//                       luz reflejada al cielo). Rango [0,1] por canal.
//                       0.3 gris medio = default neutro.
// @param normalizedSunY Si > 0, normaliza el brillo del cielo de modo
//                       que el sol tenga luminancia Y = `normalizedSunY`
//                       (default 1.15, valor recomendado por Hosek
//                       para evitar over-exposure en HDR pipelines).
//                       Si 0, deja la radiancia absoluta del modelo.
HosekCoefficients computeCoefficients(const glm::vec3& sunDirection,
                                       float turbidity,
                                       const glm::vec3& groundAlbedo,
                                       float normalizedSunY = 1.15f);

// Helper: calcula la direccion del sol a partir de la hora del dia,
// asumiendo un arco norte-sur simple (sin latitud/longitud reales).
//
//   timeOfDay = 6.0  -> sol en el horizonte este  (elev = 0)
//   timeOfDay = 12.0 -> sol en el zenit           (elev = 90°)
//   timeOfDay = 18.0 -> sol en el horizonte oeste (elev = 0)
//   timeOfDay = 0/24 -> sol bajo el horizonte     (elev = -90°)
//
// La direccion devuelta apunta DESDE el sol HACIA la escena (es decir,
// la direccion de iluminacion, lo que esperarian los shaders).
//
// Convencion: Y up, X = este, Z = sur.
glm::vec3 sunDirectionFromTimeOfDay(float timeOfDay);

} // namespace Mood::Sky
