#define _CRT_SECURE_NO_WARNINGS

#include "FakeSteamAPI_ContextProvider.h"
#include "FakeSteamAPI_Utilities.h"

#define VFunction_STUB { }

#define DerivedInterfaceGenerator(name)			\
	class name ## Derived : public name {		\
	public:										\
		static name& GetInstance(void) {		\
			static name ## Derived instance;	\
			return instance;					\
		}										\
	}

#define DerivedInterfaceGenerator_Begin(name)	\
	class name ## Derived : public name {
#define DerivedInterfaceGenerator_End(name)		\
	public:										\
		static name& GetInstance(void) {		\
			static name ## Derived instance;	\
			return instance;					\
		}										\
	}

//Total 21 interfaces
struct CSteamAPIContextStruct {
	ISteamClient		*m_pSteamClient;
	ISteamUser			*m_pSteamUser;
	ISteamFriends		*m_pSteamFriends;
	ISteamUtils			*m_pSteamUtils;
	ISteamMatchmaking	*m_pSteamMatchmaking;
	ISteamUserStats		*m_pSteamUserStats;
	ISteamApps			*m_pSteamApps;
	ISteamMatchmakingServers *m_pSteamMatchmakingServers;
	ISteamNetworking	*m_pSteamNetworking;
	ISteamRemoteStorage *m_pSteamRemoteStorage;
	ISteamScreenshots	*m_pSteamScreenshots;
	ISteamHTTP			*m_pSteamHTTP;
	ISteamController	*m_pController;
	ISteamUGC			*m_pSteamUGC;
	ISteamAppList		*m_pSteamAppList;
	ISteamMusic			*m_pSteamMusic;
	ISteamMusicRemote	*m_pSteamMusicRemote;
	ISteamHTMLSurface	*m_pSteamHTMLSurface;
	ISteamInventory		*m_pSteamInventory;
	ISteamVideo			*m_pSteamVideo;
	ISteamParentalSettings *m_pSteamParentalSettings;
};

/*
class ISteamClientDerived : ISteamClient {
public:
	static ISteamClient& GetInstance (void) {
		return instance;
	}
protected:
	static ISteamClientDerived instance;
};
*/

/*
DerivedInterfaceGenerator(ISteamClient);
DerivedInterfaceGenerator(ISteamUser);
DerivedInterfaceGenerator(ISteamFriends);
DerivedInterfaceGenerator(ISteamUtils);
DerivedInterfaceGenerator(ISteamMatchmaking);
DerivedInterfaceGenerator(ISteamUserStats);
DerivedInterfaceGenerator(ISteamApps);
DerivedInterfaceGenerator(ISteamMatchmakingServers);
DerivedInterfaceGenerator(ISteamNetworking);
DerivedInterfaceGenerator(ISteamRemoteStorage);
DerivedInterfaceGenerator(ISteamScreenshots);
DerivedInterfaceGenerator(ISteamHTTP);
DerivedInterfaceGenerator(ISteamController);
DerivedInterfaceGenerator(ISteamUGC);
DerivedInterfaceGenerator(ISteamAppList);
DerivedInterfaceGenerator(ISteamMusic);
DerivedInterfaceGenerator(ISteamMusicRemote);
DerivedInterfaceGenerator(ISteamHTMLSurface);
DerivedInterfaceGenerator(ISteamInventory);
DerivedInterfaceGenerator(ISteamVideo);
DerivedInterfaceGenerator(ISteamParentalSettings);
*/

DerivedInterfaceGenerator_Begin(ISteamClient);
	virtual HSteamPipe CreateSteamPipe() {
		return 0;
	}
	virtual bool BReleaseSteamPipe(HSteamPipe hSteamPipe) {
		return 0;
	}
	virtual HSteamUser ConnectToGlobalUser(HSteamPipe hSteamPipe) {
		return 0;
	}
	virtual HSteamUser CreateLocalUser(HSteamPipe *phSteamPipe, EAccountType eAccountType) {
		return 0;
	}
	virtual void ReleaseUser(HSteamPipe hSteamPipe, HSteamUser hUser) VFunction_STUB
	virtual ISteamUser *GetISteamUser(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) {
		return 0;
	}
	virtual ISteamGameServer *GetISteamGameServer(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) {
		return 0;
	}
	virtual void SetLocalIPBinding(uint32 unIP, uint16 usPort) VFunction_STUB
	virtual ISteamFriends *GetISteamFriends(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) {
		return 0;
	}
	virtual ISteamUtils *GetISteamUtils(HSteamPipe hSteamPipe, const char *pchVersion) {
		return 0;
	}
	virtual ISteamMatchmaking *GetISteamMatchmaking(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) {
		return 0;
	}
	virtual ISteamMatchmakingServers *GetISteamMatchmakingServers(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) {
		return 0;
	}
	virtual void *GetISteamGenericInterface(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) {
		return 0;
	}
	virtual ISteamUserStats *GetISteamUserStats(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) {
		return 0;
	}
	virtual ISteamGameServerStats *GetISteamGameServerStats(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) {
		return 0;
	}
	virtual ISteamApps *GetISteamApps(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) {
		return 0;
	}
	virtual ISteamNetworking *GetISteamNetworking(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) {
		return 0;
	}
	virtual ISteamRemoteStorage *GetISteamRemoteStorage(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) {
		return 0;
	}
	virtual ISteamScreenshots *GetISteamScreenshots(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) {
		return 0;
	}
	STEAM_PRIVATE_API(virtual void RunFrame() VFunction_STUB )
	virtual uint32 GetIPCCallCount() {
		return 0;
	}
	virtual void SetWarningMessageHook(SteamAPIWarningMessageHook_t pFunction) VFunction_STUB
	virtual bool BShutdownIfAllPipesClosed() {
		return 0;
	}
	virtual ISteamHTTP *GetISteamHTTP(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) {
		return 0;
	}
	STEAM_PRIVATE_API(virtual void *DEPRECATED_GetISteamUnifiedMessages(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) {
		return 0;
	})
	virtual ISteamController *GetISteamController(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) {
		return 0;
	}
	virtual ISteamUGC *GetISteamUGC(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) {
		return 0;
	}
	virtual ISteamAppList *GetISteamAppList(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) {
		return 0;
	}
	virtual ISteamMusic *GetISteamMusic(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) {
		return 0;
	}
	virtual ISteamMusicRemote *GetISteamMusicRemote(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) {
		return 0;
	}
	virtual ISteamHTMLSurface *GetISteamHTMLSurface(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) {
		return 0;
	}
	STEAM_PRIVATE_API(virtual void DEPRECATED_Set_SteamAPI_CPostAPIResultInProcess(void(*)()) VFunction_STUB )
	STEAM_PRIVATE_API(virtual void DEPRECATED_Remove_SteamAPI_CPostAPIResultInProcess(void(*)()) VFunction_STUB )
	STEAM_PRIVATE_API(virtual void Set_SteamAPI_CCheckCallbackRegisteredInProcess(SteamAPI_CheckCallbackRegistered_t func) VFunction_STUB )
	virtual ISteamInventory *GetISteamInventory(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) {
		return 0;
	}
	virtual ISteamVideo *GetISteamVideo(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) {
		return 0;
	}
	virtual ISteamParentalSettings *GetISteamParentalSettings(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) {
		return 0;
	}
DerivedInterfaceGenerator_End(ISteamClient);

DerivedInterfaceGenerator_Begin(ISteamUser);
	virtual HSteamUser GetHSteamUser() {
		return 0;
	}
	virtual bool BLoggedOn() {
		return 0;
	}
	virtual CSteamID GetSteamID() {
		CSteamID fuck;
		return fuck;
	}
	virtual int InitiateGameConnection(void *pAuthBlob, int cbMaxAuthBlob, CSteamID steamIDGameServer, uint32 unIPServer, uint16 usPortServer, bool bSecure) {
		return 0;
	}
	virtual void TerminateGameConnection(uint32 unIPServer, uint16 usPortServer) VFunction_STUB
	virtual void TrackAppUsageEvent(CGameID gameID, int eAppUsageEvent, const char *pchExtraInfo = "") VFunction_STUB
	virtual bool GetUserDataFolder(char *pchBuffer, int cubBuffer) {
		return 0;
	}
	virtual void StartVoiceRecording() VFunction_STUB
	virtual void StopVoiceRecording() VFunction_STUB
	virtual EVoiceResult GetAvailableVoice(uint32 *pcbCompressed, uint32 *pcbUncompressed_Deprecated = 0, uint32 nUncompressedVoiceDesiredSampleRate_Deprecated = 0) {
		return (EVoiceResult)0;
	}
	virtual EVoiceResult GetVoice(bool bWantCompressed, void *pDestBuffer, uint32 cbDestBufferSize, uint32 *nBytesWritten, bool bWantUncompressed_Deprecated = false, void *pUncompressedDestBuffer_Deprecated = 0, uint32 cbUncompressedDestBufferSize_Deprecated = 0, uint32 *nUncompressBytesWritten_Deprecated = 0, uint32 nUncompressedVoiceDesiredSampleRate_Deprecated = 0) {
		return (EVoiceResult)0;
	}
	virtual EVoiceResult DecompressVoice(const void *pCompressed, uint32 cbCompressed, void *pDestBuffer, uint32 cbDestBufferSize, uint32 *nBytesWritten, uint32 nDesiredSampleRate) {
		return (EVoiceResult)0;
	}
	virtual uint32 GetVoiceOptimalSampleRate() {
		return 0;
	}
	virtual HAuthTicket GetAuthSessionTicket(void *pTicket, int cbMaxTicket, uint32 *pcbTicket) {
		return 0;
	}
	virtual EBeginAuthSessionResult BeginAuthSession(const void *pAuthTicket, int cbAuthTicket, CSteamID steamID) {
		return (EBeginAuthSessionResult)0;
	}
	virtual void EndAuthSession(CSteamID steamID) VFunction_STUB
	virtual void CancelAuthTicket(HAuthTicket hAuthTicket) VFunction_STUB
	virtual EUserHasLicenseForAppResult UserHasLicenseForApp(CSteamID steamID, AppId_t appID) {
		return (EUserHasLicenseForAppResult)0;
	}
	virtual bool BIsBehindNAT() {
		return 0;
	}
	virtual void AdvertiseGame(CSteamID steamIDGameServer, uint32 unIPServer, uint16 usPortServer) VFunction_STUB
	virtual SteamAPICall_t RequestEncryptedAppTicket(void *pDataToInclude, int cbDataToInclude) {
		return 0;
	}
	virtual bool GetEncryptedAppTicket(void *pTicket, int cbMaxTicket, uint32 *pcbTicket) {
		return 0;
	}
	virtual int GetGameBadgeLevel(int nSeries, bool bFoil) {
		return 0;
	}
	virtual int GetPlayerSteamLevel() {
		return 0;
	}
	virtual SteamAPICall_t RequestStoreAuthURL(const char *pchRedirectURL) {
		return 0;
	}
	virtual bool BIsPhoneVerified() {
		return 0;
	}
	virtual bool BIsTwoFactorEnabled() {
		return 0;
	}
	virtual bool BIsPhoneIdentifying() {
		return 0;
	}
	virtual bool BIsPhoneRequiringVerification() {
		return 0;
	}
