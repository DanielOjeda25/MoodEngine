#version 450 core

// F3H31: fragment shader del sky procedural Hosek-Wilkie 2012.
//
// Origen: port de diharaw/sky-models (MIT, 2019) — atmosphere.glsl + sky_fs.
// Algoritmo: L. Hosek + A. Wilkie, "An Analytic Model for Full Spectral
// Sky-Dome Radiance", SIGGRAPH 2012.
//
// Pipeline:
//   1) CPU computa 10 coefs vec3 (A..I, Z) en HosekWilkie.cpp y los sube
//      como uniforms.
//   2) Este shader evalua la formula analitica per-pixel.
//   3) Output: color RGB linear HDR (no tonemapped — el bake del IBL lo
//      consume directo, o el skybox pass principal aplica exposure mas tarde).
//
// Atribucion: ver THIRD_PARTY_LICENSES.md.

in vec3 vDir;

out vec4 outColor;

// Coeficientes Hosek-Wilkie interpolados CPU-side (1 vec3 por canal RGB).
uniform vec3 uA, uB, uC, uD, uE, uF, uG, uH, uI;
uniform vec3 uZ;  // radiance scale por canal

uniform vec3 uSunDirection;  // normalizada, Y up. Y>0 = sol sobre horizonte.

// Núcleo Hosek-Wilkie: dado el angulo respecto al zenit (theta) y
// respecto al sol (gamma), evalua la formula analitica.
vec3 hosekWilkie(float cosTheta, float gamma, float cosGamma) {
    vec3 chi = (1.0 + cosGamma * cosGamma)
             / pow(1.0 + uH * uH - 2.0 * cosGamma * uH, vec3(1.5));
    return (1.0 + uA * exp(uB / (cosTheta + 0.01)))
         * (uC
          + uD * exp(uE * gamma)
          + uF * (cosGamma * cosGamma)
          + uG * chi
          + uI * sqrt(cosTheta));
}

vec3 hosekWilkieSkyRGB(vec3 dir, vec3 sunDir) {
    // cosTheta: angulo respecto al zenit. Y up → cosTheta = dir.y.
    // Clamp a [0, 1] para evitar valores negativos (bajo horizonte).
    float cosTheta = clamp(dir.y, 0.0, 1.0);

    // cosGamma: angulo respecto al sol.
    float cosGamma = clamp(dot(dir, sunDir), 0.0, 1.0);
    float gamma = acos(cosGamma);

    return uZ * hosekWilkie(cosTheta, gamma, cosGamma);
}

void main() {
    vec3 dir = normalize(vDir);
    vec3 sunDir = normalize(uSunDirection);

    vec3 color = hosekWilkieSkyRGB(dir, sunDir);

    // Bajo el horizonte (dir.y < 0): el modelo HW no esta definido —
    // typically returns valores negativos o explota. Damos un cielo
    // "ground" simple basado en el color del horizonte para evitar
    // artifacts. El IBL bake samplea esto tambien, asi que necesita
    // ser sensato.
    if (dir.y < 0.0) {
        // Color del horizonte (theta=pi/2 → cosTheta=0 → poca dependencia).
        vec3 horizonColor = uZ * hosekWilkieSkyRGB(vec3(1.0, 0.0001, 0.0), sunDir);
        // Fade a un gris suave conforme bajamos (-1 → suelo neutro).
        float t = clamp(-dir.y, 0.0, 1.0);
        color = mix(horizonColor, vec3(0.15, 0.13, 0.10), t * 0.5);
    }

    outColor = vec4(color, 1.0);
}
