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

#include "vulkan_context.hpp"

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

class HelloTriangleApplication 
{
public:
    void run() 
    {
        initWindow();
        VulkanContext vkContext(window_);
        mainLoop(vkContext);
        cleanup(vkContext);
    }

private:
    GLFWwindow* window_ = nullptr;

	void initWindow()
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		window_ = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
	}

    void mainLoop(auto &vkContext) 
    {
        while (!glfwWindowShouldClose(window_)) 
        {
            glfwPollEvents();
            vkContext.drawFrame(VulkanContext::PipelineType::Compute);
        }
    }

    void cleanup(auto &vkContext) 
    {
        vkContext.cleanup(); 

        glfwDestroyWindow(window_);

        glfwTerminate();
    }

};

int main()
{
    try
    {
        HelloTriangleApplication app;
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
