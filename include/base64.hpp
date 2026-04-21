#pragma once
#include <stdint.h>
#include <stddef.h>

namespace base64 {
int charVal(char c);
size_t decode(const char* in, size_t inLen, uint8_t* out);
}
