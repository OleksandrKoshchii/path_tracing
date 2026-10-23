#include "vulkan_context.hpp"

VulkanContext::VulkanContext(GLFWwindow* window) : window_(window)
{   
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createStorageImageViews();
    createGraphicsPipeline();
    createComputePipeline();
    createDescriptorSets();
    createCommandPool();
    createCommandBuffers();
    createSyncObjects();
}

void VulkanContext::uploadModel(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
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

    // Открываем шлюз: отображаем физический адрес GPU в обычный указатель C++
    void* data = stagingBufferMemory.mapMemory(0, totalSize, {});
    // Копируем вершины из ОЗУ в буфер
    std::memcpy(data, vertices.data(), vertexSize); 
    // Копируем индексы из ОЗУ, укладывая их в памяти строго ВСЛЕД за вершинами
    std::memcpy(static_cast<char*>(data) + vertexSize, indices.data(), indexSize); 
    // Закрываем шлюз
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

    vertexBuffers_.push_back(std::move(vertexBufferTemp));
    vertexDeviceMemories_.push_back(std::move(vertexBufferMemoryTemp));
    indexBuffers_.push_back(std::move(indexBufferTemp));
    indexDeviceMemories_.push_back(std::move(indexBufferMemoryTemp));
    
    std::cout << "Data transferred to Device Local VRAM successfully." << std::endl;
}

vk::raii::CommandBuffer VulkanContext::beginSingleTimeCommands()
{
    vk::CommandBufferAllocateInfo allocInfo{
        .commandPool        = *commandPool_,
        .level              = vk::CommandBufferLevel::ePrimary, // Главный буфер
        .commandBufferCount = 1
    };

    vk::raii::CommandBuffers commandBuffers(logicalDevice_, allocInfo);
    vk::raii::CommandBuffer commandBuffer = std::move(commandBuffers[0]);

    vk::CommandBufferBeginInfo beginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
    };
    commandBuffer.begin(beginInfo);

    return commandBuffer;
}

void VulkanContext::endSingleTimeCommands(vk::raii::CommandBuffer& commandBuffer)
{
    commandBuffer.end();

    vk::SubmitInfo submitInfo{
        .commandBufferCount = 1,
        .pCommandBuffers    = &*commandBuffer
    };

    graphicsAndComputeQueue_.submit(submitInfo, nullptr);
    graphicsAndComputeQueue_.waitIdle(); 
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
        vk::BufferDeviceAddressInfo bdai{}; // Для Vulkan 1.2+
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

void VulkanContext::cleanup()
{
    logicalDevice_.waitIdle();
}

void VulkanContext::createInstance() 
{
    constexpr vk::ApplicationInfo appInfo
    {
        .pApplicationName   = "Path Tracer",
        .applicationVersion = VK_MAKE_VERSION( 1, 0, 0 ),
        .pEngineName        = "No Engine",
        .engineVersion      = VK_MAKE_VERSION( 1, 0, 0 ),
        .apiVersion         = vk::ApiVersion14              //Vulkan 1.4
    };

    // Get the required layers
    std::vector<char const*> requiredLayers;
    if (enableValidationLayers)
    {
        requiredLayers.assign(validationLayers.begin(), validationLayers.end());
    }

    // Check if the required layers are supported by the Vulkan implementation.
    auto layerProperties = context_.enumerateInstanceLayerProperties();

    /*auto unsupportedLayerIt = std::ranges::find_if(requiredLayers,
	                                               [&layerProperties](auto const &requiredLayer) 
                                                   {
		                                               return std::ranges::none_of(layerProperties,
                                                            [requiredLayer](auto const &layerProperty) 
                                                            { 
                                                                return strcmp(layerProperty.layerName, requiredLayer) == 0; 
                                                            });
	                                               });
    if (unsupportedLayerIt != requiredLayers.end())
    {
        throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));
    }*/

    auto unsupportedLayers = requiredLayers | std::views::filter
    ([&layerProperties](const char* requiredLayer)
        {
		    return std::ranges::none_of(layerProperties,
                [requiredLayer](auto const &layerProperty) 
                { 
                    //return strcmp(layerProperty.layerName, requiredLayer) == 0; 
                    return (std::string_view(layerProperty.layerName) == requiredLayer);
                });
        }
    );

    if(!std::ranges::empty(unsupportedLayers))
    {
        std::string errorText = "Required layers are not supported:\n";
        for(auto const& unsupportedLayer : unsupportedLayers)
        {
            errorText += std::string(unsupportedLayer) + "\n";
        }

        throw std::runtime_error(errorText);
    }

    auto requiredExtensions = getRequiredInstanceExtensions();

    auto extensionProperties = context_.enumerateInstanceExtensionProperties();

    auto unsupportedExtensions = requiredExtensions | std::views::filter([&extensionProperties](auto requiredExtension)
        {
            return std::ranges::none_of(extensionProperties,
            [requiredExtension](auto const& extensionProperty)
                {
                    return (std::string_view(extensionProperty.extensionName) == requiredExtension);
                }
            );
        }
    );

    if(!std::ranges::empty(unsupportedExtensions))
    {
        std::string errorText = "Required extensions are not supported:\n";
        for(auto const& unsupportedExtension : unsupportedExtensions)
        {
            errorText += std::string(unsupportedExtension) + "\n";
        }

        throw std::runtime_error(errorText);
    }

    vk::InstanceCreateInfo createInfo
    {
        .pApplicationInfo        = &appInfo,
        .enabledLayerCount       = static_cast<uint32_t>(requiredLayers.size()),
        .ppEnabledLayerNames     = requiredLayers.data(),
        .enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size()),
        .ppEnabledExtensionNames = requiredExtensions.data()
    };

    instance_ = vk::raii::Instance(context_, createInfo);
}

