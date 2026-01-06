//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Player for HL2.
//
// $NoKeywords: $
//=============================================================================//

#ifndef HL2_PLAYER_H
#define HL2_PLAYER_H
#pragma once


#include "player.h"
#include "hl2_playerlocaldata.h"
#include "simtimer.h"
#include "soundenvelope.h"
#ifdef EZ
#include "ai_basenpc.h"
#include "ai_condition.h"
#include "ai_npcstate.h"
#include "ai_squad.h"
#endif

// In HL2MP we need to inherit from  BaseMultiplayerPlayer!
#if defined ( HL2MP )
#include "basemultiplayerplayer.h"
#elif defined ( MAPBASE )
#include "mapbase/singleplayer_animstate.h"
#endif

class CAI_Squad;
class CPropCombineBall;

extern int TrainSpeed(int iSpeed, int iMax);
extern void CopyToBodyQue( CBaseAnimating *pCorpse );

#define ARMOR_DECAY_TIME 3.5f

enum HL2PlayerPhysFlag_e
{
	// 1 -- 5 are used by enum PlayerPhysFlag_e in player.h

	PFLAG_ONBARNACLE	= ( 1<<6 )		// player is hangning from the barnalce
};

class IPhysicsPlayerController;
class CLogicPlayerProxy;

struct commandgoal_t
{
	Vector		m_vecGoalLocation;
	CBaseEntity	*m_pGoalEntity;
};

// Time between checks to determine whether NPCs are illuminated by the flashlight
#define FLASHLIGHT_NPC_CHECK_INTERVAL	0.4

//----------------------------------------------------
// Definitions for weapon slots
//----------------------------------------------------
#define	WEAPON_MELEE_SLOT			0
#define	WEAPON_SECONDARY_SLOT		1
#define	WEAPON_PRIMARY_SLOT			2
#define	WEAPON_EXPLOSIVE_SLOT		3
#define	WEAPON_TOOL_SLOT			4

//=============================================================================
//=============================================================================
class CSuitPowerDevice
{
public:
	CSuitPowerDevice( int bitsID, float flDrainRate ) { m_bitsDeviceID = bitsID; m_flDrainRate = flDrainRate; }
private:
	int		m_bitsDeviceID;	// tells what the device is. DEVICE_SPRINT, DEVICE_FLASHLIGHT, etc. BITMASK!!!!!
	float	m_flDrainRate;	// how quickly does this device deplete suit power? ( percent per second )

public:
	int		GetDeviceID( void ) const { return m_bitsDeviceID; }
	float	GetDeviceDrainRate( void ) const
	{	
		if( g_pGameRules->GetSkillLevel() == SKILL_EASY && hl2_episodic.GetBool() && !(GetDeviceID()&bits_SUIT_DEVICE_SPRINT) )
			return m_flDrainRate * 0.5f;
		else
			return m_flDrainRate; 
	}
#ifdef MAPBASE
	void	SetDeviceDrainRate( float flDrainRate ) { m_flDrainRate = flDrainRate; }
#endif
};

