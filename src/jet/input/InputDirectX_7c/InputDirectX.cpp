// InputDirectX_7c: the "DirectX" input driver plugin (DirectInput 8): keyboard, mouse, joysticks, wheels and
// force feedback. Keyboard and mouse in the game come from InputWin32, so the live part is the joystick driver.
// Transcribed from the shipped module (0x10001000..0x10005c50); InputDevice / InputDriver / ForceEffect live in
// InputSystem_7c, Window / OS / Time / Alloc / PluginFactory in KernelStandard_7c and Kernel_7c.
#define DIRECTINPUT_VERSION 0x0800
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dinput.h>

#include <jet/InputSystem.h>
#include <jet/KernelStandard.h>
#include <jet/Plugin.h>
#include <jet/Window.h>

#include <cstdlib>
#include <cstring>
#include <new>

namespace Jet {

namespace {

void SetText(PString& s, const char* text)
{
    s.~PString();
    new (&s) PString(text);
}

unsigned long Bits(unsigned long count)
{
    unsigned long mask = 0;
    for (unsigned long i = 0; i < count; ++i) mask |= 1u << i;
    return mask;
}

bool SameId(const void* a, const void* b) { return std::memcmp(a, b, 16) == 0; }

UId ToUid(const GUID& g)
{
    UId id;
    std::memcpy(&id, &g, sizeof(UId));
    return id;
}

// DirectInput scan code -> Jet key code (filled by the module's static initializer; 0 = no key)
unsigned char g_scanToKey[256];

// ---- force feedback helpers -------------------------------------------------

// DIEFFECT as this module lays it out (DirectX 6/7 size 0x38) plus its envelope, type parameters, axes,
// directions and the effect type GUID the command needs. The original builds exactly these bytes.
struct DiEffect {
    DWORD dwSize;                  // +0x00 0x38
    DWORD dwFlags;                 // +0x04
    DWORD dwDuration;              // +0x08
    DWORD dwSamplePeriod;          // +0x0c
    DWORD dwGain;                  // +0x10
    DWORD dwTriggerButton;         // +0x14
    DWORD dwTriggerRepeatInterval; // +0x18
    DWORD cAxes;                   // +0x1c
    DWORD* rgdwAxes;               // +0x20
    LONG* rglDirection;            // +0x24
    DIENVELOPE* lpEnvelope;        // +0x28
    DWORD cbTypeSpecificParams;    // +0x2c
    void* lpvTypeSpecificParams;   // +0x30
    DWORD dwStartDelay;            // +0x34 (outside the size the module passes)
    DIENVELOPE envelope;           // +0x38
    DWORD params[12];              // +0x4c
    DWORD* paramData;              // +0x7c
    DWORD axes[2];                 // +0x80
    LONG direction[2];             // +0x88
    GUID type;                     // +0x90
    bool ok;                       // +0xa0
};
JET_LAYOUT32(sizeof(DiEffect) == 0xa4);

const GUID* EffectGuid(unsigned long type)
{
    switch (type) {
    case ForceInfo::CONSTANT: return &GUID_ConstantForce;
    case ForceInfo::RAMP: return &GUID_RampForce;
    case 3: return &GUID_CustomForce;
    case ForceInfo::SQUARE: return &GUID_Square;
    case ForceInfo::SINE: return &GUID_Sine;
    case ForceInfo::TRIANGLE: return &GUID_Triangle;
    case ForceInfo::SAWTOOTH_UP: return &GUID_SawtoothUp;
    case ForceInfo::SAWTOOTH_DOWN: return &GUID_SawtoothDown;
    case ForceInfo::SPRING: return &GUID_Spring;
    case ForceInfo::DAMPER: return &GUID_Damper;
    case ForceInfo::INERTIA: return &GUID_Inertia;
    case ForceInfo::FRICTION: return &GUID_Friction;
    default: return nullptr;
    }
}

// 0x100016d0: ForceInfo -> DIEFFECT. `axisMask` says which of X/Y the effect drives.
DiEffect* BuildEffect(DiEffect* de, const ForceInfo& info, unsigned long axisMask)
{
    std::memset(de, 0, sizeof(*de));
    de->ok = true;
    const GUID* const guid = EffectGuid(info.type);
    if (!guid) {
        de->ok = false;
        return de;
    }
    de->type = *guid;
    de->dwSize = 0x38;
    de->dwFlags = 2;
    de->dwDuration = static_cast<DWORD>(info.duration);
    de->dwSamplePeriod = info.samplePeriod;
    de->dwGain = info.gain;
    de->dwTriggerButton =
        (info.triggerButton == -1 || info.triggerButton >= 0x20) ? 0xffffffff : static_cast<DWORD>(info.triggerButton + 0x30);
    de->dwTriggerRepeatInterval = info.triggerRepeat;
    de->dwStartDelay = info.startDelay;
    de->envelope.dwSize = 0x14;
    if (info.hasEnvelope) {
        de->lpEnvelope = &de->envelope;
        de->envelope.dwAttackLevel = info.attackLevel;
        de->envelope.dwAttackTime = info.attackTime;
        de->envelope.dwFadeLevel = info.fadeLevel;
        de->envelope.dwFadeTime = info.fadeTime;
    }
    de->rgdwAxes = de->axes;
    de->rglDirection = de->direction;
    de->cAxes = 0;
    if (axisMask & 1) {
        de->axes[0] = DIJOFS_X;
        de->direction[0] = info.direction[0];
        ++de->cAxes;
    }
    if (axisMask & 2) {
        de->axes[de->cAxes] = DIJOFS_Y;
        de->direction[de->cAxes] = info.direction[1];
        ++de->cAxes;
    }
    if (!de->cAxes) {
        de->ok = false;
        return de;
    }
    // cartesian: the directions are already the two cosines; polar: one angle when only one axis
    if (!info.cartesian) {
        if (de->cAxes == 2) {
            de->dwFlags = 0x22;
        } else {
            de->dwFlags = 0x12;
            const long a = info.direction[0];
            if (!de->axes[0]) {
                if (a == 0 || a == 18000)
                    de->direction[0] = 0;
                else
                    de->direction[0] = (a < 18000) ? 1 : -1;
            } else {
                if (a == 9000 || a == 27000)
                    de->direction[0] = 0;
                else if (a <= 9000 || a >= 27000)
                    de->direction[0] = -1;
                else
                    de->direction[0] = 1;
            }
        }
    } else {
        de->dwFlags = 0x12;
    }
    // the type-specific parameter block
    switch (info.type) {
    case ForceInfo::CONSTANT:
        de->cbTypeSpecificParams = 4;
        de->params[0] = static_cast<DWORD>(info.params[0]);
        break;
    case ForceInfo::RAMP:
        de->cbTypeSpecificParams = 8;
        de->params[0] = static_cast<DWORD>(info.params[0]);
        de->params[1] = static_cast<DWORD>(info.params[1]);
        break;
    case 3: {  // custom force: a count and the samples it points at
        const unsigned long count = static_cast<unsigned long>(info.params[2]) * static_cast<unsigned long>(info.params[0]);
        de->cbTypeSpecificParams = 0x10;
        de->params[0] = static_cast<DWORD>(info.params[0]);
        de->params[1] = static_cast<DWORD>(info.params[1]);
        de->params[2] = static_cast<DWORD>(info.params[2]);
        de->paramData = static_cast<DWORD*>(Alloc::New(static_cast<unsigned int>(count * 4)));
        de->params[3] = static_cast<DWORD>(info.params[3]);
        const DWORD* const src = reinterpret_cast<const DWORD*>(static_cast<uintptr_t>(info.params[3]));
        for (unsigned long i = 0; i < count && src; ++i) de->paramData[i] = src[i];
        break;
    }
    case ForceInfo::SQUARE:
    case ForceInfo::SINE:
    case ForceInfo::TRIANGLE:
    case ForceInfo::SAWTOOTH_UP:
    case ForceInfo::SAWTOOTH_DOWN:
        de->cbTypeSpecificParams = 0x10;
        for (int i = 0; i < 4; ++i) de->params[i] = static_cast<DWORD>(info.params[i]);
        break;
    case ForceInfo::SPRING:
    case ForceInfo::DAMPER:
    case ForceInfo::INERTIA:
    case ForceInfo::FRICTION:
        de->cbTypeSpecificParams = 0x30;
        for (int i = 0; i < 12; ++i) de->params[i] = static_cast<DWORD>(info.params[i]);
        break;
    default:
        break;
    }
    return de;
}

// 0x10002550: EnumObjects callback that records which axes the device reports (bit per axis GUID)
BOOL CALLBACK EnumAxisCallback(LPCDIDEVICEOBJECTINSTANCEA obj, LPVOID context)
{
    if (!obj || !context) return DIENUM_CONTINUE;
    unsigned long& mask = *static_cast<unsigned long*>(context);
    const GUID& g = obj->guidType;
    if (IsEqualGUID(g, GUID_XAxis))
        mask |= 1;
    else if (IsEqualGUID(g, GUID_YAxis))
        mask |= 2;
    else if (IsEqualGUID(g, GUID_ZAxis))
        mask |= 4;
    else if (IsEqualGUID(g, GUID_RxAxis))
        mask |= 8;
    else if (IsEqualGUID(g, GUID_RyAxis))
        mask |= 0x10;
    else if (IsEqualGUID(g, GUID_RzAxis))
        mask |= 0x20;
    else if (IsEqualGUID(g, GUID_Slider))
        mask = (mask & 0x40) ? (mask | 0x80) : (mask | 0x40);
    return DIENUM_CONTINUE;
}

}  // namespace

// ---- the classes -----------------------------------------------------------

class InputDriverDirectX;
class DirectXJoystick;

// ForceEffectDirectX: one DirectInput effect on a device (0x9c bytes)
class ForceEffectDirectX : public ForceEffect {
    friend class DirectXJoystick;

public:
    ForceEffectDirectX(InputDevice* device);
    ~ForceEffectDirectX() override;

