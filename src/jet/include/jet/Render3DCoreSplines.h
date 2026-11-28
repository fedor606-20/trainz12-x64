// Render3DCore_7c, workstream U2: the spline classes, as TS12 lays them out.
//
// Layouts come from the TS12 decompilation/assembly (notes/agents/r3d_workstreams/u2_*): every spline object is a
// vftable followed by one FastDynamicArray (4 bytes, the head chunk) for Spline, by twelve floats (four Vector3
// coefficient vectors) for SplineSegment, and by the type/parameter/point fields for SplineDefinition. Ordinary
// members of the original are __cdecl (the mangled name is QAA and `this` arrives on the stack); constructors,
// assignment and the virtual functions are __thiscall.
//
// The classes allocate through Jet::Alloc (JET_HEAP_OBJECT) because the original's deleting destructors free with
// Jet::Alloc::Delete and the game may delete an object from another module.
#pragma once

#include <jet/FastDynamicArray.h>
#include <jet/Math.h>

namespace Jet {

class Spline;
class SplineSegment;
class ConfigData;
class ResourceDatabase;

typedef FastDynamicArray<SplineSegment*, 13, Less<SplineSegment*, SplineSegment*> > SplineSegmentArray;

// 0x34 bytes: vftable + c0 + c1 + c2 + c3, each a Vector3; position(t) = ((c0*t + c1)*t + c2)*t + c3.
class SplineSegment {
public:
    JET_HEAP_OBJECT
    static void* operator new(size_t, void* p) { return p; }
    static void operator delete(void*, void*) {}
    SplineSegment();
    explicit SplineSegment(const Vector3& v);
    SplineSegment(const SplineSegment& o);
    SplineSegment& operator=(const SplineSegment& o);
    virtual ~SplineSegment();

    void __cdecl Set(const Vector3& v);
    void __cdecl GetPositionByRatio(float ratio, Vector3& out) const;
    Vector3 __cdecl GetPositionByRatio(float ratio) const;

    // Virtual slots 1..3 in the TS12 vftable, in this order.
    virtual void __cdecl GetPositionByLength(float length, Vector3& out, float start, float end,
                                             unsigned int steps) const;
    virtual float __cdecl GetRatio(float length, float start, float end, unsigned int steps) const;
    virtual float __cdecl GetLength(float start, float end, unsigned int steps) const;

    Vector3 __cdecl GetPositionByLength(float length, float start, float end, unsigned int steps) const;
    void __cdecl GetFirstDerivative(float ratio, Vector3& out) const;
    void __cdecl GetSecondDerivative(float ratio, Vector3& out) const;
    void __cdecl GetThirdDerivative(float ratio, Vector3& out) const;
    void __cdecl GetTangent(float ratio, Vector3& out) const;
    void __cdecl GetNormal(float ratio, Vector3& out) const;
    void __cdecl GetBinormal(float ratio, Vector3& out) const;
    unsigned int __cdecl DetermineNumSplineSteps(unsigned int maxSteps, float tolerance, float start, float end) const;
    bool __cdecl IsNullSplineSegment() const;
    const Vector3* __cdecl GetSegmentConstants() const;

protected:
    void __cdecl SetHermiteSplineData(const Vector3& p0, const Vector3& t0, const Vector3& p1, const Vector3& t1);

public:
    Vector3 c0;  // +0x04
    Vector3 c1;  // +0x10
    Vector3 c2;  // +0x1c
    Vector3 c3;  // +0x28
};
JET_LAYOUT32(sizeof(SplineSegment) == 0x34);

// 8 bytes: vftable + FastDynamicArray head. The array holds SplineSegment pointers (not owned copies).
class Spline {
public:
    JET_HEAP_OBJECT
    static void* operator new(size_t, void* p) { return p; }
    static void operator delete(void*, void*) {}
    Spline();
    Spline(const Spline& o);
    Spline& operator=(const Spline& o);
    virtual ~Spline();

    void __cdecl Add(SplineSegment* segment);
    bool __cdecl Remove(const SplineSegment* segment);
    bool __cdecl RemoveDelete(SplineSegment* segment);
    void __cdecl RemoveDeleteAll();

    // Virtual slots 1..2 in the TS12 vftable, in this order.
    virtual void __cdecl GetPositionByRatio(float ratio, Vector3& out) const;
    virtual void __cdecl GetPositionByLength(float length, Vector3& out, unsigned int steps) const;

    Vector3 __cdecl GetPositionByRatio(float ratio) const;
    Vector3 __cdecl GetPositionByLength(float length, unsigned int steps) const;
    float __cdecl GetLength(unsigned int steps) const;
    const SplineSegmentArray& __cdecl GetSplineSegmentArray() const;