//=============================================================================
// >> HL2_PLAYER
//=============================================================================
#if defined ( HL2MP )
class CHL2_Player : public CBaseMultiplayerPlayer
#else
class CHL2_Player : public CBasePlayer
#endif
{
public:
#if defined ( HL2MP )
	DECLARE_CLASS( CHL2_Player, CBaseMultiplayerPlayer );
#else
	DECLARE_CLASS( CHL2_Player, CBasePlayer );
#endif
#ifdef MAPBASE_VSCRIPT
	DECLARE_ENT_SCRIPTDESC();
#endif

	CHL2_Player();
	~CHL2_Player( void );
	
	static CHL2_Player *CreatePlayer( const char *className, edict_t *ed )
	{
		CHL2_Player::s_PlayerEdict = ed;
		return (CHL2_Player*)CreateEntityByName( className );
	}

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual void		CreateCorpse( void ) { CopyToBodyQue( this ); };

	virtual void		Precache( void );
	virtual void		Spawn(void);
	virtual void		Activate( void );
	virtual void		CheatImpulseCommands( int iImpulse );
	virtual void		PlayerRunCommand( CUserCmd *ucmd, IMoveHelper *moveHelper);
	virtual void		PlayerUse ( void );
#ifdef EZ
	virtual void		OnUseEntity( CBaseEntity *pEntity ) {}
#endif
	virtual void		SuspendUse( float flDuration ) { m_flTimeUseSuspended = gpGlobals->curtime + flDuration; }
	virtual void		UpdateClientData( void );
	virtual void		OnRestore();
	virtual void		StopLoopingSounds( void );
	virtual void		Splash( void );
	virtual void 		ModifyOrAppendPlayerCriteria( AI_CriteriaSet& set );

#ifdef MAPBASE
	// For the logic_playerproxy output
	void				SpawnedAtPoint( CBaseEntity *pSpawnPoint );

	Activity			Weapon_TranslateActivity( Activity baseAct, bool *pRequired = NULL );

#ifdef SP_ANIM_STATE
	void				SetAnimation( PLAYER_ANIM playerAnim );

	void				AddAnimStateLayer( int iSequence, float flBlendIn = 0.0f, float flBlendOut = 0.0f, float flPlaybackRate = 1.0f, bool bHoldAtEnd = false, bool bOnlyWhenStill = false );
#endif

	virtual CStudioHdr*	OnNewModel();

	virtual const char *GetOverrideStepSound( const char *pszBaseStepSoundName );
#endif

	void				DrawDebugGeometryOverlays(void);

	virtual Vector		EyeDirection2D( void );
	virtual Vector		EyeDirection3D( void );

	virtual void		CommanderMode();

	virtual bool		ClientCommand( const CCommand &args );

	// from cbasecombatcharacter
	void				InitVCollision( const Vector &vecAbsOrigin, const Vector &vecAbsVelocity );
	WeaponProficiency_t CalcWeaponProficiency( CBaseCombatWeapon *pWeapon );

	Class_T				Classify ( void );

	// from CBasePlayer
	virtual void		SetupVisibility( CBaseEntity *pViewEntity, unsigned char *pvs, int pvssize );

	// Suit Power Interface
	void SuitPower_Update( void );
	bool SuitPower_Drain( float flPower ); // consume some of the suit's power.
	void SuitPower_Charge( float flPower ); // add suit power.
	void SuitPower_SetCharge( float flPower ) { m_HL2Local.m_flSuitPower = flPower; }
	void SuitPower_Initialize( void );
	bool SuitPower_IsDeviceActive( const CSuitPowerDevice &device );
	bool SuitPower_AddDevice( const CSuitPowerDevice &device );
	bool SuitPower_RemoveDevice( const CSuitPowerDevice &device );
	bool SuitPower_ShouldRecharge( void );
	float SuitPower_GetCurrentPercentage( void ) { return m_HL2Local.m_flSuitPower; }
	
	void SetFlashlightEnabled( bool bState );

#ifdef MAPBASE
	// Needed for logic_playerproxy
	float GetFlashlightBattery();
#endif

#ifdef EZ2
	virtual void ApplyFlashlightColorCorrection( bool bColorCorrectionEnabled );

	virtual void SetLegModel( string_t iszModel );

	void		OnDropSatchel( CBaseEntity *pSatchel );
	void		OnSetupTripmine( CBaseEntity *pTripmine );
	void		OnSetupDetonatable( CBaseEntity *pDetonatable );
	void		OnSatchelExploded( CBaseEntity *pSatchel, CBaseEntity *pAttacker );
	void		OnTripmineExploded( CBaseEntity *pTripmine, CBaseEntity *pAttacker );
	void		OnDetonatableExploded( CBaseEntity *pDetonatable, CBaseEntity *pAttacker );
	void		OnDetonatableDisabled( CBaseEntity *pDetonatable );
#endif

	// Apply a battery
	bool ApplyBattery( float powerMultiplier = 1.0 );

	// Commander Mode for controller NPCs
	enum CommanderCommand_t
	{
		CC_NONE,
		CC_TOGGLE,
		CC_FOLLOW,
		CC_SEND,
	};

	void CommanderUpdate();
	void CommanderExecute( CommanderCommand_t command = CC_TOGGLE );
	bool CommanderFindGoal( commandgoal_t *pGoal );
	void NotifyFriendsOfDamage( CBaseEntity *pAttackerEntity );
	CAI_BaseNPC *GetSquadCommandRepresentative();
#ifdef EZ
	// Blixibon - Silent commandables follow orders, but don't count towards squad tallies
	int GetNumSquadCommandables(bool bSilentMembers = false);
#else
	int GetNumSquadCommandables();
#endif
	int GetNumSquadCommandableMedics();
#ifdef EZ2
	inline CAI_Squad	*GetPlayerSquad() { return m_pPlayerAISquad; } // Blixibon - Needed for Bad Cop responses
#endif

#ifdef MAPBASE
	void InputSquadForceSummon( inputdata_t &inputdata );
	void InputSquadForceGoTo( inputdata_t &inputdata );

	void InputEnableGeigerCounter( inputdata_t &inputdata );
	void InputDisableGeigerCounter( inputdata_t &inputdata );

	void InputShowSquadHUD( inputdata_t &inputdata );
	void InputHideSquadHUD( inputdata_t &inputdata );
#endif

	// Locator
	void UpdateLocatorPosition( const Vector &vecPosition );

	// Sprint Device
	void StartAutoSprint( void );
	void StartSprinting( void );
	void StopSprinting( void );
	void InitSprinting( void );
	bool IsSprinting( void ) { return m_fIsSprinting; }
	bool CanSprint( void );
	void EnableSprint( bool bEnable);
	
	bool CanZoom( CBaseEntity *pRequester );
	void ToggleZoom(void);
	void StartZooming( void );
	void StopZooming( void );
	bool IsZooming( void );
	void CheckSuitZoom( void );

	// Walking
	void StartWalking( void );
	void StopWalking( void );
	bool IsWalking( void ) { return m_fIsWalking; }

	// Aiming heuristics accessors
	virtual float		GetIdleTime( void ) const { return ( m_flIdleTime - m_flMoveTime ); }
	virtual float		GetMoveTime( void ) const { return ( m_flMoveTime - m_flIdleTime ); }
	virtual float		GetLastDamageTime( void ) const { return m_flLastDamageTime; }
	virtual bool		IsDucking( void ) const { return !!( GetFlags() & FL_DUCKING ); }

	virtual bool		PassesDamageFilter( const CTakeDamageInfo &info );
	void				InputIgnoreFallDamage( inputdata_t &inputdata );
	void				InputIgnoreFallDamageWithoutReset( inputdata_t &inputdata );
	void				InputEnableFlashlight( inputdata_t &inputdata );
	void				InputDisableFlashlight( inputdata_t &inputdata );

#ifdef MAPBASE
	void				InputAddArmor( inputdata_t &inputdata );
	void				InputRemoveArmor( inputdata_t &inputdata );
	void				InputSetArmor( inputdata_t &inputdata );

	void				InputAddAuxPower( inputdata_t &inputdata );
	void				InputRemoveAuxPower( inputdata_t &inputdata );
	void				InputSetAuxPower( inputdata_t &inputdata );

	void				InputTurnFlashlightOn( inputdata_t &inputdata );
	void				InputTurnFlashlightOff( inputdata_t &inputdata );
#endif

	const impactdamagetable_t &GetPhysicsImpactDamageTable();
	virtual int			OnTakeDamage( const CTakeDamageInfo &info );
	virtual int			OnTakeDamage_Alive( const CTakeDamageInfo &info );
	virtual void		OnDamagedByExplosion( const CTakeDamageInfo &info );
	bool				ShouldShootMissTarget( CBaseCombatCharacter *pAttacker );

	void				CombineBallSocketed( CPropCombineBall *pCombineBall );

	virtual void		Event_KilledOther( CBaseEntity *pVictim, const CTakeDamageInfo &info );

	virtual void		GetAutoaimVector( autoaim_params_t &params );
	bool				ShouldKeepLockedAutoaimTarget( EHANDLE hLockedTarget );

	void				SetLocatorTargetEntity( CBaseEntity *pEntity ) { m_hLocatorTargetEntity.Set( pEntity ); }

#ifdef MAPBASE
	virtual bool		CanAutoSwitchToNextBestWeapon( CBaseCombatWeapon *pWeapon );
#endif

	virtual int			GiveAmmo( int nCount, int nAmmoIndex, bool bSuppressSound);
	virtual bool		BumpWeapon( CBaseCombatWeapon *pWeapon );
	
	virtual bool		Weapon_CanUse( CBaseCombatWeapon *pWeapon );
	virtual void		Weapon_Equip( CBaseCombatWeapon *pWeapon );
	virtual bool		Weapon_Lower( void );
	virtual bool		Weapon_Ready( void );
	virtual bool		Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex = 0 );
	virtual bool		Weapon_CanSwitchTo( CBaseCombatWeapon *pWeapon );

	void FirePlayerProxyOutput( const char *pszOutputName, variant_t variant, CBaseEntity *pActivator, CBaseEntity *pCaller );

	CLogicPlayerProxy	*GetPlayerProxy( void );

	// Flashlight Device
	void				CheckFlashlight( void );
	int					FlashlightIsOn( void );
	void				FlashlightTurnOn( void );
	void				FlashlightTurnOff( void );
	bool				IsIlluminatedByFlashlight( CBaseEntity *pEntity, float *flReturnDot );
	void				SetFlashlightPowerDrainScale( float flScale ) { m_flFlashlightPowerDrainScale = flScale; }

	// Underwater breather device
	virtual void		SetPlayerUnderwater( bool state );
	virtual bool		CanBreatheUnderwater() const { return m_HL2Local.m_flSuitPower > 0.0f; }

	// physics interactions
	virtual void		PickupObject( CBaseEntity *pObject, bool bLimitMassAndSize );
	virtual	bool		IsHoldingEntity( CBaseEntity *pEnt );
	virtual void		ForceDropOfCarriedPhysObjects( CBaseEntity *pOnlyIfHoldindThis );
	virtual float		GetHeldObjectMass( IPhysicsObject *pHeldObject );
	virtual CBaseEntity	*GetHeldObject( void );

	virtual bool		IsFollowingPhysics( void ) { return (m_afPhysicsFlags & PFLAG_ONBARNACLE) > 0; }
	void				InputForceDropPhysObjects( inputdata_t &data );

	virtual void		Event_Killed( const CTakeDamageInfo &info );
	void				NotifyScriptsOfDeath( void );

	// override the test for getting hit
	virtual bool		TestHitboxes( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );

	LadderMove_t		*GetLadderMove() { return &m_HL2Local.m_LadderMove; }
	virtual void		ExitLadder();
	virtual surfacedata_t *GetLadderSurface( const Vector &origin );

	virtual void EquipSuit( bool bPlayEffects = true );
	virtual void RemoveSuit( void );
	void  HandleAdmireGlovesAnimation( void );
	void  StartAdmireGlovesAnimation( void );
	
	void  HandleSpeedChanges( void );

	void SetControlClass( Class_T controlClass ) { m_nControlClass = controlClass; }
	
	void StartWaterDeathSounds( void );
	void StopWaterDeathSounds( void );

	bool IsWeaponLowered( void ) { return m_HL2Local.m_bWeaponLowered; }
	void HandleArmorReduction( void );
	void StartArmorReduction( void ) { m_flArmorReductionTime = gpGlobals->curtime + ARMOR_DECAY_TIME; 
									   m_iArmorReductionFrom = ArmorValue(); 
									 }

	void MissedAR2AltFire();

	inline void EnableCappedPhysicsDamage();
	inline void DisableCappedPhysicsDamage();

	// HUD HINTS
	void DisplayLadderHudHint();