    bool __cdecl IsValid() const override;
    bool __cdecl IsPlaying() override;
    bool __cdecl Start(unsigned long iterations, bool solo) override;
    bool __cdecl Stop() override;
    bool __cdecl Update() override;
    bool __cdecl Load() override;
    bool __cdecl Unload() override;
    bool __cdecl Create(const ForceInfo& info, unsigned long flags) override;
    using ForceEffect::Create;
    void __cdecl CriticalShutdown() override;

    void ShutdownEffect() { CriticalShutdown(); }

private:
    IDirectInputEffect* effect;  // +0x88
    unsigned char flags;         // +0x8c bit 0 valid, 1 invalid, 2 iterations infinite, 3 duration infinite,
                                 // 4 playing, 5 trigger button, 6 trigger timing
    float time;                  // +0x90 since the trigger
    float duration;              // +0x94 seconds
    unsigned long axisMask;      // +0x98 the axes the device has (EnumObjects)
};
JET_LAYOUT32(sizeof(ForceEffectDirectX) == 0x9c);

// the DirectInput base of the three devices (0x23c bytes)
class InputDeviceDirectX : public InputDevice {
    friend class ForceEffectDirectX;
    friend class InputDriverDirectX;

public:
    explicit InputDeviceDirectX(InputDriver* driver) : InputDevice(driver), device(nullptr) {}
    ~InputDeviceDirectX() override;

    void __cdecl Done() override;
    void __cdecl CriticalShutdown() override;
    void __cdecl GetCaps(InputDeviceCaps& caps) const override = 0;
    void __cdecl Update(float dt) override = 0;

protected:
    // slot 18, added by this module: create the DirectInput device and set its data format and mode
    virtual bool __cdecl Init(Window* window, IDirectInput8A* di, const UId& id);

    void ReleaseDevice();

    IDirectInputDevice8A* device;  // +0x238
};
JET_LAYOUT32(sizeof(InputDeviceDirectX) == 0x23c);

class DirectXKeyboard : public InputDeviceDirectX {
public:
    explicit DirectXKeyboard(InputDriver* driver);
    void __cdecl GetCaps(InputDeviceCaps& caps) const override;
    bool __cdecl GetProperty(DeviceProperty& p) override;
    bool __cdecl SetProperty(const DeviceProperty& p) override;

protected:
    bool __cdecl Init(Window* window, IDirectInput8A* di, const UId& id) override;
    void __cdecl Update(float dt) override;

private:
    struct KeyState {
        unsigned long time;
        unsigned char state;  // 0 up, 1 down, 2 repeating
        unsigned char pad[3];
    };
    unsigned char lost;    // +0x23c
    KeyState keys[256];    // +0x240
};
JET_LAYOUT32(sizeof(DirectXKeyboard) == 0xa40);

class DirectXMouse : public InputDeviceDirectX {
public:
    explicit DirectXMouse(InputDriver* driver);
    void __cdecl GetCaps(InputDeviceCaps& caps) const override;
    bool __cdecl GetProperty(DeviceProperty& p) override;
    bool __cdecl SetProperty(const DeviceProperty& p) override;

protected:
    bool __cdecl Init(Window* window, IDirectInput8A* di, const UId& id) override;
    void __cdecl Update(float dt) override;

private:
    int eventDelta[3];  // +0x23c the axis deltas carried by buffered data
    int unknown248;     // +0x248 never written
    struct Range {
        int min, max, pad;
    } range[3];           // +0x24c, 12 bytes each; range[2].pad doubles as threshold[0]
    int thresholdExtra[2];  // +0x270 threshold[1], threshold[2]
    unsigned char lost;   // +0x278
    int& Threshold(int i) { return i ? thresholdExtra[i - 1] : range[2].pad; }
    const int& Threshold(int i) const { return i ? thresholdExtra[i - 1] : range[2].pad; }
};
JET_LAYOUT32(sizeof(DirectXMouse) == 0x27c);

class DirectXJoystick : public InputDeviceDirectX {
    friend class ForceEffectDirectX;

public:
    explicit DirectXJoystick(InputDriver* driver);
    ~DirectXJoystick() override;

    void __cdecl GetCaps(InputDeviceCaps& caps) const override;
    bool __cdecl GetProperty(DeviceProperty& p) override;
    bool __cdecl SetProperty(const DeviceProperty& p) override;
    ForceEffect* __cdecl CreateEffect(const PString& name, const ResourceDatabase* db, unsigned long flags) override;
    ForceEffect* __cdecl CreateEffect(const ForceInfo& info, unsigned long flags) override;

protected:
    bool __cdecl Init(Window* window, IDirectInput8A* di, const UId& id) override;
    void __cdecl Update(float dt) override;
    void __cdecl Done() override;
    void __cdecl CriticalShutdown() override;

private:
    bool __cdecl AcquireDevice();
    void DestroyEffects();
    void UpdateEffects(float dt);
    unsigned long CapabilityMask(unsigned long type) const;
    static BOOL CALLBACK EnumEffectCallback(LPCDIEFFECTINFOA info, LPVOID context);
    static bool __fastcall Hook(void* self, void* message, int& result);

    Window* window;                  // +0x23c
    int threshold[8];                // +0x240
    int unknown260[8];               // +0x260
    InputDeviceCaps cachedCaps;      // +0x280 (type..unknown34)
    unsigned long capCount;          // +0x2b8
    unsigned long capCapacity;       // +0x2bc
    struct EffectCap {
        unsigned long type, effectType, staticParams, dynamicParams;
    };
    EffectCap* caps;                 // +0x2c0
    FastDynamicArray<ForceEffect*, 13> effects;  // +0x2c4
    unsigned char flags;             // +0x2c8 bit0 acquired, 1 enabled, 2 lost, 3 force feedback, 4..6/7 properties
};
JET_LAYOUT32(sizeof(DirectXJoystick) == 0x2cc);

class InputDriverDirectX : public InputDriver {
public:
    InputDriverDirectX() : di(nullptr), os(nullptr) {}
    ~InputDriverDirectX() override { ReleaseDirectInput(); }

    bool __cdecl Init(OS* os) override;
    void __cdecl Enumerate(InputDeviceCapsList& list, int type) override;
    AutoReference<InputDevice> __cdecl CreateDevice(const UId& id) override;

protected:
    void __cdecl CriticalShutdown() override { ReleaseDirectInput(); }
    void __cdecl CriticalStartup() override {}

private:
    void ReleaseDirectInput();

