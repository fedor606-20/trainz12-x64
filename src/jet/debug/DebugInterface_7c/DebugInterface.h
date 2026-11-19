#pragma once

// Local scaffold header for DebugInterface_7c (Jet debug-state/access module).
// Declarations are based on export/demangle evidence; ABI/layout/calling conventions
// and vtable slot semantics must be verified against decomp before enabling exports.

#include <jet/FastDynamicArray.h>
#include <jet/Math.h>

namespace Jet {

class DebugResetCamera {
public:
    DebugResetCamera();
    DebugResetCamera(const DebugResetCamera& other);
    DebugResetCamera& operator=(const DebugResetCamera& other);

    // Evidence: both virtual overloads share RVA 0x1520 with JetGetPluginsDebugInterface
    // in the original module; treat as no-op stubs until verified.
    virtual void __cdecl ResetCamera(const Vector3& a, const Vector3& b);
    virtual void __cdecl ResetCamera();
};

class DebugAccess {
public:
    DebugAccess(const DebugAccess& other);
    DebugAccess& operator=(const DebugAccess& other);

    // Export evidence: instance methods, __cdecl (verify against decomp/mangling).
    void __cdecl AddResetCameraObject(DebugResetCamera* obj);
    void __cdecl RemoveResetCameraObject(DebugResetCamera* obj);

    static bool __cdecl CheckBoundingVolumeMask(unsigned long mask);
    static unsigned long __cdecl GetBoundingVolumeMask();
    static int __cdecl GetControlFlag(int index);
    static bool __cdecl GetGlobalBoundingVolumes();
    static bool __cdecl GetPortalDebugMode();
    static bool __cdecl GetPortalState();
    static bool __cdecl GetWireOnTexture();
    static void __cdecl SetBoundingVolumeMask(unsigned long mask);
    static void __cdecl SetControlFlag(int index, int value);
    static void __cdecl SetGlobalBoundingVolumes(bool value);
    static void __cdecl SetPortalDebugMode(bool value);
    static void __cdecl SetPortalState(bool value);
    static void __cdecl SetWireOnTexture(bool value);
    static bool __cdecl ToggleGlobalBoundingVolumes();
    static bool __cdecl TogglePortalDebugMode();
    static bool __cdecl TogglePortalState();
    static bool __cdecl ToggleWireOnTexture();

    // Exported static data (ABI-visible; define/export only after layout/semantics verified).
    static unsigned long boundingVolumeMask;
    static int* controlFlags;
    static bool globalBoundingVolumes;
    static bool portalDebugMode;
    static bool portalState;
    static FastDynamicArray<DebugResetCamera*, 13, Less<DebugResetCamera*, DebugResetCamera*>> resetCameraArray;
    static bool wireOnTexture;

protected:
    DebugAccess();
    virtual ~DebugAccess();

    // Function map shows DebugAccess::vf0 @ 10001020 (89 bytes). Slot/name/layout unknown.
    virtual void vf0();
};

}  // namespace Jet

extern "C" void __cdecl JetGetPluginsDebugInterface();
