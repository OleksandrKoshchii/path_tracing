#include "cgltf.h"
#include "vulkan_context.hpp"
#include <iostream>

void VulkanContext::uploadAllModels(std::vector<std::string> &filenames)
{
    for(std::string & filename : filenames)
    {
        uploadModels(filename);
    }

    vk::raii::Buffer stagingBuffer = nullptr;
    vk::raii::DeviceMemory stagingBufferMemory = nullptr;

    size_t size = sizeof(MeshPassportGPU) * meshPassports_.size();

    createBuffer
    (
        size,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        stagingBuffer,
        stagingBufferMemory
    );

    createBuffer
    (
        size,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        meshPassportsBuffer_,
        meshPassportsMemory_
    );

    void* data = stagingBufferMemory.mapMemory(0, size, {});
    std::memcpy(data, meshPassports_.data(), size); 
    stagingBufferMemory.unmapMemory();

    vk::raii::CommandBuffer cmd = beginSingleTimeCommands(); 

    vk::BufferCopy copyRegion{ 
        .srcOffset = 0, 
        .dstOffset = 0, 
        .size      = size 
    };
    cmd.copyBuffer(*stagingBuffer, *meshPassportsBuffer_, copyRegion);

    endSingleTimeCommands(cmd); 
    std::cout << "[Vulkan VRAM] Mesh passports successfully pushed to DeviceLocal storage." << std::endl;
}

void VulkanContext::uploadModels(std::string& filename)
{
    std::cout << "[cgltf] Parsing multi-mesh file: " << filename << std::endl;

    cgltf_options options{};
    cgltf_data* data = nullptr;
    cgltf_result result = cgltf_parse_file(&options, filename.c_str(), &data);
    
    if (result != cgltf_result_success) {
        throw std::runtime_error("failed to parse glTF file: " + filename);
    }

    result = cgltf_load_buffers(&options, data, filename.c_str());
    if (result != cgltf_result_success) {
        cgltf_free(data);
        throw std::runtime_error("failed to load glTF buffers for: " + filename);
    }

    // 2. ГЛАВНЫЙ ЦИКЛ: ОБХОДИМ КАЖДЫЙ МЕШ В ФАЙЛЕ ИЗОЛИРОВАННО
    for (cgltf_size m = 0; m < data->meshes_count; ++m)
    {
        cgltf_mesh& mesh = data->meshes[m];
        
        std::string meshName = mesh.name ? mesh.name : "mesh_" + std::to_string(m);
        
        std::vector<Vertex>   meshVertices;
        std::vector<uint32_t> meshIndices;
        
        std::vector<uint32_t> triangleMaterialIDs;

        std::cout << "\n--------------------------------------------------" << std::endl;
        std::cout << "👉 [Processing Discrete Mesh] Name: '" << meshName << "'" << std::endl;
        std::cout << "--------------------------------------------------" << std::endl;

        for (cgltf_size p = 0; p < mesh.primitives_count; ++p)
        {
            cgltf_primitive& primitive = mesh.primitives[p];
            
            // ─── ЭТАП А: ОБРАБОТКА МАТЕРИАЛА САБМЕША ───
            uint32_t currentSubmeshMaterialID = 0; // 0 - ID дефолтного материала

            if (primitive.material) 
            {
                std::string matName = primitive.material->name ? primitive.material->name 
                                      : meshName + "_material_" + std::to_string(p);
                
                uint32_t result;                

                if(assetManager_.materialExists(matName, result))
                {
                    currentSubmeshMaterialID = result;
                    std::cout << "  [Existing Material Reused] Name: '" << matName << "' -> ID: " << currentSubmeshMaterialID << std::endl;
                }
                else
                {
                    MaterialPBR newMaterial{};
                    if (primitive.material->has_pbr_metallic_roughness) {
                        auto& pbr = primitive.material->pbr_metallic_roughness;
                        newMaterial.albedo = glm::vec3(pbr.base_color_factor[0], pbr.base_color_factor[1], pbr.base_color_factor[2]);
                        newMaterial.roughness = pbr.roughness_factor;
                        newMaterial.metallic  = pbr.metallic_factor;
                    }
                    
                    // Вы всегда можете здесь перехватить логику и донастроить кастомные параметры:
                    // if (matName == "MyEmissiveGlass") { newMaterial.emissive = glm::vec3(10.0f); }

                    uint32_t resultMtlIndex;

                    assetManager_.addMaterialPBR(newMaterial, matName, resultMtlIndex);
                    
                    std::cout << "  [New Material Created] Name: '" << matName << "' -> Registered ID: " << resultMtlIndex << std::endl;
                }
            }

            // Нам нужно знать локальный vertexOffset внутри ЭТОГО меша для сшивки сабмешей
            uint32_t submeshVertexOffset = static_cast<uint32_t>(meshVertices.size());

            // ─── ЭТАП Б: СБОРКА ВЕРШИН САБМЕША ───
            cgltf_accessor* posAccessor = nullptr;
            cgltf_accessor* normAccessor = nullptr;

            for (cgltf_size a = 0; a < primitive.attributes_count; ++a) {
                if (primitive.attributes[a].type == cgltf_attribute_type_position) posAccessor = primitive.attributes[a].data;
                if (primitive.attributes[a].type == cgltf_attribute_type_normal) normAccessor = primitive.attributes[a].data;
            }

            if (!posAccessor) continue;

            size_t vertexCount = posAccessor->count;
            for (size_t i = 0; i < vertexCount; ++i)
            {
                Vertex vertex{};
                float pos[3];
                cgltf_accessor_read_float(posAccessor, i, pos, 3);
                vertex.pos = glm::vec3(pos[0], pos[1], pos[2]);

                if (normAccessor) {
                    float norm[3];
                    cgltf_accessor_read_float(normAccessor, i, norm, 3);
                    vertex.n = glm::vec3(norm[0], norm[1], norm[2]);
                } else {
                    vertex.n = glm::vec3(0.0f, 1.0f, 0.0f);
                }
                meshVertices.push_back(vertex);
            }

            // ─── ЭТАП В: СБОРКА ИНДЕКСОВ И ПРИВЯЗКА К МАТЕРИАЛАМ ───
            if (primitive.indices)
            {
                size_t indexCount = primitive.indices->count;
                for (size_t i = 0; i < indexCount; ++i)
                {
                    uint32_t rawIndex = static_cast<uint32_t>(cgltf_accessor_read_index(primitive.indices, i));
                    // Смещаем индекс строго в рамках текущих векторов этого отдельного меша
                    meshIndices.push_back(rawIndex + submeshVertexOffset);
                }

                // Вшиваем ID материала в параллельную ленту треугольников текущего меша
                size_t triangleCount = indexCount / 3;
                for (size_t t = 0; t < triangleCount; ++t)
                {
                    triangleMaterialIDs.push_back(currentSubmeshMaterialID);
                }
            }
        }

        std::cout << "  [Result] Mesh '" << meshName << "' compilation details:" << std::endl;
        std::cout << "    Vertices count        : " << meshVertices.size() << std::endl;
        std::cout << "    Indices count         : " << meshIndices.size() << std::endl;
        std::cout << "    Triangle Material IDs : " << triangleMaterialIDs.size() << " entries." << std::endl;

        allocateModelBuffers(meshName, meshVertices, meshIndices);
    }

    // 3. СТИРАЕМ ДАННЫЕ ПАРСЕРА
    cgltf_free(data);
    std::cout << "\n[cgltf] Parsing file " << filename << " completed successfully!" << std::endl;
}
