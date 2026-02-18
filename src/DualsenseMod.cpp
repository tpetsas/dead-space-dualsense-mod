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

// TODO: move the following to a server utils file

PROCESS_INFORMATION serverProcInfo;


#include <shellapi.h>

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

void InitTriggerSettings() {
    g_TriggerSettings =
    {
        {
            "weapon/zion/player/sp/fists_berserk",
            {

                .L2 = new TriggerSetting (
                        TriggerProfile::Choppy, {}
                ),
                .R2 = new TriggerSetting (
                        TriggerProfile::Choppy, {}
                )
            }
        },
        {
            "weapon/zion/player/sp/fists",
            {
                .L2 = new TriggerSetting (
                        TriggerProfile::Choppy, {}
                ),
                .R2 = new TriggerSetting (
                        TriggerProfile::Choppy, {}
                )
            }
        },
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
        },
        {
            "weapon/zion/player/sp/heavy_rifle_heavy_ar",
            {
                .L2 = new TriggerSetting (
                        TriggerMode::Rigid, {}
                ),
                .R2 = new TriggerSetting (
                        TriggerProfile::MultiplePositionVibration,
                        {14, 0, 1, 2, 3, 4, 5, 6, 7, 8, 8}
                )
            }
        },
        {
            "weapon/zion/player/sp/heavy_rifle_heavy_ar_mod",
            {
                .L2 = new TriggerSetting (
                        TriggerMode::Rigid, {}
                ),
                .R2 = new TriggerSetting (
                        TriggerProfile::MultiplePositionVibration,
                        {15, 0, 1, 4, 6, 7, 8, 8, 7, 6, 4}
                )
            }
        },
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
            "weapon/zion/player/sp/double_barrel",
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
            "weapon/zion/player/sp/chainsaw",
            {
                .L2 = new TriggerSetting (
                        TriggerProfile::Machine,
                        {1, 9, 1, 5, 100, 0}
                ),
                .R2 = new TriggerSetting (
                        TriggerProfile::Machine,
                        {1, 9, 7, 7, 65, 0}
                )
            }
        },
        {
            "weapon/zion/player/sp/gauss_rifle",
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
        {
            "weapon/zion/player/sp/bfg",
            {
                .L2 = new TriggerSetting (
                    TriggerProfile::Choppy, {}
                ),
                .R2 = new TriggerSetting (
                    TriggerMode::Pulse_AB,
                    {18, 197, 35, 58, 90, 120, 138}
                )
            }
        },
    };
}

void SendTriggers(std::string weaponName) {
    std::string weaponId = std::string(weaponName);
    Triggers t = g_TriggerSettings[weaponId];
    if (t.L2->isCustomTrigger)
        dualsensitive::setLeftCustomTrigger(t.L2->mode, t.L2->extras);
    else
        dualsensitive::setLeftTrigger (t.L2->profile, t.L2->extras);
    if (t.R2->isCustomTrigger)
        dualsensitive::setRightCustomTrigger(t.R2->mode, t.R2->extras);
    else
        dualsensitive::setRightTrigger (t.R2->profile, t.R2->extras);
    _LOGD("Adaptive Trigger settings sent successfully!");
}


// Game global vars

HMODULE g_deadspaceBaseAddr = nullptr;

std::vector<std::string> g_AmmoList = {
    "bullets",  // heavy_rifle_heavy_ar, chaingun
    "shells",   // shotgun, double_barrel
    "rockets",  // rocket_launcher
    "plasma",   // plasma_rifle, gauss_rifle
    "cells",    // bfg
    "fuel"      // chainsaw
};

// weapon name to ammo
static std::unordered_map<std::string, std::string> g_WeaponToAmmoType = {
  {"weapon/zion/player/sp/fists",                    "infinite"},
  {"weapon/zion/player/sp/fists_berserk",            "infinite"},
  {"weapon/zion/player/sp/pistol",                   "infinite"},
  {"weapon/zion/player/sp/heavy_rifle_heavy_ar",     "bullets"},
  {"weapon/zion/player/sp/heavy_rifle_heavy_ar_mod", "bullets"},
  {"weapon/zion/player/sp/chaingun",                 "bullets"},
  {"weapon/zion/player/sp/chaingun_mod",             "bullets"},
  {"weapon/zion/player/sp/shotgun",                  "shells"},
  {"weapon/zion/player/sp/shotgun_mod",              "shells"},
  {"weapon/zion/player/sp/double_barrel",            "shells"},
  {"weapon/zion/player/sp/rocket_launcher",          "rockets"},
  {"weapon/zion/player/sp/rocket_launcher_mod",      "rockets"},
  {"weapon/zion/player/sp/plasma_rifle",             "plasma"},
  {"weapon/zion/player/sp/gauss_rifle",              "plasma"},
  {"weapon/zion/player/sp/gauss_rifle_mod",          "plasma"},
  {"weapon/zion/player/sp/bfg",                      "cells"},
  {"weapon/zion/player/sp/chainsaw",                 "fuel"},
};



