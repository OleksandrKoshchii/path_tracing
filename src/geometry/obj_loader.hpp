#pragma once

#include <string>
#include <vector>
#include "geometry/vertex.hpp"

void loadObjFile(
    const std::string& filename, 
    std::vector<Vertex>& vertices, 
    std::vector<uint32_t>& indices
);
