#include "cbase.h"
#include "asw_weapon_railgun_shared.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#define CASW_Weapon C_ASW_Weapon
#define CASW_Marine C_ASW_Marine
#define CBasePlayer C_BasePlayer
#include "c_asw_player.h"
#include "c_asw_weapon.h"
#include "c_asw_marine.h"
#include "c_asw_railgun_beam.h"
#include "iviewrender_beams.h"
#include "c_asw_marine_resource.h"
#include "precache_register.h"
#include "c_te_effect_dispatch.h"
#else
#include "asw_lag_compensation.h"
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_weapon.h"
#include "npcevent.h"
#include "shot_manipulator.h"
#include "te_effect_dispatch.h"
#include "asw_marine_resource.h"
#include "asw_marine_speech.h"
#include "decals.h"
#include "ammodef.h"
#include "asw_weapon_ammo_bag_shared.h"
#include "asw_rocket.h"
#endif
#include "asw_marine_skills.h"
#include "asw_weapon_parse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//#define ASW_RAILGUN_SWEEP_STYLE 1			// makes railgun do hull sweep style damage
//#define ASW_RAILGUN_WIDE_SWEEP  1			// makes railgun do a wide low damage sweep in addition to the above

#define ASW_RAILGUN_BURST_COUNT 3				// how many bullets are fired in each burst
#define ASW_RAILGUN_BURST_INTERVAL 0.12f			// time between each bullet in a burst

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Railgun, DT_ASW_Weapon_Railgun )

BEGIN_NETWORK_TABLE( CASW_Weapon_Railgun, DT_ASW_Weapon_Railgun )
#ifdef CLIENT_DLL
	// recvprops
	RecvPropTime( RECVINFO( m_fSlowTime ) ),
	RecvPropInt(RECVINFO(m_iRailBurst)),	
#else
	// sendprops
	SendPropTime( SENDINFO( m_fSlowTime ) ),
	SendPropInt( SENDINFO(m_iRailBurst), Q_log2( ASW_RAILGUN_BURST_COUNT ) + 1, SPROP_UNSIGNED ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CASW_Weapon_Railgun )
	DEFINE_PRED_FIELD_TOL( m_fSlowTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),
	DEFINE_PRED_FIELD( m_iRailBurst, FIELD_INTEGER, FTYPEDESC_INSENDTABLE )		
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( asw_weapon_railgun, CASW_Weapon_Railgun );
PRECACHE_WEAPON_REGISTER(asw_weapon_railgun);

extern ConVar asw_weapon_max_shooting_distance;
ConVar asw_railgun_force_scale("asw_railgun_force_scale", "60.0f", FCVAR_REPLICATED, "Force of railgun shots");

#ifndef CLIENT_DLL
extern ConVar asw_debug_marine_damage;

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Weapon_Railgun )
	
END_DATADESC()

#else

//Precache the effects
PRECACHE_REGISTER_BEGIN( GLOBAL, ASWPrecacheEffectRailgun )
PRECACHE( MATERIAL, "swarm/effects/railgun" )
PRECACHE_REGISTER_END()

#endif /* not client */

CASW_Weapon_Railgun::CASW_Weapon_Railgun()
{
	m_fSlowTime = 0;
	m_iRailBurst = 0;
}


CASW_Weapon_Railgun::~CASW_Weapon_Railgun()
{

}

void CASW_Weapon_Railgun::Precache()
{	
	PrecacheModel("swarm/effects/railgun.vmt");
	PrecacheScriptSound("ASW_Railgun.Single");

	BaseClass::Precache();
}

