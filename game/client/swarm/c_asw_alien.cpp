#include "cbase.h"
#include "c_asw_alien.h"
#include "eventlist.h"
#include "decals.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "c_asw_generic_emitter_entity.h"
#include "c_asw_marine_resource.h"
#include "c_asw_marine.h"
#include "c_asw_game_resource.h"
#include "asw_shareddefs.h"
#include "tier0/vprof.h"
#include "c_asw_fx.h"
#include "datacache/imdlcache.h"
#include "baseparticleentity.h"
#include "c_asw_clientragdoll.h"
#include "asw_util_shared.h"
#include "functionproxy.h"
#include "imaterialproxydict.h"
#include "proxyentity.h"
#include "materialsystem/IMaterialVar.h"
#include "materialsystem/itexture.h"
//#include "c_asw_physics_prop_statue.h"
#include "c_asw_mesh_emitter_entity.h"
#include "c_asw_egg.h"
#include "props_shared.h"
#include "c_asw_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ASW_GIB_ASAP 0.175f

ConVar asw_alien_object_motion_blur_scale( "asw_alien_object_motion_blur_scale", "0.2" );
ConVar asw_drone_gib_time_min("asw_drone_gib_time_min", "0.2", 0, "Minimum time a Swarm Drone ragdoll will stay around before gibbing");
ConVar asw_drone_gib_time_max("asw_drone_gib_time_max", "0.2", 0, "Maximum time a Swarm Drone ragdoll will stay around before gibbing");
ConVar asw_drone_fade_time_min("asw_drone_fade_time_min", "2.0", 0, "Minimum time a Swarm Drone ragdoll will stay around before fading");
ConVar asw_drone_fade_time_max("asw_drone_fade_time_max", "4.0", 0, "Maximum time a Swarm Drone ragdoll will stay around before fading");
ConVar asw_directional_shadows("asw_directional_shadows", "1", 0, "Whether aliens should have flashlight directional shadows");
ConVar asw_alien_shadows("asw_alien_shadows", "0", 0, "If set to one, aliens will always have shadows (WARNING: Big fps cost when lots of aliens are active)");
ConVar asw_alien_footstep_interval( "asw_alien_footstep_interval", "0.25", 0, "Minimum interval between alien footstep sounds. Used to keep them from piling up and preventing others from playing." );
ConVar asw_breakable_aliens( "asw_breakable_aliens", "1", 0, "If set, aliens can break into ragdoll gibs" );
extern ConVar asw_override_footstep_volume;
extern ConVar asw_alien_debug_death_style;

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Alien, DT_ASW_Alien )

BEGIN_NETWORK_TABLE( CASW_Alien, DT_ASW_Alien )
	RecvPropVectorXY( RECVINFO_NAME( m_vecNetworkOrigin, m_vecOrigin ), 0, C_BaseEntity::RecvProxy_CellOriginXY ),
	RecvPropFloat( RECVINFO_NAME( m_vecNetworkOrigin[2], m_vecOrigin[2] ), 0, C_BaseEntity::RecvProxy_CellOriginZ ),

	RecvPropFloat( RECVINFO_NAME( m_angNetworkAngles[0], m_angRotation[0] ) ),
	RecvPropFloat( RECVINFO_NAME( m_angNetworkAngles[1], m_angRotation[1] ) ),
	RecvPropFloat( RECVINFO_NAME( m_angNetworkAngles[2], m_angRotation[2] ) ),

	RecvPropBool( RECVINFO( m_bElectroStunned ) ),// not using ElectroStunned
	//RecvPropBool( RECVINFO( m_bElectroShockSmall ) ),
	//RecvPropBool( RECVINFO( m_bElectroShockBig ) ),
	RecvPropBool( RECVINFO( m_bOnFire ) ),
	RecvPropInt( RECVINFO( m_nDeathStyle ), SPROP_UNSIGNED ),
	RecvPropInt			( RECVINFO( m_iHealth) ),
END_RECV_TABLE()

PRECACHE_REGISTER_BEGIN( GLOBAL, ASW_Alien )
PRECACHE( MATERIAL, "effects/TiledFire/fire_tiled_precache" )
PRECACHE( MATERIAL, "effects/model_layer_shock_1_precache" )
PRECACHE( MATERIAL, "effects/model_layer_ice_1_precache" )
PRECACHE( PARTICLE_SYSTEM, "damage_numbers" )
PRECACHE_REGISTER_END()

