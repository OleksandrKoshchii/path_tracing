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

#include "vulkan_context.hpp"
#include "geometry/obj_loader.hpp"

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

class PathTracer 
{
public:
    void run(); 


private:
    GLFWwindow* window_ = nullptr;

	void initWindow();

    void mainLoop(auto &vkContext); 

    void cleanup(auto &vkContext);
};