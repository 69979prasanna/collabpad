// server/Sha1.hpp
// Self-contained SHA-1 implementation.
// Required by the WebSocket RFC 6455 opening handshake to compute
// the Sec-WebSocket-Accept header value.
// Reference: FIPS PUB 180-4.
#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <string>

namespace collabpad {

class Sha1 {
public:
    // Compute SHA-1 of the given data and return the 20-byte raw digest.
    static std::array<uint8_t, 20> compute(const uint8_t* data, size_t length) {
        // Initial hash values (FIPS 180-4 §6.1)
        uint32_t h0 = 0x67452301u;
        uint32_t h1 = 0xEFCDAB89u;
        uint32_t h2 = 0x98BADCFEu;
        uint32_t h3 = 0x10325476u;
        uint32_t h4 = 0xC3D2E1F0u;

        // Pre-processing: append bit '1' then zeros then original length in bits (big-endian 64-bit)
        uint64_t bitLen = static_cast<uint64_t>(length) * 8;

        // Build padded message in chunks of 64 bytes
        // We process block by block without allocating the full padded message.
        auto processBlock = [&](const uint8_t* block) {
            uint32_t w[80];
            for (int i = 0; i < 16; ++i) {
                w[i] = (static_cast<uint32_t>(block[i * 4]) << 24)
                     | (static_cast<uint32_t>(block[i * 4 + 1]) << 16)
                     | (static_cast<uint32_t>(block[i * 4 + 2]) << 8)
                     |  static_cast<uint32_t>(block[i * 4 + 3]);
            }
            for (int i = 16; i < 80; ++i) {
                uint32_t val = w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16];
                w[i] = (val << 1) | (val >> 31); // ROTL1
            }
            uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
            for (int i = 0; i < 80; ++i) {
                uint32_t f, k;
                if (i < 20)       { f = (b & c) | (~b & d); k = 0x5A827999u; }
                else if (i < 40)  { f = b ^ c ^ d;           k = 0x6ED9EBA1u; }
                else if (i < 60)  { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
                else              { f = b ^ c ^ d;           k = 0xCA62C1D6u; }
                uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
                e = d; d = c;
                c = (b << 30) | (b >> 2);
                b = a; a = temp;
            }
            h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
        };

        // Process all complete 64-byte blocks from the original data
        size_t i = 0;
        for (; i + 64 <= length; i += 64) {
            processBlock(data + i);
        }

        // Remaining bytes + padding
        uint8_t lastBlocks[128] = {};
        size_t rem = length - i;
        std::memcpy(lastBlocks, data + i, rem);
        lastBlocks[rem] = 0x80; // append bit '1'

        // If the remaining bytes + padding + 8-byte length don't fit in one block, use two
        size_t padBlock = (rem < 55) ? 64 : 128;

        // Append original length in bits as 64-bit big-endian at the end
        lastBlocks[padBlock - 8] = static_cast<uint8_t>(bitLen >> 56);
        lastBlocks[padBlock - 7] = static_cast<uint8_t>(bitLen >> 48);
        lastBlocks[padBlock - 6] = static_cast<uint8_t>(bitLen >> 40);
        lastBlocks[padBlock - 5] = static_cast<uint8_t>(bitLen >> 32);
        lastBlocks[padBlock - 4] = static_cast<uint8_t>(bitLen >> 24);
        lastBlocks[padBlock - 3] = static_cast<uint8_t>(bitLen >> 16);
        lastBlocks[padBlock - 2] = static_cast<uint8_t>(bitLen >> 8);
        lastBlocks[padBlock - 1] = static_cast<uint8_t>(bitLen);

        processBlock(lastBlocks);
        if (padBlock == 128) {
            processBlock(lastBlocks + 64);
        }

        // Produce big-endian digest
        std::array<uint8_t, 20> digest{};
        auto store = [&](int offset, uint32_t v) {
            digest[offset]     = static_cast<uint8_t>(v >> 24);
            digest[offset + 1] = static_cast<uint8_t>(v >> 16);
            digest[offset + 2] = static_cast<uint8_t>(v >> 8);
            digest[offset + 3] = static_cast<uint8_t>(v);
        };
        store(0, h0); store(4, h1); store(8, h2); store(12, h3); store(16, h4);
        return digest;
    }

    // Compute SHA-1 of a std::string.
    static std::array<uint8_t, 20> compute(const std::string& s) {
        return compute(reinterpret_cast<const uint8_t*>(s.data()), s.size());
    }
};

// Base64-encode raw bytes — used to produce the Sec-WebSocket-Accept value.
inline std::string base64Encode(const uint8_t* data, size_t length) {
    static const char* table =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((length + 2) / 3) * 4);
    for (size_t i = 0; i < length; i += 3) {
        uint32_t b = static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < length) b |= static_cast<uint32_t>(data[i + 1]) << 8;
        if (i + 2 < length) b |= static_cast<uint32_t>(data[i + 2]);
        out.push_back(table[(b >> 18) & 0x3F]);
        out.push_back(table[(b >> 12) & 0x3F]);
        out.push_back((i + 1 < length) ? table[(b >> 6) & 0x3F] : '=');
        out.push_back((i + 2 < length) ? table[b & 0x3F] : '=');
    }
    return out;
}

} // namespace collabpad
