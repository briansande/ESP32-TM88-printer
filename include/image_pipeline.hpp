#pragma once
#include <Arduino.h>
#include <cstddef>

void gammaCorrect(uint8_t* pixels, size_t len, float gamma = 1.8f);
