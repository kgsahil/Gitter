#include "util/Sha256Hasher.hpp"

#include <array>
#include <cstring>

namespace gitter {

namespace {
constexpr std::array<uint32_t, 64> K = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

inline uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
}

Sha256Hasher::Sha256Hasher() { reset(); }

void Sha256Hasher::reset() {
    state[0]=0x6a09e667; state[1]=0xbb67ae85; state[2]=0x3c6ef372; state[3]=0xa54ff53a;
    state[4]=0x510e527f; state[5]=0x9b05688c; state[6]=0x1f83d9ab; state[7]=0x5be0cd19;
    bitlen = 0; bufferLen = 0; std::memset(buffer, 0, sizeof(buffer));
}

void Sha256Hasher::update(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        buffer[bufferLen++] = data[i];
        if (bufferLen == 64) {
            transform(buffer);
            bitlen += 512;
            bufferLen = 0;
        }
    }
}

void Sha256Hasher::update(const std::vector<uint8_t>& data) { update(data.data(), data.size()); }
void Sha256Hasher::update(const std::string& data) { update(reinterpret_cast<const uint8_t*>(data.data()), data.size()); }

std::vector<uint8_t> Sha256Hasher::digest() {
    uint64_t totalBits = bitlen + bufferLen * 8ULL;
    buffer[bufferLen++] = 0x80;
    if (bufferLen > 56) {
        while (bufferLen < 64) buffer[bufferLen++] = 0;
        transform(buffer); bufferLen = 0;
    }
    while (bufferLen < 56) buffer[bufferLen++] = 0;
    for (int i = 7; i >= 0; --i) buffer[bufferLen++] = (totalBits >> (i*8)) & 0xff;
    transform(buffer);

    std::vector<uint8_t> out(32);
    for (int i = 0; i < 8; ++i) {
        out[i*4+0] = (state[i] >> 24) & 0xff;
        out[i*4+1] = (state[i] >> 16) & 0xff;
        out[i*4+2] = (state[i] >> 8) & 0xff;
        out[i*4+3] = state[i] & 0xff;
    }
    reset();
    return out;
}

void Sha256Hasher::transform(const uint8_t* chunk) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (chunk[i*4+0] << 24) | (chunk[i*4+1] << 16) | (chunk[i*4+2] << 8) | (chunk[i*4+3]);
    }
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = rotr(w[i-15], 7) ^ rotr(w[i-15], 18) ^ (w[i-15] >> 3);
        uint32_t s1 = rotr(w[i-2], 17) ^ rotr(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a=state[0], b=state[1], c=state[2], d=state[3], e=state[4], f=state[5], g=state[6], h=state[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = rotr(e,6) ^ rotr(e,11) ^ rotr(e,25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = rotr(a,2) ^ rotr(a,13) ^ rotr(a,22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;
        h = g; g = f; f = e; e = d + temp1; d = c; c = b; b = a; a = temp1 + temp2;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

}