// Game functions
/*
using _DispatchEvent =
    void(__fastcall*)(
            void* mgr, uint32_t eventId, uint32_t param2, uint8_t param3);
*/

using _WriteString = void(__fastcall*)(void* builder, const char *value, int64_t n);

using _WriteKey = void(__fastcall*)(void* builder, const char *key, int64_t n);

using _WeaponChange = void(__fastcall*)(int64_t p1, int p2);


using _ApplyWeaponState = void(__fastcall*)(int64_t player, int64_t source,
        int64_t dest, int64_t secondarySource);


// Game Addresses

/*
RVA<_DispatchEvent>
DispatchEvent (
"8b 11 45 0f b6 c8 48 8b 0d 53 e1 03 03 0f 28 d1 48 8b 01 48 ff 60 30"
);
_DispatchEvent DispatchEvent_Original = nullptr;
*/


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
"48 89 5c 24 08 48 89 74 24 10 57 48 83 ec 20 49 8b d8 48 8b f2 48 8b f9 49 83 "
"f8 ff 75 0b 66 90 48 ff c3 80 3c 1a 00 75 f7 e8 52 2e 00 00 84 c0 74 5c 48 8b "
"8f 28 02 00 00 48 85 c9"
);
_WriteKey WriteKey_Original = nullptr;


RVA<_WeaponChange>
WeaponChange (
        "48 89 5c 24 18 57 48 83 ec 20 44 0f b6 41 10 8b fa 48 8b 41 18 48 8b "
        "d9 49 c1 e0 05 45 8b 4c 00 18 41 c1 e9 0e 41 f6 c1 01 0f 84 9e 00 00 "
        "00 48 83 b9 78 12 00 00 00 0f 84 90 00 00 00 48 8b 81 20 11 00 00 48 "
        "8d 54 24 30"
);
_WeaponChange WeaponChange_Original = nullptr;

// weapon switch dead space

//void FUN_140848020(longlong param_1)

//"48 89 5c 24 08 48 89 74 24 10 57 48 83 ec 20 48 8b b1 e8 01 00 00 48 8b de"


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

        _LOG("WeaponChange at %p",
            WeaponChange.GetUIntPtr()
        );


        _LOG("ApplyWeaponState at %p",
            ApplyWeaponState.GetUIntPtr()
        );

        if (!WeaponChange ||
            !WriteString  ||
            !WriteKey     ||
            !ApplyWeaponState)
            return false;

        if (!g_deadspaceBaseAddr) {
            _LOGD("Dead Space (2023) base address is not set!");
            return false;
        }

        return true;
    }

    void resetAdaptiveTriggers() {
        dualsensitive::setLeftTrigger(TriggerProfile::Normal);
        dualsensitive::setRightTrigger(TriggerProfile::Normal);
        _LOGD("Adaptive Triggers reset successfully!");
    }


    void noAmmoAdaptiveTriggers() {
        dualsensitive::setLeftTrigger(TriggerProfile::Normal);
        dualsensitive::setRightTrigger(TriggerProfile::GameCube);
        _LOGD("No Ammo Adaptive Triggers set successfully!");
    }

    void sendAdaptiveTriggersForCurrentWeapon();
    void sendAdaptiveTriggersForCurrentWeaponc(char *currWeaponName) {
        _LOGD("* curr weapon: %s!", currWeaponName);
        if (currWeaponName){
            _LOGD("* Sending adaptive trigger setting!");
            SendTriggers(currWeaponName);
            return;
        }
    }
/*
    uint64_t* DispatchEvent_Hook (uint64_t* node, const char* eventName,
            uint16_t flags, int16_t priority) {
        _LOGD("* DispatchEvent hook!!!");

        uint64_t *ret = DispatchEvent_Original (
                node, eventName, flags, priority
        );

        if (eventName == nullptr) {
            return ret;
        }

        _LOGD("Dead Space - DispatchEvent - event name: %s\n", eventName);
        //sendAdaptiveTriggersForCurrentWeapon();
        //DispatchEvent_Original(player, weapon);

        return ret;
    }
*/

#include <intrin.h>

// helper functions
static void* GetCaller() {
    return _ReturnAddress();
}

static int ReadCurrentWeaponId(int64_t p1) {
    if (!p1) return -1;
    auto state = *(int64_t*)(p1 + 0x1278);
    if (!state) return -1;
    return *(int*)(state + 0x20);
}

static thread_local const char* g_lastKey = nullptr;

void WriteKey_Hook (void* builder, const char *key, int64_t n) {


    if (key && (strcmp(key, "current_weapon") == 0)) {
        g_lastKey = key;
    } else {
        g_lastKey = nullptr;
    }

    WriteKey_Original(builder, key, n);
}

