#version 450 core

// F2H75: fragment shader de telas. Lit simple: una luz direccional + un
// termino ambiente fijo, modulando el color base de la tela. Doble cara:
// las telas se ven de ambos lados, asi que invertimos la normal cuando el
// fragmento es de la cara de atras (`gl_FrontFacing == false`) para que el
// lado opuesto tambien reciba luz coherente en vez de quedar negro.

in vec3 vNormal;

uniform vec3  uColor;          // color base de la tela
uniform vec3  uLightDir;       // direccion HACIA donde apunta la luz (world)
uniform vec3  uLightColor;     // color de la luz direccional
uniform float uLightIntensity; // 0 = sin sun
uniform float uAmbient;        // termino ambiente [0,1]

out vec4 FragColor;

void main() {
    vec3 n = normalize(vNormal);
    if (!gl_FrontFacing) n = -n;

    // Lambert con half-lambert wrap para que el lado en sombra no quede
    // totalmente plano (look suave estilo tela).
    vec3 L = normalize(-uLightDir);
    float ndl = max(dot(n, L), 0.0);
    float wrap = ndl * 0.5 + 0.5;       // half-lambert
    float diffuse = wrap * wrap * uLightIntensity;

    vec3 lit = uColor * (uAmbient + diffuse * uLightColor);
    FragColor = vec4(lit, 1.0);
}