    OS* os;                  // +0x0c
    IDirectInput8A* di;      // +0x10
};
JET_LAYOUT32(sizeof(InputDriverDirectX) == 0x20);

// ---- ForceEffectDirectX

ForceEffectDirectX::ForceEffectDirectX(InputDevice* d) : ForceEffect(d)  // 0x10001020
{
    flags = static_cast<unsigned char>((flags & 0x82) | 2);
    effect = nullptr;
    time = 0;
    duration = 0;
    axisMask = 0;
    if (d && static_cast<InputDeviceDirectX*>(d)->device)
        static_cast<InputDeviceDirectX*>(d)->device->EnumObjects(&EnumAxisCallback, &axisMask,
                                                                 DIDFT_FFACTUATOR | DIDFT_AXIS);
}

ForceEffectDirectX::~ForceEffectDirectX()  // 0x100010f0
{
    if (effect) {
        effect->Release();
        effect = nullptr;
    }
    if (info.type == 3 && info.params[3]) Alloc::Delete(reinterpret_cast<void*>(static_cast<uintptr_t>(info.params[3])));
}

bool ForceEffectDirectX::IsValid() const  // 0x10001000
{
    return (flags & 1) && !(flags & 2);
}

bool ForceEffectDirectX::IsPlaying()  // 0x100012d0
{
    bool r = (flags & 0x10) == 0;
    if (effect) {
        DWORD status = 0;
        if (effect->GetEffectStatus(&status) >= 0 && r) r = (status & 1) != 0;
    }
    return r || ForceEffect::IsPlaying();
}

bool ForceEffectDirectX::Start(unsigned long iterations, bool solo)  // 0x10001340
{
    flags = static_cast<unsigned char>((flags & 0xf3) | (iterations == 0xffffffff ? 4 : 0) |
                                       (info.duration == -1 ? 8 : 0));
    if (!effect) return false;
    if (effect->Start(iterations, solo ? 1 : 0) < 0) return false;
    flags &= 0xef;
    if (!ForceEffect::Start(iterations, solo)) return false;
    return true;
}

bool ForceEffectDirectX::Stop()  // 0x100013c0
{
    flags &= 0xf3;
    if (!effect) return false;
    if (effect->Stop() < 0) return false;
    return ForceEffect::Stop();
}

bool ForceEffectDirectX::Update()  // 0x10001400
{
    if (!effect) return false;
    DiEffect de;
    BuildEffect(&de, info, axisMask);
    HRESULT hr = effect->SetParameters(reinterpret_cast<const DIEFFECT*>(&de), 0x400003df);
    if (hr == 0x80040208) hr = effect->SetParameters(reinterpret_cast<const DIEFFECT*>(&de), 0x3df);
    if (de.paramData) Alloc::Delete(de.paramData);
    bool ok = hr >= 0;
    const long trigger = info.triggerButton;
    if (flags & 0x20) {
        if (trigger < 0 || trigger >= 0x20) flags &= ~0x20;
    } else if (trigger >= 0 && trigger < 0x20) {
        flags = static_cast<unsigned char>((flags & ~0x40) | 0x20);
    }
    time = static_cast<float>(static_cast<unsigned long>(info.duration)) * 1e-6f;
    const bool wasValid = (flags & 1) && ok;
    flags = static_cast<unsigned char>((flags & ~1) | (wasValid ? 1 : 0));
    flags = static_cast<unsigned char>((flags & ~2) | ((flags & 1) ? 0 : 2));
    if (!ok) return false;
    return ForceEffect::Update();
}

bool ForceEffectDirectX::Load()  // 0x10001590
{
    DirectXJoystick* const joy = dynamic_cast<DirectXJoystick*>(device);
    if (!joy) {
        flags &= 0xfc;
        return (flags & 1) != 0;
    }
    bool ok = false;
    if (!(joy->flags & 1)) {
        flags |= 3;
        return (flags & 1) != 0;
    }
    if (effect && effect->Download() >= 0) ok = ForceEffect::Load();
    flags = static_cast<unsigned char>((flags & ~1) | (ok ? 1 : 0));
    flags = static_cast<unsigned char>((flags & ~2) | ((flags & 1) ? 0 : 2));
    return ok;
}

bool ForceEffectDirectX::Unload()  // 0x10001640
{
    flags &= 0xfe;
    flags = static_cast<unsigned char>((flags & ~2) | ((flags & 1) ? 0 : 2));
    if (!effect) return false;
    if (effect->Unload() < 0) return false;
    return ForceEffect::Unload();
}

bool ForceEffectDirectX::Create(const ForceInfo& i, unsigned long createFlags)  // 0x10001180
{
    if (i.type < 1 || i.type > 12) return false;
    std::memcpy(&info, &i, sizeof(ForceInfo));
    info.unknown00 |= createFlags & 0x200;
    if (info.triggerButton >= 0 && info.triggerButton < 0x20) {
        flags = static_cast<unsigned char>((flags & ~0x40) | 0x20);
        duration = static_cast<float>(static_cast<unsigned long>(info.duration)) * 1e-6f;
    }
    DiEffect de;
    BuildEffect(&de, info, axisMask);
    IDirectInputDevice8A* const dev = static_cast<InputDeviceDirectX*>(device)->device;
    HRESULT hr = dev ? dev->CreateEffect(de.type, reinterpret_cast<const DIEFFECT*>(&de), &effect, nullptr)
                     : E_FAIL;
    if (de.paramData) Alloc::Delete(de.paramData);
    return hr >= 0;
}

void ForceEffectDirectX::CriticalShutdown()  // 0x100016a0
{
    if (effect) {
        effect->Release();
        effect = nullptr;
    }
    ForceEffect::CriticalShutdown();
}

// ---- InputDeviceDirectX

InputDeviceDirectX::~InputDeviceDirectX()  // 0x10001cd0
{
    ReleaseDevice();
}

void InputDeviceDirectX::ReleaseDevice()
{
    if (device) {
        device->Unacquire();
        device->Release();
        device = nullptr;
    }
    handle = nullptr;
    valid = false;
}

void InputDeviceDirectX::Done()  // 0x10001d60
{
    ReleaseDevice();
}

bool InputDeviceDirectX::Init(Window*, IDirectInput8A*, const UId&) { return false; }

void InputDeviceDirectX::CriticalShutdown()  // 0x10001da0
{
    Done();
}

// ---- DirectXKeyboard

DirectXKeyboard::DirectXKeyboard(InputDriver* driver) : InputDeviceDirectX(driver)  // 0x10004bb0
{
    lost = 0;
    std::memset(keys, 0, sizeof(keys));
    prevState.type = DEVICE_KEYBOARD;
    state.type = DEVICE_KEYBOARD;
    instanceId = UIdKeyboard;
}

bool DirectXKeyboard::Init(Window* window, IDirectInput8A* di, const UId&)  // 0x10004db0
{
    if (!window || !di) return false;
    if (di->CreateDevice(GUID_SysKeyboard, &device, nullptr) < 0) {
        Done();
        return false;
    }
    handle = device;
    if (device->SetDataFormat(&c_dfDIKeyboard) < 0) goto fail;
    if (device->SetCooperativeLevel(static_cast<HWND>(window->GetHandle()), DISCL_NONEXCLUSIVE | DISCL_FOREGROUND) < 0)
        goto fail;
    {
        DIPROPDWORD prop;
        std::memset(&prop, 0, sizeof(prop));
        prop.diph.dwSize = sizeof(DIPROPDWORD);
        prop.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        prop.dwData = 0x100;
        if (device->SetProperty(DIPROP_BUFFERSIZE, &prop.diph) < 0) goto fail;
    }
    device->Acquire();
    return true;
fail:
    Done();
    return false;
}

void DirectXKeyboard::GetCaps(InputDeviceCaps& c) const  // 0x10004c20
{
    c.type = DEVICE_KEYBOARD;
    c.id = UIdKeyboard;
    SetText(c.name, "Keyboard");
    SetText(c.description, "System keyboard");
    c.caps = 0x100;
    if (device) {
        DIDEVICEINSTANCEA inst;
        std::memset(&inst, 0, sizeof(inst));
        inst.dwSize = sizeof(inst);
        if (device->GetDeviceInfo(&inst) >= 0) {
            SetText(c.name, inst.tszInstanceName);
            SetText(c.description, inst.tszProductName);
            c.caps |= 0x3000000;
        }
    }
    c.caps |= 0xDC000001;
    c.numButtons = 0x100;
    c.buttonMask = 0xffffffff;
    c.numAxes = c.axisMask = c.numPovs = c.unknown34 = 0;
}

bool DirectXKeyboard::GetProperty(DeviceProperty& p)  // 0x10004ea0
{
    if (!valid || p.id != PROPERTY_LOCK_KEY) return false;
    switch (p.index) {
    case 0x3b: p.state = static_cast<unsigned char>(GetKeyState(VK_CAPITAL) & 1); return true;
    case 0x46: p.state = static_cast<unsigned char>(GetKeyState(VK_NUMLOCK) & 1); return true;
    case 0x47: p.state = static_cast<unsigned char>(GetKeyState(VK_SCROLL) & 1); return true;
    default: return false;
    }
}

bool DirectXKeyboard::SetProperty(const DeviceProperty& p)  // 0x10004f10
{
    if (!valid || p.id != PROPERTY_LOCK_KEY) return false;
    int vk;
    switch (p.index) {
    case 0x3b: vk = VK_CAPITAL; break;
    case 0x46: vk = VK_NUMLOCK; break;
    case 0x47: vk = VK_SCROLL; break;
    default: return false;
    }
    if ((GetKeyState(vk) & 1) != p.state) {
        INPUT in[2];
        std::memset(in, 0, sizeof(in));
        in[0].type = in[1].type = INPUT_KEYBOARD;
        in[0].ki.wVk = in[1].ki.wVk = static_cast<WORD>(vk);
        in[0].ki.time = in[1].ki.time = Time::Ms();
        in[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(2, in, sizeof(INPUT));
    }
    PropertyChangedEvent(p.id, Time::Ms());
    return true;
}

void DirectXKeyboard::Update(float)  // 0x10005000
{
    if (!valid || !device) return;
    unsigned long now = Time::Ms();
    bool justAcquired = false;
    HRESULT hr = device->Acquire();
    if (hr == 0) {
        justAcquired = true;
    } else if (hr != 1) {
        if (!lost) {
            InputLostEvent(now);
            lost = 1;
        }
        return;
    }
    prevState = state;
    unsigned char buffer[0x100];
    std::memset(buffer, 0, sizeof(buffer));
    hr = device->GetDeviceState(sizeof(buffer), buffer);
    if (hr < 0) {
        if (!lost) {
            InputLostEvent(now);
            lost = 1;
        }
        return;
    }
    if (lost || justAcquired) {
        InputGetEvent(now);
        lost = 0;
    }
    for (unsigned long i = 0; i < 0x100; ++i) {
        const unsigned char key = g_scanToKey[i];
        if (key) state.keyboard.keys[key] = (buffer[i] & 0x80) ? 1 : 0;
    }
    if (GetKeyState(VK_CAPITAL) & 1) state.keyboard.keys[59] |= 1;
    if (GetKeyState(VK_SCROLL) & 1) state.keyboard.keys[71] |= 1;
    if (GetKeyState(VK_NUMLOCK) & 1) state.keyboard.keys[70] |= 1;
    const int caps = state.keyboard.keys[59] & 1;
    const int shift = (state.keyboard.keys[43] || state.keyboard.keys[55]) ? 1 : 0;
    const int ctrl = (state.keyboard.keys[30] || state.keyboard.keys[92]) ? 1 : 0;
    const int alt = (state.keyboard.keys[57] || state.keyboard.keys[93]) ? 1 : 0;
    const int num = state.keyboard.keys[70] & 1;
    const int scroll = state.keyboard.keys[71] & 1;

    DIDEVICEOBJECTDATA data[0x100];
    DWORD count = 0x100;
    hr = device->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), data, &count, 0);
    if (hr < 0) {
        if (!lost) {
            InputLostEvent(now);
            lost = 1;
        }
        return;
    }
    if (lost || justAcquired) {
        InputGetEvent(now);
        lost = 0;
    }
    for (DWORD i = 0; i < count; ++i) {
        const unsigned char key = g_scanToKey[data[i].dwOfs & 0xff];
        if (key && (data[i].dwData & 0x80)) state.keyboard.keys[key] = 1;
    }
    const int mods = (scroll << 26) | (shift ? 0x10000000 : 0) | (ctrl ? 0x20000000 : 0) |
                     (alt ? 0x08000000 : 0) | (caps ? 0x01000000 : 0) | (num ? 0x02000000 : 0);
    for (int key = 0; key < 0x100; ++key) {
        const unsigned char down = state.keyboard.keys[key];
        const unsigned char was = prevState.keyboard.keys[key];
        int eventFlags = 0;
        if (down && !was) {
            eventFlags = INPUT_DOWN;
        } else if (!down && was) {
            eventFlags = INPUT_UP;
        }
        if (!down) {
            keys[key].state = 0;
        } else if (!(eventFlags & INPUT_DOWN)) {
            const unsigned long delay = keys[key].state == 1 ? 400 : 80;
            if (keys[key].time + delay <= now) {
                keys[key].state = 2;
                keys[key].time = now;
                eventFlags |= INPUT_DOWN | INPUT_REPEAT;
            }
        } else {
            keys[key].state = 1;
            keys[key].time = now;
        }
        if (eventFlags) {
            KeyEvent(key, eventFlags | mods, now);
            if (eventFlags & (INPUT_DOWN | INPUT_REPEAT)) {
                const unsigned short ch =
                    ASCIICode::Get().GetChar(static_cast<KeyCode>(key), caps != 0, shift != 0, num != 0);
                CharEvent(ch, eventFlags | mods, now);
            }
        }
    }
}

