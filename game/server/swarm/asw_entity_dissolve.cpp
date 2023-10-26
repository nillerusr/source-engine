#include "cbase.h"
#include "asw_entity_dissolve.h"
#include "baseanimating.h"
#include "physics_prop_ragdoll.h"
#include "ai_basenpc.h"
#include "particle_parse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static const char *s_pElectroThinkContext = "ElectroThinkContext";

// ASW - Custom version of the entity dissolve effect, used by alien goo when it fades out (doesn't have sparks, etc.)

//-----------------------------------------------------------------------------
// Lifetime 
//-----------------------------------------------------------------------------
#define ASW_DISSOLVE_FADE_IN_START_TIME			0.0f
#define ASW_DISSOLVE_FADE_IN_END_TIME			1.0f
#define ASW_DISSOLVE_FADE_OUT_MODEL_START_TIME	1.0f
#define ASW_DISSOLVE_FADE_OUT_MODEL_END_TIME	2.5f
#define ASW_DISSOLVE_FADE_OUT_START_TIME		1.0f
#define ASW_DISSOLVE_FADE_OUT_END_TIME			2.5f

//-----------------------------------------------------------------------------
// Model 
//-----------------------------------------------------------------------------
#define DISSOLVE_SPRITE_NAME	"sprites/blueglow1.vmt"	

//-----------------------------------------------------------------------------
// Save/load 
//-----------------------------------------------------------------------------
BEGIN_DATADESC( CASW_Entity_Dissolve )

DEFINE_FIELD( m_flStartTime, FIELD_TIME ),
DEFINE_FIELD( m_flFadeInStart, FIELD_FLOAT ),
DEFINE_FIELD( m_flFadeInLength, FIELD_FLOAT ),
DEFINE_FIELD( m_flFadeOutModelStart, FIELD_FLOAT ),
DEFINE_FIELD( m_flFadeOutModelLength, FIELD_FLOAT ),
DEFINE_FIELD( m_flFadeOutStart, FIELD_FLOAT ),
DEFINE_FIELD( m_flFadeOutLength, FIELD_FLOAT ),
DEFINE_FIELD( m_nDissolveType, FIELD_INTEGER ),

DEFINE_FUNCTION( DissolveThink ),
DEFINE_FUNCTION( ElectrocuteThink ),

DEFINE_INPUTFUNC( FIELD_STRING, "Dissolve", InputDissolve ),

END_DATADESC()


//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_SERVERCLASS_ST( CASW_Entity_Dissolve, DT_ASW_Entity_Dissolve )
SendPropTime( SENDINFO( m_flStartTime ) ),
SendPropFloat( SENDINFO( m_flFadeInStart ), 0, SPROP_NOSCALE ),
SendPropFloat( SENDINFO( m_flFadeInLength ), 0, SPROP_NOSCALE ),
SendPropFloat( SENDINFO( m_flFadeOutModelStart ), 0, SPROP_NOSCALE ),
SendPropFloat( SENDINFO( m_flFadeOutModelLength ), 0, SPROP_NOSCALE ),
SendPropFloat( SENDINFO( m_flFadeOutStart ), 0, SPROP_NOSCALE ),
SendPropFloat( SENDINFO( m_flFadeOutLength ), 0, SPROP_NOSCALE ),
SendPropInt( SENDINFO( m_nDissolveType ), ENTITY_DISSOLVE_BITS, SPROP_UNSIGNED ),
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( asw_entity_dissolve, CASW_Entity_Dissolve );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CASW_Entity_Dissolve::CASW_Entity_Dissolve( void )
{
	m_flStartTime	= 0.0f;
}

CASW_Entity_Dissolve::~CASW_Entity_Dissolve( void )
{
}


//-----------------------------------------------------------------------------
// Precache
//-----------------------------------------------------------------------------
void CASW_Entity_Dissolve::Precache()
{
	if ( NULL_STRING == GetModelName() )
	{
		PrecacheModel( DISSOLVE_SPRITE_NAME );
	}
	else
	{
		PrecacheModel( STRING( GetModelName() ) );
	}
}


