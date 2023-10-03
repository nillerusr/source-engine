#ifndef ASW_PLAYER_H
#define ASW_PLAYER_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_shareddefs.h"
#include "asw_player_shared.h"
#include "player.h"
#include "asw_playerlocaldata.h"
#include "server_class.h"
#include "asw_playeranimstate.h"
#include "asw_game_resource.h"
#include "asw_info_message_shared.h"
#include "basemultiplayerplayer.h"
#include "steam/steam_api.h"

class CASW_Marine;
class CRagdollProp;
class CASW_Voting_Missions;

//=============================================================================
// >> Swarm player
//=============================================================================
class CASW_Player : public CBaseMultiplayerPlayer, public IASWPlayerAnimStateHelpers
{
public:
	DECLARE_CLASS( CASW_Player, CBaseMultiplayerPlayer );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CASW_Player();
	virtual ~CASW_Player();

	virtual int UpdateTransmitState();

	static CASW_Player *CreatePlayer( const char *className, edict_t *ed );
	static CASW_Player* Instance( int iEnt );

	// This passes the event to the client's and server's CPlayerAnimState.
	void DoAnimationEvent( PlayerAnimEvent_t event );

	virtual void PreThink();
	virtual void PostThink();
	virtual void Spawn();
	virtual void Precache();
	virtual void UpdateBattery( void ) { }
	void DriveMarineMovement( CUserCmd *ucmd, IMoveHelper *moveHelper );
	void PushawayThink();
	virtual void AvoidPhysicsProps( CUserCmd *pCmd );
	virtual bool ClientCommand( const CCommand &args );

	void EmitPrivateSound( const char *soundName, bool bFromMarine = false );

	// eye position is above the marine we're remote controlling
	virtual Vector EyePosition(void);
	virtual Vector	EarPosition( void );
	virtual CBaseEntity		*GetSoundscapeListener();
	Vector	m_vecLastMarineOrigin;	// remember last known position to keep camera there when gibbing
	Vector m_vecFreeCamOrigin;	// if the player is using freecam cheat to look around, he'll tell us about that position
	bool m_bUsedFreeCam;		// has the player ever sent a freecam message (if he has, we started adding the freecam's pos to PVS)

	const QAngle& EyeAngles();
	virtual const QAngle& EyeAnglesWithCursorRoll();
	const Vector& GetCrosshairTracePos() { return m_vecCrosshairTracePos; }
	void SetCrosshairTracePos( const Vector &vecPos ) { m_vecCrosshairTracePos = vecPos; }
	virtual void SetupVisibility( CBaseEntity *pViewEntity, unsigned char *pvs, int pvssize );

	virtual void  HandleSpeedChanges( void );
	virtual bool CanBeSeenBy( CAI_BaseNPC *pNPC ) { return false; } // Players are never seen by NPCs

	CNetworkHandle (CASW_Marine, m_hMarine);

	bool ShouldAutoReload() { return m_bAutoReload; }
	bool m_bAutoReload;
	
	// anim state helper
	virtual CBaseCombatWeapon* ASWAnim_GetActiveWeapon();
	virtual bool ASWAnim_CanMove();

	// spectating
	void SpectateNextMarine();
	void SetSpectatingMarine(CASW_Marine *d);
	CASW_Marine* GetSpectatingMarine();
	CNetworkHandle (CASW_Marine, m_hSpectatingMarine);
	bool m_bLastAttackButton;	// used to detect left clicks for cycling through marines
	bool m_bRequestedSpectator;	// this player requested to be a spectator since the start of a match (won't be considered for leader, campaign votes, etc.)
	float m_fLastControlledMarineTime;

	void BecomeNonSolid();
	void OnMarineCommanded( const CASW_Marine *pMarine );
	void SetMarine( CASW_Marine *pMarine );
	CASW_Marine* GetMarine();
	CASW_Marine* GetMarine() const;
	void SelectNextMarine( bool bReverse );
	bool CanSwitchToMarine( int num );
	void SwitchMarine( int num );
	void OrderMarineFace( int iMarine, float fYaw, Vector &vecOrderPos );
	void LeaveMarines();
	bool HasLiveMarines();
	virtual bool IsAlive( void );

	CNetworkHandle(CASW_Marine, m_hOrderingMarine)

