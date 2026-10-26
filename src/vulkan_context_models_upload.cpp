#include "vulkan_context.hpp"

void VulkanContext::createTLAS()
{
    createInstances();

    if (instances_.empty()) return;

    vk::DeviceSize instancesBufferSize = sizeof(vk::AccelerationStructureInstanceKHR) * instances_.size();

    createBuffer
    (
        instancesBufferSize,
        vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        instancesBuffer_, instancesMemory_
    );

    void* data = instancesMemory_.mapMemory(0, instancesBufferSize, {});
    std::memcpy(data, instances_.data(), instancesBufferSize);
    instancesMemory_.unmapMemory();

    vk::BufferDeviceAddressInfo instancesAddrInfo{ .buffer = *instancesBuffer_ };
    vk::DeviceAddress instancesBufferAddress = logicalDevice_.getBufferAddress(instancesAddrInfo);

    // ==============================================================================
    // DESCRIBING THE STRUCTURE OF GEOMETRY INSTANCES
    // ==============================================================================
    vk::AccelerationStructureGeometryInstancesDataKHR instancesData
    {
        .arrayOfPointers = vk::False,
        .data            = { instancesBufferAddress }
    };

    vk::AccelerationStructureGeometryDataKHR geometryData(instancesData);

    vk::AccelerationStructureGeometryKHR tlasGeometry
    {
        .geometryType = vk::GeometryTypeKHR::eInstances,
        .geometry     = geometryData
    };

    vk::AccelerationStructureBuildGeometryInfoKHR tlasBuildGeometryInfo{
        .type = vk::AccelerationStructureTypeKHR::eTopLevel,
        .mode = vk::BuildAccelerationStructureModeKHR::eBuild,
        .geometryCount = 1,
        .pGeometries = &tlasGeometry
    };

    vk::AccelerationStructureBuildSizesInfoKHR tlasBuildSizes = logicalDevice_.getAccelerationStructureBuildSizesKHR
    (
        vk::AccelerationStructureBuildTypeKHR::eDevice,
        tlasBuildGeometryInfo,
        { static_cast<uint32_t>(instances_.size()) } // obj quantity
    );

    createBuffer
    (
        tlasBuildSizes.accelerationStructureSize,
        vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        tlasBuffer_, tlasMemory_
    );

    vk::raii::Buffer scratchBuffer = nullptr;
    vk::raii::DeviceMemory scratchMemory = nullptr;

    createBuffer
    (
        tlasBuildSizes.buildScratchSize, 
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        scratchBuffer,
        scratchMemory
    );

    vk::BufferDeviceAddressInfo scratchAddrInfo{ .buffer = *scratchBuffer };
    vk::DeviceAddress scratchBufferAddress = logicalDevice_.getBufferAddress(scratchAddrInfo);   

    tlasBuildGeometryInfo.scratchData = scratchBufferAddress;

    vk::AccelerationStructureCreateInfoKHR tlasCreateInfo
    {
        .buffer = *tlasBuffer_,
        .offset = 0,
        .size   = tlasBuildSizes.accelerationStructureSize,
        .type   = vk::AccelerationStructureTypeKHR::eTopLevel // TLAS
    };
    tlasHandle_ = vk::raii::AccelerationStructureKHR(logicalDevice_, tlasCreateInfo);
    tlasBuildGeometryInfo.dstAccelerationStructure = *tlasHandle_;

    vk::AccelerationStructureBuildRangeInfoKHR tlasRangeInfo
    {
        .primitiveCount  = static_cast<uint32_t>(instances_.size()),
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0
    };
    auto cmd = beginSingleTimeCommands();
    std::cout << "debug\n";
    cmd.buildAccelerationStructuresKHR({ tlasBuildGeometryInfo }, { &tlasRangeInfo });
    endSingleTimeCommands(cmd);
    std::cout << "SUCCESS: Global World TLAS compiled successfully on GPU! Scene objects count: " 
              << instances_.size() << std::endl;
}