#ifdef MAPBASE
	void InitCustomSuitDevice( int iDeviceID, float flDrainRate );
	void AddCustomSuitDevice( int iDeviceID );
	void RemoveCustomSuitDevice( int iDeviceID );
	bool IsCustomSuitDeviceActive( int iDeviceID );
#endif

	CSoundPatch *m_sndLeeches;
	CSoundPatch *m_sndWaterSplashes;

protected:
	virtual void		PreThink( void );
	virtual	void		PostThink( void );
	virtual bool		HandleInteraction(int interactionType, void *data, CBaseCombatCharacter* sourceEnt);

	virtual void		UpdateWeaponPosture( void );

	virtual void		ItemPostFrame();
	virtual void		PlayUseDenySound();

#ifdef EZ2
	virtual void		HandleKickAttack();
	virtual void		TraceKick( trace_t &tr, const Vector &vecAim );
	virtual void		TraceKickAttack( CBaseEntity* pKickedEntity = NULL );
	virtual bool		TryRagdollKickedEnemy(CBaseEntity* pKickedEntity, trace_t* tr, CTakeDamageInfo* dmgInfo, CBaseEntity* pKickingEntity);

	void  HandleKickAnimation( void );
	void  StartKickAnimation( void );

	virtual void HandleAnimEvent( animevent_t *pEvent );
