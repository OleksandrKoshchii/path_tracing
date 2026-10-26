#pragma once

#include <cstdint>
#include <glm/glm.hpp>

struct alignas(128) vkContextSceneObject
{
    glm::mat4 transform;
    uint32_t blasIndex;
};