std::vector<const char*> VulkanContext::getRequiredInstanceExtensions()
{

    const char* errDescription = nullptr;
    if (glfwGetError(&errDescription) == GLFW_NOT_INITIALIZED) 
    {
        throw std::runtime_error("VulkanContext called before glfwInit()!");
    }

    if(!glfwVulkanSupported()) return {};

    uint32_t glfwExtensionCount = 0;
    auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    if(glfwExtensions == nullptr || glfwExtensionCount == 0) return {};

    auto extensions = std::vector<const char*>(glfwExtensions, glfwExtensions + glfwExtensionCount);
    
    if (enableValidationLayers)
    {
        extensions.push_back(vk::EXTDebugUtilsExtensionName);
    }
    return extensions;
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL VulkanContext::debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT       severity,
                                                  vk::DebugUtilsMessageTypeFlagsEXT              type,
                                                  const vk::DebugUtilsMessengerCallbackDataEXT * pCallbackData,
                                                  void *                                         pUserData)
{
    if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning ||
        severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) 
    {
        std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;
    }

    return vk::False;
}   

void VulkanContext::setupDebugMessenger()
{
    if (!enableValidationLayers) return;
    
    vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                                    vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);

    vk::DebugUtilsMessageTypeFlagsEXT     messageTypeFlags(
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
    
    vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{.messageSeverity = severityFlags,
                                                                  .messageType     = messageTypeFlags,
                                                                          .pfnUserCallback = &debugCallback};

    debugMessenger_ = instance_.createDebugUtilsMessengerEXT( debugUtilsMessengerCreateInfoEXT );
}

void VulkanContext::createSurface()
{
    VkSurfaceKHR       _surface;
    if (glfwCreateWindowSurface(*instance_, window_, nullptr, &_surface) != 0) {
        throw std::runtime_error("failed to create window surface!");
    }
    surface_ = vk::raii::SurfaceKHR(instance_, _surface);
}