#endif

#ifndef EZ
private:
	void				OnSquadMemberKilled( inputdata_t &data );
#else
	virtual void		CreateSquadMarker( commandgoal_t &goal );
	virtual void		CleanUpSquadMarker( void ); // 1upD - remove squad marker if all squadmates are dead
	virtual void		OnSquadMemberKilled( inputdata_t &data ); // 1upD - made protected and virtual so EZ2 player can override							
#endif
	virtual bool		CommanderExecuteOne( CAI_BaseNPC *pNpc, const commandgoal_t &goal, CAI_BaseNPC **Allies, int numAllies ); // 1upD - made protected and virtual so EZ2 player can override
#ifdef EZ
private:
	void				helperFireSquadCommandOuput( const char * outputName,CAI_BaseNPC ** Allies ); // 1upD - fire player proxy output for squad control changes
#endif

	Class_T				m_nControlClass;			// Class when player is controlling another entity
	// This player's HL2 specific data that should only be replicated to 
	//  the player and not to other players.
	CNetworkVarEmbedded( CHL2PlayerLocalData, m_HL2Local );

	float				m_flTimeAllSuitDevicesOff;

	bool				m_bSprintEnabled;		// Used to disable sprint temporarily
	bool				m_bIsAutoSprinting;		// A proxy for holding down the sprint key.
	float				m_fAutoSprintMinTime;	// Minimum time to maintain autosprint regardless of player speed. 

	CNetworkVar( bool, m_fIsSprinting );
	CNetworkVarForDerived( bool, m_fIsWalking );

