/*
 * Copyright (C) 2026 Thanasis Petsas <thanpetsas@gmail.com>
 * Licence: MIT Licence
 */

#include "DualsenseMod.h"

#include "Logger.h"
#include "Config.h"
#include "Utils.h"
#include "rva/RVA.h"
#include "minhook/include/MinHook.h"

// headers needed for dualsensitive
#include <udp.h>
#include <dualsensitive.h>
#include <IO.h>
#include <Device.h>
#include <Helpers.h>
#include <iostream>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <cinttypes>
#include <cstdlib>
#include <memory>
#include <algorithm>
#include <string>
#include <sstream>
#include <thread>
#include <chrono>
#include <mutex>
#include <map>

#define INI_LOCATION "./mods/dualsense-mod.ini"

#define RETURN_IF_GAME_NOT_STARTED() \
    do { if (!g_gameStarted) return; } while (0)

// TODO: move the following to a server utils file

PROCESS_INFORMATION serverProcInfo;


#include <shellapi.h>

struct TriggerSetting {
    TriggerProfile profile;
    bool isCustomTrigger = false;
    TriggerMode mode = TriggerMode::Off;
    std::vector<uint8_t> extras;

    TriggerSetting(TriggerProfile profile, std::vector<uint8_t> extras) :
        profile(profile), extras(extras) {}

    TriggerSetting(TriggerMode mode, std::vector<uint8_t> extras) :
        mode(mode), extras(extras), isCustomTrigger(true) {}

};

struct Triggers {
    TriggerSetting *L2;
    TriggerSetting *R2;
};

// Globals
Config g_config;
Logger g_logger;

std::map<std::string, Triggers> g_TriggerSettings ;

HMODULE g_deadspaceBaseAddr = nullptr;
static uintptr_t g_base = 0;

int g_menuScreensUntilGameStart = 2;
bool g_gameStarted = false;


// holds the id of the current weapon
static std::atomic<int>  g_currWeaponId{-1};

std::string g_Weapons[8] = {
    "Plasma Cutter", // 0
    "Pulse Rifle",   // 1
    "Flamethrower",  // 2
    "Force Gun",     // 3
    "Line Gun",      // 4
    "Ripper",        // 5
    "Contact Beam",  // 6
    "Hand Cannon"    // 7
};

bool g_HasAmmoInClip[8] = {
    true, true, true, true, true, true, true, true
};

void InitTriggerSettings() {
    g_TriggerSettings =
    {
        {
            "Plasma Cutter",
            {
                .L2 = new TriggerSetting (
                        TriggerProfile::Choppy, {}
                ),
                /*
                .R2 = new TriggerSetting (
                        TriggerProfile::Bow,
                        {1, 4, 3, 2}
                )*/
                .R2 = new TriggerSetting(TriggerProfile::Soft, {})
            }
        },
        {
            "Flamethrower",
            {
                .L2 = new TriggerSetting (
                        TriggerMode::Rigid, {}
                ),
                .R2 = new TriggerSetting (
                        TriggerProfile::Galloping,
                        {2, 9, 1, 2, 80}
                        //{3, 9, 1, 2, 80}
                        //{1, 3, 1, 6, 40}
                )
            }
        },
        {
            "Pulse Rifle",
            {
                .L2 = new TriggerSetting (
                        TriggerMode::Rigid, {}
                ),
                .R2 = new TriggerSetting (
                        TriggerProfile::MultiplePositionVibration,
                        //{14, 0, 1, 2, 3, 4, 5, 6, 7, 8, 8}
                        //{15, 0, 1, 4, 6, 7, 8, 8, 7, 6, 4}
                        {14, 0, 1, 2, 3, 4, 5, 6, 7, 7, 7}
                )
            }
        },
        {
            "Force Gun",
            {
                .L2 = new TriggerSetting (
                        TriggerProfile::Choppy, {}
                ),
                .R2 = new TriggerSetting (
                        TriggerProfile::Bow,
                        {0, 4, 8, 8}
                )
            }
        },
        {
            "Line Gun",
            {
                .L2 = new TriggerSetting (
                        TriggerMode::Rigid_A,
                        {60, 71, 56, 128, 195, 210, 255}
                ),
                .R2 = new TriggerSetting (
                    TriggerProfile::SlopeFeedback,
                    {0, 8, 8, 1}
                )
            }
        },
        {
            "Ripper",
            {
                .L2 = new TriggerSetting (
                        TriggerProfile::Machine,
                        {1, 8, 3, 3, 184, 0}
                ),
                .R2 = new TriggerSetting (
                        TriggerMode::Pulse_B,
                        {238, 215, 66, 120, 43, 160, 215}
                )
            }
        },
        {
            "RipperWithSaw",
            {
                .L2 = new TriggerSetting (
                        TriggerProfile::Machine,
                        {1, 9, 7, 7, 75, 0}
                        //{1, 9, 5, 6, 75, 0}
                        //{1, 9, 7, 7, 65, 0}
                        //{1, 9, 5, 6, 75, 0}
                ),
                .R2 = new TriggerSetting (
                        TriggerMode::Pulse_B,
                        {238, 215, 66, 120, 43, 160, 215}
                )
            }
        },
        {
            "Contact Beam", //"weapon/zion/player/sp/gauss_rifle",
            {
                .L2 = new TriggerSetting (
                        TriggerProfile::Machine,
                        {7, 9, 0, 1, 8, 1}
                ),
                .R2 = new TriggerSetting (
                        TriggerProfile::Galloping,
                        {1, 3, 1, 6, 40}
                )
            }
        },
        {
            "ContactBeamActive", //"weapon/zion/player/sp/gauss_rifle",
            {
                .L2 = new TriggerSetting (
                        TriggerProfile::Machine,
                        {7, 9, 0, 1, 8, 1}
                ),
                .R2 = new TriggerSetting (
                        TriggerMode::Pulse_B,
                        {109, 54, 66, 80, 43, 80, 80}
                )

#if 0
                .R2 = new TriggerSetting (

                    TriggerMode::Pulse_AB,
                    {18, 197, 35, 58, 90, 120, 138}
                    //    TriggerProfile::Galloping,
                     //   {1, 3, 1, 6, 40}
                )
#endif
            }
        },
        {
            "Hand Cannon",
            {
                .L2 = new TriggerSetting (
                    TriggerProfile::Resistance, {8,1}
                ),
                /*.R2 = new TriggerSetting (
                    TriggerMode::Pulse_AB,
                    {18, 197, 35, 58, 90, 120, 138}
                    */

                .R2 = new TriggerSetting (
                    TriggerProfile::Resistance, {2, 1}
                ),
            }
        },
#if 0
        /*
           {
            "weapon/zion/player/sp/pistol",
            {
                .L2 = new TriggerSetting (
                        TriggerProfile::Galloping,
                        {3, 9, 1, 2, 30}
                ),
                .R2 = new TriggerSetting (
                        TriggerProfile::Bow,
                        {1, 4, 3, 2}
                )
            }
        },
        {
            "weapon/zion/player/sp/shotgun",
            {
                .L2 = new TriggerSetting (
                        TriggerProfile::Choppy, {}
                ),
                .R2 = new TriggerSetting (
                        TriggerProfile::Bow,
                        {0, 4, 8, 8}
                )
            }
        },
        {
            "weapon/zion/player/sp/shotgun_mod",
            {
                .L2 = new TriggerSetting (
                        TriggerMode::Rigid, {}
                ),
                .R2 = new TriggerSetting (
                        TriggerProfile::MultiplePositionFeeback,
                        {4, 7, 0, 2, 4, 6, 0, 3, 6, 0}
                )
            }
        },
        {
            "weapon/zion/player/sp/plasma_rifle",
            {
                .L2 = new TriggerSetting (
                        TriggerProfile::Choppy, {}
                ),
                .R2 = new TriggerSetting (
                        TriggerProfile::Vibration,
                        {0, 4, 10}
                )
            }
        },*/
        {
            "weapon/zion/player/sp/rocket_launcher",
            {
                .L2 = new TriggerSetting (
                        TriggerProfile::Choppy, {}
                ),
                .R2 = new TriggerSetting (
                        TriggerProfile::Bow,
                        {0, 3, 8, 8}
                )

            }
        },
        {
            "weapon/zion/player/sp/rocket_launcher_mod",
            {
                .L2 = new TriggerSetting (
                        TriggerMode::Rigid, {}
                ),
                .R2 = new TriggerSetting (
                        TriggerMode::Rigid_A,
                        {209, 42, 232, 192, 232, 209, 232}
                )
            }
        },
        {
            "weapon/zion/player/sp/chaingun",
            {
                .L2 = new TriggerSetting (
                        TriggerProfile::Vibration,
                        {1, 10, 8}
                ),
                .R2 = new TriggerSetting (
                        TriggerProfile::MultiplePositionVibration,
                        {11, 1, 3, 5, 7, 7, 8, 8, 8, 8, 8}
                )
            }
        },
        {
            "weapon/zion/player/sp/chaingun_mod",
            {
                .L2 = new TriggerSetting (
                        TriggerProfile::SlopeFeedback,
                        {0, 5, 1, 8}
                ),
                .R2 = new TriggerSetting (
                        TriggerProfile::MultiplePositionVibration,
                        {21, 1, 3, 5, 7, 7, 8, 8, 8, 8, 8}
                )
            }
        },
        {
            "weapon/zion/player/sp/gauss_rifle_mod",
            {
                .L2 = new TriggerSetting (
                        TriggerProfile::Machine,
                        {4, 9, 1, 2, 40, 0}
                ),
                .R2 = new TriggerSetting (
                        TriggerProfile::Galloping,
                        {1, 3, 1, 6, 40}
                )
            }
        },
#endif
    };
}