#define	MAX_GLASS_PENETRATION_DEPTH	16.0f
extern short	g_sModelIndexFireball;			// (in combatweapon.cpp) holds the index for the smoke cloud
void CASW_Weapon_Railgun::PrimaryAttack()
{
	// If my clip is empty (and I use clips) start reload
	if ( UsesClipsForAmmo1() && !m_iClip1 ) 
	{
		Reload();
		return;
	}

	CASW_Player *pPlayer = GetCommander();
	CASW_Marine *pMarine = GetMarine();

	if (!pMarine)		// firing from a marine
		return;

#ifndef CLIENT_DLL
	//Msg("[S] Railgun PrimaryAttack time=%f\n", gpGlobals->curtime);
#else
	//Msg("[C] Railgun PrimaryAttack time=%f\n", gpGlobals->curtime);
#endif

	m_iRailBurst = 1;	// disable burst firing..
	//if (m_iRailBurst <= 0)
		//m_iRailBurst = ASW_RAILGUN_BURST_COUNT;
	
	// MUST call sound before removing a round from the clip of a CMachineGun
	WeaponSound(SINGLE);

	m_bIsFiring = true;

	// tell the marine to tell its weapon to draw the muzzle flash
	pMarine->DoMuzzleFlash();

	// sets the animation on the weapon model iteself
	SendWeaponAnim( GetPrimaryAttackActivity() );

	// sets the animation on the marine holding this weapon
	//pMarine->SetAnimation( PLAYER_ATTACK1 );

#ifdef GAME_DLL	// check for turning on lag compensation
	if (pPlayer && pMarine->IsInhabited())
	{
		CASW_Lag_Compensation::RequestLagCompensation( pPlayer, pPlayer->GetCurrentUserCommand() );
	}
#endif

	FireBulletsInfo_t info;
	info.m_vecSrc = pMarine->Weapon_ShootPosition( );
	if ( pPlayer && pMarine->IsInhabited() )
	{
		info.m_vecDirShooting = pPlayer->GetAutoaimVectorForMarine(pMarine, GetAutoAimAmount(), GetVerticalAdjustOnlyAutoAimAmount());	// 45 degrees = 0.707106781187
	}
	else
	{
#ifdef CLIENT_DLL
		Msg("Error, clientside firing of a weapon that's being controlled by an AI marine\n");
#else
		info.m_vecDirShooting = pMarine->GetActualShootTrajectory( info.m_vecSrc );
#endif
	}

	// To make the firing framerate independent, we may have to fire more than one bullet here on low-framerate systems, 
	// especially if the weapon we're firing has a really fast rate of fire.
	info.m_iShots = 0;
	float fireRate = GetFireRate();
	if (m_iRailBurst > 1)
		fireRate = ASW_RAILGUN_BURST_INTERVAL;
	//float current_attack_time = m_flNextPrimaryAttack;
	//float attack_delta = gpGlobals->curtime - current_attack_time;
	while ( m_flNextPrimaryAttack <= gpGlobals->curtime )
	{
		if (m_iRailBurst == 0)	// this loop could fire all our burst rounds in one go, se chcek for resetting it up
			m_iRailBurst = ASW_RAILGUN_BURST_COUNT;
		m_flNextPrimaryAttack = m_flNextPrimaryAttack + fireRate;
#ifndef CLIENT_DLL
	//Msg("[S] Set m_flNextPrimaryAttack to %f Firerate=%f curtime=%f\n", m_flNextPrimaryAttack.Get(), fireRate, gpGlobals->curtime);
#else
	//Msg("[C] Set m_flNextPrimaryAttack to %f Firerate=%f curtime=%f\n", m_flNextPrimaryAttack.Get(), fireRate, gpGlobals->curtime);
#endif
		info.m_iShots++;
		m_iRailBurst--;
		// recalc the fire rate as our burst var can change it
		fireRate = GetFireRate();
		if (m_iRailBurst > 1)
			fireRate = ASW_RAILGUN_BURST_INTERVAL;
		if ( !fireRate )
			break;
	}

	// Make sure we don't fire more than the amount in the clip
	if ( UsesClipsForAmmo1() )
	{
		info.m_iShots = MIN( info.m_iShots, m_iClip1 );
		m_iClip1 -= info.m_iShots;
#ifdef GAME_DLL
			CASW_Marine *pMarine = GetMarine();
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
	}
	else
	{
		info.m_iShots = MIN( info.m_iShots, pMarine->GetAmmoCount( m_iPrimaryAmmoType ) );
		pMarine->RemoveAmmo( info.m_iShots, m_iPrimaryAmmoType );
	}

	info.m_flDistance = asw_weapon_max_shooting_distance.GetFloat();
	info.m_iAmmoType = m_iPrimaryAmmoType;
	info.m_iTracerFreq = 1;
	info.m_flDamage = GetWeaponDamage();
#ifndef CLIENT_DLL
	if (asw_debug_marine_damage.GetBool())
		Msg("Weapon dmg = %f\n", info.m_flDamage);
	info.m_flDamage *= pMarine->GetMarineResource()->OnFired_GetDamageScale();
#endif

	info.m_vecSpread = pMarine->GetActiveWeapon()->GetBulletSpread();
	info.m_flDamageForceScale = asw_railgun_force_scale.GetFloat();

	//float max_dist = asw_weapon_max_shooting_distance.GetFloat();
	//inline void UTIL_TraceLine( const Vector& vecAbsStart, const Vector& vecAbsEnd, unsigned int mask, 
					 //const IHandleEntity *ignore, int collisionGroup, trace_t *ptr )

	// find impact point of the RG shot
#ifdef ASW_RAILGUN_SWEEP_STYLE
	trace_t tr;
	UTIL_TraceLine(info.m_vecSrc, info.m_vecSrc + info.m_vecDirShooting * max_dist, MASK_SOLID_BRUSHONLY, pMarine, COLLISION_GROUP_NONE, &tr);
	if (!tr.startsolid)
	{
#ifndef CLIENT_DLL
		bool bHitGlass = false;
		if ( tr.m_pEnt != NULL )
		{
			Msg("trace hit something\n");
			surfacedata_t *psurf = physprops->GetSurfaceData( tr.surface.surfaceProps );
			if (psurf != NULL)
			{
				Msg("psurf isn't null. mat = %d glass = %d class = %s match = %d\n", psurf->game.material, CHAR_TEX_GLASS, tr.m_pEnt->GetClassname(), tr.m_pEnt->ClassMatches( "CBreakableSurface" ));
			}
			if ( ( psurf != NULL ) && ( psurf->game.material == CHAR_TEX_GLASS ) && ( tr.m_pEnt->ClassMatches( "CBreakableSurface" ) ) )
			{
				Msg("so this is a glass hit\n");
				bHitGlass = true;
			}
		}
		int glassCount = 10;
		while (bHitGlass && glassCount > 0)
		{
			Msg("hit glass\n");
			bHitGlass = false;
			glassCount--;

			// Move through the glass until we're at the other side
			Vector	testPos = tr.endpos + ( info.m_vecDirShooting * MAX_GLASS_PENETRATION_DEPTH );

			CEffectData	data;
			data.m_vNormal = tr.plane.normal;
			data.m_vOrigin = tr.endpos;
			DispatchEffect( "GlassImpact", data );

			trace_t	penetrationTrace;			
			UTIL_TraceLine(testPos, tr.endpos, MASK_SHOT, pMarine, COLLISION_GROUP_NONE, &penetrationTrace);

			// See if we found the surface again
			if ( !penetrationTrace.startsolid && tr.fraction != 0.0f && penetrationTrace.fraction != 1.0f )
			{
				// Impact the other side (will look like an exit effect)
				DoImpactEffect( penetrationTrace, GetAmmoDef()->DamageType(info.m_iAmmoType) );
				data.m_vNormal = penetrationTrace.plane.normal;
				data.m_vOrigin = penetrationTrace.endpos;				
				DispatchEffect( "GlassImpact", data );

				// continue trace past the glass
				UTIL_TraceLine(penetrationTrace.endpos, penetrationTrace.endpos + info.m_vecDirShooting * max_dist, MASK_SOLID_BRUSHONLY, pMarine, COLLISION_GROUP_NONE, &tr);

				if ( tr.m_pEnt != NULL )
				{
					surfacedata_t *psurf = physprops->GetSurfaceData( tr.surface.surfaceProps );
					if ( ( psurf != NULL ) && ( psurf->game.material == CHAR_TEX_GLASS ) && ( tr.m_pEnt->ClassMatches( "func_breakable" ) ) )
					{
						bHitGlass = true;
					}
				}
			}									
		}

		Vector vecEndPos = tr.endpos;
		trace_t ptr;
		//CTakeDamageInfo( CBaseEntity *pInflictor, CBaseEntity *pAttacker, float flDamage, int bitsDamageType, int iKillType = 0 );
		//CTakeDamageInfo( CBaseEntity *pInflictor, CBaseEntity *pAttacker, const Vector &damageForce, const Vector &damagePosition, float flDamage, int bitsDamageType, int iKillType = 0, Vector *reportedPosition = NULL );
		
		// do close high damage FF damaging trace
		CTakeDamageInfo dmg(this, pMarine, Vector(0,0,0), GetAbsOrigin(), GetWeaponDamage(), DMG_SONIC);	//  | DMG_ALWAYSGIB
		CTraceFilterRG traceFilter( pMarine, COLLISION_GROUP_NONE, &dmg, true );
		Vector vecMin(-3, -3, -3);
		Vector vecMax(3, 3, 3);
		UTIL_TraceHull( info.m_vecSrc, vecEndPos, vecMin, vecMax,
			CONTENTS_MONSTER, &traceFilter, &ptr );

		// do wide less damaging sweep
#ifdef ASW_RAILGUN_WIDE_SWEEP
		CTakeDamageInfo wdmg(this, pMarine, Vector(0,0,0), GetAbsOrigin(), GetWeaponDamage() * 0.33f, DMG_SONIC);	//  | DMG_ALWAYSGIB
		CTraceFilterRG wtraceFilter( pMarine, COLLISION_GROUP_NONE, &wdmg, false );
		Vector vecWMin(-15, -15, -15);
		Vector vecWMax(15, 15, 15);
		UTIL_TraceHull( info.m_vecSrc, vecEndPos, vecWMin, vecWMax,
			CONTENTS_MONSTER, &wtraceFilter, &ptr );
#endif	// ASW_RAILGUN_WIDE_SWEEP

#endif	// CLIENT_DLL
		//CreateRailgunBeam(GetAbsOrigin(), tr.endpos);
	}
#else
	//trace_t tr;
	//UTIL_TraceLine(info.m_vecSrc, info.m_vecSrc + info.m_vecDirShooting * max_dist, MASK_SHOT, pMarine, COLLISION_GROUP_NONE, &tr);
	//Vector vecExplosionPos = tr.endpos;
	//CPASFilter filter( vecExplosionPos );

	/*te->Explosion( filter, 0.1,		// IRecipientFilter& filter, float delay
		&vecExplosionPos,			// pos
		g_sModelIndexFireball,		// modelindex
		0.1,						// scale
		1,							// framerate
		TE_EXPLFLAG_NONE,			// flags
		0.5,							// radius
		0.5 );						// magnitude
		*/
	pMarine->FirePenetratingBullets( info, 10, 1.0f, 0, true, NULL, false );
	//pMarine->FireBullets( info );
#endif	// ASW_RAILGUN_SWEEP_STYLE
	

	// increment shooting stats
#ifndef CLIENT_DLL
	if (pMarine && pMarine->GetMarineResource())
	{
		pMarine->GetMarineResource()->UsedWeapon(this, info.m_iShots);
		pMarine->OnWeaponFired( this, info.m_iShots );
	}
#endif

	if (!m_iClip1 && pMarine->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
	{
		// HEV suit - indicate out of ammo condition
		if (pPlayer)
			pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0); 
	}
	m_fSlowTime = gpGlobals->curtime + 0.1f;
}



