#pragma once

#include <cstdint>

constexpr uint32_t WIDTH = 1000;
constexpr uint32_t HEIGHT = 600;
const float ASPECT = static_cast<float>(WIDTH)/static_cast<float>(HEIGHT);

constexpr int MAX_FRAMES_IN_FLIGHT = 2;