bool VulkanContext::isDeviceSuitable( vk::raii::PhysicalDevice const & physicalDevice )
{
    bool supportsVulkan1_3 = physicalDevice.getProperties().apiVersion >= VK_API_VERSION_1_3;

    auto queueFamilies = physicalDevice.getQueueFamilyProperties();
    uint32_t qfpIndex = 0;
    bool supportsGraphicsAndPresent = std::ranges::any_of(queueFamilies,
        [this, &physicalDevice, &qfpIndex](auto const &qfp) {
            bool const suitable = (qfp.queueFlags & vk::QueueFlagBits::eGraphics) && 
                                  physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface_);
            qfpIndex++;
            return suitable;
        }
    );

    auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
    bool supportsAllRequiredExtensions = std::ranges::all_of(requiredDeviceExtensions_,
        [&availableDeviceExtensions](auto const &requiredExtension) 
        {
            return std::ranges::any_of(availableDeviceExtensions,
                [requiredExtension](auto const &availableDeviceExtension) 
                { 
                   return std::string_view(availableDeviceExtension.extensionName) == requiredExtension; 
                });
        });

    auto featureChain = physicalDevice.template getFeatures2<
        vk::PhysicalDeviceFeatures2,
        vk::PhysicalDeviceVulkan11Features,
        vk::PhysicalDeviceVulkan13Features,
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT,
        vk::PhysicalDeviceBufferDeviceAddressFeatures,
        vk::PhysicalDeviceAccelerationStructureFeaturesKHR,
        vk::PhysicalDeviceRayQueryFeaturesKHR 
    >();


    bool supportsRequiredFeatures = featureChain.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
                                  featureChain.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
                                  featureChain.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState &&
                                  featureChain.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2;

    auto const& addressFeatures  = featureChain.template get<vk::PhysicalDeviceBufferDeviceAddressFeatures>();
    auto const& asFeatures       = featureChain.template get<vk::PhysicalDeviceAccelerationStructureFeaturesKHR>();
    auto const& rayQueryFeatures = featureChain.template get<vk::PhysicalDeviceRayQueryFeaturesKHR>();

    bool supportsRayTracing = addressFeatures.bufferDeviceAddress &&
                              asFeatures.accelerationStructure &&
                              rayQueryFeatures.rayQuery;


    return supportsVulkan1_3 && 
           supportsGraphicsAndPresent && 
           supportsAllRequiredExtensions && 
           supportsRequiredFeatures && 
           supportsRayTracing;
}

void VulkanContext::pickPhysicalDevice()
{
    std::vector<vk::raii::PhysicalDevice> physicalDevices = instance_.enumeratePhysicalDevices();
    auto const devIter = std::ranges::find_if( physicalDevices, [&]( auto const & physicalDevice ) { return isDeviceSuitable( physicalDevice ); } );
    if ( devIter == physicalDevices.end() )
    {
        throw std::runtime_error( "failed to find a suitable GPU!" );
    }
    physicalDevice_ = *devIter;
}

void VulkanContext::createLogicalDevice() 
{
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice_.getQueueFamilyProperties();

    for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
    {
        if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) && (queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eCompute) &&
            physicalDevice_.getSurfaceSupportKHR(qfpIndex, *surface_))
        {
            // found a queue family that supports both graphics and present
            queueIndex_ = qfpIndex;
            break;
        }
    }
        
    if (queueIndex_ == ~0)
    {
        throw std::runtime_error("Could not find a queue for graphics and present -> terminating");    
    }

    float queuePriority = 0.5f;
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo
    {
        .queueFamilyIndex = queueIndex_, 
        .queueCount = 1, 
        .pQueuePriorities = &queuePriority
    };

    // Create a chain of feature structures
    vk::StructureChain<
        vk::PhysicalDeviceFeatures2,
        vk::PhysicalDeviceVulkan11Features,
        vk::PhysicalDeviceVulkan13Features,
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT,
        vk::PhysicalDeviceBufferDeviceAddressFeatures,
        vk::PhysicalDeviceAccelerationStructureFeaturesKHR,
        vk::PhysicalDeviceRayQueryFeaturesKHR   
    >featureChain = 
    {
        {},                                    // vk::PhysicalDeviceFeatures2 (empty for now)
        {.shaderDrawParameters = true},        // Enable shader draw parameters from Vulkan 1.1
        {.synchronization2 = true, .dynamicRendering = true},            // Enable dynamic rendering from Vulkan 1.3
        {.extendedDynamicState = true},         // Enable extended dynamic state from the extension
        {.bufferDeviceAddress = true},
        {.accelerationStructure = true},
        {.rayQuery = true}    
    };

    vk::DeviceCreateInfo deviceCreateInfo
    {
        .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &deviceQueueCreateInfo,
        .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions_.size()),
        .ppEnabledExtensionNames = requiredDeviceExtensions_.data()
    };

    logicalDevice_ = vk::raii::Device(physicalDevice_, deviceCreateInfo);
    graphicsAndComputeQueue_ = vk::raii::Queue(logicalDevice_, queueIndex_, 0);
}

