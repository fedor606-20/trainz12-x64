// Shared unnamed helpers of the U1d2 solids: the table-driven magnitude/normalise pair (0x10008ab0 / 0x10008b50)
// and the x87 float copy. Both helpers read StdMath::s_sqrtLUT, the data export that every Jet module imports
// from KernelStandard_7c; we resolve it once by name so the unit works with the retail module and with our proxy.
#include <jet/Render3DCoreSolids.h>

#include <windows.h>
#include <xmmintrin.h>

#include <cmath>
#include <cstring>

namespace Jet::SolidsDetail {

namespace {

// The original keeps this table in KernelStandard_7c (export ?s_sqrtLUT@StdMath@Jet@@0PAIA). If the export is
// the array itself, use it directly; an older import-link form exports a pointer to the array.
const unsigned int* Lut()
{
    static const unsigned int* table = []() -> const unsigned int* {
        HMODULE module = GetModuleHandleA("KernelStandard_7c.dll");
        if (module == nullptr)
            return nullptr;
        auto symbol = reinterpret_cast<const unsigned int*>(
            GetProcAddress(module, "?s_sqrtLUT@StdMath@Jet@@0PAIA"));
        if (symbol == nullptr)
            return nullptr;
        if (symbol[0] >= 0x800000u) {  // a pointer, not mantissa bits
            const unsigned int* deref = *reinterpret_cast<const unsigned int* const*>(symbol);
            if (deref != nullptr)
                return deref;
        }
        return symbol;
    }();
    return table;
}

float Seed(float value)
{
    unsigned int bits;
    std::memcpy(&bits, &value, sizeof bits);
    const unsigned int exponent = (bits >> 23) & 0xffu;
    const unsigned int index = (bits >> 15) & 0x1ffu;
    unsigned int word = ((0x17cu - exponent) << 22) & 0xff800000u;
    const unsigned int* table = Lut();
    if (table != nullptr)
        word |= table[index];
    // No table (should not happen next to any KernelStandard_7c): a plain seed still leaves the Newton steps in
    // the right basin, only the last bits may differ.
    if (table == nullptr) {
        float rough = 1.0f / std::sqrt(value);
        std::memcpy(&word, &rough, sizeof word);
        word &= 0x807fffffu;
    }
    float result;
    std::memcpy(&result, &word, sizeof result);
    return result;
}

double NewtonInverseSqrt(float squareSum)
{
    const double half = static_cast<double>(squareSum) * 0.5;
    double y = static_cast<double>(Seed(squareSum));
    y = (1.5 - (y * y) * half) * y;
    y = (1.5 - (y * y) * half) * y;
    return y;
}

}  // namespace

// 0x10008ab0: length through the original's x87 sequence (two Newton steps and the final multiply by the sum).
__declspec(noinline) double __cdecl Length(const Vector3& v)
{
    const float sum = AddSS(AddSS(MulSS(v.x, v.x), MulSS(v.y, v.y)), MulSS(v.z, v.z));
    volatile float sumV = sum;
    volatile float seedV = Seed(sum);
    volatile float halfV = 0.5f;
    volatile double threeHalves = 1.5;
    double result;
#if defined(_M_IX86)
    __asm {
        fld dword ptr [sumV]
        fld dword ptr [halfV]
        fmul st, st(1)
        fld dword ptr [seedV]
        fld st(0)
        fmul st, st(1)
        fmul st, st(2)
        fld qword ptr [threeHalves]
        fld st(0)
        fsubrp st(2), st(0)
        fxch st(1)
        fmulp st(2), st(0)
        fld st(1)
        fmul st, st(2)
        fmulp st(3), st(0)
        fsubrp st(2), st(0)
        fmulp st(1), st(0)
        fmulp st(1), st(0)
        fstp qword ptr [result]
    }
#else
    const double half = static_cast<double>(sum) * 0.5;
    double y = static_cast<double>(Seed(sum));
    y = (1.5 - (y * y) * half) * y;
    result = y * (1.5 - (y * y) * half) * static_cast<double>(sum);
#endif
    return result;
}

// 0x10008b50: the original's x87 sequence, reproduced instruction for instruction (the Newton steps are the
// same as Length's but the factor is rounded to float before the components are scaled in SSE).
__declspec(noinline) void __cdecl Normalize(Vector3& v)
{
    const float sum = AddSS(AddSS(MulSS(v.x, v.x), MulSS(v.y, v.y)), MulSS(v.z, v.z));
    volatile float sumV = sum;
    volatile float seedV = Seed(sum);
    volatile float halfV = 0.5f;
    volatile double threeHalves = 1.5;
    float factor;
#if defined(_M_IX86)
    __asm {
        fld dword ptr [sumV]
        fmul dword ptr [halfV]
        fld dword ptr [seedV]
        fld st(0)
        fmul st, st(1)
        fmul st, st(2)
        fld qword ptr [threeHalves]
        fld st(0)
        fsubrp st(2), st(0)
        fxch st(1)
        fmulp st(2), st(0)
        fld st(1)
        fmul st, st(2)
        fmulp st(3), st(0)
        fsubrp st(2), st(0)
        fmulp st(1), st(0)
        fstp dword ptr [factor]
    }
#else
    factor = static_cast<float>(NewtonInverseSqrt(sum));
#endif
    v.x = MulSS(v.x, factor);
    v.y = MulSS(v.y, factor);
    v.z = MulSS(v.z, factor);
}

void __cdecl X87Copy(float& dst, const float& src)
{
#if defined(_M_IX86)
    const float* s = &src;
    float* d = &dst;
    __asm {
        mov ecx, s
        mov eax, d
        fld dword ptr [ecx]
        fstp dword ptr [eax]
    }
#else
    dst = src;
#endif
}

float __cdecl FromBits(unsigned int bits)
{
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

float __cdecl MulSS(float a, float b)
{
    return _mm_cvtss_f32(_mm_mul_ss(_mm_load_ss(&a), _mm_load_ss(&b)));
}

float __cdecl AddSS(float a, float b)
{
    return _mm_cvtss_f32(_mm_add_ss(_mm_load_ss(&a), _mm_load_ss(&b)));
}

float __cdecl SubSS(float a, float b)
{
    return _mm_cvtss_f32(_mm_sub_ss(_mm_load_ss(&a), _mm_load_ss(&b)));
}

float __cdecl DivSS(float a, float b)
{
    return _mm_cvtss_f32(_mm_div_ss(_mm_load_ss(&a), _mm_load_ss(&b)));
}

float __cdecl SqrtF(float a)
{
#if defined(_M_IX86)
    float result;
    __asm {
        fld dword ptr [a]
        fsqrt
        fstp dword ptr [result]
    }
    return result;
#else
    return static_cast<float>(std::sqrt(static_cast<double>(a)));
#endif
}

void* __cdecl OrigProc(const char* mangled)
{
    static HMODULE orig = []() -> HMODULE {
        HMODULE module = GetModuleHandleA("Render3DCore_7c_orig.dll");
        if (module == nullptr)
            module = LoadLibraryA("Render3DCore_7c_orig.dll");
        return module;
    }();
    if (orig == nullptr)
        return nullptr;
    return reinterpret_cast<void*>(GetProcAddress(orig, mangled));
}

}  // namespace Jet::SolidsDetail