// ---- DirectXMouse

DirectXMouse::DirectXMouse(InputDriver* driver) : InputDeviceDirectX(driver)  // 0x100056d0
{
    eventDelta[0] = eventDelta[1] = eventDelta[2] = 0;
    unknown248 = 0;
    for (int i = 0; i < 3; ++i) {
        range[i].min = 0;
        range[i].max = 0xffff;
        range[i].pad = 0;
        Threshold(i) = 0;
    }
    lost = 0;
    prevState.type = DEVICE_MOUSE;
    state.type = DEVICE_MOUSE;
    instanceId = UIdMouse;
}

bool DirectXMouse::Init(Window* window, IDirectInput8A* di, const UId&)  // 0x100059f0
{
    if (!window || !di) return false;
    if (di->CreateDevice(GUID_SysMouse, &device, nullptr) < 0) {
        Done();
        return false;
    }
    handle = device;
    if (device->SetDataFormat(&c_dfDIMouse2) < 0) goto fail;
    if (device->SetCooperativeLevel(static_cast<HWND>(window->GetHandle()), DISCL_EXCLUSIVE | DISCL_FOREGROUND) < 0)
        goto fail;
    {
        DIPROPDWORD prop;
        std::memset(&prop, 0, sizeof(prop));
        prop.diph.dwSize = sizeof(DIPROPDWORD);
        prop.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        prop.dwData = 0x200;
        if (device->SetProperty(DIPROP_BUFFERSIZE, &prop.diph) < 0) goto fail;
        prop.dwData = 1;
        if (device->SetProperty(DIPROP_AXISMODE, &prop.diph) < 0) goto fail;
    }
    device->Acquire();
    return true;
fail:
    Done();
    return false;
}

void DirectXMouse::GetCaps(InputDeviceCaps& c) const  // 0x10005780
{
    c.type = DEVICE_MOUSE;
    c.id = UIdMouse;
    SetText(c.name, "Mouse");
    SetText(c.description, "System mouse");
    c.caps = 0x3800;
    bool got = false;
    if (device) {
        DIDEVICEINSTANCEA inst;
        std::memset(&inst, 0, sizeof(inst));
        inst.dwSize = sizeof(inst);
        if (device->GetDeviceInfo(&inst) >= 0) {
            SetText(c.name, inst.tszInstanceName);
            SetText(c.description, inst.tszProductName);
            c.caps |= 0x3000000;
        }
        DIDEVCAPS dc;
        std::memset(&dc, 0, sizeof(dc));
        dc.dwSize = sizeof(dc);
        if (device->GetCapabilities(&dc) >= 0) {
            c.caps |= ((dc.dwFlags >> 7) & 2) | (dc.dwFlags & 1) | 0x64000000;
            c.numButtons = dc.dwButtons;
            c.numAxes = dc.dwAxes;
            got = true;
        }
    }
    if (!got) {
        c.caps |= 1;
        c.numButtons = 3;
        c.numAxes = 2;
    }
    c.caps |= 0x98000000;
    c.buttonMask = Bits(c.numButtons);
    c.axisMask = Bits(c.numAxes);
    c.numPovs = 0;
    c.unknown34 = 0;
}