void VulkanContext::createInstances()
{
    for(auto &sceneObject : sceneObjects_)
    {
        vk::AccelerationStructureDeviceAddressInfoKHR addrInfo{
            .accelerationStructure = *blasHandles_[sceneObject.blasIndex]
        };

        vk::DeviceAddress blasDeviceAddr = logicalDevice_.getAccelerationStructureAddressKHR(addrInfo);

        glm::mat4 rowMajorMatrix = glm::transpose(sceneObject.transform);

        vk::TransformMatrixKHR transform;

        std::memcpy(&transform.matrix, &rowMajorMatrix, sizeof(float) * 12);

        vk::AccelerationStructureInstanceKHR instance
        {
            .transform = transform,
            .instanceCustomIndex = sceneObject.blasIndex,
            .mask = 0xFF,
            .instanceShaderBindingTableRecordOffset = 0,
            .accelerationStructureReference = blasDeviceAddr
        };

        instances_.push_back(instance);
    }
}

void VulkanContext::createSceneObjects()
{
    //TODO : parsing a JSON

    sceneObjects_.emplace_back
    (
        vkContextSceneObject
        {
            glm::mat4(1.0f),
            modelNameToBlasIndex_[std::string("assets/cube.obj")]
        }
    );

    sceneObjects_.emplace_back
    (
        vkContextSceneObject
        {
            glm::mat4(1.0f),
            modelNameToBlasIndex_[std::string("assets/monkey.obj")]
        }
    );

    sceneObjects_.emplace_back
    (
        vkContextSceneObject
        {
            glm::mat4(1.0f),
            modelNameToBlasIndex_[std::string("assets/doom.obj")]
        }
    );

}

void VulkanContext::createBLASes()
{
    if(vertexBuffers_.empty()) return;

    size_t size = vertexBuffers_.size();

    std::vector<vk::raii::Buffer> temporaryScratchBuffers;
    std::vector<vk::raii::DeviceMemory> temporaryScratchMemories;

    temporaryScratchBuffers.reserve(size);
    temporaryScratchMemories.reserve(size);

    auto cmd = beginSingleTimeCommands();

    for(size_t i = 0; i < size; i++)
    {
        createBLAS(vertexBuffers_[i], indexBuffers_[i], meshInfos_[i], cmd, temporaryScratchBuffers, temporaryScratchMemories);
        modelNameToBlasIndex_[meshInfos_[i].name] = static_cast<uint32_t>(i);
    }

    endSingleTimeCommands(cmd);
    std::cout << "created BLASes: " << size << "\n";
}