DerivedInterfaceGenerator_End(ISteamUser);

DerivedInterfaceGenerator_Begin(ISteamFriends);
	virtual const char *GetPersonaName() {
		return 0;
	}
	virtual SteamAPICall_t SetPersonaName(const char *pchPersonaName) {
		return 0;
	}
	virtual EPersonaState GetPersonaState() {
		return (EPersonaState)0;
	}
	virtual int GetFriendCount(int iFriendFlags) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual CSteamID GetFriendByIndex(int iFriend, int iFriendFlags) {
		CSteamID fuck;
		return fuck;
	}
	virtual EFriendRelationship GetFriendRelationship(CSteamID steamIDFriend) {
		return (EFriendRelationship)0;
	}
	virtual EPersonaState GetFriendPersonaState(CSteamID steamIDFriend) {
		return (EPersonaState)0;
	}
	virtual const char *GetFriendPersonaName(CSteamID steamIDFriend) {
		return 0;
	}
	virtual bool GetFriendGamePlayed(CSteamID steamIDFriend, OUT_STRUCT() FriendGameInfo_t *pFriendGameInfo) {
		return 0;
	}
	virtual const char *GetFriendPersonaNameHistory(CSteamID steamIDFriend, int iPersonaName) {
		return 0;
	}
	virtual int GetFriendSteamLevel(CSteamID steamIDFriend) {
		return 0;
	}
	virtual const char *GetPlayerNickname(CSteamID steamIDPlayer) {
		return 0;
	}
	virtual int GetFriendsGroupCount() {
		return 0;
	}
	virtual FriendsGroupID_t GetFriendsGroupIDByIndex(int iFG) {
		return 0;
	}
	virtual const char *GetFriendsGroupName(FriendsGroupID_t friendsGroupID) {
		return 0;
	}
	virtual int GetFriendsGroupMembersCount(FriendsGroupID_t friendsGroupID) {
		return 0;
	}
	virtual void GetFriendsGroupMembersList(FriendsGroupID_t friendsGroupID, OUT_ARRAY_CALL(nMembersCount, GetFriendsGroupMembersCount, friendsGroupID) CSteamID *pOutSteamIDMembers, int nMembersCount) VFunction_STUB
	virtual bool HasFriend(CSteamID steamIDFriend, int iFriendFlags) {
		return 0;
	}
	virtual int GetClanCount() {
		return 0;
	}
	virtual CSteamID GetClanByIndex(int iClan) {
		CSteamID fuck;
		return fuck;
	}
	virtual const char *GetClanName(CSteamID steamIDClan) {
		return 0;
	}
	virtual const char *GetClanTag(CSteamID steamIDClan) {
		return 0;
	}
	virtual bool GetClanActivityCounts(CSteamID steamIDClan, int *pnOnline, int *pnInGame, int *pnChatting) {
		return 0;
	}
	virtual SteamAPICall_t DownloadClanActivityCounts(ARRAY_COUNT(cClansToRequest) CSteamID *psteamIDClans, int cClansToRequest) {
		return 0;
	}
	virtual int GetFriendCountFromSource(CSteamID steamIDSource) {
		return 0;
	}
	virtual CSteamID GetFriendFromSourceByIndex(CSteamID steamIDSource, int iFriend) {
		CSteamID fuck;
		return fuck;
	}
	virtual bool IsUserInSource(CSteamID steamIDUser, CSteamID steamIDSource) {
		return 0;
	}
	virtual void SetInGameVoiceSpeaking(CSteamID steamIDUser, bool bSpeaking) VFunction_STUB
	virtual void ActivateGameOverlay(const char *pchDialog) VFunction_STUB
	virtual void ActivateGameOverlayToUser(const char *pchDialog, CSteamID steamID) VFunction_STUB
	virtual void ActivateGameOverlayToWebPage(const char *pchURL) VFunction_STUB
	virtual void ActivateGameOverlayToStore(AppId_t nAppID, EOverlayToStoreFlag eFlag) VFunction_STUB
	virtual void SetPlayedWith(CSteamID steamIDUserPlayedWith) VFunction_STUB
	virtual void ActivateGameOverlayInviteDialog(CSteamID steamIDLobby) VFunction_STUB
	virtual int GetSmallFriendAvatar(CSteamID steamIDFriend) {
		return 0;
	}
	virtual int GetMediumFriendAvatar(CSteamID steamIDFriend) {
		return 0;
	}
	virtual int GetLargeFriendAvatar(CSteamID steamIDFriend) {
		return 0;
	}
	virtual bool RequestUserInformation(CSteamID steamIDUser, bool bRequireNameOnly) {
		return 0;
	}
	virtual SteamAPICall_t RequestClanOfficerList(CSteamID steamIDClan) {
		return 0;
	}
	virtual CSteamID GetClanOwner(CSteamID steamIDClan) {
		CSteamID fuck;
		return fuck;
	}
	virtual int GetClanOfficerCount(CSteamID steamIDClan) {
		return 0;
	}
	virtual CSteamID GetClanOfficerByIndex(CSteamID steamIDClan, int iOfficer) {
		CSteamID fuck;
		return fuck;
	}
	virtual uint32 GetUserRestrictions() {
		return 0;
	}
	virtual bool SetRichPresence(const char *pchKey, const char *pchValue) {
		//FakeSteamAPI_AppendLog(LogLevel_Info, "Application is setting rich presence(%s) to \"%s\".", pchKey, pchValue);
		return 0;
	}
	virtual void ClearRichPresence() VFunction_STUB
	virtual const char *GetFriendRichPresence(CSteamID steamIDFriend, const char *pchKey) {
		return 0;
	}
	virtual int GetFriendRichPresenceKeyCount(CSteamID steamIDFriend) {
		return 0;
	}
	virtual const char *GetFriendRichPresenceKeyByIndex(CSteamID steamIDFriend, int iKey) {
		return 0;
	}
	virtual void RequestFriendRichPresence(CSteamID steamIDFriend) VFunction_STUB
	virtual bool InviteUserToGame(CSteamID steamIDFriend, const char *pchConnectString) {
		return 0;
	}
	virtual int GetCoplayFriendCount() {
		return 0;
	}
	virtual CSteamID GetCoplayFriend(int iCoplayFriend) {
		CSteamID fuck;
		return fuck;
	}
	virtual int GetFriendCoplayTime(CSteamID steamIDFriend) {
		return 0;
	}
	virtual AppId_t GetFriendCoplayGame(CSteamID steamIDFriend) {
		return 0;
	}
	virtual SteamAPICall_t JoinClanChatRoom(CSteamID steamIDClan) {
		return 0;
	}
	virtual bool LeaveClanChatRoom(CSteamID steamIDClan) {
		return 0;
	}
	virtual int GetClanChatMemberCount(CSteamID steamIDClan) {
		return 0;
	}
	virtual CSteamID GetChatMemberByIndex(CSteamID steamIDClan, int iUser) {
		CSteamID fuck;
		return fuck;
	}
	virtual bool SendClanChatMessage(CSteamID steamIDClanChat, const char *pchText) {
		return 0;
	}
	virtual int GetClanChatMessage(CSteamID steamIDClanChat, int iMessage, void *prgchText, int cchTextMax, EChatEntryType *peChatEntryType, CSteamID *psteamidChatter) {
		return 0;
	}
	virtual bool IsClanChatAdmin(CSteamID steamIDClanChat, CSteamID steamIDUser) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool IsClanChatWindowOpenInSteam(CSteamID steamIDClanChat) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool OpenClanChatWindowInSteam(CSteamID steamIDClanChat) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool CloseClanChatWindowInSteam(CSteamID steamIDClanChat) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetListenForFriendsMessages(bool bInterceptEnabled) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool ReplyToFriendMessage(CSteamID steamIDFriend, const char *pchMsgToSend) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual int GetFriendMessage(CSteamID steamIDFriend, int iMessageID, void *pvData, int cubData, EChatEntryType *peChatEntryType) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t GetFollowerCount(CSteamID steamID) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t IsFollowing(CSteamID steamID) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t EnumerateFollowingList(uint32 unStartIndex) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool IsClanPublic(CSteamID steamIDClan) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool IsClanOfficialGameGroup(CSteamID steamIDClan) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
DerivedInterfaceGenerator_End(ISteamFriends);

