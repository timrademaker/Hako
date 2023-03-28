#pragma once

// -----------------------------------------------------------------------------
// Source: https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.h
// MurmurHash3 was written by Austin Appleby, and is placed in the public domain.

#if defined(_MSC_VER) && (_MSC_VER < 1600)
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef unsigned __int64 uint64_t;
#else
#include <cstdint>
#endif

void MurmurHash3_x64_128(const void* key, int len, uint32_t seed, void* out);
