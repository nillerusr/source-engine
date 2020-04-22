//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_PLAYER_H
#define TF_PLAYER_H
#ifdef _WIN32
#pragma once
#endif

#include "player.h"
#include "tf_shareddefs.h"
#include "tf_playerlocaldata.h"
#include "tf_playerclass.h"
#include "iscorer.h"
#include "tf_playeranimstate.h"
#include "vphysics/player_controller.h"

#define MENU_STRING_BUFFER_SIZE		512
#define MENU_MSG_TEXTCHUNK_SIZE		50
#define MENU_UPDATETIME				2.0

class CVehicleTeleportStation;
class CPlayerClass;
class CThrownGrenade;
class CFlybyPoint;
class CMenu;
class CControlZone;
class CTechnologyTree;
class CTFTeam;
class CWeaponBuilder;
class CWeaponCombatShield;
class COrder;
class CBaseTFCombatWeapon;
class CResourceZone;
class CRagdollShadow;
class CBaseObject;
class CBasePredictedWeapon;
class IVehicle;
enum ResupplyReason_t;

// If disguised player fires weapon, suppress disguise for this long
#define DISGUISE_FIRE_SUPPRESSTIME		4.0f
// If there are enemies within this radius, then 
//  firing your weapon will cause you to lose your
//  disguise for DISGUISE_FIRE_SUPPRESSTIME
#define DISGUISE_SUPPRESSION_RADIUS		1024.0f
// Can only initiate disguise if no enemies within this radius
#define DISGUISE_START_RADIUS			512.0f

// Adrenalin
#define ADRENALIN_DAMAGE_REDUCTION		0.5		// Damage reduction during adrenalin
#define ADRENALIN_SPEED_INCREASE		1.2		// Movement speed increase during adrenalin
#define ADRENALIN_RAMPAGE_EXTEND		5.0		// Time gained from killing an enemy during rampage

// ID
#define	MAX_ID_RANGE					2048

// Setup for class specific touch functions.
#define SetClassTouch( _player, a ) _player->SetTouch( CBaseTFPlayer::ClassTouch ); _player->m_pfnClassTouch = static_cast<void (CPlayerClass::*)(CBaseEntity*)>(a)

//=====================================================================
// TF PLAYER
//=====================================================================
class CBaseTFPlayer : public CBasePlayer, public IScorer
{
	typedef CBasePlayer BaseClass;
public:
	DECLARE_CLASS( CBaseTFPlayer, CBasePlayer );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
	DECLARE_PREDICTABLE();

	CBaseTFPlayer();
	~CBaseTFPlayer();

	// For updating hud tech tree
	struct tfplayertech_t
	{
		int	m_nAvailable;
		int m_nUserCount;
		int m_nResourceLevel;
	};

	// Helper to get a CBaseTFPlayer by its entity index.
	static CBaseTFPlayer* Instance( int iEnt )
	{
		return dynamic_cast< CBaseTFPlayer* >( CBaseEntity::Instance( INDEXENT( iEnt ) ) );
	}

	bool		IsHidden() const;
	void		SetHidden( bool bHidden );

	// Class specific touch functions.
	void		ClassTouch( CBaseEntity *pTouched );

	// This normally wouldn't go in here but we have to access CAllPlayerClasses and that is private.
	const char* GetClassModelString( int iClass, int iTeam );

	// The player class touch function
	void (CPlayerClass ::*m_pfnClassTouch)( CBaseEntity *pOther );

	// Override CBaseAnimatingOverlay to zero out m_dweights by default
	virtual int AddGesture( Activity activity, bool autokill = true );

	// Hacky shield stuff for now
	void		RemoveShieldOverlays( void );
	int			RemoveShieldOverlaysExcept( Activity activity, bool addifnotpresent = true );
	Activity	ShieldTranslateActivity( Activity activity );
	void		PickShieldAnimation( Activity& activity, int& overlayindex, 
					bool moving, bool ducked, 
					Activity overlay, Activity crouch, Activity normal, 
					bool autokill = true, float blendin = 0.0f, float blendout = 0.0f );

	// True if player was moving last time checked (used to switch between shield full body and overlay anims )
	bool		m_bWasMoving;

	virtual void	UpdateOnRemove( void );

