#pragma once
#include <cstddef>
#include <cstdint>

void gammaCorrect(uint8_t* pixels, size_t len, float gamma = 1.8f);

void ditherBayer4x4(const uint8_t* grey, uint16_t width, uint16_t height,
                    uint8_t* out);

void ditherBayer8x8(const uint8_t* grey, uint16_t width, uint16_t height,
                    uint8_t* out);

bool packEscpos24DotBand(const uint8_t* raster, uint16_t widthDots,
                         uint16_t heightDots, uint16_t yOffset,
                         uint8_t* out, size_t outLen);
