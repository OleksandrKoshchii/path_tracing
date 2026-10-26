#pragma once

#include "scene/camera.hpp"

class Scene
{
public:
    Scene() : camInfo_(CameraInfo{
                                .position   = glm::vec3(0.f, 0.f, -5.f),
                                .dirForward = glm::vec3(0.f, 0.f, 1.f),
                                .dirUp      = glm::vec3(0.f, 1.f, 0.f),
                                .fovy       = glm::radians(45.f),
                                .yaw = 0.f,
                                .pitch = 0.f,
                                .sensitivity = 0.05f,
                                .speed = 5.f,
                                }){} 

    const CameraInfo& getCameraInfo() const;

    void updateCamInfo(double dX, double dY, glm::vec3 &deltaPos);
private:
    CameraInfo camInfo_;
};