void ResetAdaptiveTriggers() {
    dualsensitive::setLeftTrigger(TriggerProfile::Normal);
    dualsensitive::setRightTrigger(TriggerProfile::Normal);
    _LOGD("Adaptive Triggers reset successfully!");
}

void ResetRightTrigger() {
    dualsensitive::setRightTrigger(TriggerProfile::Normal);
    _LOGD("Right Adaptive Trigger reset successfully!");
}

void ResetLeftTrigger() {
    dualsensitive::setLeftTrigger(TriggerProfile::Normal);
    _LOGD("Left Adaptive Trigger reset successfully!");
}

void SendLeftTrigger() {
    RETURN_IF_GAME_NOT_STARTED();
    int weaponId = g_currWeaponId.load(std::memory_order_relaxed);
    if (!g_HasAmmoInClip[ weaponId ])
        return;
    std::string weaponName = g_Weapons[ weaponId ];
    Triggers t = g_TriggerSettings[ weaponName ];
    if (t.L2->isCustomTrigger)
        dualsensitive::setLeftCustomTrigger(t.L2->mode, t.L2->extras);
    else
        dualsensitive::setLeftTrigger (t.L2->profile, t.L2->extras);
    _LOGD("Left Adaptive Trigger settings sent successfully!");
}

void SendLeftTrigger(std::string weaponName) {
    RETURN_IF_GAME_NOT_STARTED();
    int weaponId = g_currWeaponId.load(std::memory_order_relaxed);
    if (!g_HasAmmoInClip[ weaponId ])
        return;
    Triggers t = g_TriggerSettings[ weaponName ];
    if (t.L2->isCustomTrigger)
        dualsensitive::setLeftCustomTrigger(t.L2->mode, t.L2->extras);
    else
        dualsensitive::setLeftTrigger (t.L2->profile, t.L2->extras);
    _LOGD("Left Adaptive Trigger settings sent successfully!");
}

void SendRightTrigger(std::string weaponName) {
    RETURN_IF_GAME_NOT_STARTED();
    int weaponId = g_currWeaponId.load(std::memory_order_relaxed);
    if (!g_HasAmmoInClip[ weaponId ])
        return;
    Triggers t = g_TriggerSettings[ weaponName ];
    if (t.R2->isCustomTrigger)
        dualsensitive::setRightCustomTrigger(t.R2->mode, t.R2->extras);
    else
        dualsensitive::setRightTrigger (t.R2->profile, t.R2->extras);
    _LOGD("Right Adaptive Trigger settings sent successfully!");
}

void ForceSendLeftTrigger() {
    RETURN_IF_GAME_NOT_STARTED();
    int weaponId = g_currWeaponId.load(std::memory_order_relaxed);
    std::string weaponName = g_Weapons[ weaponId ];
    Triggers t = g_TriggerSettings[weaponName];
    if (t.L2->isCustomTrigger)
        dualsensitive::setLeftCustomTrigger(t.L2->mode, t.L2->extras);
    else
        dualsensitive::setLeftTrigger(t.L2->profile, t.L2->extras);

    _LOGD("Left Adaptive Trigger forced: %s", weaponName.c_str());
}

void ForceSendRightTrigger() {
    RETURN_IF_GAME_NOT_STARTED();
    int weaponId = g_currWeaponId.load(std::memory_order_relaxed);
    std::string weaponName = g_Weapons[ weaponId ];
    Triggers t = g_TriggerSettings[weaponName];
    if (t.R2->isCustomTrigger)
        dualsensitive::setRightCustomTrigger(t.R2->mode, t.R2->extras);
    else
        dualsensitive::setRightTrigger(t.R2->profile, t.R2->extras);

    _LOGD("Right Adaptive Trigger forced: %s", weaponName.c_str());
}

