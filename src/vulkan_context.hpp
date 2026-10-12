#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS 
#define GLFW_INCLUDE_VULKAN

#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>

#include <iostream>
#include <ranges>
#include <fstream>

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<char const*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

class VulkanContext
{
public:

    VulkanContext() = delete;

    VulkanContext(GLFWwindow* window);

    enum class PipelineType 
    {
        Graphics,
        Compute
    };

    void drawFrame(PipelineType type);

    void cleanup();
    
private:
    GLFWwindow* window_ = nullptr;
    vk::raii::Context  context_;             //vulkan.so/vulkan-1.dll loader. Inits ptrs to basic driver functions, which we are going to use
    vk::raii::Instance instance_ = nullptr;  //logic connection between the gpu and the application
    vk::raii::DebugUtilsMessengerEXT debugMessenger_ = nullptr;
    vk::raii::PhysicalDevice physicalDevice_ = nullptr;
    vk::raii::Device logicalDevice_ = nullptr;
    vk::raii::Queue graphicsQueue_ = nullptr;
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

    uint32_t queueIndex_ = ~0;
    vk::raii::CommandPool commandPool_ = nullptr;
    std::vector<vk::raii::CommandBuffer> commandBuffers_;

    std::vector<vk::raii::Semaphore> presentCompleteSemaphores_;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores_;
    std::vector<vk::raii::Fence>     inFlightFences_;
    uint32_t frameIndex = 0;

    std::vector<const char*> requiredDeviceExtensions_ = {vk::KHRSwapchainExtensionName};

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
};