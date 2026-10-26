#pragma once

#include <glm/glm.hpp>

struct alignas(32) Vertex
{
    glm::vec3 pos;
    glm::vec3 n;
};
