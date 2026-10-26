// Unit U1 of WaterSystem_7c: Perlin noise and fBm turbulence. Behaviour follows the TS12 assembly (addresses
// per function) bit for bit, including the original's quirks: the permutation is 1-based so perm[0] reads
// numOctaves, the last cell's second hash index is byte-truncated (which reads perm[0] again), Turbulence3
// doubles only x and y, and the ctor's table wraps are shifted copies.
//
// The original computes in x87 at the CRT's 53-bit precision (see KernelStandard's Math.cpp): the D helpers work
// in double and round to float only where the assembly stores to a float local.
#include <jet/PNoise.h>

#include <cmath>
#include <cstdlib>  // rand
#include <cstring>  // memcpy

namespace Jet {

namespace {

// 0x10002120
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

        grad2D[2 * i] = RandomComponent();
        grad2D[2 * i + 1] = RandomComponent();
        double len2 = Sqrt(grad2D[2 * i] * grad2D[2 * i] + grad2D[2 * i + 1] * grad2D[2 * i + 1]);
        grad2D[2 * i] *= static_cast<float>(1.0 / len2);
        grad2D[2 * i + 1] *= static_cast<float>(1.0 / len2);

        grad3D[3 * i] = RandomComponent();
        grad3D[3 * i + 1] = RandomComponent();
        grad3D[3 * i + 2] = RandomComponent();
        double len3 = Sqrt((grad3D[3 * i + 2] * grad3D[3 * i + 2] + grad3D[3 * i] * grad3D[3 * i])
                           + grad3D[3 * i + 1] * grad3D[3 * i + 1]);
        grad3D[3 * i] *= static_cast<float>(1.0 / len3);
        grad3D[3 * i + 1] *= static_cast<float>(1.0 / len3);
        grad3D[3 * i + 2] *= static_cast<float>(1.0 / len3);
    }

    // Fisher-Yates over perm[255..1] (perm[0], the numOctaves field, is not shuffled)
    for (int i = 255; i > 0; --i) {
        int j = rand() % 256;
        int t = perm[i];
        perm[i] = perm[j];
        perm[j] = t;
    }

    for (int i = 0; i < 258; ++i) {
        permWrapped[i] = Perm(i);  // Perm(0) is numOctaves, still unwritten: the original reads garbage here
        grad1DWrapped[i] = i < 256 ? grad1D[i] : grad1DWrapped[i - 256];
        grad2DWrapped[2 * i] = grad2D[2 * i];
        grad2DWrapped[2 * i + 1] = grad2D[2 * i + 1];
        grad3DWrapped[3 * i] = grad3D[3 * i];
        grad3DWrapped[3 * i + 1] = grad3D[3 * i + 1];
        grad3DWrapped[3 * i + 2] = grad3D[3 * i + 2];
    }

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
    float xw = static_cast<float>(static_cast<double>(x) + 4096.0);
    int cell = static_cast<unsigned char>(static_cast<int>(xw));
    float xf = xw - static_cast<float>(static_cast<int>(xw));
    float dot0 = grad1D[Perm(cell + 1)] * xf;
    double s = (3.0 - (static_cast<double>(xf) + xf)) * xf;
    double d = (static_cast<double>(xf) - 1.0) * grad1D[Perm(static_cast<unsigned char>(cell + 1))] - dot0;
    return xf * s * d + dot0;
}

float PNoise::Noise1(float x) const
{
    return static_cast<float>(Noise1D(x));
}

