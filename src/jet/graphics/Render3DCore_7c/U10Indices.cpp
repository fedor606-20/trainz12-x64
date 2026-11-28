// Unit U10 remainder: the shape index builders. These are pure array writers: they take the segment counts the
// geometry builders computed and fill the chunk's index array. Transcribed from the TS12 assembly/decompile
// (notes/agents/r3d_workstreams/u10_primitives_asm.txt); every loop bound and 16-bit truncation is the original's.
//
// The last `vertexCount` argument is unused by the original builders; the counts they need arrive packed in the
// segment arguments (the sphere/ellipsoid builders use the second one as the vertex base).
#include <jet/Render3DCorePrimitives.h>

namespace Jet {

namespace {

// 0x69550: shared by RenderSphere and RenderEllipsoid
void SphereSolid(int a, int b, unsigned short* idx)
{
    const short as = static_cast<short>(a);
    const int half = a / 2;
    int cursor = 0;
    int s = 0;
    if (a - 1 > 0) {
        do {
            idx[cursor] = 0;
            idx[cursor + 1] = static_cast<unsigned short>(s + 1);
            idx[cursor + 2] = static_cast<unsigned short>(s + 2);
            ++s;
            cursor += 3;
        } while (s < a - 1);
    }
    idx[cursor] = 0;
    idx[cursor + 1] = static_cast<unsigned short>(s + 1);
    idx[cursor + 2] = 1;
    s = 1;
    cursor += 3;
    if (1 < half - 1) {
        int rows = a;
        int n = half - 2;
        do {
            ++rows;
            for (int left = a - 1; left > 0; --left) {
                const short r1 = static_cast<short>(rows);
                const short r2 = static_cast<short>(rows + 1);
                idx[cursor] = static_cast<unsigned short>(s);
                idx[cursor + 1] = static_cast<unsigned short>(r1);
                idx[cursor + 2] = static_cast<unsigned short>(r2);
                idx[cursor + 3] = static_cast<unsigned short>(s);
                idx[cursor + 4] = static_cast<unsigned short>(r2);
                idx[cursor + 5] = static_cast<unsigned short>(s + 1);
                cursor += 6;
                ++s;
                ++rows;
            }
            const short r1 = static_cast<short>(rows);
            const short t = static_cast<short>(static_cast<short>(rows) - as + 1);
            idx[cursor] = static_cast<unsigned short>(s);
            idx[cursor + 1] = static_cast<unsigned short>(r1);
            idx[cursor + 2] = static_cast<unsigned short>(t);
            idx[cursor + 3] = static_cast<unsigned short>(s);
            idx[cursor + 4] = static_cast<unsigned short>(t);
            idx[cursor + 5] = static_cast<unsigned short>(static_cast<short>(s) - as + 1);
            cursor += 6;
            ++s;
            --n;
        } while (n != 0);
    }
    for (int left = a - 1; left > 0; --left) {
        idx[cursor] = static_cast<unsigned short>(s);
        idx[cursor + 1] = static_cast<unsigned short>(b - 1);
        idx[cursor + 2] = static_cast<unsigned short>(s + 1);
        cursor += 3;
        ++s;
    }
    idx[cursor] = static_cast<unsigned short>(s);
    idx[cursor + 1] = static_cast<unsigned short>(b - 1);
    idx[cursor + 2] = static_cast<unsigned short>(static_cast<short>(s) - as + 1);
}

// 0x696d0: shared by RenderSphere and RenderEllipsoid
void SphereWire(int a, int b, unsigned short* idx)
{
    const int half = a / 2;
    const int top = half - 1;
    int cursor = 0;
    short s = 0;
    idx[cursor] = 0;
    if (top > 0) {
        int n = top;
        do {
            idx[++cursor] = static_cast<unsigned short>(++s);
            for (int left = a - 1; left > 0; --left) idx[++cursor] = static_cast<unsigned short>(++s);
            idx[++cursor] = static_cast<unsigned short>((s - a) + 1);
            --n;
        } while (n != 0);
    }
    const short bottom = static_cast<short>(b - 1);
    idx[++cursor] = static_cast<unsigned short>(bottom);
    ++cursor;
    if (a > 1) {
        int start1 = b - 2;
        int start2 = a - 1;
        int e = 1;
        do {
            int value = start1;
            for (int i = 0; i < top; ++i) {
                idx[cursor++] = static_cast<unsigned short>(value);
                value -= a;
            }
            idx[cursor++] = 0;
            if (e >= a - 1) break;
            value = start2;
            for (int i = 0; i < top; ++i) {
                idx[cursor++] = static_cast<unsigned short>(value);
                value += a;
            }
            idx[cursor++] = static_cast<unsigned short>(bottom);
            e += 2;
            start1 -= 2;
            start2 -= 2;
        } while (e < a);
    }
}

// 0x63480
void ConeSolid(int a, int b, unsigned short* idx)
{
    const short s3 = static_cast<short>(b - 2);
    const short s4 = static_cast<short>(b - 1);
    const int n = a - 1;
    int cursor = 0;
    int i = 0;
    if (n > 0) {
        do {
            idx[cursor] = static_cast<unsigned short>(s3);
            idx[cursor + 1] = static_cast<unsigned short>(i);
            idx[cursor + 2] = static_cast<unsigned short>(i + 1);
            ++i;
            cursor += 3;
        } while (i < n);
    }
    idx[cursor] = static_cast<unsigned short>(s3);
    idx[cursor + 1] = static_cast<unsigned short>(i);
    idx[cursor + 2] = 0;
    cursor += 3;
    i = 0;
    if (n > 0) {
        do {
            idx[cursor] = static_cast<unsigned short>(s4);
            idx[cursor + 1] = static_cast<unsigned short>(i + 1);
            idx[cursor + 2] = static_cast<unsigned short>(i);
            ++i;
            cursor += 3;
        } while (i < n);
    }
    idx[cursor] = static_cast<unsigned short>(s4);
    idx[cursor + 1] = 0;
    idx[cursor + 2] = static_cast<unsigned short>(i);
}

// 0x63520
void ConeWire(int a, int b, unsigned short* idx)
{
    int cursor = 0;
    int i = 0;
    if (a - 1 > 0) {
        do {
            idx[cursor] = static_cast<unsigned short>(i);
            idx[cursor + 1] = static_cast<unsigned short>(i + 1);
            ++i;
            cursor += 2;
        } while (i < a - 1);
    }
    idx[cursor] = static_cast<unsigned short>(i);
    idx[cursor + 1] = 0;
    cursor += 2;
    i = 0;
    if (a > 0) {
        do {
            idx[cursor] = static_cast<unsigned short>(b - 2);
            idx[cursor + 1] = static_cast<unsigned short>(i);
            ++i;
            cursor += 2;
        } while (i < a);
    }
    i = 0;
    if (a > 0) {
        do {
            idx[cursor] = static_cast<unsigned short>(b - 1);
            idx[cursor + 1] = static_cast<unsigned short>(i);
            ++i;
            cursor += 2;
        } while (i < a);
    }
}

// 0x641a0
void CylinderSolid(int b, unsigned short* idx)
{
    const short s4 = static_cast<short>(b - 2);
    const short s5 = static_cast<short>(b - 1);
    const int lim = b - 4;
    int cursor = 0;
    int i = 0;
    if (lim > 0) {
        do {
            idx[cursor] = static_cast<unsigned short>(s4);
            idx[cursor + 1] = static_cast<unsigned short>(i);
            idx[cursor + 2] = static_cast<unsigned short>(i + 2);
            idx[cursor + 3] = static_cast<unsigned short>(s5);
            idx[cursor + 4] = static_cast<unsigned short>(i + 3);
            idx[cursor + 5] = static_cast<unsigned short>(i + 1);
            i += 2;
            cursor += 6;
        } while (i < lim);
    }
    idx[cursor] = static_cast<unsigned short>(s4);
    idx[cursor + 1] = static_cast<unsigned short>(i);
    idx[cursor + 2] = 0;
    idx[cursor + 3] = static_cast<unsigned short>(s5);
    idx[cursor + 4] = 1;
    idx[cursor + 5] = static_cast<unsigned short>(i + 1);
    cursor += 6;
    i = 0;
    if (lim > 0) {
        do {
            const short s = static_cast<short>(i);
            idx[cursor] = static_cast<unsigned short>(s);
            idx[cursor + 1] = static_cast<unsigned short>(s + 1);
            idx[cursor + 2] = static_cast<unsigned short>(s + 3);
            idx[cursor + 3] = static_cast<unsigned short>(s);
            idx[cursor + 4] = static_cast<unsigned short>(s + 3);
            idx[cursor + 5] = static_cast<unsigned short>(s + 2);
            i += 2;
            cursor += 6;
        } while (i < lim);
    }
    {
        const short s = static_cast<short>(i);
        idx[cursor] = static_cast<unsigned short>(s);
        idx[cursor + 1] = static_cast<unsigned short>(s + 1);
        idx[cursor + 2] = 1;
        idx[cursor + 3] = static_cast<unsigned short>(s);
        idx[cursor + 4] = 1;
        idx[cursor + 5] = 0;
    }
}

// 0x642b0
void CylinderWire(int a, int b, unsigned short* idx)
{
    const int lim = a * 2 - 2;
    int i = 0;
    int cursor = 0;
    if (lim > 0) {
        do {
            idx[cursor] = static_cast<unsigned short>(i);
            idx[cursor + 1] = static_cast<unsigned short>(i + 2);
            i += 2;
            cursor += 2;
        } while (i < lim);
    }
    idx[cursor] = static_cast<unsigned short>(i);
    idx[cursor + 1] = 0;
    cursor += 2;
    i = 0;
    if (lim >= -1) {
        do {
            idx[cursor] = static_cast<unsigned short>(b - 2);
            idx[cursor + 1] = static_cast<unsigned short>(i);
            i += 2;
            cursor += 2;
        } while (i <= lim);
    }
    i = 1;
    if (lim > 1) {
        do {
            idx[cursor] = static_cast<unsigned short>(i);
            idx[cursor + 1] = static_cast<unsigned short>(i + 2);
            i += 2;
            cursor += 2;
        } while (i < lim);
    }
    idx[cursor] = static_cast<unsigned short>(i);
    idx[cursor + 1] = 1;
    cursor += 2;
    int i2 = 1;
    const int end = a * 2 - 1;
    if (end > 0) {
        do {
            idx[cursor] = static_cast<unsigned short>(b - 1);
            idx[cursor + 1] = static_cast<unsigned short>(i2);
            i2 += 2;
            cursor += 2;
        } while (i2 <= end);
    }
    i = 0;
    if (lim >= -1) {
        do {
            idx[cursor] = static_cast<unsigned short>(i);
            idx[cursor + 1] = static_cast<unsigned short>(i + 1);
            i += 2;
            cursor += 2;
        } while (i <= lim);
    }
}

// 0x679d0
void PlaneSolid(int x, int y, unsigned short* idx)
{
    const short step = static_cast<short>(x + 1);
    int cursor = 0;
    int rows = y;
    int base = 0;
    if (y > 0) {
        do {
            int left = x;
            int b = base;
            if (x > 0) {
                do {
                    const short s = static_cast<short>(b);
                    const short s2 = static_cast<short>(s + 1 + step);
                    idx[cursor] = static_cast<unsigned short>(s);
                    idx[cursor + 1] = static_cast<unsigned short>(s + step);
                    idx[cursor + 2] = static_cast<unsigned short>(s2);
                    idx[cursor + 3] = static_cast<unsigned short>(s);
                    idx[cursor + 4] = static_cast<unsigned short>(s2);
                    idx[cursor + 5] = static_cast<unsigned short>(s + 1);
                    cursor += 6;
                    --left;
                    ++b;
                } while (left != 0);
            }
            base += x + 1;
            --rows;
        } while (rows != 0);
    }
}

// 0x67a60
void PlaneWire(int x, int y, unsigned short* idx)
{
    int cursor = 0;
    short s = 0;
    if (x >= 0) {
        int n = x + 1;
        do {
            idx[cursor] = static_cast<unsigned short>(s);
            idx[cursor + 1] = static_cast<unsigned short>(s + 1);
            cursor += 2;
            s += 2;
            --n;
        } while (n != 0);
    }
    idx[cursor] = 0;
    const short top = static_cast<short>(x * 2);
    idx[cursor + 1] = static_cast<unsigned short>(top);
    cursor += 2;
    int row = x * 2;
    if (y > 1) {
        int n = y - 1;
        do {
            row += 2;
            idx[cursor] = static_cast<unsigned short>(row);
            idx[cursor + 1] = static_cast<unsigned short>(row + 1);
            cursor += 2;
            --n;
        } while (n != 0);
    }
    idx[cursor] = 1;
    idx[cursor + 1] = static_cast<unsigned short>(top + 1);
}

}  // namespace

// ---- RenderBox --------------------------------------------------------------------------------------------------

// 0x624a0  (flags, xSegments, ySegments, zSegments, indices, vertexCount)
void RenderBox::BuildIndicesWire(int a, int b, int c, int /*unused*/, unsigned short* idx, int /*vertexCount*/)
{
    const short xs = static_cast<short>(a);
    const short ys = static_cast<short>(b);
    const short stride = static_cast<short>(static_cast<short>(xs + ys) * static_cast<short>(c) * 2);
    idx[0] = 0;
    idx[1] = static_cast<unsigned short>(xs);
    idx[2] = static_cast<unsigned short>(stride);
    idx[3] = static_cast<unsigned short>(stride + xs);
    idx[4] = static_cast<unsigned short>(xs);
    idx[5] = static_cast<unsigned short>(ys + xs);
    idx[6] = static_cast<unsigned short>(stride + xs);
    idx[7] = static_cast<unsigned short>(ys + stride + xs);
    idx[8] = static_cast<unsigned short>(ys + xs);
    idx[9] = static_cast<unsigned short>(ys + xs * 2);
    idx[10] = static_cast<unsigned short>(ys + stride + xs);
    idx[11] = static_cast<unsigned short>(ys + xs * 2 + stride);
    idx[12] = static_cast<unsigned short>(ys + xs * 2);
    idx[13] = 0;
    idx[14] = static_cast<unsigned short>(ys + xs * 2 + stride);
    idx[15] = static_cast<unsigned short>(stride);
    idx[16] = 0;
    idx[17] = static_cast<unsigned short>(stride);
    idx[18] = static_cast<unsigned short>(xs);
    idx[19] = static_cast<unsigned short>(stride + xs);
    idx[20] = static_cast<unsigned short>(ys + xs);
    idx[21] = static_cast<unsigned short>(ys + stride + xs);
    idx[22] = static_cast<unsigned short>(ys + xs * 2);
    idx[23] = static_cast<unsigned short>(ys + xs * 2 + stride);

    const int ring = (a + b) * 2;
    int cursor = 0x18;
    int tmp = ring;
    int i = 1;
    if (b > 1) {
        do {
            --tmp;
            idx[cursor] = static_cast<unsigned short>(tmp);
            idx[cursor + 1] = static_cast<unsigned short>(xs + i);
            idx[cursor + 2] = static_cast<unsigned short>(tmp);
            idx[cursor + 3] = static_cast<unsigned short>(tmp + stride);
            idx[cursor + 4] = static_cast<unsigned short>(xs + i);
            idx[cursor + 5] = static_cast<unsigned short>(xs + i + stride);
            idx[cursor + 6] = static_cast<unsigned short>(tmp + stride);
            idx[cursor + 7] = static_cast<unsigned short>(xs + i + stride);
            ++i;
            cursor += 8;
        } while (i < b);
    }
    tmp = b - 1 + a * 2;
    i = 1;
    if (a > 1) {
        do {
            idx[cursor] = static_cast<unsigned short>(i);
            idx[cursor + 1] = static_cast<unsigned short>(tmp);
            idx[cursor + 2] = static_cast<unsigned short>(i);
            idx[cursor + 3] = static_cast<unsigned short>(i + stride);
            idx[cursor + 4] = static_cast<unsigned short>(tmp);
            idx[cursor + 5] = static_cast<unsigned short>(tmp + stride);
            idx[cursor + 6] = static_cast<unsigned short>(i + stride);
            idx[cursor + 7] = static_cast<unsigned short>(tmp + stride);
            ++i;
            cursor += 8;
            --tmp;
        } while (i < a);
    }
    if (c > 1) {
        const short k = static_cast<short>(i);
        int s = ring;
        int n = c - 1;
        do {
            const short s0 = static_cast<short>(s);
            const short d = static_cast<short>(b + k + s0);
            const short e = static_cast<short>(b + k * 2 + s0);
            idx[cursor] = static_cast<unsigned short>(s0);
            idx[cursor + 1] = static_cast<unsigned short>(s0 + k);
            idx[cursor + 2] = static_cast<unsigned short>(s0 + k);
            idx[cursor + 3] = static_cast<unsigned short>(d);
            idx[cursor + 4] = static_cast<unsigned short>(d);
            idx[cursor + 5] = static_cast<unsigned short>(e);
            idx[cursor + 6] = static_cast<unsigned short>(e);
            idx[cursor + 7] = static_cast<unsigned short>(s0);
            cursor += 8;
            s += (b + i) * 2;
            --n;
        } while (n != 0);
    }
}

// ---- RenderSphere / RenderEllipsoid ----------------------------------------------------------------------------

void RenderSphere::BuildIndicesSolid(int a, int b, unsigned short* idx, int /*unused*/) { SphereSolid(a, b, idx); }
void RenderSphere::BuildIndicesWire(int a, int b, unsigned short* idx, int /*unused*/) { SphereWire(a, b, idx); }
void RenderEllipsoid::BuildIndicesSolid(int a, int b, unsigned short* idx, int /*unused*/)
{
    SphereSolid(a, b, idx);
}
void RenderEllipsoid::BuildIndicesWire(int a, int b, unsigned short* idx, int /*unused*/)
{
    SphereWire(a, b, idx);
}

// ---- RenderCone ------------------------------------------------------------------------------------------------

void RenderCone::BuildIndicesSolid(int a, int b, unsigned short* idx, int /*unused*/) { ConeSolid(a, b, idx); }
void RenderCone::BuildIndicesWire(int a, int b, unsigned short* idx, int /*unused*/) { ConeWire(a, b, idx); }

// ---- RenderCylinder --------------------------------------------------------------------------------------------

void RenderCylinder::BuildIndicesSolid(int /*a*/, int b, unsigned short* idx, int /*unused*/)
{
    CylinderSolid(b, idx);
}
void RenderCylinder::BuildIndicesWire(int a, int b, unsigned short* idx, int /*unused*/)
{
    CylinderWire(a, b, idx);
}

// ---- RenderPlane -----------------------------------------------------------------------------------------------

void RenderPlane::BuildIndicesSolid(int x, int y, int /*flags*/, unsigned short* idx, int /*vertexCount*/)
{
    PlaneSolid(x, y, idx);
}
void RenderPlane::BuildIndicesWire(int x, int y, int /*flags*/, unsigned short* idx, int /*vertexCount*/)
{
    PlaneWire(x, y, idx);
}

}  // namespace Jet
