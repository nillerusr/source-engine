#include "cbase.h"
#include "asw_aoegrenade_projectile.h"
#include "asw_marine.h"
#include "Sprite.h"
#include "SpriteTrail.h"
#include "soundent.h"
#include "te_effect_dispatch.h"
#include "IEffects.h"
#include "decals.h"
#include "asw_shareddefs.h"
#include "particle_parse.h"
#include "triggers.h"
#include "world.h"
#include "asw_game_resource.h"
#include "asw_marine_resource.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define AOEGREN_MODEL "models/items/Mine/mine.mdl"


//-----------------------------------------------------------------------------
// Purpose: SendProxy that converts the AOEGren list UtlVector to entindices
//-----------------------------------------------------------------------------
void SendProxy_AOEGrenList( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID )
{
	CASW_AOEGrenade_Projectile *pAOEGren = (CASW_AOEGrenade_Projectile*)pStruct;

	// If this assertion fails, then SendProxyArrayLength_AOEGrenArray must have failed.
	Assert( iElement < pAOEGren->m_hAOETargets.Count() );

	CBaseEntity *pEnt = pAOEGren->m_hAOETargets[iElement].Get();
	EHANDLE hOther = pEnt;

	SendProxy_EHandleToInt( pProp, pStruct, &hOther, pOut, iElement, objectID );
}

int SendProxyArrayLength_AOEGrenArray( const void *pStruct, int objectID )
{
	CASW_AOEGrenade_Projectile *pAOEGren = (CASW_AOEGrenade_Projectile*)pStruct;
	return pAOEGren->m_hAOETargets.Count();
}


class CASW_AOEGrenade_TouchTrigger : public CBaseTrigger
{
	DECLARE_CLASS( CASW_AOEGrenade_TouchTrigger, CBaseTrigger );

public:
	CASW_AOEGrenade_TouchTrigger() {}

	void Spawn( void )
	{
		BaseClass::Spawn();
		// SF_TRIGGER_ONLY_PLAYER_ALLY_NPCS means "only marines" in ASW
		AddSpawnFlags( SF_TRIGGER_ONLY_PLAYER_ALLY_NPCS );
		InitTrigger();
	}

	virtual void StartTouch( CBaseEntity *pEntity )
	{
		CASW_AOEGrenade_Projectile *pParent = dynamic_cast<CASW_AOEGrenade_Projectile *>(GetOwnerEntity());

		if ( pParent )
		{
			pParent->StartTouch( pEntity );
		}
	}

	virtual void EndTouch( CBaseEntity *pEntity )
	{
		CASW_AOEGrenade_Projectile *pParent = dynamic_cast<CASW_AOEGrenade_Projectile *>(GetOwnerEntity());

		if ( pParent )
		{
			pParent->EndTouch( pEntity );
		}
	}
};


LINK_ENTITY_TO_CLASS( asw_aoegrenade_touch_trigger, CASW_AOEGrenade_TouchTrigger );

BEGIN_DATADESC( CASW_AOEGrenade_Projectile )
	DEFINE_FUNCTION( AOEGrenadeTouch ),
	DEFINE_FUNCTION( AOEGrenadeThink ),

	// Fields
	DEFINE_FIELD( m_pMainGlow, FIELD_EHANDLE ),
	DEFINE_FIELD( m_pGlowTrail, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flTimeBurnOut, FIELD_TIME ),
	DEFINE_FIELD( m_flTimePulse, FIELD_TIME ),
	DEFINE_FIELD( m_flLastDoAOE, FIELD_TIME ),
	DEFINE_FIELD( m_flScale, FIELD_FLOAT ),
	DEFINE_FIELD( m_nBounces, FIELD_INTEGER ),
	DEFINE_FIELD( m_flDuration, FIELD_FLOAT ),
	DEFINE_FIELD( m_bFading, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bSettled, FIELD_BOOLEAN ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CASW_AOEGrenade_Projectile, DT_ASW_AOEGrenade_Projectile )
	SendPropArray2( 
			   SendProxyArrayLength_AOEGrenArray,
			   SendPropInt( "aoegren_array_element", 0, SIZEOF_IGNORE, NUM_NETWORKED_EHANDLE_BITS, SPROP_UNSIGNED, SendProxy_AOEGrenList), 
			   MAX_PLAYERS, 
			   0, 
			   "aoetarget_array"
			   ),

	SendPropFloat( SENDINFO( m_flTimeBurnOut ), 0,	SPROP_NOSCALE ),
	//SendPropFloat( SENDINFO( m_flTimePulse ), 0,	SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flScale ), 0, SPROP_NOSCALE ),
	SendPropBool( SENDINFO( m_bSettled ) ),
	SendPropFloat( SENDINFO( m_flRadius ), 0, SPROP_NOSCALE ),