bool DirectXMouse::GetProperty(DeviceProperty& p)  // 0x10005b00
{
    if (!valid) return false;
    switch (p.id) {
    case PROPERTY_RANGE:
        if (p.index >= 3) return false;
        p.a = range[p.index].min;
        p.b = range[p.index].max;
        return true;
    case PROPERTY_THRESHOLD:
        if (p.index >= 3) return false;
        p.a = Threshold(p.index);
        return true;
    case PROPERTY_POSITION:
        if (p.index >= 3) return false;
        p.a = state.mouse.position[p.index];
        return true;
    default:
        return false;
    }
}

bool DirectXMouse::SetProperty(const DeviceProperty& p)  // 0x10005b80
{
    if (!valid) return false;
    switch (p.id) {
    case PROPERTY_RANGE:
        if (p.index >= 3) return false;
        range[p.index].min = p.a;
        range[p.index].max = p.b;
        PropertyChangedEvent(p.id, Time::Ms());
        return true;
    case PROPERTY_THRESHOLD:
        if (p.index >= 3) return false;
        Threshold(p.index) = p.a;
        PropertyChangedEvent(p.id, Time::Ms());
        return true;
    case PROPERTY_POSITION:
        if (p.index >= 3) return false;
        state.mouse.position[p.index] = p.a;
        PropertyChangedEvent(p.id, Time::Ms());
        return true;
    default:
        return false;
    }
}

void DirectXMouse::Update(float)  // 0x10005c50
{
    if (!valid || !device) return;
    const unsigned long now = Time::Ms();
    HRESULT hr = device->Acquire();
    if (hr == 0) {
        InputGetEvent(now);
        lost = 0;
    } else if (hr != 1) {
        if (!lost) {
            InputLostEvent(now);
            lost = 1;
        }
        return;
    }
    prevState = state;
    DIMOUSESTATE2 ms;
    std::memset(&ms, 0, sizeof(ms));
    hr = device->GetDeviceState(sizeof(ms), &ms);
    if (hr < 0) {
        if (!lost) {
            InputLostEvent(now);
            lost = 1;
        }
        return;
    }
    std::memcpy(state.mouse.buttons, ms.rgbButtons, 8);
    for (int i = 0; i < 3; ++i) {
        int d = (&ms.lX)[i];
        if (!d || std::abs(d) < Threshold(i)) d = 0;
        state.mouse.delta[i] = d;
        int pos = state.mouse.position[i] + d;
        if (pos <= range[i].min) pos = range[i].min;
        if (pos > range[i].max) pos = range[i].max;
        state.mouse.position[i] = pos;
    }
    DIDEVICEOBJECTDATA data[0x200];
    DWORD count = 0x200;
    hr = device->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), data, &count, 0);
    if (hr < 0) {
        if (!lost) {
            InputLostEvent(now);
            lost = 1;
        }
        return;
    }
    if (lost) {
        InputGetEvent(now);
        lost = 0;
    }
    for (DWORD i = 0; i < count; ++i) {
        const DWORD ofs = data[i].dwOfs;
        if (ofs == DIMOFS_X || ofs == DIMOFS_Y || ofs == DIMOFS_Z) {
            const int axis = static_cast<int>(ofs / 4);
            const int d = static_cast<int>(data[i].dwData);
            if (d && std::abs(d) >= Threshold(axis)) {
                const int pos = state.mouse.position[axis] + d;
                const int clamped = pos <= range[axis].min ? range[axis].min
                                    : (pos > range[axis].max ? range[axis].max : pos);
                state.mouse.position[axis] = clamped;
                AxisEvent(axis, clamped, d, now);
            }
        } else if (ofs >= DIMOFS_BUTTON0 && ofs <= DIMOFS_BUTTON7) {
            const int button = static_cast<int>(ofs - DIMOFS_BUTTON0);
            ButtonEvent(button, (data[i].dwData & 0x80) ? INPUT_DOWN : INPUT_UP, now);
        }
    }
}

// ---- DirectXJoystick

DirectXJoystick::DirectXJoystick(InputDriver* driver) : InputDeviceDirectX(driver)  // 0x10002bf0
{
    window = nullptr;
    std::memset(threshold, 0, sizeof(threshold));
    std::memset(unknown260, 0, sizeof(unknown260));
    capCount = capCapacity = 0;
    caps = nullptr;
    flags = 0xf2;
    prevState.type = DEVICE_JOYSTICK;
    state.type = DEVICE_JOYSTICK;
    instanceId = UIdUnknown;
}

DirectXJoystick::~DirectXJoystick()  // 0x10002d10
{
    DestroyEffects();
    if (window) window->ClearWindowHook(this);
    if (caps) Alloc::Delete(caps);
    caps = nullptr;
    capCount = capCapacity = 0;
    InputDeviceDirectX::ReleaseDevice();
}

bool DirectXJoystick::Init(Window* window, IDirectInput8A* di, const UId& id)  // 0x10002df0
{
    if (!window || !di) return false;
    GUID instance;
    std::memcpy(&instance, &id, sizeof(instance));
    if (di->CreateDevice(instance, &device, nullptr) < 0) {
        Done();
        return false;
    }
    handle = device;
    cachedCaps.type = DEVICE_JOYSTICK;
    cachedCaps.id = id;
    SetText(cachedCaps.name, "Unknown");
    SetText(cachedCaps.description, "Unknown");
    cachedCaps.caps = 0x1000F800;
    cachedCaps.numButtons = cachedCaps.buttonMask = cachedCaps.numAxes = 0;
    cachedCaps.axisMask = cachedCaps.numPovs = cachedCaps.unknown34 = 0;
    {
        DIDEVICEINSTANCEA inst;
        std::memset(&inst, 0, sizeof(inst));
        inst.dwSize = sizeof(inst);
        if (device->GetDeviceInfo(&inst) >= 0) {
            SetText(cachedCaps.name, inst.tszInstanceName);
            SetText(cachedCaps.description, inst.tszProductName);
            cachedCaps.caps |= 0x3000000;
        }
    }
    device->EnumObjects(&EnumAxisCallback, &cachedCaps.axisMask, DIDFT_AXIS);
    DIDEVCAPS dc;
    std::memset(&dc, 0, sizeof(dc));
    dc.dwSize = sizeof(dc);
    if (device->GetCapabilities(&dc) >= 0) {
        cachedCaps.caps |= ((dc.dwFlags >> 7) & 2) | (dc.dwFlags & 1) | 0xEC000000;
        cachedCaps.numButtons = dc.dwButtons;
        cachedCaps.numAxes = dc.dwAxes;
        cachedCaps.numPovs = dc.dwPOVs;
        cachedCaps.buttonMask = Bits(cachedCaps.numButtons);
        cachedCaps.unknown34 = Bits(cachedCaps.numPovs);
    }
    prevState.type = DEVICE_JOYSTICK;
    state.type = DEVICE_JOYSTICK;
    instanceId = cachedCaps.id;
    if (device->SetDataFormat(&c_dfDIJoystick) < 0) goto fail;
    if (device->SetCooperativeLevel(static_cast<HWND>(window->GetHandle()), DISCL_EXCLUSIVE | DISCL_FOREGROUND) < 0)
        goto fail;
    {
        DIPROPDWORD prop;
        std::memset(&prop, 0, sizeof(prop));
        prop.diph.dwSize = sizeof(DIPROPDWORD);
        prop.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        prop.dwData = 0x200;
        if (device->SetProperty(DIPROP_BUFFERSIZE, &prop.diph) < 0) goto fail;
        prop.dwData = 0;
        if (device->SetProperty(DIPROP_AXISMODE, &prop.diph) < 0) goto fail;
    }
    {
        DIPROPRANGE range;
        std::memset(&range, 0, sizeof(range));
        range.diph.dwSize = sizeof(DIPROPRANGE);
        range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        range.diph.dwHow = DIPH_BYOFFSET;
        range.lMin = -32767;
        range.lMax = 32767;
        for (int axis = 0; axis < 8; ++axis) {
            range.diph.dwObj = DIJOFS_X + axis * 4;
            device->SetProperty(DIPROP_RANGE, &range.diph);
        }
    }
    if (cachedCaps.caps & 2) {
        if (device->EnumEffects(&DirectXJoystick::EnumEffectCallback, this, 0) >= 0 && capCount) {
            DIPROPDWORD prop;
            std::memset(&prop, 0, sizeof(prop));
            prop.diph.dwSize = sizeof(DIPROPDWORD);
            prop.diph.dwHeaderSize = sizeof(DIPROPHEADER);
            prop.dwData = 0;
            if (device->SetProperty(DIPROP_AUTOCENTER, &prop.diph) >= 0) {
                ResetEffects();
                MuteEffects(false);
                flags |= 8;
            } else {
                capCount = 0;
            }
        }
    }
    if (flags & 8) {
        DIPROPDWORD prop;
        std::memset(&prop, 0, sizeof(prop));
        prop.diph.dwSize = sizeof(DIPROPDWORD);
        prop.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        prop.dwData = 0;
        device->SetProperty(DIPROP_AUTOCENTER, &prop.diph);
        flags &= 0x7f;
    }
    if (device->Acquire() >= 0) flags |= 1;
    this->window = window;
    window->RegisterWindowHook(&Hook, this);
    return true;
fail:
    Done();
    return false;
}

