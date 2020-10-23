#define _CRT_SECURE_NO_WARNINGS
#define STEAM_API_EXPORTS

#include "FakeSteamAPI_ContextProvider.h"
#include "FakeSteamAPI_Utilities.h"
#include "FakeSteamAPI_Settings.h"

#include "steam_api.h"

//Make FakeSteamAPI.dll bigger, just like the normal steam_api.dll does
#pragma optimize("", off)
char placeholder[1024 * 24] = { 'I', 'n', 'f', 'o', '\0' };

#pragma optimize("", on)

//steam_api.h
S_API bool S_CALLTYPE SteamAPI_Init() {
	FakeSteamAPI_Settings_Init();

	//FakeSteamAPI_AppendLog(LogLevel_Info, "FakeSteamAPI has been started.");

	return true;
}

S_API bool S_CALLTYPE SteamAPI_InitSafe() {
	FakeSteamAPI_Settings_Init();

	//FakeSteamAPI_AppendLog(LogLevel_Info, "FakeSteamAPI has been started.");

	return true;
}


S_API void S_CALLTYPE SteamAPI_Shutdown() {

}

S_API bool S_CALLTYPE SteamAPI_RestartAppIfNecessary(uint32 unOwnAppID) {
	//FakeSteamAPI_AppendLog(LogLevel_Info, "%s() will pass false to the application.", __FUNCTION__);
	return false;
}

S_API void S_CALLTYPE SteamAPI_ReleaseCurrentThreadMemory() {

}

S_API void S_CALLTYPE SteamAPI_WriteMiniDump(uint32 uStructuredExceptionCode, void* pvExceptionInfo, uint32 uBuildID) {

}

S_API void S_CALLTYPE SteamAPI_SetMiniDumpComment(const char *pchMsg) {

}

S_API void S_CALLTYPE SteamAPI_RunCallbacks() {
	static bool bFirst = true;

	if (bFirst) {
		bFirst = false;
		//FakeSteamAPI_AppendLog(LogLevel_Info, "Calls to %s() will only be shown once.", __FUNCTION__);
	}

	if (FakeSteamAPI_GetSettingsItemInt32(FakeSteamAPI_SettingsIndex_ProcessMessageInRunCallbacks) != 0) {

	}
}

S_API void S_CALLTYPE SteamAPI_RegisterCallback(class CCallbackBase *pCallback, int iCallback) {

}

S_API void S_CALLTYPE SteamAPI_UnregisterCallback(class CCallbackBase *pCallback) {

}

S_API void S_CALLTYPE SteamAPI_RegisterCallResult(class CCallbackBase *pCallback, SteamAPICall_t hAPICall) {

}

S_API void S_CALLTYPE SteamAPI_UnregisterCallResult(class CCallbackBase *pCallback, SteamAPICall_t hAPICall) {

}

S_API bool S_CALLTYPE SteamAPI_IsSteamRunning() {
	//FakeSteamAPI_AppendLog(LogLevel_Info, "%s() will pass false to the application.", __FUNCTION__);
	return false;
}

S_API void Steam_RunCallbacks(HSteamPipe hSteamPipe, bool bGameServerCallbacks) {

}

S_API void Steam_RegisterInterfaceFuncs(void *hModule) {

}

S_API HSteamUser Steam_GetHSteamUserCurrent() {
	return NULL;
}

S_API const char *SteamAPI_GetSteamInstallPath() {
	return NULL;
}

S_API HSteamPipe SteamAPI_GetHSteamPipe() {
	return NULL;
}

S_API void SteamAPI_SetTryCatchCallbacks(bool bTryCatchCallbacks) {
	return;
}

S_API HSteamPipe GetHSteamPipe() {
	return NULL;
}

S_API HSteamUser GetHSteamUser() {
	return NULL;
}

//steam_api_internal.h
S_API HSteamUser SteamAPI_GetHSteamUser() {
	//FakeSteamAPI_AppendLog(LogLevel_Info, "%s() will pass NULL to the application.", __FUNCTION__);
	return NULL;
}

S_API void * S_CALLTYPE SteamInternal_ContextInit(void *pContextInitData) {
	static bool bFirst = true;
	if (bFirst) {
		bFirst = false;
		//FakeSteamAPI_AppendLog(LogLevel_Info, "Calls to %s() will only be shown once.", __FUNCTION__);
	}
	//FakeSteamAPI_AppendLog(LogLevel_Info, "%s() will pass NULL to the application.", __FUNCTION__);
	return &FakeSteamAPI_GetContextInstance();
}

S_API void * S_CALLTYPE SteamInternal_CreateInterface(const char *ver) {
	//FakeSteamAPI_AppendLog(LogLevel_Info, "%s() will pass NULL to the application.", __FUNCTION__);
	return NULL;
}