END_SEND_TABLE()


// aoegrenades maintain a linked list of themselves, for quick checking for autoaim
CASW_AOEGrenade_Projectile* g_pHeadAOEGrenade = NULL;

CASW_AOEGrenade_Projectile::CASW_AOEGrenade_Projectile()
{
	m_flScale		= 1.0f;
	m_flNextDamage	= gpGlobals->curtime;
	m_lifeState		= LIFE_ALIVE;
	m_iHealth		= 100;
	m_flDuration    = 20.0f;
	m_flRadius		= 128.0f;

	SetModelName( MAKE_STRING( AOEGREN_MODEL ) );
}

CASW_AOEGrenade_Projectile::~CASW_AOEGrenade_Projectile( void )
{	
	if ( m_hTouchTrigger.Get() )
	{
		UTIL_Remove( m_hTouchTrigger );
	}

	int iSize = m_hAOETargets.Count();
	for ( int i = iSize - 1; i >= 0; i-- )
	{
		EHANDLE hEntity = m_hAOETargets[i];
		StopAOE( hEntity.Get() );
	}
}

int CASW_AOEGrenade_Projectile::Restore( IRestore &restore )
{
	int result = BaseClass::Restore( restore );
	return result;	
}

void CASW_AOEGrenade_Projectile::Spawn( void )
{
	Precache( );

	SetModel( GetModelName().ToCStr() );
	UTIL_SetSize( this, Vector( -2, -2, -2 ), Vector( 2, 2, 2 ) );
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_SOLID );

	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );

	//m_iDamage		= 0;
	m_takedamage	= DAMAGE_NO;

	SetFriction( 0.6f );
	SetGravity( UTIL_ScaleForGravity( GetGrenadeGravity() ) );
	m_flTimeBurnOut = gpGlobals->curtime + GetBurnDuration();
	m_flTimePulse = -1;

	AddEffects( EF_NOSHADOW|EF_NORECEIVESHADOW );
	AddFlag( FL_OBJECT );
	SetCollisionGroup( ASW_COLLISION_GROUP_IGNORE_NPCS );

	// Tumble in air
	QAngle vecAngVelocity( 0, random->RandomFloat ( -100, -500 ), 0 );
	SetLocalAngularVelocity( vecAngVelocity );
	
	SetTouch( &CASW_AOEGrenade_Projectile::AOEGrenadeTouch );

	SetThink( &CASW_AOEGrenade_Projectile::AOEGrenadeThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CASW_AOEGrenade_Projectile::Precache( void )
{
	PrecacheModel( GetModelName().ToCStr() );

	PrecacheScriptSound( "ASW_BuffGrenade.GrenadeActivate" );
	PrecacheScriptSound( "ASW_BuffGrenade.ActiveLoop" );
	PrecacheScriptSound( "ASW_BuffGrenade.StartBuff" );
	PrecacheScriptSound( "ASW_BuffGrenade.BuffLoop" );

	PrecacheModel( "swarm/sprites/whiteglow1.vmt" );
	PrecacheModel( "swarm/sprites/greylaser1.vmt" );

	BaseClass::Precache();
}

const Vector& CASW_AOEGrenade_Projectile::GetEffectOrigin()
{
	static Vector s_vecEffectPos;
	Vector forward, right, up;
	AngleVectors(GetAbsAngles(), &forward, &right, &up);
	s_vecEffectPos = GetAbsOrigin() + up * 5;
	return s_vecEffectPos;
}


unsigned int CASW_AOEGrenade_Projectile::PhysicsSolidMaskForEntity( void ) const
{
	return MASK_NPCSOLID;
}

void CASW_AOEGrenade_Projectile::AOEGrenadeTouch( CBaseEntity *pOther )
{
	Assert( pOther );
	if ( !pOther->IsSolid() )
		return;

	// don't touch npcs
	if ( pOther->IsNPC() )
		return;

	// Change our flight characteristics
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE );
	SetGravity( UTIL_ScaleForGravity( 640 ) );

	m_nBounces++;

	//After the first bounce, smacking into whoever fired the flare is fair game
	//SetOwnerEntity( NULL );	

	// Slow down
	Vector vecNewVelocity = GetAbsVelocity();
	vecNewVelocity.x *= 0.8f;
	vecNewVelocity.y *= 0.8f;
	SetAbsVelocity( vecNewVelocity );

	//Stopped?
	if ( GetAbsVelocity().Length() < 128.0f )
	{
		LayFlat();
		SetAbsVelocity( vec3_origin );
		SetMoveType( MOVETYPE_NONE );
		//RemoveSolidFlags( FSOLID_NOT_SOLID );
		//AddSolidFlags( FSOLID_TRIGGER );
		//SetTouch( &CASW_Flare_Projectile::FlareBurnTouch );
		m_hTouchTrigger = CBaseEntity::Create( "asw_aoegrenade_touch_trigger", GetAbsOrigin(), vec3_angle, this );
		int radius = GetEffectRadius();
		UTIL_SetSize( m_hTouchTrigger.Get(), Vector(-radius,-radius,-radius), Vector(radius,radius,radius) );
		m_hTouchTrigger->SetSolid( SOLID_BBOX );

		// call StartTouch() on all marines in radius
		CASW_Game_Resource *pGameResource = ASWGameResource();
		for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
		{
			CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
			if (!pMR)
				continue;
			CASW_Marine *pMarine = pMR->GetMarineEntity();
			if (!pMarine)
				continue;

			if( GetDistanceToEntity( pMarine ) < m_flRadius )
			{
				CBaseEntity *pEntity = pMarine;
				StartTouch( pEntity );
			}
		}

		//NDebugOverlay::Box( GetAbsOrigin(), Vector(-radius,-radius,-radius), Vector(radius,radius,radius), 0, 0, 255, 200, 5.5 );

		Assert( m_hTouchTrigger.Get() );

		m_bSettled = true;
	}
}

