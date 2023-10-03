#ifndef C_ASW_PLAYER_H
#define C_ASW_PLAYER_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_shareddefs.h"
#include "asw_player_shared.h"
#include "asw_playeranimstate.h"
#include "c_baseplayer.h"
#include "c_asw_playerlocaldata.h"
#include "baseparticleentity.h"
#include "asw_info_message_shared.h"
#include "steam/steam_api.h"
#include <vgui_controls/PHandle.h>

class C_ASW_Game_Resource;

namespace vgui
{
	class Frame;
};

class C_ASW_Marine;
class IASW_Client_Vehicle;
class C_ASW_PointCamera;
class C_ASW_Voting_Missions;
class C_ASW_Marine_Resource;
class C_EnvAmbientLight;
class CASW_Map_Builder;

class C_ASW_Player : public C_BasePlayer, public IASWPlayerAnimStateHelpers
{
public:
	DECLARE_CLASS( C_ASW_Player, C_BasePlayer );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_INTERPOLATION();

	C_ASW_Player();
	virtual ~C_ASW_Player();
	virtual void Precache();
	virtual void UpdateOnRemove();

	static C_ASW_Player* GetLocalASWPlayer( int nSlot = -1 );

	virtual const QAngle& GetRenderAngles();
	virtual void UpdateClientSideAnimation();
	virtual void PostDataUpdate( DataUpdateType_t updateType );
	void DriveMarineMovement( CUserCmd *ucmd, IMoveHelper *moveHelper );
	virtual void AvoidPhysicsProps( CUserCmd *pCmd );
	virtual Vector EyePosition(void);
	virtual Vector	EarPosition( void );
	Vector m_vecLastMarineOrigin; 	// remember last known position to keep camera there when gibbing	
	virtual void SmoothCameraZ(Vector &CameraPos);		// smooth the Z motion of the camera;
	virtual void SmoothCameraYaw(float &yaw);	// smooth the yaw angle of the camera when controlling a vehicle
	virtual bool SmoothMarineChangeCamera(Vector &CameraPos);	// slide the camera from one marine to another when we change
	virtual float ASW_ClampYaw( float yawSpeedPerSec, float current, float target, float time );		
	void SmoothAimingFloorZ(float &FloorZ);
	virtual const QAngle& GetPunchAngle() { return vec3_angle; }
	virtual fogparams_t		*GetFogParams( void );
	virtual bool ShouldRegenerateOriginFromCellBits() const;

	float m_fLastFloorZ;
	CHandle<C_ASW_Marine> m_hLastMarine;
	CHandle<C_ASW_Marine> m_hLastTurningMarine;	
	CHandle<C_ASW_Marine> m_hLastAimingFloorZMarine;
	float m_fLastVehicleYaw;
	bool m_bLastInVehicle;
	float m_fMarineChangeSmooth;
	Vector m_vecMarineChangeCameraPos;

	// Camera data.
	int m_nLastCameraFrame;
	Vector m_vecLastCameraPosition;
	QAngle m_angLastCamera;
	
	// briefing
	void LaunchBriefingFrame();
	void LaunchCampaignFrame();
	void LaunchOutroFrame();
	void LaunchMissionCompleteFrame(bool bSuccess);
	void CloseBriefingFrame();
	
	// contacting the server for briefing stuff
	void StartReady();	// player is ready to start the mission
	virtual void SendRosterSelectCommand( const char *command, int index, int nPreferredSlot=-1 );
	virtual void RosterSelectMarine(int index);
	virtual void RosterSelectSingleMarine(int index);
	virtual void RosterSelectMarineForSlot( int index, int nPreferredSlot );
	virtual void RosterDeselectMarine(int iProfileIndex);	
	virtual void LoadoutSelectEquip(int iMarineIndex, int iInvSlot, int iEquipIndex);
	virtual void LoadoutSendStored(C_ASW_Marine_Resource *pMR);
	virtual void RosterSpendSkillPoint(int iProfileIndex, int iSkillIndex);

	// campaign
	virtual void CampaignLaunchMission(int iTargetMission);
	virtual void NextCampaignMission(int iTargetMission);
	virtual void CampaignSaveAndShow();
	void LaunchCredits( vgui::Panel *pParent = NULL );
	void LaunchCainMail();	// cain mail message to show in outro

	virtual void PreThink( void );
	virtual void ClientThink();
	virtual void OnDataChanged( DataUpdateType_t updateType );
	void MarinePerformClientSideObstacleAvoidance( float flFrameTime, CUserCmd *pCmd );
	void MarineStopMoveIfBlocked(float flFrameTime, CUserCmd *pCmd, C_ASW_Marine* pMarine);
	virtual bool	CreateMove( float flInputSampleTime, CUserCmd *pCmd );

	virtual bool				ShouldDraw();

