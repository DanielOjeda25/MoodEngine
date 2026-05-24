#include "engine/project/ProjectSettings.h"

#include <nlohmann/json.hpp>

namespace Mood {

nlohmann::json toJson(const ProjectSettings& s) {
    nlohmann::json j = nlohmann::json::object();

    const ProjectSettings defaults;

    if (s.targetFps != defaults.targetFps) {
        j["target_fps"] = s.targetFps;
    }
    if (!s.description.empty()) {
        j["description"] = s.description;
    }

    return j;
}

ProjectSettings projectSettingsFromJson(const nlohmann::json& j) {
    ProjectSettings s;
    if (!j.is_object()) return s;

    if (j.contains("target_fps") && j.at("target_fps").is_number_integer()) {
        s.targetFps = j.at("target_fps").get<int>();
    }
    if (j.contains("description") && j.at("description").is_string()) {
        s.description = j.at("description").get<std::string>();
    }

    return s;
}

} // namespace Mood