// creates a translucent beam effect coming out from the gun
void CASW_Weapon_Railgun::CreateRailgunBeam( const Vector &vecStartPoint, const Vector &vecEndPoint )
{
	//Vector vecEndPoint = info.m_vecSrc + info.m_vecDirShooting * 1000;
	//Vector vecStartPoint = GetAbsOrigin();

	CASW_Marine* pOwner = GetMarine();
	
	if (!pOwner)
		return;

	CEffectData data;
	data.m_vStart = vecStartPoint;
	data.m_vOrigin = vecEndPoint;
	data.m_fFlags |= TRACER_FLAG_USEATTACHMENT;
	data.m_nAttachmentIndex = LookupAttachment("muzzle");
#ifdef CLIENT_DLL
	data.m_hEntity = this;
#else
	data.m_nEntIndex = entindex();
#endif

	CPASFilter filter( data.m_vOrigin );
#ifndef CLIENT_DLL
	if (gpGlobals->maxClients > 1 && pOwner->IsInhabited() && pOwner->GetCommander())
	{
		// multiplayer game, where this marine is currently being controlled by a client, who will spawn his own effect
		// so just make the beam effect for all other clients		
		filter.RemoveRecipient(pOwner->GetCommander());
	}
#endif
	
	DispatchEffect(filter, 0.0, "RailgunBeam", data );

	return;
/*
#ifdef CLIENT_DLL	
	QAngle	muzzleAngles;
	GetAttachment( LookupAttachment("muzzle"), vecStartPoint, muzzleAngles );	

	// create a railgun beam (it'll fade out and kill itself after a short period)
	C_ASW_Railgun_Beam *pBeam = new C_ASW_Railgun_Beam();
	if (pBeam)
	{
		if (pBeam->InitializeAsClientEntity( NULL, false ))
		{
			pBeam->InitBeam(vecStartPoint, vecEndPoint);
		}
		else
		{
			pBeam->Release();
		}
	}*/

	/*
	// Draw a beam
	BeamInfo_t beamInfo;

	beamInfo.m_pStartEnt = this;
	beamInfo.m_nStartAttachment = 1;
	beamInfo.m_pEndEnt = NULL;
	beamInfo.m_nEndAttachment = -1;
	beamInfo.m_vecStart = vec3_origin;
	beamInfo.m_vecEnd = vecEndPoint;
	beamInfo.m_pszModelName = "swarm/effects/railgun.vmt";
	beamInfo.m_flHaloScale = 0.0f;
	beamInfo.m_flLife = 0.1f;
	beamInfo.m_flWidth = 12.0f;
	beamInfo.m_flEndWidth = 4.0f;
	beamInfo.m_flFadeLength = 0.0f;
	beamInfo.m_flAmplitude = 0;
	beamInfo.m_flBrightness = 255.0;
	beamInfo.m_flSpeed = 0.0f;
	beamInfo.m_nStartFrame = 0.0;
	beamInfo.m_flFrameRate = 0.0;
	beamInfo.m_flRed = 255.0;
	beamInfo.m_flGreen = 255.0;
	beamInfo.m_flBlue = 255.0;
	beamInfo.m_nSegments = 16;
	beamInfo.m_bRenderable = true;
	beamInfo.m_nFlags = FBEAM_ONLYNOISEONCE | FBEAM_SOLID;

	beams->CreateBeamEntPoint( beamInfo );
	*/

	/*
#else
	CASW_Marine* pOwner = GetMarine();
	
	if (!pOwner)
		return;

	CEffectData data;
	data.m_vStart = vecStartPoint;
	data.m_vOrigin = vecEndPoint;
	data.m_fFlags |= TRACER_FLAG_USEATTACHMENT;
	data.m_nAttachmentIndex = LookupAttachment("muzzle");
	data.m_nEntIndex = entindex();

	CPASFilter filter( data.m_vOrigin );
	if (gpGlobals->maxClients > 1 && pOwner->IsInhabited() && pOwner->GetCommander())
	{
		// multiplayer game, where this marine is currently being controlled by a client, who will spawn his own effect
		// so just make the beam effect for all other clients		
		filter.RemoveRecipient(pOwner->GetCommander());
	}
	
	te->DispatchEffect( filter, 0.0, data.m_vOrigin, "RailgunBeam", data );
#endif*/
}

