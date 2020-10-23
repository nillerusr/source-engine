#include "FakeSteamAPI_Settings.h"

#define GetArrLen(arr) (sizeof(arr) / sizeof(*(arr)))

typedef enum tagFakeSteamAPI_SettingsVariantType {
	FakeSteamAPI_SettingsVariant_Null = 0,
	FakeSteamAPI_SettingsVariant_Int32 = 1
} FakeSteamAPI_SettingsVariantType;

typedef struct tagFakeSteamAPI_SettingsVariant {
	FakeSteamAPI_SettingsVariantType type;
	union {
		int32_t n;
	};
} FakeSteamAPI_SettingsVariant;

FakeSteamAPI_SettingsVariant settingsItemList[10];

void FakeSteamAPI_Settings_Init(void) {
	settingsItemList[0] = { FakeSteamAPI_SettingsVariant_Null };
	settingsItemList[FakeSteamAPI_SettingsIndex_ProcessMessageInRunCallbacks] = { FakeSteamAPI_SettingsVariant_Int32 };
	settingsItemList[FakeSteamAPI_SettingsIndex_UseAbsoluteAddress] = { FakeSteamAPI_SettingsVariant_Int32 };
}

int32_t FakeSteamAPI_GetSettingsItemInt32(int nIndex) /*noexcept*/ {
	if (nIndex <= 0 || nIndex >= GetArrLen(settingsItemList))
		return -1;
	switch (settingsItemList[nIndex].type) {
	case FakeSteamAPI_SettingsVariant_Int32:
		return settingsItemList[nIndex].n;
	default:
		return -1;
	}
}
void FakeSteamAPI_SetSettingsItemInt32(int nIndex, int n) /*noexcept*/ {
	if (nIndex <= 0 || nIndex >= GetArrLen(settingsItemList))
		return;
	switch (settingsItemList[nIndex].type) {
	case FakeSteamAPI_SettingsVariant_Int32:
		settingsItemList[nIndex].n = n;
	default:
		return;
	}
}