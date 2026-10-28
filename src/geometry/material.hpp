#pragma once

#include <glm/glm.hpp>

struct MaterialPBR 
{
    glm::vec3 albedo;
    float     roughness;
    
    glm::vec3 emissive;
    float     metallic;
    
    float     ior;
    float     transmission;
    uint32_t  albedoTexID;
    uint32_t  normalTexID;
    
    uint32_t  metallicRoughnessTexID;
    uint32_t  transmissionTexID;
    float     clearcoat;
    float     clearcoatRoughness;
};
