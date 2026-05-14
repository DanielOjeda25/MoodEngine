#version 450 core

// SSAO (Screen-Space Ambient Occlusion) -- F2H56 Bloque A. Algoritmo
// estandar de la industria (Crytek 2007 base + reconstruccion de normal
// del depth segun Filament/Godot 4). Portado/adaptado de
// google/filament (Apache 2.0) y godotengine/godot (MIT).
//
// Flow:
//   1. Sample depth en el pixel actual -> reconstruir view-space position
//      con la inversa de la matriz de proyeccion.
//   2. Reconstruir normal del depth via ddx/ddy del view-space position
//      (cheap approximation; menos correcto que un G-buffer dedicado pero
//      cubre 95% de los casos).
//   3. Generar 16 samples en hemisferio orientado por la normal, rotados
//      por un noise por-pixel (interleaved gradient noise de Jorge Jimenez
//      "Next Generation Post Processing in COD AW 2014").
//   4. Para cada sample, proyectar a screen-space, samplear depth, contar
//      como ocluido si esta delante del sample point.
//   5. Output AO factor [0..1] en R: 1 = sin oclusion, 0 = ocluido al max.
//
// El AO factor lo aplica un pass posterior (composite) multiplicando el
// HDR scene color por el AO factor.

in vec2 vUv;

uniform sampler2D uDepthTex;       // depth attachment del scene FB (HDR)
uniform mat4      uProj;           // matriz de proyeccion del frame
uniform mat4      uInvProj;        // inversa de la proyeccion
uniform vec2      uScreenSize;     // pixels (para noise por-pixel)
uniform float     uRadius;         // en view-space units; tipico 0.3-0.8
uniform float     uIntensity;      // multiplicador, 0=sin AO, 2=fuerte
uniform float     uBias;           // self-occlusion bias, tipico 0.025

const int NUM_SAMPLES = 16;

// 16 samples hemisferio en tangent-space (z=up). Generados con
// secuencia de Hammersley. Pesos hacia el centro (sample 0 mas cerca,
// sample 15 mas lejos) escalando por (i/N)^2 para concentrar el muestreo.
const vec3 g_hemisphereSamples[16] = vec3[](
    vec3( 0.04977, -0.04471,  0.04996),
    vec3( 0.01457,  0.08366,  0.02534),
    vec3(-0.04253,  0.03821,  0.10987),
    vec3(-0.13017, -0.06401,  0.09176),
    vec3( 0.11329, -0.11588,  0.20081),
    vec3( 0.23498,  0.01457,  0.04253),
    vec3(-0.04985,  0.27106,  0.16823),
    vec3(-0.27451, -0.09851,  0.21024),
    vec3( 0.08367, -0.34528,  0.21577),
    vec3( 0.36420,  0.09587,  0.30519),
    vec3(-0.21024,  0.40516,  0.11458),
    vec3(-0.41277, -0.32814,  0.35421),
    vec3( 0.36782, -0.42158,  0.45876),
    vec3( 0.55427,  0.18763,  0.41258),
    vec3(-0.27106,  0.61288,  0.47852),
    vec3(-0.59172, -0.51437,  0.55125)
);

out float FragColor;

vec3 reconstructPos(vec2 uv, float depth) {
    // depth viene en [0..1]; NDC en [-1..1].
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = uInvProj * ndc;
    return viewPos.xyz / viewPos.w;
}

// Interleaved Gradient Noise (Jorge Jimenez COD AW 2014). Para rotar
// los samples por-pixel y romper el patron repetitivo de los 16 fixed
// samples. Output [0..1].
float ign(vec2 pixelCoord) {
    return fract(52.9829189 * fract(dot(pixelCoord, vec2(0.06711056, 0.00583715))));
}

void main() {
    float depth = texture(uDepthTex, vUv).r;
    // Si es sky / far plane, sin AO (full white).
    if (depth >= 0.9999) {
        FragColor = 1.0;
        return;
    }

    vec3 viewPos = reconstructPos(vUv, depth);

    // Normal reconstruida del depth via derivadas screen-space.
    // dFdx/dFdy del view-space position dan los tangents en X/Y; el
    // cross product (orden invertido para que apunte hacia la camara
    // en sistema right-handed) es la normal.
    vec3 normal = normalize(cross(dFdx(viewPos), dFdy(viewPos)));

    // Construir base TBN para orientar los samples del hemisferio.
    // El "random vec" del paper original lo reemplazamos por una
    // direccion rotada por IGN noise -- cubre el mismo proposito sin
    // necesitar textura de noise externa.
    float rot = ign(gl_FragCoord.xy) * 6.2831853;
    vec3 randomVec = normalize(vec3(cos(rot), sin(rot), 0.0));
    vec3 tangent   = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN       = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        // Sample en view-space alrededor de viewPos.
        vec3 sampleVec = TBN * g_hemisphereSamples[i];
        vec3 samplePos = viewPos + sampleVec * uRadius;

        // Proyectar el sample a screen-space para leer el depth de
        // la geometria que ocupa esa posicion en pantalla.
        vec4 sampleClip = uProj * vec4(samplePos, 1.0);
        vec2 sampleUv = (sampleClip.xy / sampleClip.w) * 0.5 + 0.5;

        // Si cae fuera de pantalla, ignorar.
        if (sampleUv.x < 0.0 || sampleUv.x > 1.0 ||
            sampleUv.y < 0.0 || sampleUv.y > 1.0) {
            continue;
        }

        float sampledDepth = texture(uDepthTex, sampleUv).r;
        vec3  sampledPos   = reconstructPos(sampleUv, sampledDepth);

        // Range check: si el sample esta MUY lejos (mas alla de uRadius
        // en view-space.z), ignorar. Evita oclusion entre objetos
        // separados.
        float rangeCheck = smoothstep(0.0, 1.0,
            uRadius / abs(viewPos.z - sampledPos.z + 0.0001));

        // Si la profundidad sampled esta DELANTE del sample expected
        // (en view-space, z mas negativo = mas cerca de camara en
        // sistema right-handed con camera mirando -Z), contar oclusion.
        float occluded = (sampledPos.z >= samplePos.z + uBias) ? 1.0 : 0.0;
        occlusion += occluded * rangeCheck;
    }

    float ao = 1.0 - (occlusion / float(NUM_SAMPLES)) * uIntensity;
    FragColor = clamp(ao, 0.0, 1.0);
}