void VulkanContext::createBLAS(
    vk::raii::Buffer &vertexBuffer, 
    vk::raii::Buffer &indexBuffer, 
    MeshInfo &meshInfo, 
    vk::raii::CommandBuffer &cmd,
    std::vector<vk::raii::Buffer> &tempScratchBuffers,
    std::vector<vk::raii::DeviceMemory> &tempScratchMemories
    )
{
    vk::BufferDeviceAddressInfo bufferAddressInfo
    {
        .buffer = *vertexBuffer
    };

    vk::DeviceAddress bufferAddress = (*logicalDevice_).getBufferAddress(bufferAddressInfo);

    vk::BufferDeviceAddressInfo indexAddressInfo
    {
        .buffer = *indexBuffer
    };

    vk::DeviceAddress indexAddress = (*logicalDevice_).getBufferAddress(indexAddressInfo);

    auto trianglesData = vk::AccelerationStructureGeometryTrianglesDataKHR
    {
        .vertexFormat = vk::Format::eR32G32B32Sfloat,
        .vertexData = { bufferAddress },
        .vertexStride = sizeof(Vertex),
        .maxVertex = meshInfo.maxVertex,
        .indexType = vk::IndexType::eUint32,
        .indexData = { indexAddress }
    };

    vk::AccelerationStructureGeometryDataKHR geometryData(trianglesData);

    vk::AccelerationStructureGeometryKHR blasGeometry
    {
        .geometryType = vk::GeometryTypeKHR::eTriangles,
        .geometry = geometryData,
        .flags = vk::GeometryFlagBitsKHR::eOpaque
    };

    vk::AccelerationStructureBuildGeometryInfoKHR blasBuildGeometryInfo
    {
        .type = vk::AccelerationStructureTypeKHR::eBottomLevel,
        .mode = vk::BuildAccelerationStructureModeKHR::eBuild,
        .geometryCount = 1,
        .pGeometries = &blasGeometry,
    };

    //Query the memory sizes that will be needed for this BLAS
    vk::AccelerationStructureBuildSizesInfoKHR blasBuildSizes = logicalDevice_.getAccelerationStructureBuildSizesKHR
    (
        vk::AccelerationStructureBuildTypeKHR::eDevice,
        blasBuildGeometryInfo,
        { meshInfo.primitiveCount }
    );

    vk::raii::Buffer blasBuffer = nullptr;
    vk::raii::DeviceMemory blasMemory = nullptr;
    vk::raii::Buffer scratchBuffer = nullptr;
    vk::raii::DeviceMemory scratchMemory = nullptr;

    createBuffer
    (
        blasBuildSizes.accelerationStructureSize, 
        vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        blasBuffer,
        blasMemory
    );

    createBuffer
    (
        blasBuildSizes.buildScratchSize, 
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        scratchBuffer,
        scratchMemory
    );

    vk::BufferDeviceAddressInfo scratchBufferAddressInfo
    {
        .buffer = *scratchBuffer
    };
   
    vk::DeviceAddress scratchAddress = (*logicalDevice_).getBufferAddress(scratchBufferAddressInfo);

    blasBuildGeometryInfo.scratchData = scratchAddress;

    vk::AccelerationStructureCreateInfoKHR blasCreateInfo
    {
        .buffer = *blasBuffer,
        .offset = 0,
        .size   = blasBuildSizes.accelerationStructureSize,
        .type   = vk::AccelerationStructureTypeKHR::eBottomLevel // BLAS
    };

    blasBuffers_.push_back(std::move(blasBuffer));
    blasMemories_.push_back(std::move(blasMemory));
    tempScratchBuffers.push_back(std::move(scratchBuffer));
    tempScratchMemories.push_back(std::move(scratchMemory));
    blasHandles_.emplace_back(vk::raii::AccelerationStructureKHR(logicalDevice_, blasCreateInfo));

    blasBuildGeometryInfo.dstAccelerationStructure = *blasHandles_.back();

    vk::AccelerationStructureBuildRangeInfoKHR blasRangeInfo
    {
        .primitiveCount   = meshInfo.primitiveCount, // triangles quantity
        .primitiveOffset  = 0,
        .firstVertex      = 0,
        .transformOffset  = 0
    };

    cmd.buildAccelerationStructuresKHR({ blasBuildGeometryInfo }, { &blasRangeInfo });
}

