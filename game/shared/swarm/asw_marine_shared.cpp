#include "cbase.h"

#ifdef CLIENT_DLL
	#include "c_asw_marine.h"
	#include "c_asw_weapon.h"
	#include "c_asw_player.h"
	#include "c_asw_marine_resource.h"	
	#include "iasw_client_vehicle.h"
	#include "c_te_effect_dispatch.h"
	#include "c_asw_simple_alien.h"
	#include "c_asw_fx.h"
	#include "c_asw_pickup_weapon.h"
	#include "c_asw_shieldbug.h"
	#include "c_asw_hack.h"
	#define CASW_Simple_Alien C_ASW_Simple_Alien
	#define CASW_Pickup_Weapon C_ASW_Pickup_Weapon
	#define CAI_BaseNPC C_AI_BaseNPC
#else
	#include "te_effect_dispatch.h"
	#include "asw_marine.h"
	#include "asw_weapon.h"
	#include "asw_player.h"
	#include "asw_marine_resource.h"
	#include "iasw_vehicle.h"
	#include "iservervehicle.h"
	#include "asw_simple_alien.h"
	#include "asw_pickup_weapon.h"
	#include "asw_alien.h"
	#include "asw_hack.h"
#endif
#include "game_timescale_shared.h"
#include "asw_marine_gamemovement.h"
#include "asw_trace_filter_melee.h"
#include "asw_gamerules.h"
#include "ai_debug_shared.h"
#include "takedamageinfo.h"
#include "decals.h"
#include "shot_manipulator.h"
#include "effect_dispatch_data.h"
#include "asw_marine_profile.h"
#include "asw_remote_turret_shared.h"
#include "obstacle_pushaway.h"
#include "bone_setup.h"
#include "ammodef.h" 
#include "asw_equipment_list.h"
#include "asw_weapon_parse.h"
#include "fmtstr.h"
#include "collisionutils.h"
#include "coordsize.h"
#include "asw_melee_system.h"
#include "eventlist.h"
#include "particle_parse.h"
#include "asw_trace_filter_shot.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// the amount of damage required to make an alien always gib
#define ASW_GIB_DAMAGE_LIMIT 200

extern ConVar	ai_debug_shoot_positions;
extern ConVar asw_melee_debug;
extern ConVar asw_debug_marine_damage;
extern ConVar asw_stim_time_scale;
extern ConVar asw_marine_ff;
ConVar asw_leadership_radius("asw_leadership_radius", "600", FCVAR_REPLICATED, "Radius of the leadership field around NCOs with the leadership skill");
ConVar asw_marine_speed_scale_easy("asw_marine_speed_scale_easy", "0.96", FCVAR_REPLICATED);
ConVar asw_marine_speed_scale_normal("asw_marine_speed_scale_normal", "1.0", FCVAR_REPLICATED);
ConVar asw_marine_speed_scale_hard("asw_marine_speed_scale_hard", "1.0", FCVAR_REPLICATED);
ConVar asw_marine_speed_scale_insane("asw_marine_speed_scale_insane", "1.0", FCVAR_REPLICATED);
ConVar asw_marine_box_collision("asw_marine_box_collision", "1", FCVAR_REPLICATED);
ConVar asw_allow_hull_shots("asw_allow_hull_shots", "1", FCVAR_REPLICATED);
#ifdef GAME_DLL
extern ConVar ai_show_hull_attacks;
ConVar asw_melee_knockback_up_force( "asw_melee_knockback_up_force", "1.0", FCVAR_CHEAT );
#endif

static const float ASW_MARINE_MELEE_HULL_TRACE_Z = 32.0f;

bool CASW_Marine::Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex /*=0*/ ) 
{
	if ( GetHealth() <= 0 )
		return false;

	// check we're actually carrying this weapon in one of our 3 marine slots
	if (GetASWWeapon(0)!= pWeapon && GetASWWeapon(1) != pWeapon && GetASWWeapon(2) != pWeapon)
		return false;

	if (BaseClass::Weapon_Switch( pWeapon, viewmodelindex ))
	{
		if (pWeapon != GetLastWeaponSwitchedTo() && ASWGameRules() && ASWGameRules()->GetGameState() >= ASW_GS_INGAME )
		{
			m_hLastWeaponSwitchedTo = pWeapon;
			DoAnimationEvent( PLAYERANIMEVENT_WEAPON_SWITCH );
		}

		CASW_Weapon* pASWWeapon = dynamic_cast<CASW_Weapon*>(pWeapon);
		if (pASWWeapon)
		{
			pASWWeapon->ApplyWeaponSwitchTime();
		}

#ifndef CLIENT_DLL
		CheckAndRequestAmmo();
#endif

		return true;
	}
	
	return false;
}

// allow switching to weapons with no ammo
bool CASW_Marine::Weapon_CanSwitchTo( CBaseCombatWeapon *pWeapon )
{
	//if ( !pWeapon->HasAnyAmmo() && !GetAmmoCount( pWeapon->m_iPrimaryAmmoType ) )
		//return false;
	CASW_Weapon* pASWWeapon = dynamic_cast<CASW_Weapon*>(pWeapon);
	if (pASWWeapon && !pASWWeapon->ASWCanBeSelected())
		return false;

	// disallow selection of offhand item
	if ( pASWWeapon->GetWeaponInfo() && pASWWeapon->GetWeaponInfo()->m_bExtra )
		return false;

	if ( !pWeapon->CanDeploy() )
		return false;
	
	if ( GetActiveWeapon() )
	{
		if ( !GetActiveWeapon()->CanHolster() )
			return false;
	}

	return true;
}

const QAngle& CASW_Marine::ASWEyeAngles( void )
{
	if ( GetCommander() && IsInhabited() )
		return GetCommander()->EyeAngles();

#ifdef CLIENT_DLL
	m_AIEyeAngles = EyeAngles();
	m_AIEyeAngles[PITCH] = m_fAIPitch;
	return m_AIEyeAngles;
#endif

	return EyeAngles();
}

CASW_Weapon* CASW_Marine::GetASWWeapon(int index) const
{
	CASW_Weapon* pWeapon = assert_cast<CASW_Weapon*>(GetWeapon(index));
	return pWeapon;
}

bool CASW_Marine::IsInfested()
{
	if (!GetMarineResource())
		return false;

	return GetMarineResource()->IsInfested();
}

// forces marine to look towards a certain point
void CASW_Marine::SetFacingPoint(const Vector &vec, float fDuration)
{
#ifdef CLIENT_DLL
	m_vecFacingPoint = vec;
#else
	m_vecFacingPointFromServer = vec;
#endif
	m_fStopFacingPointTime = gpGlobals->curtime + fDuration;
}

// tick emotes, if server sets any emote bool to true, it causes the emote timer
//   to go to 2.0 on all machines and tick down to zero (whereupon it gets set to false again)
void CASW_Marine::TickEmotes(float d)
{
	bEmoteMedic = TickEmote(d, bEmoteMedic, bClientEmoteMedic, fEmoteMedicTime);
	bEmoteAmmo = TickEmote(d, bEmoteAmmo, bClientEmoteAmmo, fEmoteAmmoTime);
	bEmoteSmile = TickEmote(d, bEmoteSmile, bClientEmoteSmile, fEmoteSmileTime);
	bEmoteStop = TickEmote(d, bEmoteStop, bClientEmoteStop, fEmoteStopTime);
	bEmoteGo = TickEmote(d, bEmoteGo, bClientEmoteGo, fEmoteGoTime);
	bEmoteExclaim = TickEmote(d, bEmoteExclaim, bClientEmoteExclaim, fEmoteExclaimTime);
	bEmoteAnimeSmile = TickEmote(d, bEmoteAnimeSmile, bClientEmoteAnimeSmile, fEmoteAnimeSmileTime);
	bEmoteQuestion = TickEmote(d, bEmoteQuestion, bClientEmoteQuestion, fEmoteQuestionTime);	
}

bool CASW_Marine::TickEmote(float d, bool bEmote, bool& bClientEmote, float& fEmoteTime)
{
	if (bEmote != bClientEmote)
	{
		bClientEmote = bEmote;
		if (bEmote)	// started an emote
			fEmoteTime = 2.0;
	}

	if (bEmote)		// if we're doing an emote, tick down the emote timer
	{
		fEmoteTime -= d * 2;
		if (fEmoteTime <= 0)
			bEmote = false;	
	}
	return bEmote;
}

// asw fixme to be + eye height (crouch/no)
Vector CASW_Marine::EyePosition( void ) 
{
	// if we're driving, return the position of our vehicle
	if (IsInVehicle())
	{
#ifdef CLIENT_DLL
		if (GetClientsideVehicle() && GetClientsideVehicle()->GetEntity())
			return GetClientsideVehicle()->GetEntity()->GetAbsOrigin();
#endif
		if (GetASWVehicle() && GetASWVehicle()->GetEntity())
			return GetASWVehicle()->GetEntity()->GetAbsOrigin();
	}
	//if (IsControllingTurret())
	//{
		//return GetRemoteTurret()->GetTurretCamPosition();
	//}
#ifdef CLIENT_DLL
	//if (m_bUseLastRenderedEyePosition)
		//return m_vecLastRenderedPos + GetViewOffset();
	return GetRenderOrigin() + GetViewOffset();
#else
	return GetAbsOrigin() + GetViewOffset();
#endif
}

void CASW_Marine::AvoidPhysicsProps( CUserCmd *pCmd )
{
#ifndef _XBOX
	// Don't avoid if noclipping or in movetype none
	switch ( GetMoveType() )
	{
	case MOVETYPE_NOCLIP:
	case MOVETYPE_NONE:
	case MOVETYPE_OBSERVER:
		return;
	default:
		break;
	}

	AvoidPushawayProps( this, pCmd );
#endif
}

Vector CASW_Marine::Weapon_ShootPosition( )
{
	Vector forward, right, up, v;

	v = GetAbsOrigin();

	if (IsInVehicle() && GetASWVehicle() && GetASWVehicle()->GetEntity())
	{
		v = GetASWVehicle()->GetEntity()->GetAbsOrigin();
#ifdef CLIENT_DLL
		if (gpGlobals->maxClients>1 && GetClientsideVehicle() && GetClientsideVehicle()->GetEntity())
			v = GetClientsideVehicle()->GetEntity()->GetAbsOrigin();		
#endif
	}

	QAngle ang = ASWEyeAngles();
	ang.x = 0;	// clear out pitch, so we're matching the fixes point of our autoaim calcs
	AngleVectors( ang, &forward, &right, &up );
	v = v + up * ASW_MARINE_GUN_OFFSET_Z;
	Vector vecSrc = v
					+ forward * ASW_MARINE_GUN_OFFSET_X
					+ right * ASW_MARINE_GUN_OFFSET_Y;

	trace_t tr;
	UTIL_TraceLine(v, vecSrc, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);
	if (tr.fraction < 1.0f)
		return tr.endpos;

	return vecSrc;
}

float CASW_Marine::MaxSpeed()
{
	float speed = 300;
	if (GetMarineResource())
	{
		speed = MarineSkills()->GetSkillBasedValueByMarine(this, ASW_MARINE_SKILL_AGILITY);
	}
	float speedscale = 1.0f;
	// half speed if we're firing or reloading
	if (GetActiveASWWeapon())			
		speedscale = GetActiveASWWeapon()->GetMovementScale();

	// adjust the speed by difficulty level
	switch (ASWGameRules()->GetSkillLevel())
	{
		case 5: speedscale *= asw_marine_speed_scale_insane.GetFloat(); break;
		case 4: speedscale *= asw_marine_speed_scale_insane.GetFloat(); break;
		case 3: speedscale *= asw_marine_speed_scale_hard.GetFloat(); break;
		case 2: speedscale *= asw_marine_speed_scale_normal.GetFloat(); break;
		default: speedscale *= asw_marine_speed_scale_easy.GetFloat(); break;
	}

	// if we're an AI following someone, boost our speed as we get far away.
	// it's a quadratic interpolation from 110% speed to 175% speed mapped
	// to a distance of 20 units to 1800 units
	CBaseEntity *pSquadLeader = GetSquadLeader();
	if (!IsInhabited() && pSquadLeader )
	{
		float distSq = GetAbsOrigin().DistToSqr(pSquadLeader->GetAbsOrigin());
		float t = (distSq - (20*20)) / (1800*1800);
		float q = ( 1.750f - 1.100f ) * t + 1.100f;
		speedscale *= clamp<float>( q, 1.000f, 1.750f );
	}

	// here's where we account for increased speed via powerup
	if ( m_iPowerupType == POWERUP_TYPE_INCREASED_SPEED )
		speedscale *= 1.75;

	// speed up as time slows down
	float flTimeDifference = 0.0f;
	
	if ( gpGlobals->curtime > ASWGameRules()->GetStimEndTime() )
	{
		flTimeDifference = 1.0f - MAX( asw_stim_time_scale.GetFloat(), GameTimescale()->GetCurrentTimescale() );
	}

	if ( flTimeDifference > 0.0f )
	{
		speedscale *= 1.0f + flTimeDifference * 0.75f;
	}
	
	return speed * speedscale;
}