    SplineSegmentArray segments;  // +0x04
};
JET_LAYOUT32(sizeof(Spline) == 0x08);

class BSplineSegment : public SplineSegment {
public:
    BSplineSegment(const BSplineSegment& o);
    BSplineSegment(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float tension);
    ~BSplineSegment();
    BSplineSegment& operator=(const BSplineSegment& o);

    void __cdecl Set(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float tension);
};
JET_LAYOUT32(sizeof(BSplineSegment) == 0x34);

class BSpline : public Spline {
public:
    BSpline(const BSpline& o);
    BSpline(unsigned int count, const Vector3* const points, float tension, bool closed);
    BSpline(unsigned int count, const Vector3* const points, float* const tensions, bool closed);
    ~BSpline();
    BSpline& operator=(const BSpline& o);

    void __cdecl Set(unsigned int count, const Vector3* const points, float tension, bool closed);
    void __cdecl Set(unsigned int count, const Vector3* const points, float* const tensions, bool closed);
};
JET_LAYOUT32(sizeof(BSpline) == 0x08);

class BezierSplineSegment : public SplineSegment {
public:
    BezierSplineSegment(const BezierSplineSegment& o);
    BezierSplineSegment(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3);
    ~BezierSplineSegment();
    BezierSplineSegment& operator=(const BezierSplineSegment& o);

    void __cdecl Set(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3);
};
JET_LAYOUT32(sizeof(BezierSplineSegment) == 0x34);

class BezierSpline : public Spline {
public:
    BezierSpline(const BezierSpline& o);
    BezierSpline(unsigned int count, const Vector3* const points);
    ~BezierSpline();
    BezierSpline& operator=(const BezierSpline& o);

    void __cdecl Set(unsigned int count, const Vector3* const points);
};
JET_LAYOUT32(sizeof(BezierSpline) == 0x08);

class CardinalSplineSegment : public SplineSegment {
public:
    CardinalSplineSegment(const CardinalSplineSegment& o);
    CardinalSplineSegment(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float tension);
    ~CardinalSplineSegment();
    CardinalSplineSegment& operator=(const CardinalSplineSegment& o);

    void __cdecl Set(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float tension);
};
JET_LAYOUT32(sizeof(CardinalSplineSegment) == 0x34);

class CardinalSpline : public Spline {
public:
    CardinalSpline(const CardinalSpline& o);
    CardinalSpline(unsigned int count, const Vector3* const points, float tension, bool closed);
    CardinalSpline(unsigned int count, const Vector3* const points, float* const tensions, bool closed);
    ~CardinalSpline();
    CardinalSpline& operator=(const CardinalSpline& o);

    void __cdecl Set(unsigned int count, const Vector3* const points, float tension, bool closed);
    void __cdecl Set(unsigned int count, const Vector3* const points, float* const tensions, bool closed);
};
JET_LAYOUT32(sizeof(CardinalSpline) == 0x08);

class HermiteSplineSegment : public SplineSegment {
public:
    HermiteSplineSegment(const HermiteSplineSegment& o);
    HermiteSplineSegment(const Vector3& p0, const Vector3& t0, const Vector3& p1, const Vector3& t1);
    ~HermiteSplineSegment();
    HermiteSplineSegment& operator=(const HermiteSplineSegment& o);

    void __cdecl Set(const Vector3& p0, const Vector3& t0, const Vector3& p1, const Vector3& t1);
};
JET_LAYOUT32(sizeof(HermiteSplineSegment) == 0x34);

class HermiteSpline : public Spline {
public:
    HermiteSpline(const HermiteSpline& o);
    HermiteSpline(unsigned int count, const Vector3* const points, const Vector3* const tangents, bool closed);
    HermiteSpline(unsigned int count, const Vector3* const points, bool closed);
    ~HermiteSpline();
    HermiteSpline& operator=(const HermiteSpline& o);

    void __cdecl Set(unsigned int count, const Vector3* const points, const Vector3* const tangents, bool closed);
    void __cdecl Set(unsigned int count, const Vector3* const points, bool closed);
};
JET_LAYOUT32(sizeof(HermiteSpline) == 0x08);

class TCBSplineSegment : public SplineSegment {
public:
    TCBSplineSegment(const TCBSplineSegment& o);
    TCBSplineSegment(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float tension,
                     float bias, float continuity);
    ~TCBSplineSegment();
    TCBSplineSegment& operator=(const TCBSplineSegment& o);