	static CBaseTFPlayer *CreatePlayer( const char *className, edict_t *ed )
	{
		CBaseTFPlayer::s_PlayerEdict = ed;
		return (CBaseTFPlayer*)CreateEntityByName( className );
	}

	virtual int		UpdateTransmitState();
	virtual int		ShouldTransmit( const CCheckTransmitInfo *pInfo );
	// Networking is about to update this player, let it override and specify it's own pvs
	virtual void	SetupVisibility( CBaseEntity *pViewEntity, unsigned char *pvs, int pvssize );

	virtual void	Spawn( void );
	virtual void	InitialSpawn( void );
	virtual void	ForceRespawn( void );
	virtual void	UpdateClientData( void );

	virtual void	ForceClientDllUpdate( void );  // Forces all client .dll specific data to be resent to client.

	virtual void	Precache( void );

	virtual void	InitHUD( void );
	virtual void	SelectItem( const char *pstr, int iSubType = 0 );

	virtual void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int		ObjectCaps( void );

	// Override
	virtual bool	CanSpeak( void );

	void			ResetViewOffset( void );

	// Input handlers
	void InputRespawn( inputdata_t &inputdata );

	// Team Handling
	CTFTeam			*GetTFTeam( void ) { return (CTFTeam*)GetTeam(); };
	CTechnologyTree *GetTechTree( void );
	virtual void	ChangeTeam( int iTeamNum ) OVERRIDE;
	void			PlacePlayerInTeam( void );

	// Class Handling
	int		PlayerClass( void );
	bool	IsClass( TFClass iClass );
	bool	IsSameClass( CBaseTFPlayer* pPlayer ) { Assert(pPlayer); return IsClass( (TFClass)pPlayer->PlayerClass() ); }

	void	ChangeClass( TFClass iClass );
	bool	IsClassAvailable( TFClass iClass );
	void	SetPlayerModel( void );
	CPlayerClass *GetPlayerClass();
	void	ClearPlayerClass( void );
	void	SetPlayerClass( TFClass iClass );

	int		ClassCostAdjustment( ResupplyBuyType_t nType );

	// Menu Handling
	void	MenuDisplay( void );
	bool	MenuInput( int iInput );
	void	MenuReset( void );

	// Standard functions
	virtual void ItemPostFrame();

	virtual void Jump( void );

	void	PostThink( void );
	void	PreThink( void );
	void	PlayerRespawn( void );
	CBaseEntity *EntSelectSpawnPoint( void );

	// Death
	virtual void	DeathSound( const CTakeDamageInfo &info );
	virtual void	PainSound( const CTakeDamageInfo &info );
	virtual void	Event_Killed( const CTakeDamageInfo &info );
	void			TFPlayerDeathThink( void );

	virtual void	CheatImpulseCommands( int iImpulse );
	virtual bool	ClientCommand( const CCommand &args );
	virtual void	SetAnimation( PLAYER_ANIM playerAnim );
	
	// Combat
	void	RecalculateSpeed( void );
	void	KilledPlayer( CBaseTFPlayer *pVictim );
	
	int		TakeHealth( float flHealth, int bitsDamageType );

	// Combat
	virtual int		OnTakeDamage( const CTakeDamageInfo &info );
	virtual void	TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	void			ShowPersonalShieldEffect( const Vector &vOffsetFromEnt, const Vector &vIncomingDirection, float flDamage );

	// Objects
	void	AddObject( CBaseObject *pObject );
	void	RemoveObject( CBaseObject *pObject );
	int		CanBuild( int iObjectType );
	int		GetNumObjects( int iObjectType );
	int		GetObjectCount( );
	CBaseObject *GetObject( int iObjectIndex );
	bool	IsBuilding( void );
	void	OwnedObjectDestroyed( CBaseObject *pObject );
	void	StopPlacement( void );
	int		StartedBuildingObject( int iObjectType );
	void	StoppedBuilding( int iObjectType );
	void	FinishedObject( CBaseObject *pObject );
	void	SetWeaponBuilder( CWeaponBuilder *pBuilder );
	CWeaponBuilder *GetWeaponBuilder( void );
	CWeaponCombatShield *GetCombatShield( void );
	void	OwnedObjectChangeTeam( CBaseObject *pObject, CBaseTFPlayer *pNewOwner );
	void	RemoveAllObjects( bool bForceAll = false, int iClass = 0, bool bReturnResources = false );
	
