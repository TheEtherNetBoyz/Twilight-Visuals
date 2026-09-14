#pragma once

#include <cstddef>
#include <cstring>

namespace ss {

inline unsigned read16(const unsigned char* p) {
    return unsigned(p[0]) * 256u + p[1];
}

inline unsigned read32(const unsigned char* p) {
    return (read16(p) << 16) | read16(p + 2);
}

inline bool validBck(const void* data, std::size_t size) {
    const auto* b = static_cast<const unsigned char*>(data);
    if (!b || size < 0x60 || std::memcmp(b, "J3D1bck1", 8) != 0 ||
        read32(b + 8) != size || read32(b + 12) != 1 ||
        std::memcmp(b + 32, "ANK1", 4) != 0) {
        return false;
    }

    const auto* a = b + 32;
    const unsigned block = read32(a + 4);
    if (block > size - 32 || block < 36 || read16(a + 12) != 35 ||
        read16(a + 10) == 0 || read16(a + 10) > 300 || a[8] > 2 || a[9] > 4) {
        return false;
    }

    const unsigned table = read32(a + 20);
    if (table < 36 || table > block || 35u * 54u > block - table) {
        return false;
    }

    for (unsigned kind = 0; kind < 3; ++kind) {
        const unsigned count = read16(a + 14 + kind * 2);
        const unsigned offset = read32(a + 24 + kind * 4);
        const unsigned width = kind == 1 ? 2u : 4u;
        if (offset < 36 || offset % width || offset > block ||
            count > (block - offset) / width) {
            return false;
        }
        if (kind != 1) {
            for (unsigned i = 0; i < count; ++i) {
                if ((read32(a + offset + i * 4) & 0x7f800000u) == 0x7f800000u) {
                    return false;
                }
            }
        }
        for (unsigned joint = 0; joint < 35; ++joint) {
            for (unsigned axis = 0; axis < 3; ++axis) {
                const auto* key = a + table + joint * 54 + axis * 18 + kind * 6;
                const unsigned n = read16(key);
                const unsigned first = read16(key + 2);
                const unsigned type = read16(key + 4);
                if (!n || type > 1 || first > count ||
                    (n == 1 ? 1u : n * (type ? 4u : 3u)) > count - first) {
                    return false;
                }
            }
        }
    }
    return true;
}

}  // namespace ss