bool CASW_Weapon_Railgun::ShouldMarineMoveSlow()
{
	return BaseClass::ShouldMarineMoveSlow();
	//return m_fSlowTime > gpGlobals->curtime;
}

float CASW_Weapon_Railgun::GetMovementScale()
{
	return ShouldMarineMoveSlow() ? 0.75f : 1.0f;
}

bool CASW_Weapon_Railgun::Reload()
{
	m_iRailBurst = 0;
	bool bReloaded = ASWReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD );

	// skip reload chatter on the railgun, since it reloads after every shot
	/*
	if (bReloaded)
	{
#ifdef GAME_DLL
		CASW_Marine *pMarine = GetMarine();
		if (pMarine)
		{
			if (pMarine->IsAlienNear())
			{
				//Msg("potentially reload chattering as there is an alien near\n");
				pMarine->GetMarineSpeech()->Chatter(CHATTER_RELOADING);
			}
			else
			{
				//Msg("Not reload chattering as there's no aliens near\n");
			}
		}
#endif
	}
	*/
	return bReloaded;
}

bool CASW_Weapon_Railgun::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	m_iRailBurst = 0;
	return BaseClass::Holster( pSwitchingTo );	
}

void CASW_Weapon_Railgun::GetButtons(bool& bAttack1, bool& bAttack2, bool& bReload, bool& bOldReload, bool& bOldAttack1 )
{	
	BaseClass::GetButtons(bAttack1, bAttack2, bReload, bOldReload, bOldAttack1 );
	// make sure the fire button stays held down until our burst is clear
	CASW_Marine *pMarine = GetMarine();
	if (m_iRailBurst > 0 && !(pMarine && pMarine->GetFlags() & FL_FROZEN))
		bAttack1 = true;
}

