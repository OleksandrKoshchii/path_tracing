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

const std::vector<char const*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

class VulkanContext
{
public:

    VulkanContext() = delete;

    VulkanContext(GLFWwindow* window);

    void drawFrame();

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
    vk::raii::PipelineLayout pipelineLayout_ = nullptr;
    vk::raii::Pipeline graphicsPipeline_ = nullptr;
    uint32_t queueIndex_ = ~0;
    vk::raii::CommandPool commandPool_ = nullptr;
    vk::raii::CommandBuffer commandBuffer_ = nullptr;

    vk::raii::Semaphore presentCompleteSemaphore_ = nullptr;
    vk::raii::Semaphore renderFinishedSemaphore_  = nullptr;
    vk::raii::Fence     drawFence_                = nullptr;

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
    void createGraphicsPipeline();
    [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const;
    void createCommandPool();
    void createCommandBuffer();
    void createSyncObjects();
    void recordCommandBuffer(uint32_t imageIndex);
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