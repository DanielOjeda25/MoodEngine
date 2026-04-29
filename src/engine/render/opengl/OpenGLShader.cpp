#include "engine/render/opengl/OpenGLShader.h"

#include "core/Log.h"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
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

/// Compila vs+fs y linkea un program nuevo. En cualquier fallo lanza
/// runtime_error con el mensaje de GL. El caller decide que hacer con su
/// program previo (mantenerlo si esto fallo, swapearlo si exitoso).
GLuint buildProgram(const std::string& vertexPath, const std::string& fragmentPath) {
    const std::string vsSrc = readFile(vertexPath);
    const std::string fsSrc = readFile(fragmentPath);

    GLuint vs = compileStage(GL_VERTEX_SHADER, vsSrc, vertexPath);
    GLuint fs = compileStage(GL_FRAGMENT_SHADER, fsSrc, fragmentPath);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint linked = GL_FALSE;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE) {
        GLint len = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> buf(static_cast<size_t>(len) + 1, '\0');
        glGetProgramInfoLog(prog, len, nullptr, buf.data());
        glDeleteProgram(prog);
        glDeleteShader(vs);
        glDeleteShader(fs);
        throw std::runtime_error(std::string("Fallo al linkear program:\n") + buf.data());
    }

    glDetachShader(prog, vs);
    glDetachShader(prog, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

/// Lee mtime con error_code. Si falla devuelve un file_time_type por defecto
/// (epoch). El caller compara contra el ultimo conocido — si difiere y no es
/// epoch, hubo cambio real; si es epoch, el archivo desaparecio temporalmente.
std::filesystem::file_time_type mtimeOf(const std::string& path) {
    std::error_code ec;
    auto t = std::filesystem::last_write_time(path, ec);
    if (ec) return std::filesystem::file_time_type{};
    return t;
}

} // namespace

std::vector<OpenGLShader*> OpenGLShader::s_allShaders;
f32 OpenGLShader::s_throttleAccum = 0.0f;

OpenGLShader::OpenGLShader(const std::string& vertexPath, const std::string& fragmentPath)
    : m_vertexPath(vertexPath)
    , m_fragmentPath(fragmentPath) {
    m_program = buildProgram(vertexPath, fragmentPath);
    m_vertexMtime   = mtimeOf(vertexPath);
    m_fragmentMtime = mtimeOf(fragmentPath);
    s_allShaders.push_back(this);
    Log::render()->info("Shader compilado: {} + {}", vertexPath, fragmentPath);
}

OpenGLShader::~OpenGLShader() {
    auto it = std::find(s_allShaders.begin(), s_allShaders.end(), this);
    if (it != s_allShaders.end()) s_allShaders.erase(it);
    if (m_program != 0) {
        glDeleteProgram(m_program);
    }
}

bool OpenGLShader::tryReloadIfChanged() {
    const auto vNow = mtimeOf(m_vertexPath);
    const auto fNow = mtimeOf(m_fragmentPath);
    // file_time_type{} (epoch) significa que stat fallo (archivo no existe
    // o permiso denegado). Lo tratamos como "sin cambios" hasta que vuelva
    // a aparecer — evita spam de errores si el editor de texto borra y
    // reescribe el archivo en dos pasos.
    const bool stale = (vNow != std::filesystem::file_time_type{} && vNow != m_vertexMtime)
                    || (fNow != std::filesystem::file_time_type{} && fNow != m_fragmentMtime);
    if (!stale) return false;

    try {
        const GLuint newProg = buildProgram(m_vertexPath, m_fragmentPath);
        // Compilo + linkeo OK: tirar el program viejo, swapear, invalidar
        // cache de uniforms (las locations nuevas pueden diferir si el
        // shader cambio el set de uniforms activos).
        if (m_program != 0) glDeleteProgram(m_program);
        m_program = newProg;
        m_uniformCache.clear();
        m_vertexMtime   = vNow;
        m_fragmentMtime = fNow;
        Log::render()->info("Hot-reload OK: {} + {}", m_vertexPath, m_fragmentPath);
    } catch (const std::exception& e) {
        // Mantengo el program previo funcionando. Actualizo mtime para no
        // re-spamear el mismo error en cada tick — el dev debe corregir el
        // shader y guardar de nuevo para reintentar.
        m_vertexMtime   = vNow;
        m_fragmentMtime = fNow;
        Log::render()->warn(
            "Hot-reload fallo en {} + {}: {}\n(se mantiene el shader previo)",
            m_vertexPath, m_fragmentPath, e.what());
    }
    return true;
}

void OpenGLShader::tickHotReload(f32 dt) {
    s_throttleAccum += dt;
    if (s_throttleAccum < 0.5f) return;
    s_throttleAccum = 0.0f;
    for (OpenGLShader* sh : s_allShaders) {
        sh->tryReloadIfChanged();
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

void OpenGLShader::setVec2(std::string_view name, const glm::vec2& value) {
    glUniform2fv(locationOf(name), 1, glm::value_ptr(value));
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
