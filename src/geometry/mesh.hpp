#pragma once

#include <string>
#include <cstdint>

struct MeshInfo
{
    std::string name;
    uint32_t vertexCount = 0;
    uint32_t indexCount  = 0;
    uint32_t maxVertex   = 0;
    uint32_t primitiveCount = 0;
};