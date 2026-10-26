#include "geometry/obj_loader.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_DISABLE_FAST_FLOAT
#include "tiny_obj_loader.h"

#include <iostream>
#include <unordered_map>

struct VertexKey {
    int v_idx;
    int n_idx;

    bool operator==(const VertexKey& other) const {
        return v_idx == other.v_idx && n_idx == other.n_idx;
    }
};

namespace std {
    template<> struct hash<VertexKey> {
        size_t operator()(const VertexKey& k) const {
            return hash<int>()(k.v_idx) ^ (hash<int>()(k.n_idx) << 1);
        }
    };
}

void loadObjFile(const std::string& filename, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) {
    tinyobj::ObjReaderConfig reader_config;
    reader_config.triangulate = true;
    
    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(filename, reader_config)) {
        if (!reader.Error().empty()) {
            std::cerr << "TinyObjReader: " << reader.Error();
        }
        exit(1);
    }

    auto& attrib = reader.GetAttrib();
    auto& shapes = reader.GetShapes();

    std::unordered_map<VertexKey, uint32_t> uniqueVertices{};

    for (size_t s = 0; s < shapes.size(); s++) {
        size_t index_offset = 0;
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
            size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);
            
            for (size_t v = 0; v < fv; v++) {
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
                
                VertexKey key{idx.vertex_index, idx.normal_index};
                
                auto it = uniqueVertices.find(key);
                if (it == uniqueVertices.end()) {
                    Vertex vertex{};
                    
                    if (idx.vertex_index >= 0) {
                        vertex.pos.x = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                        vertex.pos.y = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                        vertex.pos.z = attrib.vertices[3 * size_t(idx.vertex_index) + 2];
                    }
                    
                    if (idx.normal_index >= 0) {
                        vertex.n.x = attrib.normals[3 * size_t(idx.normal_index) + 0];
                        vertex.n.y = attrib.normals[3 * size_t(idx.normal_index) + 1];
                        vertex.n.z = attrib.normals[3 * size_t(idx.normal_index) + 2];
                    } else {
                        vertex.n = {0.0f, 1.0f, 0.0f};
                    }
                    
                    uint32_t newIndex = static_cast<uint32_t>(vertices.size());
                    uniqueVertices[key] = newIndex;
                    vertices.push_back(vertex);
                    
                    indices.push_back(newIndex);
                } else {
                    indices.push_back(it->second);
                }
            }
            index_offset += fv;
        }
    }
    std::cout << "OBJ loaded: " << vertices.size() << " unique vertices, " 
              << indices.size() << " indices." << std::endl;
}