void SendRightTrigger() {
    RETURN_IF_GAME_NOT_STARTED();
    int weaponId = g_currWeaponId.load(std::memory_order_relaxed);
    if (!g_HasAmmoInClip[ weaponId ]) {
        // clanky trigger for simulating no ammo
        dualsensitive::setRightTrigger(TriggerProfile::GameCube);
        return;
    }
    std::string weaponName = g_Weapons[ weaponId ];
    Triggers t = g_TriggerSettings[ weaponName ];
    if (t.R2->isCustomTrigger)
        dualsensitive::setRightCustomTrigger(t.R2->mode, t.R2->extras);
    else
        dualsensitive::setRightTrigger (t.R2->profile, t.R2->extras);
    _LOGD("Right Adaptive Trigger settings sent successfully!");
}

void SendTriggers() {
    RETURN_IF_GAME_NOT_STARTED();
    SendLeftTrigger();
    SendRightTrigger();
}


// Kinesis FSM

enum class KinesisState {
    Idle,
    Armed,      // L2 down + Circle
    Active      // Kinesis' target (npc_id) confirmed
};

static constexpr uint64_t ARM_WINDOW_MS = 250;

static std::atomic<KinesisState> g_kinesisState{KinesisState::Idle};
static std::atomic<uint64_t> g_armTick{0};
static std::atomic<bool> g_gameplayActive{true};


static void EndKinesis(const char* reason) {
    if (g_kinesisState.exchange(KinesisState::Idle) != KinesisState::Idle) {
        _LOGD("Kinesis INACTIVE (%s)", reason);
    }
    SendLeftTrigger();
}

// XInput hooking

#include <windows.h>
#include <Xinput.h>
#include <atomic>

enum class PSButton : uint16_t
{
    // Face buttons
    Cross     = XINPUT_GAMEPAD_A,   // ✕
    Circle    = XINPUT_GAMEPAD_B,   // ○
    Square    = XINPUT_GAMEPAD_X,   // □
    Triangle  = XINPUT_GAMEPAD_Y,   // △

    // Shoulders
    L1 = XINPUT_GAMEPAD_LEFT_SHOULDER,
    R1 = XINPUT_GAMEPAD_RIGHT_SHOULDER,

    // Sticks
    L3 = XINPUT_GAMEPAD_LEFT_THUMB,
    R3 = XINPUT_GAMEPAD_RIGHT_THUMB,

    // System buttons
    Options = XINPUT_GAMEPAD_START,
    Share   = XINPUT_GAMEPAD_BACK,

    // D-Pad
    DPadUp    = XINPUT_GAMEPAD_DPAD_UP,
    DPadDown  = XINPUT_GAMEPAD_DPAD_DOWN,
    DPadLeft  = XINPUT_GAMEPAD_DPAD_LEFT,
    DPadRight = XINPUT_GAMEPAD_DPAD_RIGHT,
};

inline bool IsPressed(const XINPUT_STATE& s, PSButton b)
{
    return (s.Gamepad.wButtons & static_cast<uint16_t>(b)) != 0;
}

static inline bool ButtonDownEdge(uint16_t prev, uint16_t now, PSButton b) {
    uint16_t mask = (uint16_t)static_cast<uint16_t>(b);
    return ((now & mask) != 0) && ((prev & mask) == 0);
}

using XInputGetState_t = DWORD (WINAPI*)(DWORD, XINPUT_STATE*);
static XInputGetState_t XInputGetState_Original = nullptr;

// Per-controller last trigger (L2) state
static std::atomic<uint8_t> g_lastL2[4] = {0,0,0,0};
static std::atomic<bool>    g_L2Down[4] = {false,false,false,false};
static std::atomic<uint16_t> g_prevButtons[4] = {0,0,0,0};
bool g_L2_isDown = false;

static constexpr uint8_t L2_DOWN_THRESH = 30;   // press threshold
static constexpr uint8_t L2_UP_THRESH   = 20;   // release threshold (hysteresis)

static std::atomic<uint64_t> g_R2FullPressStart[4] = {0, 0, 0, 0};
static std::atomic<bool>     g_R2HoldFired[4]      = {false, false, false, false};

static constexpr uint8_t  R2_FULL_THRESH   = 250;
static constexpr uint64_t R2_HOLD_TIME_MS  = 500;

#define TRIGGER_FULLY_PRESSED_THRESHOLD 250

bool IsR2FullyPressed(const XINPUT_GAMEPAD& pad)
{
    return pad.bRightTrigger >= TRIGGER_FULLY_PRESSED_THRESHOLD;
}

bool L2HeldAndR2FullyPressedForOneSec(DWORD userIndex, const XINPUT_GAMEPAD& pad)
{
    const bool l2Down = g_L2Down[userIndex].load(std::memory_order_relaxed);
    const bool r2Full = pad.bRightTrigger >= R2_FULL_THRESH;
    const uint64_t now = GetTickCount64();

    // The combo is only valid while L2 is held and R2 is fully pressed.
    if (l2Down && r2Full)
    {
        uint64_t start = g_R2FullPressStart[userIndex].load(std::memory_order_relaxed);

        if (start == 0)
        {
            g_R2FullPressStart[userIndex].store(now, std::memory_order_relaxed);
            g_R2HoldFired[userIndex].store(false, std::memory_order_relaxed);
            return false;
        }

        const bool alreadyFired =
            g_R2HoldFired[userIndex].load(std::memory_order_relaxed);

        if (!alreadyFired && (now - start >= R2_HOLD_TIME_MS))
        {
            g_R2HoldFired[userIndex].store(true, std::memory_order_relaxed);
            return true; // fire once
        }

        return false;
    }

    // Reset as soon as either L2 is released or R2 is no longer fully pressed.
    g_R2FullPressStart[userIndex].store(0, std::memory_order_relaxed);
    g_R2HoldFired[userIndex].store(false, std::memory_order_relaxed);
    return false;
}

static std::atomic<bool> g_R2WasFullyPressed[4] = {false, false, false, false};

