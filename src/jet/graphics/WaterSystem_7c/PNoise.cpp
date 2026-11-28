// Unit U1 of WaterSystem_7c: Perlin noise and fBm turbulence. Behaviour follows the TS12 assembly (addresses
// per function) bit for bit, including the original's quirks: the permutation helpers use 1-based entries (a
// zero entry would read numOctaves), the last cell's second hash index is byte-truncated (so it can reuse
// perm[0]), Turbulence3 doubles only x and y, and the ctor's table wraps repeat the first two entries.
//
// The original computes in x87 at the CRT's 53-bit precision (see KernelStandard's Math.cpp): the D helpers work
// in double and round to float only where the assembly stores to a float local. The noise bodies interleave SSE
// single-precision dots and smoothsteps with x87 doubles, so each store below follows the instruction stream.
#include <jet/PNoise.h>

#include <cmath>
#include <cstdlib>  // rand
#include <cstring>  // memcpy

namespace Jet {

namespace {

// 0x10002120: x87 FSQRT of a float argument; the ctor stores the result back to a float.
double Sqrt(float x)
{
    return std::sqrt(static_cast<double>(x));
}

// a table component: rand() % 512 - 256 scaled to -1..1 (1/256 steps)
float RandomComponent()
{
    return static_cast<float>(rand() % 512 - 256) * 0.00390625f;
}

}  // namespace

// 0x10001E80: 256 random unit gradients, the shuffled permutation, then two copies of every table (the 2D and 3D
// wraps are entry-shifted) so the hash chains can index past the end. 1536 + 255 rand() calls, seed-dependent.
PNoise::PNoise()
{
    for (int i = 0; i < 256; ++i) {
        perm[i] = i;
        grad1D[i] = RandomComponent();

        float x = RandomComponent();
        float y = RandomComponent();
        grad2D[2 * i] = x;
        grad2D[2 * i + 1] = y;
        float len2 = static_cast<float>(Sqrt(x * x + y * y));
        float inv2 = 1.0f / len2;
        grad2D[2 * i] = x * inv2;
        grad2D[2 * i + 1] = y * inv2;

        x = RandomComponent();
        y = RandomComponent();
        float z = RandomComponent();
        grad3D[3 * i] = x;
        grad3D[3 * i + 1] = y;
        grad3D[3 * i + 2] = z;
        float len3 = static_cast<float>(Sqrt(z * z + x * x + y * y));
        float inv3 = 1.0f / len3;
        grad3D[3 * i] = x * inv3;
        grad3D[3 * i + 1] = y * inv3;
        grad3D[3 * i + 2] = z * inv3;
    }

    // Fisher-Yates over the 255 seats perm[255..1], j drawn uniformly from all 256 entries (the original's bias)
    for (int i = 255; i > 0; --i) {
        int j = rand() % 256;
        int t = perm[i];
        perm[i] = perm[j];
        perm[j] = t;
    }

    // The original copies each table with a straight word loop that runs two entries past the source, into the
    // wrapped copy itself: wrapped[i] = base[i] for the first 256 entries, then wrapped[N] and wrapped[N+1]
    // repeat wrapped[0] and wrapped[1] (the copies are entry-shifted by the two extra slots).
    for (int i = 0; i < 258; ++i) {
        permWrapped[i] = i < 256 ? perm[i] : permWrapped[i - 256];
        grad1DWrapped[i] = i < 256 ? grad1D[i] : grad1DWrapped[i - 256];
    }
    for (int i = 0; i < 516; ++i)
        grad2DWrapped[i] = i < 512 ? grad2D[i] : grad2DWrapped[i - 512];
    for (int i = 0; i < 774; ++i)
        grad3DWrapped[i] = i < 768 ? grad3D[i] : grad3DWrapped[i - 768];

    numOctaves = 4;
}

// 0x10091D0 (shared empty body in the original)
PNoise::~PNoise()
{
}

// 0x100014C0
PNoise& PNoise::operator=(const PNoise& o)
{
    std::memcpy(static_cast<void*>(this), static_cast<const void*>(&o), sizeof(PNoise));
    return *this;
}

// 0x100014E0
void PNoise::SetNumOctaves(int n)
{
    numOctaves = n;
}

int PNoise::Perm(int k) const
{
    return k == 0 ? numOctaves : (k <= 256 ? perm[k - 1] : permWrapped[k - 257]);
}

const float* PNoise::Grad2(int entry) const
{
    return &grad2D[2 * entry];
}

const float* PNoise::Grad3(int entry) const
{
    return entry <= 255 ? &grad3D[3 * entry] : &grad3DWrapped[3 * (entry - 256)];
}

// 0x100014F0: lerp(g[cell]*x, g[cell+1]*(x-1), smoothstep(x))
double PNoise::Noise1D(float x) const
{
    float xw = x + 4096.0f;
    int xi = static_cast<int>(xw);
    int cell = xi & 0xff;
    float xf = xw - static_cast<float>(xi);
    float dot0 = grad1D[Perm(cell + 1)] * xf;
    double s = (3.0 - (static_cast<double>(xf) + xf)) * xf;
    double d = (static_cast<double>(xf) - 1.0) * grad1D[Perm(((cell + 1) & 0xff) + 1)] - dot0;
    return xf * s * d + dot0;
}

float PNoise::Noise1(float x)
{
    return static_cast<float>(Noise1D(x));
}

// 0x10001570: bilinear over the four surrounding cells (only the far-corner dot and the y blend are x87)
double PNoise::Noise2D(const float* p) const
{
    float xw = p[0] + 4096.0f;
    int xi = static_cast<int>(xw);
    float xf = xw - static_cast<float>(xi);
    float yw = p[1] + 4096.0f;
    int yi = static_cast<int>(yw);
    float yf = yw - static_cast<float>(yi);
    int cellX = xi & 0xff;
    int cellY = yi & 0xff;
    int cellY1 = (cellY + 1) & 0xff;

    const float* g00 = Grad2(Perm(Perm(cellX + 1) + cellY + 1));
    const float* g10 = Grad2(Perm(Perm(((cellX + 1) & 0xff) + 1) + cellY + 1));
    const float* g01 = Grad2(Perm(Perm(cellX + 1) + cellY1 + 1));
    const float* g11 = Grad2(Perm(Perm(((cellX + 1) & 0xff) + 1) + cellY1 + 1));

    float xf1 = xf - 1.0f;
    float yf1 = yf - 1.0f;
    float sx = (3.0f - xf * 2.0f) * xf * xf;

    float dot00 = g00[1] * yf + g00[0] * xf;
    float dot10 = g10[1] * yf + g10[0] * xf1;
    float dot01 = g01[1] * yf1 + g01[0] * xf;
    float row0 = (dot10 - dot00) * sx + dot00;

    double dot11 = static_cast<double>(g11[1]) * yf1 + static_cast<double>(g11[0]) * xf1;
    double row1 = (dot11 - dot01) * sx + dot01;
    double sy = (3.0 - (static_cast<double>(yf) + yf)) * yf * yf;
    return row0 + (row1 - row0) * sy;
}

float PNoise::Noise2(float* const p)
{
    return static_cast<float>(Noise2D(p));
}

// 0x10001700: trilinear over the eight surrounding cells (z hashes the gradient index directly)
double PNoise::Noise3D(const float* p) const
{
    float xw = p[0] + 4096.0f;
    int xi = static_cast<int>(xw);
    float xf = xw - static_cast<float>(xi);
    float yw = p[1] + 4096.0f;
    int yi = static_cast<int>(yw);
    float yf = yw - static_cast<float>(yi);
    float zw = p[2] + 4096.0f;
    int zi = static_cast<int>(zw);
    float zf = zw - static_cast<float>(zi);
    int cellX = xi & 0xff;
    int cellY = yi & 0xff;
    int cellZ = zi & 0xff;
    int cellY1 = (cellY + 1) & 0xff;
    int cellZ1 = (cellZ + 1) & 0xff;

    int h0 = Perm(cellX + 1);
    int h1 = Perm(((cellX + 1) & 0xff) + 1);
    int p00 = Perm(h0 + cellY + 1);
    int p10 = Perm(h1 + cellY + 1);
    int p01 = Perm(h0 + cellY1 + 1);
    int p11 = Perm(h1 + cellY1 + 1);

    const float* g000 = Grad3(p00 + cellZ);
    const float* g100 = Grad3(p10 + cellZ);
    const float* g010 = Grad3(p01 + cellZ);
    const float* g110 = Grad3(p11 + cellZ);
    const float* g001 = Grad3(p00 + cellZ1);
    const float* g101 = Grad3(p10 + cellZ1);
    const float* g011 = Grad3(p01 + cellZ1);
    const float* g111 = Grad3(p11 + cellZ1);

    float xf1 = xf - 1.0f;
    float yf1 = yf - 1.0f;
    float zf1 = zf - 1.0f;
    float sx = (3.0f - xf * 2.0f) * xf * xf;
    float sy = (3.0f - yf * 2.0f) * yf * yf;

    float dot000 = g000[2] * zf + g000[1] * yf + xf * g000[0];
    float dot100 = g100[2] * zf + g100[1] * yf + xf1 * g100[0];
    float dot010 = g010[2] * zf + g010[1] * yf1 + xf * g010[0];
    float dot110 = g110[2] * zf + g110[1] * yf1 + xf1 * g110[0];
    float dot001 = g001[2] * zf1 + g001[1] * yf + xf * g001[0];
    float dot101 = g101[2] * zf1 + g101[1] * yf + xf1 * g101[0];
    float dot011 = g011[2] * zf1 + g011[1] * yf1 + xf * g011[0];

    float row00 = (dot100 - dot000) * sx + dot000;
    float row10 = (dot110 - dot010) * sx + dot010;
    float row0 = (row10 - row00) * sy + row00;
    float row01 = (dot101 - dot001) * sx + dot001;

    double dot111 = static_cast<double>(g111[2]) * zf1 + static_cast<double>(g111[1]) * yf1
                    + static_cast<double>(g111[0]) * xf1;
    double row11 = (dot111 - dot011) * sx + dot011;
    double row1 = (row11 - row01) * sy + row01;
    double sz = (3.0 - (static_cast<double>(zf) + zf)) * zf * zf;
    return row0 + (row1 - row0) * sz;
}

float PNoise::Noise3(float* const p)
{
    return static_cast<float>(Noise3D(p));
}

// 0x10001A70: fractal sum, frequency doubled and amplitude halved per octave (the noise body is inlined, all SSE)
double PNoise::Turbulence1D(float x) const
{
    float sum = 0.0f;
    float amp = 1.0f;
    for (int i = numOctaves; i > 0; --i) {
        float xw = x + 4096.0f;
        int xi = static_cast<int>(xw);
        int cell = xi & 0xff;
        float xf = xw - static_cast<float>(xi);
        float dot0 = grad1D[Perm(cell + 1)] * xf;
        float n = ((xf - 1.0f) * grad1D[Perm(((cell + 1) & 0xff) + 1)] - dot0)
                      * ((3.0f - xf * 2.0f) * xf * xf)
                  + dot0;
        sum = n * amp + sum;
        amp *= 0.5f;
        x *= 2.0f;
    }
    return sum;
}

float PNoise::Turbulence1(float x)
{
    return static_cast<float>(Turbulence1D(x));
}

// 0x10001B50
double PNoise::Turbulence2D(const float* p) const
{
    float sum = 0.0f;
    float amp = 1.0f;
    double result = 0.0;
    for (int i = 0; i < numOctaves; ++i) {
        // the accumulator is stored back as a float every octave, but the original returns the last x87 sum
        result = Noise2D(p) * amp + sum;
        sum = static_cast<float>(result);
        amp *= 0.5f;
        const_cast<float*>(p)[0] *= 2.0f;
        const_cast<float*>(p)[1] *= 2.0f;
    }
    return result;
}

float PNoise::Turbulence2(float* const p)
{
    return static_cast<float>(Turbulence2D(p));
}

// 0x10001CC0
float PNoise::Turbulence2(float x, float y)
{
    float p[2] = {x, y};
    return Turbulence2(p);
}

// 0x10001BE0: like Turbulence2, but z stays put (the original's quirk)
double PNoise::Turbulence3D(const float* p) const
{
    float sum = 0.0f;
    float amp = 1.0f;
    double result = 0.0;
    for (int i = 0; i < numOctaves; ++i) {
        // as in Turbulence2, only the accumulator rounds to float; the st0 return keeps the x87 sum
        result = Noise3D(p) * amp + sum;
        sum = static_cast<float>(result);
        amp *= 0.5f;
        const_cast<float*>(p)[0] *= 2.0f;
        const_cast<float*>(p)[1] *= 2.0f;
    }
    return result;
}

float PNoise::Turbulence3(float* const p)
{
    return static_cast<float>(Turbulence3D(p));
}

// 0x10001C80
float PNoise::Turbulence3(float x, float y, float z)
{
    float p[3] = {x, y, z};
    return Turbulence3(p);
}

// 0x10001CF0: blends the four samples at x/x-period with period-x weights so the pattern tiles every period
double PNoise::TurbulenceTiledD(float x, float y, float z, float period) const
{
    float wx = period - x;
    float wy = period - y;
    float xyz[3] = {x, y, z};
    float t0 = static_cast<float>(Turbulence3D(xyz) * wx);
    float xwyz[3] = {x - period, y, z};
    float t1 = static_cast<float>((Turbulence3D(xwyz) * x + t0) * wy);
    float xwywz[3] = {x - period, y - period, z};
    float t2 = static_cast<float>(Turbulence3D(xwywz) * x);
    float xywz[3] = {x, y - period, z};
    return ((Turbulence3D(xywz) * wx + t2) * y + t1) / (static_cast<double>(period) * period);
}

float PNoise::TurbulenceTiled(float x, float y, float z, float period)
{
    return static_cast<float>(TurbulenceTiledD(x, y, z, period));
}

// 0x10001DF0: the original's looping blend: the first tile sample is taken at z - loopPeriod, and its result
// is pre-scaled by z into the final x87 sum; only that scaled term is rounded to float.
double PNoise::TurbulenceLoopingTiledD(float x, float y, float z, float period, float loopPeriod) const
{
    double a = TurbulenceTiledD(x, y, z - loopPeriod, period);
    float az = static_cast<float>(a * static_cast<double>(z));
    double b = TurbulenceTiledD(x, y, z, period);
    return (static_cast<double>(az) + b * (static_cast<double>(loopPeriod) - z)) / loopPeriod;
}

float PNoise::TurbulenceLoopingTiled(float x, float y, float z, float period, float loopPeriod)
{
    return static_cast<float>(TurbulenceLoopingTiledD(x, y, z, period, loopPeriod));
}

}  // namespace Jet
