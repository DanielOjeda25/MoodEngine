// F3H31: implementacion del CPU helper Hosek-Wilkie.
// Port reducido de diharaw/sky-models/src/hosek_wilkie_sky_model.cpp (MIT).
//
// El dataset original `hosek_data_rgb.inl` define 6 arrays globales
// (datasetRGB1/2/3 + datasetRGBRad1/2/3) sin `extern`. Para evitar
// symbol clash en linkado si otro TU lo incluyera, lo envolvemos en
// un namespace anonimo TU-local.

#include "engine/render/sky/HosekWilkie.h"

#include <algorithm>
#include <cmath>

namespace {
// Dataset crudo del paper de Hosek-Wilkie 2012 — 270 coefs/canal x 3 canales
// x 2 albedos x 10 turbidities. Envuelto en namespace anonimo para evitar
// linkage externo.
#include "engine/render/sky/hosek_data_rgb.inl"

#ifndef M_PI
constexpr double kPi = 3.14159265358979323846;
#else
constexpr double kPi = M_PI;
#endif

// Evalua un Bernstein quintic en 6 control points del dataset.
//
// Hosek-Wilkie modela cada coeficiente como una spline de orden 5 en la
// dimension "elevation^(1/3)". Los 6 control points vienen consecutivos
// en memoria con `stride` entre ellos (stride = numero de coeficientes
// por elevation point = 9 para los A..I, 1 para Z).
double evaluateSpline(const double* spline, std::size_t stride, double t) {
    const double oneMinus = 1.0 - t;
    return  1.0 * std::pow(oneMinus, 5)                          * spline[0 * stride]
         +  5.0 * std::pow(oneMinus, 4) * std::pow(t, 1.0)        * spline[1 * stride]
         + 10.0 * std::pow(oneMinus, 3) * std::pow(t, 2.0)        * spline[2 * stride]
         + 10.0 * std::pow(oneMinus, 2) * std::pow(t, 3.0)        * spline[3 * stride]
         +  5.0 *           oneMinus    * std::pow(t, 4.0)        * spline[4 * stride]
         +  1.0 *                          std::pow(t, 5.0)        * spline[5 * stride];
}

// Interpola un coeficiente dado (turbidity, albedo, sunElevation).
//
// El dataset esta organizado como:
//   layer 0 (albedo=0): turbidity 1..10 x 6 elevation points x stride
//   layer 1 (albedo=1): turbidity 1..10 x 6 elevation points x stride
//
// Interpolamos elevation con spline, turbidity con linear, albedo con linear.
double evaluate(const double* dataset, std::size_t stride,
                float turbidity, float albedo, float sunTheta) {
    // elevationK = (1 - theta/pi/2)^(1/3) — parametro [0,1] de la spline.
    const double elevationK = std::pow(
        std::max(0.0f, 1.0f - sunTheta / float(kPi * 0.5)),
        1.0 / 3.0);

    // turbidity clamp + index del layer inferior/superior.
    const int t0 = std::clamp(int(turbidity), 1, 10);
    const int t1 = std::min(t0 + 1, 10);
    const float tk = std::clamp(turbidity - float(t0), 0.0f, 1.0f);

    // 2 layers de albedo: [0] = albedo 0, [1] = albedo 1.
    const double* A0 = dataset;
    const double* A1 = dataset + stride * 6 * 10;

    const double a0t0 = evaluateSpline(A0 + stride * 6 * (t0 - 1), stride, elevationK);
    const double a1t0 = evaluateSpline(A1 + stride * 6 * (t0 - 1), stride, elevationK);
    const double a0t1 = evaluateSpline(A0 + stride * 6 * (t1 - 1), stride, elevationK);
    const double a1t1 = evaluateSpline(A1 + stride * 6 * (t1 - 1), stride, elevationK);

    return a0t0 * (1.0 - albedo) * (1.0 - tk)
         + a1t0 *        albedo  * (1.0 - tk)
         + a0t1 * (1.0 - albedo) *        tk
         + a1t1 *        albedo  *        tk;
}

// Evaluacion del modelo HW en una direccion concreta — usado para
// normalizacion opcional de luminancia solar.
glm::vec3 hosekWilkieEval(float cosTheta, float gamma, float cosGamma,
                          const Mood::Sky::HosekCoefficients& k) {
    glm::vec3 chi = (1.0f + cosGamma * cosGamma)
                  / glm::pow(1.0f + k.H * k.H - 2.0f * cosGamma * k.H, glm::vec3(1.5f));
    return (1.0f + k.A * glm::exp(k.B / (cosTheta + 0.01f)))
         * (k.C
          + k.D * glm::exp(k.E * gamma)
          + k.F * (cosGamma * cosGamma)
          + k.G * chi
          + k.I * std::sqrt(std::max(0.0f, cosTheta)));
}

} // namespace

