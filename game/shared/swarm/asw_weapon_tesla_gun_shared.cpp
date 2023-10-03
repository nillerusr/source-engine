#include "cbase.h"
#include "npcevent.h"
#include "Sprite.h"
#include "beam_shared.h"
#include "takedamageinfo.h"
#include "gamerules.h"
#include "in_buttons.h"
#include "asw_weapon_parse.h"
#include "asw_marine_skills.h"

#ifdef CLIENT_DLL
#include "c_baseplayer.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "c_te_effect_dispatch.h"
#include "c_asw_alien.h"
#include "fx.h"
#else
#include "player.h"
#include "asw_player.h"
#include "asw_marine.h"
#include "soundent.h"
#include "game.h"
#include "te_effect_dispatch.h"
#include "asw_marine_speech.h"
#include "asw_weapon_ammo_bag_shared.h"
#include "asw_alien.h"
#include "ndebugoverlay.h"
#endif

#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "IEffects.h"
#include "asw_trace_filter_shot.h"
#include "particle_parse.h"

#include "asw_weapon_tesla_gun_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static const float ASW_TG_RANGE = 240.0f; // Range of electrical discharge - TODO: make this an attribute?
static const float ASW_TG_TRACE_SIZE = 10.0f; // Size of the laser's trace attack
static const float ASW_TG_FIELD_DURATION = 2.0f; // Duration of electrical field left behind

//static const float ASW_TG_SHOCK_RATE = 0.25f; // Rate at which we shock entities we're latched on
static const float ASW_TG_SEARCH_DIAMETER = 48.0f; // How far past the range to search for enemies to latch on to
static const float SQRT3 = 1.732050807569; // for computing max extents inside a box
static const Vector ASW_TG_SEARCH_BOX_EXTENTS( ASW_TG_SEARCH_DIAMETER/2.0f, ASW_TG_SEARCH_DIAMETER/2.0f, ASW_TG_SEARCH_DIAMETER/2.0f );

static const int ASW_TG_MAX_SHOCK_ALIENS = 6; // Max # of aliens to search for a shock target


extern int	g_sModelIndexSmoke;			// (in combatweapon.cpp) holds the index for the smoke cloud

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Tesla_Gun, DT_ASW_Weapon_Tesla_Gun );

BEGIN_NETWORK_TABLE( CASW_Weapon_Tesla_Gun, DT_ASW_Weapon_Tesla_Gun )
#ifdef CLIENT_DLL
// recvprops
RecvPropTime( RECVINFO( m_fSlowTime ) ),
RecvPropInt( RECVINFO( m_FireState ) ),
RecvPropEHandle( RECVINFO( m_hShockEntity ) ),
RecvPropVector( RECVINFO( m_vecShockPos ) ),
#else
// sendprops
SendPropTime( SENDINFO( m_fSlowTime ) ),
SendPropInt( SENDINFO( m_FireState ), Q_log2(ASW_TG_NUM_FIRE_STATES)+1, SPROP_UNSIGNED ),
SendPropEHandle( SENDINFO( m_hShockEntity ) ),
SendPropVector( SENDINFO( m_vecShockPos ) ),
#endif
END_NETWORK_TABLE()    

LINK_ENTITY_TO_CLASS( asw_weapon_tesla_gun, CASW_Weapon_Tesla_Gun );
PRECACHE_WEAPON_REGISTER( asw_weapon_tesla_gun );

