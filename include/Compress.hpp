#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// LZSS compressor/decompressor for binary voxel data.
// Window : 4096 bytes (12-bit offset)
// Min match : 3 bytes
// Max match : 18 bytes
//
// Compressed stream layout:
//   Repeat until end of input:
//     control_byte : uint8   (8 flags, LSB = item 0)
//     For each flag bit (0..7):
//       0  → literal   : uint8
//       1  → back-ref  : uint8 b1, uint8 b2
//              len    = (b1 >> 4) + 3          (3..18)
//              offset = ((b1 & 0xF) << 8) | b2  (0..4095, 0 = 1 byte back)
//
// File record layout written by Encode / expected by Decode:
//   uncomp_size : uint32 LE
//   comp_size   : uint32 LE
//   compressed bytes (comp_size bytes)
namespace Compress {
    namespace detail {
        inline std::size_t lzHash(const uint8_t* p) noexcept {
            return ((static_cast<uint32_t>(p[0]) * 65599u)
                ^ (static_cast<uint32_t>(p[1]) * 257u)
                ^  static_cast<uint32_t>(p[2])) & 0xFFFu;
        }
    }

    // Encode srcLen bytes from src into dst (appended).
    // Returns false only if srcLen > UINT32_MAX (practically impossible).
    inline bool Encode(const uint8_t* src, std::size_t srcLen, std::vector<uint8_t>& dst) {
        using detail::lzHash;
        constexpr int kWindow   = 4096;
        constexpr int kMinMatch = 3;
        constexpr int kMaxMatch = 18;

        if (srcLen > static_cast<std::size_t>(UINT32_MAX)) return false;

        dst.reserve(dst.size() + 8 + srcLen + (srcLen >> 3) + 16);

        
        const uint32_t uncompSize = static_cast<uint32_t>(srcLen);
        dst.push_back(static_cast<uint8_t>(uncompSize        & 0xFF));
        dst.push_back(static_cast<uint8_t>((uncompSize >>  8) & 0xFF));
        dst.push_back(static_cast<uint8_t>((uncompSize >> 16) & 0xFF));
        dst.push_back(static_cast<uint8_t>((uncompSize >> 24) & 0xFF));

        
        const std::size_t compSizeOffset = dst.size();
        dst.push_back(0); dst.push_back(0); dst.push_back(0); dst.push_back(0);

        
        std::array<int, kWindow> ht;
        ht.fill(-1);

        std::size_t i = 0;
        while (i < srcLen) {
            const std::size_t flagIdx = dst.size();
            dst.push_back(0u);
            uint8_t flags = 0u;

            for (int bit = 0; bit < 8 && i < srcLen; ++bit) {
                bool matched = false;
                if (i + static_cast<std::size_t>(kMinMatch) <= srcLen) {
                    const std::size_t h = lzHash(src + i);
                    const int cand = ht[h];
                    ht[h] = static_cast<int>(i);

                    if (cand >= 0 && static_cast<int>(i) - cand <= kWindow) {
                        const int maxLen = static_cast<int>(
                            std::min<std::size_t>(kMaxMatch, srcLen - i));
                        int len = 0;
                        while (len < maxLen && src[i + len] == src[cand + len])
                            ++len;
                        if (len >= kMinMatch) {
                            const int off = static_cast<int>(i) - cand - 1; // 0 = 1 byte back
                            dst.push_back(static_cast<uint8_t>(((len - kMinMatch) << 4) | (off >> 8)));
                            dst.push_back(static_cast<uint8_t>(off & 0xFF));
                            flags |= static_cast<uint8_t>(1u << bit);
                            i += static_cast<std::size_t>(len);
                            matched = true;
                        }
                    }
                }
                if (!matched)
                    dst.push_back(src[i++]);
            }
            dst[flagIdx] = flags;
        }

        const uint32_t compSize = static_cast<uint32_t>(dst.size() - compSizeOffset - 4);
        dst[compSizeOffset + 0] = static_cast<uint8_t>(compSize        & 0xFF);
        dst[compSizeOffset + 1] = static_cast<uint8_t>((compSize >>  8) & 0xFF);
        dst[compSizeOffset + 2] = static_cast<uint8_t>((compSize >> 16) & 0xFF);
        dst[compSizeOffset + 3] = static_cast<uint8_t>((compSize >> 24) & 0xFF);

        return true;
    }

    // Decode a stream previously written by Encode (reads uncomp_size, comp_size,
    // then decompresses comp_size bytes).  Returns false on malformed input.
    // src / srcLen cover only the payload starting at the uncomp_size field.
    inline bool Decode(const uint8_t* src, std::size_t srcLen, std::vector<uint8_t>& dst) {
        if (srcLen < 8) return false;

        const uint32_t uncompSize = static_cast<uint32_t>(src[0])
                                | (static_cast<uint32_t>(src[1]) <<  8)
                                | (static_cast<uint32_t>(src[2]) << 16)
                                | (static_cast<uint32_t>(src[3]) << 24);
        const uint32_t compSize   = static_cast<uint32_t>(src[4])
                                | (static_cast<uint32_t>(src[5]) <<  8)
                                | (static_cast<uint32_t>(src[6]) << 16)
                                | (static_cast<uint32_t>(src[7]) << 24);

        if (static_cast<std::size_t>(compSize) + 8u > srcLen) return false;

        const uint8_t* cs  = src + 8;
        const std::size_t csLen = static_cast<std::size_t>(compSize);

        dst.clear();
        dst.reserve(static_cast<std::size_t>(uncompSize));

        std::size_t i = 0;
        while (i < csLen) {
            const uint8_t flags = cs[i++];
            for (int bit = 0; bit < 8 && i < csLen; ++bit) {
                if (flags & (1u << bit)) {
                    if (i + 1 >= csLen) return false;
                    const uint8_t b1 = cs[i++];
                    const uint8_t b2 = cs[i++];
                    const int len      = (b1 >> 4) + 3;
                    const int off      = ((b1 & 0xF) << 8) | b2;
                    const int matchPos = static_cast<int>(dst.size()) - off - 1;
                    if (matchPos < 0) return false;
                    for (int k = 0; k < len; ++k)
                        dst.push_back(dst[static_cast<std::size_t>(matchPos + k)]);
                } else {
                    dst.push_back(cs[i++]);
                }
            }
        }

        return dst.size() == static_cast<std::size_t>(uncompSize);
    }
}