void VulkanContext::createSwapChain()
{
    auto surfaceCapabilities = physicalDevice_.getSurfaceCapabilitiesKHR( *surface_ );

    std::vector<vk::SurfaceFormatKHR> availableFormats = physicalDevice_.getSurfaceFormatsKHR( *surface_ );

    std::vector<vk::PresentModeKHR> availablePresentModes = physicalDevice_.getSurfacePresentModesKHR( *surface_ );

    uint32_t minImageCount = chooseSwapMinImageCount(surfaceCapabilities);

    swapChainSurfaceFormat_ = chooseSwapSurfaceFormat(availableFormats);

    swapPresentMode_ = chooseSwapPresentMode(availablePresentModes);

    swapChainExtent_ = chooseSwapExtent(surfaceCapabilities);

    vk::SwapchainCreateInfoKHR swapChainCreateInfo{.surface          = *surface_,
                                           .minImageCount    = minImageCount,
                                           .imageFormat      = swapChainSurfaceFormat_.format,
                                           .imageColorSpace  = swapChainSurfaceFormat_.colorSpace,
                                           .imageExtent      = swapChainExtent_,
                                           .imageArrayLayers = 1,
                                           .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage,
                                           .imageSharingMode = vk::SharingMode::eExclusive,
                                           .preTransform     = surfaceCapabilities.currentTransform,
                                           .compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque,
                                           .presentMode      = swapPresentMode_,
                                           .clipped          = true
    };

    swapChain_       = vk::raii::SwapchainKHR( logicalDevice_, swapChainCreateInfo );
    swapChainImages_ = swapChain_.getImages();
}

uint32_t VulkanContext::chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &surfaceCapabilities)
{
    auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
    if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount))
    {
        minImageCount = surfaceCapabilities.maxImageCount;
    }
    return minImageCount;
}

vk::SurfaceFormatKHR VulkanContext::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) 
{
    const auto formatIt = std::ranges::find_if(
            availableFormats,
            [](const auto &format) { return format.format == vk::Format::eB8G8R8A8Unorm && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear; });
    return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
}

vk::PresentModeKHR VulkanContext::chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const &availablePresentModes)
{
    assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) { return presentMode == vk::PresentModeKHR::eFifo; }));
    return std::ranges::any_of(availablePresentModes,
                               [](const vk::PresentModeKHR value) { return vk::PresentModeKHR::eMailbox == value; }) ?
                vk::PresentModeKHR::eMailbox :
                vk::PresentModeKHR::eFifo;
}

vk::Extent2D VulkanContext::chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities)
{
    // currentExtent is only set to the special "undefined" value described above
    // when the window manager lets us choose the extent ourselves; any other value
    // means the surface already dictates a fixed extent that we must use as-is.
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }
    int width, height;
    glfwGetFramebufferSize(window_, &width, &height);

    return 
    {
        std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
    };
}

void VulkanContext::createImageViews()
{
    assert(swapChainImageViews_.empty());

    vk::ImageViewCreateInfo imageViewCreateInfo
    { 
        .viewType         = vk::ImageViewType::e2D,
        .format           = swapChainSurfaceFormat_.format,
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor, .levelCount = 1, .layerCount = 1}
    };
    for (auto &image : swapChainImages_)
    {
        imageViewCreateInfo.image = image;
        swapChainImageViews_.emplace_back( logicalDevice_, imageViewCreateInfo );
    }
}

void VulkanContext::createStorageImageViews()
{
    assert(storageImageViews_.empty());

    vk::ImageViewCreateInfo storageViewCreateInfo
    { 
        .viewType         = vk::ImageViewType::e2D,
        .format           = swapChainSurfaceFormat_.format,
        .subresourceRange = 
        {
            .aspectMask     = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1
        }
    };
    for (auto &image : swapChainImages_)
    {
        storageViewCreateInfo.image = image;
        storageImageViews_.emplace_back( logicalDevice_, storageViewCreateInfo );
    }
}

