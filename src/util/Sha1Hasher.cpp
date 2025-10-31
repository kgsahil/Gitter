#include "util/Sha1Hasher.hpp"
#include <cstring>

namespace gitter {

namespace {
inline uint32_t rotl(uint32_t x, uint32_t n) { return (x << n) | (x >> (32 - n)); }
}

Sha1Hasher::Sha1Hasher() { reset(); }

void Sha1Hasher::reset() {
    state[0] = 0x67452301;
    state[1] = 0xEFCDAB89;
    state[2] = 0x98BADCFE;
    state[3] = 0x10325476;
    state[4] = 0xC3D2E1F0;
    bitlen = 0;
    bufferLen = 0;
    std::memset(buffer, 0, sizeof(buffer));
}

void Sha1Hasher::update(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        buffer[bufferLen++] = data[i];
        if (bufferLen == 64) {
            transform(buffer);
            bitlen += 512;
            bufferLen = 0;
        }
    }
}

void Sha1Hasher::update(const std::string& data) {
    update(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

std::vector<uint8_t> Sha1Hasher::digest() {
    uint64_t totalBits = bitlen + bufferLen * 8ULL;
    buffer[bufferLen++] = 0x80;
    
    if (bufferLen > 56) {
        while (bufferLen < 64) buffer[bufferLen++] = 0;
        transform(buffer);
        bufferLen = 0;
    }
    
    while (bufferLen < 56) buffer[bufferLen++] = 0;
    
    // Append length in big-endian
    for (int i = 7; i >= 0; --i) {
        buffer[bufferLen++] = (totalBits >> (i * 8)) & 0xff;
    }
    transform(buffer);
    
    // Produce digest in big-endian
    std::vector<uint8_t> out(20);
    for (int i = 0; i < 5; ++i) {
        out[i*4 + 0] = (state[i] >> 24) & 0xff;
        out[i*4 + 1] = (state[i] >> 16) & 0xff;
        out[i*4 + 2] = (state[i] >> 8) & 0xff;
        out[i*4 + 3] = state[i] & 0xff;
    }
    
    reset();
    return out;
}

void Sha1Hasher::transform(const uint8_t* chunk) {
    uint32_t w[80];
    
    // Prepare message schedule
    for (int i = 0; i < 16; ++i) {
        w[i] = (chunk[i*4] << 24) | (chunk[i*4+1] << 16) | (chunk[i*4+2] << 8) | chunk[i*4+3];
    }
    for (int i = 16; i < 80; ++i) {
        w[i] = rotl(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
    }
    
    // Initialize working variables
    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];
    
    // Main loop
    for (int i = 0; i < 80; ++i) {
        uint32_t f, k;
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        
        uint32_t temp = rotl(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = rotl(b, 30);
        b = a;
        a = temp;
    }
    
    // Add to state
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

}