// ammo in weapons this marine is carrying
int CASW_Marine::GetWeaponAmmoCount( int iAmmoIndex )
{
	// find how much is in guns we're carrying
	int iWeapons = WeaponCount();
	int iWeaponAmmo = 0;
	for (int i=0;i<iWeapons;i++)
	{
		CASW_Weapon* pWeapon = GetASWWeapon(i);
		if (pWeapon && pWeapon->GetPrimaryAmmoType() == iAmmoIndex)
			iWeaponAmmo += pWeapon->Clip1();
		if (pWeapon && pWeapon->GetSecondaryAmmoType() == iAmmoIndex)
			iWeaponAmmo += pWeapon->Clip2();
	}

	return iWeaponAmmo;
}

// include ammo in weapons this marine is carrying
int	CASW_Marine::GetWeaponAmmoCount( char *szName )
{
	return GetWeaponAmmoCount( GetAmmoDef()->Index(szName) );
}

// include ammo in weapons this marine is carrying
int CASW_Marine::GetTotalAmmoCount( int iAmmoIndex )
{
	// find how much is in held ammo
	int iSpareAmmo = GetAmmoCount(iAmmoIndex);
	
	// find how much is in guns we're carrying
	int iWeaponAmmo = GetWeaponAmmoCount(iAmmoIndex);;
	
	return iSpareAmmo + iWeaponAmmo;
}

int CASW_Marine::GetAllAmmoCount( void )
{
	int nCount = 0;

	for ( int i = 0; i < MAX_AMMO_TYPES; ++i )
	{
		nCount += GetTotalAmmoCount( i );
	}

	return nCount;
}

// include ammo in weapons this marine is carrying
int	CASW_Marine::GetTotalAmmoCount( char *szName )
{
	return GetTotalAmmoCount( GetAmmoDef()->Index(szName) );
}

// controlling a turret
bool CASW_Marine::IsControllingTurret()
{
	return (m_hRemoteTurret.Get()!=NULL);
}

CASW_Remote_Turret* CASW_Marine::GetRemoteTurret()
{
	return m_hRemoteTurret.Get();
}

const char *CASW_Marine::GetPlayerName() const
{
	CASW_Player *pPlayer = GetCommander();
	if ( !pPlayer )
	{
		return BaseClass::GetPlayerName();
	}

	return pPlayer->GetPlayerName();
}

CASW_Player* CASW_Marine::GetCommander() const
{
	return dynamic_cast<CASW_Player*>(m_Commander.Get());
}

bool CASW_Marine::IsInhabited()
{
	if (!GetMarineResource())
		return false;
	return GetMarineResource()->IsInhabited();
}

