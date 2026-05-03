#include "engine/render/backend/opengl/OpenGLParticleRenderer.h"

#include "core/Log.h"
#include "engine/assets/AssetManager.h"
#include "engine/render/rhi/ITexture.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
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

OpenGLParticleRenderer::OpenGLParticleRenderer() {
    const std::string vsSrc = readFile("shaders/particle.vert");
    const std::string fsSrc = readFile("shaders/particle.frag");
    const GLuint vs = compileStage(GL_VERTEX_SHADER,   vsSrc, "shaders/particle.vert");
    const GLuint fs = compileStage(GL_FRAGMENT_SHADER, fsSrc, "shaders/particle.frag");

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
        throw std::runtime_error(std::string("Link particle program fallo:\n") + buf.data());
    }
    glDetachShader(m_program, vs);
    glDetachShader(m_program, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    m_uView       = glGetUniformLocation(m_program, "uView");
    m_uProjection = glGetUniformLocation(m_program, "uProjection");
    m_uTexture    = glGetUniformLocation(m_program, "uTexture");

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    // Per-instance attributes (divisor=1). Layout matchea `Instance`:
    //   loc 0: vec3 center  (offset 0)
    //   loc 1: float size   (offset 12)
    //   loc 2: vec4 color   (offset 16)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Instance),
                          reinterpret_cast<void*>(offsetof(Instance, center)));
    glVertexAttribDivisor(0, 1);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(Instance),
                          reinterpret_cast<void*>(offsetof(Instance, size)));
    glVertexAttribDivisor(1, 1);

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Instance),
                          reinterpret_cast<void*>(offsetof(Instance, color)));
    glVertexAttribDivisor(2, 1);

    glBindVertexArray(0);

    Log::render()->info("ParticleRenderer inicializado");
}

OpenGLParticleRenderer::~OpenGLParticleRenderer() {
    if (m_vbo != 0) glDeleteBuffers(1, &m_vbo);
    if (m_vao != 0) glDeleteVertexArrays(1, &m_vao);
    if (m_program != 0) glDeleteProgram(m_program);
}

void OpenGLParticleRenderer::render(Scene& scene,
                                     AssetManager& assets,
                                     const glm::mat4& view,
                                     const glm::mat4& projection) {
    // Capturamos estado GL que vamos a tocar para restaurarlo al final.
    GLboolean depthMaskWas = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskWas);
    const GLboolean blendWas = glIsEnabled(GL_BLEND);
    GLint blendSrcWas = GL_ONE, blendDstWas = GL_ZERO;
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcWas);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstWas);

    glUseProgram(m_program);
    glUniformMatrix4fv(m_uView,       1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(m_uProjection, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform1i(m_uTexture, 0);

    // Particulas no escriben depth (sino se ocluyen entre si feo cuando
    // se mezclan). Si lo escribieran, una particula opaca enfrente
    // descartaria a las que estan detras antes de blend.
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);

    glBindVertexArray(m_vao);

    scene.forEach<TransformComponent, ParticleEmitterComponent>(
        [&](Entity, TransformComponent& tf, ParticleEmitterComponent& em) {
        if (em.aliveCount == 0) return;
        if (em.alive.empty())   return; // pool no inicializada

        // Hito 31 F: localSpace => particulas almacenadas en el espacio
        // local del emisor; transformamos al world via worldMatrix antes
        // de upload.
        // Hito 40 I: ahora usamos worldMatrix completa (translation +
        // rotation + scale). Las particulas spawneadas a `(x, 0, z)`
        // local en un emisor rotado 45 deg en Y aparecen rotadas en el
        // world. Scale del entity tambien aplica al spawn pos (no al
        // size del billboard — el size sigue siendo metros directos).
        // Si !localSpace, identidad (positions ya estan en world).
        const glm::mat4 emitterMat = em.localSpace
            ? tf.worldMatrix()
            : glm::mat4(1.0f);

        // Compactar particulas vivas en m_cpu con size/color interpolados.
        m_cpu.clear();
        m_cpu.reserve(em.aliveCount);
        const u32 cap = static_cast<u32>(em.alive.size());
        for (u32 i = 0; i < cap; ++i) {
            if (em.alive[i] == 0) continue;
            const f32 lifetime = (em.lifetimes[i] > 1e-5f) ? em.lifetimes[i] : 1.0f;
            const f32 t = std::clamp(em.ages[i] / lifetime, 0.0f, 1.0f);
            const f32 size = em.sizeStart + t * (em.sizeEnd - em.sizeStart);
            const glm::vec4 color = em.colorStart + t * (em.colorEnd - em.colorStart);
            const glm::vec3 worldCenter = em.localSpace
                ? glm::vec3(emitterMat * glm::vec4(em.positions[i], 1.0f))
                : em.positions[i];
            Instance ins{};
            ins.center[0] = worldCenter.x;
            ins.center[1] = worldCenter.y;
            ins.center[2] = worldCenter.z;
            ins.size      = size;
            ins.color[0]  = color.r;
            ins.color[1]  = color.g;
            ins.color[2]  = color.b;
            ins.color[3]  = color.a;
            m_cpu.push_back(ins);
        }
        if (m_cpu.empty()) return;

        // Hito 31 F + 40 H: sort back-to-front para alpha blend correcto.
        // Skip para additive (commutativo). Hito 40 H upgrade: bucket-Z
        // estable. Las particulas se cuantizan a buckets de 0.1m en
        // view-space; std::stable_sort garantiza que dos particulas en
        // el mismo bucket mantengan su orden relativo entre frames —
        // elimina el flicker visible del Hito 31 F al ver dos puffs
        // de humo a misma profundidad.
        if (!em.additive && m_cpu.size() > 1) {
            constexpr f32 k_bucketSize = 0.1f;
            std::stable_sort(m_cpu.begin(), m_cpu.end(),
                [&view](const Instance& a, const Instance& b) {
                    const f32 za = (view * glm::vec4(
                        a.center[0], a.center[1], a.center[2], 1.0f)).z;
                    const f32 zb = (view * glm::vec4(
                        b.center[0], b.center[1], b.center[2], 1.0f)).z;
                    const i32 ba = static_cast<i32>(std::floor(za / k_bucketSize));
                    const i32 bb = static_cast<i32>(std::floor(zb / k_bucketSize));
                    return ba < bb;
                });
        }

        // Upload con orphan (BufferData null + SubData).
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        const GLsizeiptr bytes =
            static_cast<GLsizeiptr>(m_cpu.size() * sizeof(Instance));
        if (bytes > m_vboCapacityBytes) {
            m_vboCapacityBytes = bytes * 2;
            glBufferData(GL_ARRAY_BUFFER, m_vboCapacityBytes, nullptr,
                         GL_DYNAMIC_DRAW);
        }
        glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, m_cpu.data());

        // Textura del emisor (o missing si no hay). Slot 0.
        ITexture* tex = assets.getTexture(em.texture);
        if (tex == nullptr) tex = assets.getTexture(assets.missingTextureId());
        if (tex != nullptr) tex->bind(0);

        // Blend per-emisor: aditivo para fuego/sparks (clarea sumando),
        // alpha para humo/polvo (transparencia normal).
        if (em.additive) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        } else {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }

        glDrawArraysInstanced(GL_TRIANGLES, 0, 6,
                               static_cast<GLsizei>(m_cpu.size()));
    });

    glBindVertexArray(0);

    // Restaurar estado.
    glBlendFunc(blendSrcWas, blendDstWas);
    if (!blendWas) glDisable(GL_BLEND);
    glDepthMask(depthMaskWas);
}

} // namespace Mood
