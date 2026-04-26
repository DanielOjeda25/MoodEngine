#version 450 core

// Hito 11: iluminado Blinn-Phong forward. Acumula:
//   - Ambient global del shader.
//   - Una luz direccional global (uDirectional).
//   - Hasta MAX_POINT_LIGHTS luces puntuales con atenuacion cuadratica
//     basada en `radius`.
//
// Sin sombras, sin PBR, sin HDR (Hitos 16-18).

#define MAX_POINT_LIGHTS 8

in vec3 vColor;
in vec2 vUv;
in vec3 vWorldPos;
in vec3 vWorldNormal;

uniform sampler2D uTexture;

uniform vec3 uCameraPos;

uniform vec3 uAmbient;            // ambient general (ej. 0.15 * skyColor)

struct DirLight {
    vec3 direction;               // hacia donde apunta (norm)
    vec3 color;                   // RGB
    float intensity;              // [0, +inf)
    int  enabled;                 // 0 = off, 1 = on
};
uniform DirLight uDirectional;

struct PointLight {
    vec3 position;
    vec3 color;
    float intensity;
    float radius;                 // distancia de corte para atenuacion
};
uniform PointLight uPointLights[MAX_POINT_LIGHTS];
uniform int uActivePointLights;   // [0, MAX_POINT_LIGHTS]

uniform float uSpecularStrength;  // global (todas las superficies por igual)
uniform float uShininess;         // exponente Blinn-Phong (32 = mate-medio)

// Fog (Hito 15 Bloque 2). `uFogMode`: 0=off, 1=lineal, 2=exp, 3=exp2.
// Misma matematica que `engine/render/Fog.h` (testeable sin GL).
uniform int   uFogMode;
uniform vec3  uFogColor;
uniform float uFogDensity;
uniform float uFogStart;
uniform float uFogEnd;

float computeFogFactor(float distance) {
    if (uFogMode == 0) {
        return 0.0;
    } else if (uFogMode == 1) {
        float denom = uFogEnd - uFogStart;
        if (denom <= 0.0) return 1.0;
        float t = (distance - uFogStart) / denom;
        return clamp(t, 0.0, 1.0);
    } else if (uFogMode == 2) {
        return 1.0 - exp(-uFogDensity * distance);
    } else { // 3 = exp2
        float dd = uFogDensity * distance;
        return 1.0 - exp(-(dd * dd));
    }
}

vec3 evalDirectional(DirLight L, vec3 N, vec3 V, vec3 albedo) {
    if (L.enabled == 0) return vec3(0.0);
    vec3 lightDir = normalize(-L.direction);
    float diff = max(dot(N, lightDir), 0.0);
    vec3 halfway = normalize(lightDir + V);
    float spec = pow(max(dot(N, halfway), 0.0), uShininess) * uSpecularStrength;

    vec3 diffuse  = diff * albedo * L.color * L.intensity;
    vec3 specular = spec * L.color * L.intensity;
    return diffuse + specular;
}

vec3 evalPoint(PointLight L, vec3 N, vec3 V, vec3 albedo, vec3 worldPos) {
    vec3 toLight = L.position - worldPos;
    float dist = length(toLight);
    if (dist > L.radius) return vec3(0.0);

    vec3 lightDir = toLight / max(dist, 1e-4);
    float diff = max(dot(N, lightDir), 0.0);
    vec3 halfway = normalize(lightDir + V);
    float spec = pow(max(dot(N, halfway), 0.0), uShininess) * uSpecularStrength;

    // Atenuacion smooth: 1 en el centro, 0 en el borde del radio. Cuadratica
    // para que no se sienta lineal/falsa.
    float t = 1.0 - clamp(dist / L.radius, 0.0, 1.0);
    float attenuation = t * t;

    vec3 diffuse  = diff * albedo * L.color * L.intensity;
    vec3 specular = spec * L.color * L.intensity;
    return (diffuse + specular) * attenuation;
}

out vec4 FragColor;

void main() {
    vec4 tex = texture(uTexture, vUv);
    vec3 albedo = tex.rgb * vColor;

    vec3 N = normalize(vWorldNormal);
    vec3 V = normalize(uCameraPos - vWorldPos);

    vec3 lighting = uAmbient * albedo;
    lighting += evalDirectional(uDirectional, N, V, albedo);

    int n = min(uActivePointLights, MAX_POINT_LIGHTS);
    for (int i = 0; i < n; ++i) {
        lighting += evalPoint(uPointLights[i], N, V, albedo, vWorldPos);
    }

    // Fog: blend del color iluminado con el color del fog, usando la
    // distancia world-space frag-camara. Aplicado post-iluminacion para
    // que el ambient + lights tambien se desvanezcan.
    float dist = length(uCameraPos - vWorldPos);
    float fogF = computeFogFactor(dist);
    lighting = mix(lighting, uFogColor, fogF);

    FragColor = vec4(lighting, tex.a);
}