IMPLEMENT_AUTO_LIST( IClientAimTargetsAutoList );

float C_ASW_Alien::sm_flLastFootstepTime = 0.0f;

C_ASW_Alien::C_ASW_Alien() : 
m_GlowObject( this ),
m_MotionBlurObject( this, 0.0f )
{
	m_bStepSideLeft = false;
	m_nLastSetModel = 0;
	m_fNextElectroStunEffect = 0;
	m_fLastCustomContribution = 0;
	m_vecLastCustomDir = vec3_origin;
	m_iLastCustomFrame = -1;
	m_bClientOnFire = false;
	m_vecLastRenderedPos = vec3_origin;
	m_pBurningEffect = NULL;

	m_GlowObject.SetColor( Vector( 0.3f, 0.6f, 0.1f ) );
	m_GlowObject.SetAlpha( 0.55f );
	m_GlowObject.SetRenderFlags( false, false );
	m_GlowObject.SetFullBloomRender( true );
}


C_ASW_Alien::~C_ASW_Alien()
{
	m_bOnFire = false;
	UpdateFireEmitters();
}


void C_ASW_Alien::FireEvent( const Vector& origin, const QAngle& angles, int event, const char *options )
{
	if ( event == AE_ASW_FOOTSTEP || event == AE_MARINE_FOOTSTEP )
	{
		Vector vel;
		EstimateAbsVelocity( vel );
		surfacedata_t *pSurface = GetGroundSurface();
		if (pSurface)
			MarineStepSound( pSurface, GetAbsOrigin(), vel );
	}
	else if (event == AE_REMOVE_CLIENT_AIM)
	{
		IASW_Client_Aim_Target::Remove( this );
	}
	else if ( event == AE_RAGDOLL )
	{

	}
	BaseClass::FireEvent(origin, angles, event, options);
}

void C_ASW_Alien::MarineStepSound( surfacedata_t *psurface, const Vector &vecOrigin, const Vector &vecVelocity )
{
	int	fWalking;
	float fvol;
	Vector knee;
	Vector feet;
	float height;
	float speed;
	float velrun;
	float velwalk;
	float flduck;
	int	fLadder;

	if ( GetFlags() & (FL_FROZEN|FL_ATCONTROLS))
		return;

	if ( GetMoveType() == MOVETYPE_NOCLIP || GetMoveType() == MOVETYPE_OBSERVER )
		return;

	if ( gpGlobals->curtime - sm_flLastFootstepTime < asw_alien_footstep_interval.GetFloat() )
		return;

	speed = VectorLength( vecVelocity );
	float groundspeed = Vector2DLength( vecVelocity.AsVector2D() );

	// determine if we are on a ladder
	fLadder = ( GetMoveType() == MOVETYPE_LADDER );

	// UNDONE: need defined numbers for run, walk, crouch, crouch run velocities!!!!	
	if ( ( GetFlags() & FL_DUCKING) || fLadder )
	{
		velwalk = 60;		// These constants should be based on cl_movespeedkey * cl_forwardspeed somehow
		velrun = 80;		
		flduck = 100;
	}
	else
	{
		velwalk = 90;
		velrun = 220;
		flduck = 0;
	}

	bool onground = true; //( GetFlags() & FL_ONGROUND );
	bool movingalongground = ( groundspeed > 0.0f );
	bool moving_fast_enough =  ( speed >= velwalk );

	// To hear step sounds you must be either on a ladder or moving along the ground AND
	// You must be moving fast enough
	//Msg("og=%d ma=%d mf=%d\n", onground, movingalongground, moving_fast_enough);
	if ( !moving_fast_enough || !(fLadder || ( onground && movingalongground )) )
			return;

//	MoveHelper()->PlayerSetAnimation( PLAYER_WALK );

	fWalking = speed < velrun;		

	VectorCopy( vecOrigin, knee );
	VectorCopy( vecOrigin, feet );

	height = 72.0f; // bad

	knee[2] = vecOrigin[2] + 0.2 * height;

	// find out what we're stepping in or on...
	if ( fLadder )
	{
		psurface = physprops->GetSurfaceData( physprops->GetSurfaceIndex( "ladder" ) );
		fvol = 0.5;
	}
	else if ( enginetrace->GetPointContents( knee ) & MASK_WATER )
	{
		static int iSkipStep = 0;

		if ( iSkipStep == 0 )
		{
			iSkipStep++;
			return;
		}

		if ( iSkipStep++ == 3 )
		{
			iSkipStep = 0;
		}
		psurface = physprops->GetSurfaceData( physprops->GetSurfaceIndex( "wade" ) );
		fvol = 0.65;
	}
	else if ( enginetrace->GetPointContents( feet ) & MASK_WATER )
	{
		psurface = physprops->GetSurfaceData( physprops->GetSurfaceIndex( "water" ) );
		fvol = fWalking ? 0.2 : 0.5;		
	}
	else
	{
		if ( !psurface )
			return;

		switch ( psurface->game.material )
		{
		default:
		case CHAR_TEX_CONCRETE:						
			fvol = fWalking ? 0.2 : 0.5;
			break;

		case CHAR_TEX_METAL:	
			fvol = fWalking ? 0.2 : 0.5;
			break;

		case CHAR_TEX_DIRT:
			fvol = fWalking ? 0.25 : 0.55;
			break;

		case CHAR_TEX_VENT:	
			fvol = fWalking ? 0.4 : 0.7;
			break;

		case CHAR_TEX_GRATE:
			fvol = fWalking ? 0.2 : 0.5;
			break;

		case CHAR_TEX_TILE:	
			fvol = fWalking ? 0.2 : 0.5;
			break;

		case CHAR_TEX_SLOSH:
			fvol = fWalking ? 0.2 : 0.5;
			break;
		}
	}

	// play the sound
	// 65% volume if ducking
	if ( GetFlags() & FL_DUCKING )
	{
		fvol *= 0.65;
	}

	if ( asw_override_footstep_volume.GetFloat() > 0 )
	{
		fvol = asw_override_footstep_volume.GetFloat();
	}

	PlayStepSound( feet, psurface, fvol, false );
}