void CASW_Marine::DoDamagePowerupEffects( CBaseEntity *pTarget, CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
#ifdef GAME_DLL
	m_iDamageAttributeEffects = 0;

	if ( m_iPowerupType < 0 )
		return;

	switch ( m_iPowerupType )
	{
	case POWERUP_TYPE_FREEZE_BULLETS:
		m_iDamageAttributeEffects |= BULLET_ATT_FREEZE;
		//EmitSound( "ASW_Weapon_Crit.Freeze" );
		break;
	case POWERUP_TYPE_FIRE_BULLETS:
		m_iDamageAttributeEffects |= BULLET_ATT_FIRE;
		//EmitSound( "ASW_Weapon_Crit.Flame" );
		break;
	//case POWERUP_TYPE_CHEMICAL_BULLETS:
	//	m_iDamageAttributeEffects |= BULLET_ATT_CHEMICAL;
	//	EmitSound( "ASW_Weapon_Crit.Normal" );
	//	break;
	//case POWERUP_TYPE_EXPLOSIVE_BULLETS:
	//	m_iDamageAttributeEffects |= BULLET_ATT_EXPLODE;
		//	EmitSound( "ASW_Weapon_Crit.Normal" );
	//	break;
	case POWERUP_TYPE_ELECTRIC_BULLETS:
		m_iDamageAttributeEffects |= BULLET_ATT_ELECTRIC;
		//EmitSound( "ASW_Weapon_Crit.Electric" );
		break;
	default:
		break;
	}

	if ( m_iDamageAttributeEffects <= 0 || !GetMarineProfile() )
		return;

	// if we have a bullet-based powerup,
	// check if the weapon that has it is currently wielded
	CASW_Weapon* pWeapon = GetActiveASWWeapon();
	if ( !pWeapon || !pWeapon->m_bPoweredUp )
		return;

	info.ScaleDamage( 1.5f );

	if ( m_iDamageAttributeEffects & BULLET_ATT_EXPLODE )
	{
		CBaseEntity *pHelpHelpImBeingSupressed = (CBaseEntity*) te->GetSuppressHost();
		te->SetSuppressHost( NULL );		

		int iExplosionDamage = 16;
		float flExplosionRadius = 40;
		Vector vecExplosionPos = info.GetDamagePosition();
		CPASFilter filter( vecExplosionPos );
		int iFlags = TE_EXPLFLAG_SCALEPARTICLES;
		iFlags |= TE_EXPLFLAG_NOSOUND;

		if ( gpGlobals->curtime < m_flLastAttributeExplosionSound + 0.5f )
		{
			// no sound	
		}
		else
		{
			m_flLastAttributeExplosionSound = gpGlobals->curtime;
			CBaseEntity::EmitSound( filter, 0, "ASWGrenadeAmmo.Explode", &vecExplosionPos );
		}

		DispatchParticleEffect( "explosion_air_small", vecExplosionPos, vec3_angle );

		RadiusDamage( CTakeDamageInfo( this, this, iExplosionDamage, DMG_BLAST ), vecExplosionPos, flExplosionRadius, CLASS_NONE, NULL );

		te->SetSuppressHost( pHelpHelpImBeingSupressed );
	}


	if ( m_iDamageAttributeEffects & BULLET_ATT_FIRE )
	{
		IASW_Spawnable_NPC *pSpawnableNPC = dynamic_cast<IASW_Spawnable_NPC*>( pTarget );
		if ( pSpawnableNPC )
		{
			//CASW_Weapon *pWeapon = GetActiveASWWeapon();
			//int iLevel = 1; //GetMarineProfile()->GetLevel();
			//if ( pWeapon && pWeapon->GetAttributeContainer() && pWeapon->GetAttributeContainer()->GetItem() )
			//{
			//	iLevel = pWeapon->GetAttributeContainer()->GetItem()->GetItemLevel();
			//}
			/*
			static float flBaseFireDamage = 10.0f;
			static float flDamageChangePerLevel = 0.15f;
			static float flFireDuration = 3.0f;
			float flDamagePerSecond = ( ( iLevel - 1 ) * flDamageChangePerLevel * flBaseFireDamage + flBaseFireDamage ) / flFireDuration;
			

			// TODO: merge over the new ASW_Ignite to account for DPS?
			pSpawnableNPC->ASW_Ignite( flFireDuration, flDamagePerSecond, info.GetAttacker() );
			*/
			pSpawnableNPC->ASW_Ignite( 2.0f, 0, info.GetAttacker(), info.GetWeapon() );
		}
	}

	if ( m_iDamageAttributeEffects & BULLET_ATT_FREEZE )
	{
		CAI_BaseNPC *pNPC = dynamic_cast<CAI_BaseNPC*>( pTarget );
		if ( pNPC )
		{
			pNPC->Freeze( 100.0f, this );
		}
	}

	/*
	if ( m_iDamageAttributeEffects & BULLET_ATT_CHEMICAL )
	{
		CASW_Weapon_EffectVolume *pRad = (CASW_Weapon_EffectVolume*) CreateEntityByName("asw_weapon_effect_volume");
		if (pRad)
		{
			pRad->SetAbsOrigin( info.GetDamagePosition() );
			pRad->SetOwnerEntity( this );
			pRad->SetOwnerWeapon( GetActiveASWWeapon() );
			pRad->SetEffectFlag( ASW_WEV_RADIATION );
			pRad->SetDuration( 3.0f );
			pRad->Spawn();

			// also spawn our big cloud marking out the area of radiation
			CASW_Emitter *pEmitter = (CASW_Emitter*) CreateEntityByName("asw_emitter");
			if ( pEmitter )
			{
				pEmitter->UseTemplate("radcloud1");
				pEmitter->SetAbsOrigin( info.GetDamagePosition() );
				pEmitter->Spawn();
				pEmitter->SetDieTime( gpGlobals->curtime + 3.0f );
			}

			// play the geiger sound for 10 seconds
			const char *pszSound = "Misc.Geiger";
			float fRadPitch = random->RandomInt( 95, 105 );
			CPASAttenuationFilter filter( this );
			CSoundPatch *pPatch = CSoundEnvelopeController::GetController().SoundCreate( filter, entindex(), CHAN_STATIC, pszSound, ATTN_NORM );
			CSoundEnvelopeController::GetController().Play( pPatch, 1.0, fRadPitch );
			CSoundEnvelopeController::GetController().CommandAdd( pPatch, 1.0f, SOUNDCTRL_CHANGE_VOLUME, 1.0f, 0.0f );
			CSoundEnvelopeController::GetController().CommandAdd( pPatch, 2.0f, SOUNDCTRL_DESTROY, 0.0f, 0.0f );
		}
	}
	*/

	if ( m_iDamageAttributeEffects & BULLET_ATT_ELECTRIC )
	{
		int damageType = info.GetDamageType();
		damageType |= DMG_SHOCK;
		info.SetDamageType( damageType );

		// search other enemies near this one
		int iMaxArcs = 1;	// TODO: max arc should be attribute of the weapon? if we can combine two attribs into one somehow
		CUtlVector<CBaseEntity*> shocked;
		int iNumShocked = 0; // TODO: Use size of shocked vector for this?
		shocked.AddToTail( pTarget );
		float flArcDistSqr = 300.0f * 300.0f;
		float flShockDamage = info.GetDamage();	// TODO: base on attribute?
		float flDamageReduction = flShockDamage / (float)iMaxArcs; // how much damage each arc loses
		Vector vecShockSrc = info.GetDamagePosition();

		CBaseEntity *pLastShocked = pTarget;
		pLastShocked->EmitSound( "Electricity.Zap" );

		CASW_Alien *pAlien = dynamic_cast<CASW_Alien*>( pTarget );
		if ( pAlien )
			pAlien->ForceFlinch( vecShockSrc );

		for ( int k = 0; k < iMaxArcs; k++ )
		{
			CAI_BaseNPC **	ppAIs 	= g_AI_Manager.AccessAIs();
			int 			nAIs 	= g_AI_Manager.NumAIs();
			bool			bShocked = false;

			for ( int i = 0; i < nAIs; i++ )
			{
				if ( shocked.Find( ppAIs[i] ) != shocked.InvalidIndex() )	// already shocked this guy
					continue;

				if ( IRelationType( ppAIs[i] ) >= D_LI )	// friendly
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
					if ( IRelationType( shockTR.m_pEnt ) >= D_LI )	// friendly
						continue;

					// shock this thing
					vecAIPos = shockTR.endpos;
					CTakeDamageInfo shockDmgInfo( this, this, flShockDamage, DMG_SHOCK );					
					Vector vecDir = vecAIPos - vecShockSrc;
					VectorNormalize( vecDir );
					shockDmgInfo.SetDamagePosition( shockTR.endpos );
					shockDmgInfo.SetDamageForce( vecDir );
					shockDmgInfo.ScaleDamageForce( 1.0 );
					shockDmgInfo.SetAmmoType( info.GetAmmoType() );								

					shockTR.m_pEnt->DispatchTraceAttack( shockDmgInfo, vecDir, &shockTR );
					CASW_Alien *pAlien = dynamic_cast<CASW_Alien*>( shockTR.m_pEnt );
					if ( pAlien )
						pAlien->ForceFlinch( shockTR.endpos );

					// spawn a shock effect
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
		if ( iNumShocked > 0 )
		{
			// Return reduced shock damage if we arced
			info.SetDamage( flShockDamage );
		}
	}
#endif //GAME_DLL
}

void CASW_Marine::FireBullets( const FireBulletsInfo_t &info )
{
	float fPiercingChance = 0;
	if (GetMarineResource() && GetMarineProfile() && GetMarineProfile()->GetMarineClass() == MARINE_CLASS_SPECIAL_WEAPONS)
		fPiercingChance = MarineSkills()->GetSkillBasedValueByMarine(this, ASW_MARINE_SKILL_PIERCING);

	//if ( GetActiveWeapon() )
	//{
		//CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( GetActiveWeapon(), fPiercingChance, mod_piercing );
	//}

	if (fPiercingChance > 0)
	{
		FirePenetratingBullets(info, 1, fPiercingChance, 0 );
	}
	else
	{
		FireRegularBullets(info);
	}	
}

void CASW_Marine::FireRegularBullets( const FireBulletsInfo_t &info )
{
	static int	tracerCount;
	trace_t		tr;
	CAmmoDef*	pAmmoDef	= GetAmmoDef();
	int			nDamageType	= pAmmoDef->DamageType(info.m_iAmmoType);
	int			nAmmoFlags	= pAmmoDef->Flags(info.m_iAmmoType);

#ifdef GAME_DLL
	if (ASWGameRules())
		ASWGameRules()->m_fLastFireTime = gpGlobals->curtime;
#endif

	bool bDoServerEffects = true;

#if defined ( _XBOX ) && defined( GAME_DLL )
	if( IsInhabited() )
	{
		CBasePlayer *pPlayer = GetCommander();
		if (pPlayer)
		{

			int rumbleEffect = pPlayer->GetActiveWeapon()->GetRumbleEffect();

			if( rumbleEffect != RUMBLE_INVALID )
			{
				if( rumbleEffect == RUMBLE_SHOTGUN_SINGLE )
				{
					if( info.m_iShots == 12 )
					{
						// Upgrade to double barrel rumble effect
						rumbleEffect = RUMBLE_SHOTGUN_DOUBLE;
					}
				}

				pPlayer->RumbleEffect( rumbleEffect, 0, RUMBLE_FLAG_RESTART );
			}
		}
	}
#endif//_XBOX

	float flPlayerDamage = info.m_flPlayerDamage;
	if ( flPlayerDamage == 0 )
	{
		if ( nAmmoFlags & AMMO_INTERPRET_PLRDAMAGE_AS_DAMAGE_TO_PLAYER )
		{
			flPlayerDamage = pAmmoDef->PlrDamage( info.m_iAmmoType );
		}
	}

	// the default attacker is ourselves
	CBaseEntity *pAttacker = info.m_pAttacker ? info.m_pAttacker : this;

	// Make sure we don't have a dangling damage target from a recursive call
	if ( g_MultiDamage.GetTarget() != NULL )
	{
		ApplyMultiDamage();
	}
	  
	ClearMultiDamage();
	g_MultiDamage.SetDamageType( nDamageType | DMG_NEVERGIB );

	Vector vecDir;
	Vector vecEnd;
	Vector vecFinalDir;	// bullet's final direction can be changed by passing through a portal
	
	CASWTraceFilterShot traceFilter( this, info.m_pAdditionalIgnoreEnt, COLLISION_GROUP_NONE );	
	traceFilter.SetSkipMarines( false );
	traceFilter.SetSkipRollingMarines( true );

	int iSeed = CBaseEntity::GetPredictionRandomSeed();// & 255;

	CShotManipulator Manipulator( info.m_vecDirShooting );

	bool bDoImpacts = false;
	bool bDoTracers = false;
	
	for (int iShot = 0; iShot < info.m_iShots; iShot++)
	{
		bool bHitWater = false;
		bool bHitGlass = false;

		// Prediction is only usable on players
		RandomSeed( iSeed + iShot );	// init random system with this seed

		if (info.m_vecSpread[0] < 0)	// weapon wants simple random spread, not gaussian
		{
			QAngle angChange(info.m_vecSpread[0] * random->RandomFloat(-0.5f, 0.5f),
							info.m_vecSpread[1] * random->RandomFloat(-0.5f, 0.5f),
							info.m_vecSpread[2] * random->RandomFloat(-0.5f, 0.5f));
			matrix3x4_t matrix;
			AngleMatrix( angChange, matrix );
			VectorTransform(info.m_vecDirShooting, matrix, vecDir);
		}
		else
		{
			// If we're firing multiple shots, and the first shot has to be bang on target, ignore spread
			if ( iShot == 0 && info.m_iShots > 1 && (info.m_nFlags & FIRE_BULLETS_FIRST_SHOT_ACCURATE) )
			{
				vecDir = Manipulator.GetShotDirection();
			}
			else
			{
				if (info.m_nFlags & FIRE_BULLETS_ANGULAR_SPREAD)
					vecDir = Manipulator.ApplyAngularSpread( info.m_vecSpread );
				else
					vecDir = Manipulator.ApplySpread( info.m_vecSpread );
			}
		}

		vecEnd = info.m_vecSrc + vecDir * info.m_flDistance;

		if( (info.m_nFlags & FIRE_BULLETS_HULL) != 0 && asw_allow_hull_shots.GetBool())
		{
			// hulls that make it easier to hit targets with the shotgun.
			AI_TraceHull( info.m_vecSrc, vecEnd, Vector( -3, -3, -3 ), Vector( 3, 3, 3 ), MASK_SHOT, &traceFilter, &tr );
		}
		else
		{
			AI_TraceLine(info.m_vecSrc, vecEnd, MASK_SHOT, &traceFilter, &tr);
		}		

		vecFinalDir = tr.endpos - tr.startpos;
		if (vecFinalDir == vec3_origin)
		{
			vecFinalDir = info.m_vecDirShooting;
		}
		VectorNormalize( vecFinalDir );

#ifdef GAME_DLL
		if ( ai_debug_shoot_positions.GetBool() )
			NDebugOverlay::Line(info.m_vecSrc, vecEnd, 255, 255, 255, false, .1 );
#endif

		// Now hit all triggers along the ray that respond to shots...
		// Clip the ray to the first collided solid returned from traceline
		CTakeDamageInfo triggerInfo( pAttacker, pAttacker, info.m_flDamage, nDamageType );
		CalculateBulletDamageForce( &triggerInfo, info.m_iAmmoType, vecFinalDir, tr.endpos );
		triggerInfo.ScaleDamageForce( info.m_flDamageForceScale );
		triggerInfo.SetAmmoType( info.m_iAmmoType );
#ifdef GAME_DLL
		TraceAttackToTriggers( triggerInfo, tr.startpos, tr.endpos, vecFinalDir );
#endif

		// Make sure given a valid bullet type
		if (info.m_iAmmoType == -1)
		{
			DevMsg("ERROR: Undefined ammo type!\n");
			return;
		}

		Vector vecTracerDest = tr.endpos;

		// do damage, paint decals
		if (tr.fraction != 1.0)
		{
#ifdef GAME_DLL
			// For shots that don't need persistance
			int soundEntChannel = ( info.m_nFlags&FIRE_BULLETS_TEMPORARY_DANGER_SOUND ) ? SOUNDENT_CHANNEL_BULLET_IMPACT : SOUNDENT_CHANNEL_UNSPECIFIED;

			CSoundEnt::InsertSound( SOUND_BULLET_IMPACT, tr.endpos, 200, 0.5, this, soundEntChannel );
#endif

			// See if the bullet ended up underwater + started out of the water
			if ( !bHitWater && ( enginetrace->GetPointContents( tr.endpos ) & (CONTENTS_WATER|CONTENTS_SLIME) ) )
			{
				bHitWater = HandleShotImpactingWater( info, vecEnd, &traceFilter, &vecTracerDest );
			}

			float flActualDamage = info.m_flDamage;

			// If we hit a player, and we have player damage specified, use that instead
			// Adrian: Make sure to use the currect value if we hit a vehicle the player is currently driving.
			if ( flPlayerDamage != 0.0f )
			{
				if ( tr.m_pEnt->IsPlayer() )
				{
					flActualDamage = flPlayerDamage;
				}
#ifdef GAME_DLL
				else if ( tr.m_pEnt->GetServerVehicle() )
				{
					if ( tr.m_pEnt->GetServerVehicle()->GetPassenger() && tr.m_pEnt->GetServerVehicle()->GetPassenger()->IsPlayer() )
					{
						flActualDamage = flPlayerDamage;
					}
				}
#endif
			}

			int nActualDamageType = nDamageType;
			if ( flActualDamage == 0.0 )
			{
				flActualDamage = g_pGameRules->GetAmmoDamage( pAttacker, tr.m_pEnt, info.m_iAmmoType );
			}
			else
			{
				nActualDamageType = nDamageType | ((flActualDamage > ASW_GIB_DAMAGE_LIMIT) ? DMG_ALWAYSGIB : DMG_NEVERGIB );
			}

			if ( !bHitWater || ((info.m_nFlags & FIRE_BULLETS_DONT_HIT_UNDERWATER) == 0) )
			{
				// Damage specified by function parameter
				CTakeDamageInfo dmgInfo( this, pAttacker, flActualDamage, nActualDamageType );
				CalculateBulletDamageForce( &dmgInfo, info.m_iAmmoType, vecFinalDir, tr.endpos );
				dmgInfo.ScaleDamageForce( info.m_flDamageForceScale );
				dmgInfo.SetAmmoType( info.m_iAmmoType );
				dmgInfo.SetWeapon( GetActiveASWWeapon() );
				DoDamagePowerupEffects( tr.m_pEnt, dmgInfo, vecFinalDir, &tr );
				tr.m_pEnt->DispatchTraceAttack( dmgInfo, vecFinalDir, &tr );
			
				if ( !bHitWater || (info.m_nFlags & FIRE_BULLETS_ALLOW_WATER_SURFACE_IMPACTS) )
				{
					if ( bDoServerEffects == true )
					{
						DoImpactEffect( tr, nDamageType );
					}
					else
					{
						bDoImpacts = true;
					}
				}
				else
				{
					// We may not impact, but we DO need to affect ragdolls on the client
					CEffectData data;
					data.m_vStart = tr.startpos;
					data.m_vOrigin = tr.endpos;
					data.m_nDamageType = nDamageType;
					
					DispatchEffect( "RagdollImpact", data );
				}
	
#ifdef GAME_DLL
				if ( nAmmoFlags & AMMO_FORCE_DROP_IF_CARRIED )
				{
					// Make sure if the player is holding this, he drops it
					Pickup_ForcePlayerToDropThisObject( tr.m_pEnt );		
				}
#endif
			}
		}

		// See if we hit glass
		if ( tr.m_pEnt != NULL )
		{
#ifdef GAME_DLL
			surfacedata_t *psurf = physprops->GetSurfaceData( tr.surface.surfaceProps );
			if ( ( psurf != NULL ) && ( psurf->game.material == CHAR_TEX_GLASS ) && ( tr.m_pEnt->ClassMatches( "func_breakable" ) ) )
			{
				bHitGlass = true;
			}
#endif
		}

		if ( ( info.m_iTracerFreq != 0 ) && ( tracerCount++ % info.m_iTracerFreq ) == 0 && ( bHitGlass == false ) )
		{
			if ( bDoServerEffects == true )
			{
				Vector vecTracerSrc = vec3_origin;
				ComputeTracerStartPosition( info.m_vecSrc, &vecTracerSrc );

				trace_t Tracer;
				Tracer = tr;
				Tracer.endpos = vecTracerDest;

				MakeTracer( vecTracerSrc, Tracer, pAmmoDef->TracerType(info.m_iAmmoType) );
			}
			else
			{
				bDoTracers = true;
			}
		}
		// See if we should pass through glass
#ifdef GAME_DLL
		if ( bHitGlass )
		{
			HandleShotImpactingGlass( info, tr, vecFinalDir, &traceFilter );
		}
#endif

		//iSeed++;
	}

#ifdef GAME_DLL
	ApplyMultiDamage();
#endif
}

// fire bullets that will pass through NPCs, potentially hurting a whole bunch in a row
void CASW_Marine::FirePenetratingBullets( const FireBulletsInfo_t &info, int iMaxPenetrate, float fPenetrateChance, int iSeedPlus, bool bAllowChange/*=true*/, Vector *pPiercingTracerEnd/*=NULL*/, bool bSegmentTracer/*=true*/  )
{
	if (iMaxPenetrate < 0)
		return;

#ifdef GAME_DLL
	if (ASWGameRules())
		ASWGameRules()->m_fLastFireTime = gpGlobals->curtime;
#endif

	static int	tracerCount;
	trace_t		tr;
	CAmmoDef*	pAmmoDef	= GetAmmoDef();
	int			nDamageType	= pAmmoDef->DamageType(info.m_iAmmoType);
	int			nAmmoFlags	= pAmmoDef->Flags(info.m_iAmmoType);
	
	Vector vecPiercingTracerEnd(0,0,0);

	bool bDoServerEffects = true;

#if defined( HL2MP ) && defined( GAME_DLL )
	bDoServerEffects = false;
#endif

#if defined ( _XBOX ) && defined( GAME_DLL )
	if( IsPlayer() )
	{
		CBasePlayer *pPlayer = dynamic_cast<CBasePlayer*>(this);

		int rumbleEffect = pPlayer->GetActiveWeapon()->GetRumbleEffect();

		if( rumbleEffect != RUMBLE_INVALID )
		{
			if( rumbleEffect == RUMBLE_SHOTGUN_SINGLE )
			{
				if( info.m_iShots == 12 )
				{
					// Upgrade to double barrel rumble effect
					rumbleEffect = RUMBLE_SHOTGUN_DOUBLE;
				}
			}

			pPlayer->RumbleEffect( rumbleEffect, 0, RUMBLE_FLAG_RESTART );
		}
	}
#endif//_XBOX

	float flPlayerDamage = info.m_flPlayerDamage;
	if ( flPlayerDamage == 0 )
	{
		if ( nAmmoFlags & AMMO_INTERPRET_PLRDAMAGE_AS_DAMAGE_TO_PLAYER )
		{
			flPlayerDamage = pAmmoDef->PlrDamage( info.m_iAmmoType );
		}
	}

	// the default attacker is ourselves
	CBaseEntity *pAttacker = info.m_pAttacker ? info.m_pAttacker : this;

	// Make sure we don't have a dangling damage target from a recursive call
	if ( g_MultiDamage.GetTarget() != NULL )
	{
		ApplyMultiDamage();
	}
	  
	ClearMultiDamage();
	g_MultiDamage.SetDamageType( nDamageType | DMG_NEVERGIB );

	Vector vecDir;
	Vector vecEnd;
	Vector vecFinalDir;	// bullet's final direction can be changed by passing through a portal
	
	CASWTraceFilterShot traceFilter( this, info.m_pAdditionalIgnoreEnt, COLLISION_GROUP_NONE );
	traceFilter.SetSkipMarines( false );
	traceFilter.SetSkipRollingMarines( true );

	// Prediction is only usable on players
	int iSeed = (CBaseEntity::GetPredictionRandomSeed() + iSeedPlus );// & 255;

#if defined( HL2MP ) && defined( GAME_DLL )
	int iEffectSeed = iSeed;
#endif
	CShotManipulator Manipulator( info.m_vecDirShooting );

	bool bDoImpacts = false;
	
	for (int iShot = 0; iShot < info.m_iShots; iShot++)
	{
		bool bHitWater = false;
		bool bHitGlass = false;

		// Prediction is only usable on players		
		RandomSeed( iSeed + iShot );	// init random system with this seed

		if (pPiercingTracerEnd != NULL)	// if this is a refire, don't alter trajectory randomly again
		{
			vecDir = Manipulator.GetShotDirection();
		}
		else
		{
			// If we're firing multiple shots, and the first shot has to be bang on target, ignore spread
			if ( iShot == 0 && info.m_iShots > 1 && (info.m_nFlags & FIRE_BULLETS_FIRST_SHOT_ACCURATE) )
			{
				vecDir = Manipulator.GetShotDirection();
			}
			else
			{
				if (info.m_nFlags & FIRE_BULLETS_ANGULAR_SPREAD)
					vecDir = Manipulator.ApplyAngularSpread( info.m_vecSpread );
				else
					vecDir = Manipulator.ApplySpread( info.m_vecSpread );
			}
		}

		vecEnd = info.m_vecSrc + vecDir * info.m_flDistance;

		if( (info.m_nFlags & FIRE_BULLETS_HULL) != 0 && asw_allow_hull_shots.GetBool())
		{
			if (pPiercingTracerEnd == NULL)	// if this is the first trace from a shotgun, do a short wide hull search first, to catch those annoying cases where an alien is just sitting underneath our gun
			{
				AI_TraceHull( info.m_vecSrc, info.m_vecSrc + vecDir * 30, Vector( -10, -10, -10 ), Vector( 10, 10, 10 ), MASK_SHOT, &traceFilter, &tr );
#ifdef GAME_DLL
				if ( ai_debug_shoot_positions.GetBool() )
				{
					NDebugOverlay::SweptBox(info.m_vecSrc, info.m_vecSrc + vecDir * 30, Vector( -10, -10, -10 ), Vector( 10, 10, 10 ), QAngle(0,0,0), 255, 255, 0, 255, 10);
				}
#endif

				// if we didn't hit anything, then do a normal thin trace
				if (!tr.DidHit() || tr.DidHitWorld())
				{
					// hulls that make it easier to hit targets with the shotgun.
					AI_TraceHull( info.m_vecSrc, vecEnd, Vector( -3, -3, -3 ), Vector( 3, 3, 3 ), MASK_SHOT, &traceFilter, &tr );
				}
			}
			else
			{
				// hulls that make it easier to hit targets with the shotgun.
				AI_TraceHull( info.m_vecSrc, vecEnd, Vector( -3, -3, -3 ), Vector( 3, 3, 3 ), MASK_SHOT, &traceFilter, &tr );
			}
		}
		else
		{
			AI_TraceLine(info.m_vecSrc, vecEnd, MASK_SHOT, &traceFilter, &tr);
		}

		vecFinalDir = tr.endpos - tr.startpos;
		if (vecFinalDir == vec3_origin)
		{
			vecFinalDir = info.m_vecDirShooting;
		}
		VectorNormalize( vecFinalDir );

#ifdef GAME_DLL
		if ( ai_debug_shoot_positions.GetBool() )
		{
			if( (info.m_nFlags & FIRE_BULLETS_HULL) != 0 && asw_allow_hull_shots.GetBool())
			{
				NDebugOverlay::SweptBox(info.m_vecSrc, vecEnd, Vector( -3, -3, -3 ), Vector( 3, 3, 3 ), QAngle(0,0,0), 255, 255, 0, 255, 10);
			}
			else
			{
				NDebugOverlay::Line(info.m_vecSrc, vecEnd, 255, 255, 255, false, .1 );
			}
		}
#endif

		// Now hit all triggers along the ray that respond to shots...
		// Clip the ray to the first collided solid returned from traceline
		CTakeDamageInfo triggerInfo( pAttacker, pAttacker, info.m_flDamage, nDamageType );
		CalculateBulletDamageForce( &triggerInfo, info.m_iAmmoType, vecFinalDir, tr.endpos );
		triggerInfo.ScaleDamageForce( info.m_flDamageForceScale );
		triggerInfo.SetAmmoType( info.m_iAmmoType );
#ifdef GAME_DLL
		TraceAttackToTriggers( triggerInfo, tr.startpos, tr.endpos, vecFinalDir );
#endif

		// Make sure given a valid bullet type
		if (info.m_iAmmoType == -1)
		{
			DevMsg("ERROR: Undefined ammo type!\n");
			return;
		}

		Vector vecTracerDest = tr.endpos;

		vecPiercingTracerEnd = vecTracerDest;	// set to the end point of our trace.  If we recurse, then this gets filled in by a child call

		// do damage, paint decals
		if (tr.fraction != 1.0)
		{
#ifdef GAME_DLL
		
			// For shots that don't need persistance
			int soundEntChannel = ( info.m_nFlags&FIRE_BULLETS_TEMPORARY_DANGER_SOUND ) ? SOUNDENT_CHANNEL_BULLET_IMPACT : SOUNDENT_CHANNEL_UNSPECIFIED;

			CSoundEnt::InsertSound( SOUND_BULLET_IMPACT, tr.endpos, 200, 0.5, this, soundEntChannel );
#endif

			// See if the bullet ended up underwater + started out of the water
			if ( !bHitWater && ( enginetrace->GetPointContents( tr.endpos ) & (CONTENTS_WATER|CONTENTS_SLIME) ) )
			{
				bHitWater = HandleShotImpactingWater( info, vecEnd, &traceFilter, &vecTracerDest );
			}

			float flActualDamage = info.m_flDamage;

			// If we hit a player, and we have player damage specified, use that instead
			// Adrian: Make sure to use the currect value if we hit a vehicle the player is currently driving.
			if ( flPlayerDamage != 0.0f )
			{
				if ( tr.m_pEnt->IsPlayer() )
				{
					flActualDamage = flPlayerDamage;
				}
#ifdef GAME_DLL
				else if ( tr.m_pEnt->GetServerVehicle() )
				{
					if ( tr.m_pEnt->GetServerVehicle()->GetPassenger() && tr.m_pEnt->GetServerVehicle()->GetPassenger()->IsPlayer() )
					{
						flActualDamage = flPlayerDamage;
					}
				}
#endif
			}

			int nActualDamageType = nDamageType;
			if ( flActualDamage == 0.0 )
			{
				flActualDamage = g_pGameRules->GetAmmoDamage( pAttacker, tr.m_pEnt, info.m_iAmmoType );
			}
			else
			{
				// asw - upped the number on this  so the railgun doesn't always gib things
				nActualDamageType = nDamageType | ((flActualDamage > ASW_GIB_DAMAGE_LIMIT ) ? DMG_ALWAYSGIB : DMG_NEVERGIB );
			}

			if ( !bHitWater || ((info.m_nFlags & FIRE_BULLETS_DONT_HIT_UNDERWATER) == 0) )
			{
				// Damage specified by function parameter
				CTakeDamageInfo dmgInfo( this, pAttacker, flActualDamage, nActualDamageType );
				CalculateBulletDamageForce( &dmgInfo, info.m_iAmmoType, vecFinalDir, tr.endpos );
				dmgInfo.ScaleDamageForce( info.m_flDamageForceScale );
				dmgInfo.SetAmmoType( info.m_iAmmoType );
				dmgInfo.SetWeapon( GetActiveASWWeapon() );
				tr.m_pEnt->DispatchTraceAttack( dmgInfo, vecFinalDir, &tr );
			
				if ( !bHitWater || (info.m_nFlags & FIRE_BULLETS_ALLOW_WATER_SURFACE_IMPACTS) )
				{
					if ( bDoServerEffects == true )
					{
						DoImpactEffect( tr, nDamageType );
					}
					else
					{
						bDoImpacts = true;
					}
				}
				else
				{
					// We may not impact, but we DO need to affect ragdolls on the client
					CEffectData data;
					data.m_vStart = tr.startpos;
					data.m_vOrigin = tr.endpos;
					data.m_nDamageType = nDamageType;
					
					DispatchEffect( "RagdollImpact", data );
				}
	
#ifdef GAME_DLL
				if ( nAmmoFlags & AMMO_FORCE_DROP_IF_CARRIED )
				{
					// Make sure if the player is holding this, he drops it
					Pickup_ForcePlayerToDropThisObject( tr.m_pEnt );		
				}
#endif
			}
		}

		// See if we hit glass
		if ( tr.m_pEnt != NULL )
		{
#ifdef GAME_DLL
			surfacedata_t *psurf = physprops->GetSurfaceData( tr.surface.surfaceProps );
			if ( ( psurf != NULL ) && ( psurf->game.material == CHAR_TEX_GLASS ) && ( tr.m_pEnt->ClassMatches( "func_breakable" ) ) )
			{
				bHitGlass = true;
			}
#endif
		}
		float fDiceRoll = 0;
		if ( CBaseEntity::GetPredictionRandomSeed() != -1 )
		{
			// if we're predicting, use a shared float
			fDiceRoll = SharedRandomFloat("Piercing", 0.0f, 1.0f);
		}
		else
		{
			fDiceRoll = RandomFloat( 0.0f, 1.0f );
		}

		bool bPierce = ( tr.m_pEnt != NULL ) && ( fDiceRoll < fPenetrateChance) && !bHitGlass && !tr.DidHitWorld();
		if (bPierce)
		{
			CAI_BaseNPC *pNPC = dynamic_cast<CAI_BaseNPC*>(tr.m_pEnt);
			CASW_Simple_Alien *pAlien = dynamic_cast<CASW_Simple_Alien*>(tr.m_pEnt);
			if (pNPC || pAlien)
			{
#ifdef GAME_DLL
				if ( tr.m_pEnt->Classify() == CLASS_ASW_SHIELDBUG )		// don't let bullets pass through shieldbugs
#else
				if ( dynamic_cast<C_ASW_Shieldbug*>( tr.m_pEnt ) )
#endif
				{
					bPierce = false;
				}
				else if ((info.m_nFlags & FIRE_BULLETS_NO_PIERCING_SPARK) == 0)
				{
					CEffectData	data;
					data.m_vOrigin = tr.endpos;
					data.m_vNormal = vecDir;
					CPASFilter filter( data.m_vOrigin );
					//filter.SetIgnorePredictionCull(true);
					DispatchEffect( filter, 0.0, "PierceSpark", data );
				}
			}
			else
			{
				bPierce = false;	// don't pierce non-aliens
			}
		}

		if ( ( tr.m_pEnt != NULL ) && fPenetrateChance > 1.0f )
		{
			// We can only penetrate a few walls
			fPenetrateChance -= 1.0f;

			// If we didn't hit a wall, slow down the penetrating madness
			if( fPenetrateChance > 1.0f && !tr.DidHitWorld() )
			{
				fPenetrateChance -= static_cast<int>( fPenetrateChance );
			}

			bPierce = true;
		}

		// See if we should pass through glass
#ifdef GAME_DLL
		if ( bHitGlass )
		{
			HandleShotImpactingGlass( info, tr, vecFinalDir, &traceFilter );
		}

		// if we hit an entity and aren't at shot end, then see about firing another bullet
		else
#endif
		{
			{			
#ifdef GAME_DLL
				ApplyMultiDamage();		// apply the previous damage, since it'll get cleared when we refire
#endif
				if (bPierce)
				{					
					// Refire the round, as if starting from behind the NPC
					FireBulletsInfo_t behindNPCInfo(info);
					behindNPCInfo.m_iShots = 1;
					behindNPCInfo.m_vecSrc = tr.endpos;
					behindNPCInfo.m_vecDirShooting = vecDir;
					behindNPCInfo.m_vecSpread = vec3_origin;
					behindNPCInfo.m_flDistance = info.m_flDistance*( 1.0f - tr.fraction );

					// alter the angle so we're more likely to hit another enemy
					if ( tr.m_pEnt )
					{
						float zdiff = tr.m_pEnt->GetAbsOrigin().z - GetAbsOrigin().z;
						if (fabs(zdiff) < 70 && bAllowChange)	// alien and marine are roughly on the same floor, let's flatten our penetrated shot, so it's more likely to hit another
						{
							behindNPCInfo.m_vecDirShooting.z = 0;
							behindNPCInfo.m_vecDirShooting.NormalizeInPlace();
						}
					}

					if ( !tr.DidHitWorld() )
					{
						behindNPCInfo.m_pAdditionalIgnoreEnt = tr.m_pEnt;
					}
					else
					{
						// Penetrate past the solid
						behindNPCInfo.m_vecSrc = behindNPCInfo.m_vecSrc + behindNPCInfo.m_vecDirShooting * 28.0f;
						iMaxPenetrate++;
						behindNPCInfo.m_pAdditionalIgnoreEnt = NULL;
					}

					FirePenetratingBullets( behindNPCInfo, --iMaxPenetrate, fPenetrateChance, iSeedPlus, bAllowChange, &vecPiercingTracerEnd, bSegmentTracer );
						// this function returns with vecPiercingTracerEnd set to the end of the tracer
				}
			}
		}

		if ( ( info.m_iTracerFreq != 0 ) && ( tracerCount++ % info.m_iTracerFreq ) == 0 && ( bHitGlass == false ) )
		{
			if ( bDoServerEffects == true )
			{
				if ( pPiercingTracerEnd == NULL && !bSegmentTracer )	// only the very first call in a firepene chain actually makes a tracer
				{
					Vector vecTracerSrc = vec3_origin;
					ComputeTracerStartPosition( info.m_vecSrc, &vecTracerSrc );

					trace_t Tracer;
					Tracer = tr;
					Tracer.endpos = vecPiercingTracerEnd;

					// if we were a hull, move our endpoint nearer the wall so impact effects work 
					if( (info.m_nFlags & FIRE_BULLETS_HULL) != 0 && asw_allow_hull_shots.GetBool())
					{
						Tracer.endpos += vecFinalDir * 2.9f;
					}
					MakeTracer( vecTracerSrc, Tracer, pAmmoDef->TracerType(info.m_iAmmoType) );
				}
				else
				{
					Vector vecTracerSrc = vec3_origin;
					if ( pPiercingTracerEnd != NULL )  // this is not the first tracer, move the start out a bit so there's a gap
					{
						float gap = 32.0f;
						// come back to this later
						/*
						if ( info.m_pAdditionalIgnoreEnt )
						{
						Vector mins, maxs;
						info.m_pAdditionalIgnoreEnt->ExtractBbox( info.m_pAdditionalIgnoreEnt->GetSequence(), mins, maxs );

						float flRadius = MAX( MAX( FloatMakePositive( mins.x ), FloatMakePositive( maxs.x ) ),
						MAX( FloatMakePositive( mins.y ), FloatMakePositive( maxs.y ) ) );

						gap = flRadius;
						}
						*/
						vecTracerSrc = info.m_vecSrc + (vecFinalDir * gap);
					}
					else
						ComputeTracerStartPosition( info.m_vecSrc, &vecTracerSrc );

					trace_t Tracer;
					Tracer = tr;
					Tracer.endpos = tr.endpos;

					// if we were a hull, move our endpoint nearer the wall so impact effects work 
					if( (info.m_nFlags & FIRE_BULLETS_HULL) != 0 && asw_allow_hull_shots.GetBool())
					{
						Tracer.endpos += vecFinalDir * 2.9f;
					}

					if ( pPiercingTracerEnd == NULL )
					{
						if ( bSegmentTracer )
							MakeTracer( vecTracerSrc, Tracer, pAmmoDef->TracerType(info.m_iAmmoType) );
						else
						{
							trace_t Tracer2;
							Tracer = tr;
							Tracer.endpos = vecPiercingTracerEnd;
							MakeTracer( vecTracerSrc, Tracer2, pAmmoDef->TracerType(info.m_iAmmoType) );
						}
					}
					else
					{
						if ( bSegmentTracer )
							MakeUnattachedTracer( vecTracerSrc, Tracer, pAmmoDef->TracerType(info.m_iAmmoType) );
						QAngle	vecAngles;
						VectorAngles( vecFinalDir, vecAngles );
						DispatchParticleEffect( "drone_shot_exit", vecTracerSrc, vecAngles );
#ifdef CLIENT_DLL
						//CLocalPlayerFilter filter;
						//CBaseEntity::EmitSound( filter, entindex(), "ASW_Alien.Penetrate_Flesh" );
#endif //CLIENT_DLL
					}
				}

			}
		}
		// if we're a child call, then let our parent know where our tracer ended
		if (pPiercingTracerEnd != NULL)
		{
			*pPiercingTracerEnd = vecPiercingTracerEnd;
		}
	}

#ifdef GAME_DLL
	ApplyMultiDamage();
#endif
}

void CASW_Marine::FireBouncingBullets( const FireBulletsInfo_t &info, int iMaxBounce, int iSeedPlus/*=0 */ )
{
	if (iMaxBounce < 0)
		return;

#ifdef GAME_DLL
	if (ASWGameRules())
		ASWGameRules()->m_fLastFireTime = gpGlobals->curtime;
#endif

	static int	tracerCount;
	trace_t		tr;
	CAmmoDef*	pAmmoDef	= GetAmmoDef();
	int			nDamageType	= pAmmoDef->DamageType(info.m_iAmmoType);
	int			nAmmoFlags	= pAmmoDef->Flags(info.m_iAmmoType);
	
	bool bDoServerEffects = true;

#if defined ( _XBOX ) && defined( GAME_DLL )
	if( IsInhabited() )
	{
		CBasePlayer *pPlayer = GetCommander();
		if (pPlayer)
		{

			int rumbleEffect = pPlayer->GetActiveWeapon()->GetRumbleEffect();

			if( rumbleEffect != RUMBLE_INVALID )
			{
				if( rumbleEffect == RUMBLE_SHOTGUN_SINGLE )
				{
					if( info.m_iShots == 12 )
					{
						// Upgrade to double barrel rumble effect
						rumbleEffect = RUMBLE_SHOTGUN_DOUBLE;
					}
				}

				pPlayer->RumbleEffect( rumbleEffect, 0, RUMBLE_FLAG_RESTART );
			}
		}
	}
#endif//_XBOX

	float flPlayerDamage = info.m_flPlayerDamage;
	if ( flPlayerDamage == 0 )
	{
		if ( nAmmoFlags & AMMO_INTERPRET_PLRDAMAGE_AS_DAMAGE_TO_PLAYER )
		{
			flPlayerDamage = pAmmoDef->PlrDamage( info.m_iAmmoType );
		}
	}

	// the default attacker is ourselves
	CBaseEntity *pAttacker = info.m_pAttacker ? info.m_pAttacker : this;

	// Make sure we don't have a dangling damage target from a recursive call
	if ( g_MultiDamage.GetTarget() != NULL )
	{
		ApplyMultiDamage();
	}
	  
	ClearMultiDamage();
	g_MultiDamage.SetDamageType( nDamageType | DMG_NEVERGIB );

	Vector vecDir;
	Vector vecEnd;
	Vector vecFinalDir;	// bullet's final direction can be changed by passing through a portal
	
	CASWTraceFilterShot traceFilter( this, info.m_pAdditionalIgnoreEnt, COLLISION_GROUP_NONE );
	traceFilter.SetSkipMarines( false );
	traceFilter.SetSkipRollingMarines( true );

	int iSeed = (CBaseEntity::GetPredictionRandomSeed() + iSeedPlus );// & 255;

	CShotManipulator Manipulator( info.m_vecDirShooting );

	bool bDoImpacts = false;
	bool bDoTracers = false;
	
	for (int iShot = 0; iShot < info.m_iShots; iShot++)
	{
		bool bHitWater = false;
		bool bHitGlass = false;

		// Prediction is only usable on players
		RandomSeed( iSeed + iShot );	// init random system with this seed

		if (info.m_vecSpread[0] < 0)	// weapon wants simple random spread, not gaussian
		{
			QAngle angChange(info.m_vecSpread[0] * random->RandomFloat(-0.5f, 0.5f),
							info.m_vecSpread[1] * random->RandomFloat(-0.5f, 0.5f),
							info.m_vecSpread[2] * random->RandomFloat(-0.5f, 0.5f));
			matrix3x4_t matrix;
			AngleMatrix( angChange, matrix );
			VectorTransform(info.m_vecDirShooting, matrix, vecDir);
		}
		else
		{
			// If we're firing multiple shots, and the first shot has to be bang on target, ignore spread
			if ( iShot == 0 && info.m_iShots > 1 && (info.m_nFlags & FIRE_BULLETS_FIRST_SHOT_ACCURATE) )
			{
				vecDir = Manipulator.GetShotDirection();
			}
			else
			{
				if (info.m_nFlags & FIRE_BULLETS_ANGULAR_SPREAD)
					vecDir = Manipulator.ApplyAngularSpread( info.m_vecSpread );
				else
					vecDir = Manipulator.ApplySpread( info.m_vecSpread );
			}
		}

		vecEnd = info.m_vecSrc + vecDir * info.m_flDistance;

		if( (info.m_nFlags & FIRE_BULLETS_HULL) != 0 && asw_allow_hull_shots.GetBool())
		{
			// hulls that make it easier to hit targets with the shotgun.
			AI_TraceHull( info.m_vecSrc, vecEnd, Vector( -3, -3, -3 ), Vector( 3, 3, 3 ), MASK_SHOT, &traceFilter, &tr );
		}
		else
		{
			AI_TraceLine(info.m_vecSrc, vecEnd, MASK_SHOT, &traceFilter, &tr);
		}		

		vecFinalDir = tr.endpos - tr.startpos;
		if (vecFinalDir == vec3_origin)
		{
			vecFinalDir = info.m_vecDirShooting;
		}
		VectorNormalize( vecFinalDir );

#ifdef GAME_DLL
		if ( ai_debug_shoot_positions.GetBool() )
			NDebugOverlay::Line(info.m_vecSrc, vecEnd, 255, 255, 255, false, .1 );
#endif

		// Now hit all triggers along the ray that respond to shots...
		// Clip the ray to the first collided solid returned from traceline
		CTakeDamageInfo triggerInfo( pAttacker, pAttacker, info.m_flDamage, nDamageType );
		CalculateBulletDamageForce( &triggerInfo, info.m_iAmmoType, vecFinalDir, tr.endpos );
		triggerInfo.ScaleDamageForce( info.m_flDamageForceScale );
		triggerInfo.SetAmmoType( info.m_iAmmoType );
#ifdef GAME_DLL
		TraceAttackToTriggers( triggerInfo, tr.startpos, tr.endpos, vecFinalDir );
#endif

		// Make sure given a valid bullet type
		if (info.m_iAmmoType == -1)
		{
			DevMsg("ERROR: Undefined ammo type!\n");
			return;
		}

		Vector vecTracerDest = tr.endpos;

		// do damage, paint decals
		if (tr.fraction != 1.0)
		{
#ifdef GAME_DLL
			// For shots that don't need persistance
			int soundEntChannel = ( info.m_nFlags&FIRE_BULLETS_TEMPORARY_DANGER_SOUND ) ? SOUNDENT_CHANNEL_BULLET_IMPACT : SOUNDENT_CHANNEL_UNSPECIFIED;

			CSoundEnt::InsertSound( SOUND_BULLET_IMPACT, tr.endpos, 200, 0.5, this, soundEntChannel );
#endif

			// See if the bullet ended up underwater + started out of the water
			if ( !bHitWater && ( enginetrace->GetPointContents( tr.endpos ) & (CONTENTS_WATER|CONTENTS_SLIME) ) )
			{
				bHitWater = HandleShotImpactingWater( info, vecEnd, &traceFilter, &vecTracerDest );
			}

			float flActualDamage = info.m_flDamage;

			// If we hit a player, and we have player damage specified, use that instead
			// Adrian: Make sure to use the currect value if we hit a vehicle the player is currently driving.
			if ( flPlayerDamage != 0.0f )
			{
				if ( tr.m_pEnt->IsPlayer() )
				{
					flActualDamage = flPlayerDamage;
				}
#ifdef GAME_DLL
				else if ( tr.m_pEnt->GetServerVehicle() )
				{
					if ( tr.m_pEnt->GetServerVehicle()->GetPassenger() && tr.m_pEnt->GetServerVehicle()->GetPassenger()->IsPlayer() )
					{
						flActualDamage = flPlayerDamage;
					}
				}
#endif
			}

			int nActualDamageType = nDamageType;
			if ( flActualDamage == 0.0 )
			{
				flActualDamage = g_pGameRules->GetAmmoDamage( pAttacker, tr.m_pEnt, info.m_iAmmoType );
			}
			else
			{
				nActualDamageType = nDamageType | ((flActualDamage > ASW_GIB_DAMAGE_LIMIT) ? DMG_ALWAYSGIB : DMG_NEVERGIB );
			}

			if ( !bHitWater || ((info.m_nFlags & FIRE_BULLETS_DONT_HIT_UNDERWATER) == 0) )
			{
				// Damage specified by function parameter
				CTakeDamageInfo dmgInfo( this, pAttacker, flActualDamage, nActualDamageType );
				CalculateBulletDamageForce( &dmgInfo, info.m_iAmmoType, vecFinalDir, tr.endpos );
				dmgInfo.ScaleDamageForce( info.m_flDamageForceScale );
				dmgInfo.SetAmmoType( info.m_iAmmoType );
				dmgInfo.SetWeapon( GetActiveASWWeapon() );
				tr.m_pEnt->DispatchTraceAttack( dmgInfo, vecFinalDir, &tr );
			
				if ( !bHitWater || (info.m_nFlags & FIRE_BULLETS_ALLOW_WATER_SURFACE_IMPACTS) )
				{
					if ( bDoServerEffects == true )
					{
						DoImpactEffect( tr, nDamageType );
					}
					else
					{
						bDoImpacts = true;
					}
				}
				else
				{
					// We may not impact, but we DO need to affect ragdolls on the client
					CEffectData data;
					data.m_vStart = tr.startpos;
					data.m_vOrigin = tr.endpos;
					data.m_nDamageType = nDamageType;
					
					DispatchEffect( "RagdollImpact", data );
				}
	
#ifdef GAME_DLL
				if ( nAmmoFlags & AMMO_FORCE_DROP_IF_CARRIED )
				{
					// Make sure if the player is holding this, he drops it
					Pickup_ForcePlayerToDropThisObject( tr.m_pEnt );		
				}
#endif
			}
		}

		// See if we hit glass
		if ( tr.m_pEnt != NULL )
		{
#ifdef GAME_DLL
			surfacedata_t *psurf = physprops->GetSurfaceData( tr.surface.surfaceProps );
			if ( ( psurf != NULL ) && ( psurf->game.material == CHAR_TEX_GLASS ) && ( tr.m_pEnt->ClassMatches( "func_breakable" ) ) )
			{
				bHitGlass = true;
			}
#endif
		}

		if ( ( info.m_iTracerFreq != 0 ) && ( tracerCount++ % info.m_iTracerFreq ) == 0 && ( bHitGlass == false ) )
		{
			if ( bDoServerEffects == true )
			{
				if (iMaxBounce == 5) // bad hardcoded number for ricochet gun
				{
					Vector vecTracerSrc = vec3_origin;
					ComputeTracerStartPosition( info.m_vecSrc, &vecTracerSrc );

					trace_t Tracer;
					Tracer = tr;
					Tracer.endpos = vecTracerDest;

					MakeTracer( vecTracerSrc, Tracer, pAmmoDef->TracerType(info.m_iAmmoType) );
				}
				else
				{
					Vector vecTracerSrc = info.m_vecSrc;
					trace_t Tracer;
					Tracer = tr;
					Tracer.endpos = vecTracerDest;

					MakeUnattachedTracer( vecTracerSrc, Tracer, pAmmoDef->TracerType(info.m_iAmmoType) );
				}
			}
			else
			{
				bDoTracers = true;
			}
		}
		// See if we should pass through glass
#ifdef GAME_DLL
		if ( bHitGlass )
		{
			HandleShotImpactingGlass( info, tr, vecFinalDir, &traceFilter );
		}
#endif

		if ( tr.DidHitWorld() )
		{
			// Refire the round, bouncing off the surface
			FireBulletsInfo_t BouncingShotInfo(info);
			BouncingShotInfo.m_iShots = 1;
			BouncingShotInfo.m_vecSrc = tr.endpos;
			BouncingShotInfo.m_vecDirShooting = vecDir;
			// reflect the X+Y off the surface (leave Z intact so the shot is more likely to stay flat and hit enemies)
			float proj = (BouncingShotInfo.m_vecDirShooting).Dot( tr.plane.normal );
			VectorMA( BouncingShotInfo.m_vecDirShooting, -proj*2, tr.plane.normal, BouncingShotInfo.m_vecDirShooting );
			BouncingShotInfo.m_vecDirShooting.z = vecDir.z;

			BouncingShotInfo.m_vecSpread = vec3_origin;
			BouncingShotInfo.m_flDistance = info.m_flDistance*( 1.0f - tr.fraction );
			BouncingShotInfo.m_pAdditionalIgnoreEnt = NULL;			

#ifdef GAME_DLL
			ApplyMultiDamage();		// apply the previous damage, since it'll get cleared when we refire
			int iAliensKilledBeforeBounce = GetMarineResource() ? GetMarineResource()->m_iAliensKilled.Get() : 0;
#endif			
			FireBouncingBullets( BouncingShotInfo, --iMaxBounce );

#ifdef GAME_DLL
			if (GetMarineResource())
			{
				int iAliensKilledByBounce = GetMarineResource()->m_iAliensKilled.Get() - iAliensKilledBeforeBounce;
				GetMarineResource()->m_iAliensKilledByBouncingBullets += iAliensKilledByBounce;
			}
#endif
		}

		//iSeed++;
	}

#ifdef GAME_DLL
	ApplyMultiDamage();
#endif
}

CBaseCombatWeapon* CASW_Marine::GetLastWeaponSwitchedTo()
{
	return dynamic_cast<CBaseCombatWeapon*>(m_hLastWeaponSwitchedTo.Get());
}

bool CASW_Marine::TestHitboxes( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr )
{
	if (asw_marine_box_collision.GetBool())	// forces simple box collision on the marines (feels better for friendly fire)
	{
		if ( IntersectRayWithBox( ray, WorldAlignMins() + GetAbsOrigin(), WorldAlignMaxs() + GetAbsOrigin(), DIST_EPSILON, &tr ) )
		{
			tr.hitbox = 0;
			tr.hitgroup = HITGROUP_HEAD;
			return true;
		}
	}

	return BaseClass::TestHitboxes(ray, fContentsMask, tr);
}

void CASW_Marine::MakeTracer( const Vector &vecTracerSrc, const trace_t &tr, int iTracerType )
{
	const char* tracer = "ASWUTracer";
	if (GetActiveASWWeapon())
		tracer = GetActiveASWWeapon()->GetUTracerType();
#ifdef CLIENT_DLL
	CEffectData data;
	data.m_vOrigin = tr.endpos;
	data.m_hEntity = this;
	data.m_nMaterial = m_iDamageAttributeEffects;

	DispatchEffect( tracer, data );
#else
	CRecipientFilter filter;
	filter.AddAllPlayers();
	if (gpGlobals->maxClients > 1 && IsInhabited() && GetCommander())
	{ 
		filter.RemoveRecipient(GetCommander());
	}

	UserMessageBegin( filter, tracer );
		WRITE_SHORT( entindex() );
		WRITE_FLOAT( tr.endpos.x );
		WRITE_FLOAT( tr.endpos.y );
		WRITE_FLOAT( tr.endpos.z );
		WRITE_SHORT( m_iDamageAttributeEffects );
	MessageEnd();
#endif
}

void CASW_Marine::MakeUnattachedTracer( const Vector &vecTracerSrc, const trace_t &tr, int iTracerType )
{
	const char* tracer = "ASWUTracerUnattached";	
#ifdef CLIENT_DLL

	CEffectData data;
	data.m_vOrigin = tr.endpos;
	data.m_hEntity = this;
	data.m_vStart = vecTracerSrc;

	DispatchEffect( tracer, data );
#else
	CRecipientFilter filter;
	filter.AddAllPlayers();
	if (gpGlobals->maxClients > 1 && IsInhabited() && GetCommander())
	{ 
		filter.RemoveRecipient(GetCommander());
	}

	UserMessageBegin( filter, tracer );
		WRITE_SHORT( entindex() );
		WRITE_FLOAT( tr.endpos.x );
		WRITE_FLOAT( tr.endpos.y );
		WRITE_FLOAT( tr.endpos.z );
		WRITE_FLOAT( vecTracerSrc.x );
		WRITE_FLOAT( vecTracerSrc.y );
		WRITE_FLOAT( vecTracerSrc.z );
		WRITE_SHORT( m_iDamageAttributeEffects );
	MessageEnd();
#endif
}

// returns which slot in the m_hWeapons array this pickup should go in
int CASW_Marine::GetWeaponPositionForPickup( const char* szWeaponClass )
{	
	if (!szWeaponClass || !ASWEquipmentList())
	{
		Assert(0);
		return 0;
	}
	// if it's an extra type item, return the 3rd slot as that's the only place it'll fit
	CASW_WeaponInfo* pWeaponData = ASWEquipmentList()->GetWeaponDataFor(szWeaponClass);
	if (pWeaponData && pWeaponData->m_bExtra)
		return 2;

	// if item is unique, then check if we're already carrying one
	if (pWeaponData->m_bUnique)
	{
		CBaseCombatWeapon *pWeapon = GetWeapon(0);
		if (pWeapon && !Q_strcmp(szWeaponClass, pWeapon->GetClassname()))
			return 0;

		pWeapon = GetWeapon(1);
		if (pWeapon && !Q_strcmp(szWeaponClass, pWeapon->GetClassname()))
			return 1;
	}

	if (GetWeapon(0) == NULL)		// primary slot is free
		return 0;
	else if (GetWeapon(1) == NULL)	// secondary slot is free
		return 1;
	// all slots are full
	else if (GetActiveWeapon() == GetWeapon(0))		// we have primary currently selected, so exchange with that
		return 0;
	else if (GetActiveWeapon() == GetWeapon(1))		// we have primary currently selected, so exchange with that
		return 1;
	
	return 0;		// otherwise, swap with the primary slot

		/*
	Potential better system?

	for a weapon:  put in primary slot if free
			if not, put in secondary slot, if free
			if not, drop current weapon and put in that slot

	for a pickup:  put in secondary slot, if free
			if not, put in primary slot, if free
			if not, drop secondary and put in secondary
			*/
}

int CASW_Marine::GetNumberOfWeaponsUsingAmmo(int iAmmoIndex)
{	
	int count = 0;
	for (int i=0;i<3;i++)
	{
		CBaseCombatWeapon* pWeapon = GetWeapon(i);
		if (!pWeapon)
			continue;
		if (pWeapon->GetPrimaryAmmoType() == iAmmoIndex
			|| pWeapon->GetSecondaryAmmoType() == iAmmoIndex)
			count++;
	}
	return count;
}

bool CASW_Marine::CanPickupPrimaryAmmo()
{
	CASW_Weapon *pWeapon = GetActiveASWWeapon();

	if ( pWeapon && pWeapon->IsOffensiveWeapon() )
	{
		int iAmmoType = pWeapon->GetPrimaryAmmoType();
		int iGuns = GetNumberOfWeaponsUsingAmmo( iAmmoType );
		int iMaxAmmoCount = GetAmmoDef()->MaxCarry( iAmmoType, this ) * iGuns;
		int iBullets = GetAmmoCount( iAmmoType );

		if ( iBullets < iMaxAmmoCount )
		{
			return true;
		}
	}

	return false;
}

void CASW_Marine::ApplyMeleeDamage( CBaseEntity *pHitEntity, CTakeDamageInfo &dmgInfo, Vector &vecAttackDir, trace_t *tr )
{
	if ( !pHitEntity )
	{
		return;
	}

	// Knockback
#ifdef GAME_DLL
	CASW_Melee_Attack *pAttack = GetCurrentMeleeAttack();
	
	if ( pAttack )
	{
		if ( pAttack->m_flKnockbackForce > 0.0f )
		{
			CASW_Alien *pAlien = dynamic_cast<CASW_Alien*>(pHitEntity);
			float flFlatForce = pAttack->m_flKnockbackForce;
			float flUpForce = flFlatForce * asw_melee_knockback_up_force.GetFloat();
			
			// knockback aliens on broad strokes
			if ( pAlien )
			{
				Vector vecToTarget = pAlien->WorldSpaceCenter() - WorldSpaceCenter();
				vecToTarget.z = 0;
				VectorNormalize( vecToTarget );

				// undone: knock aliens to the side to better clear a path for the marine
				//Vector vecDirUp( 0, 0, 1 );
				//float flSideForce = flFlatForce * ( RandomInt( -1, 1 ) == -1 ? -1 : 1);
				//Vector vecSide;
				//CrossProduct( vecToTarget, vecDirUp, vecSide );

				pAlien->Knockback( vecToTarget * flFlatForce + Vector( 0, 0, 1 ) * flUpForce );
				//pAlien->ForceFlinch( vecAttackDir );
			}
		}
	}
#endif

	ApplyPassiveMeleeDamageEffects( dmgInfo );

	// Set the weapon to the marine so the stats system knows this was melee damage done
	dmgInfo.SetWeapon( this );
	pHitEntity->DispatchTraceAttack( dmgInfo, vecAttackDir, tr );
	if ( pHitEntity->IsNPC() && !m_bPlayedMeleeHitSound )
	{
#ifdef CLIENT_DLL
		if ( IsInhabited() )
		{
			if ( HasPowerFist() )
			{
				EmitSound( "ASW.MarinePowerFistAttackFP" );
			}
			else
			{
				EmitSound( "ASW.MarineMeleeAttackFP" );
			}
		}
		else
		{
			if ( HasPowerFist() )
			{
				EmitSound( "ASW.MarinePowerFistAttack" );
			}
			else
			{
				EmitSound( "ASW.MarineMeleeAttack" );
			}
		}
#else
		if ( gpGlobals->maxClients <= 1 && IsInhabited() )
		{
			if ( HasPowerFist() )
			{
				EmitSound( "ASW.MarinePowerFistAttackFP" );
			}
			else
			{
				EmitSound( "ASW.MarineMeleeAttackFP" );
			}
		}
		else
		{
			if ( HasPowerFist() )
			{
				EmitSound( "ASW.MarinePowerFistAttack" );
			}
			else
			{
				EmitSound( "ASW.MarineMeleeAttack" );
			}
		}
#endif
		m_bPlayedMeleeHitSound = true;
	}
	m_RecentMeleeHits.AddToTail( pHitEntity );

	ASW_WPN_MSG_CONVAR( asw_melee_debug, "%s(): [%.3f] Doing %f melee damage damage to %d:%s\n", __FUNCTION__, gpGlobals->curtime, dmgInfo.GetDamage(), pHitEntity->entindex(), pHitEntity->GetClassname() );

	ApplyMultiDamage();
}

void CASW_Marine::PlayMeleeImpactEffects( CBaseEntity *pEntity, trace_t *tr )
{
	if ( !pEntity )
	{
		return;
	}

	UTIL_ImpactTrace( tr, DMG_CLUB );

	CASW_Alien *pAlien = dynamic_cast<CASW_Alien*>(pEntity);

	if ( pAlien )
	{
#ifdef GAME_DLL
		// play a melee hit sound
		CPASAttenuationFilter otherfilter;
		CBaseEntity::EmitSound( otherfilter, pAlien->entindex(), "ASW.MarineMeleeAttack" );
#else	// #ifdef GAME_DLL
		CLocalPlayerFilter filter;
		CBaseEntity::EmitSound( filter, pAlien->entindex(), "ASW.MarineMeleeAttackFP" );
#endif	// #ifdef GAME_DLL
	}

#ifdef CLIENT_DLL
	DispatchParticleEffect( "melee_contact", tr->endpos, QAngle( 0, 0, 0 ) );
#endif	// #ifdef CLIENT_DLL
}

CASW_Melee_Attack* CASW_Marine::GetCurrentMeleeAttack()
{
	return ASWMeleeSystem()->GetMeleeAttackByID( m_iMeleeAttackID.Get() );
}

void CASW_Marine::HandlePredictedAnimEvent( int event, const char* options )
{
	if ( event == AE_EMPTY )
	{
	}
	else if ( event == AE_MELEE_DAMAGE )
	{
		float flYawStart, flYawEnd;

		flYawStart = flYawEnd = 0.0f;

		const char *p = options;
		char token[256];		
		if ( options[0] )
		{
			// Read in yaw start
			p = nexttoken( token, p, ' ' );

			if( token )
			{
				flYawStart = atof( token );
			}

			// Read in yaw end
			p = nexttoken( token, p, ' ' );

			if( token )
			{
				flYawEnd = atof( token );
			}
		}

		DoMeleeDamageTrace( flYawStart, flYawEnd );
	}
	else if ( event == AE_MELEE_START_COLLISION_DAMAGE )
	{
		if ( asw_melee_debug.GetBool() )
		{
			Msg( "%s AE_MELEE_START_COLLISION_DAMAGE\n", IsServer() ? "s" : "   c" );
		}
		m_bMeleeCollisionDamage = true;
	}
	else if ( event == AE_MELEE_STOP_COLLISION_DAMAGE )
	{
		if ( asw_melee_debug.GetBool() )
		{
			Msg( "%s AE_MELEE_STOP_COLLISION_DAMAGE\n", IsServer() ? "s" : "   c" );
		}
		m_bMeleeCollisionDamage = false;
	}
	else if ( event == AE_START_DETECTING_COMBO )
	{
		if ( asw_melee_debug.GetBool() )
		{
			Msg( "%s AE_START_DETECTING_COMBO\n", IsServer() ? "s" : "   c" );
		}
		m_bMeleeComboKeypressAllowed = true;
	}
	else if ( event == AE_STOP_DETECTING_COMBO )
	{
		if ( asw_melee_debug.GetBool() )
		{
			Msg( "%s AE_STOP_DETECTING_COMBO\n", IsServer() ? "s" : "   c" );
		}
		m_bMeleeComboKeypressAllowed = false;
	}
	else if ( event == AE_COMBO_TRANSITION )
	{
		if ( asw_melee_debug.GetBool() )
		{
			Msg( "%s AE_COMBO_TRANSITION\n", IsServer() ? "s" : "   c" );
		}
		m_bMeleeComboTransitionAllowed = true;
	}
	else if ( event == AE_ALLOW_MOVEMENT )
	{
		if ( asw_melee_debug.GetBool() )
		{
			Msg( "%s AE_ALLOW_MOVEMENT\n", IsServer() ? "s" : "   c" );
		}
		m_iMeleeAllowMovement = MELEE_MOVEMENT_FULL;
	}
	else if ( event == AE_SKILL_EVENT )
	{
		if ( asw_melee_debug.GetBool() )
		{
			Msg( "%s AE_SKILL_EVENT %s\n", IsServer() ? "s" : "   c", options );
		}
		char token[256];
		const char *p = options;
		p = nexttoken(token, p, ' ');
		if ( token ) 
		{
			if ( !Q_stricmp( token, "JumpJet" ) && ASWGameMovement() )
			{				
				//m_iMeleeAllowMovement = MELEE_MOVEMENT_FULL;
				//ASWGameMovement()->DoJumpJet();
			}
			else if ( !Q_stricmp( token, "RelectProjectiles" ) )
			{
				m_bReflectingProjectiles = true;
			}
		}
	}
	else
	{
#ifdef GAME_DLL
		//HandleAnimEvent( (animevent_t) event );
#else
		FireEvent( GetAbsOrigin(), GetAbsAngles(), event, options );
#endif
	}
}

bool CASW_Marine::IsHacking( void )
{
	return ( m_hCurrentHack.Get() != NULL );
}

// returns weapon's position in our myweapons array
int CASW_Marine::GetWeaponIndex( CBaseCombatWeapon *pWeapon ) const
{
	for ( int i = 0; i < WeaponCount() ; i++ )
	{
		if ( GetWeapon( i ) == pWeapon )
			return i;
	}
	return -1;
}

// DoMeleeDamageTrace()
// performs a melee damage trace forward or in a yaw range (counterclock wise, relative to the marine's forward vector)
void CASW_Marine::DoMeleeDamageTrace( float flYawStart, float flYawEnd )
{
	if ( asw_melee_debug.GetBool() )
	{
		Msg( "%s DoMeleeDamageTrace\n", IsServer() ? "[S]" : "[C]" );
	}

	float flDistance = 50.0f;
	float flTraceSize = 16.0f;
	CASW_Melee_Attack *pAttack = GetCurrentMeleeAttack();

	bool bHitBehindMarine = false;

	if ( pAttack )
	{
		if ( pAttack->m_flTraceDistance > 0 )
		{
			flDistance = pAttack->m_flTraceDistance;
		}
		if ( pAttack->m_flTraceHullSize > 0 )
		{
			flTraceSize = pAttack->m_flTraceHullSize;
		}

		bHitBehindMarine = pAttack->m_bAllowHitsBehindMarine;
	}

	Vector maxs, mins;
	maxs.Init();
	mins.Init();

	Vector vecTraceForward, vecTraceRight;
	AngleVectors( GetAbsAngles(), &vecTraceForward, &vecTraceRight, NULL );

	Vector vecTraceStart = GetAbsOrigin();

	// For directed attacks, this is the allowed angle tolerance for a hit
	float flAttackConeAngle = 0.0f;

	float flVerticalOffset = WorldAlignSize().z * 0.5;

	if( flVerticalOffset < maxs.z )
	{
		// There isn't enough room to trace this hull, it's going to drag the ground.
		// so make the vertical offset just enough to clear the ground.
		flVerticalOffset = maxs.z + 1.0;
	}

	vecTraceStart.z += flVerticalOffset;

	if ( fabs(flYawStart - flYawEnd) > 0.0f )
	{
		QAngle angleStart = vec3_angle;
		QAngle angleEnd = vec3_angle;

		// If a yaw range is specified, compute an AABB that bounds the yaw range
		flYawStart = AngleNormalize( flYawStart );
		flYawEnd = AngleNormalize( flYawEnd );
		
		flAttackConeAngle = 0.5f * AngleDiff(flYawEnd, flYawStart);

		angleStart[ YAW ] = flYawStart;
		angleEnd[ YAW ] = flYawEnd;

		Vector vecAimStart, vecAimEnd;
		VectorRotate( vecTraceForward, angleStart, vecAimStart );
		VectorRotate( vecTraceForward, angleEnd, vecAimEnd );

		vecTraceForward = (0.5f) * (vecAimStart + vecAimEnd);
		VectorNormalize( vecTraceForward );

		// Compute trace hull size
		// If we're given an angle to sweep out, set the Y hull size to bound the cone	
		Vector vecExtents = 0.5 * ((vecAimEnd - vecAimStart) * flDistance);

		maxs.x = MAX( vecExtents.x, -vecExtents.x );
		maxs.y = MAX( vecExtents.y, -vecExtents.y );

		mins.x = MIN( vecExtents.x, -vecExtents.x );
		mins.y = MIN( vecExtents.y, -vecExtents.y );

		maxs.z = 32.0f;
		mins.z = 4.0f;

		// Yaw angles may behind the marine, so allow hits even if creature is behind the marine
		bHitBehindMarine = true;

#ifdef GAME_DLL
		if ( ai_show_hull_attacks.GetBool() )
		{
			NDebugOverlay::Line( vecTraceStart, vecTraceStart + vecAimStart * flDistance, 255, 255, 0, true, 1.0f );
			NDebugOverlay::Line( vecTraceStart, vecTraceStart + vecAimEnd * flDistance, 0, 255, 0, true, 1.0f );
			NDebugOverlay::Line( vecTraceStart, vecTraceStart + vecTraceForward * flDistance, 255, 0, 0, true, 1.0f );		
		}
#endif
	}
	else
	{
		// otherwise, just use the standard hull
		maxs.x = flTraceSize;
		maxs.y = flTraceSize;
		maxs.z = ASW_MARINE_MELEE_HULL_TRACE_Z;

		mins.x = -flTraceSize;
		mins.y = -flTraceSize;
		mins.z = -ASW_MARINE_MELEE_HULL_TRACE_Z;
	}

	Vector vecTraceEnd = vecTraceStart + (vecTraceForward * flDistance);

	// asw - make melee attacks trace below us too, so it's possible hard to hit things just below you on a slope
	mins.z -= 30;

#ifdef GAME_DLL
	// check for turning on lag compensation	
	CASW_Player *pPlayer = GetCommander();
	bool bLagComp = false;
	if ( pPlayer && IsInhabited() && !CASW_Lag_Compensation::IsInLagCompensation() )
	{
		bLagComp = true;
		CASW_Lag_Compensation::AllowLagCompensation( pPlayer );
		CASW_Lag_Compensation::RequestLagCompensation( pPlayer, pPlayer->GetCurrentUserCommand() );
	}
#endif
	const CBaseEntity * ent = NULL;
	if ( gpGlobals->maxClients > 1 )
	{
		// temp remove suppress host
		ent = te->GetSuppressHost();
		te->SetSuppressHost( NULL );
	}

	MeleeTraceHullAttack( vecTraceStart, vecTraceEnd, mins, maxs, bHitBehindMarine, flAttackConeAngle );

	if ( gpGlobals->maxClients > 1 )
	{
		te->SetSuppressHost( (CBaseEntity *) ent );
	}
#ifdef GAME_DLL
	if ( bLagComp )
	{
		CASW_Lag_Compensation::FinishLagCompensation();	// undo lag compensation if we need to
	}
#endif
}

CBaseEntity *CASW_Marine::MeleeTraceHullAttack( const Vector &vecStart, const Vector &vecEnd, const Vector &vecMins, const Vector &vecMaxs, bool bHitBehindMarine, float flAttackCone )
{
	VPROF( "CASW_Marine::MeleeTraceHullAttack() - Marine melee attacks" );

	CTakeDamageInfo	dmgInfo( this, this, 0.0f, DMG_CLUB );
	dmgInfo.SetDamage( MarineSkills()->GetSkillBasedValueByMarine( this, ASW_MARINE_SKILL_MELEE, ASW_MARINE_SUBSKILL_MELEE_DMG ) );

	Vector vecAttackDir = vecEnd - vecStart;
	VectorNormalize( vecAttackDir );

	CASW_Trace_Filter_Melee traceFilter( this, COLLISION_GROUP_NONE, this, false, bHitBehindMarine, &vecAttackDir, flAttackCone, NULL /*&m_RecentMeleeHits*/ );

	Ray_t ray;
	ray.Init( vecStart, vecEnd, vecMins, vecMaxs );

	int nTrace = 0;
	trace_t tr;
	enginetrace->TraceRay( ray, MASK_SHOT_HULL, &traceFilter, &tr );

#ifdef GAME_DLL
	bool bHitBlockingProp = false;
	bool bHitEnemy = false;
#endif

	while ( nTrace < ASW_MAX_HITS_PER_TRACE && dmgInfo.GetDamage() > 0.0f )
	{
		Vector vecAttackerCenter = WorldSpaceCenter();

		// Perform damage on hit entities
		trace_t* tr = &traceFilter.m_HitTraces[ nTrace ];
		CBaseEntity *pHitEntity = tr->m_pEnt;

		if ( !pHitEntity )
		{
			break;
		}

		if ( asw_melee_debug.GetBool() )
		{
			Msg( "Melee trace %d hit %d:%s\n", nTrace, pHitEntity->entindex(), pHitEntity->GetDebugName() );
		}

#ifdef GAME_DLL
		// temp test
		if ( !IsInhabited() && GetPhysicsPropTarget() && GetPhysicsPropTarget() == pHitEntity )
			break;
#endif

#ifdef DISTRIBUTE_MELEE_DAMAGE_OVER_TARGETS
		int nHealth = pHitEntity->GetHealth();
#endif

		// TODO: TraceAttack should come from the impact fist?  Would need to somehow mark which bone is being used for which melee attack?
		// allow melee attacks to specify a source offset for effect playback
		Vector vecTraceAttackOffset = Vector( 0.0f, 0.0f, 0.0f );
		ASWMeleeSystem()->ComputeTraceOffset( this, vecTraceAttackOffset );

		Vector vecHitDir = tr->endpos - (vecAttackerCenter + vecTraceAttackOffset);
		if ( vecHitDir.IsZero() )
		{
			vecHitDir = vecAttackDir;
		}
		else
		{
			VectorNormalize( vecHitDir );
		}

		// notify melee weapons
		//if ( pMeleeWeapon )
		//{
			//pMeleeWeapon->OnMeleeDamageTraceHit( dmgInfo, vecHitDir, tr );
		//}

		// TODO
		//ASWCalculateMarineMeleeDamageForce( &dmgInfo, this, vecHitDir, tr );
		CalculateMeleeDamageForce( &dmgInfo, vecHitDir, vecStart );


		// actually perform the damage
		ApplyMeleeDamage( pHitEntity, dmgInfo, vecHitDir, tr );

#ifdef GAME_DLL

		if ( !IsInhabited() )
		{
			if ( GetPhysicsPropTarget() && GetPhysicsPropTarget() == pHitEntity )
			{
				bHitBlockingProp = true;
				CheckForDisablingAICollision( GetPhysicsPropTarget() );
			}
			else if ( GetEnemy() && GetEnemy() == pHitEntity )
			{
				bHitEnemy = true;
			}
		}

		// Handy debuging tool to visualize HullAttack trace
		if ( ai_show_hull_attacks.GetBool() )
		{
			NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + dmgInfo.GetDamageForce(), 0, 128, 0, true, 1.0f );
			NDebugOverlay::EntityBounds( pHitEntity, 128, 0, 0, 60, 3.0f );
		}
#endif

#ifdef DISTRIBUTE_MELEE_DAMAGE_OVER_TARGETS		// enabling this will cause melee damage to get used up with each entity it hits.  It will prevent hitting multiple entities with one melee attack.
		if ( nHealth <= 0 || pHitEntity->GetHealth() >= 0 )
		{
			dmgInfo.SetDamage( 0.0f );
		}
		else
		{
			float fHealthChange = nHealth - pHitEntity->GetHealth();
			float fDamageScale = fHealthChange / dmgInfo.GetDamage();

			dmgInfo.SetDamage( -pHitEntity->GetHealth() / fDamageScale );
		}
#endif

		nTrace++;
	}

	// do an impact effect for kicking some things
	PlayMeleeImpactEffects( traceFilter.m_hBestHit, traceFilter.m_pBestTrace );

#ifdef GAME_DLL
	// Handy debuging tool to visualize HullAttack trace
	if ( ai_show_hull_attacks.GetBool() )
	{
		// Draw this using SweptBox since the engine is doing AABB traces
		NDebugOverlay::SweptBox( vecStart, vecEnd, vecMins, vecMaxs, vec3_angle, 255, 0, 0, 255, 1.0f );
	}

	if ( !IsInhabited() )
	{
		if ( GetPhysicsPropTarget() && !bHitBlockingProp )
		{
			// AI marine is trying to punch a prop out of the way, but he's not hitting it, force it to take damage
			Vector vecTraceAttackOffset = Vector( 0.0f, 0.0f, 0.0f );
			ASWMeleeSystem()->ComputeTraceOffset( this, vecTraceAttackOffset );
			Vector vecHitDir = GetPhysicsPropTarget()->WorldSpaceCenter() - ( WorldSpaceCenter() + vecTraceAttackOffset );
			VectorNormalize( vecHitDir );
			CalculateMeleeDamageForce( &dmgInfo, vecHitDir, vecStart );
			AddMultiDamage( dmgInfo, GetPhysicsPropTarget() );
			ApplyMultiDamage();
			CheckForDisablingAICollision( GetPhysicsPropTarget() );
		}
		else if ( GetEnemy() && !bHitEnemy )
		{
			// make sure AI marine connects with his enemy
			Vector vecTraceAttackOffset = Vector( 0.0f, 0.0f, 0.0f );
			ASWMeleeSystem()->ComputeTraceOffset( this, vecTraceAttackOffset );
			Vector vecHitDir = GetEnemy()->WorldSpaceCenter() - ( WorldSpaceCenter() + vecTraceAttackOffset );
			VectorNormalize( vecHitDir );
			CalculateMeleeDamageForce( &dmgInfo, vecHitDir, vecStart );
			AddMultiDamage( dmgInfo, GetEnemy() );
			ApplyMultiDamage();
		}
	}
#endif

	return traceFilter.m_hBestHit;	
}

bool CASW_Marine::CanDoForcedAction( int iForcedAction )
{
	// check our current weapon allows us to do a forced action
	CASW_Weapon *pWeapon = GetActiveASWWeapon();
	if ( pWeapon && !pWeapon->CanDoForcedAction( iForcedAction ) )
		return false;

	return true;
}

void CASW_Marine::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
#ifdef GAME_DLL
	m_fNoDamageDecal = false;
	if ( m_takedamage == DAMAGE_NO )
		return;
#endif	

	CTakeDamageInfo subInfo = info;

#ifdef GAME_DLL
	SetLastHitGroup( ptr->hitgroup );
	m_nForceBone = ptr->physicsbone;		// save this bone for physics forces
#endif

	Assert( m_nForceBone > -255 && m_nForceBone < 256 );	

	if ( subInfo.GetDamage() >= 1.0 && !(subInfo.GetDamageType() & DMG_SHOCK )
		&& !( subInfo.GetDamageType() & DMG_BURN ) )
	{
#ifdef GAME_DLL
		Bleed( subInfo, ptr->endpos, vecDir, ptr );
		if ( ptr->hitgroup == HITGROUP_HEAD && m_iHealth - subInfo.GetDamage() > 0 )
		{
			m_fNoDamageDecal = true;
		}
#else
		Bleed( subInfo, ptr->endpos, vecDir, ptr );
#endif		
	}

	if( !info.GetInflictor() )
	{
		subInfo.SetInflictor( info.GetAttacker() );
	}

#ifdef GAME_DLL
	AddMultiDamage( subInfo, this );
#endif
}

