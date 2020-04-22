//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Base Object built by a player
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_OBJ_H
#define TF_OBJ_H
#ifdef _WIN32
#pragma once
#endif

#include "baseentity.h"
#include "tf_func_resource.h"
#include "ihasbuildpoints.h"
#include "baseobject_shared.h"
#include "info_vehicle_bay.h"

class CBaseTFPlayer;
class CTFTeam;
class CRopeKeyframe;
class CBaseTechnology;
class CGrenadeObjectSapper;
class CObjectPowerPack;
class CVGuiScreen;
class CResourceZone;
class KeyValues;
class CObjectBuffStation;
struct animevent_t;

#define OBJECT_REPAIR_RATE		10			// Health healed per second while repairing

// Construction
#define OBJECT_CONSTRUCTION_INTERVAL			0.1
#define OBJECT_CONSTRUCTION_STARTINGHEALTH		0.1


extern ConVar object_verbose;
extern ConVar obj_child_range_factor;

#if defined( _DEBUG )
#define TRACE_OBJECT( str )										\
if ( object_verbose.GetInt() )									\
{																\
	Msg( "%s", str );					\
}																
#else
#define TRACE_OBJECT( string )
#endif


// ------------------------------------------------------------------------ //
// Resupply object that's built by the player
// ------------------------------------------------------------------------ //
class CBaseObject : public CBaseCombatCharacter, public IHasBuildPoints
{
	DECLARE_CLASS( CBaseObject, CBaseCombatCharacter );
public:
	CBaseObject();

	virtual void	UpdateOnRemove( void );

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual bool	IsBaseObject( void ) const { return true; }

	virtual void	BaseObjectThink( void );
	virtual void	LostPowerThink( void );
	virtual CBaseTFPlayer *GetOwner( void );

	// Creation
	virtual void	Precache();
	virtual void	Spawn( void );
	virtual void	Activate( void );
	void			InitializeMapPlacedObject( void );

	virtual void	SetBuilder( CBaseTFPlayer *pBuilder, bool moveobjects = false );
	virtual void	SetupTeamModel( void ) { return; }
	virtual void	SetType( int iObjectType );
	int				ObjectType( ) const;

	virtual int		BloodColor( void ) { return BLOOD_COLOR_MECH; }

	// Building
	virtual float	GetTotalTime( void );
	virtual void	StartPlacement( CBaseTFPlayer *pPlayer );
	void			StopPlacement( void );
	bool			FindNearestBuildPoint( CBaseEntity *pEntity, Vector vecBuildOrigin, float &flNearestPoint, Vector &vecNearestBuildPoint );
	virtual bool	CalculatePlacement( CBaseTFPlayer *pPlayer );
	bool			CheckBuildPoint( Vector vecPoint, Vector &vecTrace, Vector *vecOutPoint=NULL );
	bool			VerifyCorner( const Vector &vBottomCenter, float xOffset, float yOffset );
	virtual bool	CheckBuildOrigin( CBaseTFPlayer *pPlayer, const Vector &vecBuildOrigin, bool bSnappedToPoint = false );
	void			AttemptToFindPower( void );
	void			AttemptToFindBuffStation( void );
	void			AttemptToActivateBuffStation( void );
	virtual float	GetNearbyObjectCheckRadius( void ) { return 30.0; }
	bool			UpdatePlacement( CBaseTFPlayer *pPlayer );
	virtual bool	ShouldAttachToParent( void ) { return true; }
	void			SetVehicleBay( CVGuiScreenVehicleBay *pBay ) { m_hVehicleBay = pBay; }

	// Sort of a hack for walkers - vehicles are pre-rotated by 90 degrees and walkers need to undo this.
	virtual void	AdjustInitialBuildAngles();
	
	// Exit points for mounted vehicles....
	virtual void	GetExitPoint( CBaseEntity *pPlayer, int nBuildPoint, Vector *pExitPoint, QAngle *pAngles );

	// I've finished building the specified object on the specified build point
	virtual int		FindObjectOnBuildPoint( CBaseObject *pObject );

	// This gives an object a chance to prevent itself from being built when the user clicks the 
	// attack button during placement. Barbed wire uses this to change which object the barbed wire 
	// is attached to.
	virtual bool PreStartBuilding();
	
	virtual bool	StartBuilding( CBaseEntity *pPlayer );
	void			BuildingThink( void );
	void			SetControlPanelsActive( bool bState );
	virtual void	FinishedBuilding( void );
	bool			IsBuilding( void ) { return m_bBuilding; };
	bool			IsPlacing( void ) { return m_bPlacing; };
	bool			MustBeBuiltInResourceZone( void ) const;
	bool			MustBeBuiltInConstructionYard( void ) const;
	virtual bool	MustNotBeBuiltInConstructionYard( void ) const;
	bool			MustBeBuiltOnAttachmentPoint( void ) const;

