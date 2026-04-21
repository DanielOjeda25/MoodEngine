#include "engine/render/opengl/OpenGLRenderer.h"

#include "core/Log.h"

#include <glad/gl.h>

namespace Mood {

void OpenGLRenderer::init() {
    // En Hito 2 no activamos depth testing (dibujamos un triangulo en NDC).
    // Se activara cuando entren las mallas 3D en Hito 3.
    Log::render()->info("OpenGLRenderer listo");
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
    shader.bind();
    mesh.bind();
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mesh.vertexCount()));
    mesh.unbind();
    shader.unbind();
}

} // namespace Mood