	virtual	C_BaseCombatCharacter *ActivePlayerCombatCharacter( void );

	// inventory stuff
	void ActivateInventoryItem(int slot);
	bool ShouldAutoReload();
	virtual C_BaseCombatWeapon *GetActiveWeapon( void ) const;
	virtual C_BaseCombatWeapon *GetWeapon( int i ) const;
	virtual bool Weapon_CanUse( CBaseCombatWeapon *pWeapon );
	virtual CBaseCombatWeapon*	Weapon_OwnsThisType( const char *pszWeapon, int iSubType = 0 ) const;  // True if already owns a weapon of this class
	virtual int Weapon_GetSlot( const char *pszWeapon, int iSubType = 0 ) const;  // Returns -1 if they don't have one
	virtual void				UpdateClientData( void ) { }		// asw player doesn't carry weapons, so this is not needed

	virtual int GetHealth() const;
	virtual int GetMaxHealth() const;

// Called by shared code.
public:
	void ItemPostFrame();

	void DoAnimationEvent( PlayerAnimEvent_t event );

	IPlayerAnimState *m_PlayerAnimState;
	
	virtual const QAngle &EyeAngles();		// Direction of eyes
	virtual const QAngle& EyeAnglesWithCursorRoll();
	QAngle	m_angEyeAngles;
	CInterpolatedVar< QAngle >	m_iv_angEyeAngles;
	CNetworkHandle( C_ASW_Marine, m_hMarine );    // our currently controlled marine
	CNetworkHandle( C_ASW_Marine, m_hSpectatingMarine );    // the marine we're spectating when dead
	const Vector& GetCrosshairTracePos() { return m_vecCrosshairTracePos; }
	void SetCrosshairTracePos( const Vector &vecPos ) { m_vecCrosshairTracePos = vecPos; }
	Vector m_vecCrosshairTracePos;			// the world location directly beneath the player's crosshair

	bool IsSniperScopeActive();

	C_ASW_Marine* GetMarine();
	C_ASW_Marine* GetMarine() const;
	C_ASW_Marine* GetSpectatingMarine();
	bool HasLiveMarines();
	virtual bool IsAlive( void );

	// anim state helper
	CBaseCombatWeapon* ASWAnim_GetActiveWeapon();
	bool ASWAnim_CanMove();

	virtual Vector GetAutoaimVectorForMarine( C_ASW_Marine* marine, float flDelta, float flNearMissDelta  );

	// searches for nearby entities that we can use (pickups, buttons, etc)
	virtual void PlayerUse();
	virtual void FindUseEntities();
	void SortUsePair( CBaseEntity **pEnt1, CBaseEntity **pEnt2, int *pnFirstPriority, int *pnSecondPriority );
	int GetUsePriority(CBaseEntity* pEnt);
	//void SendUseIconMessage(int UseEntityIndex, int UseIconType);	// send a client cmd to the server
	CBaseEntity* GetUseEntity(int i) { return m_hUseEntities[i].Get(); }
	virtual C_BaseEntity* GetUseEntity( void ) const;
	virtual C_BaseEntity* GetPotentialUseEntity( void ) const;
	int GetNumUseEntities() { return m_iUseEntities; }
	EHANDLE m_hUseEntities[ ASW_PLAYER_MAX_USE_ENTS ];
	int m_iUseEntities;
	CNetworkVar( float, m_flUseKeyDownTime );
	CNetworkVar( EHANDLE, m_hUseKeyDownEnt );

	// the HUD fills in these arrays based on the use entities above
	EHANDLE UseIconTarget[3];

	void ASWSelectWeapon(C_BaseCombatWeapon* pWeapon, int subtype);	// for switching weapons on the current marine

	// viewing info messages
	void ShowPreviousInfoMessage(C_ASW_Info_Message *pMessage);
	void ShowMessageLog();
	CNetworkHandle(C_ASW_Info_Message, m_pCurrentInfoMessage);	

	virtual void SetAnimation( PLAYER_ANIM playerAnim );

	virtual void OnMissionRestart();
	float GetLastRestartTime() { return m_fLastRestartTime; }
	float m_fLastRestartTime;
	virtual void RequestMissionRestart();	// sends the server a request to restart the mission

	virtual void RequestSkillUp();
	virtual void RequestSkillDown();

	virtual void SendBlipSpeech(int iMarine);
	
	void CreateStimCamera();
	C_ASW_PointCamera* GetStimCam() { return m_pStimCam; }
	C_ASW_PointCamera* m_pStimCam;
	float m_fStimYaw;
	bool m_bPlayingSingleBreathSound, m_bStartedStimMusic;
	void StopStimSound();

	virtual CBaseEntity		*GetSoundscapeListener();

	void UpdateRoomDetails();
	
