#include "cbase.h"
#include "engine/IEngineSound.h"
#include "asw_shareddefs.h"
#include "basegrenade_shared.h"
#include "Sprite.h"
#include "asw_bouncing_pellet_shared.h"
#ifdef CLIENT_DLL
#include "c_asw_marine.h"
#include "particles_simple.h"
#include "c_asw_trail_beam.h"
#include "idebugoverlaypanel.h"
#include "engine/IVDebugOverlay.h"
#define CASW_Marine C_ASW_Marine
#else
#include "asw_marine.h"
#include "iasw_spawnable_npc.h"
#include "IEffects.h"
#include "te_effect_dispatch.h"
#endif
#include "asw_gamerules.h"
#include "util_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define PELLET_MODEL "models/swarm/Shotgun/ShotgunPellet.mdl"

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Bouncing_Pellet, DT_ASW_Bouncing_Pellet );

BEGIN_NETWORK_TABLE( CASW_Bouncing_Pellet, DT_ASW_Bouncing_Pellet )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( asw_bouncing_pellet, CASW_Bouncing_Pellet );
PRECACHE_REGISTER(asw_bouncing_pellet);

CASW_Bouncing_Pellet::CASW_Bouncing_Pellet()
{
	m_hLastHit = NULL;
	m_iBounces = 2;

#ifdef CLIENT_DLL
	m_pTrail = NULL;
	m_bClientPellet = false;
#endif
}

void CASW_Bouncing_Pellet::Precache( void )
{
	BaseClass::Precache( );

	PrecacheModel( PELLET_MODEL );

	PrecacheModel( "swarm/sprites/whiteglow1.vmt" );
	PrecacheModel( "swarm/sprites/greylaser1.vmt" );
}

void CASW_Bouncing_Pellet::Spawn( void )
{
	Precache();

	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM );
	SetSolid( SOLID_BBOX );
	//m_flGravity = 1.0;
	SetFriction( 0.75 );
	SetModel( PELLET_MODEL );	

	SetSize( -Vector(1,1,1), Vector(1,1,1) );
	SetSolid( SOLID_BBOX );
	SetGravity( 0.05f );
	SetCollisionGroup( ASW_COLLISION_GROUP_SHOTGUN_PELLET );

	SetTouch( &CASW_Bouncing_Pellet::PelletTouch );	

	SetThink( &CASW_Bouncing_Pellet::SUB_Remove );
	SetNextThink( gpGlobals->curtime + 1.0f );

	m_flDamage		= 10;
	m_takedamage	= DAMAGE_NO;
}

void CASW_Bouncing_Pellet::UpdateOnRemove( void )
{
	BaseClass::UpdateOnRemove();
#ifdef CLIENT_DLL
	ReleaseBeamTrail();
#endif
}

CASW_Bouncing_Pellet *CASW_Bouncing_Pellet::CreatePellet( const Vector &vecOrigin, const Vector &vecForward, CBaseEntity *pMarine, float flDamage )
{
	CASW_Bouncing_Pellet *pProjectile = dynamic_cast<CASW_Bouncing_Pellet*>(CreateEntityByName("asw_bouncing_pellet"));
		//(CASW_Bouncing_Pellet*)CREATE_PREDICTED_ENTITY( "asw_shotgun_pellet_predicted" );
#ifdef CLIENT_DLL
	if (!pProjectile->InitializeAsClientEntity( PELLET_MODEL, false ))
	{
		Msg("CASW_Bouncing_Pellet Error, couldn't InitializeAsClientEntity\n");
		pProjectile->Release();
		return NULL;
	}
	if (pProjectile)
		pProjectile->m_bClientPellet = true;
#endif

	if ( pProjectile )
	{
		UTIL_SetOrigin( pProjectile, vecOrigin );
		pProjectile->SetOwnerEntity( pMarine );
		pProjectile->Spawn();
		pProjectile->SetAbsVelocity( vecForward );
		pProjectile->m_flDamage = flDamage;

		QAngle angles;
		VectorAngles( vecForward, angles );
		pProjectile->SetLocalAngles( angles );
	}

	return pProjectile;
}

#ifdef CLIENT_DLL

void CASW_Bouncing_Pellet::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );	

	if ( updateType == DATA_UPDATE_CREATED)
	{
		CreateBeamTrail();
		SetNextClientThink(CLIENT_THINK_ALWAYS);
	}
}

ConVar asw_pellet_trail_width("asw_pellet_trail_width", "1.5f", FCVAR_CHEAT);
ConVar asw_pellet_trail_fade("asw_pellet_trail_fade", "0.0f", FCVAR_CHEAT);
ConVar asw_pellet_trail_life("asw_pellet_trail_life", "1.0f", FCVAR_CHEAT);
ConVar asw_pellet_trail_r("asw_pellet_trail_r", "255", FCVAR_CHEAT);
ConVar asw_pellet_trail_g("asw_pellet_trail_g", "255", FCVAR_CHEAT);
ConVar asw_pellet_trail_b("asw_pellet_trail_b", "255", FCVAR_CHEAT);
ConVar asw_pellet_trail_a("asw_pellet_trail_a", "40", FCVAR_CHEAT);
ConVar asw_pellet_trail_material("asw_pellet_trail_material", "sprites/laserbeam.vmt", FCVAR_CHEAT);

