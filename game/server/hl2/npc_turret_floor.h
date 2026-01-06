//========= Copyright Valve Corporation, All rights reserved. ============//
#ifndef NPC_TURRET_FLOOR_H
#define NPC_TURRET_FLOOR_H
#ifdef _WIN32
#pragma once
#endif

#include "ai_basenpc.h"
#include "player_pickup.h"
#include "particle_system.h"
#ifdef EZ2
#include "ai_speech.h"
#endif

//Turret states
enum turretState_e
{
	TURRET_SEARCHING,
	TURRET_AUTO_SEARCHING,
	TURRET_ACTIVE,
	TURRET_SUPPRESSING,
	TURRET_DEPLOYING,
	TURRET_RETIRING,
	TURRET_TIPPED,
	TURRET_SELF_DESTRUCTING,

	TURRET_STATE_TOTAL
};

//Eye states
enum eyeState_t
{
	TURRET_EYE_SEE_TARGET,			//Sees the target, bright and big
	TURRET_EYE_SEEKING_TARGET,		//Looking for a target, blinking (bright)
	TURRET_EYE_DORMANT,				//Not active
	TURRET_EYE_DEAD,				//Completely invisible
	TURRET_EYE_DISABLED,			//Turned off, must be reactivated before it'll deploy again (completely invisible)
	TURRET_EYE_ALARM,				// On side, but warning player to pick it back up
};

//Spawnflags
// BUG: These all stomp Base NPC spawnflags. Any Base NPC code called by this
//		this class may have undesired side effects due to these being set.
#define SF_FLOOR_TURRET_AUTOACTIVATE		0x00000020
#define SF_FLOOR_TURRET_STARTINACTIVE		0x00000040
#define SF_FLOOR_TURRET_FASTRETIRE			0x00000080
#define SF_FLOOR_TURRET_OUT_OF_AMMO			0x00000100
#define SF_FLOOR_TURRET_CITIZEN				0x00000200	// Citizen modified turret
#ifdef MAPBASE
#define SF_FLOOR_TURRET_NO_SPRITE			0x00000400
#endif

class CTurretTipController;
class CBeam;
class CSprite;
#ifdef EZ2
//-----------------------------------------------------------------------------
// A new abstract class that allows us to use CTurretTipController on Wilson.
// -Blixibon
//-----------------------------------------------------------------------------
abstract_class ITippableTurret
{
public:
	virtual bool IsBeingCarriedByPlayer( void ) = 0;
	virtual bool WasJustDroppedByPlayer( void ) = 0;

	// Prevents tip controller from interfering with turrets under low gravity
	// trigger_vphysics_motion also uses this class and these methods to detect and apply that to both turrets and Wilson
	bool	InLowGravity( void ) { return m_bInLowGravity; }
	void	SetLowGravity( bool bToggle ) { m_bInLowGravity = bToggle; }

	bool	m_bInLowGravity; // Not saved
};
#endif

