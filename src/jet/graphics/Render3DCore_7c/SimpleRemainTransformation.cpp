// Render3DCore_7c wave 6: Jet::Transformation (0x30 bytes), transcribed from the TS12 assembly.
//
// Addresses: 0x10003e70 default ctor, 0x10003ec0 (Matrix3, Vector3) ctor, 0x100023050 assignment (folded with
// Ellipse's), 0x10004180 Transform, 0x10004280 InverseTransform, 0x10004340 MultiplyTransform, 0x10004450
// operator*, 0x10003f20 operator*=, 0x100045a0 TransformArray. The 3x3 multiply helper is 0x10008c10.
//
// The original is SSE scalar throughout (no x87 in the arithmetic); operand order and grouping are kept so the
// last bit and a NaN payload match. Matrix3 is row-major and a vector is a row vector: Transform is v * M + t.
#include "SimpleRemain.h"

namespace Jet {

namespace {

using namespace SolidsDetail;

Matrix3 Multiply3(const Matrix3& a, const Matrix3& b)
{
    const float* A = a.m;
    const float* B = b.m;
    Matrix3 out;
    out.m[0] = AddSS(AddSS(MulSS(A[0], B[0]), MulSS(A[1], B[3])), MulSS(A[2], B[6]));
    out.m[1] = AddSS(AddSS(MulSS(A[0], B[1]), MulSS(A[1], B[4])), MulSS(A[2], B[7]));
    out.m[2] = AddSS(AddSS(MulSS(A[0], B[2]), MulSS(A[1], B[5])), MulSS(A[2], B[8]));
    out.m[3] = AddSS(AddSS(MulSS(A[3], B[0]), MulSS(A[4], B[3])), MulSS(A[5], B[6]));
    out.m[4] = AddSS(AddSS(MulSS(A[3], B[1]), MulSS(A[4], B[4])), MulSS(A[5], B[7]));
    out.m[5] = AddSS(AddSS(MulSS(A[3], B[2]), MulSS(A[4], B[5])), MulSS(A[5], B[8]));
    out.m[6] = AddSS(AddSS(MulSS(A[6], B[0]), MulSS(A[7], B[3])), MulSS(A[8], B[6]));
    out.m[7] = AddSS(AddSS(MulSS(A[6], B[1]), MulSS(A[7], B[4])), MulSS(A[8], B[7]));
    out.m[8] = AddSS(AddSS(MulSS(A[6], B[2]), MulSS(A[7], B[5])), MulSS(A[8], B[8]));
    return out;
}

// this->rotation * v, the exact grouping of the translation update in operator*/operator*=:
// x = (m3*y + m6*z) + m0*x, y = (m1*x + m4*y) + m7*z, z = (m2*x + m5*y) + m8*z.
Vector3 Rotate(const Matrix3& m, const Vector3& v)
{
    Vector3 out;
    out.x = AddSS(AddSS(MulSS(m.m[3], v.y), MulSS(m.m[6], v.z)), MulSS(v.x, m.m[0]));
    out.y = AddSS(AddSS(MulSS(m.m[1], v.x), MulSS(m.m[4], v.y)), MulSS(m.m[7], v.z));
    out.z = AddSS(AddSS(MulSS(m.m[2], v.x), MulSS(m.m[5], v.y)), MulSS(m.m[8], v.z));
    return out;
}

}  // namespace

Transformation::Transformation()
{
    rotation.m[0] = 1.0f;
    rotation.m[1] = 0.0f;
    rotation.m[2] = 0.0f;
    rotation.m[3] = 0.0f;
    rotation.m[4] = 1.0f;
    rotation.m[5] = 0.0f;
    rotation.m[6] = 0.0f;
    rotation.m[7] = 0.0f;
    rotation.m[8] = 1.0f;
    translation.x = 0.0f;
    translation.y = 0.0f;
    translation.z = 0.0f;
}

Transformation::Transformation(const Matrix3& rotation_, const Vector3& translation_)
{
    for (int i = 0; i < 9; ++i) X87Copy(rotation.m[i], rotation_.m[i]);
    X87Copy(translation.x, translation_.x);
    X87Copy(translation.y, translation_.y);
    X87Copy(translation.z, translation_.z);
}

Transformation& Transformation::operator=(const Transformation& o)
{
    for (int i = 0; i < 9; ++i) X87Copy(rotation.m[i], o.rotation.m[i]);
    X87Copy(translation.x, o.translation.x);
    X87Copy(translation.y, o.translation.y);
    X87Copy(translation.z, o.translation.z);
    return *this;
}

Vector3 __cdecl Transformation::Transform(const Vector3& v) const
{
    Vector3 out;
    out.x = AddSS(MulSS(rotation.m[3], v.y), MulSS(rotation.m[6], v.z));
    out.x = AddSS(out.x, MulSS(v.x, rotation.m[0]));
    out.y = AddSS(MulSS(rotation.m[1], v.x), MulSS(rotation.m[4], v.y));
    out.y = AddSS(out.y, MulSS(rotation.m[7], v.z));
    out.z = AddSS(MulSS(rotation.m[2], v.x), MulSS(rotation.m[5], v.y));
    out.z = AddSS(out.z, MulSS(rotation.m[8], v.z));
    out.x = AddSS(translation.x, out.x);
    out.y = AddSS(translation.y, out.y);
    out.z = AddSS(translation.z, out.z);
    return out;
}

Vector3 __cdecl Transformation::InverseTransform(const Vector3& v) const
{
    const float dx = SubSS(v.x, translation.x);
    const float dy = SubSS(v.y, translation.y);
    const float dz = SubSS(v.z, translation.z);
    Vector3 out;
    out.x = AddSS(AddSS(MulSS(rotation.m[1], dy), MulSS(rotation.m[2], dz)), MulSS(rotation.m[0], dx));
    out.y = AddSS(AddSS(MulSS(rotation.m[3], dx), MulSS(rotation.m[4], dy)), MulSS(rotation.m[5], dz));
    out.z = AddSS(AddSS(MulSS(rotation.m[6], dx), MulSS(rotation.m[7], dy)), MulSS(rotation.m[8], dz));
    return out;
}

void __cdecl Transformation::MultiplyTransform(const Matrix3& rotation_, const Vector3& translation_,
                                               Transformation& result) const
{
    const Vector3 rotated = Rotate(rotation, translation_);
    result.translation.x = AddSS(translation.x, rotated.x);
    result.translation.y = AddSS(translation.y, rotated.y);
    result.translation.z = AddSS(translation.z, rotated.z);
    const Matrix3 m = Multiply3(rotation_, rotation);
    for (int i = 0; i < 9; ++i) X87Copy(result.rotation.m[i], m.m[i]);
}

Transformation __cdecl Transformation::operator*(const Transformation& o)
{
    const Vector3 rotated = Rotate(rotation, o.translation);
    Transformation result;
    result.translation.x = AddSS(translation.x, rotated.x);
    result.translation.y = AddSS(translation.y, rotated.y);
    result.translation.z = AddSS(translation.z, rotated.z);
    const Matrix3 m = Multiply3(o.rotation, rotation);
    for (int i = 0; i < 9; ++i) X87Copy(result.rotation.m[i], m.m[i]);
    return result;
}

Transformation& __cdecl Transformation::operator*=(const Transformation& o)
{
    const float* A = o.rotation.m;  // rhs
    const float* B = rotation.m;    // this (old)
    // translation first, with the inline grouping (m0*tx + m3*ty) + m6*tz
    const Vector3 oldT = translation;
    const float tx = AddSS(AddSS(MulSS(A[9], B[0]), MulSS(B[3], A[10])), MulSS(A[11], B[6]));
    const float ty = AddSS(AddSS(MulSS(B[1], A[9]), MulSS(B[4], A[10])), MulSS(B[7], A[11]));
    const float tz = AddSS(AddSS(MulSS(A[9], B[2]), MulSS(A[10], B[5])), MulSS(A[11], B[8]));
    translation.x = AddSS(oldT.x, tx);
    translation.y = AddSS(oldT.y, ty);
    translation.z = AddSS(oldT.z, tz);
    const float n0 = AddSS(AddSS(MulSS(A[2], B[6]), MulSS(A[1], B[3])), MulSS(A[0], B[0]));
    const float n1 = AddSS(AddSS(MulSS(A[2], B[7]), MulSS(A[1], B[4])), MulSS(A[0], B[1]));
    const float n2 = AddSS(AddSS(MulSS(A[2], B[8]), MulSS(A[1], B[5])), MulSS(A[0], B[2]));
    const float n3 = AddSS(AddSS(MulSS(A[3], B[0]), MulSS(A[5], B[6])), MulSS(A[4], B[3]));
    const float n4 = AddSS(AddSS(MulSS(A[3], B[1]), MulSS(A[5], B[7])), MulSS(A[4], B[4]));
    const float n5 = AddSS(AddSS(MulSS(A[3], B[2]), MulSS(A[5], B[8])), MulSS(A[4], B[5]));
    const float n6 = AddSS(AddSS(MulSS(A[6], B[0]), MulSS(A[8], B[6])), MulSS(A[7], B[3]));
    const float n7 = AddSS(AddSS(MulSS(A[6], B[1]), MulSS(A[8], B[7])), MulSS(A[7], B[4]));
    const float n8 = AddSS(AddSS(MulSS(A[6], B[2]), MulSS(A[8], B[8])), MulSS(A[7], B[5]));
    rotation.m[0] = n0;
    rotation.m[1] = n1;
    rotation.m[2] = n2;
    rotation.m[3] = n3;
    rotation.m[4] = n4;
    rotation.m[5] = n5;
    rotation.m[6] = n6;
    rotation.m[7] = n7;
    rotation.m[8] = n8;
    return *this;
}

void __cdecl Transformation::TransformArray(int count, const Vector3* in, Vector3* out)
{
    // The original unrolls by four and uses its own grouping in the main loop (the same one as Transform); the
    // tail loop groups the terms differently. Both are reproduced so an array of any length matches to the bit.
    int i = 0;
    for (; i + 4 <= count; i += 4) {
        for (int k = 0; k < 4; ++k) {
            const Vector3& v = in[i + k];
            if (k == 1) {
                out[i + k].x = AddSS(AddSS(MulSS(rotation.m[6], v.z), MulSS(v.y, rotation.m[3])),
                                     MulSS(rotation.m[0], v.x));
                out[i + k].y = AddSS(AddSS(MulSS(v.y, rotation.m[4]), MulSS(rotation.m[1], v.x)),
                                     MulSS(v.z, rotation.m[7]));
                out[i + k].z = AddSS(AddSS(MulSS(rotation.m[2], v.x), MulSS(v.y, rotation.m[5])),
                                     MulSS(rotation.m[8], v.z));
            } else {
                out[i + k].x = AddSS(AddSS(MulSS(rotation.m[6], v.z), MulSS(rotation.m[3], v.y)),
                                     MulSS(v.x, rotation.m[0]));
                out[i + k].y = AddSS(AddSS(MulSS(rotation.m[1], v.x), MulSS(v.y, rotation.m[4])),
                                     MulSS(v.z, rotation.m[7]));
                out[i + k].z = AddSS(AddSS(MulSS(rotation.m[5], v.y), MulSS(rotation.m[8], v.z)),
                                     MulSS(rotation.m[2], v.x));
            }
            out[i + k].x = AddSS(out[i + k].x, translation.x);
            out[i + k].y = AddSS(out[i + k].y, translation.y);
            out[i + k].z = AddSS(out[i + k].z, translation.z);
        }
    }
    for (; i < count; ++i) {
        const Vector3& v = in[i];
        // tail grouping: x = (x*m0 + z*m6) + y*m3, y = (z*m7 + y*m4) + x*m1, z = (z*m8 + y*m5) + x*m2
        float x = AddSS(MulSS(v.x, rotation.m[0]), MulSS(v.z, rotation.m[6]));
        x = AddSS(x, MulSS(v.y, rotation.m[3]));
        float y = AddSS(MulSS(v.z, rotation.m[7]), MulSS(v.y, rotation.m[4]));
        y = AddSS(y, MulSS(v.x, rotation.m[1]));
        float z = AddSS(MulSS(v.z, rotation.m[8]), MulSS(v.y, rotation.m[5]));
        z = AddSS(z, MulSS(rotation.m[2], v.x));
        out[i].x = AddSS(x, translation.x);
        out[i].y = AddSS(y, translation.y);
        out[i].z = AddSS(z, translation.z);
    }
}

}  // namespace Jet