void WriteString_Hook (void* builder, const char *value, int64_t n) {

    if (g_lastKey && (strcmp(g_lastKey, "current_weapon") == 0) && value) {
        _LOGD("WriteString hook - Current Weapon: %s", value);
        //1int prev = g_lastWeaponId.exchange(value);
        /*
        if (prev != value) {
            // send adaptive triggers
        }
        */
    }

    WriteString_Original(builder, value, n);
}

/*
static void Dump32(const char* tag, uint8_t* p, int off)
{
    if (!p) return;
    uint32_t v0 = *(uint32_t*)(p + off);
    uint32_t v1 = *(uint32_t*)(p + off + 4);
    uint32_t v2 = *(uint32_t*)(p + off + 8);
    uint32_t v3 = *(uint32_t*)(p + off + 12);
    _LOGD("%s +%X: %08X %08X %08X %08X", tag, off, v0, v1, v2, v3);
}

void ApplyWeaponState_Hook(int64_t player, int64_t source,
                           int64_t dest, int64_t secondarySource)
{
    uint8_t* out = dest ? *(uint8_t**)(dest + 0x18) : nullptr;

    ApplyWeaponState_Original(player, source, dest, secondarySource);

    if (!out) return;

    static uint64_t lastTick = 0;
    uint64_t now = GetTickCount64();
    if (now - lastTick > 1000) {
        lastTick = now;
        Dump32("[AWS]", out, 0x4C0);
        Dump32("[AWS]", out, 0x4D0);
        Dump32("[AWS]", out, 0x4E0);
        Dump32("[AWS]", out, 0x4E4); // yes, small overlap is fine
    }
}
*/


static void DumpU32(uint8_t* p, int off) {
    uint32_t v = *(uint32_t*)(p + off);
    _LOGD("[AWS] +%03X = %08X (%d)", off, v, (int32_t)v);
}

static std::atomic<int>  g_lastWeaponId{-1};

void ApplyWeaponState_Hook(int64_t player, int64_t source,
                           int64_t dest, int64_t secondarySource)
{

    uint8_t* out = dest ? *(uint8_t**)(dest + 0x18) : nullptr;

    ApplyWeaponState_Original(player, source, dest, secondarySource);

    if (!out) return;

    // only dump once every ~250ms so it’s responsive but not insane!
    static uint64_t lastTick = 0;
    uint64_t now = GetTickCount64();
    if (now - lastTick < 250) return;
    lastTick = now;

    // weapon offest: 0x4E4
    int weaponId = *(int*)(out + 0x4E4);

    if (weaponId < 0 || weaponId > 100)
        return;

    int prev = g_lastWeaponId.exchange(weaponId);
    if (prev != weaponId) {
        _LOGD("[ApplyWeaponState Hook] current weapon id: %d", weaponId);
    }

#if 0
    // Scan around the area you think matters
    DumpU32(out_before, 0x4C0);
    DumpU32(out_before, 0x4CC);
    DumpU32(out_before, 0x4D0);
    DumpU32(out_before, 0x4D4);
    DumpU32(out_before, 0x4DC);
    DumpU32(out_before, 0x4E0);
    DumpU32(out_before, 0x4E4);
    DumpU32(out_before, 0x4E8);
#endif
}

void WeaponChange_Hook (int64_t p1, int p2) {
    WeaponChange_Original(p1, p2);

    int cur = ReadCurrentWeaponId(p1);
    if (cur < 0) return;

    // weapon changed: p2 is new weapon
    // set DualSense trigger profile here
    _LOGD("WeaponChange: p2: %d", p2);
    _LOGD("WeaponChange: cur: %d", cur);
}

/*
unsigned long long SelectWeaponByDeclExplicit_Hook(long long *player,
                            long long decl, char param_3, char param_4) {
    _LOGD("* idPlayer::SelectWeaponByDeclExplicit hook!!!");

    Weapon *weapon = nullptr;
    if (long long *mgr = (long long *) GetWeaponMgr(player)) {
        weapon = (Weapon *)GetWeaponFromDecl(mgr, decl);
        if (weapon) {
            const char* name = GetWeaponName (
                    reinterpret_cast<long long*>(weapon)
            );
            if (name && name[0]) {
                _LOGD("* (init) curr weapon: %s", name);
            }
            g_currWeapon = weapon;
        }
    }
    unsigned long long ret = SelectWeaponByDeclExplicit_Original (
            player, decl, param_3, param_4
    );

    // reset player here (paused used for loading the latest checkpoint
    // from the main menu)
    if (g_currPlayer && (g_state == GameState::Idle ||
                g_state == GameState::Paused)) {
        g_state.store(GameState::Idle, std::memory_order_release);
        g_currPlayer = nullptr;
    }
    return ret;
}
*/

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
            WeaponChange,
            WeaponChange_Hook,
            reinterpret_cast<LPVOID *>(&WeaponChange_Original)
        );
        if (MH_EnableHook(WeaponChange) != MH_OK) {
            _LOG("FATAL: Failed to install WeaponChange hook.");
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

        ApplyHooks();

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
