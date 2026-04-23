#pragma once
#include <Arduino.h>
#include <cstddef>
#include <cstdint>

void gammaCorrect(uint8_t* pixels, size_t len, float gamma = 1.8f);

void ditherBayer4x4(const uint8_t* grey, uint16_t width, uint16_t height,
                    uint8_t* out);
