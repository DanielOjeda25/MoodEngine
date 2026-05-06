#include "engine/render/backend/opengl/OpenGLDebugRenderer.h"

#include "core/Log.h"

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

GLuint compileStage(GLenum stage, const std::string& source, const std::string& origin) {
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

OpenGLDebugRenderer::OpenGLDebugRenderer() {
    // Shader
    const std::string vsSrc = readFile("shaders/debug_line.vert");
    const std::string fsSrc = readFile("shaders/debug_line.frag");
    const GLuint vs = compileStage(GL_VERTEX_SHADER,   vsSrc, "shaders/debug_line.vert");
    const GLuint fs = compileStage(GL_FRAGMENT_SHADER, fsSrc, "shaders/debug_line.frag");

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
        throw std::runtime_error(std::string("Link debug_line fallo:\n") + buf.data());
    }
    glDetachShader(m_program, vs);
    glDetachShader(m_program, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    m_uView       = glGetUniformLocation(m_program, "uView");
    m_uProjection = glGetUniformLocation(m_program, "uProjection");

    // VAO/VBO dinamicos. Se reserva la primera vez que haya datos.
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, pos)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, color)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // F2H17: VAO/VBO separados para triangulos rellenos.
    glGenVertexArrays(1, &m_vaoTris);
    glGenBuffers(1, &m_vboTris);
    glBindVertexArray(m_vaoTris);
    glBindBuffer(GL_ARRAY_BUFFER, m_vboTris);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, pos)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, color)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    Log::render()->info("DebugRenderer inicializado");
}

OpenGLDebugRenderer::~OpenGLDebugRenderer() {
    if (m_vbo != 0) glDeleteBuffers(1, &m_vbo);
    if (m_vao != 0) glDeleteVertexArrays(1, &m_vao);
    if (m_vboTris != 0) glDeleteBuffers(1, &m_vboTris);
    if (m_vaoTris != 0) glDeleteVertexArrays(1, &m_vaoTris);
    if (m_program != 0) glDeleteProgram(m_program);
}

void OpenGLDebugRenderer::drawLine(const glm::vec3& a, const glm::vec3& b,
                                   const glm::vec3& color) {
    // Lineas siempre con alpha 1.0.
    const glm::vec4 c4(color, 1.0f);
    m_cpu.push_back({a, c4});
    m_cpu.push_back({b, c4});
}

void OpenGLDebugRenderer::drawTriangle(const glm::vec3& a,
                                         const glm::vec3& b,
                                         const glm::vec3& c,
                                         const glm::vec4& color) {
    m_cpuTris.push_back({a, color});
    m_cpuTris.push_back({b, color});
    m_cpuTris.push_back({c, color});
}

void OpenGLDebugRenderer::drawAabb(const AABB& box, const glm::vec3& color) {
    // (sin cambios funcionales — drawLine convierte vec3 a vec4 internamente)
    const glm::vec3& n = box.min;
    const glm::vec3& x = box.max;
    // 8 vertices del cubo
    const glm::vec3 v[8] = {
        {n.x, n.y, n.z}, {x.x, n.y, n.z}, {x.x, n.y, x.z}, {n.x, n.y, x.z}, // bottom
        {n.x, x.y, n.z}, {x.x, x.y, n.z}, {x.x, x.y, x.z}, {n.x, x.y, x.z}, // top
    };
    // 12 aristas
    const int edges[12][2] = {
        {0,1}, {1,2}, {2,3}, {3,0}, // bottom face
        {4,5}, {5,6}, {6,7}, {7,4}, // top face
        {0,4}, {1,5}, {2,6}, {3,7}, // verticals
    };
    for (const auto& e : edges) {
        drawLine(v[e[0]], v[e[1]], color);
    }
}

void OpenGLDebugRenderer::flush(const glm::mat4& view, const glm::mat4& projection) {
    if (m_cpu.empty() && m_cpuTris.empty()) return;

    glUseProgram(m_program);
    glUniformMatrix4fv(m_uView,       1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(m_uProjection, 1, GL_FALSE, glm::value_ptr(projection));

    // F2H17: triangulos rellenos primero (con alpha blending), despues
    // las lineas (que llevan el outline encima del fill). Asi el cyan
    // semi-transparente NO oculta el outline cyan brillante.
    if (!m_cpuTris.empty()) {
        glBindVertexArray(m_vaoTris);
        glBindBuffer(GL_ARRAY_BUFFER, m_vboTris);
        const GLsizeiptr trisBytes =
            static_cast<GLsizeiptr>(m_cpuTris.size() * sizeof(Vertex));
        if (trisBytes > m_vboTrisCapacityBytes) {
            m_vboTrisCapacityBytes = trisBytes * 2;
            glBufferData(GL_ARRAY_BUFFER, m_vboTrisCapacityBytes,
                          nullptr, GL_DYNAMIC_DRAW);
        }
        glBufferSubData(GL_ARRAY_BUFFER, 0, trisBytes, m_cpuTris.data());

        // Alpha blending para que la capa cyan se transparente.
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        // Depth test less-equal: el highlight cae encima de la
        // geometria cuando coplanar (face del brush), pero queda
        // ocluido si hay otra cosa adelante.
        GLint prevDepthFunc = GL_LESS;
        glGetIntegerv(GL_DEPTH_FUNC, &prevDepthFunc);
        glDepthFunc(GL_LEQUAL);
        // No escribir al depth buffer (asi los outlines de linea
        // que se dibujan despues no chocan con el fill).
        GLboolean prevDepthMask = GL_TRUE;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &prevDepthMask);
        glDepthMask(GL_FALSE);

        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_cpuTris.size()));

        glDepthMask(prevDepthMask);
        glDepthFunc(prevDepthFunc);
        glDisable(GL_BLEND);
    }

    if (!m_cpu.empty()) {
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

        const GLsizeiptr bytes = static_cast<GLsizeiptr>(m_cpu.size() * sizeof(Vertex));
        if (bytes > m_vboCapacityBytes) {
            m_vboCapacityBytes = bytes * 2;
            glBufferData(GL_ARRAY_BUFFER, m_vboCapacityBytes, nullptr, GL_DYNAMIC_DRAW);
        }
        glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, m_cpu.data());

        // Lineas 1px se pierden contra fondos texturados; pedimos 3px
        // (F2H13: antes 2px, dev reporto outline poco visible).
        GLfloat prevWidth = 1.0f;
        glGetFloatv(GL_LINE_WIDTH, &prevWidth);
        glLineWidth(3.0f);

        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_cpu.size()));

        glLineWidth(prevWidth);
    }

    glBindVertexArray(0);

    m_cpu.clear();
    m_cpuTris.clear();
}

} // namespace Mood