    void __cdecl Set(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float tension,
                     float bias, float continuity);
};
JET_LAYOUT32(sizeof(TCBSplineSegment) == 0x34);

class TCBSpline : public Spline {
public:
    TCBSpline(const TCBSpline& o);
    TCBSpline(unsigned int count, const Vector3* const points, float tension, float bias, float continuity, bool closed);
    TCBSpline(unsigned int count, const Vector3* const points, float* const tensions, float* const biases,
              float* const continuities, bool closed);
    ~TCBSpline();
    TCBSpline& operator=(const TCBSpline& o);

    void __cdecl Set(unsigned int count, const Vector3* const points, float tension, float bias, float continuity,
                     bool closed);
    void __cdecl Set(unsigned int count, const Vector3* const points, float* const tensions, float* const biases,
                     float* const continuities, bool closed);
};
JET_LAYOUT32(sizeof(TCBSpline) == 0x08);

// 0x24 bytes. ReadConfigData/WriteConfigData/ReadTextFile/WriteTextFile are not declared here: they need
// KernelStandard's ConfigData/ConfigParser and KernelResource's ResourceDatabase and stay forwarded to the original.
class SplineDefinition {
public:
    enum SplineType { Hermite = 0, Cardinal = 1, TCB = 2, Bezier = 3, BSpline = 4, Unknown = 5 };

    JET_HEAP_OBJECT
    static void* operator new(size_t, void* p) { return p; }
    static void operator delete(void*, void*) {}
    SplineDefinition();
    SplineDefinition(const SplineDefinition& o);
    SplineDefinition& operator=(const SplineDefinition& o);
    virtual ~SplineDefinition();

    virtual Spline* __cdecl CreateSpline();

    void __cdecl SetType(SplineType type);
    SplineType __cdecl GetType() const;
    void __cdecl SetParameters(bool closed, float tension, float bias, float continuity);
    void __cdecl GetParameters(bool& closed, float& tension, float& bias, float& continuity) const;
    void __cdecl AddPoint(const Vector3& point);
    unsigned int __cdecl GetNumPoints() const;
    void __cdecl RemoveAllPoints();
    Vector3* __cdecl GetPointArray();
    const Vector3* __cdecl GetPointArray() const;

    // Wave 6: KernelStandard's ConfigData/ConfigParser and KernelResource's ResourceDatabase paths.
    bool __cdecl ReadConfigData(const ConfigData& config);
    bool __cdecl WriteConfigData(ConfigData& config, bool includeVersion) const;
    bool __cdecl ReadTextFile(const ResourceDatabase* database, const char* name);
    bool __cdecl WriteTextFile(const ResourceDatabase* database, const char* name) const;

private:
    void CopyPointsFrom(const SplineDefinition& o);

public:
    int type;           // +0x04
    bool closed;        // +0x08
    float tension;      // +0x0c
    float bias;         // +0x10
    float continuity;   // +0x14
    unsigned int count;     // +0x18
    unsigned int capacity;  // +0x1c
    Vector3* points;        // +0x20
};
JET_LAYOUT32(sizeof(SplineDefinition) == 0x24);

}  // namespace Jet

// ---- workstream U2 helpers -----------------------------------------------------------------------------------
// The TS12 spline code is SSE and its NaN results depend on the exact operand position: with two NaN operands an
// SSE add/sub/mul/div returns the second one. The compiler is free to swap commutative operands, so the coefficient
// formulas below use these explicit helpers to keep the original's dest/source order.
namespace Jet {
namespace SplinesDetail {

#if defined(_M_IX86)
inline float Add(float a, float b)
{
    __asm {
        movss xmm0, dword ptr [a]
        addss xmm0, dword ptr [b]
        movss dword ptr [a], xmm0
    }
    return a;
}
inline float Sub(float a, float b)
{
    __asm {
        movss xmm0, dword ptr [a]
        subss xmm0, dword ptr [b]
        movss dword ptr [a], xmm0
    }
    return a;
}
inline float Mul(float a, float b)
{
    __asm {
        movss xmm0, dword ptr [a]
        mulss xmm0, dword ptr [b]
        movss dword ptr [a], xmm0
    }
    return a;
}
inline float Div(float a, float b)
{
    __asm {
        movss xmm0, dword ptr [a]
        divss xmm0, dword ptr [b]
        movss dword ptr [a], xmm0
    }
    return a;
}
#else
inline float Add(float a, float b) { return a + b; }
inline float Sub(float a, float b) { return a - b; }
inline float Mul(float a, float b) { return a * b; }
inline float Div(float a, float b) { return a / b; }
#endif

}  // namespace SplinesDetail
}  // namespace Jet

