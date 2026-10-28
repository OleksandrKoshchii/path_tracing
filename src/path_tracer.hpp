#pragma once
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS //designated initializers for vulkan.hpp(destroys C-style constructors, what causes the opportunity to use designated initializers)

#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <cstdlib>
#include <string>
#include <cstdint> // Necessary for uint32_t
#include <limits> // Necessary for std::numeric_limits
#include <algorithm> // Necessary for std::clamp

#include <iostream>

#include "constants.hpp"
#include "vulkan_context.hpp"
#include "scene/scene.hpp"
#include "input_info.hpp"
#include "asset_manager.hpp"

class PathTracer 
{
public:
    void run(); 
private:
    GLFWwindow* window_ = nullptr;
    VulkanContext vkContext;
    Scene scene;
    AssetManager assetManager_;
    InputInfo inputInfo;

	void initWindow();

    void mainLoop(auto &vkContext); 

    void cleanup(auto &vkContext);

    void update(float deltaTime);

    void processInput(float deltaTime);

    void fixedUpdate(float deltaTime);
};