#include "image_pipeline.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

static void setPixel(std::vector<uint8_t>& raster, uint16_t widthDots,
                     uint16_t x, uint16_t y) {
    uint16_t widthBytes = (widthDots + 7u) / 8u;
    raster[(size_t)y * widthBytes + (x / 8u)] |= (0x80u >> (x & 7u));
}

int main() {
    constexpr uint16_t width = 48;
    constexpr uint16_t height = 48;
    constexpr uint16_t widthBytes = (width + 7u) / 8u;

    std::vector<uint8_t> raster((size_t)widthBytes * height);
    setPixel(raster, width, 0, 0);
    setPixel(raster, width, 1, 7);
    setPixel(raster, width, 2, 8);
    setPixel(raster, width, 3, 23);
    setPixel(raster, width, 4, 24);
    setPixel(raster, width, 5, 31);
    setPixel(raster, width, 6, 47);

    std::vector<uint8_t> band(width * 3u);

    assert(packEscpos24DotBand(raster.data(), width, height, 0, band.data(), band.size()));
    assert(band[0] == 0x80);
    assert(band[3] == 0x01);
    assert(band[7] == 0x80);
    assert(band[11] == 0x01);
    assert(band[12] == 0x00);
    assert(band[13] == 0x00);
    assert(band[14] == 0x00);

    assert(packEscpos24DotBand(raster.data(), width, height, 24, band.data(), band.size()));
    assert(band[12] == 0x80);
    assert(band[15] == 0x01);
    assert(band[20] == 0x01);
    assert(!packEscpos24DotBand(raster.data(), width, height, 48, band.data(), band.size()));
    assert(!packEscpos24DotBand(raster.data(), width, height, 0, band.data(), band.size() - 1));

    return 0;
}