surfacedata_t* C_ASW_Alien::GetGroundSurface()
{
	//
	// Find the name of the material that lies beneath the player.
	//
	Vector start, end;
	VectorCopy( GetAbsOrigin(), start );
	VectorCopy( start, end );

	// Straight down
	end.z -= 38; // was 64

	// Fill in default values, just in case.
	
	Ray_t ray;
	ray.Init( start, end, GetCollideable()->OBBMins(), GetCollideable()->OBBMaxs() );

	trace_t	trace;
	UTIL_TraceRay( ray, MASK_NPCSOLID_BRUSHONLY, this, COLLISION_GROUP_NPC, &trace );

	if ( trace.fraction == 1.0f )
		return NULL;	// no ground
	
	return physprops->GetSurfaceData( trace.surface.surfaceProps );
}

void C_ASW_Alien::PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force )
{
	if ( !psurface )
		return;

	unsigned short stepSoundName = m_bStepSideLeft ? psurface->sounds.runStepLeft : psurface->sounds.runStepRight;
	m_bStepSideLeft = !m_bStepSideLeft;

	if ( !stepSoundName )
		return;

	const char *pSoundName = physprops->GetString( stepSoundName );
	CSoundParameters params;
	if ( !CBaseEntity::GetParametersForSound( pSoundName, params, NULL ) )
		return;

	// do the surface dependent sound
	CLocalPlayerFilter filter;

	EmitSound_t ep;
	ep.m_nChannel = CHAN_BODY;
	ep.m_pSoundName = params.soundname;
	ep.m_flVolume = ( asw_override_footstep_volume.GetBool() ) ? fvol : params.volume;
	ep.m_SoundLevel = params.soundlevel;
	ep.m_nFlags = 0;
	ep.m_nPitch = params.pitch;
	ep.m_pOrigin = &vecOrigin;

	EmitSound( filter, entindex(), ep );	

	DoAlienFootstep(vecOrigin, fvol);
}

// plays alien type specific footstep sound
void C_ASW_Alien::DoAlienFootstep(Vector &vecOrigin, float fvol)
{
	CSoundParameters params;
	if ( !CBaseEntity::GetParametersForSound( "ASW_Drone.FootstepSoft", params, NULL ) )
		return;

	CLocalPlayerFilter filter;

	// do the alienfleshy foot sound
	EmitSound_t ep2;
	ep2.m_nChannel = CHAN_AUTO;
	ep2.m_pSoundName = params.soundname;
	ep2.m_flVolume = fvol;
	ep2.m_SoundLevel = params.soundlevel;
	ep2.m_nFlags = 0;
	ep2.m_nPitch = params.pitch;
	ep2.m_pOrigin = &vecOrigin;

	EmitSound( filter, entindex(), ep2 );

	sm_flLastFootstepTime = gpGlobals->curtime;
}

