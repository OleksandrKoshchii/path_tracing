#include "geometry/obj_loader.hpp"

#define TINYOBJLOADER_IMPLEMENTATION

#define TINYOBJLOADER_DISABLE_FAST_FLOAT

#include "tiny_obj_loader.h"
#include <iostream>

void loadObjFile(const std::string& filename, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) {
    tinyobj::ObjReaderConfig reader_config;
    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(filename, reader_config)) {
        if (!reader.Error().empty()) {
            std::cerr << "TinyObjReader: " << reader.Error();
        }
        exit(1);
    }

    auto& attrib = reader.GetAttrib();
    auto& shapes = reader.GetShapes();

    for (size_t s = 0; s < shapes.size(); s++) {
        size_t index_offset = 0;
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
            size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);
            for (size_t v = 0; v < fv; v++) {
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
                
                Vertex vertex{};
                vertex.pos.x = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                vertex.pos.y = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                vertex.pos.z = attrib.vertices[3 * size_t(idx.vertex_index) + 2];
                
                vertices.push_back(vertex);
                indices.push_back(static_cast<uint32_t>(indices.size()));
            }
            index_offset += fv;
        }
    }
    std::cout << "OBJ loaded: " << vertices.size() << " vertices." << std::endl;
}
