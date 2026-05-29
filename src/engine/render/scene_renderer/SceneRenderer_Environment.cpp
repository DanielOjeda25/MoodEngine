// F4H1.5 split: extraido de `SceneRenderer.cpp` (que cruzaba el hard cap
// 800 LOC). Aca viven los 2 metodos relacionados al Environment:
//   - loadSkyboxAndIblFromBase: carga skybox + IBL (irradiance/prefilter/BRDF)
//     desde un base path. Idempotente por per-frame check.
//   - applyEnvironmentFromScene: copia params del primer EnvironmentComponent
//     al state del renderer (fog/exposure/tonemap/IBL intensity/SSR/Bloom/
//     SSAO/CSM/ColorGrading).
//
// Sin cambios funcionales — solo split por LOC. Los includes son
// superset de los necesarios para mantener simetria con SceneRenderer.cpp
// (que aporta los mismos includes para sus propios metodos).

#include "engine/render/scene_renderer/SceneRenderer.h"

#include "core/Log.h"
#include "core/Profiler.h"
#include "engine/render/backend/opengl/OpenGLCubemapTexture.h"
#include "engine/render/backend/opengl/OpenGLFramebuffer.h"
#include "engine/render/backend/opengl/OpenGLRenderer.h"
#include "engine/render/backend/opengl/OpenGLShader.h"
#include "engine/render/backend/opengl/OpenGLTexture.h"
#include "engine/render/passes/BloomPass.h"
#include "engine/render/passes/ColorGradingPass.h"
#include "engine/render/passes/PostProcessPass.h"
#include "engine/render/passes/SSAOPass.h"
#include "engine/render/passes/SSRPass.h"
#include "engine/render/passes/ShadowPass.h"
#include "engine/render/passes/SkyboxRenderer.h"
#include "engine/render/sky/HosekWilkie.h"
#include "engine/render/sky/IBLBaker.h"
#include "engine/render/sky/ProceduralSkyRenderer.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <glm/gtc/matrix_inverse.hpp>

#include <glad/gl.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>

