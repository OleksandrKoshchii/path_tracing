#pragma once

#include <glm/glm.hpp>
#include <cstdint>

struct Camera
{
   glm::mat4 invProjView;
   glm::vec3 position;
   float width;
   float height;
};

struct CameraInfo
{
    glm::vec3 position;
    glm::vec3 dirForward;
    glm::vec3 dirUp;
    float fovy; //radians
    float yaw;
    float pitch;
    float sensitivity;
    float speed;
};