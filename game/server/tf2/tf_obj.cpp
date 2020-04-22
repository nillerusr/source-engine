//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Base Object built by players
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "tf_player.h"
#include "tf_team.h"
#include "tf_obj.h"
#include "tf_basecombatweapon.h"
#include "rope.h"
#include "rope_shared.h"
#include "bone_setup.h"
#include "tf_func_resource.h"
#include "ndebugoverlay.h"
#include "rope_helpers.h"
#include "IEffects.h"
#include "vstdlib/random.h"
#include "tier1/strtools.h"
#include "basegrenade_shared.h"
#include "grenade_objectsapper.h"
#include "tf_stats.h"
#include "tf_gamerules.h"
#include "engine/IEngineSound.h"
#include "tf_obj_sentrygun.h"
#include "tf_obj_powerpack.h"
#include "tf_shareddefs.h"
#include "VGuiScreen.h"
#include "resource_chunk.h"
#include "hierarchy.h"
#include "tf_func_construction_yard.h"
#include "tf_func_no_build.h"
#include <KeyValues.h>
#include "team_messages.h"
#include "info_act.h"
#include "info_vehicle_bay.h"
#include "ihasbuildpoints.h"
#include "tf_obj_buff_station.h"
#include "info_buildpoint.h"
#include "utldict.h"
#include "filesystem.h"
#include "npcevent.h"
#include "tf_shareddefs.h"
#include "animation.h"

// Control panels
#define SCREEN_OVERLAY_MATERIAL "vgui/screens/vgui_overlay"

#define ROPE_HANG_DIST	150


ConVar object_verbose( "object_verbose", "0", 0, "Debug object system." );
ConVar obj_damage_factor( "obj_damage_factor","0", FCVAR_NONE, "Factor applied to all damage done to objects" );
ConVar obj_child_damage_factor( "obj_child_damage_factor","0.25", FCVAR_NONE, "Factor applied to damage done to objects that are built on a buildpoint" );
ConVar tf_fastbuild("tf_fastbuild", "0", FCVAR_CHEAT);
ConVar tf_obj_ground_clearance( "tf_obj_ground_clearance", "60", 0, "Object corners can be this high above the ground" );

extern short g_sModelIndexFireball;

// Minimum distance between 2 objects to ensure player movement between them
#define MINIMUM_OBJECT_SAFE_DISTANCE		100

// Maximum number of a type of objects on a single resource zone
#define MAX_OBJECTS_PER_ZONE			1

// Time it takes a fully healed object to deteriorate
ConVar  object_deterioration_time( "object_deterioration_time", "30", 0, "Time it takes for a fully-healed object to deteriorate." );

// Time after taking damage that an object will still drop resources on death
#define MAX_DROP_TIME_AFTER_DAMAGE			5

#define OBJ_BASE_THINK_CONTEXT				"BaseObjectThink"
#define OBJ_LOSTPOWER_THINK_CONTEXT			"LostPowerThink"

BEGIN_DATADESC( CBaseObject )
	// keys 
	DEFINE_KEYFIELD_NOT_SAVED( m_bInvulnerable,		FIELD_BOOLEAN, "Invulnerable" ),
	DEFINE_KEYFIELD_NOT_SAVED( m_flRepairMultiplier,	FIELD_FLOAT, "RepairMult" ),
	DEFINE_KEYFIELD_NOT_SAVED( m_iszUnderAttackSound,	FIELD_STRING, "AttackNotify" ), 
	DEFINE_KEYFIELD_NOT_SAVED( m_flMinDisableHealth,	FIELD_FLOAT, "MinDisabledHealth" ),
	DEFINE_KEYFIELD_NOT_SAVED( m_SolidToPlayers,		FIELD_INTEGER, "SolidToPlayer" ),
	DEFINE_KEYFIELD_NOT_SAVED( m_iszDisabledModel,		FIELD_STRING, "DisabledModel" ),
	DEFINE_KEYFIELD_NOT_SAVED( m_bCantDie,				FIELD_BOOLEAN, "CantDie" ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetHealth", InputSetHealth ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "AddHealth", InputAddHealth ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "RemoveHealth", InputRemoveHealth ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetMinDisabledHealth", InputSetMinDisabledHealth ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetSolidToPlayer", InputSetSolidToPlayer ),

	// Outputs
	DEFINE_OUTPUT( m_OnDestroyed, "OnDestroyed" ),
	DEFINE_OUTPUT( m_OnDamaged, "OnDamaged" ),
	DEFINE_OUTPUT( m_OnRepaired, "OnRepaired" ),
	DEFINE_OUTPUT( m_OnBecomingDisabled, "OnDisabled" ),
	DEFINE_OUTPUT( m_OnBecomingReenabled, "OnReenabled" ),
	DEFINE_OUTPUT( m_OnObjectHealthChanged, "OnObjectHealthChanged" )
END_DATADESC()


IMPLEMENT_SERVERCLASS_ST(CBaseObject, DT_BaseObject)
	SendPropInt(SENDINFO(m_iHealth), 13 ),
	SendPropInt(SENDINFO(m_iMaxHealth), 13 ),
	SendPropInt(SENDINFO(m_bHasSapper), 1, SPROP_UNSIGNED ),
	SendPropInt(SENDINFO(m_iObjectType),  6, SPROP_UNSIGNED ),
	SendPropInt(SENDINFO(m_bBuilding), 1, SPROP_UNSIGNED ),
	SendPropInt(SENDINFO(m_bPlacing), 1, SPROP_UNSIGNED ),
	SendPropFloat(SENDINFO(m_flPercentageConstructed), 8, 0, 0.0, 1.0f ),
	SendPropInt(SENDINFO(m_fObjectFlags), OF_BIT_COUNT, SPROP_UNSIGNED ),
	SendPropInt(SENDINFO(m_bDeteriorating), 1, SPROP_UNSIGNED ),
	SendPropEHandle(SENDINFO(m_hBuiltOnEntity)),
	SendPropInt( SENDINFO( m_takedamage ), 2, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_bDisabled ), 1, SPROP_UNSIGNED ),
	SendPropEHandle( SENDINFO( m_hBuilder ) ),
END_SEND_TABLE();


// This controls whether ropes attached to objects are transmitted or not. It's important that
// ropes aren't transmitted to guys who don't own them.
class CObjectRopeTransmitProxy : public CBaseTransmitProxy
{
public:
	CObjectRopeTransmitProxy( CBaseEntity *pRope ) : CBaseTransmitProxy( pRope )
	{
	}
	
	virtual int ShouldTransmit( const CCheckTransmitInfo *pInfo, int nPrevShouldTransmitResult )
	{
		// Don't transmit the rope if it's not even visible.
		if ( !nPrevShouldTransmitResult )
			return FL_EDICT_DONTSEND;

		// This proxy only wants to be active while one of the two objects is being placed.
		// When they're done being placed, the proxy goes away and the rope draws like normal.
		bool bAnyObjectPlacing = (m_hObj1 && m_hObj1->IsPlacing()) || (m_hObj2 && m_hObj2->IsPlacing());
		if ( !bAnyObjectPlacing )
		{
			Release();
			return nPrevShouldTransmitResult;
		}

		// Give control to whichever object is being placed.
		if ( m_hObj1 && m_hObj1->IsPlacing() )
			return m_hObj1->ShouldTransmit( pInfo );
		
		else if ( m_hObj2 && m_hObj2->IsPlacing() )
			return m_hObj2->ShouldTransmit( pInfo );
		
		else
			return FL_EDICT_ALWAYS;
	}