	void			AlignToGround( Vector vecOrigin );

	// Returns information about the various control panels
	virtual void 	GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
	virtual void	GetControlPanelClassName( int nPanelIndex, const char *&pPanelName );

	// Client commands sent by clicking on various panels....
	// NOTE: pArg->Argv(0) == pCmd, pArg->Argv(1) == the first argument
	virtual bool	ClientCommand( CBaseTFPlayer *pSender, const CCommand &args );

	// Damage
	void			SetHealth( float flHealth );
	virtual void	TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	virtual int		OnTakeDamage( const CTakeDamageInfo &info );
	bool			PassDamageOntoChildren( const CTakeDamageInfo &info, float *flDamageLeftOver );
	virtual float	GetRepairTime( void );
	virtual bool	UpdateRepair( float flRepairTime );
	virtual bool	Repair( float flHealth );

	// Powerups
	virtual bool	CanPowerupEver( int iPowerup );
	virtual bool	CanPowerupNow( int iPowerup );
	virtual void	PowerupStart( int iPowerup, float flAmount, CBaseEntity *pAttacker, CDamageModifier *pDamageModifier );
	virtual	void	PowerupEnd( int iPowerup );

	// Destruction
	virtual bool	ShouldAutoRemove( void );
	virtual void	Killed( void );
	virtual void	DestroyObject( void );		// Silent cleanup
	virtual bool	IsDying( void ) { return m_bDying; }

	// Data
	virtual Class_T	Classify( void );
	virtual int		GetType( void );
	virtual CBaseTFPlayer *GetBuilder( void );
	virtual CBaseTFPlayer *GetOriginalBuilder( void );
	CTFTeam			*GetTFTeam( void ) { return (CTFTeam*)GetTeam(); };
	
	// ID functions
	virtual bool	IsAnUpgrade( void )			{ return false; }
	virtual bool	IsAVehicle( void )			{ return false; }
	virtual bool	IsSentrygun()				{ return false; }
	virtual bool	WantsCoverFromSentryGun()	{ return false; }
	virtual bool	WantsCover()				{ return false; }

	// Inputs
	void			InputSetHealth( inputdata_t &inputdata );
	void			InputAddHealth( inputdata_t &inputdata );
	void			InputRemoveHealth( inputdata_t &inputdata );
	void			InputSetMinDisabledHealth( inputdata_t &inputdata );
	void			InputSetSolidToPlayer( inputdata_t &inputdata );

	// Pickup
	virtual int		ObjectCaps( void );
	virtual void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void	PickupObject( void );
	virtual bool	CanBeRemovedBy( CBaseTFPlayer *pPlayer );
	virtual bool	CanBeRotatedBy( CBaseTFPlayer *pPlayer );

	virtual void	ChangeTeam( int iTeamNum ) OVERRIDE;			// Assign this entity to a team.

	virtual void	ChangeBuilder( CBaseTFPlayer *pNewBuilder, bool moveobjects );
	virtual const char *GetWeaponClassnameForObject( void );
	virtual void	AddItemsNeededForObject( CBaseTFPlayer *pNewOwner );

	// Objects that are damaged/disabled can return false here when checked for being available
	virtual bool	ComputeEMPDamageState( void ) { return true; }

	// Handling object inactive
	virtual bool	ShouldBeActive( void );

	// Technology
	virtual void	GainedNewTechnology( CBaseTechnology *pTechnology );

	// Sappers
	bool			HasSapper( void );
	bool			HasSapperFromPlayer( CBaseTFPlayer *pPlayer );
	void			AddSapper( CGrenadeObjectSapper *pSapper );
	void			RemoveSapper( CGrenadeObjectSapper *pSapper );

	// Called when the builder rotates this object...
	virtual void	ObjectMoved( );

	// Minibase stuff
	virtual bool	WasMapPlaced( void );
	virtual CRopeKeyframe *ConnectCableTo( CBaseObject *pObject, int iLocalAttachment, int iTargetAttachment );
	virtual bool	HasCableTo( CBaseObject *pObject );
	virtual int		GetCableAttachment( void );

	// Returns the object flags
	int				GetObjectFlags() const { return m_fObjectFlags; }
	void			SetObjectFlags( int flags ) { m_fObjectFlags = flags; }

	CResourceZone	*GetResourceZone() { return m_hResourceZone.Get(); }
	int				RopeCount() const { return m_aRopes.Count(); }

	// Power handling (Human objects need power to operate)
	bool				IsPowered( void );
	void				SetPowerPack( CObjectPowerPack *pPack );
	CObjectPowerPack	*GetPowerPack( void ) { return m_hPowerPack; };

	void			AttemptToGoActive( void );
	virtual void	OnGoActive( void );
	virtual void	OnGoInactive( void );