void VulkanContext::createGraphicsPipeline() 
{
    auto shaderModule = createShaderModule(readFile("shader.spv"));

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo
    {
        .stage = vk::ShaderStageFlagBits::eVertex, 
        .module = shaderModule,  
        .pName = "vertMain" 
    };

        /*pSpecializationInfo, which we won’t be using here, but is worth discussing. 
        It allows you to specify values for shader constants. 
        You can use a single shader module where its behavior can be configured in pipeline creation by specifying different values for the constants used in it. 
        This is more efficient than configuring the shader using variables at render time, because the compiler can do optimizations like eliminating if statements that depend on these values.*/

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo
    { 
        .stage = vk::ShaderStageFlagBits::eFragment, 
        .module = shaderModule,  
        .pName = "fragMain" 
    };

    vk::PipelineShaderStageCreateInfo shaderStages[] = 
    {
        vertShaderStageInfo, 
        fragShaderStageInfo
    };

    std::vector<vk::DynamicState> dynamicStates = 
    {
        vk::DynamicState::eViewport, 
        vk::DynamicState::eScissor
    };

    vk::PipelineDynamicStateCreateInfo dynamicState
    {
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()), 
        .pDynamicStates = dynamicStates.data()
    };

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly
    {
        .topology = vk::PrimitiveTopology::eTriangleList
    };

    vk::Viewport viewport
    {
        0.0f, 
        0.0f, 
        static_cast<float>(swapChainExtent_.width), 
        static_cast<float>(swapChainExtent_.height), 
        0.0f, 
        1.0f
    };

    vk::Rect2D scissor
    {
        vk::Offset2D{ 0, 0 }, 
        swapChainExtent_
    };

    vk::PipelineViewportStateCreateInfo viewportState
    {
        .viewportCount = 1,
        .pViewports = &viewport, 
        .scissorCount = 1,
        .pScissors = &scissor
    };

    vk::PipelineRasterizationStateCreateInfo rasterizer
    {
        .depthClampEnable        = vk::False,
        .rasterizerDiscardEnable = vk::False,
        .polygonMode             = vk::PolygonMode::eFill,
        .cullMode                = vk::CullModeFlagBits::eBack,
        .frontFace               = vk::FrontFace::eClockwise,
        .depthBiasEnable         = vk::False,
        .lineWidth               = 1.0f
    };

    vk::PipelineMultisampleStateCreateInfo multisampling
    {
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
        .sampleShadingEnable = vk::False
    };

    vk::PipelineColorBlendAttachmentState colorBlendAttachment
    {
        .blendEnable    = vk::False,
        .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
    };

    vk::PipelineColorBlendStateCreateInfo colorBlending
    { 
        .logicOpEnable = vk::False, 
        .logicOp = vk::LogicOp::eCopy, 
        .attachmentCount = 1, 
        .pAttachments = &colorBlendAttachment
    };

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo
    {
        .setLayoutCount = 0, 
        .pushConstantRangeCount = 0
    };

    graphicsPipelineLayout_ = vk::raii::PipelineLayout(logicalDevice_, pipelineLayoutInfo);

    vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo
    {
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &swapChainSurfaceFormat_.format
    };

    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = 
    {
        {
            .stageCount          = 2,
            .pStages             = shaderStages,
            .pVertexInputState   = &vertexInputInfo,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState      = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState   = &multisampling,
            .pColorBlendState    = &colorBlending,
            .pDynamicState       = &dynamicState,
            .layout              = graphicsPipelineLayout_,
            .renderPass          = nullptr
        },
        {
            .colorAttachmentCount = 1, .pColorAttachmentFormats = &swapChainSurfaceFormat_.format
        }
    };

    graphicsPipeline_ = vk::raii::Pipeline
    (
        logicalDevice_,
        nullptr, 
        pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>()
    );
}

void VulkanContext::createComputePipeline()
{
    auto shaderModule = createShaderModule(readFile("path_tracing_main.spv")); 

    vk::PipelineShaderStageCreateInfo computeShaderStageInfo
    {
        .stage = vk::ShaderStageFlagBits::eCompute, 
        .module = shaderModule,  
        .pName = "main" 
    };

    vk::DescriptorSetLayoutBinding binding
    {
        .binding            = 1,
        .descriptorType     = vk::DescriptorType::eStorageImage,
        .descriptorCount    = 1,
        .stageFlags         = vk::ShaderStageFlagBits::eCompute,
        .pImmutableSamplers = nullptr
    };

    vk::DescriptorSetLayoutCreateInfo layoutInfo
    {
        .bindingCount = 1,
        .pBindings    = &binding
    };

    descriptorSetLayout_ = vk::raii::DescriptorSetLayout(logicalDevice_, layoutInfo);

    vk::PipelineLayoutCreateInfo computePipelineLayoutInfo
    {
        .setLayoutCount         = 1,
        .pSetLayouts            = &*descriptorSetLayout_, 
        .pushConstantRangeCount = 0
    };

    computePipelineLayout_ = vk::raii::PipelineLayout(logicalDevice_, computePipelineLayoutInfo); 

    vk::ComputePipelineCreateInfo computePipelineCreateInfo = 
    {

        .flags               = {},
        .stage               = computeShaderStageInfo,
        .layout              = *computePipelineLayout_,
    };

    computePipeline_ = vk::raii::Pipeline
    (
        logicalDevice_,
        nullptr, 
        computePipelineCreateInfo
    );
}

