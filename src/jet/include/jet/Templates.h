// Render3DCore_7c wave 5: the remaining template/free-operator exports.
//
// The class bodies live in the unit headers (Render3DCoreGeometry.h, Render3DCoreParticle.h, Render3DCoreLines.h,
// Render3DCoreMaterials.h). This header only adds the two small templates the original instantiates here and the
// Line free operators, so the definitions in Templates.cpp get the original mangled names.
#pragma once

#include <jet/Math.h>
#include <jet/Render3DCoreLines.h>
#include <jet/Render3DCoreMaterials.h>

namespace Jet {

// The original's animation interpolator: an empty class whose operator= is a no-op and whose static Interpolate
// delegates to the point type's Slerp.
template <class T>
class DefaultInterpolator {
public:
    DefaultInterpolator& operator=(const DefaultInterpolator&) { return *this; }
    static T __cdecl Interpolate(const T& a, const T& b, float t);
};

// StdMaterialPluginTemplate<M> is declared in Render3DCoreMaterials.h (it is the AutoTex plugin's base there).

// Line free operators (0x10029c80..0x10029fce). The direction is copied through x87 (sNaN quieting), the point is
// moved in SSE with the original's operand order.
Line __cdecl operator+(const Line& line, const Vector3& delta);
Line __cdecl operator-(const Line& line, const Vector3& delta);
Line __cdecl operator*(const Line& line, float scale);
Line __cdecl operator*(float scale, const Line& line);
Line __cdecl operator*(const Matrix3& matrix, const Line& line);
Line __cdecl operator/(const Line& line, float scale);

}  // namespace Jet