namespace Mood {
void SceneRenderer::loadSkyboxAndIblFromBase(const std::string& skyboxBase) {
    if (skyboxBase.empty()) return;
    if (skyboxBase == m_currentSkyboxBase) return; // idempotente per-frame.

    namespace fs = std::filesystem;

    // Resolver fuente: assets/<base>.png -> equirect; assets/<base>/ -> cubemap dir.
    // El sufijo .png puede venir incluido en el path tambien.
    const fs::path baseFs = fs::path("assets") / skyboxBase;
    fs::path equirectPath;
    fs::path cubemapDir;
    if (baseFs.extension() == ".png" && fs::exists(baseFs)) {
        equirectPath = baseFs;
    } else if (fs::exists(fs::path(baseFs.string() + ".png"))) {
        equirectPath = fs::path(baseFs.string() + ".png");
    } else if (fs::is_directory(baseFs) &&
               fs::exists(baseFs / "px.png")) {
        cubemapDir = baseFs;
    } else {
        Log::render()->warn(
            "[skybox] base '{}' no resolvible (ni equirect '<base>.png' ni cubemap dir '<base>/px.png'). Skip swap.",
            skyboxBase);
        return;
    }

    // Skybox renderer (equirect o cubemap dir).
    try {
        if (!equirectPath.empty()) {
            m_skyboxRenderer = std::make_unique<SkyboxRenderer>(
                SkyboxRenderer::Equirect{},
                equirectPath.generic_string());
        } else {
            m_skyboxRenderer = std::make_unique<SkyboxRenderer>(
                cubemapDir.generic_string());
        }
    } catch (const std::exception& e) {
        Log::render()->warn(
            "[skybox] no se pudo cargar '{}': {}. Sky fallback al clear color.",
            skyboxBase, e.what());
        m_skyboxRenderer.reset();
    }

    // IBL: derivar stem (ultimo segmento sin extension). Convencion:
    // assets/ibl/<stem>/irradiance + prefilter ya generados por
    // tools/bake_ibl.py. Si no existen, IBL se desactiva y el shader
    // cae a uAmbient escalar.
    const std::string stem = fs::path(skyboxBase).stem().string();
    const fs::path iblBase = fs::path("assets/ibl") / stem;
    if (!fs::exists(iblBase / "irradiance" / "px.png")) {
        Log::render()->warn(
            "[ibl] no hay bake para stem '{}' en '{}'. IBL desactivado (cae a ambient).",
            stem, iblBase.generic_string());
        m_iblIrradiance.reset();
        m_iblPrefilter.reset();
        m_currentSkyboxBase = skyboxBase;
        return;
    }

    try {
        const std::string base = iblBase.generic_string();
        std::array<std::string, 6> irrPaths{
            base + "/irradiance/px.png", base + "/irradiance/nx.png",
            base + "/irradiance/py.png", base + "/irradiance/ny.png",
            base + "/irradiance/pz.png", base + "/irradiance/nz.png"};
        m_iblIrradiance = std::make_unique<OpenGLCubemapTexture>(irrPaths);

        std::vector<std::array<std::string, 6>> prefilterMips;
        for (int mip = 0; mip < 5; ++mip) {
            const std::string m = base + "/prefilter/mip_" + std::to_string(mip);
            prefilterMips.push_back({
                m + "/px.png", m + "/nx.png",
                m + "/py.png", m + "/ny.png",
                m + "/pz.png", m + "/nz.png"});
        }
        m_iblPrefilter = std::make_unique<OpenGLCubemapTexture>(prefilterMips);

        Log::render()->info(
            "[skybox+ibl] swap a '{}' OK (irradiance + 5 prefilter mips).",
            skyboxBase);
    } catch (const std::exception& e) {
        Log::render()->warn(
            "[ibl] error cargando bake de '{}': {}. IBL desactivado.",
            stem, e.what());
        m_iblIrradiance.reset();
        m_iblPrefilter.reset();
    }

    m_currentSkyboxBase = skyboxBase;
}

void SceneRenderer::applyEnvironmentFromScene(Scene& scene) {
    // Reset a defaults primero. Sin este reset, abrir un proyecto sin
    // Environment hereda los valores del proyecto anterior.
    m_fog          = FogParams{};
    m_exposure     = 0.0f;
    m_tonemap      = TonemapMode{2}; // ACES
    m_iblIntensity = 1.0f;
    // F2H60 polish: defaults a OFF para bloom/SSAO/CSM (pedido del dev).
    m_bloomEnabled   = false;
    m_bloomThreshold = 1.0f;
    m_bloomIntensity = 0.6f;
    m_bloomRadius    = 1.0f;
    m_ssaoEnabled   = false;
    m_ssaoRadius    = 0.5f;
    m_ssaoIntensity = 1.0f;
    m_colorGradingEnabled   = false;
    m_colorGradingLutPath.clear();
    m_colorGradingIntensity = 1.0f;
    // F2H60 polish iter2: CSM ya no tiene "enabled" global. El gate de
    // sombras es per-light (LightComponent::castShadows). Aca solo
    // reseteamos los knobs de calidad.
    m_csmCascadeCount = 4;
    m_csmSplitLambda  = 0.5f;
    // F2H61: SSR defaults a OFF (mismo criterio que bloom/SSAO/CSM).
    m_ssrEnabled   = false;
    m_ssrMaxSteps  = 32u;
    m_ssrThickness = 0.5f;
    m_ssrStepSize  = 0.2f;
    m_ssrIntensity = 0.5f;

    bool envFound = false;
    std::string envSkyboxPath; // F2H86: capturar para swap fuera del lambda.
    // F3H31: capturar params del sky procedural para procesar fuera del
    // forEach (re-render + re-bake del IBL si cambiaron).
    EnvironmentComponent::SkyboxSource envSkyboxSource =
        EnvironmentComponent::SkyboxSource::HDRI;
    f32 envTimeOfDay = 12.0f;
    f32 envTurbidity = 2.5f;
    glm::vec3 envGroundAlbedo{0.3f};
    bool envSkyDirty = false;
    EnvironmentComponent* envPtr = nullptr;
    scene.forEach<EnvironmentComponent>(
        [&](Entity, EnvironmentComponent& env) {
            if (envFound) return; // primer Environment gana
            envFound = true;
            envSkyboxPath     = env.skyboxPath;
            envSkyboxSource   = env.skyboxSource;
            envTimeOfDay      = env.timeOfDay;
            envTurbidity      = env.turbidity;
            envGroundAlbedo   = env.groundAlbedo;
            envSkyDirty       = env.skyDirty;
            envPtr            = &env;
            m_fog.mode        = static_cast<FogMode>(env.fogMode);
            m_fog.color       = env.fogColor;
            m_fog.density     = env.fogDensity;
            m_fog.linearStart = env.fogLinearStart;
            m_fog.linearEnd   = env.fogLinearEnd;
            m_exposure        = env.exposure;
            m_tonemap         = static_cast<TonemapMode>(env.tonemapMode);
            m_iblIntensity    = env.iblIntensity;
            // F2H55: bloom params del componente al cache del renderer.
            m_bloomEnabled    = env.bloomEnabled;
            m_bloomThreshold  = env.bloomThreshold;
            m_bloomIntensity  = env.bloomIntensity;
            m_bloomRadius     = env.bloomRadius;
            // F2H56: SSAO params.
            m_ssaoEnabled     = env.ssaoEnabled;
            m_ssaoRadius      = env.ssaoRadius;
            m_ssaoIntensity   = env.ssaoIntensity;
            // F2H58: Color Grading params.
            m_colorGradingEnabled   = env.colorGradingEnabled;
            m_colorGradingLutPath   = env.colorGradingLutPath;
            m_colorGradingIntensity = env.colorGradingIntensity;
            // F2H60: CSM quality params (sin "enabled" global - gate per-light).
            m_csmCascadeCount = env.csmCascadeCount;
            m_csmSplitLambda  = env.csmSplitLambda;
            // F2H61: SSR params.
            m_ssrEnabled   = env.ssrEnabled;
            m_ssrMaxSteps  = env.ssrMaxSteps;
            m_ssrThickness = env.ssrThickness;
            m_ssrStepSize  = env.ssrStepSize;
            m_ssrIntensity = env.ssrIntensity;
        });

    // F2H86: swap del skybox + IBL si el path del Environment cambio
    // respecto al ultimo cargado. Idempotente: el loader hace early-out
    // si base == m_currentSkyboxBase, asi que llamar cada frame es OK.
    // Si la escena no tiene Environment, el skybox queda como lo dejo
    // el ultimo proyecto (defaults del SceneRenderer en su init).
    // F3H31: solo aplica al modo HDRI legacy. Procedural se procesa abajo.
    if (envFound &&
        envSkyboxSource == EnvironmentComponent::SkyboxSource::HDRI &&
        !envSkyboxPath.empty()) {
        loadSkyboxAndIblFromBase(envSkyboxPath);
    }

    // F3H31: si el Environment esta en modo Procedural, re-render del sky
    // cubemap + re-bake del IBL en GPU si los params cambiaron o si es la
    // primera vez (lazy init de los componentes). El re-bake corre fuera
    // del frame de render (antes del shadow pass) — costo tipico ~50-150ms,
    // pero solo cuando el slider del editor se mueve.
    if (envFound &&
        envSkyboxSource == EnvironmentComponent::SkyboxSource::Procedural) {
        const bool firstTime = (m_proceduralSky == nullptr);
        const bool paramsChanged =
            envSkyDirty ||
            firstTime ||
            std::fabs(envTimeOfDay - m_lastProceduralTimeOfDay) > 1e-4f ||
            std::fabs(envTurbidity - m_lastProceduralTurbidity) > 1e-4f ||
            glm::length(envGroundAlbedo - m_lastProceduralGround) > 1e-4f;

        if (paramsChanged) {
            // Lazy init.
            if (m_proceduralSky == nullptr) {
                m_proceduralSky = std::make_unique<ProceduralSkyRenderer>();
            }
            if (m_iblBaker == nullptr) {
                m_iblBaker = std::make_unique<IBLBaker>();
            }

            // 1) Sun direction del time-of-day.
            const glm::vec3 sunDir = Sky::sunDirectionFromTimeOfDay(envTimeOfDay);

            // 2) Render sky a cubemap procedural.
            const bool skyOk = m_proceduralSky->render(
                sunDir, envTurbidity, envGroundAlbedo);

            if (skyOk) {
                // 3) Bake IBL (irradiance + prefilter) del cubemap procedural.
                const auto baked = m_iblBaker->bake(m_proceduralSky->cubemap());
                if (baked.irradiance != 0) {
                    m_iblIrradiance = std::make_unique<OpenGLCubemapTexture>(
                        OpenGLCubemapTexture::k_adoptHandle, baked.irradiance, 1u);
                }
                if (baked.prefilter != 0) {
                    m_iblPrefilter = std::make_unique<OpenGLCubemapTexture>(
                        OpenGLCubemapTexture::k_adoptHandle, baked.prefilter, 5u);
                }

                // 4) Skybox renderer apunta al cubemap procedural (no-owning).
                m_skyboxRenderer = std::make_unique<SkyboxRenderer>(
                    SkyboxRenderer::ExternalCubemapTag{},
                    m_proceduralSky->cubemap());

                // 5) Cache + clear dirty.
                m_lastProceduralTimeOfDay = envTimeOfDay;
                m_lastProceduralTurbidity = envTurbidity;
                m_lastProceduralGround    = envGroundAlbedo;
                m_currentSkyboxBase = "<procedural>";  // sentinel
                if (envPtr != nullptr) {
                    envPtr->skyDirty = false;
                }

                Log::render()->info(
                    "[sky-procedural] re-bake OK (time={:.1f}h, T={:.1f}, ground=[{:.2f},{:.2f},{:.2f}])",
                    envTimeOfDay, envTurbidity,
                    envGroundAlbedo.r, envGroundAlbedo.g, envGroundAlbedo.b);
            }
        }

        // F3H31 sync: si hay un Directional Light con bindToSky=true,
        // sobrescribimos su direccion con la del sol del time-of-day.
        // El light system lee la direccion en su tick.
        const glm::vec3 sunDir = Sky::sunDirectionFromTimeOfDay(envTimeOfDay);
        scene.forEach<LightComponent>(
            [&](Entity, LightComponent& light) {
                if (light.type == LightComponent::Type::Directional &&
                    light.bindToSky) {
                    // direction es "hacia donde apunta la luz", el sol
                    // arriba significa luz bajando -> direction = -sunDir.
                    light.direction = -sunDir;
                }
            });
    }

    // F2H60 polish iter5: diagnostico. Loguea cuando los valores del
    // Environment cambian frame-a-frame (slider movido) o cuando no
    // hay Environment en la escena (cae a defaults). Esto permite
    // verificar si la edicion en el Inspector llega al renderer.
    if (envFound != m_lastLoggedEnvFound) {
        Log::render()->info(
            "applyEnvironmentFromScene: Environment {} en la escena",
            envFound ? "ENCONTRADO" : "NO encontrado -> usa defaults");
        m_lastLoggedEnvFound = envFound;
    }
    if (envFound &&
        (m_csmCascadeCount != m_lastLoggedCsmCascadeCount ||
         std::fabs(m_csmSplitLambda - m_lastLoggedCsmLambda) > 1e-3f)) {
        Log::render()->info(
            "applyEnvironmentFromScene LEE Environment.CSM: cascadas={}, lambda={:.2f}",
            m_csmCascadeCount, m_csmSplitLambda);
        m_lastLoggedCsmCascadeCount = m_csmCascadeCount;
        m_lastLoggedCsmLambda       = m_csmSplitLambda;
    }
}
} // namespace Mood