//-----------------------------------------------------------------------------
// Purpose: Floor turret
//-----------------------------------------------------------------------------
#ifdef EZ2
class CNPC_FloorTurret : public CNPCBaseInteractive<CAI_BaseNPC>, public CDefaultPlayerPickupVPhysics, public ITippableTurret
#else
class CNPC_FloorTurret : public CNPCBaseInteractive<CAI_BaseNPC>, public CDefaultPlayerPickupVPhysics
#endif
{
	DECLARE_CLASS( CNPC_FloorTurret, CNPCBaseInteractive<CAI_BaseNPC> );
public:

	CNPC_FloorTurret( void );

	virtual void	Precache( void );
	virtual void	Spawn( void );
	virtual void	Activate( void );
	virtual bool	CreateVPhysics( void );
	virtual void	UpdateOnRemove( void );
	virtual int		OnTakeDamage( const CTakeDamageInfo &info );
	virtual void	PlayerPenetratingVPhysics( void );
	virtual int		VPhysicsTakeDamage( const CTakeDamageInfo &info );
	virtual bool	CanBecomeServerRagdoll( void ) { return false; }

#ifdef HL2_EPISODIC
	// We don't want to be NPCSOLID because we'll collide with NPC clips
	virtual unsigned int PhysicsSolidMaskForEntity( void ) const { return MASK_SOLID; } 
#endif	// HL2_EPISODIC

	// Player pickup
	virtual void	OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason );
	virtual void	OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t Reason );
	virtual bool	HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer );
	virtual QAngle	PreferredCarryAngles( void );
	virtual bool	OnAttemptPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason );

	const char *GetTracerType( void ) { return "AR2Tracer"; }

	bool	ShouldSavePhysics() { return true; }

	bool	HandleInteraction( int interactionType, void *data, CBaseCombatCharacter *sourceEnt );

	// Think functions
	virtual void	Retire( void );
	virtual void	Deploy( void );
	virtual void	ActiveThink( void );
	virtual void	SearchThink( void );
	virtual void	AutoSearchThink( void );
	virtual void	TippedThink( void );
	virtual void	InactiveThink( void );
	virtual void	SuppressThink( void );
	virtual void	DisabledThink( void );
	virtual void	SelfDestructThink( void );
	virtual void	BreakThink( void );
	virtual void	HackFindEnemy( void );

	virtual float	GetAttackDamageScale( CBaseEntity *pVictim );
	virtual Vector	GetAttackSpread( CBaseCombatWeapon *pWeapon, CBaseEntity *pTarget );

	// Do we have a physics attacker?
	CBasePlayer *HasPhysicsAttacker( float dt );
	bool IsHeldByPhyscannon( )	{ return VPhysicsGetObject() && (VPhysicsGetObject()->GetGameFlags() & FVPHYSICS_PLAYER_HELD); }

	// Use functions
	void	ToggleUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	int ObjectCaps() 
	{ 
		return BaseClass::ObjectCaps() | FCAP_IMPULSE_USE;
	}

	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		CBasePlayer *pPlayer = ToBasePlayer( pActivator );
		if ( pPlayer )
		{
			pPlayer->PickupObject( this, false );
		}
	}

	// Inputs
	void	InputToggle( inputdata_t &inputdata );
	void	InputEnable( inputdata_t &inputdata );
	void	InputDisable( inputdata_t &inputdata );
	void	InputDepleteAmmo( inputdata_t &inputdata );
	void	InputRestoreAmmo( inputdata_t &inputdata );
	void	InputSelfDestruct( inputdata_t &inputdata );
#ifdef MAPBASE
	void	InputCreateSprite( inputdata_t &inputdata );
	void	InputDestroySprite( inputdata_t &inputdata );
	void	InputPowerdown( inputdata_t &inputdata ) { InputSelfDestruct(inputdata); }
#endif

	virtual bool	IsValidEnemy( CBaseEntity *pEnemy );
	bool			CanBeAnEnemyOf( CBaseEntity *pEnemy );
	bool			IsBeingCarriedByPlayer( void ) { return m_bCarriedByPlayer; }
	bool			WasJustDroppedByPlayer( void );

	int		BloodColor( void ) { return DONT_BLEED; }
	float	MaxYawSpeed( void );

	virtual Class_T	Classify( void );

	Vector EyePosition( void )
	{
		UpdateMuzzleMatrix();
		
		Vector vecOrigin;
		MatrixGetColumn( m_muzzleToWorld, 3, vecOrigin );
		
		Vector vecForward;
		MatrixGetColumn( m_muzzleToWorld, 0, vecForward );
		
		// Note: We back up into the model to avoid an edge case where the eyes clip out of the world and
		//		 cause problems with the PVS calculations -- jdw

		vecOrigin -= vecForward * 8.0f;

		return vecOrigin;
	}

	Vector	EyeOffset( Activity nActivity ) { return Vector( 0, 0, 58 ); }

	// Restore the turret to working operation after falling over
	void	ReturnToLife( void );

	int		DrawDebugTextOverlays( void );

	// INPCInteractive Functions
#ifdef MAPBASE
	virtual bool	CanInteractWith( CAI_BaseNPC *pUser );
#else
	virtual bool	CanInteractWith( CAI_BaseNPC *pUser ) { return false; } // Disabled for now (sjb)
#endif
	virtual	bool	HasBeenInteractedWith()	{ return m_bHackedByAlyx; }
	virtual void	NotifyInteraction( CAI_BaseNPC *pUser )
	{
		// For now, turn green so we can tell who is hacked.
		SetRenderColor( 0, 255, 0 );
		m_bHackedByAlyx = true; 
#ifdef MAPBASE
		m_OnHacked.FireOutput(pUser, this);
#endif
	}

	static float	fMaxTipControllerVelocity;
	static float	fMaxTipControllerAngularVelocity;

