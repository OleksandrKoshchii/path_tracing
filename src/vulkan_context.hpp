#pragma once

#define VK_VERSION_1_3 1
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 
#define GLFW_INCLUDE_VULKAN

#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>

#include <iostream>
#include <ranges>
#include <fstream>
#include <cmath>
#include <unordered_map>
#include <glm/gtc/matrix_transform.hpp>

#include "constants.hpp"
#include "geometry/obj_loader.hpp"
#include "geometry/vk_context_scene_object.hpp"
#include "geometry/mesh.hpp"
#include "scene/scene.hpp"

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

const std::vector<char const*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

class VulkanContext
{
public:
    VulkanContext() {}

    VulkanContext(GLFWwindow* window);

    enum class PipelineType 
    {
        Graphics,
        Compute
    };

    void drawFrame(PipelineType type);

    void update(Scene& scene);

    void cleanup();
private:
    GLFWwindow* window_ = nullptr;
    vk::raii::Context  context_;             //vulkan.so/vulkan-1.dll loader. Inits ptrs to basic driver functions, which we are going to use
    vk::raii::Instance instance_ = nullptr;  //logic connection between the gpu and the application
    vk::raii::DebugUtilsMessengerEXT debugMessenger_ = nullptr;
    vk::raii::PhysicalDevice physicalDevice_ = nullptr;
    vk::raii::Device logicalDevice_ = nullptr;
    vk::raii::Queue graphicsAndComputeQueue_ = nullptr;
    vk::raii::SurfaceKHR surface_ = nullptr;
    vk::raii::SwapchainKHR swapChain_ = nullptr;
    std::vector<vk::Image> swapChainImages_;
    vk::SurfaceFormatKHR   swapChainSurfaceFormat_;
    vk::Extent2D           swapChainExtent_;
    vk::PresentModeKHR     swapPresentMode_;
    std::vector<vk::raii::ImageView> swapChainImageViews_;
    std::vector<vk::raii::ImageView> storageImageViews_;
    
    vk::raii::PipelineLayout graphicsPipelineLayout_ = nullptr;
    vk::raii::Pipeline graphicsPipeline_ = nullptr;
    vk::raii::PipelineLayout computePipelineLayout_ = nullptr;
    vk::raii::Pipeline computePipeline_ = nullptr;

    vk::raii::DescriptorSetLayout descriptorSetLayout_ = nullptr;

    vk::raii::DescriptorPool descriptorPool_ = nullptr;
    vk::raii::DescriptorSets descriptorSets_ = nullptr;

    uint32_t queueIndex_ = ~0u;
    vk::raii::CommandPool commandPool_ = nullptr;
    std::vector<vk::raii::CommandBuffer> commandBuffers_;

    std::vector<vk::raii::Semaphore> presentCompleteSemaphores_;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores_;
    std::vector<vk::raii::Fence>     inFlightFences_;
    uint32_t frameIndex = 0;

    std::vector<const char*> requiredDeviceExtensions_ = 
    {
        vk::KHRSwapchainExtensionName,
        vk::KHRAccelerationStructureExtensionName,
        vk::KHRDeferredHostOperationsExtensionName,
        vk::KHRRayQueryExtensionName
    };


    //#####################################################
    //#      THERE ARE SCENE DEPENDENT OBJECTS BELOW      #
    //#####################################################

    //###################################################
    //#      ALLOCATED BUFFERS(VERTICES, INDICES)       #
    //###################################################
    std::vector<vk::raii::Buffer> vertexBuffers_;
    std::vector<vk::raii::DeviceMemory> vertexDeviceMemories_;
    std::vector<vk::raii::Buffer> indexBuffers_;
    std::vector<vk::raii::DeviceMemory> indexDeviceMemories_;
    std::vector<MeshInfo> meshInfos_;

    
    //####################
    //#       BLAS       #
    //####################
    std::vector<vk::raii::Buffer> blasBuffers_;
    std::vector<vk::raii::DeviceMemory> blasMemories_;
    std::vector<vk::raii::AccelerationStructureKHR> blasHandles_;
    std::unordered_map<std::string_view, uint32_t> modelNameToBlasIndex_;

    
    //##################################
    //#       TLAS AND INSTANCES       #
    //##################################   
    std::vector<vkContextSceneObject> sceneObjects_;
    std::vector<vk::AccelerationStructureInstanceKHR> instances_;
    vk::raii::Buffer instancesBuffer_ = nullptr;
    vk::raii::DeviceMemory instancesMemory_ = nullptr;
    vk::raii::Buffer tlasBuffer_ = nullptr;
    vk::raii::DeviceMemory tlasMemory_ = nullptr;
    vk::raii::AccelerationStructureKHR tlasHandle_ = nullptr;

    Camera camera;

    static std::vector<char> readFile(const std::string& filename) 
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("failed to open file!");
        }

        std::vector<char> buffer(file.tellg());

        file.seekg(0, std::ios::beg);
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

        file.close();

        return buffer;
    }    

    void createInstance();
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT       severity,
                                                      vk::DebugUtilsMessageTypeFlagsEXT              type,
                                                      const vk::DebugUtilsMessengerCallbackDataEXT * pCallbackData,
                                                      void *                                         pUserData);
    void setupDebugMessenger();
    void createSurface();
    bool isDeviceSuitable( vk::raii::PhysicalDevice const & physicalDevice );
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapChain();
    uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &surfaceCapabilities);
    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
    vk::PresentModeKHR chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const &availablePresentModes);
    vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities);
    void createImageViews();
    void createStorageImageViews();
    void createGraphicsPipeline();
    void createComputePipeline();
    [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const;
    void createDescriptorSets();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();
    void recordGraphicsPipelineCommandBuffer(uint32_t imageIndex);
    void recordComputePipelineCommandBuffer(uint32_t imageIndex);
    void transition_image_layout(
            uint32_t                imageIndex,
            vk::ImageLayout         old_layout,
            vk::ImageLayout         new_layout,
            vk::AccessFlags2        src_access_mask,
            vk::AccessFlags2        dst_access_mask,
            vk::PipelineStageFlags2 src_stage_mask,
            vk::PipelineStageFlags2 dst_stage_mask);

    std::vector<const char*> getRequiredInstanceExtensions();

    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);

    vk::raii::CommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(vk::raii::CommandBuffer& commandBuffer);

    void createBLAS(
        vk::raii::Buffer &vertexBuffer,
        vk::raii::Buffer &indexBuffer, 
        MeshInfo &meshInfo, 
        vk::raii::CommandBuffer &cmd,
        std::vector<vk::raii::Buffer> &tempScratchBuffers,
        std::vector<vk::raii::DeviceMemory> &tempScratchMemories
    );
    void createInstances();

    void uploadModel(std::string &name, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

    void createBuffer(
        vk::DeviceSize size, 
        vk::BufferUsageFlags usage, 
        vk::MemoryPropertyFlags properties, 
        vk::raii::Buffer& buffer, 
        vk::raii::DeviceMemory& bufferMemory);

    void createTLAS();

    void createBLASes();

    void createSceneObjects();
};