bool CASW_Weapon_Railgun::SupportsBayonet()
{
	return true;
}

float CASW_Weapon_Railgun::GetWeaponDamage()
{
	//float flDamage = 35.0f;
	float flDamage = GetWeaponInfo()->m_flBaseDamage;

	if ( GetMarine() )
	{
		flDamage += MarineSkills()->GetSkillBasedValueByMarine(GetMarine(), ASW_MARINE_SKILL_ACCURACY, ASW_MARINE_SUBSKILL_ACCURACY_RAILGUN_DMG);
	}

	//CALL_ATTRIB_HOOK_FLOAT( flDamage, mod_damage_done );

	return flDamage;
}

int CASW_Weapon_Railgun::ASW_SelectWeaponActivity(int idealActivity)
{
	switch( idealActivity )
	{		
		case ACT_WALK:			idealActivity = ACT_WALK_AIM_RIFLE; break;
		case ACT_RUN:			idealActivity = ACT_RUN_AIM_RIFLE; break;
		case ACT_IDLE:			idealActivity = ACT_IDLE_RIFLE; break;
		case ACT_RELOAD:		idealActivity = ACT_RELOAD; break;		
		default: break;
	}
	return idealActivity;
}

#ifndef CLIENT_DLL
bool CTraceFilterRG::ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
{
	if ( !StandardFilterRules( pHandleEntity, contentsMask ) )
		return false;

	if ( !PassServerEntityFilter( pHandleEntity, m_pPassEnt ) )
		return false;

	// Don't test if the game code tells us we should ignore this collision...
	CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
	
	if ( pEntity )
	{
		if ( !pEntity->ShouldCollide( m_collisionGroup, contentsMask ) )
			return false;
		
		if ( !g_pGameRules->ShouldCollide( m_collisionGroup, pEntity->GetCollisionGroup() ) )
			return false;

		if ( pEntity->m_takedamage == DAMAGE_NO )
			return false;

		Vector	attackDir = pEntity->WorldSpaceCenter() - m_dmgInfo->GetAttacker()->WorldSpaceCenter();
		VectorNormalize( attackDir );

		CTakeDamageInfo info = (*m_dmgInfo);				
		CalculateMeleeDamageForce( &info, attackDir, info.GetAttacker()->WorldSpaceCenter(), 1.0f );

		CBaseCombatCharacter *pBCC = info.GetAttacker()->MyCombatCharacterPointer();
		CBaseCombatCharacter *pVictimBCC = pEntity->MyCombatCharacterPointer();

		// Only do these comparisons between NPCs
		if ( pBCC && pVictimBCC )
		{
			// Can only damage other NPCs that we hate
			if ( m_bFFDamage || pBCC->IRelationType( pEntity ) == D_HT )
			{
				if ( info.GetDamage() )
				{
					pEntity->TakeDamage( info );
				}
				
				// Put a combat sound in
				CSoundEnt::InsertSound( SOUND_COMBAT, info.GetDamagePosition(), 200, 0.2f, info.GetAttacker() );

				m_pHit = pEntity;
				return true;
			}
		}
		else
		{
			// Make sure if the player is holding this, he drops it
			Pickup_ForcePlayerToDropThisObject( pEntity );

			// Otherwise just damage passive objects in our way
			if ( info.GetDamage() )
			{
				pEntity->TakeDamage( info );
			}
		}
	}

	return false;
}
#endif