namespace Mood::Sky {

HosekCoefficients computeCoefficients(const glm::vec3& sunDirection,
                                       float turbidity,
                                       const glm::vec3& groundAlbedo,
                                       float normalizedSunY) {
    HosekCoefficients k{};

    // sunTheta = angulo polar del sol desde el zenit. Y=1 → theta=0 (zenit).
    // Si Y<=0 (sol bajo horizonte) clamp a horizonte para evitar NaN en la
    // spline. El cielo nocturno se hace via radiance scale Z → 0 abajo.
    const float sunY = std::clamp(sunDirection.y, 0.0f, 1.0f);
    const float sunTheta = std::acos(sunY);

    // HW soporta solo albedo escalar (no per-canal). Tomamos el promedio
    // del groundAlbedo RGB.
    const float albedo = (groundAlbedo.r + groundAlbedo.g + groundAlbedo.b) / 3.0f;
    const float albedoClamped = std::clamp(albedo, 0.0f, 1.0f);

    // 9 coeficientes (A..I) interpolados por canal + 1 Z radiance scale.
    for (int channel = 0; channel < 3; ++channel) {
        k.A[channel] = float(evaluate(datasetsRGB[channel] + 0, 9, turbidity, albedoClamped, sunTheta));
        k.B[channel] = float(evaluate(datasetsRGB[channel] + 1, 9, turbidity, albedoClamped, sunTheta));
        k.C[channel] = float(evaluate(datasetsRGB[channel] + 2, 9, turbidity, albedoClamped, sunTheta));
        k.D[channel] = float(evaluate(datasetsRGB[channel] + 3, 9, turbidity, albedoClamped, sunTheta));
        k.E[channel] = float(evaluate(datasetsRGB[channel] + 4, 9, turbidity, albedoClamped, sunTheta));
        k.F[channel] = float(evaluate(datasetsRGB[channel] + 5, 9, turbidity, albedoClamped, sunTheta));
        k.G[channel] = float(evaluate(datasetsRGB[channel] + 6, 9, turbidity, albedoClamped, sunTheta));
        // NOTA HW: los indices 7 y 8 estan swapped en el dataset original.
        // Confirmado en diharaw + benanders. Si se intercambia, el cielo
        // sale invertido en la formula chi.
        k.H[channel] = float(evaluate(datasetsRGB[channel] + 8, 9, turbidity, albedoClamped, sunTheta));
        k.I[channel] = float(evaluate(datasetsRGB[channel] + 7, 9, turbidity, albedoClamped, sunTheta));
        k.Z[channel] = float(evaluate(datasetsRGBRad[channel], 1, turbidity, albedoClamped, sunTheta));
    }

    // Normalizacion opcional de luminancia solar — evita que el cielo
    // sea "demasiado brillante" en HDR pipelines. Si normalizedSunY > 0,
    // ajustamos Z de modo que el sol en zenit tenga luminancia Y = sunY.
    if (normalizedSunY > 0.0f && sunY > 0.0f) {
        const glm::vec3 sunRadiance = hosekWilkieEval(sunY, 0.0f, 1.0f, k) * k.Z;
        const float luminance = glm::dot(sunRadiance,
                                          glm::vec3(0.2126f, 0.7152f, 0.0722f));
        if (luminance > 1e-6f) {
            k.Z /= luminance;
            k.Z *= normalizedSunY;
        }
    }

    // Sol bajo el horizonte → cielo nocturno. Aplicamos un fade smooth
    // del radiance scale Z basado en sunDirection.y para evitar pop en
    // el atardecer (HW no modela civil/nautical twilight, lo aproximamos).
    if (sunDirection.y < 0.0f) {
        // Fade lineal en [-0.1, 0.0]: a -0.1 el cielo es 0 (noche),
        // a 0 el cielo esta en su valor pleno.
        const float fade = std::clamp(1.0f + sunDirection.y * 10.0f, 0.0f, 1.0f);
        k.Z *= fade;
    }

    return k;
}

glm::vec3 sunDirectionFromTimeOfDay(float timeOfDay) {
    // Arco N-S simple: el sol sale por el este (X+) a las 6, esta en
    // el zenit (Y+) a las 12, se pone por el oeste (X-) a las 18, y
    // pasa "por abajo" (Y-) durante la noche.
    //
    // Parametrizamos como un angulo phi en [0, 2*pi] donde phi=0 es
    // sol en el este horizonte (timeOfDay=6).
    //
    //   phi = (timeOfDay - 6) * pi / 12
    //
    //   timeOfDay=6   -> phi=0     -> dir = (1, 0, 0)     [este, horizonte]
    //   timeOfDay=12  -> phi=pi/2  -> dir = (0, 1, 0)     [zenit]
    //   timeOfDay=18  -> phi=pi    -> dir = (-1, 0, 0)    [oeste, horizonte]
    //   timeOfDay=0   -> phi=-pi/2 -> dir = (0, -1, 0)    [nadir, medianoche]
    const float phi = (timeOfDay - 6.0f) * float(kPi) / 12.0f;
    return glm::vec3(std::cos(phi), std::sin(phi), 0.0f);
}

} // namespace Mood::Sky