bool WasR2FullyPressedAndReleasedWhileL2Held(DWORD userIndex, const XINPUT_GAMEPAD& pad)
{
    const bool l2Down = g_L2Down[userIndex].load(std::memory_order_relaxed);
    const bool r2FullNow = pad.bRightTrigger >= R2_FULL_THRESH;
    const bool r2WasFull = g_R2WasFullyPressed[userIndex].load(std::memory_order_relaxed);

    // If L2 is no longer held, clear the latch.
    if (!l2Down) {
        g_R2WasFullyPressed[userIndex].store(false, std::memory_order_relaxed);
        return false;
    }

    // While L2 is held, remember that R2 reached full press.
    if (r2FullNow) {
        g_R2WasFullyPressed[userIndex].store(true, std::memory_order_relaxed);
        return false;
    }

    // Fire once when R2 is no longer fully pressed, but previously was.
    if (r2WasFull) {
        g_R2WasFullyPressed[userIndex].store(false, std::memory_order_relaxed);
        return true;
    }

    return false;
}


DWORD WINAPI XInputGetState_Hook(DWORD dwUserIndex, XINPUT_STATE* pState)
{
    DWORD ret = XInputGetState_Original(dwUserIndex, pState);

    if (!g_gameplayActive)
        return ret;

    if (ret == ERROR_SUCCESS && pState && dwUserIndex < 4)
    {

        uint16_t currButtons  = pState->Gamepad.wButtons;
        uint16_t prevButtons = g_prevButtons[dwUserIndex].load (
            std::memory_order_relaxed
        );

        bool circleDown = ButtonDownEdge (
            prevButtons, currButtons, PSButton::Circle
        );

        // update prev now since we alrready compted if
        // circle is pressed
        g_prevButtons[dwUserIndex].store (
            currButtons, std::memory_order_relaxed
        );

        if (circleDown) {
            _LOGD("[XInput] Circle DOWN (user=%u)", dwUserIndex);

            if (g_kinesisState.load() == KinesisState::Active) {
                EndKinesis("Circle");
            } else {
                // transition to armed only if L2 is currently held
                if (g_L2Down[dwUserIndex].load(std::memory_order_relaxed)) {
                    g_kinesisState.store(KinesisState::Armed);
                    g_armTick.store(GetTickCount64());
                    _LOGD("Kinesis ARMED (L2 held + Circle)");
                }
            }
        }

        // L2

        uint8_t l2 = pState->Gamepad.bLeftTrigger;

        // hysteresis to avoid flicker around threshold
        bool L2_wasDown = g_L2Down[dwUserIndex].load(std::memory_order_relaxed);
        g_L2_isDown  = L2_wasDown;

        if (!L2_wasDown && l2 >= L2_DOWN_THRESH) g_L2_isDown = true;
        else if (L2_wasDown && l2 <= L2_UP_THRESH) g_L2_isDown = false;

        if (g_L2_isDown != L2_wasDown)
        {
            g_L2Down[dwUserIndex].store(g_L2_isDown, std::memory_order_relaxed);

            if (g_L2_isDown && IsPressed(*pState, PSButton::Circle)) {
                if (g_kinesisState == KinesisState::Active) {
                    EndKinesis("Circle");
                } else {
                    g_kinesisState.store(KinesisState::Armed);
                    g_armTick.store(GetTickCount64());
                }
            }
            if (g_L2_isDown) {
                _LOGD("[XInput] L2 DOWN (user=%u l2=%u)", dwUserIndex, l2);
                SendRightTrigger();
            } else {
                _LOGD("[XInput] L2 UP   (user=%u l2=%u)", dwUserIndex, l2);
                EndKinesis("L2_UP");
                ResetRightTrigger();
                ForceSendLeftTrigger();
            }
        }


        // custom Ripper Rule computed on every poll, not only on L2 edge
        int weaponId = g_currWeaponId.load(std::memory_order_relaxed);
        if (weaponId == 5) {
            /*if (!g_HasAmmoInClip[ weaponId ]) {
                _LOGD("RipperWithSaw OFF (no ammo)!");
                ForceSendLeftTrigger();
            }*/
            if (g_L2Down[dwUserIndex].load(std::memory_order_relaxed)) {
                if (IsPressed(*pState, PSButton::R1)) {
                    _LOGD("RipperWithSaw OFF!");
                    ForceSendLeftTrigger();
                //} else if (L2HeldAndR2FullyPressedForOneSec(dwUserIndex, pState->Gamepad)) {
                }
                /*else if (IsR2FullyPressed(pState->Gamepad)) {
                    _LOGD("RipperWithSaw ON!");
                    SendLeftTrigger("RipperWithSaw");
                }*/
            }


            /*else {
                // optional explicit reset when L2 not held
                g_R2FullPressStart[dwUserIndex].store(0, std::memory_order_relaxed);
                g_R2HoldFired[dwUserIndex].store(false, std::memory_order_relaxed);
            }*/
        }
        else if (weaponId == 6) {
            //if (g_L2Down[dwUserIndex].load(std::memory_order_relaxed)) {
#if 0
                if (IsPressed(*pState, PSButton::R1)) {
                    _LOGD("ContactBeamActive OFF!");
                    ForceSendLeftTrigger();
                } else
#endif
                if (L2HeldAndR2FullyPressedForOneSec(dwUserIndex, pState->Gamepad)) {
                    _LOGD("ContactBeamActive ON!");
                    SendRightTrigger("ContactBeamActive");
                }
                else if (WasR2FullyPressedAndReleasedWhileL2Held(dwUserIndex, pState->Gamepad)) {
                    _LOGD("ContactBeamActive OFF!");
                    SendRightTrigger("Contact Beam");
                    // or ResetRightTrigger(); depending on the effect you want
                }
            //}
        }

        g_lastL2[dwUserIndex].store(l2, std::memory_order_relaxed);
    }

    return ret;
}

bool HookXInputGetState()
{
    HMODULE hX = GetModuleHandleA("xinput1_4.dll");
    if (!hX) hX = LoadLibraryA("xinput1_4.dll");
    if (!hX) return false;

    auto p = GetProcAddress(hX, "XInputGetState");
    if (!p) return false;

    if (MH_CreateHook(p, &XInputGetState_Hook, reinterpret_cast<void**>(&XInputGetState_Original)) != MH_OK)
        return false;

    return MH_EnableHook(p) == MH_OK;
}

bool launchServerElevated (
        const std::wstring& exePath = L"./mods/DualSensitive/dualsensitive-service.exe"
    ) {
    wchar_t fullExePath[MAX_PATH];
    if (!GetFullPathNameW(exePath.c_str(), MAX_PATH, fullExePath, nullptr)) {
        _LOG("Failed to resolve full path");
        return false;
    }

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas";
    sei.lpFile = fullExePath;
    sei.nShow = SW_HIDE;
    sei.fMask = SEE_MASK_NO_CONSOLE;

    if (!ShellExecuteExW(&sei)) {
        DWORD err = GetLastError();
        _LOG("ShellExecuteEx failed: %lu", err);
        return false;
    }

    return true;
}

