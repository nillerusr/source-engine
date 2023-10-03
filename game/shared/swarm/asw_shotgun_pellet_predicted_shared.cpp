#include "cbase.h"
#include "engine/IEngineSound.h"
#include "asw_shareddefs.h"
#include "basegrenade_shared.h"
#include "Sprite.h"
#include "asw_shotgun_pellet_predicted_shared.h"
#ifdef CLIENT_DLL
#include "c_asw_marine.h"
#include "particles_simple.h"
#include "c_asw_trail_beam.h"
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

int g_iEMPPulseEffectIndex = 0;

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Shotgun_Pellet_Predicted, DT_ASW_Shotgun_Pellet_Predicted );

BEGIN_NETWORK_TABLE( CASW_Shotgun_Pellet_Predicted, DT_ASW_Shotgun_Pellet_Predicted )
#if !defined( CLIENT_DLL )
	SendPropEHandle( SENDINFO( m_hLiveSprite ) ),
#else
	RecvPropEHandle( RECVINFO( m_hLiveSprite ) ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CASW_Shotgun_Pellet_Predicted )
	DEFINE_PRED_FIELD( m_hLiveSprite, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( asw_shotgun_pellet_predicted, CASW_Shotgun_Pellet_Predicted );
PRECACHE_REGISTER(asw_shotgun_pellet_predicted);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CASW_Shotgun_Pellet_Predicted::CASW_Shotgun_Pellet_Predicted()
{
	m_pLastHit = NULL;
	m_pCommander = NULL;
	SetPredictionEligible( true );

#if defined( CLIENT_DLL )
	m_ParticleEvent.Init( 100 );
#else
	UseClientSideAnimation();
#endif
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_Shotgun_Pellet_Predicted::Precache( void )
{
	BaseClass::Precache( );

	PrecacheModel( PELLET_MODEL );

	PrecacheModel( "swarm/sprites/whiteglow1.vmt" );
	PrecacheModel( "swarm/sprites/greylaser1.vmt" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Shotgun_Pellet_Predicted::Spawn( void )
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

	SetTouch( &CASW_Shotgun_Pellet_Predicted::PelletTouch );	

	SetThink( &CASW_Shotgun_Pellet_Predicted::SUB_Remove );
	SetNextThink( gpGlobals->curtime + 1.0f );

	m_flDamage		= 10;
	m_takedamage	= DAMAGE_NO;

	// Create a white light
	CBasePlayer *player = ToBasePlayer( GetOwnerEntity() );
	if ( player )
	{
		m_hLiveSprite = SPRITE_CREATE_PREDICTABLE( "sprites/chargeball2.vmt", GetLocalOrigin() + Vector(0,0,1), false );
		if ( m_hLiveSprite )
		{
			m_hLiveSprite->SetOwnerEntity( player );
			m_hLiveSprite->SetPlayerSimulated( player );
			m_hLiveSprite->SetTransparency( kRenderGlow, 255, 255, 255, 128, kRenderFxNoDissipation );
			m_hLiveSprite->SetBrightness( 255 );
			m_hLiveSprite->SetScale( 0.15, 5.0f );
			m_hLiveSprite->SetAttachment( this, 0 );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Shotgun_Pellet_Predicted::UpdateOnRemove( void )
{
	// Remove our live sprite
	if ( m_hLiveSprite )
	{
		m_hLiveSprite->Remove( );
		m_hLiveSprite = NULL;
	}

	// Chain at end to mimic destructor unwind order
	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: Allow shield parry's
//-----------------------------------------------------------------------------
void CASW_Shotgun_Pellet_Predicted::BounceTouch( CBaseEntity *pOther )
{
	Assert( pOther );
	if ( !pOther->IsSolid() )
		return;

	BaseClass::BounceTouch( pOther );
}

//-----------------------------------------------------------------------------
// Purpose: Play a distinctive grenade bounce sound to warn nearby players
//-----------------------------------------------------------------------------
void CASW_Shotgun_Pellet_Predicted::BounceSound( void )
{
	CPASAttenuationFilter filter( this, "GrenadeEMP.Bounce" );
	filter.UsePredictionRules();
	EmitSound( filter, entindex(), "GrenadeEMP.Bounce" );
}

//-----------------------------------------------------------------------------
// Purpose: Return the amplitude for the screenshake
//-----------------------------------------------------------------------------
float CASW_Shotgun_Pellet_Predicted::GetShakeAmplitude( void )
{
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Create a missile
//-----------------------------------------------------------------------------
CASW_Shotgun_Pellet_Predicted *CASW_Shotgun_Pellet_Predicted::CreatePellet( const Vector &vecOrigin, const Vector &vecForward, CBasePlayer *pCommander, CBaseEntity *pMarine )
{
	CASW_Shotgun_Pellet_Predicted *pProjectile = (CASW_Shotgun_Pellet_Predicted*)CREATE_PREDICTED_ENTITY( "asw_shotgun_pellet_predicted" );
	if ( pProjectile )
	{
		UTIL_SetOrigin( pProjectile, vecOrigin );
		pProjectile->SetOwnerEntity( pMarine );
		pProjectile->Spawn();
		pProjectile->SetPlayerSimulated( pCommander );
		pProjectile->SetAbsVelocity( vecForward );
		pProjectile->SetCommander(pCommander);

		QAngle angles;
		VectorAngles( vecForward, angles );
		pProjectile->SetLocalAngles( angles );

#ifdef CLIENT_DLL
		// create a trail beam (it'll fade out and kill itself after a short period)
		C_ASW_Trail_Beam *pBeam = new C_ASW_Trail_Beam();
		if (pBeam)
		{
			if ( pBeam->InitializeAsClientEntity( NULL, false ) )
			{
				pBeam->InitBeam(pProjectile->GetAbsOrigin(), pProjectile);
			}
			else
			{
				UTIL_Remove( pBeam );
			}
		}
#endif

		//pProjectile->SetLocalAngularVelocity( SHARED_RANDOMANGLE( -500, 500 ) );
	}

	return pProjectile;
}

#if defined( CLIENT_DLL )
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Shotgun_Pellet_Predicted::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );	

	if ( updateType == DATA_UPDATE_CREATED && gpGlobals->maxClients <= 1)
	{
		// create a trail beam (it'll fade out and kill itself after a short period)
		C_ASW_Trail_Beam *pBeam = new C_ASW_Trail_Beam();
		if (pBeam)
		{
			if (pBeam->InitializeAsClientEntity( NULL, false ))
			{
				C_ASW_Marine *pMarine = dynamic_cast<C_ASW_Marine*>(GetOwnerEntity());
				if (pMarine)
					pBeam->InitBeam(pMarine->Weapon_ShootPosition(), this);
				else
					pBeam->InitBeam(GetAbsOrigin(), this);
			}
			else
			{
				UTIL_Remove( pBeam );
			}
		}
	}

	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: Trail smoke
//-----------------------------------------------------------------------------
void CASW_Shotgun_Pellet_Predicted::ClientThink( void )
{
return;

	CSmartPtr<CSimpleEmitter> pEmitter = CSimpleEmitter::Create( "CASW_Shotgun_Pellet_Predicted::Effect" );
	PMaterialHandle	hSphereMaterial = pEmitter->GetPMaterial( "sprites/chargeball" );

	// Add particles at the target.
	float flCur = gpGlobals->frametime;
	while ( m_ParticleEvent.NextEvent( flCur ) )
	{
		Vector vecOrigin = GetAbsOrigin() + RandomVector( -2,2 );
		pEmitter->SetSortOrigin( vecOrigin );

		SimpleParticle *pParticle = (SimpleParticle *) pEmitter->AddParticle( sizeof(SimpleParticle), hSphereMaterial, vecOrigin );
		if ( pParticle == NULL )
			return;

		pParticle->m_flLifetime	= 0.0f;
		pParticle->m_flDieTime	= random->RandomFloat( 0.1f, 0.3f );

		pParticle->m_uchStartSize	= random->RandomFloat(2,4);
		pParticle->m_uchEndSize		= pParticle->m_uchStartSize + 2;

		pParticle->m_vecVelocity = vec3_origin;
		pParticle->m_uchStartAlpha = 128;
		pParticle->m_uchEndAlpha = 0;
		pParticle->m_flRoll	= random->RandomFloat( 180, 360 );
		pParticle->m_flRollDelta = random->RandomFloat( -1, 1 );

		pParticle->m_uchColor[0] = 128;
		pParticle->m_uchColor[1] = 128;
		pParticle->m_uchColor[2] = 128;
	}
}

int CASW_Shotgun_Pellet_Predicted::DrawModel( int flags, const RenderableInstance_t &instance )
{
	int iret = BaseClass::DrawModel( flags, instance );
	return iret;
}

#endif


void CASW_Shotgun_Pellet_Predicted::PelletTouch( CBaseEntity *pOther )
{
	if (!pOther)
		return;

	if (pOther == m_pLastHit)		// don't damage the same alien twice
		return;

	if ( !pOther->IsSolid() || pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS) )
		return;

	// make sure we don't die on things we shouldn't
	if (!ASWGameRules() || !ASWGameRules()->ShouldCollide(GetCollisionGroup(), pOther->GetCollisionGroup()))
		return;

	if ( pOther->m_takedamage != DAMAGE_NO )
	{
		trace_t	tr, tr2;
		tr = BaseClass::GetTouchTrace();
		Vector	vecNormalizedVel = GetAbsVelocity();
		VectorNormalize( vecNormalizedVel );
#ifdef GAME_DLL
		ClearMultiDamage();		

		if( GetOwnerEntity() && GetOwnerEntity()->IsPlayer() && pOther->IsNPC() )
		{
			CTakeDamageInfo	dmgInfo( this, GetOwnerEntity(), m_flDamage, DMG_NEVERGIB );
			dmgInfo.AdjustPlayerDamageInflictedForSkillLevel();
			CalculateMeleeDamageForce( &dmgInfo, vecNormalizedVel, tr.endpos, 0.7f );
			dmgInfo.SetDamagePosition( tr.endpos );
			pOther->DispatchTraceAttack( dmgInfo, vecNormalizedVel, &tr );
		}
		else
		{
			CTakeDamageInfo	dmgInfo( this, GetOwnerEntity(), m_flDamage, DMG_BULLET | DMG_NEVERGIB );
			CalculateMeleeDamageForce( &dmgInfo, vecNormalizedVel, tr.endpos, 0.7f );
			dmgInfo.SetDamagePosition( tr.endpos );
			pOther->DispatchTraceAttack( dmgInfo, vecNormalizedVel, &tr );
		}

		ApplyMultiDamage();
#endif
		//Adrian: keep going through the glass.
		if ( pOther->GetCollisionGroup() == COLLISION_GROUP_BREAKABLE_GLASS )
			 return;

		// pellets should carry on through spawnable enemies?
		//IASW_Spawnable_NPC* pSpawnable = dynamic_cast<IASW_Spawnable_NPC*>(pOther);
		//if (pSpawnable && asw_shotgun_pellets_pass.GetBool())
		//{
			//m_pLastHit = pOther;
			//return;
		//}

		SetAbsVelocity( Vector( 0, 0, 0 ) );

		// play body "thwack" sound
		EmitSound( "Weapon_Crossbow.BoltHitBody" );

		Vector vForward;

		AngleVectors( GetAbsAngles(), &vForward );
		VectorNormalize ( vForward );

		UTIL_TraceLine( GetAbsOrigin(),	GetAbsOrigin() + vForward * 128, MASK_OPAQUE, pOther, COLLISION_GROUP_NONE, &tr2 );
#ifdef GAME_DLL
		if ( tr2.fraction != 1.0f )
		{
//			NDebugOverlay::Box( tr2.endpos, Vector( -16, -16, -16 ), Vector( 16, 16, 16 ), 0, 255, 0, 0, 10 );
//			NDebugOverlay::Box( GetAbsOrigin(), Vector( -16, -16, -16 ), Vector( 16, 16, 16 ), 0, 0, 255, 0, 10 );

			if ( tr2.m_pEnt == NULL || ( tr2.m_pEnt && tr2.m_pEnt->GetMoveType() == MOVETYPE_NONE ) )
			{
				CEffectData	data;

				data.m_vOrigin = tr2.endpos;
				data.m_vNormal = vForward;
				data.m_nEntIndex = tr2.fraction != 1.0f;
			
				//DispatchEffect( "BoltImpact", data );
			}
		}		
#endif
		SetTouch( NULL );
		SetThink( NULL );

		//KillEffects();
		//UTIL_Remove( this );
		//Release();
		SetThink( &CASW_Shotgun_Pellet_Predicted::SUB_Remove );
		SetNextThink( gpGlobals->curtime );
	}
	else
	{
		trace_t	tr;
		tr = BaseClass::GetTouchTrace();

		// See if we struck the world
		if ( pOther->GetMoveType() == MOVETYPE_NONE && !( tr.surface.flags & SURF_SKY ) )
		{
			//EmitSound( "Weapon_Crossbow.BoltHitWorld" );

			// if what we hit is static architecture, can stay around for a while.
			Vector vecDir = GetAbsVelocity();
			VectorNormalize( vecDir );
								
			SetMoveType( MOVETYPE_NONE );
		
			Vector vForward;

			AngleVectors( GetAbsAngles(), &vForward );
			VectorNormalize ( vForward );
#ifdef GAME_DLL
			CEffectData	data;

			data.m_vOrigin = tr.endpos;
			data.m_vNormal = vForward;
			data.m_nEntIndex = 0;
#endif
			//DispatchEffect( "BoltImpact", data );
			
			UTIL_ImpactTrace( &tr, DMG_BULLET );

			AddEffects( EF_NODRAW );
			SetTouch( NULL );
			//KillEffects();
			SetThink( &CASW_Shotgun_Pellet_Predicted::SUB_Remove );
			SetNextThink( gpGlobals->curtime + 2.0f );
			
			// Shoot some sparks
#ifdef GAME_DLL
			if ( UTIL_PointContents( GetAbsOrigin(), CONTENTS_WATER ) != CONTENTS_WATER)
			{
				g_pEffects->Sparks( GetAbsOrigin() );
			}
#endif
		}
		else
		{
			// Put a mark unless we've hit the sky
			if ( ( tr.surface.flags & SURF_SKY ) == false )
			{
				UTIL_ImpactTrace( &tr, DMG_BULLET );
			}
			//KillEffects();
			//UTIL_Remove( this );
			//Release();
			SetThink( &CASW_Shotgun_Pellet_Predicted::SUB_Remove );
			SetNextThink( gpGlobals->curtime );
		}
	}
}