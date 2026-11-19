// DebugInterface_7c scaffold (Wave 1, module-owned). Minimal placeholders only.
// Most exports remain forwarded via implemented.txt until ABI/layout is verified.

#include "DebugInterface.h"

namespace Jet {

// ---- DebugResetCamera ----
DebugResetCamera::DebugResetCamera() {}
DebugResetCamera::DebugResetCamera(const DebugResetCamera&) {}
DebugResetCamera& DebugResetCamera::operator=(const DebugResetCamera&) { return *this; }

void __cdecl DebugResetCamera::ResetCamera(const Vector3&, const Vector3&) {}
void __cdecl DebugResetCamera::ResetCamera() {}

// ---- DebugAccess ----
// Static data declarations are present for layout/planning; definitions/exports are
// intentionally deferred until exact object layout, initialization, and ownership
// (especially controlFlags/resetCameraArray) are confirmed from decomp.

DebugAccess::DebugAccess() {}
DebugAccess::DebugAccess(const DebugAccess&) {}
DebugAccess& DebugAccess::operator=(const DebugAccess&) { return *this; }
DebugAccess::~DebugAccess() {}

void __cdecl DebugAccess::AddResetCameraObject(DebugResetCamera*) {
    // TODO: reproduce registry mutation semantics against resetCameraArray.
}
void __cdecl DebugAccess::RemoveResetCameraObject(DebugResetCamera*) {
    // TODO: reproduce registry mutation semantics against resetCameraArray.
}

bool __cdecl DebugAccess::CheckBoundingVolumeMask(unsigned long) { return false; }
unsigned long __cdecl DebugAccess::GetBoundingVolumeMask() { return 0; }
int __cdecl DebugAccess::GetControlFlag(int) { return 0; }
bool __cdecl DebugAccess::GetGlobalBoundingVolumes() { return false; }
bool __cdecl DebugAccess::GetPortalDebugMode() { return false; }
bool __cdecl DebugAccess::GetPortalState() { return false; }
bool __cdecl DebugAccess::GetWireOnTexture() { return false; }
void __cdecl DebugAccess::SetBoundingVolumeMask(unsigned long) {}
void __cdecl DebugAccess::SetControlFlag(int, int) {}
void __cdecl DebugAccess::SetGlobalBoundingVolumes(bool) {}
void __cdecl DebugAccess::SetPortalDebugMode(bool) {}
void __cdecl DebugAccess::SetPortalState(bool) {}
void __cdecl DebugAccess::SetWireOnTexture(bool) {}
bool __cdecl DebugAccess::ToggleGlobalBoundingVolumes() { return false; }
bool __cdecl DebugAccess::TogglePortalDebugMode() { return false; }
bool __cdecl DebugAccess::TogglePortalState() { return false; }
bool __cdecl DebugAccess::ToggleWireOnTexture() { return false; }

void DebugAccess::vf0() {
    // TODO: map to correct vtable slot/semantics (function map: 10001020, 89 bytes).
}

}  // namespace Jet

extern "C" void __cdecl JetGetPluginsDebugInterface() {
    // Evidence: RVA 0x1520 aliased with DebugResetCamera::ResetCamera bodies in original.
    // Safe placeholder: no registration side effects. Must be verified and implemented
    // (likely factory/group registration via Kernel plugin infrastructure) before use.
}