//-----------------------------------------------------------------------------
// Spawn
//-----------------------------------------------------------------------------
void CASW_Entity_Dissolve::Spawn()
{
	BaseClass::Spawn();
	Precache();

	if ( NULL_STRING != GetModelName() )
	{
		UTIL_SetModel( this, STRING( GetModelName() ) );
	}

	if ( (m_nDissolveType == ENTITY_DISSOLVE_ELECTRICAL) || (m_nDissolveType == ENTITY_DISSOLVE_ELECTRICAL_LIGHT) )
	{
		if ( dynamic_cast< CRagdollProp* >( GetMoveParent() ) )
		{
			SetContextThink( &CASW_Entity_Dissolve::ElectrocuteThink, gpGlobals->curtime + 0.01f, s_pElectroThinkContext );
		}
	}

	// Setup our times
	m_flFadeInStart = ASW_DISSOLVE_FADE_IN_START_TIME;
	m_flFadeInLength = ASW_DISSOLVE_FADE_IN_END_TIME - ASW_DISSOLVE_FADE_IN_START_TIME;

	m_flFadeOutModelStart = ASW_DISSOLVE_FADE_OUT_MODEL_START_TIME;
	m_flFadeOutModelLength = ASW_DISSOLVE_FADE_OUT_MODEL_END_TIME - ASW_DISSOLVE_FADE_OUT_MODEL_START_TIME;

	m_flFadeOutStart = ASW_DISSOLVE_FADE_OUT_START_TIME;
	m_flFadeOutLength = ASW_DISSOLVE_FADE_OUT_END_TIME - ASW_DISSOLVE_FADE_OUT_START_TIME;

	m_nRenderMode = kRenderTransColor;
	SetRenderColor( 255, 255, 255 );
	SetRenderAlpha( 255 );
	m_nRenderFX = kRenderFxNone;

	// Turn them into debris
	CBaseAnimating *pTarget = ( GetMoveParent() ) ? GetMoveParent()->GetBaseAnimating() : NULL;
	if ( pTarget )
	{
		pTarget->SetCollisionGroup( COLLISION_GROUP_DISSOLVING );
	}

	SetThink( &CASW_Entity_Dissolve::DissolveThink );
	if ( gpGlobals->curtime > m_flStartTime )
	{
		// Necessary for server-side ragdolls
		DissolveThink();
	}
	else
	{
		SetNextThink( gpGlobals->curtime + 0.01f );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : inputdata - 
//-----------------------------------------------------------------------------
void CASW_Entity_Dissolve::InputDissolve( inputdata_t &inputdata )
{
	string_t strTarget = inputdata.value.StringID();

	if (strTarget == NULL_STRING)
	{
		strTarget = m_target;
	}

	CBaseEntity *pTarget = NULL;
	while ((pTarget = gEntList.FindEntityGeneric(pTarget, STRING(strTarget), this, inputdata.pActivator)) != NULL)
	{
		// Combat characters know how to catch themselves on fire.
		CBaseAnimating *pBaseAnim = dynamic_cast<CBaseAnimating*>(pTarget);
		if (pBaseAnim)
		{
			pBaseAnim->Dissolve( NULL, gpGlobals->curtime, false );
		}
	}
}



//-----------------------------------------------------------------------------
// Purpose: Creates a flame and attaches it to a target entity.
// Input  : pTarget - 
//-----------------------------------------------------------------------------
CASW_Entity_Dissolve *CASW_Entity_Dissolve::Create( CBaseEntity *pTarget, const char *pMaterialName, 
												   float flStartTime, int nDissolveType, bool *pRagdollCreated )
{
	if ( pRagdollCreated )
	{
		*pRagdollCreated = false;
	}

	if ( !pMaterialName )
	{
		pMaterialName = DISSOLVE_SPRITE_NAME;
	}

	if ( pTarget->IsPlayer() )
	{
		// Simply immediately kill the player.
		CBasePlayer *pPlayer = assert_cast< CBasePlayer* >( pTarget );
		pPlayer->SetArmorValue( 0 );
		CTakeDamageInfo info( pPlayer, pPlayer, pPlayer->GetHealth(), DMG_GENERIC | DMG_REMOVENORAGDOLL | DMG_PREVENT_PHYSICS_FORCE );
		pPlayer->TakeDamage( info );
		return NULL;
	}

	CASW_Entity_Dissolve *pDissolve = (CASW_Entity_Dissolve *) CreateEntityByName( "asw_entity_dissolve" );

	if ( pDissolve == NULL )
		return NULL;

	pDissolve->m_nDissolveType = nDissolveType;
	if ( (nDissolveType == ENTITY_DISSOLVE_ELECTRICAL) || (nDissolveType == ENTITY_DISSOLVE_ELECTRICAL_LIGHT) )
	{
		if ( pTarget->IsNPC() && pTarget->MyNPCPointer()->CanBecomeRagdoll() )
		{
			CTakeDamageInfo info;
			CBaseEntity *pRagdoll = CreateServerRagdoll( pTarget->MyNPCPointer(), 0, info, COLLISION_GROUP_DEBRIS, true );
			pRagdoll->SetCollisionBounds( pTarget->CollisionProp()->OBBMins(), pTarget->CollisionProp()->OBBMaxs() );

			// Necessary to cause it to do the appropriate death cleanup
			if ( pTarget->m_lifeState == LIFE_ALIVE )
			{
				CBasePlayer *pPlayer = UTIL_PlayerByIndex( 1 );
				CTakeDamageInfo ragdollInfo( pPlayer, pPlayer, 10000.0, DMG_SHOCK | DMG_REMOVENORAGDOLL | DMG_PREVENT_PHYSICS_FORCE );
				pTarget->TakeDamage( ragdollInfo );
			}

			if ( pRagdollCreated )
			{
				*pRagdollCreated = true;
			}

			UTIL_Remove( pTarget );
			pTarget = pRagdoll;
		}
	}

	if ( nDissolveType == ENTITY_DISSOLVE_NORMAL ) // this is used by the biomass
	{
		DispatchParticleEffect( "biomass_dissolve", PATTACH_ABSORIGIN, pTarget );
	}

	//pDissolve->SetModelName( AllocPooledString(pMaterialName) );
	pDissolve->AttachToEntity( pTarget );
	pDissolve->SetStartTime( flStartTime );
	pDissolve->Spawn();

	// Send to the client even though we don't have a model
	pDissolve->AddEFlags( EFL_FORCE_CHECK_TRANSMIT );

	// Play any appropriate noises when we start to dissolve
	if ( (nDissolveType == ENTITY_DISSOLVE_ELECTRICAL) || (nDissolveType == ENTITY_DISSOLVE_ELECTRICAL_LIGHT) )
	{
		pTarget->DispatchResponse( "TLK_ELECTROCUTESCREAM" );
	}
	else
	{
		pTarget->DispatchResponse( "TLK_DISSOLVESCREAM" );
	}
	return pDissolve;
}


//-----------------------------------------------------------------------------
// What type of dissolve?
//-----------------------------------------------------------------------------
CASW_Entity_Dissolve *CASW_Entity_Dissolve::Create( CBaseEntity *pTarget, CBaseEntity *pSource )
{
	// Look for other boogies on the ragdoll + kill them
	for ( CBaseEntity *pChild = pSource->FirstMoveChild(); pChild; pChild = pChild->NextMovePeer() )
	{
		CASW_Entity_Dissolve *pDissolve = dynamic_cast<CASW_Entity_Dissolve*>(pChild);
		if ( !pDissolve )
			continue;

		return Create( pTarget, STRING( pDissolve->GetModelName() ), pDissolve->m_flStartTime, pDissolve->m_nDissolveType );
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Attaches the flame to an entity and moves with it
// Input  : pTarget - target entity to attach to
//-----------------------------------------------------------------------------
void CASW_Entity_Dissolve::AttachToEntity( CBaseEntity *pTarget )
{
	// So our dissolver follows the entity around on the server.
	SetParent( pTarget );
	SetLocalOrigin( vec3_origin );
	SetLocalAngles( vec3_angle );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : lifetime - 
//-----------------------------------------------------------------------------
void CASW_Entity_Dissolve::SetStartTime( float flStartTime )
{
	m_flStartTime = flStartTime;
}

//-----------------------------------------------------------------------------
// Purpose: Burn targets around us
//-----------------------------------------------------------------------------
void CASW_Entity_Dissolve::DissolveThink( void )
{
	CBaseAnimating *pTarget = ( GetMoveParent() ) ? GetMoveParent()->GetBaseAnimating() : NULL;

	if ( GetModelName() == NULL_STRING && pTarget == NULL )
		return;

	if ( pTarget == NULL )
	{
		UTIL_Remove( this );
		return;
	}

	if ( pTarget && pTarget->GetFlags() & FL_TRANSRAGDOLL )
	{
		SetRenderAlpha( 0 );
	}

	float dt = gpGlobals->curtime - m_flStartTime;

	if ( dt < m_flFadeInStart )
	{
		SetNextThink( m_flStartTime + m_flFadeInStart );
		return;
	}

	// If we're done fading, then kill our target entity and us
	if ( dt >= m_flFadeOutStart + m_flFadeOutLength )
	{
		// Necessary to cause it to do the appropriate death cleanup
		// Yeah, the player may have nothing to do with it, but
		// passing NULL to TakeDamage causes bad things to happen
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( 1 );
		int iNoPhysicsDamage = g_pGameRules->Damage_GetNoPhysicsForce();
		CTakeDamageInfo info( pPlayer, pPlayer, 10000.0, DMG_GENERIC | DMG_REMOVENORAGDOLL | iNoPhysicsDamage );
		pTarget->TakeDamage( info );

		if ( pTarget != pPlayer )
		{
			UTIL_Remove( pTarget );
		}

		UTIL_Remove( this );

		return;
	}

	SetNextThink( gpGlobals->curtime + TICK_INTERVAL );
}


//-----------------------------------------------------------------------------
// Purpose: Burn targets around us
//-----------------------------------------------------------------------------
void CASW_Entity_Dissolve::ElectrocuteThink( void )
{
	CRagdollProp *pRagdoll = dynamic_cast< CRagdollProp* >( GetMoveParent() );
	if ( !pRagdoll )
		return;

	ragdoll_t *pRagdollPhys = pRagdoll->GetRagdoll( );
	for ( int j = 0; j < pRagdollPhys->listCount; ++j )
	{
		Vector vecForce;
		vecForce = RandomVector( -2400.0f, 2400.0f );
		pRagdollPhys->list[j].pObject->ApplyForceCenter( vecForce ); 
	}

	SetContextThink( &CASW_Entity_Dissolve::ElectrocuteThink, gpGlobals->curtime + random->RandomFloat( 0.1, 0.2f ), 
		s_pElectroThinkContext );
}
