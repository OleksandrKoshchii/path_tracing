#pragma once

#include <cstdint>

constexpr uint32_t WIDTH = 2560;
constexpr uint32_t HEIGHT = 1440;
const float ASPECT = static_cast<float>(WIDTH)/static_cast<float>(HEIGHT);

constexpr int MAX_FRAMES_IN_FLIGHT = 2;