// 0x10001570: bilinear over the four surrounding cells
double PNoise::Noise2D(const float* p) const
{
    float xw = static_cast<float>(static_cast<double>(p[0]) + 4096.0);
    int xi = static_cast<int>(xw);
    float xf = xw - static_cast<float>(xi);
    float yw = static_cast<float>(static_cast<double>(p[1]) + 4096.0);
    int yi = static_cast<int>(yw);
    float yf = yw - static_cast<float>(yi);
    int cellX = static_cast<unsigned char>(xi);
    int cellY = static_cast<unsigned char>(yi);

    const float* g00 = Grad2(Perm(Perm(cellX + 1) + cellY + 1));
    const float* g10 = Grad2(Perm(Perm(static_cast<unsigned char>(cellX + 1) + 1) + cellY + 1));
    const float* g01 = Grad2(Perm(Perm(cellX + 1) + static_cast<unsigned char>(cellY + 1) + 1));
    const float* g11 = Grad2(Perm(Perm(static_cast<unsigned char>(cellX + 1) + 1)
                                   + static_cast<unsigned char>(cellY + 1) + 1));

    float dot00 = g00[1] * yf + g00[0] * xf;
    float dot10y = g10[1] * yf;  // + g10[0]*(xf-1) below, in the original's order
    float dot01 = g01[1] * (yf - 1.0f) + g01[0] * xf;
    double sx = (3.0 - (static_cast<double>(xf) * 2.0)) * xf * xf;
    double row0 = (dot10y + g10[0] * (xf - 1.0f) - dot00) * sx + dot00;
    double row1 = g11[1] * (yf - 1.0f) + g11[0] * (xf - 1.0f) - dot01;
    double sy = yf * ((3.0 - (static_cast<double>(yf) + yf)) * yf);
    return (dot01 + row1 * sx - row0) * sy + row0;
}

float PNoise::Noise2(const float* p) const
{
    return static_cast<float>(Noise2D(p));
}

// 0x10001700: trilinear over the eight surrounding cells (z hashes the gradient index directly)
double PNoise::Noise3D(const float* p) const
{
    float xw = static_cast<float>(static_cast<double>(p[0]) + 4096.0);
    int xi = static_cast<int>(xw);
    float xf = xw - static_cast<float>(xi);
    float yw = static_cast<float>(static_cast<double>(p[1]) + 4096.0);
    int yi = static_cast<int>(yw);
    float yf = yw - static_cast<float>(yi);
    float zw = static_cast<float>(static_cast<double>(p[2]) + 4096.0);
    int zi = static_cast<int>(zw);
    float zf = zw - static_cast<float>(zi);
    int cellX = static_cast<unsigned char>(xi);
    int cellY = static_cast<unsigned char>(yi);
    int cellZ = static_cast<unsigned char>(zi);

    int h0 = Perm(cellX + 1);
    int h1 = Perm(static_cast<unsigned char>(cellX + 1) + 1);
    int cellY1 = static_cast<unsigned char>(cellY + 1);
    int h00 = Perm(cellY + h0 + 1);
    int h10 = Perm(cellY + h1 + 1);
    int h01 = Perm(cellY1 + h0 + 1);
    int h11 = Perm(cellY1 + h1 + 1);
    int cellZ1 = static_cast<unsigned char>(cellZ + 1);

    const float* g000 = Grad3(h00 + cellZ);
    const float* g100 = Grad3(h10 + cellZ);
    const float* g010 = Grad3(h01 + cellZ);
    const float* g110 = Grad3(h11 + cellZ);
    const float* g001 = Grad3(h00 + cellZ1);
    const float* g101 = Grad3(h10 + cellZ1);
    const float* g011 = Grad3(h01 + cellZ1);
    const float* g111 = Grad3(h11 + cellZ1);

    float xf1 = xf - 1.0f;
    float yf1 = yf - 1.0f;
    float sx = static_cast<float>((3.0 - (static_cast<double>(xf) * 2.0)) * xf * xf);
    float sy = static_cast<float>((3.0 - (static_cast<double>(yf) * 2.0)) * yf * yf);

    float dot000 = g000[2] * zf + g000[1] * yf + xf * g000[0];
    float dot100 = g100[2] * zf + g100[1] * yf + xf1 * g100[0];
    float dot010 = g010[2] * zf + g010[1] * yf1 + xf * g010[0];
    float dot110 = g110[2] * zf + g110[1] * yf1 + xf1 * g110[0];
    double row0z0 = (dot100 - dot000) * sx + dot000;
    float row1z0 = static_cast<float>((dot110 - dot010) * sx + dot010);
    float dot001 = g001[2] * (zf - 1.0f) + g001[1] * yf + xf * g001[0];
    float dot101 = g101[2] * (zf - 1.0f) + g101[1] * yf + xf1 * g101[0];
    float dot011 = g011[2] * (zf - 1.0f) + g011[1] * yf1 + xf * g011[0];
    float dot111 = g111[2] * (zf - 1.0f) + g111[1] * yf1 + xf1 * g111[0];
    double row0z1 = (dot101 - dot001) * sx + dot001;
    double sz = zf * ((3.0 - (static_cast<double>(zf) + zf)) * zf);
    return (row0z1 + (dot011 + (dot111 - dot011) * sx - row0z1) * sy - row1z0) * sz + row1z0;
}