void CASW_Weapon_Railgun::SecondaryAttack()
{
	// Only the player fires this way so we can cast
	CASW_Player *pPlayer = GetCommander();

	if (!pPlayer)
		return;

	CASW_Marine *pMarine = GetMarine();

	if (!pMarine)
		return;

	// dry fire - no secondary enabled on RG (below rocket stuff is an experiment)
	SendWeaponAnim( ACT_VM_DRYFIRE );
	BaseClass::WeaponSound( EMPTY );
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f;
	return;

	//Must have ammo - temp comment	
	//bool bUsesSecondary = UsesSecondaryAmmo();
	bool bUsesClips = UsesClipsForAmmo2();
	//int iAmmoCount = pMarine->GetAmmoCount(m_iSecondaryAmmoType);
	//bool bInWater = ( pMarine->GetWaterLevel() == 3 );
	/*
	if ( (bUsesSecondary &&  
			(   ( bUsesClips && m_iClip2 <= 0) ||
			    ( !bUsesClips && iAmmoCount<=0 )
				) )
				 || bInWater || m_bInReload )

	{
		SendWeaponAnim( ACT_VM_DRYFIRE );
		BaseClass::WeaponSound( EMPTY );
		m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f;
		return;
	}
	*/


	// MUST call sound before removing a round from the clip of a CMachineGun
	BaseClass::WeaponSound( SPECIAL1 );

	Vector vecSrc = pMarine->Weapon_ShootPosition();
	Vector	vecThrow;
	// Don't autoaim on grenade tosses
	vecThrow = pPlayer->GetAutoaimVectorForMarine(pMarine, GetAutoAimAmount(), GetVerticalAdjustOnlyAutoAimAmount());	// 45 degrees = 0.707106781187
	QAngle angGrenFacing;
	VectorAngles(vecThrow, angGrenFacing);
	VectorScale( vecThrow, 1000.0f, vecThrow );
	
#ifndef CLIENT_DLL
	//Create the grenade
	/*
	float fGrenadeDamage = MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_GRENADES, ASW_MARINE_SUBSKILL_GRENADE_DMG);
	float fGrenadeRadius = MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_GRENADES, ASW_MARINE_SUBSKILL_GRENADE_RADIUS);
	if (asw_debug_marine_damage.GetBool())
	{
		Msg("Grenade damage = %f radius = %f\n", fGrenadeDamage, fGrenadeRadius);
	}
	CASW_Grenade_PRifle::PRifle_Grenade_Create( 
		fGrenadeDamage,
		fGrenadeRadius,
		vecSrc, angGrenFacing, vecThrow, AngularImpulse(0,0,0), pMarine );
	*/
	const int num_rockets = 5;
	const int fan_spread = 90;
	for (int i=0;i<num_rockets;i++)
	{
		int angle = (-fan_spread * 0.5f) + float(fan_spread) * (float(i) / float(num_rockets));
		QAngle vecRocketAngle = angGrenFacing;
		vecRocketAngle[YAW] += angle;
		//CASW_Rocket::Create(vecSrc, vecRocketAngle, GetMarine());
	}
#endif

	SendWeaponAnim( ACT_VM_SECONDARYATTACK );
	if ( bUsesClips )
	{
		m_iClip2 -= 1;
	}
	else
	{
		pMarine->RemoveAmmo( 1, m_iSecondaryAmmoType );
	}

	// Can shoot again immediately
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;

	// Can blow up after a short delay (so have time to release mouse button)
	m_flNextSecondaryAttack = gpGlobals->curtime + 1.0f;
}

float CASW_Weapon_Railgun::GetFireRate()
{
	//float flRate = 0.1f;
	float flRate = GetWeaponInfo()->m_flFireRate;

	//CALL_ATTRIB_HOOK_FLOAT( flRate, mod_fire_rate );

	return flRate;
}
