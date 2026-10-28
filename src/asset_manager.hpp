#pragma once

#include <vector>
#include <unordered_map>
#include <cstdint>
#include <string>
#include "geometry/material.hpp"

class AssetManager
{
public:
    AssetManager(){}

    bool addMaterialPBR(MaterialPBR &mtl, std::string &name, uint32_t &resultIndex);

    bool materialExists(std::string &name, uint32_t &resultIndex);
private:
    std::vector<MaterialPBR> mtls_;
    std::unordered_map<std::string, uint32_t> mtlNameToIndex;
};