	int		NumPumpsOnResourceZone( CResourceZone *pZone );

	// Deployment
	void	StartDeploying( void );
	void	StartUnDeploying( void );
	void	CheckDeployFinish( void );
	void	FinishDeploying( void );
	void	FinishUnDeploying( void );
	bool	IsDeployed( void );
	bool	IsDeploying( void );
	bool	IsUnDeploying( void );

	// Movement prevention
	bool	CantMove( void );
	void	SetCantMove( bool bCantMove );

	bool	HasNamedTechnology( const char *name );
	void	SetPreferredTechnology( CTechnologyTree *pTechnologyTree, int iTechIndex );
	int		GetPreferredTechnology( void );
	tfplayertech_t &AvailableTech( int i ) { return m_rgClientTechAvail[i]; }

	// Powerups
	virtual void	PowerupStart( int iPowerup, float flAmount, CBaseEntity *pAttacker, CDamageModifier *pDamageModifier );
	virtual void	PowerupEnd( int iPowerup );
	virtual	float	PowerupDuration( int iPowerup, float flTime );

	void	SetRampage( bool bRampage );
	float	GetDefaultAnimSpeed( void );

	// Knockdown
	void	KnockDownPlayer( const Vector& sourceDir, float velocity, float duration );
	bool	IsKnockedDown( void );

	// Resources
	void	AddBankResources( int iAmount );
	void	RemoveBankResources( int iAmount, bool bSpent = true );
	void	DonateResources( CBaseTFPlayer *pTarget, int pCount );
	int 	GetBankResources( void );
	void	SetBankResources( int iAmount );

	// Check for a collision....
	bool	IsHittingShield( const Vector &vecVelocity, float *flDamage );

	// Combat prototyping
	bool	IsBlocking( void ) const { return m_bIsBlocking; }
	bool	IsParrying( void ) const { return m_bIsParrying; }
	void	SetBlocking( bool bBlocking ) { m_bIsBlocking = bBlocking; }
	void	SetParrying( bool bParrying ) { m_bIsParrying = bParrying; }

	// Thermal Vision
	bool	IsUsingThermalVision( void );
	void	SetUsingThermalVision( bool thermal );

	CTFPlayerLocalData *GetLocalData() { return &m_TFLocal; }

	// Acts
	void	CleanupOnActStart( void );

	// Resource chunks
	bool	AddResourceChunks( int iChunks, bool bProcessed );
	void	RemoveResourceChunks( int iChunks, bool bProcessed );
	int		GetTotalResourceChunks( void );
	int		GetResourceChunkCount( bool bProcessed );

	void	SetGagged( bool gag );

	// Control zone
	CControlZone* GetCurrentZone( ) { return m_pCurrentZone; }
	void SetCurrentZone( CControlZone* pZone ) { m_pCurrentZone = pZone; }

	// Camouflage
	float	GetCamouflageAmount( void ) { return m_flCamouflageAmount; };
	bool	IsCamouflaged( void );
	void	SetCamouflaged( int percentage, float changerate );
	void	ClearCamouflage( void );

	float	LastAttackTime() const { return m_flLastAttackTime; }
	void	SetLastAttackTime( float flTime ) { m_flLastAttackTime = flTime; }

	bool	ResupplyAmmo( float flFraction, ResupplyReason_t reason );

	// The last time we were damaged by an enemy. 
	double	LastTimeDamagedByEnemy() const { return m_flLastTimeDamagedByEnemy; }

	// Orders
	COrder*	GetOrder( void ) { return m_hSelectedOrder; }
	void	SetOrder( COrder *pOrder );

	// Count resource zone orders.
	int		GetNumResourceZoneOrders();
	
	// Reinforcement
	bool	IsReadyToReinforce( void );
	void	Reinforce( void );

	void	ShowTacticalView( bool bTactical );

	void	SetRespawnStation( CBaseEntity *pStation );

	// ID
	void	SetIDEnt( CBaseEntity *pEntity );