void DirectXJoystick::GetCaps(InputDeviceCaps& c) const  // 0x10002b60
{
    c = cachedCaps;
}

bool DirectXJoystick::GetProperty(DeviceProperty& p)  // 0x10003400
{
    if (!valid) return false;
    switch (p.id) {
    case PROPERTY_RANGE:
        if (p.index >= 8) return false;
        {
            DIPROPRANGE range;
            std::memset(&range, 0, sizeof(range));
            range.diph.dwSize = sizeof(DIPROPRANGE);
            range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
            range.diph.dwHow = DIPH_BYOFFSET;
            range.diph.dwObj = DIJOFS_X + p.index * 4;
            if (device->GetProperty(DIPROP_RANGE, &range.diph) < 0) return false;
            p.a = range.lMin;
            p.b = range.lMax;
            return true;
        }
    case PROPERTY_THRESHOLD:
        if (p.index >= 8) return false;
        p.a = threshold[p.index];
        return true;
    case PROPERTY_POSITION:
        if (p.index >= 8) return false;
        p.a = state.joystick.position[p.index];
        return true;
    case static_cast<DevicePropertyID>(6):
    case static_cast<DevicePropertyID>(7):
        {
            DIPROPDWORD prop;
            std::memset(&prop, 0, sizeof(prop));
            prop.diph.dwSize = sizeof(DIPROPDWORD);
            prop.diph.dwHeaderSize = sizeof(DIPROPHEADER);
            prop.diph.dwHow = DIPH_BYOFFSET;
            prop.diph.dwObj = DIJOFS_X + p.index * 4;
            if (device->GetProperty(p.id == 6 ? DIPROP_DEADZONE : DIPROP_SATURATION, &prop.diph) < 0) return false;
            p.a = static_cast<int>(prop.dwData);
            return true;
        }
    case static_cast<DevicePropertyID>(8):
        {
            DIPROPDWORD prop;
            std::memset(&prop, 0, sizeof(prop));
            prop.diph.dwSize = sizeof(DIPROPDWORD);
            prop.diph.dwHeaderSize = sizeof(DIPROPHEADER);
            if (device->GetProperty(DIPROP_AUTOCENTER, &prop.diph) < 0) return false;
            p.index = prop.dwData == 1;
            return true;
        }
    case static_cast<DevicePropertyID>(10): p.index = (flags >> 4) & 1; return true;
    case static_cast<DevicePropertyID>(11): p.index = (flags >> 5) & 1; return true;
    case static_cast<DevicePropertyID>(12): p.index = (flags >> 6) & 1; return true;
    default:
        return false;
    }
}

bool DirectXJoystick::SetProperty(const DeviceProperty& p)  // 0x100036d0
{
    if (!valid) return false;
    device->Unacquire();
    switch (p.id) {
    case PROPERTY_RANGE:
        if (p.index >= 8) break;
        {
            DIPROPRANGE range;
            std::memset(&range, 0, sizeof(range));
            range.diph.dwSize = sizeof(DIPROPRANGE);
            range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
            range.diph.dwHow = DIPH_BYOFFSET;
            range.diph.dwObj = DIJOFS_X + p.index * 4;
            range.lMin = p.a;
            range.lMax = p.b;
            if (device->SetProperty(DIPROP_RANGE, &range.diph) < 0) break;
            PropertyChangedEvent(p.id, Time::Ms());
            AcquireDevice();
            return true;
        }
    case PROPERTY_THRESHOLD:
        if (p.index >= 8) break;
        threshold[p.index] = p.a;
        PropertyChangedEvent(p.id, Time::Ms());
        AcquireDevice();
        return true;
    case PROPERTY_POSITION:
    case static_cast<DevicePropertyID>(9):
        break;
    case static_cast<DevicePropertyID>(6):
    case static_cast<DevicePropertyID>(7):
        {
            DIPROPDWORD prop;
            std::memset(&prop, 0, sizeof(prop));
            prop.diph.dwSize = sizeof(DIPROPDWORD);
            prop.diph.dwHeaderSize = sizeof(DIPROPHEADER);
            prop.diph.dwHow = DIPH_BYOFFSET;
            prop.diph.dwObj = DIJOFS_X + p.index * 4;
            prop.dwData = static_cast<DWORD>(p.a);
            if (device->SetProperty(p.id == 6 ? DIPROP_DEADZONE : DIPROP_SATURATION, &prop.diph) < 0) break;
            PropertyChangedEvent(p.id, Time::Ms());
            AcquireDevice();
            return true;
        }
    case static_cast<DevicePropertyID>(8):
        flags = static_cast<unsigned char>((flags & 0x7f) | (p.index << 7));
        {
            DIPROPDWORD prop;
            std::memset(&prop, 0, sizeof(prop));
            prop.diph.dwSize = sizeof(DIPROPDWORD);
            prop.diph.dwHeaderSize = sizeof(DIPROPHEADER);
            prop.dwData = p.index != 0;
            if (device->SetProperty(DIPROP_AUTOCENTER, &prop.diph) < 0) break;
            PropertyChangedEvent(p.id, Time::Ms());
            AcquireDevice();
            return true;
        }
    case static_cast<DevicePropertyID>(10):
        // the original has no break here: 10 also writes 11 and 12
        flags = static_cast<unsigned char>((flags & ~0x10) | ((p.index & 1) << 4));
        PropertyChangedEvent(p.id, Time::Ms());
        // fall through
    case static_cast<DevicePropertyID>(11):
        flags = static_cast<unsigned char>((flags & ~0x20) | ((p.index & 1) << 5));
        PropertyChangedEvent(p.id, Time::Ms());
        // fall through
    case static_cast<DevicePropertyID>(12):
        flags = static_cast<unsigned char>((flags & ~0x40) | ((p.index & 1) << 6));
        PropertyChangedEvent(p.id, Time::Ms());
        AcquireDevice();
        return true;
    default:
        break;
    }
    AcquireDevice();
    return false;
}

ForceEffect* DirectXJoystick::CreateEffect(const PString& name, const ResourceDatabase* db, unsigned long flags)  // 0x100047d0
{
    if (!device) return nullptr;
    device->Acquire();
    ForceEffectDirectX* const effect = new ForceEffectDirectX(this);
    if (!effect) return nullptr;
    if (!effect->Create(name, db, flags)) {
        effect->ReleaseReference();
        return nullptr;
    }
    effect->info.unknown00 |= CapabilityMask(effect->info.type);
    effect->AddReference();
    ForceEffect* const raw = effect;
    effects.Add(raw);
    return raw;
}

ForceEffect* DirectXJoystick::CreateEffect(const ForceInfo& info, unsigned long flags)  // 0x100046e0
{
    if (!device) return nullptr;
    device->Acquire();
    ForceEffectDirectX* const effect = new ForceEffectDirectX(this);
    if (!effect) return nullptr;
    if (!effect->Create(info, flags)) {
        effect->ReleaseReference();
        return nullptr;
    }
    effect->info.unknown00 |= CapabilityMask(info.type);
    effect->AddReference();
    ForceEffect* const raw = effect;
    effects.Add(raw);
    return raw;
}

unsigned long DirectXJoystick::CapabilityMask(unsigned long type) const  // 0x10002b80
{
    if (!(flags & 8) || !capCount) return 0;
    for (unsigned long i = 0; i < capCount; ++i) {
        if (caps[i].type != type) continue;
        unsigned long v = caps[i].staticParams | caps[i].dynamicParams;
        // the original reshuffles the DIEFFECTINFO flags into the ForceInfo flag word
        v = ((v & 0x300) >> 1) | (((v & 2) * 2 | (v & 0x18)) * 2 | (v & 5)) * 2 |
            (((v >> 3) & 0x10 | (v & 0x40)) >> 4);
        return v;
    }
    return 0;
}

