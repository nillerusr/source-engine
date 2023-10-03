#include "cbase.h"
#include "c_asw_alien.h"
#include "c_asw_simple_alien.h"
#include "eventlist.h"
#include "decals.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "c_asw_generic_emitter_entity.h"
#include "c_asw_marine_resource.h"
#include "c_asw_marine.h"
#include "c_asw_game_resource.h"
#include "asw_shareddefs.h"
#include "tier0/vprof.h"
#include "datacache/imdlcache.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


extern ConVar asw_drone_gib_time_min;
extern ConVar asw_drone_gib_time_max;
extern ConVar asw_directional_shadows;
extern ConVar asw_alien_footstep_interval;

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Simple_Alien, DT_ASW_Simple_Alien, CASW_Simple_Alien)

END_RECV_TABLE()


C_ASW_Simple_Alien::C_ASW_Simple_Alien()
	: m_GlowObject( this )
{
	m_bStepSideLeft = false;
	m_nLastSetModel = 0;
	m_vecLastRenderedPos = vec3_origin;

	m_GlowObject.SetColor( Vector( 0.3f, 0.6f, 0.1f ) );
	m_GlowObject.SetAlpha( 0.55f );
	m_GlowObject.SetRenderFlags( false, false );
	m_GlowObject.SetFullBloomRender( true );
}


void C_ASW_Simple_Alien::FireEvent( const Vector& origin, const QAngle& angles, int event, const char *options )
{
	if (event == AE_ASW_FOOTSTEP || event == AE_MARINE_FOOTSTEP)
	{
		Vector vel;
		EstimateAbsVelocity( vel );
		surfacedata_t *pSurface = GetGroundSurface();
		if (pSurface)
			AlienStepSound( pSurface, GetAbsOrigin(), vel );
	}
	BaseClass::FireEvent(origin, angles, event, options);
}

void C_ASW_Simple_Alien::AlienStepSound( surfacedata_t *psurface, const Vector &vecOrigin, const Vector &vecVelocity )
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

	PlayStepSound( feet, psurface, fvol, false );
}


surfacedata_t* C_ASW_Simple_Alien::GetGroundSurface()
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

void C_ASW_Simple_Alien::PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force )
{
	if ( !psurface )
		return;

	unsigned short stepSoundName = m_bStepSideLeft ? psurface->sounds.runStepLeft : psurface->sounds.runStepRight;
	m_bStepSideLeft = !m_bStepSideLeft;

	if ( !stepSoundName )
		return;

	//IPhysicsSurfaceProps *physprops = MoveHelper( )->GetSurfaceProps();
	const char *pSoundName = physprops->GetString( stepSoundName );
	CSoundParameters params;
	if ( !CBaseEntity::GetParametersForSound( pSoundName, params, NULL ) )
		return;

	// do the surface dependent sound
	CLocalPlayerFilter filter;

	EmitSound_t ep;
	ep.m_nChannel = CHAN_BODY;
	ep.m_pSoundName = params.soundname;
	ep.m_flVolume = fvol;
	ep.m_SoundLevel = params.soundlevel;
	ep.m_nFlags = 0;
	ep.m_nPitch = params.pitch * 0.7f;	// drone plays 'deeper' footstep sounds
	ep.m_pOrigin = &vecOrigin;

	EmitSound( filter, entindex(), ep );	

	DoAlienFootstep(vecOrigin, fvol);
}

// plays alien type specific footstep sound
void C_ASW_Simple_Alien::DoAlienFootstep(Vector &vecOrigin, float fvol)
{
	CSoundParameters params;
	if ( !CBaseEntity::GetParametersForSound( "ASW_Drone.FootstepSoft", params, NULL ) )
		return;

	CLocalPlayerFilter filter;

	// do the alienfleshy foot sound
	EmitSound_t ep2;
	ep2.m_nChannel = CHAN_AUTO;
	ep2.m_pSoundName = params.soundname;
	ep2.m_flVolume = fvol * 0.2f;
	ep2.m_SoundLevel = params.soundlevel;
	ep2.m_nFlags = 0;
	ep2.m_nPitch = params.pitch;
	ep2.m_pOrigin = &vecOrigin;

	EmitSound( filter, entindex(), ep2 );

	sm_flLastFootstepTime = gpGlobals->curtime;
}

