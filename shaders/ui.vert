#version 450 core

// Hito 20: shader del backend de RmlUi. El layout matchea `Rml::Vertex`
// (pos vec2 + colour Colourb + tex_coord vec2). RmlUi pasa por
// `RenderGeometry` una `translation` vec2 que sumamos al pos antes de
// proyectar. Ortho top-left: (0,0) en la esquina superior izquierda
// del viewport, (W, H) en la inferior derecha.

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;   // u8 normalized RGBA (premultiplicado)
layout(location = 2) in vec2 aUv;

uniform vec2 uTranslate;                // del frame -> render geometry
uniform vec2 uViewportSize;             // pixeles del FB

out vec4 vColor;
out vec2 vUv;

void main() {
    vec2 pos = aPos + uTranslate;
    // Pixel-space (origen top-left) -> NDC.
    vec2 ndc = (pos / uViewportSize) * 2.0 - 1.0;
    ndc.y = -ndc.y;
    gl_Position = vec4(ndc, 0.0, 1.0);
    vColor = aColor;
    vUv = aUv;
}