	// Buffed objects (objects that connect to a medic's buff station)
	bool			IsHookedAndBuffed( void );
	virtual bool	CanBeHookedToBuffStation( void );
	void			SetBuffStation( CObjectBuffStation *pBuffStation, bool bPlacing );
	CObjectBuffStation *GetBuffStation( void );

	virtual void	BuffStationActivate( void );
	virtual void	BuffStationDeactivate( void );

	// Deterioration
	void			StartDeteriorating( void );
	void			StopDeteriorating( void );
	bool			IsDeteriorating( void ) { return m_bDeteriorating; };
	void			DeterioratingThink( void );

	// Disabling
	void			SetDisabled( bool bDisabled );
	bool			IsDisabled( void ) { return m_bDisabled; }

	// Animation
	virtual void	PlayStartupAnimation( void );

	Activity		GetActivity( ) const;
	void			SetActivity( Activity act );
	void			SetObjectSequence( int sequence );
	
	virtual void	OnActivityChanged( Activity act );

	// Object points
	void			SpawnObjectPoints( void );

	// Derive to customize an object's attached version
	virtual	void	SetupAttachedVersion( void ) { return; }
	virtual void	SetupUnattachedVersion( void ) { return; }

	QAngle			ConvertAbsAnglesToLocal( QAngle vecLocalAngles );

public:
	// VulnerablePoints
	void				CreateVulnerablePoints( void );
	void				AddVulnerablePoint( const char* szHitboxName, float Multiplier );
	float				FindVulnerablePointMultiplier( int nGroup, int nBox ); 		

	// Build points
	CUtlVector<VulnerablePoint_t>	m_VulnerablePoints;

	virtual bool TestHitboxes( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );

public:
	// Client/Server shared build point code
	void				CreateBuildPoints( void );
	void				AddAndParseBuildPoint( int iAttachmentNumber, KeyValues *pkvBuildPoint );
	virtual int			AddBuildPoint( int iAttachmentNum );
	virtual void		AddValidObjectToBuildPoint( int iPoint, int iObjectType );
	virtual CBaseObject *GetBuildPointObject( int iPoint );
	bool				IsBuiltOnAttachment( void ) { return (m_hBuiltOnEntity.Get() != NULL); }
	void				AttachObjectToObject( const CBaseEntity *pEntity, int iPoint, Vector &vecOrigin );
	void				DetachObjectFromObject( void );
	CBaseObject			*GetParentObject( void );
	void				SetBuildPointPassenger( int iPoint, int iPassenger );
	int					GetBuildPointPassenger( int iPoint ) const;

	virtual float		GetSapperAttachTime( void );

// IHasBuildPoints
public:
	virtual int			GetNumBuildPoints( void ) const;
	virtual bool		GetBuildPoint( int iPoint, Vector &vecOrigin, QAngle &vecAngles );
	virtual int			GetBuildPointAttachmentIndex( int iPoint ) const;
	virtual bool		CanBuildObjectOnBuildPoint( int iPoint, int iObjectType );
	virtual void		SetObjectOnBuildPoint( int iPoint, CBaseObject *pObject );
	virtual float		GetMaxSnapDistance( int iBuildPoint );
	virtual bool		ShouldCheckForMovement( void ) { return true; }
	virtual int			GetNumObjectsOnMe( void );
	virtual CBaseEntity	*GetFirstObjectOnMe( void );
	virtual CBaseObject *GetObjectOfTypeOnMe( int iObjectType );
	virtual void		RemoveAllObjects( void );


// IServerNetworkable.
public:

	virtual int		ShouldTransmit( const CCheckTransmitInfo *pInfo );
	virtual void	SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways );


protected:
	// Clean off the object of offensive material, returns true if it found anything
	bool RemoveEnemyAttachments( CBaseEntity *pActivator );
	void RemoveAnalyzer( CBaseEntity *pRemovingEntity );
	void RemoveAllSappers( CBaseEntity *pRemovingEntity );

	void GiveNamedTechnology( CBaseTFPlayer *pRecipient, const char *techname );

	// Show/hide vgui screens.
	bool ShowVGUIScreen( int panelIndex, bool bShow );

private:
	void DetermineAnimation( void );

	// Spawns the various control panels
	void SpawnControlPanels();

	// Purpose: Spawn any objects specified inside the mdl
	void SpawnEntityOnBuildPoint( const char *pEntityName, int iAttachmentNumber );

	// Various commands sent by control panels
	void DismantleCommand( CBaseTFPlayer *pSender );
	void YawCommand( CBaseTFPlayer *pSender, float flYaw );
	void TakeControlCommand( CBaseTFPlayer *pSender );