bool scheduledTaskExists(std::string taskName) {
    std::string query = "schtasks /query /TN \"" + taskName + "\" >nul 2>&1";
    int result = WinExec(query.c_str(), SW_HIDE);
    _LOG("Task exists: %d", result);
    return (result > 31);
}

bool launchServerTask() {
    std::string command = "schtasks /run /TN \"DualSensitive Service\" /I";
    int result = WinExec(command.c_str(), SW_HIDE);
    return (result > 31);
}

// schtasks /Create /TN "DualSensitive Service" /TR "wscript.exe \"C:\Program Files (x86)\Steam\steamapps\common\Dead Space (2023)\mods\DualSensitive\launch-service.vbs\" \"C:\Program Files (x86)\Steam\steamapps\common\Dead Space (2023)\mods\DualSensitive\dualsensitive-service.exe\"" /SC ONCE /ST 00:00 /RL HIGHEST /F

bool launchServerTaskOrElevated() {
    std::string taskName = "DualSensitive Service";
    if (scheduledTaskExists(taskName.c_str())) {
        std::string command("schtasks /run /TN \"" + taskName + "\" /I ");
        _LOG("Running task, command: %s", command.c_str());
        int result = WinExec(command.c_str(), SW_HIDE);
        if (result > 31){
            _LOG("Service ran successfully");
            return true;
        }
        _LOG("Running task failed (code: %d). Falling back to elevation.", result);
    } else {
        _LOG("Scheduled task not found. Falling back to elevation.");
    }

    // Final fallback
    if (!launchServerElevated()) {
        _LOG("Fallback elevation also failed. Check permissions or try manually running dualsensitive-service.exe.");
        return false;
    }

    return true;
}


bool terminateServer(PROCESS_INFORMATION& procInfo) {
    if (procInfo.hProcess != nullptr) {
        BOOL result = TerminateProcess(procInfo.hProcess, 0); // 0 = exit code
        CloseHandle(procInfo.hProcess);
        procInfo.hProcess = nullptr;
        return result == TRUE;
    }
    return false;
}


void logLastError(const char* context) {
    DWORD errorCode = GetLastError();
    wchar_t* msgBuf = nullptr;

    FormatMessageW (
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        0,
        (LPWSTR)&msgBuf,
        0,
        nullptr
    );

    // Convert wchar_t* msgBuf to char*
    char narrowBuf[1024] = {};
    if (msgBuf) {
        WideCharToMultiByte (
            CP_UTF8, 0, msgBuf, -1, narrowBuf,
            sizeof(narrowBuf) - 1, nullptr, nullptr
        );
    }

    _LOG("[ERROR] %s failed (code %lu): %s", context, errorCode,
            msgBuf ? narrowBuf : "(Unknown error)");

    if (msgBuf) {
        LocalFree(msgBuf);
    }
}


// Game functions

using _WriteString = void(__fastcall*)(void* builder, const char *value, int64_t n);

using _WriteKey = void(__fastcall*)(void* builder, const char *key, int64_t n);

using _ApplyWeaponState = void(__fastcall*)(int64_t player, int64_t source,
        int64_t dest, int64_t secondarySource);

using  _InterruptGame = void(__fastcall*)(
    void*   param_1,
    int     param_2,
    int64_t param_3,
    void*   param_4
);

using _ResumeGame = void(__fastcall*)(void* param_1, int param_2, int64_t param_3, int64_t param_4);

using _EventDispatcher = bool(__fastcall*)(
    void* a1, void* a2, void* a3,
    uint64_t a4, uint64_t a5, uint32_t a6
);

using _UpdateAmmo = void(__fastcall*)(long long *param_1, int param_2);

using _WeaponReload = void(__fastcall*)(long long* param_1, long long param_2);

static inline int32_t ReadKinesisCounter(void* self) {
    return *(int32_t*)((uint8_t*)self + 0x4C); // <-- BYTE offset, 32-bit
}


// Game Addresses


RVA<_UpdateAmmo>
UpdateAmmo (
    "48 89 5c 24 10 48 89 6c 24 18 48 89 74 24 20 57 48 81 ec 80 00 00 00 48 8b 01"
);
_UpdateAmmo UpdateAmmo_Original = nullptr;

RVA<_WeaponReload>
WeaponReload (
    "48 89 5C 24 08 48 89 6C 24 18 48 89 74 24 20 57 48 83 EC 60 8B 42 08"
);
_WeaponReload WeaponReload_Original = nullptr;

RVA<_ApplyWeaponState>
ApplyWeaponState (
    "4c 8b dc 53 56 48 81 ec 48 05 00 00 48 8b 05 95 b5 36 04 48 33 c4 48 89 "
    "84 24 10 05 00 00 49 8b 70 18 48 8b da"
);
_ApplyWeaponState ApplyWeaponState_Original = nullptr;

RVA<_WriteString>
WriteString (
    "48 89 5c 24 08 48 89 74 24 10 57 48 83 ec 20 49 8b d8 48 8b f2 48 8b f9 "
    "49 83 f8 ff 75 0b 66 90 48 ff c3 80 3c 1a 00 75 f7 e8 a2 06 00 00 84 c0 "
    "74 72 48 8b 8f 28 02 00 00"
);
_WriteString WriteString_Original = nullptr;

RVA<_WriteKey>
WriteKey (
    "48 89 5c 24 08 48 89 74 24 10 57 48 83 ec 20 49 8b d8 48 8b f2 48 8b f9 "
    "49 83 f8 ff 75 0b 66 90 48 ff c3 80 3c 1a 00 75 f7 e8 52 2e 00 00 84 c0 "
    "74 5c 48 8b 8f 28 02 00 00 48 85 c9"
);
_WriteKey WriteKey_Original = nullptr;

RVA<_InterruptGame>
InterruptGame (
    "81 fa ff 00 00 00 0f 84 83 05 00 00 48 8b c4 4c 89 48 20"
);
_InterruptGame InterruptGame_Original = nullptr;


RVA<_ResumeGame>
ResumeGame (
    "89 54 24 10 55 56 41 56 41 57 48 83 ec 38 4c 8b 51 10 8b f2"
);
_ResumeGame ResumeGame_Original = nullptr;

RVA<_EventDispatcher>
EventDispatcher (
    "48 8b c4 48 89 58 10 48 89 70 18 48 89 78 20 55 41 54 41 55 41 56 41 57 "
    "48 8d 68 b1 48 81 ec f0 00 00 00 0f 57 c0 0f 29 70 c8 0f 11 44 24 40 45 "
    "33 ff"
);
_EventDispatcher EventDispatcher_Original = nullptr;