DerivedInterfaceGenerator_Begin(ISteamUtils);
	virtual uint32 GetSecondsSinceAppActive() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual uint32 GetSecondsSinceComputerActive() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual EUniverse GetConnectedUniverse() {
		FakeSteamAPI_LogFuncBeingCalled();
		return (EUniverse)0;
	}
	virtual uint32 GetServerRealTime() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual const char *GetIPCountry() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetImageSize(int iImage, uint32 *pnWidth, uint32 *pnHeight) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetImageRGBA(int iImage, uint8 *pubDest, int nDestBufferSize) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetCSERIPPort(uint32 *unIP, uint16 *usPort) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual uint8 GetCurrentBatteryPower() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual uint32 GetAppID() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual void SetOverlayNotificationPosition(ENotificationPosition eNotificationPosition) VFunction_STUB
	virtual bool IsAPICallCompleted(SteamAPICall_t hSteamAPICall, bool *pbFailed) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual ESteamAPICallFailure GetAPICallFailureReason(SteamAPICall_t hSteamAPICall) {
		FakeSteamAPI_LogFuncBeingCalled();
		return (ESteamAPICallFailure)0;
	}
	virtual bool GetAPICallResult(SteamAPICall_t hSteamAPICall, void *pCallback, int cubCallback, int iCallbackExpected, bool *pbFailed) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	STEAM_PRIVATE_API(virtual void RunFrame() VFunction_STUB )
	virtual uint32 GetIPCCallCount() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual void SetWarningMessageHook(SteamAPIWarningMessageHook_t pFunction) VFunction_STUB
	virtual bool IsOverlayEnabled() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool BOverlayNeedsPresent() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	CALL_RESULT(CheckFileSignature_t)
	virtual SteamAPICall_t CheckFileSignature(const char *szFileName) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool ShowGamepadTextInput(EGamepadTextInputMode eInputMode, EGamepadTextInputLineMode eLineInputMode, const char *pchDescription, uint32 unCharMax, const char *pchExistingText) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual uint32 GetEnteredGamepadTextLength() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetEnteredGamepadTextInput(char *pchText, uint32 cchText) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual const char *GetSteamUILanguage() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool IsSteamRunningInVR() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual void SetOverlayNotificationInset(int nHorizontalInset, int nVerticalInset) VFunction_STUB
	virtual bool IsSteamInBigPictureMode() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual void StartVRDashboard() VFunction_STUB
	virtual bool IsVRHeadsetStreamingEnabled() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual void SetVRHeadsetStreamingEnabled(bool bEnabled) VFunction_STUB
DerivedInterfaceGenerator_End(ISteamUtils);

DerivedInterfaceGenerator_Begin(ISteamMatchmaking);
	virtual int GetFavoriteGameCount() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetFavoriteGame(int iGame, AppId_t *pnAppID, uint32 *pnIP, uint16 *pnConnPort, uint16 *pnQueryPort, uint32 *punFlags, uint32 *pRTime32LastPlayedOnServer) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual int AddFavoriteGame(AppId_t nAppID, uint32 nIP, uint16 nConnPort, uint16 nQueryPort, uint32 unFlags, uint32 rTime32LastPlayedOnServer) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool RemoveFavoriteGame(AppId_t nAppID, uint32 nIP, uint16 nConnPort, uint16 nQueryPort, uint32 unFlags) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t RequestLobbyList() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual void AddRequestLobbyListStringFilter(const char *pchKeyToMatch, const char *pchValueToMatch, ELobbyComparison eComparisonType) VFunction_STUB
	virtual void AddRequestLobbyListNumericalFilter(const char *pchKeyToMatch, int nValueToMatch, ELobbyComparison eComparisonType) VFunction_STUB
	virtual void AddRequestLobbyListNearValueFilter(const char *pchKeyToMatch, int nValueToBeCloseTo) VFunction_STUB
	virtual void AddRequestLobbyListFilterSlotsAvailable(int nSlotsAvailable) VFunction_STUB
	virtual void AddRequestLobbyListDistanceFilter(ELobbyDistanceFilter eLobbyDistanceFilter) VFunction_STUB
	virtual void AddRequestLobbyListResultCountFilter(int cMaxResults) VFunction_STUB
	virtual void AddRequestLobbyListCompatibleMembersFilter(CSteamID steamIDLobby) VFunction_STUB
	virtual CSteamID GetLobbyByIndex(int iLobby) {
		CSteamID fuck;
		FakeSteamAPI_LogFuncBeingCalled();
		return fuck;
	}
	virtual SteamAPICall_t CreateLobby(ELobbyType eLobbyType, int cMaxMembers) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t JoinLobby(CSteamID steamIDLobby) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual void LeaveLobby(CSteamID steamIDLobby) VFunction_STUB
	virtual bool InviteUserToLobby(CSteamID steamIDLobby, CSteamID steamIDInvitee) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual int GetNumLobbyMembers(CSteamID steamIDLobby) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual CSteamID GetLobbyMemberByIndex(CSteamID steamIDLobby, int iMember) {
		CSteamID fuck;
		FakeSteamAPI_LogFuncBeingCalled();
		return fuck;
	}
	virtual const char *GetLobbyData(CSteamID steamIDLobby, const char *pchKey) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetLobbyData(CSteamID steamIDLobby, const char *pchKey, const char *pchValue) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual int GetLobbyDataCount(CSteamID steamIDLobby) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetLobbyDataByIndex(CSteamID steamIDLobby, int iLobbyData, char *pchKey, int cchKeyBufferSize, char *pchValue, int cchValueBufferSize) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool DeleteLobbyData(CSteamID steamIDLobby, const char *pchKey) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual const char *GetLobbyMemberData(CSteamID steamIDLobby, CSteamID steamIDUser, const char *pchKey) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual void SetLobbyMemberData(CSteamID steamIDLobby, const char *pchKey, const char *pchValue) VFunction_STUB
	virtual bool SendLobbyChatMsg(CSteamID steamIDLobby, const void *pvMsgBody, int cubMsgBody) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual int GetLobbyChatEntry(CSteamID steamIDLobby, int iChatID, CSteamID *pSteamIDUser, void *pvData, int cubData, EChatEntryType *peChatEntryType) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool RequestLobbyData(CSteamID steamIDLobby) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual void SetLobbyGameServer(CSteamID steamIDLobby, uint32 unGameServerIP, uint16 unGameServerPort, CSteamID steamIDGameServer) VFunction_STUB
	virtual bool GetLobbyGameServer(CSteamID steamIDLobby, uint32 *punGameServerIP, uint16 *punGameServerPort, CSteamID *psteamIDGameServer) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetLobbyMemberLimit(CSteamID steamIDLobby, int cMaxMembers) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual int GetLobbyMemberLimit(CSteamID steamIDLobby) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetLobbyType(CSteamID steamIDLobby, ELobbyType eLobbyType) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetLobbyJoinable(CSteamID steamIDLobby, bool bLobbyJoinable) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual CSteamID GetLobbyOwner(CSteamID steamIDLobby) {
		CSteamID fuck;
		FakeSteamAPI_LogFuncBeingCalled();
		return fuck;
	}
	virtual bool SetLobbyOwner(CSteamID steamIDLobby, CSteamID steamIDNewOwner) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetLinkedLobby(CSteamID steamIDLobby, CSteamID steamIDLobbyDependent) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
DerivedInterfaceGenerator_End(ISteamMatchmaking);

DerivedInterfaceGenerator_Begin(ISteamUserStats);
	virtual bool RequestCurrentStats() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetStat(const char *pchName, int32 *pData) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetStat(const char *pchName, float *pData) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetStat(const char *pchName, int32 nData) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetStat(const char *pchName, float fData) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool UpdateAvgRateStat(const char *pchName, float flCountThisSession, double dSessionLength) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetAchievement(const char *pchName, bool *pbAchieved) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetAchievement(const char *pchName) {
		FakeSteamAPI_LogFuncBeingCalled();
		FakeSteamAPI_AppendLog(LogLevel_Info, "Application is setting achievement %s. Returning false to the application.", pchName);
		return false;
	}
	virtual bool ClearAchievement(const char *pchName) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetAchievementAndUnlockTime(const char *pchName, bool *pbAchieved, uint32 *punUnlockTime) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool StoreStats() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual int GetAchievementIcon(const char *pchName) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual const char *GetAchievementDisplayAttribute(const char *pchName, const char *pchKey) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool IndicateAchievementProgress(const char *pchName, uint32 nCurProgress, uint32 nMaxProgress) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual uint32 GetNumAchievements() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual const char *GetAchievementName(uint32 iAchievement) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t RequestUserStats(CSteamID steamIDUser) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetUserStat(CSteamID steamIDUser, const char *pchName, int32 *pData) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetUserStat(CSteamID steamIDUser, const char *pchName, float *pData) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetUserAchievement(CSteamID steamIDUser, const char *pchName, bool *pbAchieved) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetUserAchievementAndUnlockTime(CSteamID steamIDUser, const char *pchName, bool *pbAchieved, uint32 *punUnlockTime) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool ResetAllStats(bool bAchievementsToo) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t FindOrCreateLeaderboard(const char *pchLeaderboardName, ELeaderboardSortMethod eLeaderboardSortMethod, ELeaderboardDisplayType eLeaderboardDisplayType) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t FindLeaderboard(const char *pchLeaderboardName) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual const char *GetLeaderboardName(SteamLeaderboard_t hSteamLeaderboard) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual int GetLeaderboardEntryCount(SteamLeaderboard_t hSteamLeaderboard) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual ELeaderboardSortMethod GetLeaderboardSortMethod(SteamLeaderboard_t hSteamLeaderboard) {
		FakeSteamAPI_LogFuncBeingCalled();
		return (ELeaderboardSortMethod)0;
	}
	virtual ELeaderboardDisplayType GetLeaderboardDisplayType(SteamLeaderboard_t hSteamLeaderboard) {
		FakeSteamAPI_LogFuncBeingCalled();
		return (ELeaderboardDisplayType)0;
	}
	virtual SteamAPICall_t DownloadLeaderboardEntries(SteamLeaderboard_t hSteamLeaderboard, ELeaderboardDataRequest eLeaderboardDataRequest, int nRangeStart, int nRangeEnd) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t DownloadLeaderboardEntriesForUsers(SteamLeaderboard_t hSteamLeaderboard, CSteamID *prgUsers, int cUsers) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetDownloadedLeaderboardEntry(SteamLeaderboardEntries_t hSteamLeaderboardEntries, int index, LeaderboardEntry_t *pLeaderboardEntry, int32 *pDetails, int cDetailsMax) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t UploadLeaderboardScore(SteamLeaderboard_t hSteamLeaderboard, ELeaderboardUploadScoreMethod eLeaderboardUploadScoreMethod, int32 nScore, const int32 *pScoreDetails, int cScoreDetailsCount) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t AttachLeaderboardUGC(SteamLeaderboard_t hSteamLeaderboard, UGCHandle_t hUGC) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t GetNumberOfCurrentPlayers() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t RequestGlobalAchievementPercentages() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual int GetMostAchievedAchievementInfo(char *pchName, uint32 unNameBufLen, float *pflPercent, bool *pbAchieved) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual int GetNextMostAchievedAchievementInfo(int iIteratorPrevious, char *pchName, uint32 unNameBufLen, float *pflPercent, bool *pbAchieved) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetAchievementAchievedPercent(const char *pchName, float *pflPercent) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t RequestGlobalStats(int nHistoryDays) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetGlobalStat(const char *pchStatName, int64 *pData) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetGlobalStat(const char *pchStatName, double *pData) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual int32 GetGlobalStatHistory(const char *pchStatName, int64 *pData, uint32 cubData) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual int32 GetGlobalStatHistory(const char *pchStatName, double *pData, uint32 cubData) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