C_BaseAnimating * C_ASW_Alien::BecomeRagdollOnClient( void )
{
	// if we have a custom death force get it here, before the ragdoll is created
	// the shield bug uses this
	if ( HasCustomDeathForce() )
	{
		m_vecForce = GetCustomDeathForce();
	}
	//Msg("[C] C_ASW_Alien::BecomeRagdollOnClient on fire? %d / %d", ( GetFlags() & FL_ONFIRE ), m_bOnFire.Get());
	C_BaseAnimating* pEnt = BaseClass::BecomeRagdollOnClient();
	C_ASW_ClientRagdoll* pRagdoll = dynamic_cast<C_ASW_ClientRagdoll*>(pEnt);
	if (pRagdoll)
	{
		if ( asw_alien_debug_death_style.GetBool() )
		{
			Msg( "'%s' C_ASW_Alien::BecomeRagdollOnClient: m_nDeathStyle = %d\n", GetClassname(), m_nDeathStyle );
		}

		C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();

		pRagdoll->m_nDeathStyle = m_nDeathStyle;
		pRagdoll->AddEffects(EF_NOSHADOW);
		// if we broke don't draw the ragdoll
		if ( m_nDeathStyle == kDIE_BREAKABLE )
		{  
			if ( asw_breakable_aliens.GetBool() )
			{
				pRagdoll->AddEffects(EF_NODRAW);
			}
			pRagdoll->m_fASWGibTime = gpGlobals->curtime + ASW_GIB_ASAP; //gpGlobals->curtime + random->RandomFloat( 0.2f, 0.4f );
		}
		// make this ragdoll gib RIGHT MEOW
		else if ( m_nDeathStyle == kDIE_INSTAGIB )
		{
			// force instant gib
			// this happens when an alien takes a large amount of damage
			pRagdoll->m_fASWGibTime = gpGlobals->curtime + ASW_GIB_ASAP;
		}
		else if ( m_bOnFire.Get() )
		{
			pRagdoll->m_fASWGibTime = gpGlobals->curtime + random->RandomFloat( 0.3f, 0.7f );
		}
		else if ( IsHurler() )
		{
			pRagdoll->m_fASWGibTime = gpGlobals->curtime + random->RandomFloat( 4, 9 );
			pRagdoll->pszGibParticleEffect = GetRagdollGibParticleEffectName();
		}
		else if ( IsMeleeThrown() )
		{
			pRagdoll->m_fASWGibTime = gpGlobals->curtime + random->RandomFloat( 4, 9 );
			pRagdoll->pszGibParticleEffect = GetRagdollGibParticleEffectName();
		}
		else if ( m_nDeathStyle == kDIE_RAGDOLLFADE )
		{
			pRagdoll->m_fASWGibTime = gpGlobals->curtime + random->RandomFloat(asw_drone_fade_time_min.GetFloat(), asw_drone_fade_time_max.GetFloat());
			pRagdoll->pszGibParticleEffect = GetRagdollGibParticleEffectName();
		}
		else
		{
			pRagdoll->m_fASWGibTime = gpGlobals->curtime + random->RandomFloat(asw_drone_gib_time_min.GetFloat(), asw_drone_gib_time_max.GetFloat());
			pRagdoll->pszGibParticleEffect = GetRagdollGibParticleEffectName();
		}

		if ( m_bOnFire.Get() )
		{
			pRagdoll->AddFlag( FL_ONFIRE );

			CNewParticleEffect	*pBurningEffect = pRagdoll->ParticleProp()->Create( "ent_on_fire", PATTACH_ABSORIGIN_FOLLOW );
			if (pBurningEffect)
			{
				Vector vecOffest1 = (pRagdoll->WorldSpaceCenter() - pRagdoll->GetAbsOrigin()) + Vector( 0, 0, 16 );
				pPlayer->ParticleProp()->AddControlPoint( pBurningEffect, 1, pRagdoll, PATTACH_ABSORIGIN_FOLLOW, NULL, vecOffest1 );
			}			
		}
		else
		{
			if ( m_bElectroStunned )
			{
				pRagdoll->m_bElectroShock = true;
			}

			if ( pPlayer )
			{
				// if we're going to ragdoll, create a big blood spurt now so players get feedback about killing this alien
				QAngle	vecAngles;
				if ( m_vecForce == vec3_origin )
				{
					m_vecForce = Vector( RandomFloat( 1, 100 ), RandomFloat( 1, 100 ), 100.0f );
				}
				VectorAngles( m_vecForce, vecAngles );
				Vector vecForward, vecRight, vecUp;
				AngleVectors( vecAngles, &vecForward, &vecRight, &vecUp );

				const char *pchEffectName = NULL;

				switch ( m_nDeathStyle )
				{
				case kDIE_TUMBLEGIB:
				case kDIE_RAGDOLLFADE:
					pchEffectName = GetSmallDeathParticleEffectName();
					break;

				case kDIE_INSTAGIB:
				case kDIE_BREAKABLE:
					pchEffectName = GetBigDeathParticleEffectName();
					break;

				default:
					pchEffectName = GetDeathParticleEffectName();
					break;
				}

				CUtlReference< CNewParticleEffect > pEffect;
				pEffect = pPlayer->ParticleProp()->Create( pchEffectName, PATTACH_ABSORIGIN_FOLLOW );

				if ( pEffect )
				{
					pPlayer->ParticleProp()->AddControlPoint( pEffect, 1, pRagdoll, PATTACH_CUSTOMORIGIN );
					pEffect->SetControlPoint( 1, WorldSpaceCenter() );//origin - pMarine->GetAbsOrigin()
					pEffect->SetControlPointOrientation( 1, vecForward, vecRight, vecUp );
					pEffect->SetControlPointEntity( 0, pRagdoll );
				}
				else
				{
					Warning( "Could not create effect for alien death: %s", pchEffectName );
				}
			}

			if ( IsHurler() )
			{
				ASWHurlRagdollAtCamera( pRagdoll );
			}
			else if ( IsMeleeThrown() )
			{
				ASWMeleeThrowRagdoll( pRagdoll );
			}
		}
	}
	return pRagdoll;
}

