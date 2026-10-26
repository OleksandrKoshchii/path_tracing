#include "scene.hpp"

const CameraInfo& Scene::getCameraInfo() const
{
    return camInfo_;
}

void Scene::updateCamInfo(double dX, double dY, glm::vec3 &deltaPos)
{
    camInfo_.position += deltaPos;

    camInfo_.yaw    += dX * camInfo_.sensitivity;
    camInfo_.pitch  += dY * camInfo_.sensitivity;

    glm::vec3 newForward;
    newForward.x = cos(glm::radians(camInfo_.yaw)) * cos(glm::radians(camInfo_.pitch));
    newForward.y = sin(glm::radians(camInfo_.pitch));
    newForward.z = sin(glm::radians(camInfo_.yaw)) * cos(glm::radians(camInfo_.pitch));
    
    camInfo_.dirForward = glm::normalize(newForward);
}