void CASW_Marine::Bleed( const CTakeDamageInfo &info, const Vector &vecPos, const Vector &vecDir, trace_t *ptr )
{
#ifdef GAME_DLL
	Vector vecDamagePos = info.GetDamagePosition();
	CRecipientFilter filter;
	filter.AddAllPlayers();

	// if we've been shot by another marine...
	if ( info.GetAttacker() && info.GetAttacker()->Classify() == CLASS_ASW_MARINE )
	{
		if ( asw_marine_ff.GetInt() == 0 )
			return;

		UserMessageBegin( filter, "ASWMarineHitByFF" );	
		WRITE_SHORT( entindex() );
		WRITE_FLOAT( vecDamagePos.x );
		WRITE_FLOAT( vecDamagePos.y );
		WRITE_FLOAT( vecDamagePos.z );

		Vector vecNewDir = -vecDir;
		WRITE_FLOAT( vecNewDir.x );
		WRITE_FLOAT( vecNewDir.y );
		WRITE_FLOAT( vecNewDir.z );
		MessageEnd();
	}
	else
	{
		Vector vecInflictorPos = vecDamagePos;
		if ( info.GetInflictor() )
		{
			vecInflictorPos = info.GetInflictor()->GetAbsOrigin();
		}

		UserMessageBegin( filter, "ASWMarineHitByMelee" );
		WRITE_SHORT( entindex() );
		WRITE_FLOAT( vecInflictorPos.x );
		WRITE_FLOAT( vecInflictorPos.y );
		WRITE_FLOAT( vecInflictorPos.z );

		MessageEnd();
	}
#endif
	//DoBloodDecal( subInfo.GetDamage(), vecPos, vecDir, ptr, subInfo.GetDamageType() );
}