protected:

	virtual bool	PreThink( turretState_e state );
	virtual void	Shoot( const Vector &vecSrc, const Vector &vecDirToEnemy, bool bStrict = false );
	virtual void	SetEyeState( eyeState_t state );
	void			Ping( void );	
	void			Toggle( void );
	void			Enable( void );
	void			Disable( void );
	void			SpinUp( void );
	void			SpinDown( void );

	virtual bool	OnSide( void );

	bool	IsCitizenTurret( void ) { return HasSpawnFlags( SF_FLOOR_TURRET_CITIZEN ); }
	bool	UpdateFacing( void );
#ifdef EZ2
	virtual
#endif
	void	DryFire( void );
	void	UpdateMuzzleMatrix();

#ifdef EZ2
	virtual float	GetRange();
#endif

protected:
	matrix3x4_t m_muzzleToWorld;
	int		m_muzzleToWorldTick;
	int		m_iAmmoType;

	bool	m_bAutoStart;
	bool	m_bActive;		//Denotes the turret is deployed and looking for targets
	bool	m_bBlinkState;
	bool	m_bEnabled;		//Denotes whether the turret is able to deploy or not
	bool	m_bNoAlarmSounds;
	bool	m_bSelfDestructing;	// Going to blow up

	float	m_flDestructStartTime;
	float	m_flShotTime;
	float	m_flLastSight;
	float	m_flThrashTime;
	float	m_flPingTime;
	float	m_flNextActivateSoundTime;
	bool	m_bCarriedByPlayer;
	bool	m_bUseCarryAngles;
	float	m_flPlayerDropTime;
#ifndef MAPBASE // Replaced with m_nSkin.
	int		m_iKeySkin;
#endif

	CHandle<CBaseCombatCharacter> m_hLastNPCToKickMe;		// Stores the last NPC who tried to knock me over
	float	m_flKnockOverFailedTime;						// Time at which we should tell the NPC that he failed to knock me over

	QAngle	m_vecGoalAngles;

	int						m_iEyeAttachment;
	int						m_iMuzzleAttachment;
	eyeState_t				m_iEyeState;
	CHandle<CSprite>		m_hEyeGlow;
	CHandle<CBeam>			m_hLaser;
	CHandle<CTurretTipController>	m_pMotionController;

	CHandle<CParticleSystem>	m_hFizzleEffect;
	Vector	m_vecEnemyLKP;

	// physics influence
	CHandle<CBasePlayer>	m_hPhysicsAttacker;
	float					m_flLastPhysicsInfluenceTime;

	static const char		*m_pShotSounds[];

	COutputEvent m_OnDeploy;
	COutputEvent m_OnRetire;
	COutputEvent m_OnTipped;
	COutputEvent m_OnPhysGunPickup;
	COutputEvent m_OnPhysGunDrop;

	bool	m_bHackedByAlyx;
	HSOUNDSCRIPTHANDLE			m_ShotSounds;

	DECLARE_DATADESC();
	DEFINE_CUSTOM_AI;
};

//
// Tip controller
//

class CTurretTipController : public CPointEntity, public IMotionEvent
{
	DECLARE_CLASS( CTurretTipController, CPointEntity );
	DECLARE_DATADESC();

public:

	~CTurretTipController( void );
	void Spawn( void );
	void Activate( void );
	void Enable( bool state = true );
	void Suspend( float time );
	float SuspendedTill( void );

	bool Enabled( void );

#ifdef EZ2
	static CTurretTipController	*CreateTipController( CBaseEntity *pOwner )
#else
	static CTurretTipController	*CreateTipController( CNPC_FloorTurret *pOwner )
#endif
	{
		if ( pOwner == NULL )
			return NULL;

#ifdef EZ2
		ITippableTurret *pTippable = dynamic_cast<ITippableTurret*>(pOwner);
		if ( pTippable == NULL )
		{
			Warning("WARNING: %s isn't an ITippableTurret!\n", pOwner->GetDebugName());
			return NULL;
		}
#endif

		CTurretTipController *pController = (CTurretTipController *) Create( "floorturret_tipcontroller", pOwner->GetAbsOrigin(), pOwner->GetAbsAngles() );

		if ( pController != NULL )
		{
			pController->m_pParentTurret = pOwner;
#ifdef EZ2
			pController->m_pTippable = pTippable;
#endif
		}

		return pController;
	}

	// IMotionEvent
	virtual simresult_e	Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular );

private:
	bool						m_bEnabled;
	float						m_flSuspendTime;
	Vector						m_worldGoalAxis;
	Vector						m_localTestAxis;
	IPhysicsMotionController	*m_pController;
	float						m_angularLimit;
