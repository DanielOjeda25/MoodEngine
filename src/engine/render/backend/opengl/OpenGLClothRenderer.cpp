#include "engine/render/backend/opengl/OpenGLClothRenderer.h"

#include "core/Log.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace Mood {

namespace {

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("No se pudo abrir shader: " + path);
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

GLuint compileStage(GLenum stage, const std::string& source,
                    const std::string& origin) {
    GLuint s = glCreateShader(stage);
    const char* src = source.c_str();
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = GL_FALSE;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> buf(static_cast<size_t>(len) + 1, '\0');
        glGetShaderInfoLog(s, len, nullptr, buf.data());
        glDeleteShader(s);
        throw std::runtime_error("Fallo al compilar " + origin + ":\n" + buf.data());
    }
    return s;
}

} // namespace

OpenGLClothRenderer::OpenGLClothRenderer() {
    const std::string vsSrc = readFile("shaders/cloth.vert");
    const std::string fsSrc = readFile("shaders/cloth.frag");
    const GLuint vs = compileStage(GL_VERTEX_SHADER,   vsSrc, "shaders/cloth.vert");
    const GLuint fs = compileStage(GL_FRAGMENT_SHADER, fsSrc, "shaders/cloth.frag");

    m_program = glCreateProgram();
    glAttachShader(m_program, vs);
    glAttachShader(m_program, fs);
    glLinkProgram(m_program);
    GLint linked = GL_FALSE;
    glGetProgramiv(m_program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint len = 0;
        glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> buf(static_cast<size_t>(len) + 1, '\0');
        glGetProgramInfoLog(m_program, len, nullptr, buf.data());
        glDeleteProgram(m_program);
        m_program = 0;
        glDeleteShader(vs);
        glDeleteShader(fs);
        throw std::runtime_error(std::string("Link cloth program fallo:\n") + buf.data());
    }
    glDetachShader(m_program, vs);
    glDetachShader(m_program, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    m_uView           = glGetUniformLocation(m_program, "uView");
    m_uProjection     = glGetUniformLocation(m_program, "uProjection");
    m_uColor          = glGetUniformLocation(m_program, "uColor");
    m_uLightDir       = glGetUniformLocation(m_program, "uLightDir");
    m_uLightColor     = glGetUniformLocation(m_program, "uLightColor");
    m_uLightIntensity = glGetUniformLocation(m_program, "uLightIntensity");
    m_uAmbient        = glGetUniformLocation(m_program, "uAmbient");

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    // Layout: pos vec3 (loc 0) + normal vec3 (loc 1), interleaved 6 floats.
    constexpr GLsizei stride = 6 * sizeof(f32);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(3 * sizeof(f32)));
    glBindVertexArray(0);

    Log::render()->info("ClothRenderer inicializado");
}

OpenGLClothRenderer::~OpenGLClothRenderer() {
    if (m_vbo != 0) glDeleteBuffers(1, &m_vbo);
    if (m_vao != 0) glDeleteVertexArrays(1, &m_vao);
    if (m_program != 0) glDeleteProgram(m_program);
}

void OpenGLClothRenderer::render(Scene& scene,
                                  const glm::mat4& view,
                                  const glm::mat4& projection,
                                  const glm::vec3& lightDir,
                                  const glm::vec3& lightColor,
                                  f32 lightIntensity) {
    // Fill default si no hay sun en la escena: la tela igual debe verse.
    glm::vec3 dir = lightDir;
    glm::vec3 col = lightColor;
    f32 intensity = lightIntensity;
    if (intensity <= 0.0f) {
        dir = glm::vec3(0.0f, -1.0f, 0.0f);  // desde arriba
        col = glm::vec3(1.0f);
        intensity = 0.7f;
    }

    // Estado GL a restaurar: cull face (la tela es doble cara).
    const GLboolean cullWas = glIsEnabled(GL_CULL_FACE);
    if (cullWas) glDisable(GL_CULL_FACE);

    glUseProgram(m_program);
    glUniformMatrix4fv(m_uView,       1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(m_uProjection, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(m_uLightDir,   1, glm::value_ptr(dir));
    glUniform3fv(m_uLightColor, 1, glm::value_ptr(col));
    glUniform1f(m_uLightIntensity, intensity);
    glUniform1f(m_uAmbient, 0.35f);

    glBindVertexArray(m_vao);

    scene.forEach<ClothComponent>([&](Entity, ClothComponent& cl) {
        if (cl.renderVertices.empty()) return;

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        const GLsizeiptr bytes = static_cast<GLsizeiptr>(
            cl.renderVertices.size() * sizeof(f32));
        if (bytes > m_vboCapacityBytes) {
            m_vboCapacityBytes = bytes * 2;
            glBufferData(GL_ARRAY_BUFFER, m_vboCapacityBytes, nullptr,
                         GL_DYNAMIC_DRAW);
        }
        glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, cl.renderVertices.data());

        glUniform3fv(m_uColor, 1, glm::value_ptr(cl.color));

        const GLsizei vertCount =
            static_cast<GLsizei>(cl.renderVertices.size() / 6);
        glDrawArrays(GL_TRIANGLES, 0, vertCount);
    });

    glBindVertexArray(0);
    if (cullWas) glEnable(GL_CULL_FACE);
}

} // namespace Mood