	CASW_Game_Resource* GetGNI();

	// This player's Infested specific data that should only be replicated to 
	//  the player and not to other players.
	CNetworkVarEmbedded( CASWPlayerLocalData, m_ASWLocal );

	// gets the firing direction when the player is firing through a marine
	Vector GetAutoaimVectorForMarine(CASW_Marine* marine, float flDelta, float flNearMissDelta);
	QAngle MarineAutoaimDeflection( Vector &vecSrc, float flDist, float flDelta, float flNearMissDelta);
	QAngle m_angMarineAutoAim;

	void				FlashlightTurnOn( void );
	void				FlashlightTurnOff( void );

	// Marine position prediction (used by AI to compensate for player lag times)
	void	MoveMarineToPredictedPosition();
	void	RestoreMarinePosition();
	Vector	m_vecStoredPosition;

	virtual	CBaseCombatCharacter *ActivePlayerCombatCharacter( void );

	// shared code
	void ItemPostFrame();
	void ASWSelectWeapon(CBaseCombatWeapon* pWeapon, int subtype);	// for switching weapons on the current marine
	virtual bool Weapon_CanUse( CBaseCombatWeapon *pWeapon );
	virtual CBaseCombatWeapon*	Weapon_OwnsThisType( const char *pszWeapon, int iSubType = 0 ) const;  // True if already owns a weapon of this class
	virtual int Weapon_GetSlot( const char *pszWeapon, int iSubType = 0 ) const;  // Returns -1 if they don't have one

	// searches for nearby entities that we can use (pickups, buttons, etc)
	virtual void PlayerUse();
	virtual void FindUseEntities();
	void SortUsePair( CBaseEntity **pEnt1, CBaseEntity **pEnt2, int *pnFirstPriority, int *pnSecondPriority );
	int GetUsePriority(CBaseEntity* pEnt);
	void ActivateUseIcon( int iUseEntityIndex, int nHoldType );
	CBaseEntity* GetUseEntity( int i );
	int GetNumUseEntities() { return m_iUseEntities; }
	EHANDLE m_hUseEntities[ ASW_PLAYER_MAX_USE_ENTS ];
	int m_iUseEntities;
	CNetworkVar( float, m_flUseKeyDownTime );
	CNetworkVar( EHANDLE, m_hUseKeyDownEnt );

	// looking at an info panel
	void ShowInfoMessage(CASW_Info_Message* pMessage);
	CNetworkHandle (CASW_Info_Message, m_pCurrentInfoMessage);
	float m_fClearInfoMessageTime;	

	virtual void SetAnimation( PLAYER_ANIM playerAnim );

	// ragdoll blend test
	void RagdollBlendTest();
	CRagdollProp* m_pBlendRagdoll;
	float m_fBlendAmount;

	// AI perf debugging
	float m_fLastAICountTime;

	// changing name
	bool CanChangeName() { return true; }
	void ChangeName(const char *pszNewName);
	bool HasFullyJoined() { return m_bSentJoinedMessage; }
	bool m_bSentJoinedMessage;	// has this player told everyone that he's fully joined yet
		
	// voting
	int m_iKLVotesStarted;	// how many kick or leader votes this player has started, if he starts too many, flood control will be applied to the announcements
	float m_fLastKLVoteTime;	// last time we started a kick or leader vote
	CNetworkVar(int, m_iLeaderVoteIndex);	// entindex of the player we want to be leader
	CNetworkVar(int, m_iKickVoteIndex);		// entindex of the player we want to be kicked
	void VoteMissionList(int nMissionOffset, int iNumSlots);	// player wants to see a list of missions he can vote for
	void VoteCampaignMissionList(int nCampaignIndex, int nMissionOffset, int iNumSlots);		// player wants to see a list of missions within a specific campaign 
	void VoteCampaignList(int nCampaignOffset, int iNumSlots);	// player wants to see a list of new campaigns he can vote for
	void VoteSavedCampaignList(int nSaveOffset, int iNumSlots);	// player wants to see a list of saved campaign games he can vote for
	CNetworkHandle(CASW_Voting_Missions, m_hVotingMissions);	// handle to voting missions entity that will network down the list of missions/campaigns/saves
	const char* GetASWNetworkID();
	CNetworkVar( int, m_iMapVoted );	// 0 = didn't vote, 1 = "no" vote, 2 = "yes" vote

