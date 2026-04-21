#include "base64.hpp"

namespace base64 {

int charVal(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

size_t decode(const char* in, size_t inLen, uint8_t* out) {
  size_t outLen = 0;
  int buf = 0, nbits = 0;
  for (size_t i = 0; i < inLen; i++) {
    int val = charVal(in[i]);
    if (val < 0) continue;
    buf = (buf << 6) | val;
    nbits += 6;
    if (nbits >= 8) {
      nbits -= 8;
      out[outLen++] = (uint8_t)(buf >> nbits);
    }
  }
  return outLen;
}

}
