//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "dod_gamerules.h"
#include "takedamageinfo.h"
#include "dod_shareddefs.h"
#include "effect_dispatch_data.h"

#include "weapon_dodbase.h"
#include "weapon_dodbipodgun.h"
#include "weapon_dodbaserpg.h"
#include "weapon_dodsniper.h"

#include "movevars_shared.h"
#include "engine/IEngineSound.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "engine/ivdebugoverlay.h"
#include "obstacle_pushaway.h"
#include "props_shared.h"

#include "decals.h"
#include "util_shared.h"

#ifdef CLIENT_DLL
	
	#include "c_dod_player.h"
	#include "prediction.h"
	#include "clientmode_dod.h"
	#include "vgui_controls/AnimationController.h"

	#define CRecipientFilter C_RecipientFilter

#else

	#include "dod_player.h"

#endif

ConVar dod_bonusround( "dod_bonusround", "1", FCVAR_REPLICATED, "If true, the winners of the round can attack in the intermission." );
ConVar sv_showimpacts("sv_showimpacts", "0", FCVAR_REPLICATED | FCVAR_CHEAT, "Shows client (red) and server (blue) bullet impact point" );

void DispatchEffect( const char *pName, const CEffectData &data );

bool CDODPlayer::CanMove( void ) const
{
	bool bValidMoveState = (State_Get() == STATE_ACTIVE || State_Get() == STATE_OBSERVER_MODE);
			
	if ( !bValidMoveState )
	{
		return false;
	}

	return true;
}

// BUG! This is not called on the client at respawn, only first spawn!
void CDODPlayer::SharedSpawn()
{	
	BaseClass::SharedSpawn();

	// Reset the animation state or we will animate to standing
	// when we spawn

	m_Shared.SetJumping( false );

	m_flMinNextStepSoundTime = gpGlobals->curtime;

	m_bPlayingProneMoveSound = false;
}

float GetDensityFromMaterial( surfacedata_t *pSurfaceData )
{
	float flMaterialMod = 1.0f;

	Assert( pSurfaceData );

	// material mod is how many points of damage it costs to go through
	// 1 unit of the material

	switch( pSurfaceData->game.material )
	{
	//super soft
//	case CHAR_TEX_LEAVES:
//		flMaterialMod = 1.2f;
//		break;

	case CHAR_TEX_FLESH:
		flMaterialMod = 1.35f;
		break;

	//soft
//	case CHAR_TEX_STUCCO:
//	case CHAR_TEX_SNOW:
	case CHAR_TEX_GLASS:
	case CHAR_TEX_WOOD:
	case CHAR_TEX_TILE:
		flMaterialMod = 1.8f;
		break;

	//hard
//	case CHAR_TEX_SKY:
//	case CHAR_TEX_ROCK:
//	case CHAR_TEX_SAND:	
	case CHAR_TEX_CONCRETE:
	case CHAR_TEX_DIRT:		// "sand"
		flMaterialMod = 6.6f;
		break;

	//really hard
//	case CHAR_TEX_HEAVYMETAL:
	case CHAR_TEX_GRATE:
	case CHAR_TEX_METAL:
		flMaterialMod = 13.5f;
		break;

	case 'X':		// invisible collision material
		flMaterialMod = 0.1f;
		break;

	//medium
//	case CHAR_TEX_BRICK:
//	case CHAR_TEX_GRAVEL:
//	case CHAR_TEX_GRASS:
	default:

#ifndef CLIENT_DLL
		AssertMsg( 0, UTIL_VarArgs( "Material has unknown materialmod - '%c' \n", pSurfaceData->game.material ) );
#endif

		flMaterialMod = 5.0f;
		break;
	}

	Assert( flMaterialMod > 0 );

	return flMaterialMod;
}

static bool TraceToExit( const Vector &start,
						const Vector &dir,
						Vector &end,
						const float flStepSize,
						const float flMaxDistance )
{
	float flDistance = 0;
	Vector last = start;

	while ( flDistance < flMaxDistance )
	{
		flDistance += flStepSize;

		// no point in tracing past the max distance.
		// if this check fails, we save ourselves a traceline later
		if ( flDistance > flMaxDistance )
		{
			flDistance = flMaxDistance;
		}

		end = start + flDistance * dir; 

		// point contents fails to return proper contents inside a func_detail brush, eg the dod_flash 
		// stairs

		//int contents = UTIL_PointContents( end );

		trace_t tr;
		UTIL_TraceLine( end, end, MASK_SOLID | CONTENTS_HITBOX, NULL, &tr );

		//if ( (UTIL_PointContents ( end ) & MASK_SOLID) == 0 )

		if ( !tr.startsolid )
		{
			// found first free point
			return true;
		}
	}

	return false;
}

#include "ammodef.h"

#define NEW_HITBOX_GROUP_CODE 1
#undef ARM_PENETRATION

