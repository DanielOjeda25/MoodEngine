#pragma once

// Fixture compartido para tests F2H66: skeleton Mixamo standard.

#include "engine/animation/skeleton/Skeleton.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/vec3.hpp>

#include <string>

namespace Mood::tests {

inline Skeleton makeMixamoSkeleton() {
    Skeleton sk;
    auto add = [&](const std::string& name, int parent, glm::vec3 localOffset) {
        Bone b;
        b.name = "mixamorig:" + name;
        b.parent = parent;
        b.localBindTransform = glm::translate(glm::mat4(1.0f), localOffset);
        b.inverseBind = glm::mat4(1.0f);
        sk.bones.push_back(b);
    };
    add("Hips",         -1, glm::vec3(0.0f, 1.0f, 0.0f));
    add("Spine",         0, glm::vec3(0.0f, 0.15f, 0.0f));
    add("Spine1",        1, glm::vec3(0.0f, 0.15f, 0.0f));
    add("Spine2",        2, glm::vec3(0.0f, 0.15f, 0.0f));
    add("Neck",          3, glm::vec3(0.0f, 0.15f, 0.0f));
    add("Head",          4, glm::vec3(0.0f, 0.15f, 0.0f));
    add("LeftShoulder",  3, glm::vec3(0.1f, 0.0f, 0.0f));
    add("LeftArm",       6, glm::vec3(0.15f, 0.0f, 0.0f));
    add("LeftForeArm",   7, glm::vec3(0.0f, -0.30f, 0.0f));
    add("LeftHand",      8, glm::vec3(0.0f, -0.25f, 0.0f));
    add("RightShoulder", 3, glm::vec3(-0.1f, 0.0f, 0.0f));
    add("RightArm",     10, glm::vec3(-0.15f, 0.0f, 0.0f));
    add("RightForeArm", 11, glm::vec3(0.0f, -0.30f, 0.0f));
    add("RightHand",    12, glm::vec3(0.0f, -0.25f, 0.0f));
    add("LeftUpLeg",     0, glm::vec3(0.08f, -0.05f, 0.0f));
    add("LeftLeg",      14, glm::vec3(0.0f, -0.45f, 0.0f));
    add("LeftFoot",     15, glm::vec3(0.0f, -0.45f, 0.0f));
    add("RightUpLeg",    0, glm::vec3(-0.08f, -0.05f, 0.0f));
    add("RightLeg",     17, glm::vec3(0.0f, -0.45f, 0.0f));
    add("RightFoot",    18, glm::vec3(0.0f, -0.45f, 0.0f));
    return sk;
}

} // namespace Mood::tests