C_ClientRagdoll *C_ASW_Alien::CreateClientRagdoll( bool bRestoring )
{
	return new C_ASW_ClientRagdoll( bRestoring );
}

// shadow direction test
bool C_ASW_Alien::GetShadowCastDirection( Vector *pDirection, ShadowType_t shadowType ) const			
{ 	
	VPROF_BUDGET( "C_ASW_Alien::GetShadowCastDistance", VPROF_BUDGETGROUP_ASW_CLIENT );

	if (!asw_directional_shadows.GetBool())
		return false;
	
	if (m_fLastCustomContribution > 0)
	{
		Vector vecDir = m_vecLastCustomDir;
		vecDir += (*pDirection) * (1.0f - m_fLastCustomContribution);
		vecDir.NormalizeInPlace();
		pDirection->x = vecDir.x;
		pDirection->y = vecDir.y;
		pDirection->z = vecDir.z;
		return true;
	}
	return false;	
}

ShadowType_t C_ASW_Alien::ShadowCastType()
{	
	if (asw_alien_shadows.GetBool())
		return BaseClass::ShadowCastType();
	float fContribution = 0;
	Vector vecDir = vec3_origin;
	GetShadowFromFlashlight(vecDir, fContribution);
	m_fLastCustomContribution = fContribution;
	m_vecLastCustomDir = vecDir;
	m_iLastCustomFrame = gpGlobals->framecount;
	if (m_fLastCustomContribution <= 0)
	{
		return SHADOWS_NONE;
	}
	return BaseClass::ShadowCastType();
}

void C_ASW_Alien::GetShadowFromFlashlight(Vector &vecDir, float &fContribution) const
{
	if (gpGlobals->framecount == m_iLastCustomFrame)
	{
		return;
	}
	
	if ( ASWGameResource() )
	{
		// go through all marines
		int iMaxMarines = ASWGameResource()->GetMaxMarineResources();
		for (int i=0;i<iMaxMarines;i++)
		{
			C_ASW_Marine_Resource *pMR = ASWGameResource()->GetMarineResource(i);
			C_ASW_Marine *pMarine = pMR ? pMR->GetMarineEntity() : NULL;
			if (pMarine && pMarine->m_pFlashlight)	// if this is a marine with a flashlight
			{
				Vector diff = WorldSpaceCenter() - pMarine->EyePosition();
				if (diff.Length() < 700.0f)
				{
					diff.NormalizeInPlace();
					Vector vecMarineFacing(0,0,0);
					AngleVectors(pMarine->GetAbsAngles(), &vecMarineFacing);
					float dot = vecMarineFacing.Dot(diff);
					if (dot > 0.2)	// if the flashlight is facing us
					{
						vecDir += dot * diff;
						fContribution += (dot - 0.2f) * 1.25f;
					}
				}
			}
		}
	}
}