[[nodiscard]] vk::raii::ShaderModule VulkanContext::createShaderModule(const std::vector<char>& code) const
{
    vk::ShaderModuleCreateInfo createInfo
    {
        .codeSize = code.size() * sizeof(char), 
        .pCode = reinterpret_cast<const uint32_t*>(code.data())
    };

    vk::raii::ShaderModule shaderModule{ logicalDevice_, createInfo };

    return shaderModule;
}

void VulkanContext::createDescriptorSets()
{
    // 3 descriptors for images from our swapchain
    vk::DescriptorPoolSize poolSize{
        .type            = vk::DescriptorType::eStorageImage,
        .descriptorCount = 3 
    };

    vk::DescriptorPoolCreateInfo poolInfo{
        .maxSets       = 3,
        .poolSizeCount = 1,
        .pPoolSizes    = &poolSize
    };
    descriptorPool_ = vk::raii::DescriptorPool(logicalDevice_, poolInfo);

    std::vector<vk::DescriptorSetLayout> layouts(3, *descriptorSetLayout_);
    vk::DescriptorSetAllocateInfo allocInfo{
        .descriptorPool     = *descriptorPool_,
        .descriptorSetCount = 3,
        .pSetLayouts        = layouts.data()
    };

    descriptorSets_ = vk::raii::DescriptorSets(logicalDevice_, allocInfo);

    for (uint32_t i = 0; i < 3; ++i)
    {
        vk::DescriptorImageInfo imageInfo{
            .sampler     = nullptr,
            .imageView   = *storageImageViews_[i],
            .imageLayout = vk::ImageLayout::eGeneral
        };


        vk::WriteDescriptorSet descriptorWrite{
            .dstSet          = *descriptorSets_[i],
            .dstBinding      = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType  = vk::DescriptorType::eStorageImage,
            .pImageInfo      = &imageInfo 
        };

        logicalDevice_.updateDescriptorSets(descriptorWrite, nullptr);
    }
}


void VulkanContext::createCommandPool()
{
    vk::CommandPoolCreateInfo poolInfo
    {   
        .flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = queueIndex_
    };

    commandPool_ = vk::raii::CommandPool(logicalDevice_, poolInfo);
}

void VulkanContext::createCommandBuffers()
{
    vk::CommandBufferAllocateInfo allocInfo
    { 
        .commandPool = commandPool_, 
        .level = vk::CommandBufferLevel::ePrimary, 
        .commandBufferCount = MAX_FRAMES_IN_FLIGHT 
    };
    /*
    vk::CommandBufferLevel::ePrimary: Can be submitted to a queue for execution, but cannot be called from other command buffers.

    vk::CommandBufferLevel::eSecondary: Cannot be submitted directly, but can be called from primary command buffers.
    */

    commandBuffers_ = vk::raii::CommandBuffers(logicalDevice_, allocInfo);
    /*
    The vk::raii::CommandBuffers constructor generates a std::vector<vk::raii::CommandBuffer>, 
    but we just want a single one here, so we move it out of that vector into a singular variable.
    */
}

void VulkanContext::createSyncObjects()
{
    assert(presentCompleteSemaphores_.empty() && renderFinishedSemaphores_.empty() && inFlightFences_.empty());

	for (size_t i = 0; i < swapChainImages_.size(); i++)
	{
		renderFinishedSemaphores_.emplace_back(logicalDevice_, vk::SemaphoreCreateInfo());
	}

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		presentCompleteSemaphores_.emplace_back(logicalDevice_, vk::SemaphoreCreateInfo());
		inFlightFences_.emplace_back(logicalDevice_, vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
	}
}

