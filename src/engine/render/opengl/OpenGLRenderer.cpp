#include "engine/render/opengl/OpenGLRenderer.h"

#include "core/Log.h"

#include <glad/gl.h>

namespace Mood {

void OpenGLRenderer::init() {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // Back-face culling: las mallas opacas del motor tienen winding CCW
    // vistas desde afuera. PrimitiveMeshes::createCubeMesh respeta el orden;
    // mallas futuras con winding incorrecto se veran "hundidas" y el bug es
    // obvio. Ahorra ~50% de fragment shader cuando la escena es opaca.
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    Log::render()->info("OpenGLRenderer listo (depth test + back-face culling)");
}

void OpenGLRenderer::beginFrame(const ClearValues& clear) {
    GLbitfield mask = 0;
    if (clear.clearColor) {
        glClearColor(clear.color.r, clear.color.g, clear.color.b, clear.color.a);
        mask |= GL_COLOR_BUFFER_BIT;
    }
    if (clear.clearDepth) {
        glClearDepth(static_cast<GLclampd>(clear.depth));
        mask |= GL_DEPTH_BUFFER_BIT;
    }
    if (mask != 0) {
        glClear(mask);
    }
}

void OpenGLRenderer::endFrame() {
    // Nada por ahora. Reservado para flush/barrier/estadisticas.
}

void OpenGLRenderer::setViewport(i32 x, i32 y, u32 width, u32 height) {
    glViewport(x, y, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
}

void OpenGLRenderer::drawMesh(const IMesh& mesh, const IShader& shader) {
    // NO desbindeamos shader/mesh al final: romperia el patron "bind once,
    // draw many" usado por el render del grid (un setMat4 entre draws). Si
    // `glUniform*` se llama con program 0 bound, la llamada es no-op silencioso
    // y todos los cubos terminarian dibujandose en la posicion del primero.
    shader.bind();
    mesh.bind();
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mesh.vertexCount()));
}

} // namespace Mood
