//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
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
#include "ihasbuildpoints.h"
#include "iscorer.h"
#include "baseobject_shared.h"
#include "utlmap.h"
#include "props_shared.h"

class CTFPlayer;
class CTFTeam;
class CRopeKeyframe;
class CVGuiScreen;
class KeyValues;
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
class CBaseObject : public CBaseCombatCharacter, public IHasBuildPoints, public IScorer
{
	DECLARE_CLASS( CBaseObject, CBaseCombatCharacter );
public:
	CBaseObject();

	virtual void	UpdateOnRemove( void );

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual bool	IsBaseObject( void ) const { return true; }

	virtual void	BaseObjectThink( void );
	//virtual void	LostPowerThink( void );
	virtual CTFPlayer *GetOwner( void );

	// Creation
	virtual void	Precache();
	virtual void	Spawn( void );
	virtual void	Activate( void );

	virtual bool	ShouldCollide( int collisionGroup, int contentsMask ) const;

	virtual void	SetBuilder( CTFPlayer *pBuilder );
	virtual void	SetType( int iObjectType );
	int				ObjectType( ) const;

	virtual int		BloodColor( void ) { return BLOOD_COLOR_MECH; }

	// Building
	virtual float	GetTotalTime( void );
	virtual void	StartPlacement( CTFPlayer *pPlayer );
	void			StopPlacement( void );
	bool			FindNearestBuildPoint( CBaseEntity *pEntity, CBasePlayer *pBuilder, float &flNearestPoint, Vector &vecNearestBuildPoint );
	bool			VerifyCorner( const Vector &vBottomCenter, float xOffset, float yOffset );
	virtual float	GetNearbyObjectCheckRadius( void ) { return 30.0; }
	bool			UpdatePlacement( void );
	bool			UpdateAttachmentPlacement( void );
	bool			IsValidPlacement( void ) const;
	bool			EstimateValidBuildPos( void );

	bool			CalculatePlacementPos( void );
	virtual bool	IsPlacementPosValid( void );
	bool			FindSnapToBuildPos( void );

	void			ReattachChildren( void );
	
	// I've finished building the specified object on the specified build point
	virtual int		FindObjectOnBuildPoint( CBaseObject *pObject );
	
	virtual bool	StartBuilding( CBaseEntity *pPlayer );
	void			BuildingThink( void );
	void			SetControlPanelsActive( bool bState );
	virtual void	FinishedBuilding( void );
	bool			IsBuilding( void ) { return m_bBuilding; };
	bool			IsPlacing( void ) { return m_bPlacing; };
	virtual bool	IsUpgrading( void ) const { return false; }
	bool			MustBeBuiltOnAttachmentPoint( void ) const;

	// Returns information about the various control panels
	virtual void 	GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
	virtual void	GetControlPanelClassName( int nPanelIndex, const char *&pPanelName );

	// Client commands sent by clicking on various panels....
	// NOTE: pArg->Argv(0) == pCmd, pArg->Argv(1) == the first argument
	virtual bool	ClientCommand( CTFPlayer *pSender, const CCommand &args );

	// Damage
	void			SetHealth( float flHealth );
	virtual void	TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	virtual int		OnTakeDamage( const CTakeDamageInfo &info );
	bool			PassDamageOntoChildren( const CTakeDamageInfo &info, float *flDamageLeftOver );
	virtual bool	Repair( float flHealth );

	void			OnRepairHit( CTFPlayer *pPlayer );
	float			GetRepairMultiplier( void );

	// Destruction
	virtual void	DetonateObject( void );
	virtual bool	ShouldAutoRemove( void );
	virtual void	Explode( void );
	virtual void	Killed( const CTakeDamageInfo &info );
	virtual void	DestroyObject( void );		// Silent cleanup
	virtual bool	IsDying( void ) { return m_bDying; }
	void DestroyScreens( void );

	// Data
	virtual Class_T	Classify( void );
	virtual int		GetType( void );
	virtual CTFPlayer *GetBuilder( void );
	CTFTeam			*GetTFTeam( void ) { return (CTFTeam*)GetTeam(); };
	
	// ID functions
	virtual bool	IsAnUpgrade( void )			{ return false; }
	virtual bool	IsHostileUpgrade( void )	{ return false; }	// Attaches to enemy buildings

	// Inputs
	void			InputSetHealth( inputdata_t &inputdata );
	void			InputAddHealth( inputdata_t &inputdata );
	void			InputRemoveHealth( inputdata_t &inputdata );
	void			InputSetSolidToPlayer( inputdata_t &inputdata );

	// Wrench hits
	bool			InputWrenchHit( CTFPlayer *pPlayer );
	virtual bool	OnWrenchHit( CTFPlayer *pPlayer );
	virtual bool	Command_Repair( CTFPlayer *pActivator );

	virtual void	ChangeTeam( int iTeamNum );			// Assign this entity to a team.

	// Handling object inactive
	virtual bool	ShouldBeActive( void );

	// Sappers
	bool			HasSapper( void );
	void			OnAddSapper( void );
	void			OnRemoveSapper( void );

	// Returns the object flags
	int				GetObjectFlags() const { return m_fObjectFlags; }
	void			SetObjectFlags( int flags ) { m_fObjectFlags = flags; }

	void			AttemptToGoActive( void );
	virtual void	OnGoActive( void );
	virtual void	OnGoInactive( void );

	// Disabling
	bool			IsDisabled( void ) { return m_bDisabled; }
	void			UpdateDisabledState( void );
	void			SetDisabled( bool bDisabled );
	virtual void	OnStartDisabled( void );
	virtual void	OnEndDisabled( void );