void CASW_Bouncing_Pellet::CreateBeamTrail()
{
	BeamInfo_t beamInfo;
	beamInfo.m_pStartEnt = this;
    beamInfo.m_nStartAttachment = 0;
    beamInfo.m_nModelIndex = modelinfo->GetModelIndex( asw_pellet_trail_material.GetString() );
    beamInfo.m_nHaloIndex = 0;
    beamInfo.m_flHaloScale = 0;
    beamInfo.m_flLife = asw_pellet_trail_life.GetFloat();
    beamInfo.m_flWidth = asw_pellet_trail_width.GetFloat();
    beamInfo.m_flEndWidth = asw_pellet_trail_width.GetFloat();
    beamInfo.m_flFadeLength = asw_pellet_trail_fade.GetFloat();
    beamInfo.m_flBrightness = asw_pellet_trail_a.GetFloat();
    beamInfo.m_flRed = asw_pellet_trail_r.GetFloat();
    beamInfo.m_flGreen = asw_pellet_trail_g.GetFloat();
    beamInfo.m_flBlue = asw_pellet_trail_b.GetFloat();
	if (!IsClientCreated())
	{
		beamInfo.m_flBlue = 0;
	}
    beamInfo.m_flAmplitude = asw_pellet_trail_life.GetFloat();

	m_pTrail = beams->CreateBeamFollow( beamInfo );
}

void CASW_Bouncing_Pellet::ReleaseBeamTrail()
{
	if (m_pTrail)
	{
		m_pTrail->flags = 0;
		m_pTrail->die = gpGlobals->curtime - 1;
		m_pTrail = NULL;
	}
}

void CASW_Bouncing_Pellet::ClientThink()
{
	if (!m_bClientPellet)
		return;

	// move us forward
	Vector vecForward;
	AngleVectors(GetAbsAngles(), &vecForward);
	Vector vecEndPosition = GetAbsOrigin() + vecForward * 2500 * gpGlobals->frametime;	// todo: FIX HARDCODED speed
	trace_t trace;
	CTraceFilterWorldOnly traceFilter;
	UTIL_TraceHull( GetAbsOrigin(), vecEndPosition, -Vector(1,1,1), Vector(1,1,1),
		              MASK_SOLID_BRUSHONLY, &traceFilter, &trace ); 

	if ( trace.fraction < 1.0f )
	{
		PelletTouch(trace.m_pEnt);
	}
	else
	{
		debugoverlay->AddLineOverlay(GetAbsOrigin(), vecEndPosition,
					0, 0, 255, true, 1.0f);
		SetAbsOrigin(vecEndPosition);
	}
	SetNextClientThink(CLIENT_THINK_ALWAYS);
}

#endif

#ifdef GAME_DLL
void CASW_Bouncing_Pellet::PelletHurt( CBaseEntity *pOther, trace_t &tr )
{
	if (pOther == m_hLastHit.Get())		// don't damage the same alien twice
		return;

	ClearMultiDamage();	

	CTakeDamageInfo	dmgInfo( this, GetOwnerEntity(), m_flDamage, DMG_BUCKSHOT );
	Vector	vecNormalizedVel = GetAbsVelocity();
	VectorNormalize( vecNormalizedVel );
	CalculateMeleeDamageForce( &dmgInfo, vecNormalizedVel, tr.endpos, 0.7f );
	dmgInfo.SetDamagePosition( tr.endpos );
	pOther->DispatchTraceAttack( dmgInfo, vecNormalizedVel, &tr );

	ApplyMultiDamage();
}
#endif

void CASW_Bouncing_Pellet::PelletTouch( CBaseEntity *pOther )
{
	if (!pOther)
		return;
	
	// make sure we don't die on things we shouldn't
	if (!ASWGameRules() || !ASWGameRules()->ShouldCollide(GetCollisionGroup(), pOther->GetCollisionGroup()))
		return;

	if ( !pOther->IsSolid() || pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS) )
		return;

	trace_t	tr;
	tr = BaseClass::GetTouchTrace();
	
	if ( pOther->m_takedamage != DAMAGE_NO )
	{
#ifdef GAME_DLL
		PelletHurt(pOther, tr);
#endif
	}

	// keep going through glass
	if ( pOther->GetCollisionGroup() == COLLISION_GROUP_BREAKABLE_GLASS )
		return;

	// keep going through aliens
	if ( pOther->IsNPC() )
		return;

	// todo: impact effects?

	if (m_iBounces > 0)
	{
		// bounce
		Vector vecNewDir = GetAbsVelocity();
		float speed = vecNewDir.NormalizeInPlace();
		float proj = (vecNewDir).Dot( tr.plane.normal );
		VectorMA( vecNewDir, -proj*2, tr.plane.normal, vecNewDir );

		vecNewDir *= speed;
		SetAbsVelocity(Vector(vecNewDir.x, vecNewDir.y, GetAbsVelocity().z));	// don't alter z

		m_iBounces--;
	}
	
	// hit something with no bounces left, time to die
	SetAbsVelocity( Vector( 0, 0, 0 ) );
	SetTouch( NULL );
	SetThink( &CASW_Bouncing_Pellet::SUB_Remove );
	SetNextThink( gpGlobals->curtime );	
}