void CASW_AOEGrenade_Projectile::AOEGrenadeThink( void )
{
	if ( m_flTimeBurnOut != -1.0f )
	{
		//Burned out
		if ( m_flTimeBurnOut < gpGlobals->curtime )
		{
			OnBurnout();
			UTIL_Remove( this );
			return;
		}
	}
	
	//Act differently underwater
	if ( GetWaterLevel() > 1 )
	{
		UTIL_Bubbles( GetEffectOrigin() + Vector( -2, -2, -2 ), GetEffectOrigin() + Vector( 2, 2, 2 ), 1 );
	}

	if ( !m_bSettled && GetAbsVelocity().Length() < 128.0f && GetGroundEntity() )
	{
		AOEGrenadeTouch( GetGroundEntity() );
	}

	GiveEffectToEntitesInRadius();

	//Next update
	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CASW_AOEGrenade_Projectile::GiveEffectToEntitesInRadius( void )
{
	bool bDoAOE = ( gpGlobals->curtime > m_flLastDoAOE + GetDoAOEDelayTime() );

	// for each player in touching list
	int iSize = m_hTouchingEntities.Count();
	for ( int i = iSize-1; i >= 0; i-- )
	{
		EHANDLE hEntity = m_hTouchingEntities[i];

		CBaseEntity *pEntity = hEntity.Get();
		if ( !pEntity )
			continue;

		if ( !IsAOETarget( pEntity ) )
		{
			// if they're on the list, but we aren't buffing them, do so
			StartAOE( pEntity );
		}

		if ( bDoAOE )
		{
			DoAOE( pEntity );
		}
	}

	if ( bDoAOE )
	{
		m_flLastDoAOE = gpGlobals->curtime;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_AOEGrenade_Projectile::StartTouch( CBaseEntity *pEntity )
{
	if ( !ShouldTouchEntity( pEntity ) )
		return;

	// add to touching entities
	EHANDLE hEntity = pEntity;
	if( m_hTouchingEntities.Find( hEntity ) == -1 )
	{
		m_hTouchingEntities.AddToTail( hEntity );

		// try to start healing them
		StartAOE( pEntity );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_AOEGrenade_Projectile::EndTouch( CBaseEntity *pEntity )
{
	// remove from touching entities
	EHANDLE hEntity = pEntity;
	m_hTouchingEntities.FindAndRemove( hEntity );

	// remove from healing list
	StopAOE( pEntity );
}

//-----------------------------------------------------------------------------
// Purpose: Add the buff to this target
//-----------------------------------------------------------------------------
void CASW_AOEGrenade_Projectile::StartAOE( CBaseEntity *pEntity )
{
	AddAOETarget( pEntity );
}

//-----------------------------------------------------------------------------
// Purpose: Stop healing this target
//-----------------------------------------------------------------------------
bool CASW_AOEGrenade_Projectile::StopAOE( CBaseEntity *pEntity )
{
	bool bFound = false;

	EHANDLE hEntity = pEntity;
	bFound = m_hAOETargets.FindAndRemove( hEntity );
	NetworkStateChanged();

	return bFound;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_AOEGrenade_Projectile::AddAOETarget( CBaseEntity *pEntity )
{
	Assert( pEntity );
	// add to tail
	EHANDLE hEntity = pEntity;
	m_hAOETargets.AddToTail( hEntity );
	NetworkStateChanged();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_AOEGrenade_Projectile::RemoveAOETarget( CBaseEntity *pEntity )
{
	Assert( pEntity );
	// remove
	EHANDLE hEntity = pEntity;
	m_hAOETargets.FindAndRemove( hEntity );
	NetworkStateChanged();
}

//-----------------------------------------------------------------------------
// Purpose: Are we buffing this target already
//-----------------------------------------------------------------------------
bool CASW_AOEGrenade_Projectile::IsAOETarget( CBaseEntity *pEntity  )
{
	Assert( pEntity );
	EHANDLE hEntity = pEntity;
	return m_hAOETargets.HasElement( hEntity );
}

void CASW_AOEGrenade_Projectile::LayFlat()
{
	QAngle angFacing = GetAbsAngles();
	//if (angFacing[PITCH] > 0 && angFacing[PITCH] < 180.0f)
		angFacing[PITCH] = 0;  //90
	//else
	//	angFacing[PITCH] = 270;
	SetAbsAngles(angFacing);
	//Msg("Laying flat to %f, %f, %f\n", angFacing[PITCH], angFacing[YAW], angFacing[ROLL]);
}

//extern ConVar asw_aoegrenade_autoaim_radius;
void CASW_AOEGrenade_Projectile::DrawDebugGeometryOverlays()
{
/*
	// draw arrows showing the extent of our autoaim
	for (int i=0;i<360;i+=45)
	{
		float flBaseSize = 10;
		float flHeight = asw_aoegrenade_autoaim_radius.GetFloat();

		Vector vBasePos = GetAbsOrigin() + Vector( 0, 0, 5 );
		QAngle angles( 0, 0, 0 );
		Vector vForward, vRight, vUp;
		angles[YAW] = i;
		AngleVectors( angles, &vForward, &vRight, &vUp );
		NDebugOverlay::Triangle( vBasePos+vRight*flBaseSize/2, vBasePos-vRight*flBaseSize/2, vBasePos+vForward*flHeight, 0, 255, 0, 255, false, 10 );		
	}
*/
	BaseClass::DrawDebugGeometryOverlays();
}