void VulkanContext::uploadModel(std::string &name, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
{
    vk::DeviceSize vertexSize = sizeof(Vertex) * vertices.size();
    vk::DeviceSize indexSize  = sizeof(uint32_t) * indices.size();
    vk::DeviceSize totalSize  = vertexSize + indexSize;

    //create and fill the staging buffer
    vk::raii::Buffer stagingBuffer = nullptr;
    vk::raii::DeviceMemory stagingBufferMemory = nullptr;
    
    createBuffer(
        totalSize, 
        vk::BufferUsageFlagBits::eTransferSrc, // Source for copying
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, 
        stagingBuffer, stagingBufferMemory
    );

    void* data = stagingBufferMemory.mapMemory(0, totalSize, {});
    std::memcpy(data, vertices.data(), vertexSize); 
    std::memcpy(static_cast<char*>(data) + vertexSize, indices.data(), indexSize); 
    stagingBufferMemory.unmapMemory();

    //Allocation of device local mem for our data
    vk::raii::Buffer vertexBufferTemp = nullptr;
    vk::raii::DeviceMemory vertexBufferMemoryTemp = nullptr;
    createBuffer(
        vertexSize,
        vk::BufferUsageFlagBits::eTransferDst | 
        vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,//vk::BufferUsageFlagBits::eVertexBuffer for rasterization
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        vertexBufferTemp, vertexBufferMemoryTemp
    );

    vk::raii::Buffer indexBufferTemp = nullptr;
    vk::raii::DeviceMemory indexBufferMemoryTemp = nullptr;
    createBuffer(
        indexSize,
        vk::BufferUsageFlagBits::eTransferDst | 
        vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,// vk::BufferUsageFlagBits::eIndexBuffer for rasterization
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        indexBufferTemp, indexBufferMemoryTemp
    );

    vk::raii::CommandBuffer cmd = beginSingleTimeCommands(); 

    // cmd: copy from stagingBuffer ---> to vertexBufferTemp
    vk::BufferCopy vertexCopyRegion{ .srcOffset = 0, .dstOffset = 0, .size = vertexSize };
    cmd.copyBuffer(*stagingBuffer, *vertexBufferTemp, vertexCopyRegion);

    // same logic
    vk::BufferCopy indexCopyRegion{ .srcOffset = vertexSize, .dstOffset = 0, .size = indexSize };
    cmd.copyBuffer(*stagingBuffer, *indexBufferTemp, indexCopyRegion);

    endSingleTimeCommands(cmd); 

    struct MeshInfo meshInfo
    {
        .name = name,
        .vertexCount    = static_cast<uint32_t>(vertices.size()),
        .indexCount     = static_cast<uint32_t>(indices.size()),
        .maxVertex      = static_cast<uint32_t>(vertices.size() - 1),
        .primitiveCount = static_cast<uint32_t>(indices.size() / 3)
    };

    vertexBuffers_.push_back(std::move(vertexBufferTemp));
    vertexDeviceMemories_.push_back(std::move(vertexBufferMemoryTemp));
    indexBuffers_.push_back(std::move(indexBufferTemp));
    indexDeviceMemories_.push_back(std::move(indexBufferMemoryTemp));
    meshInfos_.push_back(meshInfo);

    std::cout << "Data transferred to Device Local VRAM successfully." << std::endl;
}

void VulkanContext::createBuffer(
    vk::DeviceSize size, 
    vk::BufferUsageFlags usage, 
    vk::MemoryPropertyFlags properties, 
    vk::raii::Buffer& buffer, 
    vk::raii::DeviceMemory& bufferMemory)
{
    vk::BufferCreateInfo bufferInfo{
        .size        = size,
        .usage       = usage,
        .sharingMode = vk::SharingMode::eExclusive
    };
    
    if (usage & vk::BufferUsageFlagBits::eShaderDeviceAddress) {
        vk::BufferDeviceAddressInfo bdai{};
    }

    buffer = vk::raii::Buffer(logicalDevice_, bufferInfo);

    vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();

    vk::MemoryAllocateInfo allocInfo{
        .allocationSize  = memRequirements.size,
        .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)
    };

    vk::MemoryAllocateFlagsInfo allocFlagsInfo;
    if (usage & vk::BufferUsageFlagBits::eShaderDeviceAddress) {
        allocFlagsInfo.flags = vk::MemoryAllocateFlagBits::eDeviceAddress;
        allocInfo.pNext = &allocFlagsInfo;
    }

    bufferMemory = vk::raii::DeviceMemory(logicalDevice_, allocInfo);

    buffer.bindMemory(*bufferMemory, 0);
}

uint32_t VulkanContext::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
{
    vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice_.getMemoryProperties();

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}