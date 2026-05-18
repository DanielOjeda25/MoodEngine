#version 450 core

// F2H64: OIT Weighted Blended composite pass (McGuire & Bavoil 2013).
//
// Lee los 2 attachments del m_oitAccumFb (escritos durante el oitPass)
// y los mezcla sobre el scene color (m_sceneFb loc 0) via blend hardware.
//
//   accumColor (RGBA16F):
//     rgb = sum(color_i * alpha_i * weight_i)
//     a   = sum(alpha_i * weight_i)
//   revealage (R16F):
//     r   = product(1 - alpha_i)  (acumulado multiplicativo desde clear 1.0)
//
// El composite:
//   premultiplied = accum.rgb / max(accum.a, epsilon)
//   FragColor.rgb = premultiplied
//   FragColor.a   = 1 - revealage   -> "cuanto del background queda visible"
//
// El caller bindea blendFunc (GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA), que
// combina: dst_new = dst_old * revealage + src * (1 - revealage)
//        = dst_old * product(1-α) + accum_rgb/accum_a * (1-product(1-α))
// — la transparencia translúcida resuelta sin sort.

in vec2 vUv;

uniform sampler2D uAccum;       // unit 0: accumColor RGBA16F
uniform sampler2D uRevealage;   // unit 1: revealage R16F

layout(location = 0) out vec4 FragColor;
// NOTA: el caller hace glDrawBuffers(1, { GL_COLOR_ATTACHMENT0 }) para
// NO escribir al loc 1 del scene FB (normales de los opacos). Si el FB
// tiene un attachment en loc 1, el driver lo deja intacto.

void main() {
    float revealage = texture(uRevealage, vUv).r;

    // Pixel sin translucents en absoluto: el clear inicial dejo revealage=1
    // y accum=(0,0,0,0). discard preserva exactamente el opaque que ya esta
    // en el scene FB.
    if (revealage >= 1.0) {
        discard;
    }

    vec4 accum = texture(uAccum, vUv);

    // Divide por la alpha acumulada (ponderada) para recuperar el color
    // premultiplied. max() evita division por cero en pixels con peso muy
    // chico (1e-5 es seguro porque clamp del weight min es 1e-2).
    vec3 averageColor = accum.rgb / max(accum.a, 1e-5);

    // Output: src = (color, 1 - revealage). El blendFunc del caller hace
    // el composite correcto sobre el scene FB.
    FragColor = vec4(averageColor, 1.0 - revealage);
}
