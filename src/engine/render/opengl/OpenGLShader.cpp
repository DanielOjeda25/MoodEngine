#include "engine/render/opengl/OpenGLShader.h"

#include "core/Log.h"

#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
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

GLuint compileStage(GLenum stage, const std::string& source, const std::string& originPath) {
    GLuint shader = glCreateShader(stage);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_FALSE) {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> buf(static_cast<size_t>(len) + 1, '\0');
        glGetShaderInfoLog(shader, len, nullptr, buf.data());
        glDeleteShader(shader);
        throw std::runtime_error("Fallo al compilar " + originPath + ":\n" + buf.data());
    }
    return shader;
}

} // namespace

OpenGLShader::OpenGLShader(const std::string& vertexPath, const std::string& fragmentPath) {
    const std::string vsSrc = readFile(vertexPath);
    const std::string fsSrc = readFile(fragmentPath);

    GLuint vs = compileStage(GL_VERTEX_SHADER, vsSrc, vertexPath);
    GLuint fs = compileStage(GL_FRAGMENT_SHADER, fsSrc, fragmentPath);

    m_program = glCreateProgram();
    glAttachShader(m_program, vs);
    glAttachShader(m_program, fs);
    glLinkProgram(m_program);

    GLint linked = GL_FALSE;
    glGetProgramiv(m_program, GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE) {
        GLint len = 0;
        glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> buf(static_cast<size_t>(len) + 1, '\0');
        glGetProgramInfoLog(m_program, len, nullptr, buf.data());
        glDeleteProgram(m_program);
        m_program = 0;
        glDeleteShader(vs);
        glDeleteShader(fs);
        throw std::runtime_error(std::string("Fallo al linkear program:\n") + buf.data());
    }

    // Los shaders individuales ya estan referenciados por el program; se pueden
    // eliminar para liberar recursos.
    glDetachShader(m_program, vs);
    glDetachShader(m_program, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    Log::render()->info("Shader compilado: {} + {}", vertexPath, fragmentPath);
}

OpenGLShader::~OpenGLShader() {
    if (m_program != 0) {
        glDeleteProgram(m_program);
    }
}

void OpenGLShader::bind() const {
    glUseProgram(m_program);
}

void OpenGLShader::unbind() const {
    glUseProgram(0);
}

GLint OpenGLShader::locationOf(std::string_view name) {
    // Buscar en cache primero. Si no esta, materializar la clave y consultar GL.
    auto it = m_uniformCache.find(std::string(name));
    if (it != m_uniformCache.end()) {
        return it->second;
    }
    std::string key(name);
    GLint loc = glGetUniformLocation(m_program, key.c_str());
    if (loc < 0) {
        Log::render()->warn("Uniform no encontrado: {}", key);
    }
    m_uniformCache.emplace(std::move(key), loc);
    return loc;
}

void OpenGLShader::setInt(std::string_view name, int value) {
    glUniform1i(locationOf(name), value);
}

void OpenGLShader::setFloat(std::string_view name, float value) {
    glUniform1f(locationOf(name), value);
}

void OpenGLShader::setVec3(std::string_view name, const glm::vec3& value) {
    glUniform3fv(locationOf(name), 1, glm::value_ptr(value));
}

void OpenGLShader::setVec4(std::string_view name, const glm::vec4& value) {
    glUniform4fv(locationOf(name), 1, glm::value_ptr(value));
}

void OpenGLShader::setMat4(std::string_view name, const glm::mat4& value) {
    glUniformMatrix4fv(locationOf(name), 1, GL_FALSE, glm::value_ptr(value));
}

} // namespace Mood