#ifndef CLIENT_DLL
#include "ndebugoverlay.h"
#endif
void CDODPlayer::FireBullets( const FireBulletsInfo_t &info )
{
	trace_t			tr;								
	trace_t			reverseTr;						//Used to find exit points
	static int		iMaxPenetrations	= 6;
	int				iPenetrations		= 0;
	float			flDamage			= info.m_flDamage;		//Remaining damage in the bullet
	Vector			vecSrc				= info.m_vecSrc;
	Vector			vecEnd				= vecSrc + info.m_vecDirShooting * info.m_flDistance;

	static int		iTraceMask = ( ( MASK_SOLID | CONTENTS_DEBRIS | CONTENTS_HITBOX | CONTENTS_PRONE_HELPER ) & ~CONTENTS_GRATE );
	 
	CBaseEntity		*pLastHitEntity		= this;	// start with us so we don't trace ourselves
		
	int iDamageType = GetAmmoDef()->DamageType( info.m_iAmmoType );
	int iCollisionGroup = COLLISION_GROUP_NONE;

#ifdef GAME_DLL
	int iNumHeadshots = 0;
#endif

	while ( flDamage > 0 && iPenetrations < iMaxPenetrations )
	{
		//DevMsg( 2, "penetration: %d, starting dmg: %.1f\n", iPenetrations, flDamage );

		CBaseEntity *pPreviousHit = pLastHitEntity;

		// skip the shooter always
		CTraceFilterSkipTwoEntities ignoreShooterAndPrevious( this, pPreviousHit, iCollisionGroup );
		UTIL_TraceLine( vecSrc, vecEnd, iTraceMask, &ignoreShooterAndPrevious, &tr );

		const float rayExtension = 40.0f;
		UTIL_ClipTraceToPlayers( vecSrc, vecEnd + info.m_vecDirShooting * rayExtension, iTraceMask, &ignoreShooterAndPrevious, &tr );

		if ( tr.fraction == 1.0f )
			break; // we didn't hit anything, stop tracing shoot

		// New hitbox code that uses hitbox groups instead of trying to trace
		// through the player
		if ( tr.m_pEnt && tr.m_pEnt->IsPlayer() )
		{
			switch( tr.hitgroup )
			{
#ifdef GAME_DLL
			case HITGROUP_HEAD:
				{
					if ( tr.m_pEnt->GetTeamNumber() != GetTeamNumber() )
					{
						iNumHeadshots++;
					}
				}
				break;
#endif

			case HITGROUP_LEFTARM:
			case HITGROUP_RIGHTARM:
				{
					//DevMsg( 2, "Hit arms, tracing against alt hitboxes.. \n" );

					CDODPlayer *pPlayer = ToDODPlayer( tr.m_pEnt );

					// set hitbox set to "dod_no_arms"
					pPlayer->SetHitboxSet( 1 );

					trace_t newTr;

					// re-fire the trace
					UTIL_TraceLine( vecSrc, vecEnd, iTraceMask, &ignoreShooterAndPrevious, &newTr );

					// if we hit the same player in the chest
					if ( tr.m_pEnt == newTr.m_pEnt )
					{
						//DevMsg( 2, ".. and we hit the chest.\n" );

						Assert( tr.hitgroup != newTr.hitgroup );	// If we hit this, hitbox sets are broken

						// use that damage instead
						tr = newTr;
					}

					// set hitboxes back to "dod"
					pPlayer->SetHitboxSet( 0 );
				}
				break;

			default:
				break;
			}			
		}
			
		pLastHitEntity = tr.m_pEnt;

		if ( sv_showimpacts.GetBool() )
		{
#ifdef CLIENT_DLL
			// draw red client impact markers
			debugoverlay->AddBoxOverlay( tr.endpos, Vector(-1,-1,-1), Vector(1,1,1), QAngle(0,0,0), 255, 0, 0, 127, 4 );

			if ( tr.m_pEnt && tr.m_pEnt->IsPlayer() )
			{
				C_BasePlayer *player = ToBasePlayer( tr.m_pEnt );
				player->DrawClientHitboxes( 4, true );
			}
#else
			// draw blue server impact markers
			NDebugOverlay::Box( tr.endpos, Vector(-1,-1,-1), Vector(1,1,1), 0,0,255,127, 4 );

			if ( tr.m_pEnt && tr.m_pEnt->IsPlayer() )
			{
				CBasePlayer *player = ToBasePlayer( tr.m_pEnt );
				player->DrawServerHitboxes( 4, true );
			}
#endif
		}

#ifdef CLIENT_DLL
		// See if the bullet ended up underwater + started out of the water
		if ( enginetrace->GetPointContents( tr.endpos ) & (CONTENTS_WATER|CONTENTS_SLIME) )
		{	
			trace_t waterTrace;
			UTIL_TraceLine( vecSrc, tr.endpos, (MASK_SHOT|CONTENTS_WATER|CONTENTS_SLIME), this, iCollisionGroup, &waterTrace );
			
			if( waterTrace.allsolid != 1 )
			{
				CEffectData	data;
 				data.m_vOrigin = waterTrace.endpos;
				data.m_vNormal = waterTrace.plane.normal;
				data.m_flScale = random->RandomFloat( 8, 12 );

				if ( waterTrace.contents & CONTENTS_SLIME )
				{
					data.m_fFlags |= FX_WATER_IN_SLIME;
				}

				DispatchEffect( "gunshotsplash", data );
			}
		}
		else
		{
			//Do Regular hit effects

			// Don't decal nodraw surfaces
			if ( !( tr.surface.flags & (SURF_SKY|SURF_NODRAW|SURF_HINT|SURF_SKIP) ) )
			{
				CBaseEntity *pEntity = tr.m_pEnt;
				if ( !( !friendlyfire.GetBool() && pEntity && pEntity->GetTeamNumber() == GetTeamNumber() ) )
				{
					UTIL_ImpactTrace( &tr, iDamageType );
				}
			}
		}
#endif

		// Get surface where the bullet entered ( if it had different surfaces on enter and exit )
		surfacedata_t *pSurfaceData = physprops->GetSurfaceData( tr.surface.surfaceProps );
		Assert( pSurfaceData );
		
		float flMaterialMod = GetDensityFromMaterial(pSurfaceData);

		if ( iDamageType & DMG_MACHINEGUN )
		{
			flMaterialMod *= 0.65;
		}

		// try to penetrate object
		Vector penetrationEnd;
		float flMaxDistance = flDamage / flMaterialMod;

#ifndef CLIENT_DLL
		ClearMultiDamage();

		float flActualDamage = flDamage;

		CTakeDamageInfo dmgInfo( info.m_pAttacker, info.m_pAttacker, flActualDamage, iDamageType );
		CalculateBulletDamageForce( &dmgInfo, info.m_iAmmoType, info.m_vecDirShooting, tr.endpos );
		tr.m_pEnt->DispatchTraceAttack( dmgInfo, info.m_vecDirShooting, &tr );

		DevMsg( 2, "Giving damage ( %.1f ) to entity of type %s\n", flActualDamage, tr.m_pEnt->GetClassname() );

		TraceAttackToTriggers( dmgInfo, tr.startpos, tr.endpos, info.m_vecDirShooting );
#endif

		int stepsize = 16;

		// displacement always stops the bullet
		if ( tr.IsDispSurface() )
		{
			DevMsg( 2, "bullet was stopped by displacement\n" );
			ApplyMultiDamage();
			break;
		}

		// trace through the solid to find the exit point and how much material we went through
		if ( !TraceToExit( tr.endpos, info.m_vecDirShooting, penetrationEnd, stepsize, flMaxDistance ) )
		{
			DevMsg( 2, "bullet was stopped\n" );
			ApplyMultiDamage();
			break;
		}

		// find exact penetration exit
		CTraceFilterSimple ignoreShooter( this, iCollisionGroup );
		UTIL_TraceLine( penetrationEnd, tr.endpos, iTraceMask, &ignoreShooter, &reverseTr );

		// Now we can apply the damage, after we have traced the entity
		// so it doesn't break or die before we have a change to test against it
#ifndef CLIENT_DLL
		ApplyMultiDamage();
#endif

		// Continue looking for the exit point
		if( reverseTr.m_pEnt != tr.m_pEnt && reverseTr.m_pEnt != NULL )
		{
			// something was blocking, trace again
			CTraceFilterSkipTwoEntities ignoreShooterAndBlocker( this, reverseTr.m_pEnt, iCollisionGroup );
			UTIL_TraceLine( penetrationEnd, tr.endpos, iTraceMask, &ignoreShooterAndBlocker, &reverseTr );
		}

		if ( sv_showimpacts.GetBool() )
		{
			debugoverlay->AddLineOverlay( penetrationEnd, reverseTr.endpos, 255, 0, 0, true, 3.0 );
		}

		// penetration was successful

#ifdef CLIENT_DLL
		// bullet did penetrate object, exit Decal
		if ( !( reverseTr.surface.flags & (SURF_SKY|SURF_NODRAW|SURF_HINT|SURF_SKIP) ) )
		{
			CBaseEntity *pEntity = reverseTr.m_pEnt;
			if ( !( !friendlyfire.GetBool() && pEntity && pEntity->GetTeamNumber() == GetTeamNumber() ) )
			{
				UTIL_ImpactTrace( &reverseTr, iDamageType );
			}
		}
#endif

		//setup new start end parameters for successive trace

		// New start point is our last exit point
		vecSrc = reverseTr.endpos + /* 1.0 * */ info.m_vecDirShooting;

		// Reduce bullet damage by material and distanced travelled through that material
		// if it is < 0 we won't go through the loop again
		float flTraceDistance = VectorLength( reverseTr.endpos - tr.endpos );
		
		flDamage -= flMaterialMod * flTraceDistance;

		if( flDamage > 0 )
		{
			DevMsg( 2, "Completed penetration, new damage is %.1f\n", flDamage );
		}
		else
		{
			DevMsg( 2, "bullet was stopped\n" );
		}

		iPenetrations++;
	}

#ifdef GAME_DLL
	HandleHeadshotAchievement( iNumHeadshots );
#endif
}