protected:	// Jeep: Portal_Player needs access to this variable to overload PlayerUse for picking up objects through portals
	bool				m_bPlayUseDenySound;		// Signaled by PlayerUse, but can be unset by HL2 ladder code...

private:

	CAI_Squad *			m_pPlayerAISquad;
	CSimpleSimTimer		m_CommanderUpdateTimer;
	float				m_RealTimeLastSquadCommand;
	CommanderCommand_t	m_QueuedCommand;
#ifdef EZ
	EHANDLE				m_hCommandPointProp;
#endif

#ifdef EZ2
	CUtlVector<CBaseEntity*>	m_hActiveSatchels;
	CUtlVector<CBaseEntity*>	m_hActiveTripmines;
	CUtlVector<CBaseEntity*>	m_hActiveDetonatables;
#endif

	Vector				m_vecMissPositions[16];
	int					m_nNumMissPositions;

	float				m_flTimeIgnoreFallDamage;
	bool				m_bIgnoreFallDamageResetAfterImpact;

	// Suit power fields
	float				m_flSuitPowerLoad;	// net suit power drain (total of all device's drainrates)
	float				m_flAdmireGlovesAnimTime;

	float				m_flNextFlashlightCheckTime;
	float				m_flFlashlightPowerDrainScale;

#ifdef EZ2
	EHANDLE				m_hFlashlightColorCorrection;
	bool				m_bHandledColorCorrection; // NOT saved - this tells us that within this session, CC hasn't been cleaned up yet

	float				m_flNextKickAttack;
	bool				m_bKickWeaponLowered;

	string_t		    m_LegModelName;