	// client stat counts (these are numbers each client stores and provides to the server on player creation, so server can decide to award medals)
	int m_iClientKills;
	int m_iClientMissionsCompleted;
	int m_iClientCampaignsCompleted;

	// char array of medals awarded (used for stats upload)
	CUtlVector<unsigned char> m_CharMedals;

	// have we notified other players that we're impatiently waiting on them?
	bool m_bPrintedWantStartMessage;
	bool m_bPrintedWantsContinueMessage;

	virtual void			CreateViewModel( int viewmodelindex = 0 ) { return; }	// ASW players don't have viewmodels
	virtual void SetPunchAngle( const QAngle &punchAngle ) { return; }				// ASW players never use punch angle

	void StartWalking( void );
	void StopWalking( void );
	bool IsWalking( void ) { return m_fIsWalking; }

	// random map generation
	CNetworkVar( float, m_fMapGenerationProgress );			// how far this player is through generating their local copy of the next random map

	virtual CBaseEntity* FindPickerEntity();

	// entity this player is highlighting with his mouse cursor
	void SetHighlightEntity( CBaseEntity* pEnt ) { m_hHighlightEntity = pEnt; }
	CBaseEntity* GetHighlightEntity() const { return m_hHighlightEntity.Get(); }
	EHANDLE m_hHighlightEntity;

	// status of selecting marine/weapons in the briefing
	CNetworkVar( int, m_nChangingSlot );

	// experience
	int GetLevel();
	int GetLevelBeforeDebrief();
	int GetExperience();
	int GetExperienceBeforeDebrief() { return m_iExperienceBeforeDebrief; }
	void SetNetworkedExperience( int iExperience ) { m_iNetworkedXP = iExperience; }
	void SetNetworkedPromotion( int iPromotion ) { m_iNetworkedPromotion = iPromotion; }
	int GetPromotion();
	int GetEarnedXP( CASW_Earned_XP_t nType ) { return m_iEarnedXP[ nType ]; }	
	void CalculateEarnedXP();

	void AwardExperience();					// calculates earned XP.  Steam stat setting actually happens on the client.
	void RequestExperience();				// asks Steam for your current XP

	int32 m_iExperience;
	int32 m_iExperienceBeforeDebrief;
	int32 m_iPromotion;
	int32 m_iEarnedXP[ ASW_NUM_XP_TYPES ];
	int32 m_iStatNumXP[ ASW_NUM_XP_TYPES ];
	CNetworkVar( int, m_iNetworkedXP );
	CNetworkVar( int, m_iNetworkedPromotion );

	bool m_bHasAwardedXP;
	bool m_bPendingSteamStats;
	float m_flPendingSteamStatsStart;
	bool m_bSentPromotedMessage;

#if !defined(NO_STEAM)
	CCallResult< CASW_Player, GSStatsReceived_t > m_CallbackGSStatsReceived;
	void Steam_OnGSStatsReceived( GSStatsReceived_t *pGSStatsReceived, bool bError );
#endif

private:

	// Copyed from EyeAngles() so we can send it to the client.
	CNetworkQAngle( m_angEyeAngles );	
	Vector m_vecCrosshairTracePos;			// the world location directly beneath the player's crosshair
	CNetworkVarForDerived( bool, m_fIsWalking );

	IPlayerAnimState *m_PlayerAnimState;
	bool m_bFirstInhabit;
};

extern void TE_MarineAnimEvent( CASW_Marine *pMarine, PlayerAnimEvent_t event );
extern void TE_MarineAnimEventExceptCommander( CASW_Marine *pMarine, PlayerAnimEvent_t event );
extern void TE_MarineAnimEventJustCommander( CASW_Marine *pMarine, PlayerAnimEvent_t event );

void OrderNearbyMarines(CASW_Player *pPlayer, ASW_Orders NewOrders, bool bAcknowledge = true );

inline CASW_Player *ToASW_Player( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;

#ifdef _DEBUG
	Assert( dynamic_cast<CASW_Player*>( pEntity ) != 0 );
#endif
	return static_cast< CASW_Player* >( pEntity );
}


#endif	// ASW_PLAYER_H