bool DirectXJoystick::AcquireDevice()  // 0x10003a50
{
    if (flags & 1) return true;
    if (!device) return false;
    {
        DIPROPDWORD prop;
        std::memset(&prop, 0, sizeof(prop));
        prop.diph.dwSize = sizeof(DIPROPDWORD);
        prop.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        prop.dwData = flags >> 7;
        device->SetProperty(DIPROP_AUTOCENTER, &prop.diph);
    }
    const HRESULT hr = device->Acquire();
    if (hr == 0 || hr == 1) {
        flags |= 1;
        return true;
    }
    return false;
}

void DirectXJoystick::DestroyEffects()  // 0x100042f0 / 0x10003310
{
    for (FastDynamicArray<ForceEffect*, 13>::Iterator it = effects.First(); it.node;) {
        ForceEffect* const e = *it;
        it = effects.Remove(it);
        static_cast<ForceEffectDirectX*>(e)->ShutdownEffect();
        e->ReleaseReference();
    }
    if (window) window->ClearWindowHook(this);
}

void DirectXJoystick::Done()  // 0x10003310
{
    DestroyEffects();
    if (device && (flags & 8)) {
        DIPROPDWORD prop;
        std::memset(&prop, 0, sizeof(prop));
        prop.diph.dwSize = sizeof(DIPROPDWORD);
        prop.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        device->SetProperty(DIPROP_AUTOCENTER, &prop.diph);
        flags &= 0xf7;
    }
    if (device) {
        device->Unacquire();
        device->Release();
        device = nullptr;
    }
    handle = nullptr;
    valid = false;
}

void DirectXJoystick::CriticalShutdown()  // 0x100042f0
{
    DestroyEffects();
}

void DirectXJoystick::UpdateEffects(float dt)  // part of 0x10003ad0
{
    if (!(flags & 8)) return;
    for (FastDynamicArray<ForceEffect*, 13>::Iterator it = effects.First(); it.node;) {
        ForceEffect* const e = *it;
        if (e->GetReferenceCount() == 1) {
            it = effects.Remove(it);
            e->ReleaseReference();
            continue;
        }
        ForceEffectDirectX* const fx = static_cast<ForceEffectDirectX*>(e);
        unsigned char& ef = fx->flags;
        if ((ef & 2) && (flags & 0x10) && (ef & 1)) fx->Load();
        const bool infinite = (ef >> 2) & 1;
        if (((infinite || (ef & 8)) && !(ef & 2)) && (ef & 1) && (ef & 0x10) && (flags & 0x20))
            fx->Start(infinite ? 0xffffffff : 1, false);
        // the original tests bit 6 of info.unknown00 here but sets bit 6 of the effect flags (kept)
        if (!(fx->info.unknown00 & 0x40) && (flags & 0x40) && (ef & 0x20)) {
            const int button = static_cast<int>(fx->info.triggerButton);
            if (button < 0 || button >= 32 ||
                !(state.joystick.buttons[button] & 0x80)) {
                ef &= ~0x40;
            } else if (ef & 0x40) {
                fx->time += dt;
                if (fx->time >= fx->duration) {
                    fx->Start(1, false);
                    fx->time = 0;
                }
            } else {
                ef |= 0x40;
                fx->time = 0;
            }
        }
        ++it;
    }
}

void DirectXJoystick::Update(float dt)
{
    if (!device || !(flags & 2)) return;
    if (!AcquireDevice()) return;
    if (device->Poll() < 0) {
        flags &= 0xfe;
        if (!AcquireDevice()) return;
        if (device->Poll() < 0) return;
    }
    prevState = state;
    DIJOYSTATE js;
    std::memset(&js, 0, sizeof(js));
    if (device->GetDeviceState(sizeof(js), &js) < 0) return;
    const long* const rawAxes = &js.lX;
    unsigned long buttons = 0;
    for (int i = 0; i < 32; ++i) {
        const unsigned char b = js.rgbButtons[i] & 0x80;
        state.joystick.buttons[i] = b;
        if (!b) {
            state.joystick.pressure[i] = 0;
        } else {
            state.joystick.pressure[i] = 0xff;
            buttons |= 1u << i;
        }
    }
    for (int i = 0; i < 8; ++i) {
        if (!(cachedCaps.axisMask & (1u << i))) {
            state.joystick.position[i] = 0;
            state.joystick.delta[i] = 0;
            continue;
        }
        const int value = rawAxes[i];
        int delta = value - state.joystick.position[i];
        if (delta < 0) delta = -delta;
        if (delta > threshold[i]) {
            state.joystick.position[i] = value;
            state.joystick.delta[i] = value - prevState.joystick.position[i];
        }
    }
    for (int i = 0; i < 4; ++i) {
        unsigned long v = js.rgdwPOV[i];
        if (v > 0xfffe) v = 0xffffffff;
        state.joystick.pov[i] = static_cast<int>(v);
    }
    DIDEVICEOBJECTDATA data[0x200];
    DWORD count = 0x200;
    HRESULT hr = device->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), data, &count, 0);
    if (hr < 0) {
        if (!(flags & 4)) {
            InputLostEvent(Time::Ms());
            flags |= 4;
        }
        return;
    }
    if (flags & 4) {
        InputGetEvent(Time::Ms());
        flags &= ~4;
    }
    const unsigned long now = Time::Ms();
    for (DWORD i = 0; i < count; ++i) {
        const DWORD ofs = data[i].dwOfs;
        const int value = static_cast<int>(data[i].dwData);
        if (ofs < 0x20) {
            const int axis = static_cast<int>(ofs / 4);
            if (value && std::abs(value) >= threshold[axis]) {
                const int pos = state.joystick.position[axis] + value;
                state.joystick.position[axis] = pos;
                AxisEvent(axis, pos, value, now);
            }
        } else if (ofs < 0x30) {
            const int pov = static_cast<int>((ofs - 0x20) / 4);
            const unsigned long v = static_cast<unsigned long>(value) > 0xfffe ? 0xffffffff
                                                                               : static_cast<unsigned long>(value);
            state.joystick.pov[pov] = static_cast<int>(v);
            PovEvent(pov, static_cast<int>(v), now);
        } else if (ofs < 0x50) {
            const int button = static_cast<int>(ofs - 0x30);
            ButtonEvent(button, (value & 0x80) ? INPUT_DOWN : INPUT_UP, now);
        }
    }
    UpdateEffects(dt);
}

BOOL CALLBACK DirectXJoystick::EnumEffectCallback(LPCDIEFFECTINFOA info, LPVOID context)  // 0x10004380
{
    DirectXJoystick* const self = static_cast<DirectXJoystick*>(context);
    if (!info || !self) return DIENUM_CONTINUE;
    unsigned long type = 0;
    for (unsigned long t = 1; t <= 12; ++t) {
        const GUID* const g = EffectGuid(t);
        if (g && IsEqualGUID(info->guid, *g)) {
            type = t;
            break;
        }
    }
    if (type) {
        if (self->capCount >= self->capCapacity) {
            const unsigned long capacity = self->capCapacity + 6;
            auto* const block = static_cast<DirectXJoystick::EffectCap*>(Alloc::New(capacity * sizeof(EffectCap)));
            if (self->caps) {
                std::memcpy(block, self->caps, self->capCount * sizeof(EffectCap));
                Alloc::Delete(self->caps);
            }
            self->caps = block;
            self->capCapacity = capacity;
        }
        EffectCap& cap = self->caps[self->capCount++];
        cap.type = type;
        cap.effectType = info->dwEffType;
        cap.staticParams = info->dwStaticParams;
        cap.dynamicParams = info->dwDynamicParams;
    }
    return DIENUM_CONTINUE;
}

bool __fastcall DirectXJoystick::Hook(void* self, void* message, int&)  // 0x10004230
{
    DirectXJoystick* const joy = static_cast<DirectXJoystick*>(self);
    const Window::HookMessage* const m = static_cast<const Window::HookMessage*>(message);
    if (m->id != WM_ACTIVATEAPP) return false;
    unsigned long now = Time::Ms();
    if (!m->wParam) {
        joy->InputLostEvent(now);
        joy->flags = static_cast<unsigned char>((joy->flags & 0xfc) | 4);
        for (FastDynamicArray<ForceEffect*, 13>::Iterator it = joy->effects.First(); it.node; ++it) {
            ForceEffectDirectX* const fx = static_cast<ForceEffectDirectX*>(*it);
            fx->flags = static_cast<unsigned char>((fx->flags | 2 | 0x10) & 0xbf);
        }
    } else {
        joy->flags = static_cast<unsigned char>((joy->flags & 0xfb) | 2);
        joy->AcquireDevice();
        joy->InputGetEvent(now);
    }
    return false;
}