C_BaseAnimating * C_ASW_Simple_Alien::BecomeRagdollOnClient( void )
{
	C_BaseAnimating* pEnt = BaseClass::BecomeRagdollOnClient();
	C_ClientRagdoll* pRagdoll = dynamic_cast<C_ClientRagdoll*>(pEnt);
	if (pRagdoll)
	{
		// make this ragdoll gib after a short time
		// ASWTODO - get ragdolls exploding
		//pRagdoll->m_fASWGibTime = gpGlobals->curtime + random->RandomFloat(asw_drone_gib_time_min.GetFloat(), asw_drone_gib_time_max.GetFloat());

		// make it bleed
		//C_ASW_Emitter *pEmitter = dynamic_cast<C_ASW_Emitter*>(CreateEntityByName("client_asw_emitter"));
		int iNumBleeds = random->RandomInt(2,4);
		for (int i=0;i<iNumBleeds;i++)
		{
			C_ASW_Emitter *pEmitter = new C_ASW_Emitter;
			if (pEmitter)
			{
				if (pEmitter->InitializeAsClientEntity( NULL, false ))
				{
					// randomly pick a jet, a drip or a burst
					float f = random->RandomFloat();
					if (f < 0.33f)
						Q_snprintf(pEmitter->m_szTemplateName, sizeof(pEmitter->m_szTemplateName), "dronebloodjet");
					else if (f < 0.66f)
						Q_snprintf(pEmitter->m_szTemplateName, sizeof(pEmitter->m_szTemplateName), "dronebloodburst");
					else
						Q_snprintf(pEmitter->m_szTemplateName, sizeof(pEmitter->m_szTemplateName), "droneblooddroplets");
					pEmitter->m_fScale = 1.0f;
					pEmitter->m_bEmit = true;
					pEmitter->CreateEmitter();
					pEmitter->SetAbsOrigin(pRagdoll->WorldSpaceCenter());			
					pEmitter->SetAbsAngles(pRagdoll->GetAbsAngles());

					// randomly pick an attach point
					int iAttach = random->RandomInt(0, 7);
					switch (iAttach)
					{
					case 0: pEmitter->ClientAttach(pRagdoll, "bleed1"); break;
					case 1: pEmitter->ClientAttach(pRagdoll, "bleed2"); break;
					case 2: pEmitter->ClientAttach(pRagdoll, "bleed3"); break;
					case 3: pEmitter->ClientAttach(pRagdoll, "bleed4"); break;
					case 4: pEmitter->ClientAttach(pRagdoll, "bleed5"); break;
					case 5: pEmitter->ClientAttach(pRagdoll, "bleedjaw1"); break;
					case 6: pEmitter->ClientAttach(pRagdoll, "bleedleg1"); break;
					case 7: pEmitter->ClientAttach(pRagdoll, "bleedleg2"); break;
					}

					// ASWTODO - put this in when we have ragdolls exploding
					//pEmitter->SetDieTime(pRagdoll->m_fASWGibTime);	// stop emitting once the ragdoll gibs
					pEmitter->SetDieTime(3.0f);
				}
				else
				{
					UTIL_Remove( pEmitter );
				}
			}
		}
	}
	return pRagdoll;
}

// shadow direction test
bool C_ASW_Simple_Alien::GetShadowCastDirection( Vector *pDirection, ShadowType_t shadowType ) const			
{ 	
	VPROF_BUDGET( "C_ASW_Simple_Alien::GetShadowCastDistance", VPROF_BUDGETGROUP_ASW_CLIENT );

	if (!asw_directional_shadows.GetBool())
		return false;

	Vector vecCustomDir;
	float fCustomContribution = 0;
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
				diff.NormalizeInPlace();
				Vector vecMarineFacing(0,0,0);
				AngleVectors(pMarine->GetAbsAngles(), &vecMarineFacing);
				float dot = vecMarineFacing.Dot(diff);
				if (dot > 0)	// if the flashlight is facing us
				{
					vecCustomDir += dot * diff;
					fCustomContribution += dot;
				}
			}
		}
	}
	if (fCustomContribution > 0)
	{
		vecCustomDir += (*pDirection) * (1.0f - fCustomContribution);
		vecCustomDir.NormalizeInPlace();
		pDirection->x = vecCustomDir.x;
		pDirection->y = vecCustomDir.y;
		pDirection->z = vecCustomDir.z;
		return true;
	}
	return false;	
}