// Globals

// This is to make sure that the PID will be sent to the server after
// the server has started
std::mutex g_serverLaunchMutex;
bool g_serverStarted = false;

namespace DualsenseMod {

    // Read and populate offsets and addresses from game code
    bool PopulateOffsets() {

        _LOG("WriteString at %p",
            WriteString.GetUIntPtr()
        );

        _LOG("WriteKey at %p",
            WriteKey.GetUIntPtr()
        );

        _LOG("ApplyWeaponState at %p",
            ApplyWeaponState.GetUIntPtr()
        );

        _LOG("InterruptGame at %p",
            InterruptGame.GetUIntPtr()
        );

        _LOG("ResumeGame at %p",
            ResumeGame.GetUIntPtr()
        );

        _LOG("EventDispatcher at %p",
            EventDispatcher.GetUIntPtr()
        );

        _LOG("UpdateAmmo at %p",
            UpdateAmmo.GetUIntPtr()
        );

        _LOG("WeaponReload at %p",
            WeaponReload.GetUIntPtr()
        );

        if (
            !WriteString        ||
            !WriteKey           ||
            !ApplyWeaponState   ||
            !InterruptGame      ||
            !ResumeGame         ||
            !EventDispatcher    ||
            !UpdateAmmo         ||
            !WeaponReload
        ) return false;

        if (!g_deadspaceBaseAddr) {
            _LOGD("Dead Space (2023) base address is not set!");
            return false;
        }

        return true;
    }


    // hooked functions

    static std::atomic<const char*> g_lastKeyAtomic{nullptr};


    void WriteKey_Hook (void* builder, const char *key, int64_t n) {
        WriteKey_Original(builder, key, n);
        g_lastKeyAtomic.store(key, std::memory_order_relaxed);
    }

    void WriteString_Hook (void* builder, const char *value, int64_t n) {
        WriteString_Original(builder, value, n);

        auto lastKey = g_lastKeyAtomic.load(std::memory_order_relaxed);

        //!strcmp(lastKey,"ability_mode") || !strcmp(lastKey,"status") ||

        if (lastKey && (!strcmp(lastKey, "npc_id"))) {
            //_LOGD("[WS] key=%s value='%s'",
            //      lastKey, value ? value : "(null)");

            // Enter Kinesis state if kinesis in armed state and
            // target npc id is found
            if (g_kinesisState == KinesisState::Armed) {
                uint64_t now = GetTickCount64();
                if (now - g_armTick.load() <= ARM_WINDOW_MS) {
                    ResetAdaptiveTriggers();
                    g_kinesisState.store(KinesisState::Active);
                    _LOGD("Kinesis ACTIVE (target npc_id=%s)", value ? value : "(null)");
                }
            }
        }

    }

    void ApplyWeaponState_Hook(int64_t player, int64_t source,
                               int64_t dest, int64_t secondarySource)
    {

        uint8_t* out = dest ? *(uint8_t**)(dest + 0x18) : nullptr;
        ApplyWeaponState_Original(player, source, dest, secondarySource);

        int weaponId = *(int*)(out + 0x4E4);
        if (weaponId < 0 || weaponId > 7) return;

        int prev = g_currWeaponId.exchange(weaponId);
        if (prev != weaponId) {
            _LOGD("[ApplyWeaponState Hook] - switched to weapon: %s,  id: %d",
                g_Weapons[weaponId].c_str(),  weaponId
            );
            SendLeftTrigger();
            if (g_L2_isDown)
                SendRightTrigger();
        }
    }

    static std::atomic<int32_t> g_lastEvent{0};

    static const char* SafeNameFromEvt(int64_t evt) {
        __try {
            int64_t p = *(int64_t*)(evt + 0x30);
            if (!p) return nullptr;
            const char* s = *(const char**)(p + 0x10);
            if (!s) return nullptr;
            // sanity: avoid garbage pointers / non-terminated strings
            for (int i = 0; i < 128; i++) {
                char c = s[i];
                if (c == '\0') return s;
                if ((unsigned char)c < 0x09) return nullptr;
            }
            return nullptr;
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            return nullptr;
        }
    }

    using  _InterruptGame = void(__fastcall*)(
        void*   param_1,   // RCX
        int     param_2,   // EDX
        int64_t param_3,   // R8
        void*   param_4    // R9   (was longlong* in decompile; treat as opaque pointer)
    );

    void __fastcall InterruptGame_Hook(void *param_1, int param_2, int64_t param_3, void *param_4)
    {
        _LOGD("InterruptGame Hook - Game paused or entered in RIG inventory!");
        g_gameplayActive.store(false, std::memory_order_release);
        EndKinesis("gameplay paused");
        ResetAdaptiveTriggers();
        if (g_menuScreensUntilGameStart > 0)
            g_menuScreensUntilGameStart--;
        InterruptGame_Original(param_1, param_2, param_3, param_4);
    }

    void __fastcall ResumeGame_Hook (void* param_1, int param_2, int64_t param_3, int64_t param_4)
    {
        _LOGD("ResumeGame Hook - Game is on again!");
        g_gameplayActive.store(true, std::memory_order_release);

        if (g_menuScreensUntilGameStart <= 0)
            g_gameStarted = true;

        SendLeftTrigger();

        ResumeGame_Original(param_1, param_2, param_3, param_4);
    }

    static std::atomic<bool> g_kinesisActive{false};

    static inline bool IsCanonicalUserPtr(uint64_t p) {
        // Typical user-mode canonical range on Windows x64
        return (p >= 0x0000000000010000ULL) &&
            (p <= 0x00007FFFFFFFFFFFULL);
    }

