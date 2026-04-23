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

static constexpr uint8_t kBayer4x4[4][4] = {
    {  0, 128,  32, 160},
    {192,  64, 224,  96},
    { 48, 176,  16, 144},
    {240, 112, 208,  80}
};

void ditherBayer4x4(const uint8_t* grey, uint16_t width, uint16_t height,
                    uint8_t* out) {
    uint16_t rowBytes = (width + 7u) / 8u;

    for (uint16_t y = 0; y < height; y++) {
        for (uint16_t bx = 0; bx < rowBytes; bx++) {
            uint8_t byte = 0;
            for (int bit = 7; bit >= 0; bit--) {
                uint16_t x = bx * 8 + (7 - bit);
                if (x < width) {
                    uint8_t threshold = kBayer4x4[y & 3][x & 3];
                    if (grey[y * width + x] <= threshold) byte |= (1 << bit);
                }
            }
            out[y * rowBytes + bx] = byte;
        }
    }
}