void C_ASW_Simple_Alien::PostDataUpdate( DataUpdateType_t updateType )
{	
	BaseClass::PostDataUpdate(updateType);

	// If this entity was new, then latch in various values no matter what.
	if ( updateType == DATA_UPDATE_CREATED )
	{
		// We want to think every frame.
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}

	// NOTE: Below is a stripped down version of PostDataUpdate with perf notes in.  Do we need this anymore?
	/*
	MDLCACHE_CRITICAL_SECTION();
	// Make sure that the correct model is referenced for this entity
	if (m_nLastSetModel != m_nModelIndex)
	{
	m_nLastSetModel = m_nModelIndex;
	SetModelByIndex( m_nModelIndex );
	}
	//cheap

	// as c_baseanimating and c_baseentity but streamlined, since we want lots of aliens

	// Cycle change? Then re-render
	if ( m_flOldCycle != GetCycle() || m_nOldSequence != GetSequence() )
	{
	InvalidatePhysicsRecursive( ANIMATION_CHANGED );
	}
	// cheap - 1.59%     931.981
	// with custom flcycle: cheap - 1.56%     915.995 

	// reset prev cycle if new sequence
	if (m_nNewSequenceParity != m_nPrevNewSequenceParity)
	{
	// It's important not to call Reset() on a static prop, because if we call
	// Reset(), then the entity will stay in the interpolated entities list
	// forever, wasting CPU.
	CStudioHdr *hdr = GetModelPtr();
	if ( hdr && !( hdr->flags() & STUDIOHDR_FLAGS_STATIC_PROP ) )
	{
	// cheap without this - 1.86%    1086.119 
	// asw temp anim comment
	m_iv_flCycle.Reset( gpGlobals->curtime );
	//m_flCycle = 0;	// asw added
	}
	}
	// with custom flyccle: cheap - 1040.942  1.78%

	// mostly exp - 18.09%   10657.891 

	// NOTE: This *has* to happen first. Otherwise, Origin + angles may be wrong 
	if ( m_nRenderFX == kRenderFxRagdoll && updateType == DATA_UPDATE_CREATED )
	{
	MoveToLastReceivedPosition( true );
	}
	else
	{
	MoveToLastReceivedPosition( false );
	}

	if ( m_nOldRenderMode != m_nRenderMode )
	{
	SetRenderMode( (RenderMode_t)m_nRenderMode, true );
	}
	// expensive - 19.67%   11575.138
	// with custom flycycle: cheap - 1279.913  2.19%
	bool animTimeChanged = ( m_flAnimTime != m_flOldAnimTime ) ? true : false;
	bool originChanged = ( m_vecOldOrigin != GetLocalOrigin() ) ? true : false;
	bool anglesChanged = ( m_vecOldAngRotation != GetLocalAngles() ) ? true : false;
	bool simTimeChanged = ( m_flSimulationTime != m_flOldSimulationTime ) ? true : false;

	// Detect simulation changes 
	bool simulationChanged = originChanged || anglesChanged || simTimeChanged;

	// with custom flcycle: cheap - 1229.052  2.10%

	//if ( !GetPredictable() && !IsClientCreated() )
	{
	if (animTimeChanged || simulationChanged)
	{
	float animchangetime = animTimeChanged ? GetLastChangeTime( LATCH_ANIMATION_VAR ) : 0;
	float simchangetime = simulationChanged ? GetLastChangeTime( LATCH_SIMULATION_VAR ) : 0;

	int c = m_VarMap.m_Entries.Count();
	for ( int i = 0; i < c; i++ )
	{
	VarMapEntry_t *e = &m_VarMap.m_Entries[ i ];
	IInterpolatedVar *watcher = e->watcher;

	int type = watcher->GetType();

	if ( type & EXCLUDE_AUTO_LATCH )
	continue;

	if (animTimeChanged && (type & LATCH_ANIMATION_VAR) )
	{
	if ( watcher->NoteChanged( gpGlobals->curtime, animchangetime ) )
	e->m_bNeedsToInterpolate = true;
	}
	if (simulationChanged && (type & LATCH_SIMULATION_VAR) )
	{
	if ( watcher->NoteChanged( gpGlobals->curtime, simchangetime ) )
	e->m_bNeedsToInterpolate = true;
	}
	}

	if ( index != 0 && GetModel() && IsVisible() )
	{
	AddToInterpolationList();
	//if ( m_InterpolationListEntry == 0xFFFF )
	//m_InterpolationListEntry = g_InterpolationList.AddToTail( this );
	}
	}		
	}
	//expensive - 26.21%   15504.664
	// with custom flcycle: expensive - 10021.575 17.07%
	// Deal with hierarchy. Have to do it here (instead of in a proxy)
	// because this is the only point at which all entities are loaded
	// If this condition isn't met, then a child was sent without its parent
	Assert( m_hNetworkMoveParent.Get() || !m_hNetworkMoveParent.IsValid() );
	HierarchySetParent(m_hNetworkMoveParent);

	//MarkMessageReceived();		// asw - inline it below
	m_flLastMessageTime = engine->GetLastTimeStamp();	

	// If this entity was new, then latch in various values no matter what.
	if ( updateType == DATA_UPDATE_CREATED )
	{
	// Construct a random value for this instance
	m_flProxyRandomValue = random->RandomFloat( 0, 1 );

	m_vecLastRenderedPos = GetAbsOrigin();

	ResetLatched();
	}

	//UpdatePartitionListEntry();	// asw - inline it	
	CollideType_t shouldCollide = IsRagdoll() ? ENTITY_SHOULD_RESPOND : ENTITY_SHOULD_COLLIDE;

	// Choose the list based on what kind of collisions we want
	int list = PARTITION_CLIENT_NON_STATIC_EDICTS;
	if (shouldCollide == ENTITY_SHOULD_COLLIDE)
	list |= PARTITION_CLIENT_SOLID_EDICTS;
	else if (shouldCollide == ENTITY_SHOULD_RESPOND)
	list |= PARTITION_CLIENT_RESPONSIVE_EDICTS;

	// add the entity to the KD tree so we will collide against it
	partition->RemoveAndInsert( PARTITION_CLIENT_SOLID_EDICTS | PARTITION_CLIENT_RESPONSIVE_EDICTS | PARTITION_CLIENT_NON_STATIC_EDICTS, list, CollisionProp()->GetPartitionHandle() );

	// Add the entity to the nointerp list?
	if ( Teleported() || IsEffectActive(EF_NOINTERP) )
	AddToTeleportList();
	// with custom flcycle: 9775.681 16.61%
	// with custom flycycle and cleaned latch:	9803.268 16.72%
	// without NoteChanged for simulation, etc: 5010.101  8.61%
	// and without NoteChanged for anim 1509.852  2.59%
	*/
}


void C_ASW_Simple_Alien::ClientThink()
{
	BaseClass::ClientThink();

	//ASWUpdateClientSideAnimation();

	m_vecLastRenderedPos = WorldSpaceCenter();

	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( pPlayer && pPlayer->IsSniperScopeActive() )
	{
		m_GlowObject.SetRenderFlags( true, true );
	}
	else
	{
		m_GlowObject.SetRenderFlags( false, false );
	}
}

const Vector& C_ASW_Simple_Alien::GetAimTargetPos(const Vector &vecFiringSrc, bool bWeaponPrefersFlatAiming)
{
	return m_vecLastRenderedPos;
}

int C_ASW_Simple_Alien::DrawModel( int flags, const RenderableInstance_t &instance )
{
	m_vecLastRenderedPos = WorldSpaceCenter();

	return BaseClass::DrawModel(flags, instance);
}


float C_ASW_Simple_Alien::sm_flLastFootstepTime = 0.0f;