#include "asset_manager.hpp"

bool AssetManager::addMaterialPBR(MaterialPBR &mtl, std::string &name, uint32_t &resultIndex)
{
    if(mtlNameToIndex.contains(name))
    {
        resultIndex = mtlNameToIndex[name];
        return false;
    }

    mtls_.push_back(mtl);
    mtlNameToIndex[name] = static_cast<uint32_t>(mtls_.size());
    return true;
}

bool AssetManager::materialExists(std::string &name, uint32_t &resultIndex)
{
    if(mtlNameToIndex.contains(name))
    {
        resultIndex = mtlNameToIndex[name];
        return true; 
    }

    return false;
}