#endif

	// Aiming heuristics code
	float				m_flIdleTime;		//Amount of time we've been motionless
	float				m_flMoveTime;		//Amount of time we've been in motion
	float				m_flLastDamageTime;	//Last time we took damage
	float				m_flTargetFindTime;

	EHANDLE				m_hPlayerProxy;

	bool				m_bFlashlightDisabled;
	bool				m_bUseCappedPhysicsDamageTable;
	
	float				m_flArmorReductionTime;
	int					m_iArmorReductionFrom;

	float				m_flTimeUseSuspended;

	CSimpleSimTimer		m_LowerWeaponTimer;
	CSimpleSimTimer		m_AutoaimTimer;

	EHANDLE				m_hLockedAutoAimEntity;

	EHANDLE				m_hLocatorTargetEntity; // The entity that's being tracked by the suit locator.

	float				m_flTimeNextLadderHint;	// Next time we're eligible to display a HUD hint about a ladder.
	
	friend class CHL2GameMovement;

#ifdef SP_ANIM_STATE
	CSinglePlayerAnimState* m_pPlayerAnimState;

	// At the moment, we network the render angles since almost none of the player anim stuff is done on the client in SP.
	// If any of this is ever adapted for MP, this method should be replaced with replicating/moving the anim state to the client.
	CNetworkVar( float, m_flAnimRenderYaw );
#endif
};


//-----------------------------------------------------------------------------
// FIXME: find a better way to do this
// Switches us to a physics damage table that caps the max damage.
//-----------------------------------------------------------------------------
void CHL2_Player::EnableCappedPhysicsDamage()
{
	m_bUseCappedPhysicsDamageTable = true;
}


void CHL2_Player::DisableCappedPhysicsDamage()
{
	m_bUseCappedPhysicsDamageTable = false;
}

#ifdef EZ
//-----------------------------------------------------------------------------
// Purpose: Special template class for minor Combine units like manhacks to serve as "silent" squad members
// which follow orders, but don't count towards squad tallies and prioritize non-silent members like soldiers.
// 
// This is intended to be a temporary location for this class until something more dedicated comes along.
// 
// Created by Blixibon.
//-----------------------------------------------------------------------------
template <class BASE_NPC>
class CAI_SilentSquadMember : public BASE_NPC
{
public:
	// This should probably still be overridden
	virtual bool	IsCommandable() { return true; }

	virtual bool	IsSilentCommandable() { return true; }

	virtual bool TargetOrder( CBaseEntity *pTarget, CAI_BaseNPC **Allies, int numAllies ) { this->OnTargetOrder(); this->ClearCommandGoal(); this->ClearCondition( COND_RECEIVED_ORDERS ); return true; }

	virtual CAI_BaseNPC *GetSquadCommandRepresentative()
	{
		// Look through the squad and find the first member that isn't silent
		if ( this->m_pSquad )
		{
			AISquadIter_t iter;
			CAI_BaseNPC *pSquadmate = this->m_pSquad->GetFirstMember( &iter );
			while ( pSquadmate )
			{
				if ( pSquadmate->IsCommandable() && !pSquadmate->IsSilentCommandable() )
					return pSquadmate->GetSquadCommandRepresentative();

				pSquadmate = this->m_pSquad->GetNextMember( &iter );
			}
		}

		// Well, if we can't find a non-silent squadmate, Bad Cop must be stuck with us.
		// (lets Bad Cop order silent members without needing fully-fledged members)
		return this;
	}

	// Override with commander interrupt conditions
	virtual bool CanOrdersInterrupt() { return this->GetState() != NPC_STATE_COMBAT; }

	void BuildScheduleTestBits( void )
	{
		BASE_NPC::BuildScheduleTestBits();

		if ( this->CanOrdersInterrupt() )
		{
			this->SetCustomInterruptCondition( COND_RECEIVED_ORDERS );
		}
	}

	bool HaveCommandGoal() const { return this->GetCommandGoal() != vec3_invalid; }
};
#endif

#endif	//HL2_PLAYER_H