	// Animation
	virtual void	PlayStartupAnimation( void );

	Activity		GetActivity( ) const;
	void			SetActivity( Activity act );
	void			SetObjectSequence( int sequence );
	
	// Object points
	void			SpawnObjectPoints( void );

	// Derive to customize an object's attached version
	virtual	void	SetupAttachedVersion( void ) { return; }

	virtual int		DrawDebugTextOverlays( void );

	void			RotateBuildAngles( void );

	virtual bool	ShouldPlayersAvoid( void );

	void			IncrementKills( void ) { m_iKills++; }
	int				GetKills() { return m_iKills; }

	void			CreateObjectGibs( void );
	virtual void	SetModel( const char *pModel );

	const char		*GetResponseRulesModifier( void );

public:		

	virtual bool TestHitboxes( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );

public:
	// IScorer interface
	virtual CBasePlayer *GetScorer( void ) { return ( CBasePlayer *)GetBuilder(); }
	virtual CBasePlayer *GetAssistant( void ) { return NULL; }

public:
	// Client/Server shared build point code
	void				CreateBuildPoints( void );
	void				AddAndParseBuildPoint( int iAttachmentNumber, KeyValues *pkvBuildPoint );
	virtual int			AddBuildPoint( int iAttachmentNum );
	virtual void		AddValidObjectToBuildPoint( int iPoint, int iObjectType );
	virtual CBaseObject *GetBuildPointObject( int iPoint );
	bool				IsBuiltOnAttachment( void ) { return (m_hBuiltOnEntity.Get() != NULL); }
	void				AttachObjectToObject( CBaseEntity *pEntity, int iPoint, Vector &vecOrigin );
	virtual void		DetachObjectFromObject( void );
	CBaseObject			*GetParentObject( void );

// IHasBuildPoints
public:
	virtual int			GetNumBuildPoints( void ) const;
	virtual bool		GetBuildPoint( int iPoint, Vector &vecOrigin, QAngle &vecAngles );
	virtual int			GetBuildPointAttachmentIndex( int iPoint ) const;
	virtual bool		CanBuildObjectOnBuildPoint( int iPoint, int iObjectType );
	virtual void		SetObjectOnBuildPoint( int iPoint, CBaseObject *pObject );
	virtual float		GetMaxSnapDistance( int iBuildPoint );
	virtual bool		ShouldCheckForMovement( void ) { return true; }
	virtual int			GetNumObjectsOnMe();
	virtual CBaseEntity	*GetFirstFriendlyObjectOnMe( void );
	virtual CBaseObject *GetObjectOfTypeOnMe( int iObjectType );
	virtual void		RemoveAllObjects( void );


// IServerNetworkable.
public:

	virtual int		UpdateTransmitState( void );
	virtual int		ShouldTransmit( const CCheckTransmitInfo *pInfo );
	virtual void	SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways );


protected:
	// Show/hide vgui screens.
	bool ShowVGUIScreen( int panelIndex, bool bShow );

	// Spawns the various control panels
	void SpawnControlPanels();

	virtual void DetermineAnimation( void );
	virtual void DeterminePlaybackRate( void );

	void UpdateDesiredBuildRotation( float flFrameTime );

private:
	// Purpose: Spawn any objects specified inside the mdl
	void SpawnEntityOnBuildPoint( const char *pEntityName, int iAttachmentNumber );

	bool TestAgainstRespawnRoomVisualizer( CTFPlayer *pPlayer, const Vector &vecEnd );

	//bool TestPositionForPlayerBlock( Vector vecBuildOrigin, CBasePlayer *pPlayer );
	//void RecursiveTestBuildSpace( int iNode, bool *bNodeClear, bool *bNodeVisited );

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
	CNetworkHandle( CTFPlayer,	m_hBuilder );

	// Placement
	Vector			m_vecBuildOrigin;
	CNetworkVector( m_vecBuildMaxs );
	CNetworkVector( m_vecBuildMins );
	CNetworkHandle( CBaseEntity, m_hBuiltOnEntity );
	int				m_iBuiltOnPoint;

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

	// Disabled
	CNetworkVar( bool, m_bDisabled );

	// Building
	CNetworkVar( bool, m_bPlacing );					// True while the object's being placed
	CNetworkVar( bool, m_bBuilding );				// True while the object's still constructing itself
	float	m_flConstructionTimeLeft;	// Current time left in construction
	float	m_flTotalConstructionTime;	// Total construction time (the value of GetTotalTime() at the time construction 
										// started, ie, incase you teleport out of a construction yard)

	CNetworkVar( float, m_flPercentageConstructed );	// Used to send to client
	float	m_flHealth;					// Health during construction. Needed a float due to small increases in health.

	// Sapper on me
	CNetworkVar( bool, m_bHasSapper );

	// Build points
	CUtlVector<BuildPoint_t>	m_BuildPoints;

	// Maps player ent index to repair expire time
	CUtlMap<int, float>	m_RepairerList;

	// Result of last placement test
	bool		m_bPlacementOK;				// last placement state

	CNetworkVar( int, m_iKills );

	CNetworkVar( int, m_iDesiredBuildRotations );		// Number of times we've rotated, used to calc final rotation
	float m_flCurrentBuildRotation;

	// Gibs.
	CUtlVector<breakmodel_t>	m_aGibs;

	CNetworkVar( bool, m_bServerOverridePlacement );
};

extern short g_sModelIndexFireball;		// holds the index for the fireball

#endif // TF_OBJ_H
