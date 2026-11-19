// InputDirectX_7c (Jet, Trainz 2012 x64 port) - Wave 1 scaffold.
// Evidence: ghidra_maps/jet/input/InputDirectX_7c.dll.functions.tsv,
// vtnames_InputDirectX_7c.dll.tsv, exports/InputDirectX_7c.tsv,
// notes/agents/InputDirectX_7c.md, InputSystem_7c/InputWin32_7c patterns.
// Status: placeholders/stubs only; ABI/layout/vtable/COM contracts NOT verified.
#pragma once

#include <jet/InputSystem.h>
#include <jet/Plugin.h>

namespace Jet {

// TODO(ABI): Confirm class names, inheritance, object layout, and vtable slot order
// against original InputDirectX_7c before implementing behavior.
// Evidence suggests:
// - InputDriverDirectX : InputDriver (driver lifecycle/enumeration/create)
// - InputDeviceDirectX base/helpers shared by keyboard/mouse/joystick device classes
// - DirectXJoystick / DirectXKeyboard / DirectXMouse : device implementations
// - ForceEffectDirectX : ForceEffect (DirectInput force-feedback effect)
// - PluginFactoryOf<Jet::InputDriverDirectX, Jet::InputDriver> used for registration
// vtnames show heavy slot aliasing/thunks; do not infer concrete vtable layout from stubs.

class InputDriverDirectX : public InputDriver {
public:
    InputDriverDirectX();
    ~InputDriverDirectX() override;

    bool __cdecl Init(OS* os) override;
    void __cdecl Enumerate(InputDeviceCapsList& list, int type) override;
    AutoReference<InputDevice> __cdecl CreateDevice(const UId& id) override;

protected:
    void __cdecl Update(float dt) override;
    void __cdecl CriticalShutdown() override;
    void __cdecl CriticalStartup() override;

    // TODO(contracts): store OS*/HWND, IDirectInput8*, device lists, ref/lifetime state
    // as required by original. Do not assume 32-bit pointer/handle sizes.
    OS* os_ = nullptr;
};

// Placeholders for RE-driven implementation units (deferred; no ABI claims here).
// class InputDeviceDirectX : public InputDevice { /* TODO */ };
// class DirectXKeyboard : public KeyboardDevice /* or InputDeviceDirectX */ { /* TODO */ };
// class DirectXMouse : public MouseDevice /* or InputDeviceDirectX */ { /* TODO */ };
// class DirectXJoystick : public JoystickDevice /* or InputDeviceDirectX */ { /* TODO */ };
// class ForceEffectDirectX : public ForceEffect { /* TODO */ };

}  // namespace Jet

extern "C" void __cdecl JetGetPluginsInputDirectX();