	// hacking
	virtual void SelectHackOption(int iHackOption);
	virtual void StopUsing();	// stop using a computer
	virtual void SelectTumbler(int iTumblerImpulse);	// sending impulse command to flip tumblers

	C_ASW_Marine* FindMarineToHoldOrder(const Vector &pos);
	C_ASW_Marine* FindMarineToFollowOrder(const Vector &pos);
	
	CNetworkHandle(C_ASW_Marine, m_hOrderingMarine);

	// voting
	CNetworkVar(int, m_iLeaderVoteIndex);	// entindex of the player we want to be leader
	CNetworkVar(int, m_iKickVoteIndex);		// entindex of the player we want to be kicked
	CNetworkVar(int, m_iMapVoted);	// my yes/no vote status during a map vote

	// music	
	void StartStimMusic();
	void StopStimMusic(bool bInstantly = false);
	void ClearStimMusic();
	CSoundPatch		*m_pStimMusic;

	C_ASWPlayerLocalData		m_ASWLocal;

	// Walking
	void StartWalking( void );
	void StopWalking( void );
	bool IsWalking( void ) { return m_fIsWalking; }
	void HandleSpeedChanges( void );
	bool m_fIsWalking;

	float m_fMapGenerationProgress;		// how far this player is through generating their local copy of the next random map

	// entity this player is highlighting with his mouse cursor
	void SetHighlightEntity( C_BaseEntity* pEnt ) { m_hHighlightEntity = pEnt; }
	C_BaseEntity* GetHighlightEntity() const { return m_hHighlightEntity.Get(); }
	EHANDLE m_hHighlightEntity;

	// status of selecting marine/weapons in the briefing
	CNetworkVar( int, m_nChangingSlot );

private:	
	bool m_bCheckedLevel;	// have we checked the level name to see if this is a tutorial yet?
	float m_fNextThinkPushAway;
	bool m_bGuidingMarine;	// are we overriding the player's movement direction to guide him around an NPC?
	Vector m_vecGuiding;
	CNetworkHandle(C_ASW_Voting_Missions, m_hVotingMissions);		
	float	m_flStepSoundTime;

	void UpdateLocalMarineGlow();
	dlight_t* m_pLocalMarineGlow;

	// Marine avoidance.
	void AvoidMarines( CUserCmd *pCmd );

	// setting soundscape per room in randomly generated maps
	char m_szSoundscape[64];
	CountdownTimer m_roomDetailsCheckTimer;

	CHandle<C_EnvAmbientLight> m_hAmbientLight;

public:
	CSteamID GetSteamID();
	// experience
	int GetLevel();
	int GetLevelBeforeDebrief();
	int GetExperience();
	int GetExperienceBeforeDebrief() { return m_iExperienceBeforeDebrief; }
	int GetPromotion();
	void AcceptPromotion();
	int GetEarnedXP( CASW_Earned_XP_t nType ) { return m_iEarnedXP[ nType ]; }
	int GetStatNumXP( CASW_Earned_XP_t nType ) { return m_iStatNumXP[ nType ]; }
	void CalculateEarnedXP();

	void RequestExperience();				// asks Steam for your current XP
	void AwardExperience();					// award experience for this mission
	bool IsWeaponUnlocked( const char *szWeaponClass );
	int GetWeaponLevelRequirement( const char *szWeaponClass );

	const char* GetNextWeaponClassUnlocked();
	const char* GetWeaponUnlockedAtLevel( int nLevel );

	int32 m_iExperience;
	int32 m_iExperienceBeforeDebrief;
	int32 m_iPromotion;
	int32 m_iEarnedXP[ ASW_NUM_XP_TYPES ];
	int32 m_iStatNumXP[ ASW_NUM_XP_TYPES ];
	CNetworkVar( int, m_iNetworkedXP );
	CNetworkVar( int, m_iNetworkedPromotion );

	bool m_bPendingSteamStats;
	float m_flPendingSteamStatsStart;

	CUtlVector<int> m_aNonLocalPlayerAchievementsEarned;	// list of achievements earned by this non-local player

#if !defined(NO_STEAM)
	STEAM_CALLBACK( C_ASW_Player, Steam_OnUserStatsReceived, UserStatsReceived_t, m_CallbackUserStatsReceived );
	STEAM_CALLBACK( C_ASW_Player, Steam_OnUserStatsStored, UserStatsStored_t, m_CallbackUserStatsStored );
#endif

private:
	C_ASW_Player( const C_ASW_Player & );
};

inline C_ASW_Player* ToASW_Player( CBaseEntity *pEntity )
{
	if ( !pEntity )
		return NULL;
	Assert( dynamic_cast< C_ASW_Player* >( pEntity ) != NULL );
	return static_cast< C_ASW_Player* >( pEntity );
}


#endif // C_ASW_PLAYER_H