Vector CASW_Marine::GetOffhandThrowSource( const Vector *vecStandingPos )
{
	Vector vecOrigin = GetAbsOrigin();
	if ( vecStandingPos )
	{
		vecOrigin = *vecStandingPos;
	}
	Vector	vecSrc = Weapon_ShootPosition() - GetAbsOrigin() + vecOrigin;

	if ( IsInhabited() )
	{		
		// check it fits where we want to spawn it
		Ray_t ray;
		trace_t pm;
		ray.Init( WorldSpaceCenter() - GetAbsOrigin() + vecOrigin, vecSrc, -Vector( 4,4,4 ), Vector( 4,4,4 ) );
		UTIL_TraceRay( ray, MASK_SOLID, this, COLLISION_GROUP_PROJECTILE, &pm );
		if ( pm.fraction < 1.0f )
			vecSrc = pm.endpos;
	}
	else
	{
		// AI marines throw from their center, so facing doesn't affect the arc
		vecSrc.x = WorldSpaceCenter().x - GetAbsOrigin().x + vecOrigin.x;
		vecSrc.y = WorldSpaceCenter().y - GetAbsOrigin().y + vecOrigin.y;
	}
	return vecSrc;
}

bool CASW_Marine::IsFiring()
{
	return GetActiveASWWeapon() && GetActiveASWWeapon()->IsFiring();
}

// you can assume there is an attacker when this function is called.
void CASW_Marine::ApplyPassiveMeleeDamageEffects( CTakeDamageInfo &dmgInfo )
{
	for (int i=0; i<ASW_MAX_MARINE_WEAPONS; ++i) 
	{
		CASW_Weapon *pWeapon = GetASWWeapon( i );
		if ( pWeapon )
		{
			float flDamageScale = pWeapon->GetPassiveMeleeDamageScale();
			if ( flDamageScale != 1.0f )
			{
				dmgInfo.ScaleDamage( flDamageScale );
			}
		}
	}
}

bool CASW_Marine::HasPowerFist()
{
	CBaseEntity *pWeapon = GetWeapon( ASW_INVENTORY_SLOT_EXTRA );
	return( pWeapon && pWeapon->Classify() == CLASS_ASW_FIST );
}