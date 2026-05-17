#version 450 core

// SSR (Screen-Space Reflections) -- F2H61 Bloque B.
//
// Algoritmo base: Morgan McGuire & Mike Mara, "Efficient GPU Screen-Space
// Ray Tracing" (2014). Adaptacion para v1: linear DDA en view-space
// (no Hi-Z, no perspective-correct DDA en NDC). Suficiente para reflejos
// de calidad decente sobre charcos / pisos pulidos / agua.
//
// Flow:
//   1. Lee normal RT del G-buffer. Si alpha < 0.5 -> pixel no-PBR ->
//      output baseColor (sin reflejo).
//   2. Reconstruye view-space pos desde depth + uInvProj.
//   3. Reflect del view vector sobre la normal -> ray direction.
//   4. Marcha en view-space N pasos, proyecta cada paso a screen-space,
//      sample depth, compara contra la geometria visible.
//   5. Si hit (gap > 0 && gap < uThickness) -> sample color en esa UV.
//   6. Output = baseColor + reflection * uIntensity (aditivo). Sin hit ->
//      baseColor inalterado (fallback al cubemap IBL ya bakeado en
//      sceneColor por el PBR pass).

in vec2 vUv;

uniform sampler2D uSceneColor;   // HDR del scene FB (post-PBR + IBL spec)
uniform sampler2D uSceneDepth;   // depth attachment del scene FB
uniform sampler2D uSceneNormal;  // view-space normal RT (alpha = PBR flag)
uniform mat4      uProj;         // proyeccion del frame
uniform mat4      uInvProj;      // inversa
uniform float     uIntensity;    // blend factor [0..1]
uniform int       uMaxSteps;     // pasos del ray marching (16-128)
uniform float     uThickness;    // tolerancia para considerar hit (view-space units, 0.01-1.0)
uniform float     uStepSize;     // tamano de cada step (view-space units, ~0.1-0.3)

out vec4 FragColor;

// Reconstruye view-space position desde UV + depth. depth en [0,1] viene
// del depth buffer (NDC z + 0.5). uInvProj convierte NDC -> view.
vec3 reconstructViewPos(vec2 uv, float depth) {
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 view = uInvProj * ndc;
    return view.xyz / view.w;
}

void main() {
    vec3 baseColor = texture(uSceneColor, vUv).rgb;
    vec4 normalSample = texture(uSceneNormal, vUv);

    // Skip pixels no-PBR (skybox/particles/debug -- alpha 0 del clear).
    if (normalSample.a < 0.5) {
        FragColor = vec4(baseColor, 1.0);
        return;
    }

    float depth = texture(uSceneDepth, vUv).r;
    // Far plane: tampoco reflejar.
    if (depth >= 1.0) {
        FragColor = vec4(baseColor, 1.0);
        return;
    }

    vec3 N = normalize(normalSample.xyz);
    vec3 viewPos = reconstructViewPos(vUv, depth);
    // Camara en origen view-space. V apunta de la superficie hacia el eye.
    vec3 V = normalize(-viewPos);
    // Reflect espera incidente, asi que pasamos -V (de eye a superficie).
    vec3 R = reflect(-V, N);

    bool hit = false;
    vec2 hitUv = vec2(0.0);

    // Marcha lineal en view-space. Step size constante; lejano refleja
    // menos preciso pero el algoritmo es estable y barato. Hi-Z queda
    // diferido para v2 si emerge presion de perf.
    for (int i = 1; i <= uMaxSteps; ++i) {
        vec3 currPos = viewPos + R * (float(i) * uStepSize);

        // Proyectar a clip space.
        vec4 clipPos = uProj * vec4(currPos, 1.0);
        if (clipPos.w <= 0.0) break;  // detras de camara
        vec3 ndcPos = clipPos.xyz / clipPos.w;

        // Fuera de pantalla -> abort.
        if (ndcPos.x < -1.0 || ndcPos.x > 1.0 ||
            ndcPos.y < -1.0 || ndcPos.y > 1.0) {
            break;
        }

        vec2 sampleUv = ndcPos.xy * 0.5 + 0.5;
        float sampleDepth = texture(uSceneDepth, sampleUv).r;
        vec3 sampleViewPos = reconstructViewPos(sampleUv, sampleDepth);

        // En view-space GL right-handed: Z negativo hacia adelante. La
        // geometria visible tiene Z > currPos.z si el ray esta delante de
        // ella. gap > 0 -> ray cruzo la superficie visible. gap < thickness
        // -> superficie razonable (no fondo lejano).
        float gap = sampleViewPos.z - currPos.z;
        if (gap > 0.0 && gap < uThickness) {
            hit = true;
            hitUv = sampleUv;
            break;
        }
    }

    if (hit) {
        vec3 reflection = texture(uSceneColor, hitUv).rgb;
        FragColor = vec4(baseColor + reflection * uIntensity, 1.0);
    } else {
        FragColor = vec4(baseColor, 1.0);
    }
}
