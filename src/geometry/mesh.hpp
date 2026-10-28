#pragma once

#include <string>
#include <cstdint>

struct MeshPassportGPU
{
    uint64_t vertexAddress;
    uint64_t indexAddress;
};

struct MeshInfo
{
    std::string name;
    uint32_t vertexCount = 0;
    uint32_t indexCount  = 0;
    uint32_t maxVertex   = 0;
    uint32_t primitiveCount = 0;
};