void CDODPlayer::SetSprinting( bool bIsSprinting )
{
	m_Shared.SetSprinting( bIsSprinting );
}

bool CDODPlayer::IsSprinting( void )
{
	float flVelSqr = GetAbsVelocity().LengthSqr();

	return m_Shared.m_bIsSprinting && ( flVelSqr > 0.5f );
}

bool CDODPlayer::CanAttack( void )
{
	if ( IsSprinting() ) 
		return false;

	if ( GetMoveType() == MOVETYPE_LADDER )
		return false;

	if ( m_Shared.IsJumping() )
		return false;

	if ( m_Shared.IsDefusing() )
		return false;

	// cannot attack while prone moving. except if you have a bazooka
	if ( m_Shared.IsProne() && GetAbsVelocity().LengthSqr() > 1 )
	{
		return false;
	}

	if( m_Shared.IsGoingProne() || m_Shared.IsGettingUpFromProne() )
	{
		return false;
	}

	CDODGameRules *rules = DODGameRules();

	Assert( rules );

	DODRoundState state = rules->State_Get();

	if ( dod_bonusround.GetBool() )
	{
		if ( GetTeamNumber() == TEAM_ALLIES )
		{ 
			return ( state == STATE_RND_RUNNING || state == STATE_ALLIES_WIN );
		}
		else
		{
			return ( state == STATE_RND_RUNNING || state == STATE_AXIS_WIN );
		}
	}
	else
        return ( state == STATE_RND_RUNNING );
}


