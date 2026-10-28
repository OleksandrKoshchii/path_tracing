#include "path_tracer.hpp"

void PathTracer::run()
{
    initWindow();
    vkContext = VulkanContext(window_, assetManager_);
    mainLoop(vkContext);
    cleanup(vkContext);
}

void PathTracer::initWindow()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	window_ = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glfwGetCursorPos(window_, &inputInfo.lastMouseX, &inputInfo.lastMouseY);
}

void PathTracer::mainLoop(auto &vkContext)
{
    double lastTime = glfwGetTime();
    double accumulator = 0.0;   

    const double fixedDeltaTime = 1/60.f; 

    while (!glfwWindowShouldClose(window_)) 
    {
        glfwPollEvents();

        double currentTime = glfwGetTime();
        double deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        if (deltaTime > 0.25) deltaTime = 0.25; 

        while (accumulator >= fixedDeltaTime)
        {
            fixedUpdate(static_cast<float>(fixedDeltaTime)); 
            
            accumulator -= fixedDeltaTime;
        }

        update(static_cast<float>(deltaTime));        

        vkContext.drawFrame(VulkanContext::PipelineType::Compute);
    }
}

void PathTracer::fixedUpdate(float deltaTime)
{

}

void PathTracer::update(float deltaTime)
{
    processInput(deltaTime);
}

void PathTracer::processInput(float deltaTime)
{
    glfwGetCursorPos(window_, &inputInfo.mouseX, &inputInfo.mouseY);
    inputInfo.deltaMouseX = inputInfo.mouseX - inputInfo.lastMouseX;
    inputInfo.deltaMouseY = inputInfo.mouseY - inputInfo.lastMouseY;

    glm::vec3 deltaPos{0.f, 0.f, 0.f};

    auto camInfo = scene.getCameraInfo();

    glm::vec3 forward = camInfo.dirForward;
    glm::vec3 right = glm::cross(camInfo.dirForward, camInfo.dirUp);
    float speed = camInfo.speed;

    if(glfwGetKey(window_, GLFW_KEY_W) == GLFW_PRESS)
    {
        deltaPos += forward * speed * deltaTime;
    }
    if(glfwGetKey(window_, GLFW_KEY_S) == GLFW_PRESS)
    {
        deltaPos -= forward * speed * deltaTime;
    }
    if(glfwGetKey(window_, GLFW_KEY_D) == GLFW_PRESS)
    {
        deltaPos += right * speed * deltaTime;
    }
    if(glfwGetKey(window_, GLFW_KEY_A) == GLFW_PRESS)
    {
        deltaPos -= right * speed * deltaTime;
    }

    scene.updateCamInfo(inputInfo.deltaMouseX, inputInfo.deltaMouseY, deltaPos);

    vkContext.update(scene);

    inputInfo.lastMouseX = inputInfo.mouseX;
    inputInfo.lastMouseY = inputInfo.mouseY;
}

void PathTracer::cleanup(auto &vkContext)
{
    vkContext.cleanup(); 

    glfwDestroyWindow(window_);

    glfwTerminate();
}