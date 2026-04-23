#include "image_pipeline.hpp"
#include <cmath>

void gammaCorrect(uint8_t* pixels, size_t len, float gamma) {
    uint8_t lut[256];
    float invGamma = 1.0f / gamma;
    for (int i = 0; i < 256; i++) {
        float v = roundf(powf(i / 255.0f, invGamma) * 255.0f);
        if (v < 0.0f) v = 0.0f;
        if (v > 255.0f) v = 255.0f;
        lut[i] = (uint8_t)v;
    }
    for (size_t i = 0; i < len; i++) {
        pixels[i] = lut[pixels[i]];
    }
}