void C_ASW_Alien::PostDataUpdate( DataUpdateType_t updateType )
{	
	BaseClass::PostDataUpdate(updateType);
	// If this entity was new, then latch in various values no matter what.
	if ( updateType == DATA_UPDATE_CREATED )
	{
		// We want to think every frame.
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
}

Vector C_ASW_Alien::GetLocalAutoTargetRadiusPos()
{
	// drone overrides this
	return m_vecLastRenderedPos;
}

void C_ASW_Alien::ClientThink()
{
	BaseClass::ClientThink();

	// asw temp fix demo playback 
	//ASWUpdateClientSideAnimation();

	m_vecLastRenderedPos = WorldSpaceCenter();
	m_vecAutoTargetRadiusPos = GetLocalAutoTargetRadiusPos();

	if ( GetHealth() > 0 && m_bElectroStunned && m_fNextElectroStunEffect <= gpGlobals->curtime)
	{
		// apply electro stun effect
		HACK_GETLOCALPLAYER_GUARD( "C_ASW_Alien::ClientThink FX_ElectroStun" );
		FX_ElectroStun(this);
		m_fNextElectroStunEffect = gpGlobals->curtime + RandomFloat( 0.3, 1.0 );
		//Msg( "%f - ElectroStunEffect\n", gpGlobals->curtime );
	}

	UpdateFireEmitters();

	C_ASW_Player* pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( pPlayer && pPlayer->IsSniperScopeActive() )
	{
		m_GlowObject.SetRenderFlags( true, true );
	}
	else
	{
		m_GlowObject.SetRenderFlags( false, false );
	}
}

// asw - test always advancing the frames
void C_ASW_Alien::ASWUpdateClientSideAnimation()
{
	if ( GetSequence() != -1 )
	{
		// latch old values
		//OnLatchInterpolatedVariables( LATCH_ANIMATION_VAR );
		// move frame forward
		//FrameAdvance( 0.0f ); // 0 means to use the time we last advanced instead of a constant

		CStudioHdr *hdr = GetModelPtr();
		float cyclerate = hdr ? GetSequenceCycleRate( hdr, GetSequence() ) : 1.0f;
		float addcycle = gpGlobals->frametime * cyclerate * m_flPlaybackRate;
		float flNewCycle = GetCycle() + addcycle;
		m_flAnimTime = gpGlobals->curtime;

		if ( (flNewCycle < 0.0f) || (flNewCycle >= 1.0f) ) 
		{	
			if (flNewCycle >= 1.0f)	// asw
				ReachedEndOfSequence(); // asw
			if ( IsSequenceLooping( hdr, GetSequence() ) )
			{
				flNewCycle -= (int)(flNewCycle);
			}
			else
			{
				flNewCycle = (flNewCycle < 0.0f) ? 0.0f : 1.0f;
			}
		}

		SetCycle( flNewCycle );
	}
}

void C_ASW_Alien::UpdateFireEmitters()
{
	bool bOnFire = (m_bOnFire.Get() && !IsEffectActive(EF_NODRAW));
	if (bOnFire != m_bClientOnFire)
	{
		m_bClientOnFire = bOnFire;
		if (m_bClientOnFire)
		{
			if ( !m_pBurningEffect )
			{
				m_pBurningEffect = UTIL_ASW_CreateFireEffect( this );
			}
			EmitSound( "ASWFire.BurningFlesh" );
		}
		else
		{
			if ( m_pBurningEffect )
			{
				ParticleProp()->StopEmission( m_pBurningEffect );
				m_pBurningEffect = NULL;
			}
			StopSound("ASWFire.BurningFlesh");
			if ( C_BaseEntity::IsAbsQueriesValid() )
				EmitSound("ASWFire.StopBurning");
		}
	}
}

void C_ASW_Alien::UpdateOnRemove( void )
{
	BaseClass::UpdateOnRemove();
	m_bOnFire = false;
	UpdateFireEmitters();
}

// aliens require extra interpolation time due to think rate
ConVar cl_alien_extra_interp( "cl_alien_extra_interp", "0.1", FCVAR_NONE, "Extra interpolation for aliens." );

float C_ASW_Alien::GetInterpolationAmount( int flags )
{
	return BaseClass::GetInterpolationAmount( flags ) + cl_alien_extra_interp.GetFloat();
}