	// Health boosts
	void	TakeHealthBoost( int iHealthBoost, int iTarget, int iDuration );

	// Vehicle
	bool	CanGetInVehicle( void );

	CVehicleTeleportStation* GetSelectedMCV() const;
	void SetSelectedMCV( CVehicleTeleportStation *pMCV );

	// Weapon handling
	virtual bool Weapon_ShouldSetLast( CBaseCombatWeapon *pOldWeapon, CBaseCombatWeapon *pNewWeapon );
	virtual bool Weapon_ShouldSelectItem( CBaseCombatWeapon *pWeapon );
	virtual CBaseCombatWeapon  *GetLastWeaponBeforeObject( void ) { return m_hLastWeaponBeforeObject; }

	// Sapper attaching
	bool	IsAttachingSapper( void );
	float	GetSapperAttachmentTime( void );
	void	StartAttachingSapper( CBaseObject *pObject, CGrenadeObjectSapper *pSapper );
	void	CleanupAfterAttaching( void );
	void	StopAttaching( void );
	void	FinishAttaching( void );
	void	CheckSapperAttaching( void );

// IScorer
public:
	// Return the entity that should receive the score
	virtual CBasePlayer *GetScorer( void );
	// Return the entity that should get assistance credit
	virtual CBasePlayer *GetAssistant( void );

public:
	// FIXME: Make these private

	// Menu-related goodies
	CMenu	*m_pCurrentMenu;
	char	m_MenuStringBuffer[MENU_STRING_BUFFER_SIZE];
	int		m_MenuSelectionBuffer;
	float	m_MenuRefreshTime;
	float	m_MenuUpdateTime;

	bool	m_bBuffHealthBoost;

	// Object sapper placement handling
	float							m_flSapperAttachmentFinishTime;
	float							m_flSapperAttachmentStartTime;
	CHandle< CGrenadeObjectSapper >	m_hSapper;
	CHandle< CBaseObject >			m_hSappedObject;

private:
	// Medic Buffs
	void	CheckBuffs( void );
	void	RemoveHealthBoost( void );

	// vehicles
	void	OnVehicleStart();
	void	OnVehicleEnd( Vector & );

	bool	IsInTacticalView( void ) const;

	// Get rid of resource zone orders.
	void	KillResourceZoneOrders();

	// Knockdowns
	void	ResetKnockdown( void );
	void	CheckKnockdown( void );

	// Gagging
	bool	IsGagged( void );

	void	DropAllResourceChunks( void );

	// Thinking
	void	CheckCamouflage( void );

	// Rampage
	bool		IsInRampage( void );

	// Vertification
	inline bool HasClass( void ) { return GetPlayerClass() != NULL; }

	// Respawn stations
	CBaseEntity *GetInitialSpawnPoint( void );

	// Go ragdoll, create shadow object, etc.
	bool		BecomeRagdollOnClient( const Vector &force );
	// Remove ragdoll
	bool		ClearClientRagdoll( bool moveplayertofinalspot );
	// Move origin in sync with shadow object if ragdolling
	void		FollowClientRagdoll( void );
	bool		CheckRagdollToStand( trace_t &trace );
	bool		IsClientRagdoll() const { return m_hRagdollShadow.Get() != 0; }

	// Deployed?
	bool		IsDeployed() const { return m_bDeployed; }


	void		StoreCycle( void );
	float		RetrieveCycle( void );

	bool		ShouldMatchCycles( Activity newActivity, Activity currentActivity );

	// TF Player Physics Shadow
	void		InitVCollision( void );

	void		ApplyDamageForce( const CTakeDamageInfo &info, int nDamageToDo );

	// Movement.
	Vector		m_vecPosDelta;

	enum { MOMENTUM_MAXSIZE = 10 };
	float		m_aMomentum[MOMENTUM_MAXSIZE];
	int			m_iMomentumHead;

	// Spawn spot
	CNetworkHandle( CBaseEntity, m_hSpawnPoint );

