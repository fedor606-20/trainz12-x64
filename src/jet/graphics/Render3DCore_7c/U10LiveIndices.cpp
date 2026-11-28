// Unit U10 wave 5: box-solid and tube index builders (0x61c70, 0x6a510, 0x6a7f0). Pure 16-bit writers; every loop
// bound and truncation is the original's.
#include <jet/Render3DCorePrimitives.h>

namespace Jet {

// 0x61c70
void __cdecl RenderBox::BuildIndicesSolid(int xSegments, int ySegments, int zSegments, int xCells,
                                          unsigned short* idx, int vertexCount)
{
    (void)xCells;
    (void)vertexCount;
    int x = xSegments;
    int y = ySegments;
    const int z = zSegments;
    int cursor = 0;
    const short ys = static_cast<short>(y);
    int local_1c = 0;
    if (0 < z) {
        int local_18 = 0;
        int rows = z;
        local_1c = z;
        do {
            int left = y;
            int cell = local_18;
            if (0 < y) {
                do {
                    const short s = static_cast<short>(cell);
                    idx[cursor] = s;
                    idx[cursor + 1] = static_cast<unsigned short>(ys + 1 + s);
                    const short s2 = static_cast<short>(ys + 2 + s);
                    idx[cursor + 2] = static_cast<unsigned short>(s2);
                    idx[cursor + 3] = s;
                    idx[cursor + 4] = static_cast<unsigned short>(s2);
                    idx[cursor + 5] = static_cast<unsigned short>(s + 1);
                    cursor += 6;
                    --left;
                    ++cell;
                } while (left != 0);
            }
            local_18 += y + 1;
            --rows;
        } while (rows != 0);
    }
    if (zSegments == 1) {
        unsigned int base = 0;
        int row = (local_1c + 1) * (y + 1);
        if (0 < y) {
            do {
                idx[cursor] = static_cast<unsigned short>(base);
                const short s = static_cast<short>(row + 1);
                idx[cursor + 1] = static_cast<unsigned short>(s);
                idx[cursor + 2] = static_cast<unsigned short>(row);
                idx[cursor + 3] = static_cast<unsigned short>(base);
                idx[cursor + 4] = static_cast<unsigned short>(static_cast<unsigned short>(base) + 1);
                idx[cursor + 5] = static_cast<unsigned short>(s);
                ++base;
                cursor += 6;
                ++row;
            } while (static_cast<int>(base) < y);
        }
        if (0 < z) {
            int rows = z;
            do {
                const short s = static_cast<short>(base);
                idx[cursor] = s;
                const int stride = y + 1;
                const short s2 = static_cast<short>(static_cast<short>(row) + static_cast<short>(stride));
                idx[cursor + 1] = static_cast<unsigned short>(s2);
                idx[cursor + 2] = static_cast<unsigned short>(row);
                idx[cursor + 3] = s;
                idx[cursor + 4] = static_cast<unsigned short>(s + static_cast<short>(stride));
                idx[cursor + 5] = static_cast<unsigned short>(s2);
                cursor += 6;
                base += stride;
                row += stride;
                --rows;
            } while (rows != 0);
        }
        if (0 < y) {
            int rows = y;
            do {
                idx[cursor] = static_cast<unsigned short>(base);
                const short s = static_cast<short>(row - 1);
                idx[cursor + 1] = static_cast<unsigned short>(s);
                idx[cursor + 2] = static_cast<unsigned short>(row);
                idx[cursor + 3] = static_cast<unsigned short>(base);
                idx[cursor + 4] = static_cast<unsigned short>(static_cast<unsigned short>(base) - 1);
                idx[cursor + 5] = static_cast<unsigned short>(s);
                cursor += 6;
                --base;
                --row;
                --rows;
            } while (rows != 0);
        }
        const int step = -1 - y;
        if (0 < z) {
            int rows = z;
            do {
                const short s = static_cast<short>(base);
                idx[cursor] = s;
                const short s2 = static_cast<short>(static_cast<short>(row) + static_cast<short>(step));
                idx[cursor + 1] = static_cast<unsigned short>(s2);
                idx[cursor + 2] = static_cast<unsigned short>(row);
                idx[cursor + 3] = s;
                idx[cursor + 4] = static_cast<unsigned short>(s + static_cast<short>(step));
                idx[cursor + 5] = static_cast<unsigned short>(s2);
                cursor += 6;
                base += step;
                row += step;
                --rows;
            } while (rows != 0);
        }
    } else {
        int row = (local_1c + 1) * (y + 1);
        unsigned int base = 0;
        int next = row;
        if (0 < y) {
            do {
                const short s = static_cast<short>(base);
                idx[cursor] = s;
                const short s2 = static_cast<short>(next + 1);
                idx[cursor + 1] = static_cast<unsigned short>(s2);
                idx[cursor + 2] = static_cast<unsigned short>(next);
                idx[cursor + 3] = s;
                idx[cursor + 4] = static_cast<unsigned short>(s + 1);
                idx[cursor + 5] = static_cast<unsigned short>(s2);
                ++base;
                cursor += 6;
                ++next;
            } while (static_cast<int>(base) < y);
        }
        if (0 < z) {
            int rows = z;
            do {
                const short s = static_cast<short>(base);
                idx[cursor] = s;
                const short s2 = static_cast<short>(next + 1);
                idx[cursor + 1] = static_cast<unsigned short>(s2);
                idx[cursor + 2] = static_cast<unsigned short>(next);
                idx[cursor + 3] = s;
                idx[cursor + 4] = static_cast<unsigned short>(s + static_cast<short>(y + 1));
                idx[cursor + 5] = static_cast<unsigned short>(s2);
                cursor += 6;
                base += y + 1;
                ++next;
                --rows;
            } while (rows != 0);
        }
        if (0 < y) {
            int rows = y;
            do {
                idx[cursor] = static_cast<unsigned short>(base);
                const short s = static_cast<short>(next + 1);
                idx[cursor + 1] = static_cast<unsigned short>(s);
                idx[cursor + 2] = static_cast<unsigned short>(next);
                idx[cursor + 3] = static_cast<unsigned short>(base);
                idx[cursor + 4] = static_cast<unsigned short>(static_cast<unsigned short>(base) - 1);
                idx[cursor + 5] = static_cast<unsigned short>(s);
                cursor += 6;
                --base;
                ++next;
                --rows;
            } while (rows != 0);
        }
        const unsigned short b0 = static_cast<unsigned short>(next);
        const unsigned short b1 = static_cast<unsigned short>(base);
        const int step = -1 - y;
        int rows = z - 1;
        if (0 < rows) {
            do {
                const short s = static_cast<short>(base);
                idx[cursor] = s;
                const short s2 = static_cast<short>(next + 1);
                idx[cursor + 1] = static_cast<unsigned short>(s2);
                idx[cursor + 2] = static_cast<unsigned short>(next);
                idx[cursor + 3] = s;
                idx[cursor + 4] = static_cast<unsigned short>(s + static_cast<short>(step));
                idx[cursor + 5] = static_cast<unsigned short>(s2);
                cursor += 6;
                base += step;
                ++next;
                --rows;
            } while (rows != 0);
        }
        idx[cursor] = b1;
        idx[cursor + 1] = static_cast<unsigned short>(row);
        idx[cursor + 2] = b0;
        idx[cursor + 3] = b1;
        idx[cursor + 4] = 0;
        idx[cursor + 5] = static_cast<unsigned short>(row);
        cursor += 6;
        const int ring = (z + y) * 2;
        const int lastRing = row + ring;
        if (1 < z - 1) {
            int levels = z - 2;
            int prev = row;
            int cur = lastRing;
            do {
                row = cur;
                cur = prev;
                int other = row;
                if (0 < y) {
                    int n = y;
                    do {
                        const short s = static_cast<short>(cur);
                        idx[cursor] = s;
                        const short s2 = static_cast<short>(other + 1);
                        idx[cursor + 1] = static_cast<unsigned short>(s2);
                        idx[cursor + 2] = static_cast<unsigned short>(other);
                        idx[cursor + 3] = s;
                        idx[cursor + 4] = static_cast<unsigned short>(s + 1);
                        idx[cursor + 5] = static_cast<unsigned short>(s2);
                        cursor += 6;
                        ++cur;
                        ++other;
                        --n;
                    } while (n != 0);
                }
                if (0 < z) {
                    int n = z;
                    do {
                        const short s = static_cast<short>(cur);
                        idx[cursor] = s;
                        const short s2 = static_cast<short>(other + 1);
                        idx[cursor + 1] = static_cast<unsigned short>(s2);
                        idx[cursor + 2] = static_cast<unsigned short>(other);
                        idx[cursor + 3] = s;
                        idx[cursor + 4] = static_cast<unsigned short>(s + 1);
                        idx[cursor + 5] = static_cast<unsigned short>(s2);
                        cursor += 6;
                        ++cur;
                        ++other;
                        --n;
                    } while (n != 0);
                }
                if (0 < y) {
                    int n = y;
                    do {
                        const short s = static_cast<short>(cur);
                        idx[cursor] = s;
                        const short s2 = static_cast<short>(other + 1);
                        idx[cursor + 1] = static_cast<unsigned short>(s2);
                        idx[cursor + 2] = static_cast<unsigned short>(other);
                        idx[cursor + 3] = s;
                        idx[cursor + 4] = static_cast<unsigned short>(s + 1);
                        idx[cursor + 5] = static_cast<unsigned short>(s2);
                        cursor += 6;
                        ++cur;
                        ++other;
                        --n;
                    } while (n != 0);
                }
                const unsigned short o0 = static_cast<unsigned short>(other);
                const unsigned short o1 = static_cast<unsigned short>(cur);
                int n = z - 1;
                if (0 < n) {
                    do {
                        const short s = static_cast<short>(cur);
                        idx[cursor] = s;
                        const short s2 = static_cast<short>(other + 1);
                        idx[cursor + 1] = static_cast<unsigned short>(s2);
                        idx[cursor + 2] = static_cast<unsigned short>(other);
                        idx[cursor + 3] = s;
                        idx[cursor + 4] = static_cast<unsigned short>(s + 1);
                        idx[cursor + 5] = static_cast<unsigned short>(s2);
                        cursor += 6;
                        ++cur;
                        ++other;
                        --n;
                    } while (n != 0);
                }
                idx[cursor] = o1;
                idx[cursor + 1] = static_cast<unsigned short>(row);
                idx[cursor + 2] = o0;
                idx[cursor + 3] = o1;
                idx[cursor + 4] = static_cast<unsigned short>(prev);
                idx[cursor + 5] = static_cast<unsigned short>(row);
                cursor += 6;
                cur = row + ring;
                --levels;
                prev = row;
            } while (levels != 0);
        }
        row = lastRing;
        int cur = lastRing + ring;
        int other = cur;
        if (0 < y) {
            do {
                const short s = static_cast<short>(row);
                idx[cursor] = s;
                const short s2 = static_cast<short>(other + 1);
                idx[cursor + 1] = static_cast<unsigned short>(s2);
                idx[cursor + 2] = static_cast<unsigned short>(other);
                idx[cursor + 3] = s;
                idx[cursor + 4] = static_cast<unsigned short>(s + 1);
                idx[cursor + 5] = static_cast<unsigned short>(s2);
                cursor += 6;
                ++row;
                ++other;
                --y;
            } while (y != 0);
        }
        if (0 < z) {
            int n = z;
            do {
                const short s = static_cast<short>(row);
                idx[cursor] = s;
                const short s2 = static_cast<short>(other + static_cast<short>(xSegments + 1));
                idx[cursor + 1] = static_cast<unsigned short>(s2);
                idx[cursor + 2] = static_cast<unsigned short>(other);
                idx[cursor + 3] = s;
                idx[cursor + 4] = static_cast<unsigned short>(s + 1);
                idx[cursor + 5] = static_cast<unsigned short>(s2);
                cursor += 6;
                ++row;
                other += xSegments + 1;
                --n;
            } while (n != 0);
        }
        for (int n = xSegments; 0 < n; --n) {
            const short s = static_cast<short>(row);
            idx[cursor] = s;
            const short s2 = static_cast<short>(other - 1);
            idx[cursor + 1] = static_cast<unsigned short>(s2);
            idx[cursor + 2] = static_cast<unsigned short>(other);
            idx[cursor + 3] = s;
            idx[cursor + 4] = static_cast<unsigned short>(s + 1);
            idx[cursor + 5] = static_cast<unsigned short>(s2);
            cursor += 6;
            ++row;
            --other;
        }
        const unsigned short o0 = static_cast<unsigned short>(other);
        const unsigned short o1 = static_cast<unsigned short>(row);
        int n = z - 1;
        if (0 < n) {
            do {
                const short s = static_cast<short>(row);
                idx[cursor] = s;
                const short s2 = static_cast<short>(other + step);
                idx[cursor + 1] = static_cast<unsigned short>(s2);
                idx[cursor + 2] = static_cast<unsigned short>(other);
                idx[cursor + 3] = s;
                idx[cursor + 4] = static_cast<unsigned short>(s + 1);
                idx[cursor + 5] = static_cast<unsigned short>(s2);
                cursor += 6;
                ++row;
                other += step;
                --n;
            } while (n != 0);
        }
        idx[cursor] = o1;
        idx[cursor + 1] = static_cast<unsigned short>(cur);
        idx[cursor + 2] = o0;
        idx[cursor + 3] = o1;
        idx[cursor + 4] = static_cast<unsigned short>(lastRing);
        idx[cursor + 5] = static_cast<unsigned short>(cur);
        cursor += 6;
    }
    int ringVertices = (z + 1) * (y + 1) + (y + z) * (x - 1) * 2;
    if (0 < z) {
        int rows = z;
        do {
            if (0 < y) {
                int cols = y;
                int base = ringVertices;
                do {
                    const short s = static_cast<short>(base);
                    idx[cursor] = s;
                    const short s2 = static_cast<short>(ys + 2 + s);
                    idx[cursor + 1] = static_cast<unsigned short>(s2);
                    idx[cursor + 2] = static_cast<unsigned short>(ys + 1 + s);
                    idx[cursor + 3] = s;
                    idx[cursor + 4] = static_cast<unsigned short>(s + 1);
                    idx[cursor + 5] = static_cast<unsigned short>(s2);
                    cursor += 6;
                    ++base;
                    --cols;
                } while (cols != 0);
            }
            ++ringVertices;
            --rows;
        } while (rows != 0);
    }
}

// 0x6a510
void __cdecl RenderTube::BuildIndicesSolid(int segments, int vertexCount, unsigned short* idx, int unused)
{
    (void)unused;
    const int n2 = segments * 2;
    const short sn2 = static_cast<short>(n2);
    int cursor = 0;
    int i = 0;
    if (height <= 0.0f) {
        const short bottom = static_cast<short>(vertexCount - 2);
        const short top = static_cast<short>(vertexCount - 1);
        if (vertexCount != 4 && -1 < vertexCount - 2) {
            int k = 0;
            do {
                idx[cursor] = bottom;
                const short s = static_cast<short>(k);
                idx[cursor + 1] = s;
                idx[cursor + 2] = static_cast<unsigned short>(s + 2);
                idx[cursor + 3] = top;
                idx[cursor + 4] = static_cast<unsigned short>(s + 3);
                idx[cursor + 5] = static_cast<unsigned short>(s + 1);
                k += 2;
                cursor += 6;
            } while (k < vertexCount - 2);
        }
        idx[cursor] = bottom;
        idx[cursor + 1] = static_cast<unsigned short>(i);
        idx[cursor + 2] = 0;
        idx[cursor + 3] = top;
        idx[cursor + 4] = 1;
        idx[cursor + 5] = static_cast<unsigned short>(i + 1);
        cursor += 6;
    } else {
        if (n2 != 2 && -1 < n2 - 2) {
            do {
                const short s = static_cast<short>(i);
                idx[cursor] = static_cast<unsigned short>(s + sn2);
                idx[cursor + 1] = s;
                idx[cursor + 3] = static_cast<unsigned short>(s + 2);
                idx[cursor + 4] = static_cast<unsigned short>(s + 1 + sn2);
                idx[cursor + 5] = static_cast<unsigned short>(s + 3);
                idx[cursor + 7] = static_cast<unsigned short>(s + 1);
                idx[cursor + 8] = static_cast<unsigned short>(s + sn2);
                idx[cursor + 9] = static_cast<unsigned short>(s + 2);
                idx[cursor + 10] = static_cast<unsigned short>(s + 2 + sn2);
                idx[cursor + 11] = static_cast<unsigned short>(s + 1 + sn2);
                idx[cursor + 12] = static_cast<unsigned short>(s + 3 + sn2);
                idx[cursor + 13] = static_cast<unsigned short>(s + 3);
                i += 2;
                cursor += 12;
            } while (i < n2 - 2);
        }
        idx[cursor] = static_cast<unsigned short>(static_cast<short>(i) + sn2);
        idx[cursor + 1] = static_cast<unsigned short>(i);
        idx[cursor + 2] = 0;
        idx[cursor + 4] = static_cast<unsigned short>(static_cast<short>(i) + sn2 + 1);
        idx[cursor + 5] = 1;
        idx[cursor + 6] = static_cast<unsigned short>(i + 1);
        idx[cursor + 7] = static_cast<unsigned short>(static_cast<short>(i) + sn2);
        idx[cursor + 8] = 0;
        idx[cursor + 10] = sn2;
        idx[cursor + 11] = static_cast<unsigned short>(static_cast<short>(i) + sn2 + 1);
        idx[cursor + 12] = static_cast<unsigned short>(sn2 + 1);
        cursor += 11;
        idx[cursor] = 1;
        cursor += 1;
        i = 0;
        if (0 < n2 - 2) {
            do {
                const short s = static_cast<short>(i);
                idx[cursor] = s;
                idx[cursor + 1] = static_cast<unsigned short>(s + 1);
                idx[cursor + 2] = static_cast<unsigned short>(s + 3);
                idx[cursor + 3] = s;
                idx[cursor + 4] = static_cast<unsigned short>(s + 3);
                idx[cursor + 5] = static_cast<unsigned short>(s + 2);
                i += 2;
                cursor += 6;
            } while (i < n2 - 2);
        }
        idx[cursor] = static_cast<unsigned short>(i);
        idx[cursor + 1] = static_cast<unsigned short>(i + 1);
        idx[cursor + 2] = 1;
        idx[cursor + 3] = static_cast<unsigned short>(i);
        idx[cursor + 4] = 1;
        idx[cursor + 5] = 0;
        cursor += 6;
        if (0.0f < height) {
            i = n2;
            if (n2 < segments * 4 - 2) {
                do {
                    const short s = static_cast<short>(i);
                    idx[cursor] = s;
                    idx[cursor + 1] = static_cast<unsigned short>(s + 3);
                    idx[cursor + 2] = static_cast<unsigned short>(s + 1);
                    idx[cursor + 3] = s;
                    idx[cursor + 4] = static_cast<unsigned short>(s + 2);
                    idx[cursor + 5] = static_cast<unsigned short>(s + 3);
                    i += 2;
                    cursor += 6;
                } while (i < segments * 4 - 2);
            }
            const short s = static_cast<short>(i);
            idx[cursor] = s;
            idx[cursor + 1] = static_cast<unsigned short>(sn2 + 1);
            idx[cursor + 2] = static_cast<unsigned short>(s + 1);
            idx[cursor + 3] = s;
            idx[cursor + 4] = sn2;
            idx[cursor + 6] = static_cast<unsigned short>(sn2 + 1);
        }
        return;
    }
    // inner radius <= 0: the remaining cap/ring indices, after the bottom cap above
    const int start = cursor;
    int k = 0;
    cursor = start;
    i = 0;
    if (0 < n2 - 2) {
        do {
            const short s = static_cast<short>(i);
            idx[cursor] = s;
            idx[cursor + 1] = static_cast<unsigned short>(s + 1);
            idx[cursor + 2] = static_cast<unsigned short>(s + 3);
            idx[cursor + 3] = s;
            idx[cursor + 4] = static_cast<unsigned short>(s + 3);
            idx[cursor + 5] = static_cast<unsigned short>(s + 2);
            i += 2;
            cursor += 6;
        } while (i < n2 - 2);
    }
    idx[cursor] = static_cast<unsigned short>(i);
    idx[cursor + 1] = static_cast<unsigned short>(i + 1);
    idx[cursor + 2] = 1;
    idx[cursor + 3] = static_cast<unsigned short>(i);
    idx[cursor + 4] = 1;
    idx[cursor + 5] = 0;
    cursor += 6;
    if (0.0f < height) {
        i = n2;
        if (n2 < segments * 4 - 2) {
            do {
                const short s = static_cast<short>(i);
                idx[cursor] = s;
                idx[cursor + 1] = static_cast<unsigned short>(s + 3);
                idx[cursor + 2] = static_cast<unsigned short>(s + 1);
                idx[cursor + 3] = s;
                idx[cursor + 4] = static_cast<unsigned short>(s + 2);
                idx[cursor + 5] = static_cast<unsigned short>(s + 3);
                i += 2;
                cursor += 6;
            } while (i < segments * 4 - 2);
        }
        const short s = static_cast<short>(i);
        idx[cursor] = s;
        idx[cursor + 1] = static_cast<unsigned short>(sn2 + 1);
        idx[cursor + 2] = static_cast<unsigned short>(s + 1);
        idx[cursor + 3] = s;
        idx[cursor + 4] = sn2;
        idx[cursor + 6] = static_cast<unsigned short>(sn2 + 1);
    }
}

// 0x6a7f0
void __cdecl RenderTube::BuildIndicesWire(int segments, int vertexCount, unsigned short* idx, int unused)
{
    (void)unused;
    int n2 = segments * 2;
    const int limit = n2 - 2;
    int cursor = 0;
    int i = 0;
    const short sn2 = static_cast<short>(n2);
    const short slimit = static_cast<short>(limit);
    if (height <= 0.0f) {
        if (0 < limit) {
            do {
                idx[cursor] = static_cast<unsigned short>(i);
                idx[cursor + 1] = static_cast<unsigned short>(i + 2);
                i += 2;
                cursor += 2;
            } while (i < limit);
        }
        idx[cursor] = static_cast<unsigned short>(i);
        idx[cursor + 1] = 0;
        cursor += 2;
        i = 0;
        if (-1 < limit) {
            do {
                idx[cursor] = static_cast<unsigned short>(vertexCount - 2);
                idx[cursor + 1] = static_cast<unsigned short>(i);
                i += 2;
                cursor += 2;
            } while (i <= limit);
        }
        i = 1;
        if (1 < limit) {
            do {
                idx[cursor] = static_cast<unsigned short>(i);
                idx[cursor + 1] = static_cast<unsigned short>(i + 2);
                i += 2;
                cursor += 2;
            } while (i < limit);
        }
        idx[cursor] = static_cast<unsigned short>(i);
        idx[cursor + 1] = 1;
        cursor += 2;
        i = 1;
        if (0 < n2 - 1) {
            do {
                idx[cursor] = static_cast<unsigned short>(vertexCount - 1);
                idx[cursor + 1] = static_cast<unsigned short>(i);
                i += 2;
                cursor += 2;
            } while (i <= n2 - 1);
        }
        i = 0;
        if (-1 < limit) {
            do {
                idx[cursor] = static_cast<unsigned short>(i);
                idx[cursor + 1] = static_cast<unsigned short>(i + 1);
                i += 2;
                cursor += 2;
            } while (i <= limit);
        }
    } else {
        if (0 < limit) {
            do {
                idx[cursor] = static_cast<unsigned short>(i);
                idx[cursor + 1] = static_cast<unsigned short>(i + 2);
                i += 2;
                cursor += 2;
            } while (i < limit);
        }
        idx[cursor] = static_cast<unsigned short>(i);
        i = limit + n2;
        idx[cursor + 1] = 0;
        int other = n2;
        while (true) {
            const int next = cursor + 2;
            const short s = static_cast<short>(other);
            if (i <= other) break;
            idx[next] = static_cast<unsigned short>(s);
            idx[cursor + 3] = static_cast<unsigned short>(s + 2);
            other += 2;
            cursor = next;
        }
        idx[cursor + 2] = static_cast<unsigned short>(other);
        idx[cursor + 3] = static_cast<unsigned short>(sn2);
        cursor += 4;
        other = 0;
        if (-1 < limit) {
            do {
                idx[cursor] = static_cast<unsigned short>(other + sn2);
                idx[cursor + 1] = static_cast<unsigned short>(other);
                other += 2;
                cursor += 2;
            } while (other <= limit);
        }
        other = 1;
        if (1 < limit) {
            do {
                idx[cursor] = static_cast<unsigned short>(other);
                idx[cursor + 1] = static_cast<unsigned short>(other + 2);
                other += 2;
                cursor += 2;
            } while (other < limit);
        }
        idx[cursor] = static_cast<unsigned short>(other);
        idx[cursor + 1] = 1;
        other = n2 + 1;
        while (true) {
            const int next = cursor + 2;
            const short s = static_cast<short>(other);
            if (i <= other) break;
            idx[next] = static_cast<unsigned short>(s);
            idx[cursor + 3] = static_cast<unsigned short>(s + 2);
            other += 2;
            cursor = next;
        }
        idx[cursor + 2] = static_cast<unsigned short>(other);
        idx[cursor + 3] = static_cast<unsigned short>(sn2 + 1);
        other = 1;
        cursor += 4;
        if (0 < n2 - 1) {
            do {
                idx[cursor] = static_cast<unsigned short>(other + sn2);
                idx[cursor + 1] = static_cast<unsigned short>(other);
                other += 2;
                cursor += 2;
            } while (other <= n2 - 1);
        }
        other = 0;
        if (-1 < limit) {
            do {
                idx[cursor] = static_cast<unsigned short>(other);
                idx[cursor + 1] = static_cast<unsigned short>(other + 1);
                other += 2;
                cursor += 2;
            } while (other <= limit);
        }
        if (n2 <= i) {
            do {
                idx[cursor] = static_cast<unsigned short>(n2);
                idx[cursor + 1] = static_cast<unsigned short>(n2 + 1);
                n2 += 2;
                cursor += 2;
            } while (n2 <= i);
        }
    }
}

}  // namespace Jet