// ---- the driver

namespace {

struct EnumContext {
    InputDeviceCapsList* list;
    IDirectInput8A* di;
};

BOOL CALLBACK EnumDevicesCallback(LPCDIDEVICEINSTANCEA inst, LPVOID context)  // 0x10002660
{
    EnumContext* const ctx = static_cast<EnumContext*>(context);
    if (!inst || !ctx || !ctx->list || !ctx->di) return DIENUM_CONTINUE;
    int type = 0;
    switch (inst->dwDevType & 0xff) {
    case DI8DEVTYPE_MOUSE:
    case DI8DEVTYPE_SCREENPOINTER: type = DEVICE_MOUSE; break;
    case DI8DEVTYPE_KEYBOARD: type = DEVICE_KEYBOARD; break;
    case DI8DEVTYPE_JOYSTICK:
    case DI8DEVTYPE_GAMEPAD:
    case DI8DEVTYPE_DRIVING:
    case DI8DEVTYPE_FLIGHT:
    case DI8DEVTYPE_1STPERSON:
    case DI8DEVTYPE_SUPPLEMENTAL: type = DEVICE_JOYSTICK; break;
    default: return DIENUM_CONTINUE;
    }
    DIDEVCAPS dc;
    std::memset(&dc, 0, sizeof(dc));
    dc.dwSize = sizeof(dc);
    IDirectInputDevice8A* dev = nullptr;
    if (ctx->di->CreateDevice(inst->guidInstance, &dev, nullptr) < 0 || !dev) return DIENUM_CONTINUE;
    unsigned long axisMask = 0;
    dev->EnumObjects(&EnumAxisCallback, &axisMask, DIDFT_AXIS);
    dev->GetCapabilities(&dc);
    dev->Release();

    InputDeviceCaps c;
    c.type = type;
    if (SameId(&inst->guidInstance, &GUID_SysKeyboard))
        c.id = UIdKeyboard;
    else if (SameId(&inst->guidInstance, &GUID_SysMouse))
        c.id = UIdMouse;
    else
        c.id = ToUid(inst->guidInstance);
    SetText(c.name, inst->tszInstanceName);
    SetText(c.description, inst->tszProductName);
    c.caps = ((dc.dwFlags >> 7) & 2) | (dc.dwFlags & 1);
    c.numButtons = dc.dwButtons;
    c.buttonMask = Bits(c.numButtons);
    c.numAxes = dc.dwAxes;
    c.axisMask = 0;  // the original leaves this 0 (the axis mask stays local)
    c.numPovs = dc.dwPOVs;
    c.unknown34 = Bits(c.numPovs);
    ctx->list->Add(c);
    return DIENUM_CONTINUE;
}

// DirectInput scan code -> Jet key code (0x100053c0, the module's static initializer)
const bool g_scanMapReady = [] {
    std::memset(g_scanToKey, 0, sizeof(g_scanToKey));
    unsigned char* m = g_scanToKey;
    m[DIK_ESCAPE] = 1;
    m[DIK_PAUSE] = 2;
    static const unsigned char digits[] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    for (int i = 0; i < 10; ++i) m[DIK_1 + i] = digits[i];
    m[DIK_MINUS] = 13;
    m[DIK_EQUALS] = 14;
    m[DIK_BACK] = 15;
    m[DIK_TAB] = 16;
    static const unsigned char rowQ[] = {17, 18, 19, 20, 21, 22, 23, 24, 25, 26};
    for (int i = 0; i < 10; ++i) m[DIK_Q + i] = rowQ[i];
    m[DIK_LBRACKET] = 27;
    m[DIK_RBRACKET] = 28;
    m[DIK_RETURN] = 29;
    m[DIK_LCONTROL] = 30;
    static const unsigned char rowA[] = {31, 32, 33, 34, 35, 36, 37, 38, 39};
    for (int i = 0; i < 9; ++i) m[DIK_A + i] = rowA[i];
    m[DIK_SEMICOLON] = 40;
    m[DIK_APOSTROPHE] = 41;
    m[DIK_GRAVE] = 42;
    m[DIK_LSHIFT] = 43;
    m[DIK_BACKSLASH] = 44;
    static const unsigned char rowZ[] = {45, 46, 47, 48, 49, 50, 51};
    for (int i = 0; i < 7; ++i) m[DIK_Z + i] = rowZ[i];
    m[DIK_COMMA] = 52;
    m[DIK_PERIOD] = 53;
    m[DIK_SLASH] = 54;
    m[DIK_RSHIFT] = 55;
    m[DIK_MULTIPLY] = 56;
    m[DIK_LMENU] = 57;
    m[DIK_SPACE] = 58;
    m[DIK_CAPITAL] = 59;
    for (int i = 0; i < 10; ++i) m[DIK_F1 + i] = static_cast<unsigned char>(60 + i);
    m[DIK_NUMLOCK] = 70;
    m[DIK_SCROLL] = 71;
    for (int i = 0; i < 10; ++i) m[DIK_NUMPAD7 + i] = static_cast<unsigned char>(72 + i);
    m[DIK_DECIMAL] = 84;
    m[DIK_F11] = 85;
    m[DIK_F12] = 86;
    m[DIK_F13] = 87;
    m[DIK_F14] = 88;
    m[DIK_F15] = 89;
    m[DIK_NUMPADENTER] = 90;
    m[DIK_DIVIDE] = 91;
    m[DIK_RCONTROL] = 92;
    m[DIK_RMENU] = 93;
    m[DIK_HOME] = 94;
    m[DIK_UP] = 95;
    m[DIK_PRIOR] = 96;
    m[DIK_LEFT] = 97;
    m[DIK_RIGHT] = 98;
    m[DIK_END] = 99;
    m[DIK_DOWN] = 100;
    m[DIK_NEXT] = 101;
    m[DIK_INSERT] = 102;
    m[DIK_DELETE] = 103;
    m[DIK_LWIN] = 104;
    m[DIK_RWIN] = 105;
    m[DIK_APPS] = 106;
    m[DIK_SYSRQ] = 107;
    return true;
}();

}  // namespace

void InputDriverDirectX::ReleaseDirectInput()
{
    if (di) {
        di->Release();
        di = nullptr;
    }
}

bool InputDriverDirectX::Init(OS* o)  // 0x10002230
{
    if (!o || !o->GetWindow() || !o->GetWindow()->GetHandle()) return false;
    os = o;
    return DirectInput8Create(GetModuleHandleA(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8A,
                              reinterpret_cast<void**>(&di), nullptr) >= 0;
}

void InputDriverDirectX::Enumerate(InputDeviceCapsList& list, int type)  // 0x10002290
{
    if (!di) return;
    DWORD devType;
    switch (type) {
    case DEVICE_KEYBOARD: devType = 3; break;  // DIDEVTYPE_KEYBOARD (legacy filter value the original passes)
    case DEVICE_MOUSE: devType = 2; break;
    case DEVICE_JOYSTICK: devType = 4; break;
    default: return;
    }
    EnumContext ctx = {&list, di};
    di->EnumDevices(devType, &EnumDevicesCallback, &ctx, DIEDFL_ALLDEVICES);
}

AutoReference<InputDevice> InputDriverDirectX::CreateDevice(const UId& id)  // 0x10002320
{
    AutoReference<InputDevice> result;
    if (!di || !os || !os->GetWindow()) return result;
    Window* const window = os->GetWindow();
    InputDevice* device = nullptr;
    if (SameId(&id, &UIdKeyboard))
        device = new DirectXKeyboard(this);
    else if (SameId(&id, &UIdMouse))
        device = new DirectXMouse(this);
    else
        device = new DirectXJoystick(this);
    if (!device) return result;
    if (static_cast<InputDeviceDirectX*>(device)->Init(window, di, id)) {
        result = device;
    } else {
        device->ReleaseReference();
    }
    return result;
}

// 0x10001ec0 (JetGetPluginsInputDirectX)
extern "C" void __cdecl JetGetPluginsInputDirectX()
{
    new PluginFactoryOf<InputDriverDirectX, InputDriver>(PString("DirectX"),
                                                         UId("D2488477-E537-4542-9ED2-554C93E28F3C"),
                                                         UId("F3117B18-F364-41a1-8993-966236A35B5E"));
}

}  // namespace Jet