	// This player's TF2 specific data that should only be replicated to 
	//  the player and not to other players.
	CNetworkVarEmbedded( CTFPlayerLocalData, m_TFLocal );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_iMaxHealth ); // Make sure this ent is marked as changed when m_iMaxHealth changes.

	CHandle< CWeaponBuilder > m_hWeaponBuilder;
	CHandle< CWeaponCombatShield > m_hWeaponCombatShield;
	float			m_flKnockdownEndTime;

	// Weapon used before switching to an object placement
	CHandle<CBaseCombatWeapon>	m_hLastWeaponBeforeObject;

	CNetworkQAngle( m_vecDeployedAngles );

	// Tracks when medics are boosting the damage this guy applies.
	int		m_nMedicDamageBoosts;

	CNetworkVar( int, m_TFPlayerFlags );	// Combination of TF_PLAYER_ flags.

	bool	m_bCantMove;

	bool	m_bGagged;

	bool	m_bFirstTeamSpawn;	// When true, the player's joined the server but not picked a team for the first time

	float	m_flLastAttackTime;

	// Camouflage
	CNetworkVar( float, m_flCamouflageAmount );
	
	// Camouflage state machine
	float	m_flGoalCamouflageAmount;
	float	m_flGoalCamouflageChangeRate;

	// State information
	bool	m_bSwitchingView;

	// Zone the player's currently in
	CControlZone	*m_pCurrentZone;
	CNetworkVar( int, m_iCurrentZoneState );

	// Accuracy
	bool	m_bSnapAccuracy;
	float	m_flAccuracy;
	float	m_flTargetAccuracy;
	float	m_flLastRicochetNearby;
	float	m_flNumberOfRicochets;
	float	m_flLastExplosionNearby;

	// Buff states
	int		m_iHealthBoostTarget;
	float	m_flHealthBoostDecrement;
	float	m_flHealthBoostTime;

	EHANDLE	m_hNextPlayerToUpdateOnRadar;

	CNetworkHandle( CBaseEntity, m_hSelectedMCV );

	// Orders
	CHandle< COrder >	m_hSelectedOrder;

	// Tech spending vote preference
	int		m_nPreferredTechnology;

	// The last time we were damaged by an enemy. 
	double	m_flLastTimeDamagedByEnemy;

	// Menu Handling
	float   m_MenuDisplayTime;

	// Rampage
	bool	m_bRampage;				// Do I get adrenalin extension for killing enemies?

	// Handheld shield
	CNetworkVar( bool, m_bIsBlocking );
	bool	m_bIsParrying;

	float	m_flTimeOfDeath;

	EHANDLE	m_hLastBoostEntity;		// Last entity that gave me a power boost

	// Deployment
	CNetworkVar( bool, m_bDeployed );
	CNetworkVar( bool, m_bDeploying );
	CNetworkVar( bool, m_bUnDeploying );
	float	m_flFinishedDeploying;
	
	CNetworkVar( int, m_iPlayerClass );
	CAllPlayerClasses			m_PlayerClasses;

	// This times how long each class is active.
	CFastTimer					m_Timer;

	tfplayertech_t	m_rgClientTechAvail[ MAX_TECHNOLOGIES ];

	CHandle< CRagdollShadow >	m_hRagdollShadow;
	Vector						m_vecLastGoodRagdollPos;

	// Last reinforcment second counter for player ( so we don't spam the player 
	//  with text every frame )
	int							m_iLastSecondsToGo;

	CPlayerAnimState			m_PlayerAnimState;

	// Fractional boost amount
	float		m_flFractionalBoost;

	float			m_flStoredCycle;


	// Are we under attack?
	CNetworkVar( bool, m_bUnderAttack );

	friend void* SendProxy_RideVehicle( const void *pStruct, const void *pData, CSendProxyRecipients *pRecipients, int objectID );
	friend class CTFPlayerMove;
};

inline CBaseTFPlayer *ToBaseTFPlayer( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;

	return dynamic_cast<CBaseTFPlayer *>( pEntity );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CPhysicsPlayerCallback : public IPhysicsPlayerControllerEvent
{
public:

	int ShouldMoveTo( IPhysicsObject *pObject, const Vector &position )
	{
		CBaseTFPlayer *pPlayer = ( CBaseTFPlayer* )pObject->GetGameData();
		if ( pPlayer->TouchedPhysics() )
		{
			return 0;
		}
		return 1;
	}
};

extern CPhysicsPlayerCallback playerCallback;

#endif // TF_PLAYER_H
