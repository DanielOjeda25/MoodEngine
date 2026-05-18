#pragma once

// Helpers compartidos entre los splits de tests de SceneSerializer
// (AUDIT-3, split por familia). `NullTexture` no necesita estado real
// (GL); las pruebas son headless.

#include "engine/assets/manager/AssetManager.h"
#include "engine/render/rhi/ITexture.h"

#include <filesystem>
#include <memory>
#include <string>
#include <utility>

namespace Mood::SceneSerializerTests {

class NullTexture : public ITexture {
public:
    explicit NullTexture(std::string p) : m_p(std::move(p)) {}
    void bind(u32 = 0) const override {}
    void unbind() const override {}
    u32 width() const override { return 1; }
    u32 height() const override { return 1; }
    TextureHandle handle() const override { return nullptr; }
    const std::string& path() const { return m_p; }
private:
    std::string m_p;
};

inline AssetManager::TextureFactory nullFactory() {
    return [](const std::string& p) { return std::make_unique<NullTexture>(p); };
}

inline std::filesystem::path tempPath(const char* suffix) {
    return std::filesystem::temp_directory_path() /
        (std::string("moodengine_test_") + suffix);
}

} // namespace Mood::SceneSerializerTests