protected:
	enum OBJSOLIDTYPE
	{
		SOLID_TO_PLAYER_USE_DEFAULT = 0,
		SOLID_TO_PLAYER_YES,
		SOLID_TO_PLAYER_NO,
	};

	bool		IsSolidToPlayers( void ) const;

	// object flags....
	CNetworkVar( int, m_fObjectFlags );
	CNetworkHandle( CBaseTFPlayer,	m_hBuilder );

	// Zone we're in (valid only for objects that sit in zones)
	CNetworkHandle( CResourceZone, m_hResourceZone );

	// Combat Objects
	char	*m_szAmmoName;		// Ammo used by players to build me

	// Cables
	CUtlVector< CHandle<CRopeKeyframe> > m_aRopes;

	// Placement
	Vector			m_vecBuildOrigin;
	Vector			m_vecBuildMins;
	Vector			m_vecBuildMaxs;
	CNetworkHandle( CBaseEntity, m_hBuiltOnEntity );
	int				m_iBuiltOnPoint;

	bool	m_bInvulnerable;
	bool	m_bCantDie;
	float	m_flRepairMultiplier;
	bool	m_bDying;

	// Outputs
	COutputEvent m_OnDestroyed;
	COutputEvent m_OnDamaged;
	COutputEvent m_OnRepaired;

	COutputEvent m_OnBecomingDisabled;
	COutputEvent m_OnBecomingReenabled;

	COutputFloat m_OnObjectHealthChanged;

	// Control panel
	typedef CHandle<CVGuiScreen>	ScreenHandle_t;
	CUtlVector<ScreenHandle_t>	m_hScreens;

private:
	// Make sure we pick up changes to these.
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_iHealth );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_takedamage );

	Activity	m_Activity;

	CNetworkVar( int, m_iObjectType );


	// True if players shouldn't do collision avoidance, but should just collide exactly with the object.
	OBJSOLIDTYPE	m_SolidToPlayers;
	void		SetSolidToPlayers( OBJSOLIDTYPE stp, bool force = false );

	// True if this was a map placed object, not a player built one
	bool	m_bWasMapPlaced;

	// Disabled
	CNetworkVar( bool, m_bDisabled );

	// Vehicle bays
	CHandle<CVGuiScreenVehicleBay>	m_hVehicleBay;

	// Building
	CNetworkVar( bool, m_bPlacing );					// True while the object's being placed
	CNetworkVar( bool, m_bBuilding );				// True while the object's still constructing itself
	float	m_flConstructionTimeLeft;	// Current time left in construction
	float	m_flTotalConstructionTime;	// Total construction time (the value of GetTotalTime() at the time construction 
										// started, ie, incase you teleport out of a construction yard)

	CNetworkVar( float, m_flPercentageConstructed );	// Used to send to client
	float	m_flHealth;					// Health during construction. Needed a float due to small increases in health.

	// Repair capping
	float	m_flLastRepairTime;
	float	m_flNextRepairMultiplier;
	float	m_flRepairedSinceLastTime;

	// Sappers on me
	CNetworkVar( bool, m_bHasSapper );
	typedef CHandle<CGrenadeObjectSapper>	SapperHandle;
	CUtlVector< SapperHandle >	m_hSappers;

	// Power handling (Human objects need power to operate)
	CHandle< CObjectPowerPack >	m_hPowerPack;

	// Buff Station
	CHandle< CObjectBuffStation >	m_hBuffStation;
	bool							m_bBuffActivated;

	// Deterioration
	CNetworkVar( bool, m_bDeteriorating );
	float	m_flStartedDeterioratingAt;
	CHandle<CBaseTFPlayer>	m_hOriginalBuilder;

	// Build points
	CUtlVector<BuildPoint_t>	m_BuildPoints;

	// Store the last time I took damage from an enemy. Use this to know whether to drop resources
	// when I die, because I only want to drop resources if I was "destroyed" by an enemy, not if I deteriorated.
	float	m_flLastRealDamage;

	// Amount of resources the player who built me paid for me
	int		m_iAmountPlayerPaidForMe;

	// Attack notification sounds
	string_t	m_iszUnderAttackSound;

	// If non-zero then if health gets below this amount, the object becomes disabled
	float		m_flMinDisableHealth;

	// If not NULL, then when going disabled, swith to this model
	string_t	m_iszDisabledModel;
	string_t	m_iszEnabledModel;
};

inline bool	CBaseObject::CanBeHookedToBuffStation( void )
{ 
	return false; 
}

inline CObjectBuffStation *CBaseObject::GetBuffStation( void )
{
	return m_hBuffStation.Get(); 
}

inline void	CBaseObject::BuffStationActivate( void )
{ 
	m_bBuffActivated = true; 
}

inline void	CBaseObject::BuffStationDeactivate( void )
{ 
	m_bBuffActivated = false; 
}

extern short g_sModelIndexFireball;		// holds the index for the fireball


#endif // TF_OBJ_H