#ifdef EZ2
	// Should still have ITippableTurret
	CBaseEntity					*m_pParentTurret;
	// So we don't have to use dynamic_cast
	ITippableTurret				*m_pTippable;
#else
	CNPC_FloorTurret			*m_pParentTurret;
#endif
};

#ifdef EZ2
// Turret concepts
#define TLK_SEARCHING "TLK_SEARCHING"
#define TLK_AUTOSEARCH "TLK_AUTOSEARCH"
#define TLK_ACTIVE "TLK_ACTIVE"
#define TLK_SUPPRESS "TLK_SUPPRESS"
#define TLK_DEPLOY "TLK_DEPLOY"
#define TLK_RETIRE "TLK_RETIRE"
#define TLK_TIPPED "TLK_TIPPED"
#define TLK_DISABLED "TLK_DISABLED"
#define TLK_PICKUP "TLK_PICKUP"

//-----------------------------------------------------------------------------
// Purpose: Bootleg version of the Portal floor turret
//-----------------------------------------------------------------------------
class CNPC_Arbeit_FloorTurret : public CAI_ExpresserHost<CNPC_FloorTurret>
{
	DECLARE_CLASS( CNPC_Arbeit_FloorTurret, CAI_ExpresserHost<CNPC_FloorTurret> );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();
public:

	CNPC_Arbeit_FloorTurret( void );

	virtual void	Precache( void );
	virtual void	Spawn( void );
	virtual void	Activate( void );
	virtual bool	CreateVPhysics( void );

	// Player pickup
	virtual void	OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason );
	virtual void	OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t Reason );

	const char *GetTracerType( void ) { return "Tracer"; }

	void			OnChangeActivity( Activity eNewActivity );

	Class_T			Classify( void );

	bool			CanBeAnEnemyOf( CBaseEntity *pEnemy );

	void			InputTurnOnEyeLight( inputdata_t &inputdata ) { m_bEyeLightEnabled = true; }
	void			InputTurnOffEyeLight( inputdata_t &inputdata ) { m_bEyeLightEnabled = false; }

	void			InputTurnOnLaser( inputdata_t &inputdata ) { m_bLaser = true; }
	void			InputTurnOffLaser( inputdata_t &inputdata ) { m_bLaser = false; }

	void			InputEnableSilently( inputdata_t &inputdata );

	void			InputEnableRetire( inputdata_t &inputdata );
	void			InputDisableRetire( inputdata_t &inputdata );

	bool SpeakIfAllowed( const char *concept );
	bool SpeakIfAllowed( const char *concept, AI_CriteriaSet &modifiers );
	void ModifyOrAppendCriteria( AI_CriteriaSet& set );

	virtual CAI_Expresser *CreateExpresser( void );
	virtual CAI_Expresser *GetExpresser() { return m_pExpresser; }
	virtual void		PostConstructor( const char *szClassname );

	bool IsPlayerAlly() { return m_iTurretType == TURRET_TYPE_ALLY; }

protected:

	virtual void	SetEyeState( eyeState_t state );
	virtual bool	PreThink( turretState_e state );

	virtual void	TippedThink( void );
	virtual void	InactiveThink( void );

	virtual void	RetireRestrictedThink( void );

	void	UpdateLaser();

	void	DryFire( void );

	float	GetRange() { return m_flRange; }

private:
	CAI_Expresser *m_pExpresser;

	turretState_e m_iCurrentState;

	bool	m_bCanRetire;

	enum TurretType_t
	{
		TURRET_TYPE_NORMAL,
		TURRET_TYPE_BEAST,
		TURRET_TYPE_ALLY,
	};

	TurretType_t	m_iTurretType;

	bool	m_bStatic;

	CNetworkVar( float, m_flRange );
	CNetworkVar( float, m_flFOV );

	// Enables a projected texture spotlight on the client.
	CNetworkVar( bool, m_bEyeLightEnabled );
	CNetworkVar( int, m_iEyeLightBrightness );

	bool	m_bUseLaser;
	CNetworkVar( bool, m_bLaser );

	CNetworkVar( bool, m_bGooTurret );

	CNetworkVar( bool, m_bCitizenTurret );

	// Client needs to know for ropes
	CNetworkVar( bool, m_bClosedIdle );
};
#endif

#endif //#ifndef NPC_TURRET_FLOOR_H
