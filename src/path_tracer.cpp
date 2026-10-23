#include "path_tracer.hpp"

void PathTracer::run()
{
    initWindow();
    VulkanContext vkContext(window_);

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    loadObjFile("assets/cube.obj", vertices, indices);

    for(Vertex &v : vertices)
    {
        std::cout << "v x:" << v.pos.x << " y:" << v.pos.y << " z:" << v.pos.z << "\n";
    }

    for(uint32_t i : indices)
    {
        std::cout << "i:" << i << "\n";
    }

    vkContext.uploadModel(vertices, indices);

    mainLoop(vkContext);
    cleanup(vkContext);
}

void PathTracer::initWindow()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	window_ = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
}

void PathTracer::mainLoop(auto &vkContext)
{
    while (!glfwWindowShouldClose(window_)) 
    {
        glfwPollEvents();
        vkContext.drawFrame(VulkanContext::PipelineType::Compute);
    }
}

void PathTracer::cleanup(auto &vkContext)
{
    vkContext.cleanup(); 

    glfwDestroyWindow(window_);

    glfwTerminate();
}