	CHandle<CBaseObject> m_hObj1;
	CHandle<CBaseObject> m_hObj2;
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseObject::CBaseObject()
{
	m_hPowerPack = NULL;
	m_iHealth = m_iMaxHealth = m_flHealth = 0;
	m_aRopes.Purge();
	m_flPercentageConstructed = 0;
	m_bPlacing = false;
	m_bBuilding = false;
	m_bInvulnerable = false;
	m_bCantDie = false;
	m_bDeteriorating = false;
	m_flRepairMultiplier = 1;
	m_hBuffStation = NULL;
	m_bBuffActivated = false;
	m_Activity = ACT_OBJ_IDLE;
	m_bDisabled = false;
	m_hVehicleBay = NULL;
	m_flLastRepairTime = 0;
	m_flNextRepairMultiplier = 0;
	m_flRepairedSinceLastTime = 0;
	m_iszUnderAttackSound = NULL_STRING;
	m_SolidToPlayers = SOLID_TO_PLAYER_USE_DEFAULT;
	m_iszDisabledModel = NULL_STRING;
	m_iszEnabledModel = NULL_STRING;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseObject::UpdateOnRemove( void )
{
	m_bDying = true;

	// Remove anything left on me
	IHasBuildPoints *pBPInterface = dynamic_cast<IHasBuildPoints*>(this);
	if ( pBPInterface && pBPInterface->GetNumObjectsOnMe() )
	{
		pBPInterface->RemoveAllObjects();
	}

	DestroyObject();
	
	if ( GetTeam() )
	{
		((CTFTeam*)GetTeam())->RemoveObject( this );
	}

	// Make sure the object isn't in either team's list of objects...
	Assert( !GetGlobalTFTeam(1)->IsObjectOnTeam( this ) );
	Assert( !GetGlobalTFTeam(2)->IsObjectOnTeam( this ) );

	// Chain at end to mimic destructor unwind order
	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseObject::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	// Always transmit to owner
	if ( GetBuilder() && pInfo->m_pClientEnt == GetBuilder()->edict() )
		return FL_EDICT_ALWAYS;

	// Placement models only transmit to owners
	if ( IsPlacing() )
		return FL_EDICT_DONTSEND;

	return BaseClass::ShouldTransmit( pInfo );
}


void CBaseObject::SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways )
{
	// Are we already marked for transmission?
	if ( pInfo->m_pTransmitEdict->Get( entindex() ) )
		return;

	BaseClass::SetTransmit( pInfo, bAlways );

	// Force our screens to be sent too.
	int nTeam = CBaseEntity::Instance( pInfo->m_pClientEnt )->GetTeamNumber();
	for ( int i=0; i < m_hScreens.Count(); i++ )
	{
		CVGuiScreen *pScreen = m_hScreens[i].Get();
		if ( pScreen && pScreen->IsVisibleToTeam( nTeam ) )
			pScreen->SetTransmit( pInfo, bAlways );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseObject::Precache()
{
	PrecacheVGuiScreen( "screen_basic_with_disable" );

	if ( m_iszUnderAttackSound != NULL_STRING )
	{
		PrecacheScriptSound( STRING(m_iszUnderAttackSound) );
	}
	PrecacheMaterial( SCREEN_OVERLAY_MATERIAL );

	if ( m_iszDisabledModel != NULL_STRING )
	{
		PrecacheModel( STRING( m_iszDisabledModel ) );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseObject::Spawn( void )
{
	Precache();

	CollisionProp()->SetSurroundingBoundsType( USE_BEST_COLLISION_BOUNDS );
	SetSolidToPlayers( m_SolidToPlayers, true );

	m_bWasMapPlaced = false;
	m_bHasSapper = false;
	m_takedamage = DAMAGE_YES;
	m_flHealth = m_iMaxHealth = m_iHealth;
	m_iAmountPlayerPaidForMe = CalculateObjectCost( GetType(), 0, GetTeamNumber() );

	SetContextThink( BaseObjectThink, gpGlobals->curtime + 0.1, OBJ_BASE_THINK_CONTEXT );
	m_szAmmoName = NULL;

	AddFlag( FL_OBJECT ); // So NPCs will notice it
	SetViewOffset( WorldSpaceCenter() - GetAbsOrigin() );

	// Don't take damage if we're invulnerable, and don't require power either
	if ( m_bInvulnerable )
	{
		m_takedamage = DAMAGE_NO;
		m_fObjectFlags |= OF_DOESNT_NEED_POWER;
		AddFlag( FL_NOTARGET );
	}

	// Cache off the normal model name
	m_iszEnabledModel = GetModelName();
}


//-----------------------------------------------------------------------------
// Returns information about the various control panels
//-----------------------------------------------------------------------------
void CBaseObject::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = NULL;
}

//-----------------------------------------------------------------------------
// Returns information about the various control panels
//-----------------------------------------------------------------------------
void CBaseObject::GetControlPanelClassName( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "vgui_screen";
}

//-----------------------------------------------------------------------------
// This is called by the base object when it's time to spawn the control panels
//-----------------------------------------------------------------------------
void CBaseObject::SpawnControlPanels()
{
	char buf[64];

	// FIXME: Deal with dynamically resizing control panels?

	// If we're attached to an entity, spawn control panels on it instead of use
	CBaseAnimating *pEntityToSpawnOn = this;
	char *pOrgLL = "controlpanel%d_ll";
	char *pOrgUR = "controlpanel%d_ur";
	char *pAttachmentNameLL = pOrgLL;
	char *pAttachmentNameUR = pOrgUR;
	if ( IsBuiltOnAttachment() )
	{
		pEntityToSpawnOn = dynamic_cast<CBaseAnimating*>((CBaseEntity*)m_hBuiltOnEntity.Get());
		if ( pEntityToSpawnOn )
		{
			char sBuildPointLL[64];
			char sBuildPointUR[64];
			Q_snprintf( sBuildPointLL, sizeof( sBuildPointLL ), "bp%d_controlpanel%%d_ll", m_iBuiltOnPoint );
			Q_snprintf( sBuildPointUR, sizeof( sBuildPointUR ), "bp%d_controlpanel%%d_ur", m_iBuiltOnPoint );
			pAttachmentNameLL = sBuildPointLL;
			pAttachmentNameUR = sBuildPointUR;
		}
		else
		{
			pEntityToSpawnOn = this;
		}
	}

	Assert( pEntityToSpawnOn );

	// Lookup the attachment point...
	int nPanel;
	for ( nPanel = 0; true; ++nPanel )
	{
		Q_snprintf( buf, sizeof( buf ), pAttachmentNameLL, nPanel );
		int nLLAttachmentIndex = pEntityToSpawnOn->LookupAttachment(buf);
		if (nLLAttachmentIndex <= 0)
		{
			// Try and use my panels then
			pEntityToSpawnOn = this;
			Q_snprintf( buf, sizeof( buf ), pOrgLL, nPanel );
			nLLAttachmentIndex = pEntityToSpawnOn->LookupAttachment(buf);
			if (nLLAttachmentIndex <= 0)
				return;
		}

		Q_snprintf( buf, sizeof( buf ), pAttachmentNameUR, nPanel );
		int nURAttachmentIndex = pEntityToSpawnOn->LookupAttachment(buf);
		if (nURAttachmentIndex <= 0)
		{
			// Try and use my panels then
			Q_snprintf( buf, sizeof( buf ), pOrgUR, nPanel );
			nURAttachmentIndex = pEntityToSpawnOn->LookupAttachment(buf);
			if (nURAttachmentIndex <= 0)
				return;
		}

		const char *pScreenName;
		GetControlPanelInfo( nPanel, pScreenName );
		if (!pScreenName)
			continue;

		const char *pScreenClassname;
		GetControlPanelClassName( nPanel, pScreenClassname );
		if ( !pScreenClassname )
			continue;

		// Compute the screen size from the attachment points...
		matrix3x4_t	panelToWorld;
		pEntityToSpawnOn->GetAttachment( nLLAttachmentIndex, panelToWorld );

		matrix3x4_t	worldToPanel;
		MatrixInvert( panelToWorld, worldToPanel );

		// Now get the lower right position + transform into panel space
		Vector lr, lrlocal;
		pEntityToSpawnOn->GetAttachment( nURAttachmentIndex, panelToWorld );
		MatrixGetColumn( panelToWorld, 3, lr );
		VectorTransform( lr, worldToPanel, lrlocal );

		float flWidth = lrlocal.x;
		float flHeight = lrlocal.y;

		CVGuiScreen *pScreen = CreateVGuiScreen( pScreenClassname, pScreenName, pEntityToSpawnOn, this, nLLAttachmentIndex );
		pScreen->ChangeTeam( GetTeamNumber() );
		pScreen->SetActualSize( flWidth, flHeight );
		pScreen->SetActive( false );
		pScreen->MakeVisibleOnlyToTeammates( true );
		pScreen->SetOverlayMaterial( SCREEN_OVERLAY_MATERIAL );
		int nScreen = m_hScreens.AddToTail( );
		m_hScreens[nScreen].Set( pScreen );
	}
}


//-----------------------------------------------------------------------------
// Various commands sent by control panels
//-----------------------------------------------------------------------------
void CBaseObject::DismantleCommand( CBaseTFPlayer *pSender )
{
	if (CanBeRemovedBy( pSender ))
	{
		PickupObject();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseObject::YawCommand( CBaseTFPlayer *pSender, float flYaw )
{
	if ( CanBeRotatedBy(pSender) )
	{
		QAngle angles = GetAbsAngles();

		angles.y = anglemod( flYaw );
		SetLocalAngles( ConvertAbsAnglesToLocal( angles ) );
		Teleport( NULL, &GetLocalAngles(), NULL );

		// Notify the object that it moved
		ObjectMoved();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseObject::TakeControlCommand( CBaseTFPlayer *pSender )
{
	// Deteriorating objects can be bought
	if ( InSameTeam( pSender ) && IsDeteriorating() )
	{
		if ( ClassCanBuild( pSender->PlayerClass(), GetType() ) )
		{
			// Make sure he has the resources
			int iCost = CalculateObjectCost( GetType(), pSender->GetNumObjects( GetType() ), GetTeamNumber() );
			if ( pSender->GetBankResources() >= iCost )
			{
				pSender->RemoveBankResources( iCost );
				SetBuilder( pSender );
				pSender->AddObject( this );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Handle commands sent from vgui panels on the client 
//-----------------------------------------------------------------------------
bool CBaseObject::ClientCommand( CBaseTFPlayer *pSender, const char *pCmd, ICommandArguments *pArg )
{
	if ( FStrEq( pCmd, "dismantle" ) )
	{
		DismantleCommand( pSender );
		return true;
	}

	if ( FStrEq( pCmd, "yaw" ) )
	{
		if ( pArg->Argc() == 2 )
		{
			float flYaw = atof( pArg->Argv(1) );
			YawCommand( pSender, flYaw );
		}
		return true;
	}

	if ( FStrEq( pCmd, "takecontrol" ) )
	{
		TakeControlCommand( pSender );
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseObject::BaseObjectThink( void )
{
	SetNextThink( gpGlobals->curtime + 0.1, OBJ_BASE_THINK_CONTEXT );

	// Make sure animation is up to date
	DetermineAnimation();

	// Can't animate without a model
	if ( !(m_fObjectFlags & OF_DOESNT_HAVE_A_MODEL) )
	{
		StudioFrameAdvance();
	}

	/*
	ROBIN: Hierarchy should do this for us

	// If we were built on an attachment that's moved, update our position
	if ( !IsPlacing() && IsBuiltOnAttachment() )
	{
		IHasBuildPoints *pBPInterface = dynamic_cast<IHasBuildPoints*>((CBaseEntity*)m_hBuiltOnEntity);
		Assert( pBPInterface );

		if ( pBPInterface->ShouldCheckForMovement() )
		{
			Vector vecOrigin;
			QAngle vecAngles;
			pBPInterface->GetBuildPoint( m_iBuiltOnPoint, vecOrigin, vecAngles );

			EntityMatrix vehicleToWorld, childMatrix;
			vehicleToWorld.InitFromEntity( GetMoveParent() ); // parent->world
			vecOrigin = vehicleToWorld.WorldToLocal( vecOrigin );

			if ( vecOrigin != GetLocalOrigin() )
			{
				Teleport( &vecOrigin, NULL, NULL );
			}
		}
	}
	*/

	// Do nothing while we're being placed
	if ( IsPlacing() )
	{
		for ( int i=0; i < m_aRopes.Count(); i++ )
		{
			if ( m_aRopes[i].Get() )
				m_aRopes[i]->SetupHangDistance( ROPE_HANG_DIST );
		}

		return;
	}

	// If we're deteriorating, keep going
	if ( IsDeteriorating() )
	{
		DeterioratingThink();
	}

	// If we're building, keep going
	if ( IsBuilding() )
	{
		BuildingThink();
		return;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseTFPlayer *CBaseObject::GetOwner()
{ 
	return m_hBuilder; 
}


//-----------------------------------------------------------------------------
// Do we have to be built in a resource zone?
//-----------------------------------------------------------------------------
bool CBaseObject::MustBeBuiltInResourceZone( void ) const
{
	return (m_fObjectFlags & OF_MUST_BE_BUILT_IN_RESOURCE_ZONE) != 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseObject::MustBeBuiltInConstructionYard( ) const
{
	return (m_fObjectFlags & OF_MUST_BE_BUILT_IN_CONSTRUCTION_YARD) != 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseObject::MustNotBeBuiltInConstructionYard( void ) const
{
	return !MustBeBuiltInConstructionYard();
}

//-----------------------------------------------------------------------------
// Do we have to be built on an attachment point
//-----------------------------------------------------------------------------
bool CBaseObject::MustBeBuiltOnAttachmentPoint( void ) const
{
	return (m_fObjectFlags & OF_MUST_BE_BUILT_ON_ATTACHMENT) != 0;
}

//-----------------------------------------------------------------------------
// Purpose: Map placed objects need to setup here.
//-----------------------------------------------------------------------------
void CBaseObject::InitializeMapPlacedObject( void )
{
	m_bWasMapPlaced = true;
	m_fObjectFlags |= OF_CANNOT_BE_DISMANTLED;

	// If a map-placed object spawns child objects with their own control
	// panels, all of this lovely code will already have been run
	if (m_hBuiltOnEntity.Get())
		return;

	SetBuilder( NULL );

	// NOTE: We must spawn the control panels now, instead of during
	// Spawn, because until placement is started, we don't actually know
	// the position of the control panel because we don't know what it's
	// been attached to (could be a vehicle which supplies a different
	// place for the control panel)

	if ( !(m_fObjectFlags & OF_DOESNT_HAVE_A_MODEL) )
	{
		SpawnControlPanels();
	}

	SetHealth( GetMaxHealth() );

	AlignToGround( GetAbsOrigin() );
	FinishedBuilding();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseObject::Activate( void )
{
	BaseClass::Activate();

	// Add myself to the team
	InitializeMapPlacedObject();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseObject::SetBuilder( CBaseTFPlayer *pBuilder, bool moveobjects )
{
	TRACE_OBJECT( UTIL_VarArgs( "%0.2f CBaseObject::SetBuilder builder %s, moveobjects == %s\n", gpGlobals->curtime, 
		pBuilder ? pBuilder->GetPlayerName() : "NULL", 
		moveobjects ? "true" : "false" ) );

	ChangeBuilder( pBuilder, moveobjects );
}


//-----------------------------------------------------------------------------
// Called when the builder rotates this object...
//-----------------------------------------------------------------------------
void CBaseObject::ObjectMoved( )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CBaseObject::ObjectType( ) const
{
	return m_iObjectType;
}


//-----------------------------------------------------------------------------
// Purpose: Remove this object from it's team and mark for deletion
//-----------------------------------------------------------------------------
void CBaseObject::DestroyObject( void )
{
	TRACE_OBJECT( UTIL_VarArgs( "%0.2f CBaseObject::DestroyObject %p:%s\n", gpGlobals->curtime, this, GetClassname() ) );

	COrderEvent_ObjectDestroyed order( this );
	GlobalOrderEvent( &order );

	if ( GetBuilder() )
	{
		GetBuilder()->OwnedObjectDestroyed( this );
	}

	// Tell my powerpack that I'm gone
	if ( m_hPowerPack != NULL )
	{
		m_hPowerPack->UnPowerObject( this );
	}

	// Tell my power up source that I have been destroyed.
	if ( GetBuffStation() )
	{
		GetBuffStation()->DeBuffObject( this );
	}

	// Detach all my ropes
	int i;
	for ( i = 0; i < m_aRopes.Size(); i++ )
	{
		if ( m_aRopes[i] )
		{
			m_aRopes[i]->DieAtNextRest();
		}
	}

	UTIL_Remove( this );

	// Kill the control panels
	for ( i = m_hScreens.Count(); --i >= 0; )
	{
		DestroyVGuiScreen( m_hScreens[i].Get() );
	}
	m_hScreens.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: My builder's switched class/team, so start deteriorating.
//-----------------------------------------------------------------------------
void CBaseObject::StartDeteriorating( void )
{
	if ( tf_fastbuild.GetInt() )
		return;

	m_bDeteriorating = true;
	m_flStartedDeterioratingAt = gpGlobals->curtime;
	SetBuilder( NULL, true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseObject::StopDeteriorating( void )
{
	m_bDeteriorating = false;
}

//-----------------------------------------------------------------------------
// Purpose: Continue deterioration of this object
//-----------------------------------------------------------------------------
void CBaseObject::DeterioratingThink( void )
{
	// Calculate damage. The longer we've lasted, the faster we should go.
	float flDamage;
	float flDeteriorationTime = (gpGlobals->curtime - m_flStartedDeterioratingAt);
	// If we've lasted less than the base time, we want to take the base time to die
	flDamage = 0.1 * ( GetMaxHealth() / object_deterioration_time.GetFloat() ) * ceil(flDeteriorationTime / object_deterioration_time.GetFloat());
	// Hax0r the damage to get around the object damage reduction
	if ( obj_damage_factor.GetFloat() )
	{
		flDamage *= 1 / obj_damage_factor.GetFloat();
	}

	// Apply the damage
	OnTakeDamage( CTakeDamageInfo( GetContainingEntity(INDEXENT(0)), GetContainingEntity(INDEXENT(0)), flDamage, DMG_GENERIC ) );
}

//-----------------------------------------------------------------------------
// Purpose: Get the total time it will take to build this object
//-----------------------------------------------------------------------------
float CBaseObject::GetTotalTime( void )
{
	if (tf_fastbuild.GetInt())
		return 2.f;

	// If it's in a construction yard, don't take more than 5 seconds to build
  	if ( PointInConstructionYard( GetAbsOrigin() ) )
  	{
		if ( GetObjectInfo( ObjectType() )->m_flBuildTime > 5.0 )
  			return 5.0;
  	}

	return GetObjectInfo( ObjectType() )->m_flBuildTime;
}

//-----------------------------------------------------------------------------
// Purpose: Start placing the object
//-----------------------------------------------------------------------------
void CBaseObject::StartPlacement( CBaseTFPlayer *pPlayer )
{
	AddSolidFlags( FSOLID_NOT_SOLID );

	m_bPlacing = true;
	m_bBuilding = false;
	if ( pPlayer )
	{
		SetBuilder( pPlayer );
		ChangeTeam( pPlayer->GetTeamNumber() );
	}

	// Make it semi-transparent
	m_nRenderMode = kRenderTransAlpha; 
	SetRenderColorA( 128 );

	// Set my build size
	CollisionProp()->WorldSpaceAABB( &m_vecBuildMins, &m_vecBuildMaxs );
	m_vecBuildMins -= Vector( 4,4,0 );
	m_vecBuildMaxs += Vector( 4,4,0 );
	m_vecBuildMins -= GetAbsOrigin();
	m_vecBuildMaxs -= GetAbsOrigin();
}

//-----------------------------------------------------------------------------
// Purpose: Stop placing the object
//-----------------------------------------------------------------------------
void CBaseObject::StopPlacement( void )
{
	UTIL_Remove( this );
}

//-----------------------------------------------------------------------------
// Purpose: Find the nearest buildpoint on the specified entity
//-----------------------------------------------------------------------------
bool CBaseObject::FindNearestBuildPoint( CBaseEntity *pEntity, Vector vecBuildOrigin, float &flNearestPoint, Vector &vecNearestBuildPoint )
{
	bool bFoundPoint = false;

	IHasBuildPoints *pBPInterface = dynamic_cast<IHasBuildPoints*>(pEntity);
	Assert( pBPInterface );

	// Any empty buildpoints?
	for ( int i = 0; i < pBPInterface->GetNumBuildPoints(); i++ )
	{
		// Can this object build on this point?
		if ( pBPInterface->CanBuildObjectOnBuildPoint( i, GetType() ) )
		{
			// Close to this point?
			Vector vecBPOrigin;
			QAngle vecBPAngles;
			if ( pBPInterface->GetBuildPoint(i, vecBPOrigin, vecBPAngles) )
			{
				float flDist = (vecBPOrigin - vecBuildOrigin).Length();
				if ( flDist < MIN(flNearestPoint, pBPInterface->GetMaxSnapDistance( i )) )
				{
					flNearestPoint = flDist;
					vecNearestBuildPoint = vecBPOrigin;
					m_hBuiltOnEntity = pEntity;
					m_iBuiltOnPoint = i;

					// Set our angles to the buildpoint's angles
					SetAbsAngles( vecBPAngles );

					bFoundPoint = true;
				}
			}
		}
	}

	return bFoundPoint;
}

//-----------------------------------------------------------------------------
// Purpose: Calculate the placement model's position
//-----------------------------------------------------------------------------
bool CBaseObject::CalculatePlacement( CBaseTFPlayer *pPlayer )
{
	// Calculate build position
	Vector forward;
	QAngle vecAngles = vec3_angle;
	vecAngles.y = pPlayer->EyeAngles().y;
	SetLocalAngles( vecAngles );
	AngleVectors(vecAngles, &forward );

	// Adjust build distance based upon object size
	Vector2D xyDims;
	xyDims.x = MAX( fabs( m_vecBuildMins.x ), fabs( m_vecBuildMaxs.x ) );
	xyDims.y = MAX( fabs( m_vecBuildMins.y ), fabs( m_vecBuildMaxs.y ) );
	float flDistance = xyDims.Length() + 16; // small safety buffer
	Vector vecBuildOrigin = pPlayer->WorldSpaceCenter() + forward * flDistance;

	bool bSnappedToPoint = false;
	bool bShouldAttachToParent = false;

	// See if there are any nearby build positions to snap to
	Vector vecNearestBuildPoint = vec3_origin;
	float flNearestPoint = 9999;
	// First, look for nearby buildpoints on other objects
	for ( int i = 0; i < GetTFTeam()->GetNumObjects(); i++ )
	{
		CBaseObject *pObject = GetTFTeam()->GetObject(i);
		if ( pObject && !pObject->IsPlacing() )
		{
			if ( FindNearestBuildPoint( pObject, vecBuildOrigin, flNearestPoint, vecNearestBuildPoint ) )
			{
				bSnappedToPoint = true;

				// If I'm a vehicle, I'm being built on an MCV. Don't attach to the parent.
				if ( ShouldAttachToParent() )
				{
					bShouldAttachToParent = true;
				}
			}
		}
	}

	// If we're a vehicle, look for vehicle build points
	if ( IsAVehicle() )
	{
		CBaseEntity *pEntity = NULL;
		while ((pEntity = gEntList.FindEntityByClassname( pEntity, "info_vehicle_bay" )) != NULL)
		{
			if ( FindNearestBuildPoint( pEntity, vecBuildOrigin, flNearestPoint, vecNearestBuildPoint ) )
			{
				bSnappedToPoint = true;
			}
		}
	}

	// Check for resource zones for resource pumps
	if ( GetType() == OBJ_RESOURCEPUMP )
	{
		CBaseEntity *pEntity = NULL;
		while ((pEntity = gEntList.FindEntityByClassname( pEntity, "trigger_resourcezone" )) != NULL)
		{
			if ( FindNearestBuildPoint( pEntity, vecBuildOrigin, flNearestPoint, vecNearestBuildPoint ) )
			{
				bSnappedToPoint = true;
			}
		}
	}

	// See if there's any mapdefined build points near me
	int iCount = g_MapDefinedBuildPoints.Count();
	for ( i = 0; i < iCount; i++ )
	{
		if ( !InSameTeam(g_MapDefinedBuildPoints[i]) )
			continue;

		if ( FindNearestBuildPoint( g_MapDefinedBuildPoints[i], vecBuildOrigin, flNearestPoint, vecNearestBuildPoint ) )
		{
			bSnappedToPoint = true;
		}
	}

	// Upgrades become invisible if the player's not attaching them to a snap pint
	if ( IsAnUpgrade() )
	{
		if ( MustBeBuiltOnAttachmentPoint() && !bSnappedToPoint )
		{
			AddEffects( EF_NODRAW );
			return false;
		}
		else
		{
			RemoveEffects( EF_NODRAW );
		}
	}

	// Did we find a snap point?
	if ( bSnappedToPoint )
	{
		if ( bShouldAttachToParent )
		{
			AttachObjectToObject( m_hBuiltOnEntity.Get(), m_iBuiltOnPoint, vecNearestBuildPoint );
		}

		return CheckBuildOrigin( pPlayer, vecNearestBuildPoint, true );
	}

	// Clear out previous parent 
	if ( m_hBuiltOnEntity.Get() )
	{
		m_hBuiltOnEntity = NULL;
		m_iBuiltOnPoint = 0;
		SetParent( NULL );

		SetupUnattachedVersion();
	}

	// Check the build position
	return CheckBuildOrigin( pPlayer, vecBuildOrigin, false );
}


bool CBaseObject::VerifyCorner( const Vector &vBottomCenter, float xOffset, float yOffset )
{
	Vector vStart( vBottomCenter.x + xOffset, vBottomCenter.y + yOffset, vBottomCenter.z );
	
	trace_t tr;
	UTIL_TraceLine( 
		vStart, 
		vStart - Vector( 0, 0, tf_obj_ground_clearance.GetFloat() ), 
		MASK_SOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &tr );
	
	return !tr.startsolid && tr.fraction < 1;
}


//-----------------------------------------------------------------------------
// Purpose: Check under a build point to ensure it's buildable on
//-----------------------------------------------------------------------------
bool CBaseObject::CheckBuildPoint( Vector vecPoint, Vector &vecTrace, Vector *vecOutPoint )
{
	trace_t tr;

	bool bClear = true;
	Vector vecEnd;

	// Ensure that this point isn't in a no-build zone:
	if( !tf_fastbuild.GetInt() && NoBuildPreventsBuild(this, vecPoint ) )
		bClear = false;

	// If the point isn't in solid, trace down until we find the ground
	if ( enginetrace->GetPointContents( vecPoint ) == CONTENTS_EMPTY )
	{
		vecEnd = vecPoint - vecTrace;
		UTIL_TraceLine( vecPoint, vecEnd, MASK_SOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &tr);

		// Can't find ground to build on?
		if ( tr.fraction == 1.0 )
		{
			bClear = false;
		}
	} 
	else
	{
		// If the point's solid, trace up until we find empty air
		vecEnd = vecPoint + vecTrace;
		UTIL_TraceLine( vecPoint, vecEnd, MASK_SOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &tr);

		// Can't find ground to build on?
		if ( tr.allsolid )
		{
			bClear = false;
		}
	}

	// FIXME: HACK! This is a test to try to make mud non-buildable!!
	const surfacedata_t *pSurfaceProp = physprops->GetSurfaceData( tr.surface.surfaceProps );
	if (pSurfaceProp->game.maxSpeedFactor < 1.0f)
		bClear = false;

	if ( vecOutPoint )
	{
		*vecOutPoint = tr.endpos;
	}

	return bClear;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

bool CBaseObject::CheckBuildOrigin( CBaseTFPlayer *pPlayer, const Vector &vecInitialBuildOrigin, bool bSnappedToPoint )
{
	// By default, use the vecBuildOrigin..
	bool bResult = true;
	m_vecBuildOrigin = vecInitialBuildOrigin;
	Vector vErrorOrigin = vecInitialBuildOrigin - (m_vecBuildMaxs - m_vecBuildMins) * 0.5f - m_vecBuildMins;

	// If we're snapping to a build point, don't bother performing area checks
	if ( !bSnappedToPoint )
	{
		Vector vBuildDims = m_vecBuildMaxs - m_vecBuildMins;
		Vector vHalfBuildDims = vBuildDims * 0.5;
		Vector vHalfBuildDimsXY( vHalfBuildDims.x, vHalfBuildDims.y, 0 );

		// Here, we start at the highest Z we'll allow for the top of the object. Then
		// we sweep an XY cross section downwards until it hits the ground.
		//
		// The rule is that the top of to box can't go lower than the player's feet, and the bottom of the
		// box can't go higher than the player's head.
		//
		// To simplify things in here, we treat the box as though it's symmetrical about all axes
		// (so mins = -maxs), then reoffset the box at the very end.
		Vector vHalfPlayerDims = (pPlayer->WorldAlignMaxs() - pPlayer->WorldAlignMins()) * 0.5f;
		float flBoxTopZ = pPlayer->WorldSpaceCenter().z + vHalfPlayerDims.z + vBuildDims.z;
		float flBoxBottomZ = pPlayer->WorldSpaceCenter().z - vHalfPlayerDims.z - vBuildDims.z;
		
		// First, find the ground (ie: where the bottom of the box goes).
		trace_t tr;
		float bottomZ = 0;
		int nIterations = 6;
		float topZ = flBoxTopZ;
		float topZInc = (flBoxBottomZ - flBoxTopZ) / (nIterations-1);
		for ( int iIteration = 0; iIteration < nIterations; iIteration++ )
		{
			UTIL_TraceHull( 
				Vector( m_vecBuildOrigin.x, m_vecBuildOrigin.y, topZ ), 
				Vector( m_vecBuildOrigin.x, m_vecBuildOrigin.y, flBoxBottomZ ), 
				-vHalfBuildDimsXY, vHalfBuildDimsXY, MASK_SOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &tr );
			bottomZ = tr.endpos.z;

			// If there is no ground, then we can't place here.
			if ( tr.fraction == 1 )
			{
				m_vecBuildOrigin = vErrorOrigin;
				return false;
			}

			// If it started in solid, keep moving down.
			// Note that a working CGameTrace::fractionleftsolid would make this trivial, but it isn't
			// working now so we must resort to rubitry.
			if ( !tr.startsolid )
				break;

			topZ += topZInc;
		}
		
		if ( iIteration == nIterations )
		{
			m_vecBuildOrigin = vErrorOrigin;
			return false;
		}


		// Now see if the range we've got leaves us room for our box.
		if ( topZ - bottomZ < vBuildDims.z )
		{
			m_vecBuildOrigin = vErrorOrigin;
			return false;
		}

		// Verify that it's not on too much of a slope by seeing how far the corners are from the ground.
		Vector vBottomCenter( m_vecBuildOrigin.x, m_vecBuildOrigin.y, bottomZ );
		if ( !VerifyCorner( vBottomCenter, -vHalfBuildDims.x, -vHalfBuildDims.y ) ||
			 !VerifyCorner( vBottomCenter, +vHalfBuildDims.x, +vHalfBuildDims.y ) ||
			 !VerifyCorner( vBottomCenter, +vHalfBuildDims.x, -vHalfBuildDims.y ) ||
			 !VerifyCorner( vBottomCenter, -vHalfBuildDims.x, +vHalfBuildDims.y ) )
		{
			m_vecBuildOrigin = vErrorOrigin;
			return false;
		}

		// Ok, now we know the Z range where this box can fit.
		Vector vBottomLeft = m_vecBuildOrigin - vHalfBuildDims;
		vBottomLeft.z = bottomZ;
		m_vecBuildOrigin = vBottomLeft - m_vecBuildMins;
	}

	Vector vecForward, vecRight, vecUp;
	AngleVectors( GetLocalAngles(), &vecForward, &vecRight, &vecUp );
	AttemptToFindPower();
	AttemptToFindBuffStation();

	// Make sure construction yards don't screw us up (tf_fastbuild allows builds anywhere)
	if ( !tf_fastbuild.GetInt() && ConstructionYardPreventsBuild( this, m_vecBuildOrigin ))
		return false;

	// If we have to be attached to something, and we're not, abort
	if ( !tf_fastbuild.GetInt() && MustBeBuiltOnAttachmentPoint() && !bSnappedToPoint )
		return false;

	// Make sure there aren't any solid objects in the area
	if ( !bSnappedToPoint || IsAVehicle() )
	{
		if ( !(m_fObjectFlags & OF_DONT_PREVENT_BUILD_NEAR_OBJ) )
		{
			// Get a list of nearby entities
			CBaseEntity *pListOfNearbyEntities[100];
			int iNumberOfNearbyEntities = UTIL_EntitiesInSphere( pListOfNearbyEntities, 100, m_vecBuildOrigin, GetNearbyObjectCheckRadius(), 0 );
			for ( int i = 0; i < iNumberOfNearbyEntities; i++ )
			{
				CBaseEntity *pEntity = pListOfNearbyEntities[i];
				if ( pEntity->IsSolid( ) )
				{
					// Ignore shields..
					if ( pEntity->GetCollisionGroup() == TFCOLLISION_GROUP_SHIELD )
						continue;

					// Ignore func brushes
					// BUGBUG: Shouldn't this test against MOVETYPE_PUSH instead of SOLID_BSP?
					if ( pEntity->GetSolid() == SOLID_BSP )
						continue;

					// Ignore the player who's building
					if ( pEntity == GetBuilder() )
						continue;

					// YWB:  Ignore other players
					if ( pEntity->IsPlayer() )
						continue;

					// Ignore map placed objects
					if ( pEntity->GetTeamNumber() == 0 )
						continue;
				
					//NDebugOverlay::EntityBounds( pEntity, 0,255,0,8, 0.1 );
					return false;
				}

				// Sentryguns may be turtled, and non-solid
				if ( pEntity->Classify() == CLASS_MILITARY )
				{
					CObjectSentrygun *pSentry = dynamic_cast<CObjectSentrygun*>(pEntity);
					if ( pSentry && pSentry->IsTurtled() )
						return false;
				}
			}
		}
	}

	if ( !bSnappedToPoint )
	{
		AlignToGround( m_vecBuildOrigin );
	}

	return bResult;
}

//-----------------------------------------------------------------------------
// Purpose: Align myself to the ground below the specified point
//-----------------------------------------------------------------------------
void CBaseObject::AlignToGround( Vector vecOrigin )
{
	if ( !(m_fObjectFlags & OF_ALIGN_TO_GROUND) )
		return;

	trace_t tr;
	Vector vecWorldMins, vecWorldMaxs;
	CollisionProp()->WorldSpaceAABB( &vecWorldMins, &vecWorldMaxs );
	float flHeight = MAX( vecWorldMaxs.z - vecWorldMins.z, 60 );
	UTIL_TraceLine( vecOrigin, vecOrigin + Vector(0,0,-flHeight), MASK_SOLID, this, COLLISION_GROUP_PLAYER_MOVEMENT, &tr );
	if ( tr.fraction != 1.0 )
	{
		// Orient the *up* axis to be along the plane normal
		Vector perp( 1, 0, 0 );
		Vector forward, right;
		CrossProduct( perp, tr.plane.normal, forward );
		if (forward.LengthSqr() < 0.1f)
		{
			perp.Init( 0, 1, 0 );
			CrossProduct( perp, tr.plane.normal, forward );
		}
		VectorNormalize( forward );
		CrossProduct( tr.plane.normal, forward, right );

		VMatrix orientation( forward, right, tr.plane.normal );

		QAngle angles;
		MatrixToAngles( orientation, angles );
		SetAbsAngles( angles );
	}
}

//-----------------------------------------------------------------------------
// Exit points for mounted vehicles....
//-----------------------------------------------------------------------------
void CBaseObject::GetExitPoint( CBaseEntity *pPlayer, int nBuildPoint, Vector *pAbsPosition, QAngle *pAbsAngles )
{
	// Deal with hierarchy...
	IHasBuildPoints *pMount = dynamic_cast<IHasBuildPoints*>(GetMoveParent());
	if (pMount)
	{
		int nBuildPoint = pMount->FindObjectOnBuildPoint( this );
		if (nBuildPoint >= 0)
		{
			pMount->GetExitPoint( pPlayer, nBuildPoint, pAbsPosition, pAbsAngles );
			return;
		}
	}

	// FIXME: In future, we may well want to use specific exit attachments here...
	GetBuildPoint( nBuildPoint, *pAbsPosition, *pAbsAngles );

	// Move back along the forward direction a bit...
	Vector vecForward, vecUp;
	AngleVectors( *pAbsAngles, &vecForward, NULL, &vecUp );
	*pAbsPosition -= vecForward * 60;
	*pAbsPosition += vecUp * 30;

	// Now select a good spot to drop onto
	Vector vNewPos;
	if ( !EntityPlacementTest(pPlayer, *pAbsPosition, vNewPos, true) )
	{
		Warning("Can't find valid place to exit object.\n");
		return;
	}

	*pAbsPosition = vNewPos;
}


void CBaseObject::AdjustInitialBuildAngles()
{
}


//-----------------------------------------------------------------------------
// Purpose: Try and find power for this object during placement
//-----------------------------------------------------------------------------
void CBaseObject::AttemptToFindPower( void )
{
	// Human objects need power, so show the player if the current position will have power, but don't prevent building.
	if ( !CanPowerupEver( POWERUP_POWER ) )
		return;

	// If I have a powerpack, see if I'm unable to keep power, or not needed.
	// This is done before checking to see if the object needs power, because it may
	// have once needed power, but doesn't anymore (i.e. snapped to an attachment point)
	if ( m_hPowerPack )
	{
		m_hPowerPack->EnsureObjectPower( this );
	}

	// If I don't have a powerpack, or I just moved too far from it, look for a powerpack
	if ( !m_hPowerPack )
	{
		GetTFTeam()->UpdatePowerpacks( NULL, this );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseObject::AttemptToFindBuffStation( void )
{
	// Check to see if this object can be connected to a buff station.
	if ( !CanBeHookedToBuffStation() )
		return;

	// We have already found a buff station, we want to use - check distances.
	if ( GetBuffStation() )
	{
		GetBuffStation()->CheckBuffConnection( this );
	}
	// Look for a buff station to use.
	else
	{
		GetTFTeam()->UpdateBuffStations( NULL, this, true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Move the placement model to the current position. Return false if it's an invalid position
//-----------------------------------------------------------------------------
bool CBaseObject::UpdatePlacement( CBaseTFPlayer *pPlayer )
{
	bool placementOk = CalculatePlacement( pPlayer );
	if ( placementOk )
	{
		SetRenderColor( 255, 255, 255, GetRenderColor().a );
	}
	else
	{
		SetRenderColor( 255, 0, 0, GetRenderColor().a );
	}

	Teleport( &m_vecBuildOrigin, &GetLocalAngles(), NULL );

	return placementOk;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseObject::PreStartBuilding()
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Start building the object
//-----------------------------------------------------------------------------
bool CBaseObject::StartBuilding( CBaseEntity *pBuilder )
{
	// Need to add the object to the team now...
	CTFTeam *pTFTeam = ( CTFTeam * )GetGlobalTeam( GetTeamNumber() );

	// Deduct the cost from the player
	if ( pBuilder && pBuilder->IsPlayer() )
	{
		m_iAmountPlayerPaidForMe = ((CBaseTFPlayer*)pBuilder)->StartedBuildingObject( m_iObjectType );
		if ( !m_iAmountPlayerPaidForMe )
		{
			// Player couldn't afford to pay for me, so abort
			ClientPrint( (CBasePlayer*)pBuilder, HUD_PRINTCENTER, "Not enough resources.\n" );
			StopPlacement();
			return false;
		}
	}
	
	// Add this object to the team's list (because we couldn't add it during
	// placement mode)
	if ( pTFTeam && !pTFTeam->IsObjectOnTeam( this ) )
	{
		pTFTeam->AddObject( this );
	}

	m_bPlacing = false;
	m_bBuilding = true;
	SetHealth( OBJECT_CONSTRUCTION_STARTINGHEALTH );
	m_flPercentageConstructed = 0;

	// Compute a good fitting AABB since we know where this thing belongs
	if ( VPhysicsGetObject() && !IsBuiltOnAttachment() )
	{
		Vector absmins, absmaxs;		
		physcollision->CollideGetAABB( &absmins, &absmaxs, VPhysicsGetObject()->GetCollide(), GetAbsOrigin(), GetAbsAngles() );

		// This is required to get the client + server looking the same
		// since the client uses the mins to compute absmins + absmaxs
		SetCollisionBounds( absmins - GetAbsOrigin(), absmaxs - GetAbsOrigin() );
	}

	m_nRenderMode = kRenderNormal; 
	RemoveSolidFlags( FSOLID_NOT_SOLID );

	// NOTE: We must spawn the control panels now, instead of during
	// Spawn, because until placement is started, we don't actually know
	// the position of the control panel because we don't know what it's
	// been attached to (could be a vehicle which supplies a different
	// place for the control panel)
	// NOTE: We must also spawn it before FinishedBuilding can be called
	SpawnControlPanels();

	// Tell the object we've been built on that we exist
	if ( IsBuiltOnAttachment() && ShouldAttachToParent() )
	{
		IHasBuildPoints *pBPInterface = dynamic_cast<IHasBuildPoints*>((CBaseEntity*)m_hBuiltOnEntity.Get());
		Assert( pBPInterface );
		pBPInterface->SetObjectOnBuildPoint( m_iBuiltOnPoint, this );
	}

	// Start the build animations
	m_flTotalConstructionTime = m_flConstructionTimeLeft = GetTotalTime();

	if ( pBuilder && pBuilder->IsPlayer() )
	{
		((CBaseTFPlayer*)pBuilder)->FinishedObject( this );
	}

	m_vecBuildOrigin = GetAbsOrigin();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Continue construction of this object
//-----------------------------------------------------------------------------
void CBaseObject::BuildingThink( void )
{
	// Continue construction
	Repair( (GetMaxHealth() - OBJECT_CONSTRUCTION_STARTINGHEALTH) / m_flTotalConstructionTime * OBJECT_CONSTRUCTION_INTERVAL );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseObject::AttemptToActivateBuffStation( void )
{
	if ( !GetBuffStation() )
		return;

	if ( GetBuffStation()->IsPlacing() || GetBuffStation()->IsBuilding() ||
		 !GetBuffStation()->IsPowered() )
		return;

	if ( m_bBuffActivated )
		return;

	BuffStationActivate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseObject::SetControlPanelsActive( bool bState )
{
	// Activate control panel screens
	for ( int i = m_hScreens.Count(); --i >= 0; )
	{
		if (m_hScreens[i].Get())
		{
			m_hScreens[i]->SetActive( bState );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseObject::FinishedBuilding( void )
{
	SetControlPanelsActive( true );

	// Only make a shadow if the object doesn't use vphysics
	if (!VPhysicsGetObject())
	{
		VPhysicsInitStatic();
	}

	m_bBuilding = false;

	AttemptToGoActive();
	AttemptToActivateBuffStation();

	// We're done building, add in the stat...
	TFStats()->IncrementStat( (TFStatId_t)(TF_STAT_FIRST_OBJECT_BUILT + ObjectType()), 1 );

	// Spawn any objects on this one
	SpawnObjectPoints();

	// Let our vehicle bay know, if we have one
	if ( m_hVehicleBay )
	{
		m_hVehicleBay->FinishedBuildVehicle( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Objects store health in hacky ways
//-----------------------------------------------------------------------------
void CBaseObject::SetHealth( float flHealth )
{
	if ( IsDisabled() )
	{
		if ( ( m_flMinDisableHealth != 0.0f && flHealth > m_flMinDisableHealth ) ||
			 ( flHealth > 1 ) )
		{
			// Reenable and fire output
			SetDisabled( false );

			m_OnBecomingReenabled.FireOutput( this, this );
		}
	}

	bool changed = m_flHealth != flHealth;

	m_flHealth = flHealth;
	m_iHealth = ceil(m_flHealth);

	// If we have a model, and a pose parameter, set the pose parameter to reflect our health
	if ( !(m_fObjectFlags & OF_DOESNT_HAVE_A_MODEL) )
	{
		if (LookupPoseParameter( "object_health") >= 0 && GetMaxHealth() > 0 )
		{
			SetPoseParameter( "object_health", 100 * ( GetHealth() / (float)GetMaxHealth() ) );
		}
	}

	if ( changed )
	{
		// Set value and fire output
		m_OnObjectHealthChanged.Set( m_flHealth, this, this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Powerup has just started
//-----------------------------------------------------------------------------
void CBaseObject::PowerupStart( int iPowerup, float flAmount, CBaseEntity *pAttacker, CDamageModifier *pDamageModifier )
{
	switch( iPowerup )
	{
	case POWERUP_BOOST:
		{
			// Can we boost health further?
			if ( GetHealth() < GetMaxHealth() )
			{
				/*
				if ( (gpGlobals->curtime - m_flLastRepairTime) > 0.01 )
				{
					Msg("TOTAL REPAIR: %.2f in %.2f\n\n", m_flRepairedSinceLastTime, (gpGlobals->curtime - m_flLastRepairTime) );
				}

				if ( pAttacker->IsPlayer() )
				{
					CBaseTFPlayer *pPlayer = (CBaseTFPlayer *)pAttacker;
					Msg("(%.2f) %s repaired %s for %.2f health.\n", gpGlobals->curtime, pPlayer->GetPlayerName(), GetClassname(), flAmount );
				}
				*/
				// Is this repair happening at the same time as other repairing on me?
				if ( (gpGlobals->curtime - m_flLastRepairTime) < 0.01 )
				{
					//Msg("  ->Reducing repair by %.2f\n", m_flNextRepairMultiplier );
					flAmount *= m_flNextRepairMultiplier;
					m_flNextRepairMultiplier *= 0.5;
				}
				else
				{
					m_flLastRepairTime = gpGlobals->curtime;
					m_flNextRepairMultiplier = 0.5;
					m_flRepairedSinceLastTime = 0;
				}

				//Msg("  REPAIRED: %.2f\n", flAmount );

				Repair( flAmount );

				m_flRepairedSinceLastTime += flAmount;
			}

			// Prevent callback to base class, since we handled it here
			return;
		}
		break;

	case POWERUP_POWER:
		{
			Assert( m_hPowerPack );
			AttemptToGoActive();
		}
		break;

	default:
		break;
	}

	BaseClass::PowerupStart( iPowerup, flAmount, pAttacker, pDamageModifier );
}

//-----------------------------------------------------------------------------
// Purpose: Powerup has just started
//-----------------------------------------------------------------------------
void CBaseObject::PowerupEnd( int iPowerup )
{
	switch( iPowerup )
	{
	case POWERUP_POWER:
		{
			OnGoInactive();
			m_hPowerPack = NULL;
		}
		break;

	default:
		break;
	}

	BaseClass::PowerupEnd( iPowerup );
}

//-----------------------------------------------------------------------------
// Purpose: Override base traceattack to prevent visible effects from team members shooting me
//-----------------------------------------------------------------------------
void CBaseObject::TraceAttack( const CTakeDamageInfo &inputInfo, const Vector &vecDir, trace_t *ptr )
{
	// Prevent team damage here so blood doesn't appear
	if ( inputInfo.GetAttacker() )
	{
		if ( InSameTeam(inputInfo.GetAttacker()) )
			return;
	}

	float fVulnerableMultiplier = FindVulnerablePointMultiplier( ptr->hitgroup, ptr->hitbox ); 

	CTakeDamageInfo info = inputInfo;
	info.ScaleDamage( fVulnerableMultiplier );

	SpawnBlood( ptr->endpos, vecDir, BloodColor(), info.GetDamage() );
	AddMultiDamage( info, this );
}

//-----------------------------------------------------------------------------
// Purpose: Prevent Team Damage
//-----------------------------------------------------------------------------
ConVar object_show_damage( "obj_show_damage", "0", 0, "Show all damage taken by objects." );
ConVar object_capture_damage( "obj_capture_damage", "0", 0, "Captures all damage taken by objects for dumping later." );

CUtlDict<int,int> g_DamageMap;

void Cmd_DamageDump_f(void)
{	
	CUtlDict<bool,int> g_UniqueColumns;

	// Build the unique columns:
	for( int idx = g_DamageMap.First(); idx != g_DamageMap.InvalidIndex(); idx = g_DamageMap.Next(idx) )
	{
		char* szColumnName = strchr(g_DamageMap.GetElementName(idx),',') + 1;

		int ColumnIdx = g_UniqueColumns.Find( szColumnName );

		if( ColumnIdx == g_UniqueColumns.InvalidIndex() ) 
		{
			g_UniqueColumns.Insert( szColumnName, false );
		}
	}

	// Dump the column names:
	FileHandle_t f = filesystem->Open("damage.txt","wt+");

	for( idx = g_UniqueColumns.First(); idx != g_UniqueColumns.InvalidIndex(); idx = g_UniqueColumns.Next(idx) )
	{
		filesystem->FPrintf(f,"\t%s",g_UniqueColumns.GetElementName(idx));
	}

	filesystem->FPrintf(f,"\n");

 
	CUtlDict<bool,int> g_CompletedRows;

	// Dump each row:
	bool bDidRow;

	do
	{
		bDidRow = false;

		for( idx = g_DamageMap.First(); idx != g_DamageMap.InvalidIndex(); idx = g_DamageMap.Next(idx) )
		{
			char szRowName[256];

			// Check the Row name of each entry to see if I've done this row yet.
			Q_strncpy(szRowName, g_DamageMap.GetElementName(idx), sizeof( szRowName ) );
			*strchr(szRowName,',') = '\0';

			char szRowNameComma[256];
			Q_snprintf( szRowNameComma, sizeof( szRowNameComma ), "%s,", szRowName );

			if( g_CompletedRows.Find(szRowName) == g_CompletedRows.InvalidIndex() )
			{
				bDidRow = true;
				g_CompletedRows.Insert(szRowName,false);


				// Output the row name:
				filesystem->FPrintf(f,szRowName);

				for( int ColumnIdx = g_UniqueColumns.First(); ColumnIdx != g_UniqueColumns.InvalidIndex(); ColumnIdx = g_UniqueColumns.Next( ColumnIdx ) )
				{
					char szRowNameCommaColumn[256];
					Q_strncpy( szRowNameCommaColumn, szRowNameComma, sizeof( szRowNameCommaColumn ) );					
					Q_strncat( szRowNameCommaColumn, g_UniqueColumns.GetElementName( ColumnIdx ), sizeof( szRowNameCommaColumn ), COPY_ALL_CHARACTERS );

					int nDamageAmount = 0;
					// Fine to reuse idx since we are going to break anyways.
					for( idx = g_DamageMap.First(); idx != g_DamageMap.InvalidIndex(); idx = g_DamageMap.Next(idx) )
					{
						if( !stricmp( g_DamageMap.GetElementName(idx), szRowNameCommaColumn ) )
						{
							nDamageAmount = g_DamageMap[idx];
							break;
						}
					}

					filesystem->FPrintf(f,"\t%i",nDamageAmount);

				}

				filesystem->FPrintf(f,"\n");
				break;
			}
		}
		// Grab the row name:

	} while(bDidRow);

	// close the file:
	filesystem->Close(f);
}

static ConCommand obj_dump_damage( "obj_dump_damage", Cmd_DamageDump_f );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ReportDamage( const char* szInflictor, const char* szVictim, float fAmount, int nCurrent, int nMax )
{
	int iAmount = (int)fAmount;

	if( object_show_damage.GetBool() && iAmount )
	{
		Msg( "ShowDamage: Object %s taking %0.1f damage from %s ( %i / %i )\n", szVictim, fAmount, szInflictor, nCurrent, nMax );
	}

	if( object_capture_damage.GetBool() )
	{
		char szMangledKey[256];

		Q_snprintf(szMangledKey,sizeof(szMangledKey)/sizeof(szMangledKey[0]),"%s,%s",szInflictor,szVictim);
		int idx = g_DamageMap.Find( szMangledKey );

		if( idx == g_DamageMap.InvalidIndex() )
		{
			g_DamageMap.Insert( szMangledKey, iAmount );

		} else
		{
			g_DamageMap[idx] += iAmount;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Pass the specified amount of damage through to any objects I have built on me
//-----------------------------------------------------------------------------
bool CBaseObject::PassDamageOntoChildren( const CTakeDamageInfo &info, float *flDamageLeftOver )
{
	IHasBuildPoints *pBPInterface = dynamic_cast<IHasBuildPoints*>(this);
	Assert( pBPInterface );

	float flDamage = info.GetDamage();

	// Double the amount of damage done (and get around the child damage modifier)
	flDamage *= 2;
	if ( obj_child_damage_factor.GetFloat() )
	{
		flDamage *= (1 / obj_child_damage_factor.GetFloat());
	}

	// Remove blast damage because child objects (well specifically upgrades)
	// want to ignore direct blast damage but still take damage from parent
	CTakeDamageInfo childInfo = info;
	childInfo.SetDamage( flDamage );
	childInfo.SetDamageType( info.GetDamageType() & (~DMG_BLAST) );

	CBaseEntity *pEntity = pBPInterface->GetFirstObjectOnMe();
	while ( pEntity )
	{
		Assert( pEntity->m_takedamage != DAMAGE_NO );
		// Do damage to the next object
		float flDamageTaken = pEntity->OnTakeDamage( childInfo );
		// If we didn't kill it, abort
		CBaseObject *pObject = dynamic_cast<CBaseObject*>(pEntity);
		if ( !pObject || !pObject->IsDying() )
		{
			char* szInflictor = "unknown";
			if( info.GetInflictor() )
				szInflictor = (char*)info.GetInflictor()->GetClassname();

			ReportDamage( szInflictor, GetClassname(), flDamageTaken, GetHealth(), GetMaxHealth() );

			*flDamageLeftOver = flDamage;
			return true;
		}
		// Reduce the damage and move on to the next
		flDamage -= flDamageTaken;
		pEntity = pBPInterface->GetFirstObjectOnMe();
	}

	*flDamageLeftOver = flDamage;
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseObject::OnTakeDamage( const CTakeDamageInfo &info )
{
	// Prevent damage if the game hasn't started yet
	if ( CurrentActIsAWaitingAct() )
		return 0;
	if ( !IsAlive() )
		return info.GetDamage();
	if (m_bInvulnerable)
		return 0;
	if ( m_takedamage == DAMAGE_NO )
		return 0;
	if ( IsPlacing() )
		return 0;

	// Check teams
	if ( info.GetAttacker() )
	{
		if ( InSameTeam(info.GetAttacker()) )
			return 0;
	}

	IHasBuildPoints *pBPInterface = dynamic_cast<IHasBuildPoints*>(this);

	float flDamage = info.GetDamage();

	// Objects take half damage from bullets
	if ( info.GetDamageType() & DMG_BULLET )
	{
		flDamage *= 0.5;
	}

	// Objects build on other objects take less damage
	if ( !IsAnUpgrade() && GetParentObject() )
	{
		flDamage *= obj_child_damage_factor.GetFloat();
	}

	if (obj_damage_factor.GetFloat())
	{
		flDamage *= obj_damage_factor.GetFloat();
	}

	// Constructing objects take extra damage
	if ( IsBuilding() )
	{
		flDamage *= 3;
	}

	// If has min health, and damage would put it below min health disable it if not already disabled
	bool bShouldBeDisabled = false;
	if ( m_flMinDisableHealth != 0 && ( m_flHealth - flDamage ) < m_flMinDisableHealth )
	{
		bShouldBeDisabled = true;
	}
	else if ( pBPInterface && pBPInterface->GetNumObjectsOnMe() && (( m_flHealth - flDamage ) < 1) )
	{
		bShouldBeDisabled = true;
	}

	// Make sure we're disabled if we're supposed to be
	if ( bShouldBeDisabled )
	{
		// Remove any sappers on me
		if ( m_bCantDie )
		{
			RemoveAllSappers( this );
		}

		// Make sure this only fires first time we cross the threshold and go disabled
		if ( !IsDisabled() )
		{
			SetDisabled( true );
			m_OnBecomingDisabled.FireOutput( info.GetAttacker(), this );

			// Special case: If we have a min disabled health, and we're set to not die, immediately fall to 1 health
			if ( m_bCantDie )
			{
				SetHealth( 1 );
			}
		}
	}

	// If I have objects on me, I can't be destroyed until they're gone. Ditto if I can't be killed.
	bool bWillDieButCant = (m_bCantDie || pBPInterface->GetNumObjectsOnMe()) && (( m_flHealth - flDamage ) < 1);
	if ( bWillDieButCant )
	{
		// Soak up the damage it would take to drop us to 1 health
		flDamage = flDamage - m_flHealth;
		SetHealth( 1 );

		// Pass leftover damage 
		if ( flDamage )
		{
			if ( PassDamageOntoChildren( info, &flDamage ) )
				return flDamage;
		}
	}

	if ( flDamage )
	{
		// Recheck our death possibility, because our objects may have all been blown off us by now
		bWillDieButCant = (m_bCantDie || pBPInterface->GetNumObjectsOnMe()) && (( m_flHealth - flDamage ) < 1);
		if ( !bWillDieButCant )
		{
			// Reduce health
			SetHealth( m_flHealth - flDamage );
		}
	}

	m_OnDamaged.FireOutput(info.GetAttacker(), this);

	// Hurt by an enemy?
	if ( info.GetAttacker() && info.GetAttacker()->entindex() > 0 )
	{
		m_flLastRealDamage = gpGlobals->curtime;
	}

	if ( GetHealth() <= 0 )
	{
		if ( info.GetAttacker() )
		{
			TFStats()->IncrementTeamStat( info.GetAttacker()->GetTeamNumber(), TF_TEAM_STAT_DESTROYED_OBJECT_COUNT, 1 );
			TFStats()->IncrementPlayerStat( info.GetAttacker(), TF_PLAYER_STAT_DESTROYED_OBJECT_COUNT, 1 );
		}

		m_lifeState = LIFE_DEAD;
		m_OnDestroyed.FireOutput( info.GetAttacker(), this);
		Killed();
	}
	else
	{
		// Notify team about interesting stuff going on with this object
		if ( !(m_fObjectFlags & OF_SUPPRESS_NOTIFY_UNDER_ATTACK) && ( m_iszUnderAttackSound != NULL_STRING ) )
		{
			CTFTeam *pTeam = GetTFTeam();
			if ( pTeam )
			{
				Vector vecPosition = GetAbsOrigin();

				// Tell everyone on the team that this object's underattack
				CRecipientFilter myteam;
				myteam.MakeReliable();
				myteam.AddRecipientsByTeam( pTeam );
				UserMessageBegin( myteam, "MinimapPulse" );
					WRITE_VEC3COORD( vecPosition );
				MessageEnd();

				GetTFTeam()->PostMessage( TEAMMSG_CUSTOM_SOUND, NULL, (char*)STRING(m_iszUnderAttackSound) );
			}
		}
	}

	{
		char* szInflictor = "unknown";
		if( info.GetInflictor() )
			szInflictor = (char*)info.GetInflictor()->GetClassname();

		ReportDamage( szInflictor, GetClassname(), flDamage, GetHealth(), GetMaxHealth() );
	}

	return flDamage;
}

//-----------------------------------------------------------------------------
// Purpose: Get the time it will take to repair this object
//-----------------------------------------------------------------------------
float CBaseObject::GetRepairTime( void )
{
	// Can't be repaired while being constructed
	if ( IsBuilding() )
		return 0;

	int iRepairHealth = GetMaxHealth() - GetHealth();
	if ( iRepairHealth )
	{
		return ((float)iRepairHealth / OBJECT_REPAIR_RATE);
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Repair myself for the time period passed in. Return true if I'm fully repaired.
//-----------------------------------------------------------------------------
bool CBaseObject::UpdateRepair( float flRepairTime )
{
	return Repair( (flRepairTime * OBJECT_REPAIR_RATE) );
}

//-----------------------------------------------------------------------------
// Purpose: Repair / Help-Construct this object the specified amount
//-----------------------------------------------------------------------------
bool CBaseObject::Repair( float flHealth )
{
	// Multiply it by the repair rate
	flHealth *= m_flRepairMultiplier;
	if ( !flHealth )
		return false;

	if ( IsBuilding() )
	{
		if ( HasPowerup(POWERUP_EMP) )
			return false;

		// Reduce the construction time by the correct amount for the health passed in
		float flConstructionTime = flHealth / ((GetMaxHealth() - OBJECT_CONSTRUCTION_STARTINGHEALTH) / m_flTotalConstructionTime);
		m_flConstructionTimeLeft = MAX( 0, m_flConstructionTimeLeft - flConstructionTime);
		m_flConstructionTimeLeft = clamp( m_flConstructionTimeLeft, 0.0f, m_flTotalConstructionTime );
		m_flPercentageConstructed = 1 - (m_flConstructionTimeLeft / m_flTotalConstructionTime);
		m_flPercentageConstructed = clamp( m_flPercentageConstructed, 0.0f, 1.0f );

		// Increase health.
		SetHealth( MIN( GetMaxHealth(), m_flHealth + flHealth ) );

		// Return true if we're constructed now
		if ( m_flConstructionTimeLeft <= 0.0f )
		{
			FinishedBuilding();
			return true;
		}
	}
	else
	{
		// Return true if we're already fully healed
		if ( GetHealth() >= GetMaxHealth() )
			return true;

		// Increase health.
		SetHealth( MIN( GetMaxHealth(), m_flHealth + flHealth ) );

		m_OnRepaired.FireOutput( this, this);

		// Return true if we're fully healed now
		if ( GetHealth() == GetMaxHealth() )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Object has been blown up. Drop resource chunks upto the value of my max health.
//-----------------------------------------------------------------------------
void CBaseObject::Killed( void )
{
	m_bDying = true;

	// Do an explosion.
	CPASFilter filter( GetAbsOrigin() );
	te->Explosion( 
		filter, 
		0.0,				
		&GetAbsOrigin(),
		g_sModelIndexFireball,
		5.4,		// radius
		15,
		TE_EXPLFLAG_NODLIGHTS,
		256,
		200);

	Vector vecOrigin = WorldSpaceCenter() + Vector(0,0,32);

	bool bDropResources = true;

	// Don't drop resources if I'm built out of brushes, or I'm an upgrade
	if ( m_fObjectFlags & OF_DOESNT_HAVE_A_MODEL || IsAnUpgrade() )
	{
		bDropResources = false;
	}

	// Don't drop resources if I haven't taken damage from an enemy for a while (i.e. I've deteriorated instead)
	if ( gpGlobals->curtime > (m_flLastRealDamage + MAX_DROP_TIME_AFTER_DAMAGE) )
	{
		bDropResources = false;
	}

	// Drop resources based upon our base cost
	int iCost = CalculateObjectCost( GetType(), 0, GetTeamNumber() );
	iCost *= 0.5;
	if ( bDropResources && iCost )
	{
		// Convert value to chunks.
		int nProcessedChunks = 0;
		int nNormalChunks = 0;
		ConvertResourceValueToChunks( iCost, &nProcessedChunks, &nNormalChunks );

		// Make everything drop at least 1 chunk
		if ( !nProcessedChunks && !nNormalChunks )
		{
			nNormalChunks++;
		}

		// Drop processed chunks.
		int iChunk;
		for ( iChunk = 0; iChunk < nProcessedChunks; iChunk++ )
		{
			// Generate a random velocity vector.
			Vector vecVelocity = Vector( random->RandomFloat( -250,250 ), random->RandomFloat( -250,250 ), random->RandomFloat( 200,450 ) );

			// Create a processed chunk.
			CResourceChunk *pChunk = CResourceChunk::Create( true, vecOrigin, vecVelocity );
			pChunk->ChangeTeam( GetTeamNumber() );
		}

		// Drop normal chunks
		for ( iChunk = 0; iChunk < nNormalChunks; iChunk++ )
		{
			// Generate a random velocity vector.
			Vector vecVelocity = Vector( random->RandomFloat( -250,250 ), random->RandomFloat( -250,250 ), random->RandomFloat( 200,450 ) );

			// Create a processed chunk.
			CResourceChunk *pChunk = CResourceChunk::Create( false, vecOrigin, vecVelocity );
			pChunk->ChangeTeam( GetTeamNumber() );
		}

		TFStats()->IncrementTeamStat( GetTeamNumber(), TF_TEAM_STAT_RESOURCE_CHUNKS_DROPPED, (resource_chunk_processed_value.GetFloat() * nProcessedChunks) + (resource_chunk_value.GetFloat() * nNormalChunks) );
	}

	DetachObjectFromObject();

	UTIL_Remove( this );
}

//-----------------------------------------------------------------------------
// Purpose: Indicates this NPC's place in the relationship table.
//-----------------------------------------------------------------------------
Class_T	CBaseObject::Classify( void )
{
	return (CLASS_MILITARY);
}

//-----------------------------------------------------------------------------
// Purpose: Get the type of this object
//-----------------------------------------------------------------------------
int	CBaseObject::GetType()
{
	return m_iObjectType;
}

//-----------------------------------------------------------------------------
// Purpose: Get the builder of this object
//-----------------------------------------------------------------------------
CBaseTFPlayer *CBaseObject::GetBuilder( void )
{
	return m_hBuilder;
}

//-----------------------------------------------------------------------------
// Purpose: Get the original builder of this object
//			Used to get the builder of a deteriorating object
//-----------------------------------------------------------------------------
CBaseTFPlayer *CBaseObject::GetOriginalBuilder( void )
{
	return m_hOriginalBuilder;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the Owning CTeam should clean this object up automatically
//-----------------------------------------------------------------------------
bool CBaseObject::ShouldAutoRemove( void )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: If the object's still being built, it's not usable
//-----------------------------------------------------------------------------
int	CBaseObject::ObjectCaps( void ) 
{ 
	if ( IsPlacing() )
		return 0;

	// If I'm being built, only allow +use if I don't have a sapper on me and I haven't been disabled by a plasma weapon
	if ( IsBuilding() && !HasSapper() && !IsPlasmaDisabled() )
		return 0;

	return FCAP_ONOFF_USE; 
};

//-----------------------------------------------------------------------------
// Clean off the object of offensive material...
//-----------------------------------------------------------------------------
bool CBaseObject::RemoveEnemyAttachments( CBaseEntity *pActivator )
{
	bool bRemoved = false;

	// Sapper removal
	if ( pActivator->IsPlayer() )
	{
		if ( HasSapper() )
		{
			RemoveAllSappers( pActivator );
			bRemoved = true;
		}
	}

	return bRemoved;
}


//-----------------------------------------------------------------------------
// Object using!
//-----------------------------------------------------------------------------
void CBaseObject::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// If we're friendly, pickup / remove sappers
	// If we're an enemy, plant a sapper
	if ( pActivator->IsPlayer() )
	{
		if ( InSameTeam( pActivator ) )
		{
			if ( useType == USE_ON )
			{
				// Some combat objects can be picked up
				if ( m_fObjectFlags & OF_CAN_BE_PICKED_UP )
				{
					if ( GetBuilder() == pActivator )
					{
						if ( GetBuilder()->GetPlayerClass()->ResupplyAmmoType( 1, m_szAmmoName ) )
						{
							PickupObject();
						}
						return;
					}
				}

				// Sapper removal
				if ( RemoveEnemyAttachments( pActivator ) )
					return;
			}
		}
		else
		{
			CBaseTFPlayer *pPlayer = (CBaseTFPlayer *)pActivator;

			// If we're already planting a sapper, abort
			if ( useType == USE_OFF || pPlayer->IsAttachingSapper() ) 
			{
				// Don't abort if we just started placing it. This is to catch people who'd like to +use toggle instead of hold down
				if ( pPlayer->GetSapperAttachmentTime() > 0.2 && pPlayer->IsAttachingSapper() )
				{
					pPlayer->StopAttaching();
				}
			}
			else if ( useType == USE_ON )
			{
				// Don't allow sappers to be planted on invulnerable objects
				if ( m_bInvulnerable )
					return;

				// If the object's already got a sapper from me on it, I can't put another
				if ( HasSapperFromPlayer( ((CBaseTFPlayer*)pActivator ) ) )
					return;

				Vector vecAiming;
				pPlayer->EyeVectors( &vecAiming );
				// Trace from the player to the object to find an attachment position
				trace_t tr;
				Vector vecStart = pPlayer->EyePosition();
				UTIL_TraceLine( vecStart, vecStart + (vecAiming * 256), MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );
				if ( tr.fraction < 1.0 && tr.m_pEnt == this )
				{
					CGrenadeObjectSapper *sapper = CGrenadeObjectSapper::Create( tr.endpos, vecAiming, pPlayer, this );
					pPlayer->StartAttachingSapper( this, sapper );
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Builder has picked up the object
//-----------------------------------------------------------------------------
void CBaseObject::PickupObject( void )
{
	// Tell the playerclass
	if ( GetBuilder() && GetBuilder()->GetPlayerClass() )
	{
		GetBuilder()->GetPlayerClass()->PickupObject( this );
	}

	UTIL_Remove( this );
}


//-----------------------------------------------------------------------------
// Purpose: Return true if the specified player's allowed to remove this object
//-----------------------------------------------------------------------------
bool CBaseObject::CanBeRemovedBy( CBaseTFPlayer *pPlayer )
{
	if ( m_fObjectFlags & OF_CANNOT_BE_DISMANTLED )
		return false;

	// If I'm a map-defined object, I'm not removable by anyone
	if ( WasMapPlaced() )
		return false;

	// If I have an owner, only he can remove me
	if ( GetBuilder() != pPlayer )
		return false;
	
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Return true if the specified player's allowed to rotate this object
//-----------------------------------------------------------------------------
bool CBaseObject::CanBeRotatedBy( CBaseTFPlayer *pPlayer )
{
	// If I'm a map-defined object, I'm not removable by anyone
	if ( WasMapPlaced() )
		return false;

	// If I have an owner, only he can remove me
	if ( GetBuilder() != pPlayer )
		return false;
	
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iTeamNum - 
//-----------------------------------------------------------------------------
void CBaseObject::ChangeTeam( int iTeamNum )
{
	CTFTeam *pTeam = ( CTFTeam * )GetGlobalTeam( iTeamNum );
	CTFTeam *pExisting = ( CTFTeam * )GetTeam();

	TRACE_OBJECT( UTIL_VarArgs( "%0.2f CBaseObject::ChangeTeam old %s new %s\n", gpGlobals->curtime, 
		pExisting ? pExisting->GetName() : "NULL",
		pTeam ? pTeam->GetName() : "NULL" ) );

	// Already on this team
	if ( GetTeamNumber() == iTeamNum )
		return;

	if ( pExisting )
	{
		// Remove it from current team ( if it's in one ) and give it to new team
		pExisting->RemoveObject( this );
	}
		
	// Change to new team
	BaseClass::ChangeTeam( iTeamNum );
	
	// Add this object to the team's list
	// But only if we're not placing it
	if ( pTeam && (!m_bPlacing) )
	{
		pTeam->AddObject( this );
	}

	// Setup for our new team's model
	SetupTeamModel();
	CreateBuildPoints();
	CreateVulnerablePoints();

	// Alien buildings never need power
	if ( GetTeamNumber() == TEAM_ALIENS )
	{
		m_fObjectFlags |= OF_DOESNT_NEED_POWER;
	}

	GainedNewTechnology( NULL );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CBaseObject::GetWeaponClassnameForObject( void )
{
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pNewOwner - 
//-----------------------------------------------------------------------------
void CBaseObject::AddItemsNeededForObject( CBaseTFPlayer *pNewOwner )
{
}

//-----------------------------------------------------------------------------
// Purpose: Derived classes might want to give the new builder the appropriate
//  items needed to own this object and move the objects owned over as well
// Input  : *pNewOwner - 
//-----------------------------------------------------------------------------
void CBaseObject::ChangeBuilder( CBaseTFPlayer *pNewBuilder, bool moveobjects )
{
	CBaseTFPlayer *oldBuilder = GetOwner();

	TRACE_OBJECT( UTIL_VarArgs( "%0.2f CBaseObject::ChangeBuilder old %s, new %s, moveobjects %s\n", gpGlobals->curtime, 
		oldBuilder ? oldBuilder->GetPlayerName() : "NULL",
		pNewBuilder ? pNewBuilder->GetPlayerName() : "NULL",
		moveobjects ? "true" : "false" ) );

	// Store off original builder
	if ( GetOwner() )
	{
		m_hOriginalBuilder = GetOwner();
	}

	m_hBuilder = pNewBuilder;

	if ( !moveobjects )
		return;

	if ( oldBuilder )
	{
		oldBuilder->OwnedObjectChangeTeam( this, pNewBuilder );
	}

	// For instance, if this is a mortar being added to a technician via subversion, then
	//  the "weapon_mortar" will be added to the player if the player doesn't have it.
	AddItemsNeededForObject( pNewBuilder );

	const char *classname = GetWeaponClassnameForObject();
	if ( !classname )
		return;

	TRACE_OBJECT( UTIL_VarArgs( "%0.2f CBaseObject::ChangeBuilder moving associated objects %s\n", gpGlobals->curtime, 
		classname ) );

	// Find the old player who owned a weapon that owned this object type and remove it 
	// Then add to current player under the approrpriate weapon ( same classname ) which
	//  should have been added in ChangeBuilder
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseTFPlayer *player = static_cast< CBaseTFPlayer *>( UTIL_PlayerByIndex( i ) );
		if ( !player )
			continue;

		// Cycle through weapons
		for ( int j = 0; j < player->WeaponCount(); j++ )
		{
			if ( !player->GetWeapon( j ) )
				continue;

			if ( !FClassnameIs( player->GetWeapon( j ), classname ) )
				continue;

			// Add to this player
			if ( player == pNewBuilder )
			{
				( ( CBaseTFCombatWeapon * )player->GetWeapon( j ))->AddAssociatedObject( this );
			}
			// Remove from any other
			else
			{
				( ( CBaseTFCombatWeapon * )player->GetWeapon( j ))->RemoveAssociatedObject( this );
			}
		}

	}
}


//-----------------------------------------------------------------------------
// Purpose: Return true if I have at least 1 sapper on me
//-----------------------------------------------------------------------------
bool CBaseObject::HasSapper( void )
{
	return ( m_hSappers.Size() > 0 );
}


//-----------------------------------------------------------------------------
// Purpose: Return true if the specified player has attached a sapper to me
//-----------------------------------------------------------------------------
bool CBaseObject::HasSapperFromPlayer( CBaseTFPlayer *pPlayer )
{
	for ( int i = 0; i < m_hSappers.Size(); i++ )
	{
		if ( m_hSappers[i] == NULL )
			continue;

		if ( m_hSappers[i]->GetThrower() == pPlayer )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Add a sapper to this object
//-----------------------------------------------------------------------------
void CBaseObject::AddSapper( CGrenadeObjectSapper *pSapper )
{
	SapperHandle hSapper;
	hSapper = pSapper;
	m_hSappers.AddToTail( hSapper );
	m_bHasSapper = true;
}

//-----------------------------------------------------------------------------
// Purpose: Tell all sappers on this object to remove themselves 
//-----------------------------------------------------------------------------
void CBaseObject::RemoveAllSappers( CBaseEntity *pRemovingEntity )
{
	// Loop through all the sappers and fire a +use on them (backwards because list will change)
	int iSize = m_hSappers.Size();
	for (int i = iSize-1; i >= 0; i--)
	{
		m_hSappers[i]->Use( pRemovingEntity, pRemovingEntity, USE_TOGGLE, 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Remove a sapper from this object
//-----------------------------------------------------------------------------
void CBaseObject::RemoveSapper( CGrenadeObjectSapper *pSapper )
{
	SapperHandle hSapper;
	hSapper = pSapper;
	m_hSappers.FindAndRemove( hSapper );
	m_bHasSapper = HasSapper();
}

//-----------------------------------------------------------------------------
// Purpose: My owner's just received a new technology, see if it affects me
//-----------------------------------------------------------------------------
void CBaseObject::GainedNewTechnology( CBaseTechnology *pTechnology )
{
	// Base object doesn't respond to tech
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pRecipient - 
//			*techname - 
//-----------------------------------------------------------------------------
void CBaseObject::GiveNamedTechnology( CBaseTFPlayer *pRecipient, const char *techname )
{
	CTFTeam *team = static_cast< CTFTeam * >( pRecipient->GetTeam() );
	if ( !team )
		return;

	CBaseTechnology *tech = team->m_pTechnologyTree->GetTechnology( techname );
	if ( tech )
	{
		team->EnableTechnology( tech, true );
	}
}


bool CBaseObject::ShowVGUIScreen( int panelIndex, bool bShow )
{
	Assert( panelIndex >= 0 && panelIndex < m_hScreens.Count() );
	if ( m_hScreens[panelIndex].Get() )
	{
		m_hScreens[panelIndex]->SetActive( bShow );
		return true;
	}
	else
	{
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if this object was placed in the map, not built by a player
//-----------------------------------------------------------------------------
bool CBaseObject::WasMapPlaced( void )
{
	return m_bWasMapPlaced;
}

//-----------------------------------------------------------------------------
// Purpose: Find nearby objects on my team and connect to them
//-----------------------------------------------------------------------------
CRopeKeyframe *CBaseObject::ConnectCableTo( CBaseObject *pObject, int iLocalAttachment, int iTargetAttachment )
{
	// Connect to it
	CRopeKeyframe *pRope = CRopeKeyframe::Create( this, pObject, iLocalAttachment, iTargetAttachment );
	if ( pRope )
	{
		pRope->m_Width = 3;
		pRope->m_nSegments = ROPE_MAX_SEGMENTS;
		//pRope->m_RopeFlags |= (ROPE_RESIZE | ROPE_COLLIDE);
		pRope->EnableCollision();
		pRope->EnableWind( false );
		pRope->SetupHangDistance( ROPE_HANG_DIST );
		pRope->ActivateStartDirectionConstraints( true );
		pRope->ActivateEndDirectionConstraints( true );
	}

	// Add the rope to both Object's lists
	CHandle< CRopeKeyframe > hHandle;
	hHandle = pRope;
	m_aRopes.AddToTail( hHandle );
	pObject->m_aRopes.AddToTail( hHandle );

	// During placement, the rules for whether the rope is transmitted or not are 
	// tricky, so we make a proxy here to control it.
	if ( IsPlacing() || pObject->IsPlacing() )
	{
		CObjectRopeTransmitProxy *pProxy = new CObjectRopeTransmitProxy( pRope );
		pProxy->m_hObj1 = this;
		pProxy->m_hObj2 = pObject;
		// pRope->NetworkProp()->SetTransmitProxy( pProxy ); TODO
	}

	return pRope;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if I have a cable to the specified object
//-----------------------------------------------------------------------------
bool CBaseObject::HasCableTo( CBaseObject *pObject )
{
	for (int i = 0; i < m_aRopes.Size(); i++)
	{
		CHandle< CRopeKeyframe > hHandle;
		hHandle = m_aRopes[i];
		if ( hHandle )
		{
			if ( m_aRopes[i]->GetEndPoint() == pObject )
				return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Return an attachment point for a cable
//-----------------------------------------------------------------------------
int CBaseObject::GetCableAttachment( void )
{
	Vector vecOrigin, vecAngles;
	// If I already have a rope attached, try and use a different attachment point
	if ( m_aRopes.Size() )
	{
		// First, check to see if we've lost any ropes (this can happen because 
		// the other object it was attached to has been destroyed.
		int iSize = m_aRopes.Size();
		for (int i = iSize-1; i >= 0; i--)
		{
			CHandle< CRopeKeyframe > hHandle;
			hHandle = m_aRopes[i];
			if ( hHandle == NULL )
			{
				m_aRopes.Remove(i);
			}
		}

		// If I have enough connections, tell 'em I don't want no more
		if ( m_aRopes.Size() >= MAX_CABLE_CONNECTIONS )
			return -1;

		char sAttachment[32];
		Q_snprintf( sAttachment,sizeof(sAttachment), "cablepoint%d", m_aRopes.Size() + 1 );
		int iPoint = LookupAttachment( sAttachment );
		if ( iPoint > 0 )
			return iPoint;											
	}


	return LookupAttachment( "cablepoint1" );
}


//====================================================================================================================
// POWER PACKS
//====================================================================================================================
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseObject::SetPowerPack( CObjectPowerPack *pPack )
{
	bool bHadPower = HasPowerup( POWERUP_POWER );
	CObjectPowerPack *pOldPack = m_hPowerPack;

	m_hPowerPack = pPack;

	// If it's placing, I don't get power yet
	if ( m_hPowerPack && !m_hPowerPack->IsPlacing() )
	{
		SetPowerup( POWERUP_POWER, true );
	}
	else
	{
		// Lose power in a second, to give any nearby powerpacks time to connect to me and replace the power
		if ( bHadPower )
		{
			SetContextThink( LostPowerThink, gpGlobals->curtime + 1.0, OBJ_LOSTPOWER_THINK_CONTEXT );
			if ( GetTFTeam() )
			{
				// Dirty hack to make powerpack think I need power
				m_iPowerups &= ~(1 << POWERUP_POWER);
				GetTFTeam()->UpdatePowerpacks( pOldPack, this );
			}
		}
		else
		{
			SetPowerup( POWERUP_POWER, false );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: We've lost power fully
//-----------------------------------------------------------------------------
void CBaseObject::LostPowerThink( void )
{
	// We may have found another powerpack
	if ( !m_hPowerPack )
	{
		// Dirty hack to get our powerup removed properly
		m_iPowerups |= (1 << POWERUP_POWER);
		SetPowerup( POWERUP_POWER, false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the health of the object
//-----------------------------------------------------------------------------
void CBaseObject::InputSetHealth( inputdata_t &inputdata )
{
	m_iMaxHealth = inputdata.value.Int();
	SetHealth( m_iMaxHealth );
}

//-----------------------------------------------------------------------------
// Purpose: Add health to the object
//-----------------------------------------------------------------------------
void CBaseObject::InputAddHealth( inputdata_t &inputdata )
{
	int iHealth = inputdata.value.Int();
	SetHealth( MIN( GetMaxHealth(), m_flHealth + iHealth ) );
}

//-----------------------------------------------------------------------------
// Purpose: Remove health from the object
//-----------------------------------------------------------------------------
void CBaseObject::InputRemoveHealth( inputdata_t &inputdata )
{
	int iDamage = inputdata.value.Int();

	SetHealth( m_flHealth - iDamage );
	if ( GetHealth() <= 0 )
	{
		m_lifeState = LIFE_DEAD;
		m_OnDestroyed.FireOutput(this, this);
		Killed();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CBaseObject::InputSetMinDisabledHealth( inputdata_t &inputdata )
{
	float minhealth = inputdata.value.Float();

	bool wasdisabled = IsDisabled();

	if ( m_flHealth < minhealth )
	{
		SetDisabled( true );
		// NOTE:  This could theoretically add health, sigh.
		SetHealth( minhealth );

		// Disable it if not already disabled
		if ( !wasdisabled )
		{
			m_OnBecomingDisabled.FireOutput( inputdata.pActivator, this );
		}
	}
	else if ( wasdisabled && ( m_flHealth > minhealth ) )
	{
		SetDisabled( false );

		m_OnBecomingReenabled.FireOutput( inputdata.pActivator, this );
	}

	m_flMinDisableHealth = minhealth;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CBaseObject::InputSetSolidToPlayer( inputdata_t &inputdata )
{
	int ival = inputdata.value.Int();
	ival = clamp( ival, (int)SOLID_TO_PLAYER_USE_DEFAULT, (int)SOLID_TO_PLAYER_NO );
	OBJSOLIDTYPE stp = (OBJSOLIDTYPE)ival;
	SetSolidToPlayers( stp );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseObject::PlayStartupAnimation( void )
{
	SetActivity( ACT_OBJ_STARTUP );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseObject::DetermineAnimation( void )
{
	Activity desiredActivity = m_Activity;

	switch ( m_Activity )
	{
	default:
		{
			if ( IsPlacing() )
			{
				desiredActivity = ACT_OBJ_PLACING;
			}
			else if ( IsBuilding() )
			{
				desiredActivity = ACT_OBJ_ASSEMBLING;
			}
			/*
			TODO:
				ACT_OBJ_DISMANTLING;
				ACT_OBJ_DETERIORATING;
			*/
			else
			{
				desiredActivity = ACT_OBJ_RUNNING;
			}
		}
		break;
	case ACT_OBJ_STARTUP:
		{
			if ( IsActivityFinished() )
			{
				desiredActivity = ACT_OBJ_RUNNING;
			}
		}
		break;
	}

	if ( desiredActivity == m_Activity )
		return;

	SetActivity( desiredActivity );
}

//-----------------------------------------------------------------------------
// Purpose: Attach this object to the specified object
//-----------------------------------------------------------------------------

void CBaseObject::AttachObjectToObject( const CBaseEntity *pEntity, int iPoint, Vector &vecOrigin )
{
	SetupAttachedVersion();

	m_hBuiltOnEntity = pEntity;
	m_iBuiltOnPoint = iPoint;

	if ( m_hBuiltOnEntity.Get() )
	{
		// Parent ourselves to the object
		int iAttachment = 0;
		const IHasBuildPoints *pBPInterface = dynamic_cast<const IHasBuildPoints*>( pEntity );
		if ( pBPInterface )
			iAttachment = pBPInterface->GetBuildPointAttachmentIndex( iPoint );

		SetParent( m_hBuiltOnEntity.Get(), iAttachment );

		if ( iAttachment >= 1 )
		{
			// Stick right onto the attachment point.
			vecOrigin.Init();
			SetLocalOrigin( vecOrigin );
			SetLocalAngles( QAngle(0,0,0) );
		}
		else
		{
			SetAbsOrigin( vecOrigin );
			vecOrigin = GetLocalOrigin();
		}
	}

	Assert( m_hBuiltOnEntity == GetMoveParent() );
}

//-----------------------------------------------------------------------------
// Purpose: Detach this object from its parent, if it has one
//-----------------------------------------------------------------------------
void CBaseObject::DetachObjectFromObject( void )
{
	if ( !GetParentObject() )
		return;

	// Clear the build point
	IHasBuildPoints *pBPInterface = dynamic_cast<IHasBuildPoints*>(GetParentObject() );
	Assert( pBPInterface );
	pBPInterface->SetObjectOnBuildPoint( m_iBuiltOnPoint, NULL );

	SetParent( NULL );
	m_hBuiltOnEntity = NULL;
	m_iBuiltOnPoint = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Spawn any objects specified inside the mdl
//-----------------------------------------------------------------------------
void CBaseObject::SpawnEntityOnBuildPoint( const char *pEntityName, int iAttachmentNumber )
{
	// Try and spawn the object
	CBaseEntity *pEntity = CreateEntityByName( pEntityName );
	if ( !pEntity )
		return;

	Vector vecOrigin;
	QAngle vecAngles;
	GetAttachment( iAttachmentNumber, vecOrigin, vecAngles );
	pEntity->SetAbsOrigin( vecOrigin );
	pEntity->SetAbsAngles( vecAngles );
	pEntity->Spawn();
	
	// If it's an object, finish setting it up
	CBaseObject *pObject = dynamic_cast<CBaseObject*>(pEntity);
	if ( !pObject )
		return;

	// Add a buildpoint here
	int iPoint = AddBuildPoint( iAttachmentNumber );
	AddValidObjectToBuildPoint( iPoint, pObject->GetType() );
	pObject->SetBuilder( GetBuilder() );
	pObject->ChangeTeam( GetTeamNumber() );
	pObject->SpawnControlPanels();
	pObject->SetHealth( pObject->GetMaxHealth() );
	pObject->FinishedBuilding();
	pObject->AttachObjectToObject( this, iPoint, vecOrigin );
	pObject->m_fObjectFlags |= OF_CANNOT_BE_DISMANTLED;
	pObject->AttemptToFindPower();

	IHasBuildPoints *pBPInterface = dynamic_cast<IHasBuildPoints*>(this);
	Assert( pBPInterface );
	pBPInterface->SetObjectOnBuildPoint( iPoint, pObject );
}


//-----------------------------------------------------------------------------
// Purpose: Spawn any objects specified inside the mdl
//-----------------------------------------------------------------------------
void CBaseObject::SpawnObjectPoints( void )
{
	KeyValues *modelKeyValues = new KeyValues("");
	if ( !modelKeyValues->LoadFromBuffer( modelinfo->GetModelName( GetModel() ), modelinfo->GetModelKeyValueText( GetModel() ) ) )
	{
		modelKeyValues->deleteThis();
		return;
	}

	// Do we have a build point section?
	KeyValues *pkvAllObjectPoints = modelKeyValues->FindKey("object_points");
	if ( !pkvAllObjectPoints )
	{
		modelKeyValues->deleteThis();
		return;
	}

	// Start grabbing the sounds and slotting them in
	KeyValues *pkvObjectPoint;
	for ( pkvObjectPoint = pkvAllObjectPoints->GetFirstSubKey(); pkvObjectPoint; pkvObjectPoint = pkvObjectPoint->GetNextKey() )
	{
		// Find the attachment first
		const char *sAttachment = pkvObjectPoint->GetName();
		int iAttachmentNumber = LookupAttachment( sAttachment );
		if ( iAttachmentNumber == 0 )
		{
			Msg( "ERROR: Model %s specifies object point %s, but has no attachment named %s.\n", STRING(GetModelName()), pkvObjectPoint->GetString(), pkvObjectPoint->GetString() );
			continue;
		}

		// Now see what we're supposed to spawn there
		// The count check is because it seems wrong to emit multiple entities on the same point
		int nCount = 0;
		KeyValues *pkvObject;
		for ( pkvObject = pkvObjectPoint->GetFirstSubKey(); pkvObject; pkvObject = pkvObject->GetNextKey() )
		{
			SpawnEntityOnBuildPoint( pkvObject->GetName(), iAttachmentNumber );
			++nCount;
			Assert( nCount <= 1 );
		}
	}

	modelKeyValues->deleteThis();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseObject::CreateVulnerablePoints()
{
	KeyValues *modelKeyValues = new KeyValues("");
	if ( !modelKeyValues->LoadFromBuffer( modelinfo->GetModelName( GetModel() ), modelinfo->GetModelKeyValueText( GetModel() ) ) )
		return;

	// Do we have a build point section?
	KeyValues *pkvAllVulnerablePoints = modelKeyValues->FindKey("vulnerable_points");
	if ( !pkvAllVulnerablePoints )
		return;

	// Start grabbing the sounds and slotting them in
	KeyValues *pkvVulnerablePoint = pkvAllVulnerablePoints->GetFirstSubKey();
	while ( pkvVulnerablePoint )
	{
		AddVulnerablePoint( pkvVulnerablePoint->GetName(), pkvVulnerablePoint->GetFloat() );
		pkvVulnerablePoint = pkvVulnerablePoint->GetNextKey();
	}

}

ConVar obj_debug_vulnerable( "obj_debug_vulnerable","0", FCVAR_NONE, "Show vulnerable points" );

void CBaseObject::AddVulnerablePoint( const char* szName, float fMultiplier )
{
	// Make a new vulnerable point
	VulnerablePoint_t v;
	Q_memset(&v,0,sizeof(v));
	v.m_fDamageMultiplier = fMultiplier;

	int nSet, nBox;

	if( !LookupHitbox(szName, nSet, nBox) )
	{
		Msg( "Error: Vulnerable point on model %s unable to locate hitbox %s\n", STRING(GetModelName()), szName);
		return;
	}

	v.m_nSet = nSet;
	v.m_nBox = nBox;

	// Insert it into our list
	if( obj_debug_vulnerable.GetBool() )
	{
		Msg( "Vulnerable point %s on model %s added with a damage multiplier of %f. (%i, %i)\n", szName, STRING(GetModelName()), fMultiplier, nSet, nBox);
	}

	m_VulnerablePoints.AddToTail( v );
}


float CBaseObject::FindVulnerablePointMultiplier( int nGroup, int nBox ) 		
{
	for( int i=0; i < m_VulnerablePoints.Count(); i++ )
	{
		VulnerablePoint_t& v = m_VulnerablePoints[i];

		if( v.m_nBox == nBox /* && v.m_nSet == nGroup */)
		{
			if( obj_debug_vulnerable.GetBool() )
			{
				Msg("VulnerablePoint: %f\n",v.m_fDamageMultiplier);
			}
			return v.m_fDamageMultiplier;
		}
	}

	if( obj_debug_vulnerable.GetBool() )
	{
		Msg("Couldn't find vulnerable point: %i %i\n",nGroup,nBox);
	}
	return 1.f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
QAngle CBaseObject::ConvertAbsAnglesToLocal( QAngle vecLocalAngles )
{
	if ( !GetMoveParent() )
		return vecLocalAngles;

	EntityMatrix vehicleToWorld, childMatrix;
	vehicleToWorld.InitFromEntity( GetMoveParent() ); // parent->world

	// Calculate the build point angles in vehicle space
	VMatrix attachmentToWorld;
	MatrixFromAngles( vecLocalAngles, attachmentToWorld );
	
	VMatrix worldToVehicle = vehicleToWorld.Transpose();
	VMatrix attachmentToVehicle;
	MatrixMultiply( worldToVehicle, attachmentToWorld, attachmentToVehicle );
	QAngle vecAbsAngles;
	MatrixToAngles( attachmentToVehicle, vecAbsAngles );

	return vecAbsAngles;
}

bool CBaseObject::IsSolidToPlayers( void ) const
{
	switch ( m_SolidToPlayers )
	{
	default:
		break;
	case SOLID_TO_PLAYER_USE_DEFAULT:
		{
			if ( GetObjectInfo( ObjectType() ) )
			{
				return GetObjectInfo( ObjectType() )->m_bSolidToPlayerMovement;
			}
		}
		break;
	case SOLID_TO_PLAYER_YES:
		return true;
	case SOLID_TO_PLAYER_NO:
		return false;
	}

	return false;
}

void CBaseObject::SetSolidToPlayers( OBJSOLIDTYPE stp, bool force )
{
	bool changed = stp != m_SolidToPlayers;
	m_SolidToPlayers = stp;

	if ( changed || force )
	{
		SetCollisionGroup( 
			IsSolidToPlayers() ? 
				TFCOLLISION_GROUP_OBJECT_SOLIDTOPLAYERMOVEMENT : 
				TFCOLLISION_GROUP_OBJECT );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Hooks to support swapping out model if the object is damaged
// Input  : bDisabled - 
//-----------------------------------------------------------------------------
void CBaseObject::SetDisabled( bool bDisabled )
{ 
	bool changed = m_bDisabled != bDisabled;
	m_bDisabled = bDisabled;

	// value changed and mapper specified a "disabled"/damaged model
	if ( changed && NULL_STRING != m_iszDisabledModel )
	{
		// Change model
		SetModel( STRING( m_bDisabled ? m_iszDisabledModel : m_iszEnabledModel ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseObject::SetBuffStation( CObjectBuffStation *pBuffStation, bool bPlacing )	
{ 
	// Activate
	if ( pBuffStation && !GetBuffStation() && !bPlacing && !IsBuilding() && IsPowered() )
	{
		BuffStationActivate();
	}

	if ( GetBuffStation() && ( pBuffStation == GetBuffStation() ) && !m_bBuffActivated && !bPlacing && !IsBuilding() && IsPowered() )
	{
		BuffStationActivate();
	}

	// Deactivate
	if ( !pBuffStation && GetBuffStation() )
	{
		BuffStationDeactivate();
	}

	m_hBuffStation = pBuffStation; 
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CBaseObject::IsHookedAndBuffed( void )												
{ 
	if ( GetBuffStation() && HasPowerup( POWERUP_BOOST ) )
	{
		if ( GetBuffStation()->IsPowered() && !GetBuffStation()->IsPlacing() &&
			 !GetBuffStation()->IsBuilding() )
			return true;
	}

	return false;
}
