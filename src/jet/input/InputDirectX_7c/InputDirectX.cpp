// InputDirectX_7c (Jet, Trainz 2012 x64 port) - Wave 1 scaffold.
// Minimal safe stubs only; DirectInput/COM/device/force-feedback behavior deferred.
// This file is module-owned scaffold work and is not yet wired into shared build files.

#include "InputDirectX.h"

namespace Jet {

InputDriverDirectX::InputDriverDirectX() : InputDriver() {}

InputDriverDirectX::~InputDriverDirectX() {
    // TODO(contracts): release DirectInput/COM/device resources if acquired; mirror original order.
}

bool __cdecl InputDriverDirectX::Init(OS* os) {
    // TODO(DI): DirectInput8Create(DINPUT8), obtain HINSTANCE/HWND/cooperative level per original.
    // TODO(behavior): preserve original selection/coexistence with InputWin32 (keyboard/mouse typically Win32).
    os_ = os;
    return false;  // safe stub: driver not initialized/available
}

void __cdecl InputDriverDirectX::Update(float /*dt*/) {
    // TODO(DI): poll/acquire/update managed devices; handle lost/not-acquired/focus transitions.
}

void __cdecl InputDriverDirectX::Enumerate(InputDeviceCapsList& /*list*/, int /*type*/) {
    // TODO(DI): enumerate relevant devices/caps (joysticks/wheels/FF primary per PORT_PLAN; confirm original).
    // Must not disturb Win32 keyboard/mouse path or invent IDs/caps.
}

AutoReference<InputDevice> __cdecl InputDriverDirectX::CreateDevice(const UId& /*id*/) {
    // TODO(DI): create/wrap DirectInput device instance matching requested id; set instanceId/handle/state.
    return AutoReference<InputDevice>();
}

void __cdecl InputDriverDirectX::CriticalShutdown() {
    // TODO(DI): unacquire/release as required during crash/critical shutdown; coordinate with device CriticalShutdown.
}

void __cdecl InputDriverDirectX::CriticalStartup() {
    // TODO(DI): reacquire/reinitialize as required after critical startup.
}

// TODO(impl units, from notes/agents/InputDirectX_7c.md):
// U1 driver lifecycle/state, U2 shared InputDeviceDirectX helpers, U3 DirectXJoystick,
// U4 DirectXKeyboard/DirectXMouse compatibility, U5 ForceEffectDirectX + FF hooks.
// TODO(x64): COM vtable/stdcall semantics, pointer/handle sizes, DINPUT struct sizeof/alignment/packing,
// callback typedefs, GUID/UId byte-exact mapping, HWND storage.

}  // namespace Jet

extern "C" void __cdecl JetGetPluginsInputDirectX() {
    // TODO(plugin/ABI): register DirectInput driver factory in the Jet input-driver plugin group.
    // Expected (evidence): name "DirectX" in group UId("F3117B18-F364-41a1-8993-966236A35B5E")
    // so Input::LoadDriver default joystick selection can find it.
    // Factory/type IDs and exact plugin-table/registration side effects must be recovered from the original binary;
    // do NOT register with guessed IDs. Safe scaffold: no-op.
    //
    // Pattern when IDs are known:
    //   new Jet::PluginFactoryOf<Jet::InputDriverDirectX, Jet::InputDriver>(
    //       Jet::PString("DirectX"), Jet::UId("<factory-id-TODO>"), Jet::UId("F3117B18-F364-41a1-8993-966236A35B5E"));
}