DerivedInterfaceGenerator_End(ISteamUserStats);

DerivedInterfaceGenerator_Begin(ISteamApps);
	virtual bool BIsSubscribed() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool BIsLowViolence() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool BIsCybercafe() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool BIsVACBanned() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual const char *GetCurrentGameLanguage() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual const char *GetAvailableGameLanguages() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool BIsSubscribedApp(AppId_t appID) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool BIsDlcInstalled(AppId_t appID) {
		static int nAllowedDlcsArray[1024];
		static int nAllowedDlcsCount = 0;
		static bool bInited = false;
		bool bAssumeInstalled;

		FakeSteamAPI_LogFuncBeingCalled();

		if (!bInited) {
			FILE *fp;

			bInited = true;

			fp = fopen("DlcsFilter.txt", "r");
			if (fp != NULL) {
				FakeSteamAPI_AppendLog(LogLevel_Info, "Dlcs filter text found.");

				while (fscanf(fp, "%d", &nAllowedDlcsArray[nAllowedDlcsCount]) == 1)
					nAllowedDlcsCount++;

				FakeSteamAPI_AppendLog(
					LogLevel_Info,
					"Read total %d allowed Dlc%s.",
					nAllowedDlcsCount,
					nAllowedDlcsCount == 1 ? "" : "s"
				);

				fclose(fp);
			}
		}

		bAssumeInstalled = false;
		for (int i = 0; i < nAllowedDlcsCount; i++) {
			if (nAllowedDlcsArray[i] == appID) {
				bAssumeInstalled = true;
				break;
			}
		}

		FakeSteamAPI_AppendLog(
			LogLevel_Info,
			"Application is requesting DLC %d. Assuming %s.",
			(int)appID,
			bAssumeInstalled ? "installed" : "not installed"
		);

		return bAssumeInstalled;
	}
	virtual uint32 GetEarliestPurchaseUnixTime(AppId_t nAppID) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool BIsSubscribedFromFreeWeekend() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual int GetDLCCount() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool BGetDLCDataByIndex(int iDLC, AppId_t *pAppID, bool *pbAvailable, char *pchName, int cchNameBufferSize) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual void InstallDLC(AppId_t nAppID) VFunction_STUB
	virtual void UninstallDLC(AppId_t nAppID) VFunction_STUB
	virtual void RequestAppProofOfPurchaseKey(AppId_t nAppID) VFunction_STUB
	virtual bool GetCurrentBetaName(char *pchName, int cchNameBufferSize) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool MarkContentCorrupt(bool bMissingFilesOnly) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual uint32 GetInstalledDepots(AppId_t appID, DepotId_t *pvecDepots, uint32 cMaxDepots) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual uint32 GetAppInstallDir(AppId_t appID, char *pchFolder, uint32 cchFolderBufferSize) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool BIsAppInstalled(AppId_t appID) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual CSteamID GetAppOwner() {
		CSteamID fuck;
		FakeSteamAPI_LogFuncBeingCalled();
		return fuck;
	}
	virtual const char *GetLaunchQueryParam(const char *pchKey) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetDlcDownloadProgress(AppId_t nAppID, uint64 *punBytesDownloaded, uint64 *punBytesTotal) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual int GetAppBuildId() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual void RequestAllProofOfPurchaseKeys() VFunction_STUB
	virtual SteamAPICall_t GetFileDetails(const char* pszFileName) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
DerivedInterfaceGenerator_End(ISteamApps);