void CDODPlayer::SetAnimation( PLAYER_ANIM playerAnim )
{
	// In DoD, its CPlayerAnimState object manages ALL the animation state.
	return;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : const Vector
//-----------------------------------------------------------------------------
const Vector CDODPlayer::GetPlayerMins( void ) const
{
	if ( IsObserver() )
	{
		return VEC_OBS_HULL_MIN;	
	}
	else
	{
		if ( GetFlags() & FL_DUCKING )
		{
			return VEC_DUCK_HULL_MIN;
		}
		else if ( m_Shared.IsProne() )
		{
			return VEC_PRONE_HULL_MIN;
		}
		else
		{
			return VEC_HULL_MIN;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : const Vector
//-----------------------------------------------------------------------------
const Vector CDODPlayer::GetPlayerMaxs( void ) const
{	
	if ( IsObserver() )
	{
		return VEC_OBS_HULL_MAX_SCALED( this );	
	}
	else
	{
		if ( GetFlags() & FL_DUCKING )
		{
			return VEC_DUCK_HULL_MAX_SCALED( this );
		}
		else if ( m_Shared.IsProne() )
		{
			return VEC_PRONE_HULL_MAX_SCALED( this );
		}
		else
		{
			return VEC_HULL_MAX_SCALED( this );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : collisionGroup - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CDODPlayer::ShouldCollide( int collisionGroup, int contentsMask ) const
{
	if ( collisionGroup == COLLISION_GROUP_PLAYER_MOVEMENT || collisionGroup == COLLISION_GROUP_PROJECTILE )
	{
		switch( GetTeamNumber() )
		{
		case TEAM_ALLIES:
			if ( !( contentsMask & CONTENTS_TEAM2 ) )
				return false;
			break;

		case TEAM_AXIS:
			if ( !( contentsMask & CONTENTS_TEAM1 ) )
				return false;
			break;
		}
	}

	return BaseClass::ShouldCollide( collisionGroup, contentsMask );
}

// --------------------------------------------------------------------------------------------------- //
// CDODPlayerShared implementation.
// --------------------------------------------------------------------------------------------------- //

CDODPlayerShared::CDODPlayerShared()
{
	m_bProne = false;
	m_bForceProneChange = false;
	m_flNextProneCheck = 0;

	m_flSlowedUntilTime = 0;

	m_flUnProneTime = 0;
	m_flGoProneTime = 0;

	m_flDeployedHeight = STANDING_DEPLOY_HEIGHT;
	m_flDeployChangeTime = gpGlobals->curtime;
	
	SetDesiredPlayerClass( PLAYERCLASS_UNDEFINED );

	m_flLastViewAnimationTime = gpGlobals->curtime;

	m_pViewOffsetAnim = NULL;
}

CDODPlayerShared::~CDODPlayerShared()
{
	if ( m_pViewOffsetAnim )
	{
		delete m_pViewOffsetAnim;
		m_pViewOffsetAnim = NULL;
	}
}

void CDODPlayerShared::Init( CDODPlayer *pPlayer )
{
	m_pOuter = pPlayer;
}

bool CDODPlayerShared::IsDucking( void ) const
{
	return !!( m_pOuter->GetFlags() & FL_DUCKING );
}

bool CDODPlayerShared::IsProne() const
{
	return m_bProne;
}

bool CDODPlayerShared::IsGettingUpFromProne() const
{
	return ( m_flUnProneTime > 0 );
}

bool CDODPlayerShared::IsGoingProne() const
{
	return ( m_flGoProneTime > 0 );
}

void CDODPlayerShared::SetSprinting( bool bSprinting )
{
	if ( bSprinting && !m_bIsSprinting )
	{
		StartSprinting();

		// only one penalty per key press
		if ( m_bGaveSprintPenalty == false )
		{
			m_flStamina -= INITIAL_SPRINT_STAMINA_PENALTY;
			m_bGaveSprintPenalty = true;
		}
	}
	else if ( !bSprinting && m_bIsSprinting )
	{
		StopSprinting();
	}
}

// this is reset when we let go of the sprint key
void CDODPlayerShared::ResetSprintPenalty( void )
{
	m_bGaveSprintPenalty = false;
}

void CDODPlayerShared::StartSprinting( void )
{
	m_bIsSprinting = true;

#ifndef CLIENT_DLL
	m_pOuter->RemoveHintTimer( HINT_USE_SPRINT );
#endif
}

void CDODPlayerShared::StopSprinting( void )
{
	m_bIsSprinting = false;
}

void CDODPlayerShared::SetProne( bool bProne, bool bNoAnimation /* = false */ )
{
	m_bProne = bProne;

	if ( bNoAnimation )
	{
		m_flGoProneTime = 0;
		m_flUnProneTime = 0;

		// cancel the view animation!
		m_bForceProneChange = true;
	}

	if ( !bProne /*&& IsSniperZoomed()*/ )	// forceunzoom for going prone is in StartGoingProne
	{
		ForceUnzoom();
	}
}

void CDODPlayerShared::SetJumping( bool bJumping )
{
	m_bJumping = bJumping;
	
	if ( IsSniperZoomed() )
	{
		ForceUnzoom();
	}
}

void CDODPlayerShared::ForceUnzoom( void )
{
	CWeaponDODBase *pWeapon = GetActiveDODWeapon();
	if( pWeapon && ( pWeapon->GetDODWpnData().m_WeaponType & WPN_MASK_GUN ) )
	{
		CDODSniperWeapon *pSniper = dynamic_cast<CDODSniperWeapon *>(pWeapon);

		if ( pSniper )
		{
			pSniper->ZoomOut();
		}
	}
}

bool CDODPlayerShared::IsBazookaDeployed( void ) const
{
	CWeaponDODBase *pWeapon = GetActiveDODWeapon();
	if( pWeapon && pWeapon->GetDODWpnData().m_WeaponType == WPN_TYPE_BAZOOKA )
	{
		CDODBaseRocketWeapon *pBazooka = (CDODBaseRocketWeapon *)pWeapon;
		return pBazooka->IsDeployed() && !pBazooka->m_bInReload;
	}

	return false;
}

bool CDODPlayerShared::IsBazookaOnlyDeployed( void ) const
{
	CWeaponDODBase *pWeapon = GetActiveDODWeapon();
	if( pWeapon && pWeapon->GetDODWpnData().m_WeaponType == WPN_TYPE_BAZOOKA )
	{
		CDODBaseRocketWeapon *pBazooka = (CDODBaseRocketWeapon *)pWeapon;
		return pBazooka->IsDeployed();
	}

	return false;
}

bool CDODPlayerShared::IsSniperZoomed( void ) const
{
	CWeaponDODBase *pWeapon = GetActiveDODWeapon();
	if( pWeapon && ( pWeapon->GetDODWpnData().m_WeaponType & WPN_MASK_GUN ) )
	{
		CWeaponDODBaseGun *pGun = (CWeaponDODBaseGun *)pWeapon;
		Assert( pGun );
		return pGun->IsSniperZoomed();
	}

	return false;
}

bool CDODPlayerShared::IsInMGDeploy( void ) const
{
	CWeaponDODBase *pWeapon = GetActiveDODWeapon();
	if( pWeapon && pWeapon->GetDODWpnData().m_WeaponType == WPN_TYPE_MG )
	{
		CDODBipodWeapon *pMG = dynamic_cast<CDODBipodWeapon *>( pWeapon );
		Assert( pMG );
		return pMG->IsDeployed();
	}

	return false;
}

bool CDODPlayerShared::IsProneDeployed( void ) const
{
	return ( IsProne() && IsInMGDeploy() );
}

bool CDODPlayerShared::IsSandbagDeployed( void ) const
{
	return ( !IsProne() && IsInMGDeploy() );
}

void CDODPlayerShared::SetDesiredPlayerClass( int playerclass )
{
	m_iDesiredPlayerClass = playerclass;
}

int CDODPlayerShared::DesiredPlayerClass( void )
{
	return m_iDesiredPlayerClass;
}

void CDODPlayerShared::SetPlayerClass( int playerclass )
{
	m_iPlayerClass = playerclass;
}

int CDODPlayerShared::PlayerClass( void )
{
	return m_iPlayerClass;
}

void CDODPlayerShared::SetStamina( float flStamina )
{
	m_flStamina = clamp( flStamina, 0, 100 );
}

CWeaponDODBase* CDODPlayerShared::GetActiveDODWeapon() const
{
	CBaseCombatWeapon *pWeapon = m_pOuter->GetActiveWeapon();
	if ( pWeapon )
	{
		Assert( dynamic_cast< CWeaponDODBase* >( pWeapon ) == static_cast< CWeaponDODBase* >( pWeapon ) );
		return static_cast< CWeaponDODBase* >( pWeapon );
	}
	else
	{
		return NULL;
	}
}

void CDODPlayerShared::SetDeployed( bool bDeployed, float flHeight /* = -1 */ )
{
	if( gpGlobals->curtime - m_flDeployChangeTime < 0.2 )
	{
		Assert(0);
	}

	m_flDeployChangeTime = gpGlobals->curtime;
	m_vecDeployedAngles = m_pOuter->EyeAngles();

	if( flHeight > 0 )
	{
		m_flDeployedHeight = flHeight;
	}
	else
	{
		m_flDeployedHeight = m_pOuter->GetViewOffset()[2];
	}
}

QAngle CDODPlayerShared::GetDeployedAngles( void ) const
{
	return m_vecDeployedAngles;
}

void CDODPlayerShared::SetDeployedYawLimits( float flLeftYaw, float flRightYaw )
{
	m_flDeployedYawLimitLeft = flLeftYaw;
	m_flDeployedYawLimitRight = -flRightYaw;

	m_vecDeployedAngles = m_pOuter->EyeAngles();
}

void CDODPlayerShared::ClampDeployedAngles( QAngle *vecTestAngles )
{
	Assert( vecTestAngles );

	// Clamp Pitch
	vecTestAngles->x = clamp( vecTestAngles->x, MAX_DEPLOY_PITCH, MIN_DEPLOY_PITCH );

	// Clamp Yaw - do a bit more work as yaw will wrap around and cause problems
	float flDeployedYawCenter = GetDeployedAngles().y;

	float flDelta = AngleNormalize( vecTestAngles->y - flDeployedYawCenter );

	if( flDelta < m_flDeployedYawLimitRight )
	{
		vecTestAngles->y = flDeployedYawCenter + m_flDeployedYawLimitRight;
	}
	else if( flDelta > m_flDeployedYawLimitLeft )
	{
		vecTestAngles->y = flDeployedYawCenter + m_flDeployedYawLimitLeft;
	}

	/*
	Msg( "delta %.1f ( left %.1f, right %.1f ) ( %.1f -> %.1f )\n",
		flDelta,
		flDeployedYawCenter + m_flDeployedYawLimitLeft,
		flDeployedYawCenter + m_flDeployedYawLimitRight,
		before,
		vecTestAngles->y );
		*/

}

float CDODPlayerShared::GetDeployedHeight( void ) const
{
	return m_flDeployedHeight;
}

float CDODPlayerShared::GetSlowedTime( void ) const
{
	return m_flSlowedUntilTime;
}

void CDODPlayerShared::SetSlowedTime( float t )
{
	m_flSlowedUntilTime = gpGlobals->curtime + t;
}

void CDODPlayerShared::StartGoingProne( void )
{
	// make the prone sound
	CPASFilter filter( m_pOuter->GetAbsOrigin() );
	filter.UsePredictionRules();
	m_pOuter->EmitSound( filter, m_pOuter->entindex(), "Player.GoProne" );

	// slow to prone speed
	m_flGoProneTime = gpGlobals->curtime + TIME_TO_PRONE;

	m_flUnProneTime = 0.0f;	//reset

	if ( IsSniperZoomed() )
		ForceUnzoom();
}

void CDODPlayerShared::StandUpFromProne( void )
{	
	// make the prone sound
	CPASFilter filter( m_pOuter->GetAbsOrigin() );
	filter.UsePredictionRules();
	m_pOuter->EmitSound( filter, m_pOuter->entindex(), "Player.UnProne" );

	// speed up to target speed
	m_flUnProneTime = gpGlobals->curtime + TIME_TO_PRONE;

	m_flGoProneTime = 0.0f;	//reset 
}

bool CDODPlayerShared::CanChangePosition( void )
{
	if ( IsInMGDeploy() )
		return false;

	if ( IsGettingUpFromProne() )
		return false;

	if ( IsGoingProne() )
		return false;

	return true;
}

void CDODPlayer::UpdateStepSound( surfacedata_t *psurface, const Vector &vecOrigin, const Vector &vecVelocity )
{
	Vector knee;
	Vector feet;
	float height;
	int	fLadder;

	if ( m_flStepSoundTime > 0 )
	{
		m_flStepSoundTime -= 1000.0f * gpGlobals->frametime;
		if ( m_flStepSoundTime < 0 )
		{
			m_flStepSoundTime = 0;
		}
	}

	if ( m_flStepSoundTime > 0 )
		return;

	if ( GetFlags() & (FL_FROZEN|FL_ATCONTROLS))
		return;

	if ( GetMoveType() == MOVETYPE_NOCLIP || GetMoveType() == MOVETYPE_OBSERVER )
		return;

	if ( !sv_footsteps.GetFloat() )
		return;

	float speed = VectorLength( vecVelocity );
	float groundspeed = Vector2DLength( vecVelocity.AsVector2D() );

	// determine if we are on a ladder
	fLadder = ( GetMoveType() == MOVETYPE_LADDER );

	float flDuck;

	if ( ( GetFlags() & FL_DUCKING) || fLadder )
	{
		flDuck = 100;
	}
	else
	{
		flDuck = 0;
	}

	static float flMinProneSpeed = 10.0f;
	static float flMinSpeed = 70.0f;
	static float flRunSpeed = 110.0f;

	bool onground = ( GetFlags() & FL_ONGROUND );
	bool movingalongground = ( groundspeed > 0.0f );
	bool moving_fast_enough =  ( speed >= flMinSpeed );

	// always play a step sound if we are moving faster than 

	// To hear step sounds you must be either on a ladder or moving along the ground AND
	// You must be moving fast enough

	CheckProneMoveSound( groundspeed, onground );

	if ( !moving_fast_enough || !(fLadder || ( onground && movingalongground )) )
	{
		return;
	}

	bool bWalking = ( speed < flRunSpeed );		// or ducking!

	VectorCopy( vecOrigin, knee );
	VectorCopy( vecOrigin, feet );

	height = GetPlayerMaxs()[ 2 ] - GetPlayerMins()[ 2 ];

	knee[2] = vecOrigin[2] + 0.2 * height;

	float flVol;

	// find out what we're stepping in or on...
	if ( fLadder )
	{
		psurface = physprops->GetSurfaceData( physprops->GetSurfaceIndex( "ladder" ) );
		flVol = 1.0;
		m_flStepSoundTime = 350;
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
		flVol = 0.65;
		m_flStepSoundTime = 600;
	}
	else if ( enginetrace->GetPointContents( feet ) & MASK_WATER )
	{
		psurface = physprops->GetSurfaceData( physprops->GetSurfaceIndex( "water" ) );
		flVol = bWalking ? 0.2 : 0.5;
		m_flStepSoundTime = bWalking ? 400 : 300;		
	}
	else
	{
		if ( !psurface )
			return;

		if ( bWalking )
		{
			m_flStepSoundTime = 400;
		}
		else
		{
			if ( speed > 200 )
			{
				int speeddiff = PLAYER_SPEED_SPRINT - PLAYER_SPEED_RUN;
				int diff = speed - PLAYER_SPEED_RUN;

				float percent = (float)diff / (float)speeddiff;

				m_flStepSoundTime = 300.0f - 30.0f * percent;
			}
			else 
			{
				m_flStepSoundTime = 400;
			}
		}

		switch ( psurface->game.material )
		{
		default:
		case CHAR_TEX_CONCRETE:						
			flVol = bWalking ? 0.2 : 0.5;
			break;

		case CHAR_TEX_METAL:	
			flVol = bWalking ? 0.2 : 0.5;
			break;

		case CHAR_TEX_DIRT:
			flVol = bWalking ? 0.25 : 0.55;
			break;

		case CHAR_TEX_VENT:	
			flVol = bWalking ? 0.4 : 0.7;
			break;

		case CHAR_TEX_GRATE:
			flVol = bWalking ? 0.2 : 0.5;
			break;

		case CHAR_TEX_TILE:	
			flVol = bWalking ? 0.2 : 0.5;
			break;

		case CHAR_TEX_SLOSH:
			flVol = bWalking ? 0.2 : 0.5;
			break;
		}
	}

	m_flStepSoundTime += flDuck; // slower step time if ducking

	if ( GetFlags() & FL_DUCKING )
	{
		flVol *= 0.65;
	}

	// protect us from prediction errors a little bit
	if ( m_flMinNextStepSoundTime > gpGlobals->curtime )
	{
		return;
	}

	m_flMinNextStepSoundTime = gpGlobals->curtime + 0.1f;	

	PlayStepSound( feet, psurface, flVol, false );
}

void CDODPlayer::CheckProneMoveSound( int groundspeed, bool onground )
{
#ifdef CLIENT_DLL
	bool bShouldPlay = (groundspeed > 10) && (onground == true) && m_Shared.IsProne() && IsAlive();

	if ( m_bPlayingProneMoveSound && !bShouldPlay )
	{
		StopSound( "Player.MoveProne" );
		m_bPlayingProneMoveSound= false;
	}
	else if ( !m_bPlayingProneMoveSound && bShouldPlay )
	{
		CRecipientFilter filter;
		filter.AddRecipientsByPAS( WorldSpaceCenter() );
		EmitSound( filter, entindex(), "Player.MoveProne" );

		m_bPlayingProneMoveSound = true;
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : step - 
//			fvol - 
//			force - force sound to play
//-----------------------------------------------------------------------------
void CDODPlayer::PlayStepSound( Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force )
{
	if ( gpGlobals->maxClients > 1 && !sv_footsteps.GetFloat() )
		return;

#if defined( CLIENT_DLL )
	// during prediction play footstep sounds only once
	if ( prediction->InPrediction() && !prediction->IsFirstTimePredicted() )
		return;
#endif

	if ( !psurface )
		return;

	unsigned short stepSoundName = m_Local.m_nStepside ? psurface->sounds.stepleft : psurface->sounds.stepright;
	m_Local.m_nStepside = !m_Local.m_nStepside;

	if ( !stepSoundName )
		return;

	IPhysicsSurfaceProps *physprops = MoveHelper( )->GetSurfaceProps();
	const char *pSoundName = physprops->GetString( stepSoundName );
	CSoundParameters params;

	// we don't always know the model, so go by team
	char *pModelNameForGender = DOD_PLAYERMODEL_AXIS_RIFLEMAN;

	if( GetTeamNumber() == TEAM_ALLIES )
		pModelNameForGender = DOD_PLAYERMODEL_US_RIFLEMAN;

	if ( !CBaseEntity::GetParametersForSound( pSoundName, params, pModelNameForGender ) )
		return;	

	CRecipientFilter filter;
	filter.AddRecipientsByPAS( vecOrigin );

#ifndef CLIENT_DLL
	// im MP, server removed all players in origins PVS, these players 
	// generate the footsteps clientside
	if ( gpGlobals->maxClients > 1 )
		filter.RemoveRecipientsByPVS( vecOrigin );
#endif

	EmitSound_t ep;
	ep.m_nChannel = params.channel;
	ep.m_pSoundName = params.soundname;
	ep.m_flVolume = fvol;
	ep.m_SoundLevel = params.soundlevel;
	ep.m_nFlags = 0;
	ep.m_nPitch = params.pitch;
	ep.m_pOrigin = &vecOrigin;

	EmitSound( filter, entindex(), ep );
}

Activity CDODPlayer::TranslateActivity( Activity baseAct, bool *pRequired /* = NULL */ )
{
	Activity translated = baseAct;

	if ( GetActiveWeapon() )
	{
		translated = GetActiveWeapon()->ActivityOverride( baseAct, pRequired );
	}
	else if (pRequired)
	{
		*pRequired = false;
	}

	return translated;
}

void CDODPlayerShared::SetCPIndex( int index )
{
#ifdef CLIENT_DLL

	if ( m_pOuter->IsLocalPlayer() )
	{
		if ( index == -1 )
		{
			// just left an area
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "ObjectiveIconShrink" ); 
		}
		else
		{
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "ObjectiveIconGrow" ); 
		}
	}

#endif

	m_iCPIndex = index;
}

void CDODPlayerShared::SetLastViewAnimTime( float flTime )
{
	m_flLastViewAnimationTime = flTime;
}

float CDODPlayerShared::GetLastViewAnimTime( void )
{
	return m_flLastViewAnimationTime;
}

void CDODPlayerShared::ResetViewOffsetAnimation( void )
{
    if ( m_pViewOffsetAnim )
	{
		//cancel it!
		m_pViewOffsetAnim->Reset();
	}
}

void CDODPlayerShared::ViewOffsetAnimation( Vector vecDest, float flTime, ViewAnimationType type )
{
	if ( !m_pViewOffsetAnim )
	{
		m_pViewOffsetAnim =  CViewOffsetAnimation::CreateViewOffsetAnim( m_pOuter );
	}

	Assert( m_pViewOffsetAnim );

	if ( m_pViewOffsetAnim )
	{
		m_pViewOffsetAnim->StartAnimation( m_pOuter->GetViewOffset(), vecDest, flTime, type );
	}
}

void CDODPlayerShared::ViewAnimThink( void )
{
	// Check for the flag that will reset our view animations
	// when the player respawns
	if ( m_bForceProneChange )
	{
		ResetViewOffsetAnimation();

		m_pOuter->SetViewOffset( VEC_VIEW_SCALED( m_pOuter ) );

		m_bForceProneChange = false;
	}

	if ( m_pViewOffsetAnim )
	{
		m_pViewOffsetAnim->Think();
	}
}

void CDODPlayerShared::ComputeWorldSpaceSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs )
{
	Vector org = m_pOuter->GetAbsOrigin();

	if ( IsProne() )
	{
		static Vector vecProneMin(-44, -44, 0 );
		static Vector vecProneMax(44, 44, 24 );

		VectorAdd( vecProneMin, org, *pVecWorldMins );
		VectorAdd( vecProneMax, org, *pVecWorldMaxs );
	}
	else
	{
		static Vector vecMin(-32, -32, 0 );
		static Vector vecMax(32, 32, 72 );

		VectorAdd( vecMin, org, *pVecWorldMins );
		VectorAdd( vecMax, org, *pVecWorldMaxs );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets whether this player is dominating the specified other player
//-----------------------------------------------------------------------------
void CDODPlayerShared::SetPlayerDominated( CDODPlayer *pPlayer, bool bDominated )
{
	int iPlayerIndex = pPlayer->entindex();
	m_bPlayerDominated.Set( iPlayerIndex, bDominated );
	pPlayer->m_Shared.SetPlayerDominatingMe( m_pOuter, bDominated );
}

//-----------------------------------------------------------------------------
// Purpose: Sets whether this player is being dominated by the other player
//-----------------------------------------------------------------------------
void CDODPlayerShared::SetPlayerDominatingMe( CDODPlayer *pPlayer, bool bDominated )
{
	int iPlayerIndex = pPlayer->entindex();
	m_bPlayerDominatingMe.Set( iPlayerIndex, bDominated );
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether this player is dominating the specified other player
//-----------------------------------------------------------------------------
bool CDODPlayerShared::IsPlayerDominated( int iPlayerIndex )
{
#ifdef CLIENT_DLL
	// On the client, we only have data for the local player.
	// As a result, it's only valid to ask for dominations related to the local player
	C_DODPlayer *pLocalPlayer = C_DODPlayer::GetLocalDODPlayer();
	if ( !pLocalPlayer )
		return false;

	Assert( m_pOuter->IsLocalPlayer() || pLocalPlayer->entindex() == iPlayerIndex );

	if ( m_pOuter->IsLocalPlayer() )
		return m_bPlayerDominated.Get( iPlayerIndex );

	return pLocalPlayer->m_Shared.IsPlayerDominatingMe( m_pOuter->entindex() );
#else
	// Server has all the data.
	return m_bPlayerDominated.Get( iPlayerIndex );
#endif
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CDODPlayerShared::IsPlayerDominatingMe( int iPlayerIndex )
{
	return m_bPlayerDominatingMe.Get( iPlayerIndex );
}