    static const char* TryAsciiAt(uint64_t p) {
        if (!IsCanonicalUserPtr(p)) return nullptr;
        __try {
            auto s = (const char*)p;
            // require first char printable-ish
            unsigned char c0 = (unsigned char)s[0];
            if (c0 == 0) return nullptr;
            if (c0 < 0x20 || c0 > 0x7E) return nullptr;

            for (int i = 0; i < 128; i++) {
                unsigned char c = (unsigned char)s[i];
                if (c == 0) return s;
                if (c < 0x20 || c > 0x7E) return nullptr;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        return nullptr;
    }

    static const char* TryStringObject(uint64_t a) {
        // try direct
        if (auto s = TryAsciiAt(a)) return s;

        if (!IsCanonicalUserPtr(a)) return nullptr;

        __try {
            // try common layouts:
            // [0]=char*, [1]=char*, at +0x10, +0x18, etc
            uint64_t q0 = *(uint64_t*)(a + 0x00);
            uint64_t q8 = *(uint64_t*)(a + 0x08);
            uint64_t q10 = *(uint64_t*)(a + 0x10);
            uint64_t q18 = *(uint64_t*)(a + 0x18);

            if (auto s = TryAsciiAt(q0)) return s;
            if (auto s = TryAsciiAt(q8)) return s;
            if (auto s = TryAsciiAt(q10)) return s;
            if (auto s = TryAsciiAt(q18)) return s;

            // pointer-to-pointer
            if (IsCanonicalUserPtr(q0)) {
                uint64_t qq0 = *(uint64_t*)q0;
                if (auto s = TryAsciiAt(qq0)) return s;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}

        return nullptr;
    }

    bool EventDispatcher_Hook(
        void* a1, void* a2, void* a3,
        uint64_t a4, uint64_t a5, uint32_t a6
    ){
        const char* eventName = TryStringObject(a4);

        if (eventName && (
                !strcmp(eventName, "KinesisThrow") ||
                !strcmp(eventName, "ApplyDamageAction"))
        ) {
            if (g_kinesisState.load() != KinesisState::Idle) {
                _LOGD("Kinesis Throw...");
                EndKinesis("Throw");
            }
        }

        return EventDispatcher_Original(a1, a2, a3, a4, a5, a6);
    }

    void UpdateAmmo_Hook(long long* param_1, int param_2)
    {
        _LOGD("UpdateAmmo Hook!");

        // Let the game update its internal ammo state first.
        UpdateAmmo_Original(param_1, param_2);

        if (!g_gameplayActive)
            return;

        if (param_1 == nullptr) {
            _LOGD("UpdateAmmo Hook! invalid param_1");
            return;
        }

        _LOGD("Ammo object ptr: %p", param_1);
        _LOGD("usedInClip addr: %p", (void*)((uintptr_t)param_1 + 0x3c));

        // This field appears to be "ammo used in current clip", not ammo remaining.
        unsigned int usedInClip = *(unsigned int *)((char*)param_1 + 0x3c);

        // vfunc at +0xD0 appears to return clip size / max count for this weapon.
        using GetClipSizeFn = unsigned int(__fastcall*)(long long*);
        GetClipSizeFn GetClipSize = *(GetClipSizeFn*)(*(long long*)param_1 + 0xD0);

        unsigned int clipSize = 0;
        if (GetClipSize) {
            clipSize = GetClipSize(param_1);
        }

        // Clamp defensively in case the internal state is odd during transitions.
        if (usedInClip > clipSize) {
            usedInClip = clipSize;
        }

        unsigned int ammoLeftInClip = (clipSize >= usedInClip) ? (clipSize - usedInClip) : 0;

        // The game also appears to store an "empty clip" bool at +0x48.
        bool isClipEmptyFlag = *(bool *)((char*)param_1 + 0x48);

        // Reliable empty check.
        bool isClipEmpty = (clipSize > 0) ? (usedInClip >= clipSize) : isClipEmptyFlag;

        _LOGD("UpdateAmmo Hook! used=%u clipSize=%u ammoLeft=%u emptyFlag=%d empty=%d",
              usedInClip, clipSize, ammoLeftInClip, isClipEmptyFlag ? 1 : 0, isClipEmpty ? 1 : 0);

        int weaponId = g_currWeaponId.load(std::memory_order_relaxed);

        // if we had ammo before, just transition to ripper with saw
        // no matter what
        if (g_HasAmmoInClip [ weaponId ]) {
            if (weaponId == 5) { // Ripper
                if (g_L2_isDown) {
                    _LOGD("RipperWithSaw!");
                    // Send a Special Left Trigger
                    SendLeftTrigger("RipperWithSaw");
                }
            }
        }



        if (isClipEmpty) {
            if (g_HasAmmoInClip [ weaponId ]) {
                _LOGD("clip empty - disable triggers");
#if 0
                if (weaponId == 5) { // Ripper
                    if (g_L2_isDown) {
                        _LOGD("RipperWithSaw!");
                        // Send a Special Left Trigger
                        SendLeftTrigger("RipperWithSaw");
                    }
                }
#endif

                g_HasAmmoInClip [ weaponId ] = false;
                // when HasAmmoInClip is false the following will send a clanky
                // trigger setting, which will be active until the weapon gets
                // reloaded
                if (g_L2_isDown)
                    SendRightTrigger();
            }
        } else {
            // Optional: re-enable when there is ammo in the clip again.
            if (!g_HasAmmoInClip [ weaponId ]) {
                _LOGD("clip has ammo again - enable triggers");
                g_HasAmmoInClip [ weaponId ] = true;
                SendLeftTrigger();
            }
            // this is fiering when we're aiming with ripper
#if 0
            else {
                if (weaponId == 5) { // Ripper
                    if (g_L2_isDown) {
                        _LOGD("RipperWithSaw!");
                        // Send a Special Left Trigger
                        SendLeftTrigger("RipperWithSaw");
                    }
                }
            }
#endif

        }
    }

    static uint32_t s_lastReloadTick = 0;
    static uint32_t s_lastReloadUsedBefore = 0;

    void WeaponReload_Hook(long long* param_1, long long param_2)
    {
        if (!param_1 || !param_2) {
            WeaponReload_Original(param_1, param_2);
            return;
        }

        int eventType = *(int*)((char*)param_2 + 0x8);

        unsigned int beforeUsed = *(unsigned int*)((char*)param_1 + 0x3c);

        WeaponReload_Original(param_1, param_2);

        unsigned int afterUsed = *(unsigned int*)((char*)param_1 + 0x3c);
        bool emptyFlag = *(bool*)((char*)param_1 + 0x48);


        uint32_t now = GetTickCount();

        if (eventType == 0x13 && afterUsed < beforeUsed) {
            if (!(now - s_lastReloadTick < 100 && s_lastReloadUsedBefore == beforeUsed)) {
               s_lastReloadTick = now;
               s_lastReloadUsedBefore = beforeUsed;
               _LOGD("Reload detected");
                int weaponId = g_currWeaponId.load(std::memory_order_relaxed);
                g_HasAmmoInClip [ weaponId ] = true;
                SendLeftTrigger();
                if (g_L2_isDown)
                    SendRightTrigger();
            }
        }
    }

    bool ApplyHooks() {
        _LOG("Applying hooks...");
        // Hook loadout type registration to obtain pointer to the model handle
        MH_Initialize();

        MH_CreateHook (
            WriteKey,
            WriteKey_Hook,
            reinterpret_cast<LPVOID *>(&WriteKey_Original)
        );
        if (MH_EnableHook(WriteKey) != MH_OK) {
            _LOG("FATAL: Failed to install WriteKey hook.");
            return false;
        }

        MH_CreateHook (
            WriteString,
            WriteString_Hook,
            reinterpret_cast<LPVOID *>(&WriteString_Original)
        );
        if (MH_EnableHook(WriteString) != MH_OK) {
            _LOG("FATAL: Failed to install WriteString hook.");
            return false;
        }

        MH_CreateHook (
            ApplyWeaponState,
            ApplyWeaponState_Hook,
            reinterpret_cast<LPVOID *>(&ApplyWeaponState_Original)
        );
        if (MH_EnableHook(ApplyWeaponState) != MH_OK) {
            _LOG("FATAL: Failed to install ApplyWeaponState hook.");
            return false;
        }

        MH_CreateHook (
            InterruptGame,
            InterruptGame_Hook,
            reinterpret_cast<LPVOID *>(&InterruptGame_Original)
        );
        if (MH_EnableHook(InterruptGame) != MH_OK) {
            _LOG("FATAL: Failed to install InterruptGame hook.");
            return false;
        }


        MH_CreateHook (
            ResumeGame,
            ResumeGame_Hook,
            reinterpret_cast<LPVOID *>(&ResumeGame_Original)
        );
        if (MH_EnableHook(ResumeGame) != MH_OK) {
            _LOG("FATAL: Failed to install ResumeGame hook.");
            return false;
        }

        MH_CreateHook (
            EventDispatcher,
            EventDispatcher_Hook,
            reinterpret_cast<LPVOID *>(&EventDispatcher_Original)
        );
        if (MH_EnableHook(EventDispatcher) != MH_OK) {
            _LOG("FATAL: Failed to install EventDispatcher hook.");
            return false;
        }

        MH_CreateHook (
            UpdateAmmo,
            UpdateAmmo_Hook,
            reinterpret_cast<LPVOID *>(&UpdateAmmo_Original)
        );
        if (MH_EnableHook(UpdateAmmo) != MH_OK) {
            _LOG("FATAL: Failed to install UpdateAmmo hook.");
            return false;
        }

        MH_CreateHook (
            WeaponReload,
            WeaponReload_Hook,
            reinterpret_cast<LPVOID *>(&WeaponReload_Original)
        );
        if (MH_EnableHook(WeaponReload) != MH_OK) {
            _LOG("FATAL: Failed to install WeaponReload hook.");
            return false;
        }

        if (!HookXInputGetState()) {
            _LOG("WARNING: Failed to hook XInputGetState (continuing).");
            return false;
        }

        _LOG("Hooks applied successfully!");

        return true;
    }

    bool InitAddresses() {
        _LOG("Sigscan start");
        RVAUtils::Timer tmr; tmr.start();
        RVAManager::UpdateAddresses(0);
        _LOG("Sigscan elapsed: %llu ms.", tmr.stop());

        // Check if all addresses were resolved
        for (auto rvaData : RVAManager::GetAllRVAs()) {
            if (!rvaData->effectiveAddress) {
                _LOG("Signature: %s was not resolved!", rvaData->sig);
            }
        }
        if (!RVAManager::IsAllResolved())
            return false;

        return true;
    }


#define DEVICE_ENUM_INFO_SZ 16
#define CONTROLLER_LIMIT 16
std::string wstring_to_utf8(const std::wstring& ws) {
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1,
                                   nullptr, 0, nullptr, nullptr);
    std::string s(len, 0);
    WideCharToMultiByte (
            CP_UTF8, 0, ws.c_str(), -1, &s[0], len, nullptr, nullptr
    );
    s.resize(len - 1);
    return s;
}

    constexpr int MAX_ATTEMPTS = 5;
    constexpr int RETRY_DELAY_MS = 2000;

    void Init() {
        g_logger.Open("./mods/dualsensemod.log");
        _LOG(
            "Dead Space (2023) DualsenseMod v1.0 by Thanos Petsas (SkyExplosionist)");
        g_deadspaceBaseAddr = GetModuleHandle(NULL);
        g_base = reinterpret_cast<uintptr_t>(g_deadspaceBaseAddr);
        _LOG("Module base: %p (g_base=%p)", g_deadspaceBaseAddr, (void*)g_base);
        _LOG("Module base: %p", g_deadspaceBaseAddr);

        // Sigscan
        if (!InitAddresses() || !PopulateOffsets()) {
            MessageBoxA (
                NULL,
                "DualsenseMod is not compatible with this version of Dead Space"
                " (2023).\nPlease visit the mod page for updates.",
                "DualsenseMod",
                MB_OK | MB_ICONEXCLAMATION
            );
            _LOG("FATAL: Incompatible version");
            return;
        }

        _LOG("Addresses set");

        // init config
        g_config = Config(INI_LOCATION);
        g_config.print();

        InitTriggerSettings();
        if (!ApplyHooks()) {
            MessageBoxA (
                NULL,
                "DualsenseMod is not compatible with this version of Dead Space"
                " (2023).\nPlease visit the mod page for updates.\n\n"
                "Error message: Could not apply hooks!",
                "DualsenseMod",
                MB_OK | MB_ICONEXCLAMATION
            );
            _LOG("FATAL: Incompatible version");
            return;
        }

        CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
            _LOG("Client starting DualSensitive Service...\n");
            if (!launchServerTaskOrElevated()) {
                _LOG("Error launching the DualSensitive Service...\n");
                return 1;
            }
            g_serverLaunchMutex.lock();
            g_serverStarted = true;
            g_serverLaunchMutex.unlock();
            _LOG("DualSensitive Service launched successfully...\n");
            return 0;
        }, nullptr, 0, nullptr);


        CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
            // wait for server to start first
            do {
                g_serverLaunchMutex.lock();
                bool started = g_serverStarted;
                g_serverLaunchMutex.unlock();
                if (started) break;
                std::this_thread::sleep_for(std::chrono::seconds(2));
            } while (true);

            _LOG("Client starting DualSensitive Service...\n");
            auto status = dualsensitive::init (
                    AgentMode::CLIENT,
                    "./mods/duaslensitive-client.log",
                    g_config.isDebugMode
            );
            if (status != dualsensitive::Status::Ok) {
                _LOG(
                    "Failed to initialize DualSensitive in CLIENT mode, "
                    "status: %d",
                    static_cast<
                        std::underlying_type<
                            dualsensitive::Status>::type>(status)
                );
            }
            _LOG("DualSensitive Service launched successfully...\n");
            dualsensitive::sendPidToServer();
            return 0;
        }, nullptr, 0, nullptr);

        _LOG("Ready.");
    }
}