#if defined( CLIENT_DLL )
BEGIN_PREDICTION_DATA( CASW_Weapon_Tesla_Gun )
	DEFINE_PRED_FIELD_TOL( m_fSlowTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),
	DEFINE_PRED_FIELD( m_vecShockPos, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CASW_Weapon_Tesla_Gun::CASW_Weapon_Tesla_Gun( void )
{
	m_bReloadsSingly	= false;
	m_bFiresUnderwater	= true;
#ifdef GAME_DLL
	m_fLastForcedFireTime = 0;	// last time we forced an AI marine to push its fire button
#endif
	m_flLastShockTime = 0.0f;
	m_flLastDischargeTime = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CASW_Weapon_Tesla_Gun::Precache( void )
{
	PrecacheScriptSound("ASW_MiningLaser.ReloadA");
	PrecacheScriptSound("ASW_MiningLaser.ReloadB");
	PrecacheScriptSound("ASW_MiningLaser.ReloadC");
	PrecacheScriptSound( "ASW_Tesla_Laser.Damage" );
	PrecacheScriptSound( "ASW_Tesla_Laser.Loop" );
	PrecacheScriptSound( "ASW_Tesla_Laser.Stop" );
	PrecacheParticleSystem( "mining_laser_exhaust" );
	BaseClass::Precache();
}

bool CASW_Weapon_Tesla_Gun::HasAmmo()
{
	return m_iClip1 > 0;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CASW_Weapon_Tesla_Gun::PrimaryAttack( void )
{
	CASW_Player *pPlayer = GetCommander();
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine || !pMarine->IsAlive() )
	{
		EndAttack();
		return;
	}

	// don't fire underwater
	if ( pMarine->GetWaterLevel() == 3 )
	{
		WeaponSound( EMPTY );

		m_flNextPrimaryAttack = gpGlobals->curtime + 0.5;
		m_flNextSecondaryAttack = gpGlobals->curtime + 0.5;
		return;
	}

	Vector vecSrc = pMarine->Weapon_ShootPosition( );
	Vector vecAiming = vec3_origin;

	if ( pPlayer && pMarine->IsInhabited() )
	{
		vecAiming = pPlayer->GetAutoaimVectorForMarine(pMarine, GetAutoAimAmount(), GetVerticalAdjustOnlyAutoAimAmount());	// 45 degrees = 0.707106781187
	}
	else
	{
#ifndef CLIENT_DLL
		vecAiming = pMarine->GetActualShootTrajectory( vecSrc );
#endif
	}

#ifdef GAME_DLL
	m_bIsFiring = true;

	switch ( m_FireState )
	{
		case ASW_TG_FIRE_OFF:
		{
			Fire( vecSrc, vecAiming );
			m_flNextPrimaryAttack = gpGlobals->curtime;
			break;
		}

		case ASW_TG_FIRE_DISCHARGE:
		{
			Fire( vecSrc, vecAiming );

			EHANDLE hShockEntity = m_hShockEntity.Get();

			// Search for nearby entities to shock
			if ( !hShockEntity )
			{
				CBaseEntity *pList[ASW_TG_MAX_SHOCK_ALIENS];
				int nCount = UTIL_EntitiesInBox( pList, ASW_TG_MAX_SHOCK_ALIENS, m_vecShockPos.Get() - ASW_TG_SEARCH_BOX_EXTENTS, m_vecShockPos.Get() + ASW_TG_SEARCH_BOX_EXTENTS, 0 );
				for ( int i = 0; i < nCount; i++ )
				{
					if ( ShockAttach( pList[ i ] ) )
					{
						SetFiringState( ASW_TG_FIRE_ATTACHED );
						m_flLastShockTime = 0.0f;
						break;
					}
				}
			}

			if ( gpGlobals->curtime > m_flLastDischargeTime + (GetFireRate()*3) )
			{
				m_iClip1 = MAX( 0, m_iClip1 - 1 );
				m_flLastDischargeTime = gpGlobals->curtime;
			}

			m_flNextPrimaryAttack = gpGlobals->curtime;
			break;
		}

		case ASW_TG_FIRE_ATTACHED:
		{
			Fire( vecSrc, vecAiming );

			EHANDLE hShockEntity = m_hShockEntity.Get();
			if ( !hShockEntity || hShockEntity->GetHealth() <= 0 )
			{
				SetFiringState( ASW_TG_FIRE_DISCHARGE );
				break;
			}

			// detach if...
			
			// too far
			float flShockDistance = (hShockEntity->GetAbsOrigin() - pMarine->GetAbsOrigin()).LengthSqr();
			if ( flShockDistance > (GetWeaponRange() + SQRT3*ASW_TG_SEARCH_DIAMETER)*(GetWeaponRange() + SQRT3*ASW_TG_SEARCH_DIAMETER) )
			{
				ShockDetach();
				SetFiringState( ASW_TG_FIRE_DISCHARGE );
				break;
			}

			// dead
			if ( hShockEntity->m_iHealth <= 0 )
			{
				ShockDetach();
				SetFiringState( ASW_TG_FIRE_DISCHARGE );
				break;
			}

			// facing another direction
			Vector vecAttach = hShockEntity->GetAbsOrigin() - pMarine->GetAbsOrigin();
			Vector vecForward;
			CASW_Player *pPlayer = pMarine->GetCommander();

			if ( pPlayer )
			{
				AngleVectors( pPlayer->EyeAngles(), &vecForward );
				if ( DotProduct( vecForward, vecAttach ) < 0.0f )
				{
					ShockDetach();
					SetFiringState( ASW_TG_FIRE_DISCHARGE );
					break;
				}
			}


			if ( gpGlobals->curtime >= m_flLastShockTime + GetFireRate() )
			{
				ShockEntity();
				m_flLastShockTime = gpGlobals->curtime;
				pMarine->OnWeaponFired( this, 1 );
			}

			//if (m_iClip1 > 0)		// only force the fire wait time if we have ammo for another shot
			//	m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
			//else
			m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
			break;
		}
	}

	SendWeaponAnim( GetPrimaryAttackActivity() );			

	SetWeaponIdleTime( gpGlobals->curtime + GetFireRate() );
#endif

	m_fSlowTime = gpGlobals->curtime + GetFireRate();
}

float CASW_Weapon_Tesla_Gun::GetWeaponDamage()
{
	float flDamage = GetWeaponInfo()->m_flBaseDamage;

	if ( GetMarine() )
	{
		flDamage += MarineSkills()->GetSkillBasedValueByMarine(GetMarine(), ASW_MARINE_SKILL_ACCURACY, ASW_MARINE_SUBSKILL_ACCURACY_TESLA_CANNON_DMG);
	}

	return flDamage;
}

void CASW_Weapon_Tesla_Gun::ShockEntity()
{
	CASW_Marine *pMarine = GetMarine();
	EHANDLE hShockEntity = m_hShockEntity.Get();
	Assert( hShockEntity && hShockEntity->m_takedamage != DAMAGE_NO && pMarine );

	Vector vecDir = hShockEntity->GetAbsOrigin() - pMarine->GetAbsOrigin();

	ClearMultiDamage();
	float flBaseDamage = GetWeaponDamage() * g_pGameRules->GetDamageMultiplier();
	CTakeDamageInfo info( this, pMarine, flBaseDamage, DMG_SHOCK );
	info.SetWeapon(this);
	CalculateMeleeDamageForce( &info, vecDir, m_AttackTrace.endpos );

#ifdef GAME_DLL
	DoArcingShock( flBaseDamage, hShockEntity.Get() );
#endif

	// Whatever damage didn't get used for electricity arcing gets applied to a single target			
	info.SetDamage( info.GetDamage() + flBaseDamage );
	hShockEntity->DispatchTraceAttack( info, vecDir, &m_AttackTrace );
	ApplyMultiDamage();

	//Msg( "%f - Shocking target for %f\n", gpGlobals->curtime, info.GetDamage() + flBaseDamage );

#ifdef GAME_DLL
	CASW_Alien *pTargetAlien = dynamic_cast<CASW_Alien*>( hShockEntity.Get() );

	if ( pTargetAlien )
	{
		pTargetAlien->ForceFlinch( pMarine->GetAbsOrigin() );		
	}
#endif

	// decrement ammo
	m_iClip1 = MAX( 0, m_iClip1 - 1 );

	CPASFilter filter( hShockEntity->GetAbsOrigin() );
	CBaseEntity::EmitSound( filter, SOUND_FROM_WORLD, "ASW_Tesla_Laser.Damage", &hShockEntity->GetAbsOrigin() );
}

#ifdef GAME_DLL
void CASW_Weapon_Tesla_Gun::DoArcingShock( float flBaseDamage, CBaseEntity *pLastShocked )
{
	if ( !GetOwner() )
		return;

	// search other enemies near this one
	int iMaxArcs = 2;	// TODO: max arc should be attribute of the weapon? if we can combine two attribs into one somehow
	CUtlVector<CBaseEntity*> shocked;
	int iNumShocked = 0; // TODO: Use size of shocked vector for this?
	shocked.AddToTail( pLastShocked );
	float flArcDistSqr = (GetWeaponRange()/1.5f) * (GetWeaponRange()/1.5f);
	float flShockDamage = flBaseDamage;	// TODO: base on attribute?
	float flDamageReduction = flShockDamage / (float)iMaxArcs; // how much damage each arc loses
	Vector vecShockSrc = pLastShocked->WorldSpaceCenter();

	//CBaseEntity *pLastShocked = pTarget;
	//pLastShocked->EmitSound( "Electricity.Zap" );

	//CASW_Alien *pAlien = dynamic_cast<CASW_Alien*>( pTarget );
	//if ( pAlien )
	//	pAlien->ForceFlinch( vecShockPos );	

	for ( int k = 0; k < iMaxArcs; k++ )
	{
		CAI_BaseNPC **	ppAIs 	= g_AI_Manager.AccessAIs();
		int 			nAIs 	= g_AI_Manager.NumAIs();
		bool			bShocked = false;

		for ( int i = 0; i < nAIs; i++ )
		{
			if ( shocked.Find( ppAIs[i] ) != shocked.InvalidIndex() )	// already shocked this guy
				continue;

			if ( GetOwner()->IRelationType( ppAIs[i] ) >= D_LI )	// friendly
				continue;

			if ( ppAIs[i]->GetAbsOrigin().DistToSqr( vecShockSrc ) > flArcDistSqr )	// too far away
				continue;

			// trace from the last shock position to this guy
			trace_t shockTR;
			Vector vecAIPos = ppAIs[i]->WorldSpaceCenter();
			CASWTraceFilterShot traceFilter( this, pLastShocked, COLLISION_GROUP_NONE );
			AI_TraceLine( vecShockSrc, vecAIPos, MASK_SHOT, &traceFilter, &shockTR );

			if ( shockTR.fraction != 1.0 && shockTR.m_pEnt )
			{
				if ( GetOwner()->IRelationType( shockTR.m_pEnt ) >= D_LI )	// friendly
					continue;

				// shock this thing
				vecAIPos = shockTR.endpos;
				CTakeDamageInfo shockDmgInfo( this, this, flShockDamage, DMG_SHOCK );					
				Vector vecDir = vecAIPos - vecShockSrc;
				VectorNormalize( vecDir );

				ClearMultiDamage();	
				shockDmgInfo.SetDamagePosition( shockTR.endpos );
				shockDmgInfo.SetDamageForce( vecDir );
				shockDmgInfo.ScaleDamageForce( 1.0 );
				shockDmgInfo.SetWeapon( this );							

				shockTR.m_pEnt->DispatchTraceAttack( shockDmgInfo, vecDir, &shockTR );
				CASW_Alien *pAlien = dynamic_cast<CASW_Alien*>( shockTR.m_pEnt );
				if ( pAlien )
					pAlien->ForceFlinch( pLastShocked->WorldSpaceCenter() );	

				// spawn a shock effect
				CRecipientFilter filter;
				filter.AddAllPlayers();
				UserMessageBegin( filter, "ASWEnemyTeslaGunArcShock" );
				WRITE_SHORT( shockTR.m_pEnt->entindex() );
				WRITE_SHORT( pLastShocked->entindex() );
				MessageEnd();

				vecShockSrc = vecAIPos;
				pLastShocked = shockTR.m_pEnt;
				bShocked = true;
				iNumShocked++;

				// reduce shock damage as energy arcs
				flShockDamage = MAX( flShockDamage - flDamageReduction, 0.0f );

				break;
			}
		}
		if ( !bShocked )
			break;
	}
	/*
	if ( iNumShocked > 0 )
	{
		// Return reduced shock damage if we arced
		info.SetDamage( flShockDamage );
	}*/
}
#endif //GAME_DLL

void CASW_Weapon_Tesla_Gun::Fire( const Vector &vecOrigSrc, const Vector &vecDir )
{
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
	{
		return;
	}
	
	Vector vecDest	= vecOrigSrc + (vecDir * GetWeaponRange());
	Vector mins = Vector( -ASW_TG_TRACE_SIZE, -ASW_TG_TRACE_SIZE, 0.0f);
	Vector maxs = Vector( ASW_TG_TRACE_SIZE, ASW_TG_TRACE_SIZE, 12.0f );

	trace_t	tr;
	//UTIL_TraceLine( vecOrigSrc, vecDest, MASK_SHOT, pMarine, COLLISION_GROUP_NONE, &tr );
	UTIL_TraceHull( vecOrigSrc, vecDest, mins, maxs, MASK_SHOT, pMarine, COLLISION_GROUP_NONE, &tr );
#ifndef CLIENT_DLL
	//QAngle angles;
	//VectorAngles( vecDir, angles );

	//if ( tr.DidHit() && tr.m_pEnt && tr.m_pEnt->IsNPC() )
	//{
	//	NDebugOverlay::SweptBox( vecOrigSrc, tr.endpos, mins, maxs, angles, 0, 255, 0, 128, 5.0f );
	//}
	//else
	//{
	//	//NDebugOverlay::Line( vecOrigSrc, tr.endpos, 0, 255, 0, true, 5.0f );
	//	NDebugOverlay::SweptBox( vecOrigSrc, (tr.m_pEnt && tr.m_pEnt->IsNPC()) ? tr.endpos : vecDest, mins, maxs, angles, 255, 0, 0, 128, 5.0f );
	//}
#endif


	if ( tr.allsolid )
		return;

	CBaseEntity *pEntity = tr.m_pEnt;
	CASW_Marine *pTargetMarine = NULL;
	if ( pEntity )
	{
		pTargetMarine = CASW_Marine::AsMarine( pEntity );
	}

	Vector vecUp, vecRight;
	QAngle angDir;

	VectorAngles( vecDir, angDir );
	AngleVectors( angDir, NULL, &vecRight, &vecUp );
	m_AttackTrace = tr;

	if( ShockAttach( pEntity ) )
	{
		SetFiringState( ASW_TG_FIRE_ATTACHED );
	}
	else if ( !m_hShockEntity.Get() )
	{
		// If we fail to hit something, and we're not shocking someone, we're just discharging onto the ground
		SetFiringState( ASW_TG_FIRE_DISCHARGE );
	}


#ifdef GAME_DLL
	if (pMarine && m_iClip1 <= 0 && pMarine->GetAmmoCount(m_iPrimaryAmmoType) <= 0 )
	{
		// check he doesn't have ammo in an ammo bay
		CASW_Weapon_Ammo_Bag* pAmmoBag = dynamic_cast<CASW_Weapon_Ammo_Bag*>(pMarine->GetASWWeapon(0));
		if (!pAmmoBag)
			pAmmoBag = dynamic_cast<CASW_Weapon_Ammo_Bag*>(pMarine->GetASWWeapon(1));
		if (!pAmmoBag || !pAmmoBag->CanGiveAmmoToWeapon(this))
			pMarine->OnWeaponOutOfAmmo(true);
	}
#endif

	if ( tr.DidHit() )
	{
		vecDest = tr.endpos;
	}
	else
	{
		// Arc down to the ground with a bit of randomness
		Vector down = 0.9f * Vector( 0.0f, 0.0f, -1.0f ) + 0.1f * RandomVector( -1.0, 1.0 );
		// Trace down far enough to hit lower levels
		UTIL_TraceLine( vecDest, vecDest + GetWeaponRange() * down * 2.0f, MASK_NPCSOLID_BRUSHONLY, pMarine, COLLISION_GROUP_NONE, &tr );
		
		// FIXME: if we really can't hit anything, just leave vecDest alone
		if( tr.DidHit() )
		{
			vecDest = tr.endpos;
		}
	}

	m_vecShockPos = vecDest;
}

bool CASW_Weapon_Tesla_Gun::Reload( void )
{
	EndAttack();

	return BaseClass::Reload();
}

void CASW_Weapon_Tesla_Gun::EndAttack( void )
{
	/*
	Vector vecEjectPos, vecEjectPos2;
	QAngle angEject, angEject2;
	int iAttachment = LookupAttachment( "eject1" );
	GetAttachment( iAttachment, vecEjectPos, angEject );

	unsigned char color[3];
	color[0] = 50;
	color[1] = 128;
	color[2] = 50;		

	iAttachment = LookupAttachment( "eject2" );
	GetAttachment( iAttachment, vecEjectPos2, angEject2 );
	*/

#ifdef CLIENT_DLL
	//FX_Smoke( vecEjectPos, angEject, 0.5, 5, &color[0], 192 );
	//FX_Smoke( vecEjectPos2, angEject2, 0.5, 5, &color[0], 192 );

	if ( m_pDischargeEffect )
	{
		m_pDischargeEffect->StopEmission();
	}
#else
	//CPVSFilter filter( vecEjectPos );
	//te->Smoke( filter, 0.0, &vecEjectPos, g_sModelIndexSmoke, 0.5f, 10 );
	//te->Smoke( filter, 0.0, &vecEjectPos2, g_sModelIndexSmoke, 0.5f, 10 );
#endif

	SetWeaponIdleTime( gpGlobals->curtime + 2.0 );
	
	SetFiringState( ASW_TG_FIRE_OFF );
	
	ShockDetach();

	ClearIsFiring();

	m_bIsFiring = false;
}

void CASW_Weapon_Tesla_Gun::Drop( const Vector &vecVelocity )
{	
	EndAttack();

	BaseClass::Drop( vecVelocity );
}

bool CASW_Weapon_Tesla_Gun::Holster( CBaseCombatWeapon *pSwitchingTo )
{
    EndAttack();

	return BaseClass::Holster( pSwitchingTo );
}

void CASW_Weapon_Tesla_Gun::WeaponIdle( void )
{
	if ( !HasWeaponIdleTimeElapsed() )
		return;

	if ( m_FireState != ASW_TG_FIRE_OFF )
	{
		EndAttack();
	}

	float flIdleTime = gpGlobals->curtime + 5.0;

	SetWeaponIdleTime( flIdleTime );
}

bool CASW_Weapon_Tesla_Gun::ShouldMarineMoveSlow()
{
	return m_fSlowTime > gpGlobals->curtime || IsReloading();
}

#ifdef CLIENT_DLL
void CASW_Weapon_Tesla_Gun::ClientThink()
{
	BaseClass::ClientThink();

	UpdateEffects();
}

const char* CASW_Weapon_Tesla_Gun::GetPartialReloadSound(int iPart)
{
	switch (iPart)
	{
		case 1: return "ASW_MiningLaser.ReloadB"; break;
		case 2: return "ASW_MiningLaser.ReloadC"; break;
		default: break;
	};
	return "ASW_MiningLaser.ReloadA";
}

void CASW_Weapon_Tesla_Gun::UpdateEffects()
{
	switch( m_FireState )
	{
		case ASW_TG_FIRE_OFF:
		{
			if ( m_pDischargeEffect )
			{
				m_pDischargeEffect->StopEmission();
				m_pDischargeEffect = NULL;
				DispatchParticleEffect( "mining_laser_exhaust", PATTACH_POINT_FOLLOW, this, "eject1" );
			}
			break;
		}
		case ASW_TG_FIRE_DISCHARGE:
		{
			if ( m_pDischargeEffect && m_pDischargeEffect->GetControlPointEntity( 1 ) != NULL )
			{
				// Still attach, detach us
				m_pDischargeEffect->StopEmission();
				m_pDischargeEffect = NULL;
			}

			if ( !m_pDischargeEffect )
			{
				m_pDischargeEffect = ParticleProp()->Create( "electric_weapon_shot_continuous_off", PATTACH_POINT_FOLLOW, "muzzle" ); 
			}

			m_pDischargeEffect->SetControlPoint( 1, m_vecShockPos );
			
			Vector vecPos = m_vecShockPos.Get();
			//FX_MicroExplosion( vecPos, Vector(0,0,1) );
			break;
		}
		case ASW_TG_FIRE_ATTACHED:
		{
			EHANDLE hTarget = m_hShockEntity.Get();

			if ( !hTarget )
			{
				break;
			}

			if ( m_pDischargeEffect )
			{
				if ( m_pDischargeEffect->GetControlPointEntity( 1 ) != m_hShockEntity.Get() )
				{
					m_pDischargeEffect->StopEmission();
					m_pDischargeEffect = NULL;
				}
			}

			if ( !m_pDischargeEffect )
			{				
				m_pDischargeEffect = ParticleProp()->Create( "electric_weapon_shot_continuous", PATTACH_POINT_FOLLOW, "muzzle" ); 
			}

			Assert( m_pDischargeEffect );
	
			if ( m_pDischargeEffect->GetControlPointEntity( 1 ) == NULL )
			{
				float flHeight = hTarget->BoundingRadius();
				Vector vOffset( 0.0f, 0.0f, flHeight * 0.25 );

				C_ASW_Alien *pAlien = dynamic_cast<C_ASW_Alien*>(hTarget.Get());
				// TODO: get some standardization in the attachment naming
				if ( pAlien && pAlien->LookupAttachment( "eyes" ) )
				{
					ParticleProp()->AddControlPoint( m_pDischargeEffect, 1, pAlien, PATTACH_POINT_FOLLOW, "eyes" );
				}
				else
				{
					ParticleProp()->AddControlPoint( m_pDischargeEffect, 1, hTarget, PATTACH_ABSORIGIN_FOLLOW, NULL, vOffset );
				}
			}

			break;
		}
	}
}
#endif

#ifdef GAME_DLL
void CASW_Weapon_Tesla_Gun::GetButtons(bool& bAttack1, bool& bAttack2, bool& bReload, bool& bOldReload, bool& bOldAttack1 )
{
	CASW_Marine *pMarine = GetMarine();

	// make AI fire this weapon whenever they have an enemy within a certain range
	if (pMarine && !pMarine->IsInhabited())
	{
		bool bHasEnemy = (pMarine->GetEnemy() && pMarine->GetEnemy()->GetAbsOrigin().DistTo(pMarine->GetAbsOrigin()) < 250.0f);

		if (bHasEnemy)
			m_fLastForcedFireTime = gpGlobals->curtime;

		bAttack1 = (bHasEnemy || gpGlobals->curtime < m_fLastForcedFireTime + 1.0f);	// fire for 2 seconds after killing our enemy
		bAttack2 = false;
		bReload = false;
		bOldReload = false;

		return;
	}

	BaseClass::GetButtons(bAttack1, bAttack2, bReload, bOldReload, bOldAttack1);
}
#endif

void CASW_Weapon_Tesla_Gun::SetFiringState(ASW_Weapon_TeslaGunFireState_t state)
{
#ifdef CLIENT_DLL
	//Msg("[C] SetFiringState %d\n", state);
#else
	//Msg("[C] SetFiringState %d\n", state);
#endif

	// Check for transitions
	if ( m_FireState != state )
	{	
		if ( state == ASW_TG_FIRE_DISCHARGE || state == ASW_TG_FIRE_ATTACHED )
		{
			EmitSound( "ASW_Tesla_Laser.Loop" );
		}
		else if ( state == ASW_TG_FIRE_OFF )
		{
			StopSound( "ASW_Tesla_Laser.Loop" );
			EmitSound( "ASW_Tesla_Laser.Stop" );

		}
	}

	m_FireState = state;
}

bool CASW_Weapon_Tesla_Gun::ShockAttach( CBaseEntity *pEntity )
{
#ifdef CLIENT_DLL
	return false;
#else
	if ( !pEntity || (pEntity->m_takedamage == DAMAGE_NO) || 
		(pEntity->Classify() == CLASS_ASW_MARINE) || (pEntity->Classify() == CLASS_ASW_SENTRY_BASE) ||
		(pEntity == m_hShockEntity.Get()) || (pEntity->m_iHealth <= 0) ||
		(pEntity == GetMarine()) ) 
	{
		return false;
	}

	if ( m_hShockEntity.Get() )
	{
		ShockDetach();
	}

	m_hShockEntity.Set( pEntity );

	// we need a trace_t struct representing the entity we're shocking, but if we 
	//  attached based on proximity to the end of the beam, we might not have actually performed a trace - for now just update m_AttackTrace
	//  but it'd be nice to decouple attribute damage from requiring a trace
	if ( m_AttackTrace.m_pEnt != pEntity )
	{
		m_AttackTrace.endpos = pEntity->GetAbsOrigin();
		m_AttackTrace.m_pEnt = pEntity;
	}

	ASW_WPN_DEV_MSG( "Attached to %d:%s\n", m_hShockEntity.Get()->entindex(), m_hShockEntity.Get()->GetClassname() );
	return true;
#endif
}

void CASW_Weapon_Tesla_Gun::ShockDetach()
{
#ifdef GAME_DLL
	ASW_WPN_DEV_MSG( "Detaching from %d:%s\n", m_hShockEntity.Get() ? m_hShockEntity.Get()->entindex() : -1, m_hShockEntity.Get() ? m_hShockEntity.Get()->GetClassname() : "(null)" );
	m_hShockEntity.Set( NULL );
#endif
}