float PNoise::Noise3(const float* p) const
{
    return static_cast<float>(Noise3D(p));
}

// 0x10001A70: fractal sum, frequency doubled and amplitude halved per octave (the noise body is inlined)
double PNoise::Turbulence1D(float x) const
{
    float sum = 0.0f;
    float amp = 1.0f;
    for (int i = numOctaves; i > 0; --i) {
        float xw = static_cast<float>(static_cast<double>(x) + 4096.0);
        int cell = static_cast<unsigned char>(static_cast<int>(xw));
        float xf = xw - static_cast<float>(static_cast<int>(xw));
        float dot0 = grad1D[Perm(cell + 1)] * xf;
        double n = ((static_cast<double>(xf) - 1.0) * grad1D[Perm(static_cast<unsigned char>(cell + 1))] - dot0)
                   * ((3.0 - (static_cast<double>(xf) * 2.0)) * xf * xf) + dot0;
        sum = static_cast<float>(n * amp) + sum;
        amp *= 0.5f;
        x *= 2.0f;
    }
    return sum;
}

float PNoise::Turbulence1(float x) const
{
    return static_cast<float>(Turbulence1D(x));
}

// 0x10001B50
double PNoise::Turbulence2D(const float* p) const
{
    float sum = 0.0f;
    float amp = 1.0f;
    for (int i = 0; i < numOctaves; ++i) {
        sum = static_cast<float>(Noise2D(p) * amp + sum);
        amp *= 0.5f;
        const_cast<float*>(p)[0] *= 2.0f;
        const_cast<float*>(p)[1] *= 2.0f;
    }
    return sum;
}

float PNoise::Turbulence2(const float* p) const
{
    return static_cast<float>(Turbulence2D(p));
}

// 0x10001CC0
float PNoise::Turbulence2(float x, float y) const
{
    float p[2] = {x, y};
    return Turbulence2(p);
}

// 0x10001BE0: like Turbulence2, but z stays put (the original's quirk)
double PNoise::Turbulence3D(const float* p) const
{
    float sum = 0.0f;
    float amp = 1.0f;
    for (int i = 0; i < numOctaves; ++i) {
        sum = static_cast<float>(Noise3D(p) * amp + sum);
        amp *= 0.5f;
        const_cast<float*>(p)[0] *= 2.0f;
        const_cast<float*>(p)[1] *= 2.0f;
    }
    return sum;
}

float PNoise::Turbulence3(const float* p) const
{
    return static_cast<float>(Turbulence3D(p));
}

// 0x10001C80
float PNoise::Turbulence3(float x, float y, float z) const
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

float PNoise::TurbulenceTiled(float x, float y, float z, float period) const
{
    return static_cast<float>(TurbulenceTiledD(x, y, z, period));
}

// 0x10001DF0: two tile samples cross-blended so the noise also loops every loopPeriod
double PNoise::TurbulenceLoopingTiledD(float x, float y, float z, float period, float loopPeriod) const
{
    float a = static_cast<float>(TurbulenceTiledD(x, y, z - loopPeriod, period) * z);
    return (TurbulenceTiledD(x, y, z, period) * (loopPeriod - z) + a) / loopPeriod;
}

float PNoise::TurbulenceLoopingTiled(float x, float y, float z, float period, float loopPeriod) const
{
    return static_cast<float>(TurbulenceLoopingTiledD(x, y, z, period, loopPeriod));
}

}  // namespace Jet
