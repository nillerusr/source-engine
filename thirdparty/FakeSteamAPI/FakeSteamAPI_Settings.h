#pragma once

#include <cstdint>

#define FakeSteamAPI_SettingsIndex_ProcessMessageInRunCallbacks 1
#define FakeSteamAPI_SettingsIndex_UseAbsoluteAddress 2

void FakeSteamAPI_Settings_Init(void);
int32_t FakeSteamAPI_GetSettingsItemInt32(int nIndex);
void FakeSteamAPI_SetSettingsItemInt32(int nIndex, int n);