DerivedInterfaceGenerator_Begin(ISteamMatchmakingServers);
	virtual HServerListRequest RequestInternetServerList(AppId_t iApp, MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual HServerListRequest RequestLANServerList(AppId_t iApp, ISteamMatchmakingServerListResponse *pRequestServersResponse) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual HServerListRequest RequestFriendsServerList(AppId_t iApp, MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual HServerListRequest RequestFavoritesServerList(AppId_t iApp, MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual HServerListRequest RequestHistoryServerList(AppId_t iApp, MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual HServerListRequest RequestSpectatorServerList(AppId_t iApp, MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual void ReleaseRequest(HServerListRequest hServerListRequest) VFunction_STUB
	virtual gameserveritem_t *GetServerDetails(HServerListRequest hRequest, int iServer) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual void CancelQuery(HServerListRequest hRequest) VFunction_STUB
	virtual void RefreshQuery(HServerListRequest hRequest) VFunction_STUB
	virtual bool IsRefreshing(HServerListRequest hRequest) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual int GetServerCount(HServerListRequest hRequest) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual void RefreshServer(HServerListRequest hRequest, int iServer) VFunction_STUB
	virtual HServerQuery PingServer(uint32 unIP, uint16 usPort, ISteamMatchmakingPingResponse *pRequestServersResponse) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual HServerQuery PlayerDetails(uint32 unIP, uint16 usPort, ISteamMatchmakingPlayersResponse *pRequestServersResponse) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual HServerQuery ServerRules(uint32 unIP, uint16 usPort, ISteamMatchmakingRulesResponse *pRequestServersResponse) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual void CancelServerQuery(HServerQuery hServerQuery) VFunction_STUB
DerivedInterfaceGenerator_End(ISteamMatchmakingServers);

DerivedInterfaceGenerator_Begin(ISteamNetworking);
	virtual bool SendP2PPacket(CSteamID steamIDRemote, const void *pubData, uint32 cubData, EP2PSend eP2PSendType, int nChannel = 0) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool IsP2PPacketAvailable(uint32 *pcubMsgSize, int nChannel = 0) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool ReadP2PPacket(void *pubDest, uint32 cubDest, uint32 *pcubMsgSize, CSteamID *psteamIDRemote, int nChannel = 0) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool AcceptP2PSessionWithUser(CSteamID steamIDRemote) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool CloseP2PSessionWithUser(CSteamID steamIDRemote) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool CloseP2PChannelWithUser(CSteamID steamIDRemote, int nChannel) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetP2PSessionState(CSteamID steamIDRemote, P2PSessionState_t *pConnectionState) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool AllowP2PPacketRelay(bool bAllow) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SNetListenSocket_t CreateListenSocket(int nVirtualP2PPort, uint32 nIP, uint16 nPort, bool bAllowUseOfPacketRelay) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SNetSocket_t CreateP2PConnectionSocket(CSteamID steamIDTarget, int nVirtualPort, int nTimeoutSec, bool bAllowUseOfPacketRelay) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SNetSocket_t CreateConnectionSocket(uint32 nIP, uint16 nPort, int nTimeoutSec) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool DestroySocket(SNetSocket_t hSocket, bool bNotifyRemoteEnd) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool DestroyListenSocket(SNetListenSocket_t hSocket, bool bNotifyRemoteEnd) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SendDataOnSocket(SNetSocket_t hSocket, void *pubData, uint32 cubData, bool bReliable) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool IsDataAvailableOnSocket(SNetSocket_t hSocket, uint32 *pcubMsgSize) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool RetrieveDataFromSocket(SNetSocket_t hSocket, void *pubDest, uint32 cubDest, uint32 *pcubMsgSize) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool IsDataAvailable(SNetListenSocket_t hListenSocket, uint32 *pcubMsgSize, SNetSocket_t *phSocket) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool RetrieveData(SNetListenSocket_t hListenSocket, void *pubDest, uint32 cubDest, uint32 *pcubMsgSize, SNetSocket_t *phSocket) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetSocketInfo(SNetSocket_t hSocket, CSteamID *pSteamIDRemote, int *peSocketStatus, uint32 *punIPRemote, uint16 *punPortRemote) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetListenSocketInfo(SNetListenSocket_t hListenSocket, uint32 *pnIP, uint16 *pnPort) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual ESNetSocketConnectionType GetSocketConnectionType(SNetSocket_t hSocket) {
		FakeSteamAPI_LogFuncBeingCalled();
		return (ESNetSocketConnectionType)0;
	}
	virtual int GetMaxPacketSize(SNetSocket_t hSocket) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
DerivedInterfaceGenerator_End(ISteamNetworking);

DerivedInterfaceGenerator_Begin(ISteamRemoteStorage);
	virtual bool FileWrite(const char *pchFile, const void *pvData, int32 cubData) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual int32 FileRead(const char *pchFile, void *pvData, int32 cubDataToRead) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t FileWriteAsync(const char *pchFile, const void *pvData, uint32 cubData) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t FileReadAsync(const char *pchFile, uint32 nOffset, uint32 cubToRead) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool FileReadAsyncComplete(SteamAPICall_t hReadCall, void *pvBuffer, uint32 cubToRead) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool FileForget(const char *pchFile) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool FileDelete(const char *pchFile) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t FileShare(const char *pchFile) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetSyncPlatforms(const char *pchFile, ERemoteStoragePlatform eRemoteStoragePlatform) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual UGCFileWriteStreamHandle_t FileWriteStreamOpen(const char *pchFile) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool FileWriteStreamWriteChunk(UGCFileWriteStreamHandle_t writeHandle, const void *pvData, int32 cubData) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool FileWriteStreamClose(UGCFileWriteStreamHandle_t writeHandle) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool FileWriteStreamCancel(UGCFileWriteStreamHandle_t writeHandle) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool FileExists(const char *pchFile) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool FilePersisted(const char *pchFile) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual int32 GetFileSize(const char *pchFile) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual int64 GetFileTimestamp(const char *pchFile) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual ERemoteStoragePlatform GetSyncPlatforms(const char *pchFile) {
		FakeSteamAPI_LogFuncBeingCalled();
		return (ERemoteStoragePlatform)0;
	}
	virtual int32 GetFileCount() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual const char *GetFileNameAndSize(int iFile, int32 *pnFileSizeInBytes) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetQuota(uint64 *pnTotalBytes, uint64 *puAvailableBytes) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool IsCloudEnabledForAccount() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool IsCloudEnabledForApp() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual void SetCloudEnabledForApp(bool bEnabled) VFunction_STUB
	virtual SteamAPICall_t UGCDownload(UGCHandle_t hContent, uint32 unPriority) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetUGCDownloadProgress(UGCHandle_t hContent, int32 *pnBytesDownloaded, int32 *pnBytesExpected) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetUGCDetails(UGCHandle_t hContent, AppId_t *pnAppID, OUT_STRING() char **ppchName, int32 *pnFileSizeInBytes, CSteamID *pSteamIDOwner) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual int32 UGCRead(UGCHandle_t hContent, void *pvData, int32 cubDataToRead, uint32 cOffset, EUGCReadAction eAction) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual int32 GetCachedUGCCount() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual UGCHandle_t GetCachedUGCHandle(int32 iCachedContent) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t PublishWorkshopFile(const char *pchFile, const char *pchPreviewFile, AppId_t nConsumerAppId, const char *pchTitle, const char *pchDescription, ERemoteStoragePublishedFileVisibility eVisibility, SteamParamStringArray_t *pTags, EWorkshopFileType eWorkshopFileType) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual PublishedFileUpdateHandle_t CreatePublishedFileUpdateRequest(PublishedFileId_t unPublishedFileId) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool UpdatePublishedFileFile(PublishedFileUpdateHandle_t updateHandle, const char *pchFile) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool UpdatePublishedFilePreviewFile(PublishedFileUpdateHandle_t updateHandle, const char *pchPreviewFile) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool UpdatePublishedFileTitle(PublishedFileUpdateHandle_t updateHandle, const char *pchTitle) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool UpdatePublishedFileDescription(PublishedFileUpdateHandle_t updateHandle, const char *pchDescription) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool UpdatePublishedFileVisibility(PublishedFileUpdateHandle_t updateHandle, ERemoteStoragePublishedFileVisibility eVisibility) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool UpdatePublishedFileTags(PublishedFileUpdateHandle_t updateHandle, SteamParamStringArray_t *pTags) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t CommitPublishedFileUpdate(PublishedFileUpdateHandle_t updateHandle) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t GetPublishedFileDetails(PublishedFileId_t unPublishedFileId, uint32 unMaxSecondsOld) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t DeletePublishedFile(PublishedFileId_t unPublishedFileId) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t EnumerateUserPublishedFiles(uint32 unStartIndex) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t SubscribePublishedFile(PublishedFileId_t unPublishedFileId) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t EnumerateUserSubscribedFiles(uint32 unStartIndex) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t UnsubscribePublishedFile(PublishedFileId_t unPublishedFileId) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool UpdatePublishedFileSetChangeDescription(PublishedFileUpdateHandle_t updateHandle, const char *pchChangeDescription) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t GetPublishedItemVoteDetails(PublishedFileId_t unPublishedFileId) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t UpdateUserPublishedItemVote(PublishedFileId_t unPublishedFileId, bool bVoteUp) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t GetUserPublishedItemVoteDetails(PublishedFileId_t unPublishedFileId) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t EnumerateUserSharedWorkshopFiles(CSteamID steamId, uint32 unStartIndex, SteamParamStringArray_t *pRequiredTags, SteamParamStringArray_t *pExcludedTags) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t PublishVideo(EWorkshopVideoProvider eVideoProvider, const char *pchVideoAccount, const char *pchVideoIdentifier, const char *pchPreviewFile, AppId_t nConsumerAppId, const char *pchTitle, const char *pchDescription, ERemoteStoragePublishedFileVisibility eVisibility, SteamParamStringArray_t *pTags) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t SetUserPublishedFileAction(PublishedFileId_t unPublishedFileId, EWorkshopFileAction eAction) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t EnumeratePublishedFilesByUserAction(EWorkshopFileAction eAction, uint32 unStartIndex) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t EnumeratePublishedWorkshopFiles(EWorkshopEnumerationType eEnumerationType, uint32 unStartIndex, uint32 unCount, uint32 unDays, SteamParamStringArray_t *pTags, SteamParamStringArray_t *pUserTags) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t UGCDownloadToLocation(UGCHandle_t hContent, const char *pchLocation, uint32 unPriority) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
DerivedInterfaceGenerator_End(ISteamRemoteStorage);

DerivedInterfaceGenerator_Begin(ISteamScreenshots);
	virtual ScreenshotHandle WriteScreenshot(void *pubRGB, uint32 cubRGB, int nWidth, int nHeight) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual ScreenshotHandle AddScreenshotToLibrary(const char *pchFilename, const char *pchThumbnailFilename, int nWidth, int nHeight) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual void TriggerScreenshot() VFunction_STUB
	virtual void HookScreenshots(bool bHook) VFunction_STUB
	virtual bool SetLocation(ScreenshotHandle hScreenshot, const char *pchLocation) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool TagUser(ScreenshotHandle hScreenshot, CSteamID steamID) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool TagPublishedFile(ScreenshotHandle hScreenshot, PublishedFileId_t unPublishedFileID) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool IsScreenshotsHooked() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual ScreenshotHandle AddVRScreenshotToLibrary(EVRScreenshotType eType, const char *pchFilename, const char *pchVRFilename) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
DerivedInterfaceGenerator_End(ISteamScreenshots);

DerivedInterfaceGenerator_Begin(ISteamHTTP);
	virtual HTTPRequestHandle CreateHTTPRequest(EHTTPMethod eHTTPRequestMethod, const char *pchAbsoluteURL) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetHTTPRequestContextValue(HTTPRequestHandle hRequest, uint64 ulContextValue) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetHTTPRequestNetworkActivityTimeout(HTTPRequestHandle hRequest, uint32 unTimeoutSeconds) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetHTTPRequestHeaderValue(HTTPRequestHandle hRequest, const char *pchHeaderName, const char *pchHeaderValue) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetHTTPRequestGetOrPostParameter(HTTPRequestHandle hRequest, const char *pchParamName, const char *pchParamValue) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SendHTTPRequest(HTTPRequestHandle hRequest, SteamAPICall_t *pCallHandle) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SendHTTPRequestAndStreamResponse(HTTPRequestHandle hRequest, SteamAPICall_t *pCallHandle) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool DeferHTTPRequest(HTTPRequestHandle hRequest) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool PrioritizeHTTPRequest(HTTPRequestHandle hRequest) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetHTTPResponseHeaderSize(HTTPRequestHandle hRequest, const char *pchHeaderName, uint32 *unResponseHeaderSize) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetHTTPResponseHeaderValue(HTTPRequestHandle hRequest, const char *pchHeaderName, uint8 *pHeaderValueBuffer, uint32 unBufferSize) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetHTTPResponseBodySize(HTTPRequestHandle hRequest, uint32 *unBodySize) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetHTTPResponseBodyData(HTTPRequestHandle hRequest, uint8 *pBodyDataBuffer, uint32 unBufferSize) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetHTTPStreamingResponseBodyData(HTTPRequestHandle hRequest, uint32 cOffset, uint8 *pBodyDataBuffer, uint32 unBufferSize) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool ReleaseHTTPRequest(HTTPRequestHandle hRequest) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetHTTPDownloadProgressPct(HTTPRequestHandle hRequest, float *pflPercentOut) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetHTTPRequestRawPostBody(HTTPRequestHandle hRequest, const char *pchContentType, uint8 *pubBody, uint32 unBodyLen) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual HTTPCookieContainerHandle CreateCookieContainer(bool bAllowResponsesToModify) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool ReleaseCookieContainer(HTTPCookieContainerHandle hCookieContainer) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetCookie(HTTPCookieContainerHandle hCookieContainer, const char *pchHost, const char *pchUrl, const char *pchCookie) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetHTTPRequestCookieContainer(HTTPRequestHandle hRequest, HTTPCookieContainerHandle hCookieContainer) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetHTTPRequestUserAgentInfo(HTTPRequestHandle hRequest, const char *pchUserAgentInfo) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetHTTPRequestRequiresVerifiedCertificate(HTTPRequestHandle hRequest, bool bRequireVerifiedCertificate) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetHTTPRequestAbsoluteTimeoutMS(HTTPRequestHandle hRequest, uint32 unMilliseconds) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetHTTPRequestWasTimedOut(HTTPRequestHandle hRequest, bool *pbWasTimedOut) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
DerivedInterfaceGenerator_End(ISteamHTTP);

DerivedInterfaceGenerator_Begin(ISteamController);
	virtual bool Init() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool Shutdown() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual void RunFrame() VFunction_STUB
	virtual int GetConnectedControllers(ControllerHandle_t *handlesOut) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool ShowBindingPanel(ControllerHandle_t controllerHandle) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual ControllerActionSetHandle_t GetActionSetHandle(const char *pszActionSetName) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual void ActivateActionSet(ControllerHandle_t controllerHandle, ControllerActionSetHandle_t actionSetHandle) VFunction_STUB
	virtual ControllerActionSetHandle_t GetCurrentActionSet(ControllerHandle_t controllerHandle) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual void ActivateActionSetLayer(ControllerHandle_t controllerHandle, ControllerActionSetHandle_t actionSetLayerHandle) VFunction_STUB
	virtual void DeactivateActionSetLayer(ControllerHandle_t controllerHandle, ControllerActionSetHandle_t actionSetLayerHandle) VFunction_STUB
	virtual void DeactivateAllActionSetLayers(ControllerHandle_t controllerHandle) VFunction_STUB
	virtual int GetActiveActionSetLayers(ControllerHandle_t controllerHandle, ControllerActionSetHandle_t *handlesOut) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual ControllerDigitalActionHandle_t GetDigitalActionHandle(const char *pszActionName) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual ControllerDigitalActionData_t GetDigitalActionData(ControllerHandle_t controllerHandle, ControllerDigitalActionHandle_t digitalActionHandle) {
		ControllerDigitalActionData_t fuck;
		FakeSteamAPI_LogFuncBeingCalled();
		return fuck;
	}
	virtual int GetDigitalActionOrigins(ControllerHandle_t controllerHandle, ControllerActionSetHandle_t actionSetHandle, ControllerDigitalActionHandle_t digitalActionHandle, EControllerActionOrigin *originsOut) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual ControllerAnalogActionHandle_t GetAnalogActionHandle(const char *pszActionName) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual ControllerAnalogActionData_t GetAnalogActionData(ControllerHandle_t controllerHandle, ControllerAnalogActionHandle_t analogActionHandle) {
		ControllerAnalogActionData_t fuck;
		FakeSteamAPI_LogFuncBeingCalled();
		return fuck;
	}
	virtual int GetAnalogActionOrigins(ControllerHandle_t controllerHandle, ControllerActionSetHandle_t actionSetHandle, ControllerAnalogActionHandle_t analogActionHandle, EControllerActionOrigin *originsOut) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual void StopAnalogActionMomentum(ControllerHandle_t controllerHandle, ControllerAnalogActionHandle_t eAction) VFunction_STUB
	virtual void TriggerHapticPulse(ControllerHandle_t controllerHandle, ESteamControllerPad eTargetPad, unsigned short usDurationMicroSec) VFunction_STUB
	virtual void TriggerRepeatedHapticPulse(ControllerHandle_t controllerHandle, ESteamControllerPad eTargetPad, unsigned short usDurationMicroSec, unsigned short usOffMicroSec, unsigned short unRepeat, unsigned int nFlags) VFunction_STUB
	virtual void TriggerVibration(ControllerHandle_t controllerHandle, unsigned short usLeftSpeed, unsigned short usRightSpeed) VFunction_STUB
	virtual void SetLEDColor(ControllerHandle_t controllerHandle, uint8 nColorR, uint8 nColorG, uint8 nColorB, unsigned int nFlags) VFunction_STUB
	virtual int GetGamepadIndexForController(ControllerHandle_t ulControllerHandle) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual ControllerHandle_t GetControllerForGamepadIndex(int nIndex) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual ControllerMotionData_t GetMotionData(ControllerHandle_t controllerHandle) {
		ControllerMotionData_t fuck;
		FakeSteamAPI_LogFuncBeingCalled();
		return fuck;
	}
	virtual bool ShowDigitalActionOrigins(ControllerHandle_t controllerHandle, ControllerDigitalActionHandle_t digitalActionHandle, float flScale, float flXPosition, float flYPosition) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool ShowAnalogActionOrigins(ControllerHandle_t controllerHandle, ControllerAnalogActionHandle_t analogActionHandle, float flScale, float flXPosition, float flYPosition) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual const char *GetStringForActionOrigin(EControllerActionOrigin eOrigin) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual const char *GetGlyphForActionOrigin(EControllerActionOrigin eOrigin) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual ESteamInputType GetInputTypeForHandle(ControllerHandle_t controllerHandle) {
		FakeSteamAPI_LogFuncBeingCalled();
		return (ESteamInputType)0;
	}
DerivedInterfaceGenerator_End(ISteamController);

DerivedInterfaceGenerator_Begin(ISteamUGC);
	virtual UGCQueryHandle_t CreateQueryUserUGCRequest(AccountID_t unAccountID, EUserUGCList eListType, EUGCMatchingUGCType eMatchingUGCType, EUserUGCListSortOrder eSortOrder, AppId_t nCreatorAppID, AppId_t nConsumerAppID, uint32 unPage) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual UGCQueryHandle_t CreateQueryAllUGCRequest(EUGCQuery eQueryType, EUGCMatchingUGCType eMatchingeMatchingUGCTypeFileType, AppId_t nCreatorAppID, AppId_t nConsumerAppID, uint32 unPage) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual UGCQueryHandle_t CreateQueryUGCDetailsRequest(PublishedFileId_t *pvecPublishedFileID, uint32 unNumPublishedFileIDs) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t SendQueryUGCRequest(UGCQueryHandle_t handle) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetQueryUGCResult(UGCQueryHandle_t handle, uint32 index, SteamUGCDetails_t *pDetails) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetQueryUGCPreviewURL(UGCQueryHandle_t handle, uint32 index, char *pchURL, uint32 cchURLSize) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetQueryUGCMetadata(UGCQueryHandle_t handle, uint32 index, char *pchMetadata, uint32 cchMetadatasize) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetQueryUGCChildren(UGCQueryHandle_t handle, uint32 index, PublishedFileId_t* pvecPublishedFileID, uint32 cMaxEntries) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetQueryUGCStatistic(UGCQueryHandle_t handle, uint32 index, EItemStatistic eStatType, uint64 *pStatValue) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual uint32 GetQueryUGCNumAdditionalPreviews(UGCQueryHandle_t handle, uint32 index) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetQueryUGCAdditionalPreview(UGCQueryHandle_t handle, uint32 index, uint32 previewIndex, char *pchURLOrVideoID, uint32 cchURLSize, char *pchOriginalFileName, uint32 cchOriginalFileNameSize, EItemPreviewType *pPreviewType) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual uint32 GetQueryUGCNumKeyValueTags(UGCQueryHandle_t handle, uint32 index) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetQueryUGCKeyValueTag(UGCQueryHandle_t handle, uint32 index, uint32 keyValueTagIndex, char *pchKey, uint32 cchKeySize, char *pchValue, uint32 cchValueSize) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool ReleaseQueryUGCRequest(UGCQueryHandle_t handle) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool AddRequiredTag(UGCQueryHandle_t handle, const char *pTagName) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool AddExcludedTag(UGCQueryHandle_t handle, const char *pTagName) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetReturnOnlyIDs(UGCQueryHandle_t handle, bool bReturnOnlyIDs) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetReturnKeyValueTags(UGCQueryHandle_t handle, bool bReturnKeyValueTags) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetReturnLongDescription(UGCQueryHandle_t handle, bool bReturnLongDescription) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetReturnMetadata(UGCQueryHandle_t handle, bool bReturnMetadata) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetReturnChildren(UGCQueryHandle_t handle, bool bReturnChildren) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetReturnAdditionalPreviews(UGCQueryHandle_t handle, bool bReturnAdditionalPreviews) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetReturnTotalOnly(UGCQueryHandle_t handle, bool bReturnTotalOnly) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetReturnPlaytimeStats(UGCQueryHandle_t handle, uint32 unDays) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetLanguage(UGCQueryHandle_t handle, const char *pchLanguage) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetAllowCachedResponse(UGCQueryHandle_t handle, uint32 unMaxAgeSeconds) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetCloudFileNameFilter(UGCQueryHandle_t handle, const char *pMatchCloudFileName) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetMatchAnyTag(UGCQueryHandle_t handle, bool bMatchAnyTag) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetSearchText(UGCQueryHandle_t handle, const char *pSearchText) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetRankedByTrendDays(UGCQueryHandle_t handle, uint32 unDays) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool AddRequiredKeyValueTag(UGCQueryHandle_t handle, const char *pKey, const char *pValue) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t RequestUGCDetails(PublishedFileId_t nPublishedFileID, uint32 unMaxAgeSeconds) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t CreateItem(AppId_t nConsumerAppId, EWorkshopFileType eFileType) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual UGCUpdateHandle_t StartItemUpdate(AppId_t nConsumerAppId, PublishedFileId_t nPublishedFileID) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetItemTitle(UGCUpdateHandle_t handle, const char *pchTitle) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetItemDescription(UGCUpdateHandle_t handle, const char *pchDescription) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetItemUpdateLanguage(UGCUpdateHandle_t handle, const char *pchLanguage) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetItemMetadata(UGCUpdateHandle_t handle, const char *pchMetaData) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetItemVisibility(UGCUpdateHandle_t handle, ERemoteStoragePublishedFileVisibility eVisibility) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetItemTags(UGCUpdateHandle_t updateHandle, const SteamParamStringArray_t *pTags) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetItemContent(UGCUpdateHandle_t handle, const char *pszContentFolder) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetItemPreview(UGCUpdateHandle_t handle, const char *pszPreviewFile) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool RemoveItemKeyValueTags(UGCUpdateHandle_t handle, const char *pchKey) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool AddItemKeyValueTag(UGCUpdateHandle_t handle, const char *pchKey, const char *pchValue) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool AddItemPreviewFile(UGCUpdateHandle_t handle, const char *pszPreviewFile, EItemPreviewType type) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool AddItemPreviewVideo(UGCUpdateHandle_t handle, const char *pszVideoID) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool UpdateItemPreviewFile(UGCUpdateHandle_t handle, uint32 index, const char *pszPreviewFile) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool UpdateItemPreviewVideo(UGCUpdateHandle_t handle, uint32 index, const char *pszVideoID) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool RemoveItemPreview(UGCUpdateHandle_t handle, uint32 index) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t SubmitItemUpdate(UGCUpdateHandle_t handle, const char *pchChangeNote) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual EItemUpdateStatus GetItemUpdateProgress(UGCUpdateHandle_t handle, uint64 *punBytesProcessed, uint64* punBytesTotal) {
		FakeSteamAPI_LogFuncBeingCalled();
		return (EItemUpdateStatus)0;
	}
	virtual SteamAPICall_t SetUserItemVote(PublishedFileId_t nPublishedFileID, bool bVoteUp) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t GetUserItemVote(PublishedFileId_t nPublishedFileID) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t AddItemToFavorites(AppId_t nAppId, PublishedFileId_t nPublishedFileID) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t RemoveItemFromFavorites(AppId_t nAppId, PublishedFileId_t nPublishedFileID) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t SubscribeItem(PublishedFileId_t nPublishedFileID) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t UnsubscribeItem(PublishedFileId_t nPublishedFileID) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual uint32 GetNumSubscribedItems() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual uint32 GetSubscribedItems(PublishedFileId_t* pvecPublishedFileID, uint32 cMaxEntries) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual uint32 GetItemState(PublishedFileId_t nPublishedFileID) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetItemInstallInfo(PublishedFileId_t nPublishedFileID, uint64 *punSizeOnDisk, char *pchFolder, uint32 cchFolderSize, uint32 *punTimeStamp) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetItemDownloadInfo(PublishedFileId_t nPublishedFileID, uint64 *punBytesDownloaded, uint64 *punBytesTotal) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool DownloadItem(PublishedFileId_t nPublishedFileID, bool bHighPriority) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool BInitWorkshopForGameServer(DepotId_t unWorkshopDepotID, const char *pszFolder) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual void SuspendDownloads(bool bSuspend) {
		FakeSteamAPI_LogFuncBeingCalled();
	}
	virtual SteamAPICall_t StartPlaytimeTracking(PublishedFileId_t *pvecPublishedFileID, uint32 unNumPublishedFileIDs) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t StopPlaytimeTracking(PublishedFileId_t *pvecPublishedFileID, uint32 unNumPublishedFileIDs) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t StopPlaytimeTrackingForAllItems() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t AddDependency(PublishedFileId_t nParentPublishedFileID, PublishedFileId_t nChildPublishedFileID) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t RemoveDependency(PublishedFileId_t nParentPublishedFileID, PublishedFileId_t nChildPublishedFileID) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t AddAppDependency(PublishedFileId_t nPublishedFileID, AppId_t nAppID) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t RemoveAppDependency(PublishedFileId_t nPublishedFileID, AppId_t nAppID) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t GetAppDependencies(PublishedFileId_t nPublishedFileID) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t DeleteItem(PublishedFileId_t nPublishedFileID) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
DerivedInterfaceGenerator_End(ISteamUGC);

DerivedInterfaceGenerator_Begin(ISteamAppList);
	virtual uint32 GetNumInstalledApps() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual uint32 GetInstalledApps(AppId_t *pvecAppID, uint32 unMaxAppIDs) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual int GetAppName(AppId_t nAppID, char *pchName, int cchNameMax) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual int GetAppInstallDir(AppId_t nAppID, char *pchDirectory, int cchNameMax) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual int GetAppBuildId(AppId_t nAppID) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
DerivedInterfaceGenerator_End(ISteamAppList);

DerivedInterfaceGenerator_Begin(ISteamMusic);
	virtual bool BIsEnabled() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool BIsPlaying() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual AudioPlayback_Status GetPlaybackStatus() {
		FakeSteamAPI_LogFuncBeingCalled();
		return (AudioPlayback_Status)0;
	}
	virtual void Play() VFunction_STUB
	virtual void Pause() VFunction_STUB
	virtual void PlayPrevious() VFunction_STUB
	virtual void PlayNext() VFunction_STUB
	virtual void SetVolume(float flVolume) VFunction_STUB
	virtual float GetVolume() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
DerivedInterfaceGenerator_End(ISteamMusic);

DerivedInterfaceGenerator_Begin(ISteamMusicRemote);
	virtual bool RegisterSteamMusicRemote(const char *pchName) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool DeregisterSteamMusicRemote() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool BIsCurrentMusicRemote() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool BActivationSuccess(bool bValue) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetDisplayName(const char *pchDisplayName) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetPNGIcon_64x64(void *pvBuffer, uint32 cbBufferLength) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool EnablePlayPrevious(bool bValue) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool EnablePlayNext(bool bValue) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool EnableShuffled(bool bValue) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool EnableLooped(bool bValue) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool EnableQueue(bool bValue) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool EnablePlaylists(bool bValue) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool UpdatePlaybackStatus(AudioPlayback_Status nStatus) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool UpdateShuffled(bool bValue) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool UpdateLooped(bool bValue) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool UpdateVolume(float flValue) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool CurrentEntryWillChange() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool CurrentEntryIsAvailable(bool bAvailable) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool UpdateCurrentEntryText(const char *pchText) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool UpdateCurrentEntryElapsedSeconds(int nValue) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool UpdateCurrentEntryCoverArt(void *pvBuffer, uint32 cbBufferLength) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool CurrentEntryDidChange() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool QueueWillChange() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool ResetQueueEntries() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetQueueEntry(int nID, int nPosition, const char *pchEntryText) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetCurrentQueueEntry(int nID) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool QueueDidChange() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool PlaylistWillChange() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool ResetPlaylistEntries() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetPlaylistEntry(int nID, int nPosition, const char *pchEntryText) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetCurrentPlaylistEntry(int nID) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool PlaylistDidChange() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
DerivedInterfaceGenerator_End(ISteamMusicRemote);

DerivedInterfaceGenerator_Begin(ISteamHTMLSurface);
	virtual ~ISteamHTMLSurfaceDerived() {}
	virtual bool Init() {
		FakeSteamAPI_LogFuncBeingCalled();
		return false;
	}
	virtual bool Shutdown() {
		FakeSteamAPI_LogFuncBeingCalled();
		return false;
	}
	virtual SteamAPICall_t CreateBrowser(const char *pchUserAgent, const char *pchUserCSS) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual void RemoveBrowser(HHTMLBrowser unBrowserHandle) VFunction_STUB
	virtual void LoadURL(HHTMLBrowser unBrowserHandle, const char *pchURL, const char *pchPostData) VFunction_STUB
	virtual void SetSize(HHTMLBrowser unBrowserHandle, uint32 unWidth, uint32 unHeight) VFunction_STUB
	virtual void StopLoad(HHTMLBrowser unBrowserHandle) VFunction_STUB
	virtual void Reload(HHTMLBrowser unBrowserHandle) VFunction_STUB
	virtual void GoBack(HHTMLBrowser unBrowserHandle) VFunction_STUB
	virtual void GoForward(HHTMLBrowser unBrowserHandle) VFunction_STUB
	virtual void AddHeader(HHTMLBrowser unBrowserHandle, const char *pchKey, const char *pchValue) VFunction_STUB
	virtual void ExecuteJavascript(HHTMLBrowser unBrowserHandle, const char *pchScript) VFunction_STUB
	virtual void MouseUp(HHTMLBrowser unBrowserHandle, EHTMLMouseButton eMouseButton) VFunction_STUB
	virtual void MouseDown(HHTMLBrowser unBrowserHandle, EHTMLMouseButton eMouseButton) VFunction_STUB
	virtual void MouseDoubleClick(HHTMLBrowser unBrowserHandle, EHTMLMouseButton eMouseButton) VFunction_STUB
	virtual void MouseMove(HHTMLBrowser unBrowserHandle, int x, int y) VFunction_STUB
	virtual void MouseWheel(HHTMLBrowser unBrowserHandle, int32 nDelta) VFunction_STUB
	virtual void KeyDown(HHTMLBrowser unBrowserHandle, uint32 nNativeKeyCode, EHTMLKeyModifiers eHTMLKeyModifiers) VFunction_STUB
	virtual void KeyUp(HHTMLBrowser unBrowserHandle, uint32 nNativeKeyCode, EHTMLKeyModifiers eHTMLKeyModifiers) VFunction_STUB
	virtual void KeyChar(HHTMLBrowser unBrowserHandle, uint32 cUnicodeChar, EHTMLKeyModifiers eHTMLKeyModifiers) VFunction_STUB
	virtual void SetHorizontalScroll(HHTMLBrowser unBrowserHandle, uint32 nAbsolutePixelScroll) VFunction_STUB
	virtual void SetVerticalScroll(HHTMLBrowser unBrowserHandle, uint32 nAbsolutePixelScroll) VFunction_STUB
	virtual void SetKeyFocus(HHTMLBrowser unBrowserHandle, bool bHasKeyFocus) VFunction_STUB
	virtual void ViewSource(HHTMLBrowser unBrowserHandle) VFunction_STUB
	virtual void CopyToClipboard(HHTMLBrowser unBrowserHandle) VFunction_STUB
	virtual void PasteFromClipboard(HHTMLBrowser unBrowserHandle) VFunction_STUB
	virtual void Find(HHTMLBrowser unBrowserHandle, const char *pchSearchStr, bool bCurrentlyInFind, bool bReverse) VFunction_STUB
	virtual void StopFind(HHTMLBrowser unBrowserHandle) VFunction_STUB
	virtual void GetLinkAtPosition(HHTMLBrowser unBrowserHandle, int x, int y) VFunction_STUB
	virtual void SetCookie(const char *pchHostname, const char *pchKey, const char *pchValue, const char *pchPath = "/", RTime32 nExpires = 0, bool bSecure = false, bool bHTTPOnly = false) VFunction_STUB
	virtual void SetPageScaleFactor(HHTMLBrowser unBrowserHandle, float flZoom, int nPointX, int nPointY) VFunction_STUB
	virtual void SetBackgroundMode(HHTMLBrowser unBrowserHandle, bool bBackgroundMode) VFunction_STUB
	virtual void SetDPIScalingFactor(HHTMLBrowser unBrowserHandle, float flDPIScaling) VFunction_STUB
	virtual void AllowStartRequest(HHTMLBrowser unBrowserHandle, bool bAllowed) VFunction_STUB
	virtual void JSDialogResponse(HHTMLBrowser unBrowserHandle, bool bResult) VFunction_STUB
	virtual void FileLoadDialogResponse(HHTMLBrowser unBrowserHandle, const char **pchSelectedFiles) VFunction_STUB
DerivedInterfaceGenerator_End(ISteamHTMLSurface);

DerivedInterfaceGenerator_Begin(ISteamInventory);
	virtual EResult GetResultStatus(SteamInventoryResult_t resultHandle) {
		FakeSteamAPI_LogFuncBeingCalled();
		return (EResult)0;
	}
	virtual bool GetResultItems(SteamInventoryResult_t resultHandle, SteamItemDetails_t *pOutItemsArray, uint32 *punOutItemsArraySize) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetResultItemProperty(SteamInventoryResult_t resultHandle, uint32 unItemIndex, const char *pchPropertyName, char *pchValueBuffer, uint32 *punValueBufferSizeOut) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual uint32 GetResultTimestamp(SteamInventoryResult_t resultHandle) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool CheckResultSteamID(SteamInventoryResult_t resultHandle, CSteamID steamIDExpected) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual void DestroyResult(SteamInventoryResult_t resultHandle) {
		FakeSteamAPI_LogFuncBeingCalled();
	}
	virtual bool GetAllItems(SteamInventoryResult_t *pResultHandle) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetItemsByID(SteamInventoryResult_t *pResultHandle, const SteamItemInstanceID_t *pInstanceIDs, uint32 unCountInstanceIDs) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SerializeResult(SteamInventoryResult_t resultHandle, void *pOutBuffer, uint32 *punOutBufferSize) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool DeserializeResult(SteamInventoryResult_t *pOutResultHandle, const void *pBuffer, uint32 unBufferSize, bool bRESERVED_MUST_BE_FALSE = false) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GenerateItems(SteamInventoryResult_t *pResultHandle, const SteamItemDef_t *pArrayItemDefs, const uint32 *punArrayQuantity, uint32 unArrayLength) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GrantPromoItems(SteamInventoryResult_t *pResultHandle) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool AddPromoItem(SteamInventoryResult_t *pResultHandle, SteamItemDef_t itemDef) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool AddPromoItems(SteamInventoryResult_t *pResultHandle, const SteamItemDef_t *pArrayItemDefs, uint32 unArrayLength) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool ConsumeItem(SteamInventoryResult_t *pResultHandle, SteamItemInstanceID_t itemConsume, uint32 unQuantity) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool ExchangeItems(SteamInventoryResult_t *pResultHandle, const SteamItemDef_t *pArrayGenerate, const uint32 *punArrayGenerateQuantity, uint32 unArrayGenerateLength, const SteamItemInstanceID_t *pArrayDestroy, const uint32 *punArrayDestroyQuantity, uint32 unArrayDestroyLength) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool TransferItemQuantity(SteamInventoryResult_t *pResultHandle, SteamItemInstanceID_t itemIdSource, uint32 unQuantity, SteamItemInstanceID_t itemIdDest) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual void SendItemDropHeartbeat() {
		FakeSteamAPI_LogFuncBeingCalled();
	}
	virtual bool TriggerItemDrop(SteamInventoryResult_t *pResultHandle, SteamItemDef_t dropListDefinition) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool TradeItems(SteamInventoryResult_t *pResultHandle, CSteamID steamIDTradePartner, const SteamItemInstanceID_t *pArrayGive, const uint32 *pArrayGiveQuantity, uint32 nArrayGiveLength, const SteamItemInstanceID_t *pArrayGet, const uint32 *pArrayGetQuantity, uint32 nArrayGetLength) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool LoadItemDefinitions() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetItemDefinitionIDs(SteamItemDef_t *pItemDefIDs, uint32 *punItemDefIDsArraySize) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetItemDefinitionProperty(SteamItemDef_t iDefinition, const char *pchPropertyName, char *pchValueBuffer, uint32 *punValueBufferSizeOut) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t RequestEligiblePromoItemDefinitionsIDs(CSteamID steamID) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetEligiblePromoItemDefinitionIDs(CSteamID steamID, SteamItemDef_t *pItemDefIDs, uint32 *punItemDefIDsArraySize) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t StartPurchase(const SteamItemDef_t *pArrayItemDefs, const uint32 *punArrayQuantity, uint32 unArrayLength) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamAPICall_t RequestPrices() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual uint32 GetNumItemsWithPrices() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetItemsWithPrices(SteamItemDef_t *pArrayItemDefs, uint64 *pPrices, uint32 unArrayLength) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool GetItemPrice(SteamItemDef_t iDefinition, uint64 *pPrice) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual SteamInventoryUpdateHandle_t StartUpdateProperties() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool RemoveProperty(SteamInventoryUpdateHandle_t handle, SteamItemInstanceID_t nItemID, const char *pchPropertyName) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetProperty(SteamInventoryUpdateHandle_t handle, SteamItemInstanceID_t nItemID, const char *pchPropertyName, const char *pchPropertyValue) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetProperty(SteamInventoryUpdateHandle_t handle, SteamItemInstanceID_t nItemID, const char *pchPropertyName, bool bValue) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetProperty(SteamInventoryUpdateHandle_t handle, SteamItemInstanceID_t nItemID, const char *pchPropertyName, int64 nValue) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SetProperty(SteamInventoryUpdateHandle_t handle, SteamItemInstanceID_t nItemID, const char *pchPropertyName, float flValue) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool SubmitUpdateProperties(SteamInventoryUpdateHandle_t handle, SteamInventoryResult_t * pResultHandle) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
DerivedInterfaceGenerator_End(ISteamInventory);

DerivedInterfaceGenerator_Begin(ISteamVideo);
	virtual void GetVideoURL(AppId_t unVideoAppID) VFunction_STUB
	virtual bool IsBroadcasting(int *pnNumViewers) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual void GetOPFSettings(AppId_t unVideoAppID) VFunction_STUB
	virtual bool GetOPFStringForApp(AppId_t unVideoAppID, char *pchBuffer, int32 *pnBufferSize) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
DerivedInterfaceGenerator_End(ISteamVideo);

DerivedInterfaceGenerator_Begin(ISteamParentalSettings);
	virtual bool BIsParentalLockEnabled() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool BIsParentalLockLocked() {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool BIsAppBlocked(AppId_t nAppID) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool BIsAppInBlockList(AppId_t nAppID) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool BIsFeatureBlocked(EParentalFeature eFeature) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
	virtual bool BIsFeatureInBlockList(EParentalFeature eFeature) {
		FakeSteamAPI_LogFuncBeingCalled();
		return 0;
	}
DerivedInterfaceGenerator_End(ISteamParentalSettings);

static CSteamAPIContextStruct sSteamContext;

bool FakeSteamAPI_InitializeContext(void) {
	sSteamContext.m_pSteamClient = &ISteamClientDerived::GetInstance();
	sSteamContext.m_pSteamUser = &ISteamUserDerived::GetInstance();
	sSteamContext.m_pSteamFriends = &ISteamFriendsDerived::GetInstance();
	sSteamContext.m_pSteamUtils = &ISteamUtilsDerived::GetInstance();
	sSteamContext.m_pSteamMatchmaking = &ISteamMatchmakingDerived::GetInstance();
	sSteamContext.m_pSteamUserStats = &ISteamUserStatsDerived::GetInstance();
	sSteamContext.m_pSteamApps = &ISteamAppsDerived::GetInstance();
	sSteamContext.m_pSteamMatchmakingServers = &ISteamMatchmakingServersDerived::GetInstance();
	sSteamContext.m_pSteamNetworking = &ISteamNetworkingDerived::GetInstance();
	sSteamContext.m_pSteamRemoteStorage = &ISteamRemoteStorageDerived::GetInstance();
	sSteamContext.m_pSteamHTTP = &ISteamHTTPDerived::GetInstance();
	sSteamContext.m_pSteamScreenshots = &ISteamScreenshotsDerived::GetInstance();
	sSteamContext.m_pSteamMusic = &ISteamMusicDerived::GetInstance();
	sSteamContext.m_pController = &ISteamControllerDerived::GetInstance();
	sSteamContext.m_pSteamUGC = &ISteamUGCDerived::GetInstance();
	sSteamContext.m_pSteamAppList = &ISteamAppListDerived::GetInstance();
	sSteamContext.m_pSteamMusic = &ISteamMusicDerived::GetInstance();
	sSteamContext.m_pSteamMusicRemote = &ISteamMusicRemoteDerived::GetInstance();
	sSteamContext.m_pSteamHTMLSurface = &ISteamHTMLSurfaceDerived::GetInstance();
	sSteamContext.m_pSteamInventory = &ISteamInventoryDerived::GetInstance();
	sSteamContext.m_pSteamVideo = &ISteamVideoDerived::GetInstance();
	sSteamContext.m_pSteamParentalSettings = &ISteamParentalSettingsDerived::GetInstance();

	return true;
}

CSteamAPIContext& FakeSteamAPI_GetContextInstance(void) {
	static bool bInitialized = false;
	if (!bInitialized)
		FakeSteamAPI_InitializeContext();
	return *(CSteamAPIContext*)&sSteamContext;
}
