#version 450 core

// F3H14: fondo gradient vertical para los thumbnails de meshes. Estilo
// Substance Designer / Marmoset Toolbag — gris medio con leve fade de
// oscuro (abajo) a un poco mas claro (arriba). Da una sensacion de
// "estudio iluminado por arriba" sin competir con el mesh.

in  vec2 v_uv;       // [0,1] x [0,1], origen bottom-left
out vec4 fragColor;

void main() {
    // Mismo gris neutro que F2H80 como midpoint, con +-0.035 entre
    // top y bottom. La curva es lineal — sutil deliberadamente, el
    // gradient no debe llamar la atencion.
    const vec3 bottomColor = vec3(0.125, 0.125, 0.145);
    const vec3 topColor    = vec3(0.195, 0.195, 0.215);
    fragColor = vec4(mix(bottomColor, topColor, v_uv.y), 1.0);
}
