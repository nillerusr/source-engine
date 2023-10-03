#include "cbase.h"
#include "asw_weapon_autogun_shared.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "c_asw_player.h"
#include "c_asw_weapon.h"
#include "c_asw_marine.h"
#include "c_asw_marine_resource.h"
#include "c_te_legacytempents.h"
#include "c_asw_gun_smoke_emitter.h"
#define CASW_Marine_Resource C_ASW_Marine_Resource
#define CASW_Marine C_ASW_Marine
#else
#include "asw_lag_compensation.h"
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_weapon.h"
#include "npcevent.h"
#include "shot_manipulator.h"
#include "asw_marine_resource.h"
#endif
#include "asw_marine_skills.h"
#include "asw_weapon_parse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Autogun, DT_ASW_Weapon_Autogun )

BEGIN_NETWORK_TABLE( CASW_Weapon_Autogun, DT_ASW_Weapon_Autogun )
#ifdef CLIENT_DLL
	// recvprops
#else
	// sendprops
	SendPropExclude( "DT_BaseAnimating", "m_flPlaybackRate" ),	
	SendPropExclude( "DT_BaseAnimating", "m_nSequence" ),	
	SendPropExclude( "DT_BaseAnimatingOverlay", "overlay_vars" ),
	SendPropExclude( "DT_BaseAnimating", "m_nNewSequenceParity" ),
	SendPropExclude( "DT_BaseAnimating", "m_nResetEventsParity" ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CASW_Weapon_Autogun )
	DEFINE_PRED_FIELD( m_flCycle, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_nSequence, FIELD_INTEGER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),	
	DEFINE_PRED_FIELD( m_nNewSequenceParity, FIELD_INTEGER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_nResetEventsParity, FIELD_INTEGER, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( asw_weapon_autogun, CASW_Weapon_Autogun );
PRECACHE_WEAPON_REGISTER(asw_weapon_autogun);

extern ConVar asw_weapon_max_shooting_distance;

#ifndef CLIENT_DLL
extern ConVar asw_debug_marine_damage;
//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Weapon_Autogun )
	
END_DATADESC()

void CASW_Weapon_Autogun::Spawn()
{
	BaseClass::Spawn();

	UseClientSideAnimation();
}

void CASW_Weapon_Autogun::SecondaryAttack( void )
{
	CASW_Player *pPlayer = GetCommander();
	if (!pPlayer)
		return;

	CASW_Marine *pMarine = GetMarine();
	if (!pMarine)
		return;

	// dry fire
	SendWeaponAnim( ACT_VM_DRYFIRE );
	BaseClass::WeaponSound( EMPTY );
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f;
}

#endif /* not client */

CASW_Weapon_Autogun::CASW_Weapon_Autogun()
{
#ifdef CLIENT_DLL
	m_flLastMuzzleFlashTime = 0;
#endif
}


CASW_Weapon_Autogun::~CASW_Weapon_Autogun()
{

}

void CASW_Weapon_Autogun::Precache()
{
	// precache the weapon model here?
	PrecacheScriptSound( "ASW_Autogun.ReloadA" );
	PrecacheScriptSound( "ASW_Autogun.ReloadB" );
	PrecacheScriptSound( "ASW_Autogun.ReloadC" );
	PrecacheScriptSound( "ASW_Weapon_Autogun.SpinDown" );

	BaseClass::Precache();
}

void CASW_Weapon_Autogun::OnStoppedFiring()
{
	BaseClass::OnStoppedFiring();

	EmitSound( "ASW_Weapon_Autogun.SpinDown" );
}

#ifdef CLIENT_DLL

void CASW_Weapon_Autogun::OnMuzzleFlashed()
{
	BaseClass::OnMuzzleFlashed();
	if (GetSequenceActivity(GetSequence()) != ACT_VM_PRIMARYATTACK)
	{
		SetActivity(ACT_VM_PRIMARYATTACK, 0);
	}
	m_flPlaybackRate = 1.0f;
	m_flLastMuzzleFlashTime = gpGlobals->curtime;
	
	int iAttachment = LookupAttachment( "eject1" );
	if( iAttachment != -1 )
	{
		EjectParticleBrass( "weapon_shell_casing_rifle", iAttachment );
	}
	if ( m_hGunSmoke.IsValid() )
	{
		m_hGunSmoke->OnFired();
	}
}


void CASW_Weapon_Autogun::ReachedEndOfSequence()
{
	BaseClass::ReachedEndOfSequence();
	if (gpGlobals->curtime - m_flLastMuzzleFlashTime > 1.0)	// 0.3 is the real firing time, but spin for a bit longer
	{
		if (GetSequenceActivity(GetSequence()) != ACT_VM_IDLE)
		{
			SetActivity(ACT_VM_IDLE, 0);
		}
	}
}

float CASW_Weapon_Autogun::GetMuzzleFlashScale( void )
{
	return BaseClass::GetMuzzleFlashScale();
}

bool CASW_Weapon_Autogun::GetMuzzleFlashRed()
{
	return BaseClass::GetMuzzleFlashRed();
}

#endif

bool CASW_Weapon_Autogun::SupportsBayonet()
{
	return false;
}

float CASW_Weapon_Autogun::GetMovementScale()
{
	return ShouldMarineMoveSlow() ? 0.5f : 0.8f;
}

float CASW_Weapon_Autogun::GetWeaponDamage()
{
	float flDamage = GetWeaponInfo()->m_flBaseDamage;

	if ( GetMarine() )
	{
		flDamage += MarineSkills()->GetSkillBasedValueByMarine(GetMarine(), ASW_MARINE_SKILL_AUTOGUN, ASW_MARINE_SUBSKILL_AUTOGUN_DMG);
	}

	return flDamage;
}

const Vector& CASW_Weapon_Autogun::GetBulletSpread( void )
{
	static Vector cone = VECTOR_CONE_7DEGREES;
	return cone;
}

#ifdef CLIENT_DLL
const char* CASW_Weapon_Autogun::GetPartialReloadSound(int iPart)
{
	switch (iPart)
	{
		case 1: return "ASW_Autogun.ReloadB"; break;
		case 2: return "ASW_Autogun.ReloadC"; break;
		default: break;
	};
	return "ASW_Autogun.ReloadA";
}

void CASW_Weapon_Autogun::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		CreateGunSmoke();
	}
}

void CASW_Weapon_Autogun::CreateGunSmoke()
{	
	int iAttachment = LookupAttachment( "muzzle" );
	if ( iAttachment <= 0 )
	{
		Msg("error, couldn't find muzzle attachment for flamer tip\n");
		return;
	}

	C_ASW_Gun_Smoke_Emitter *pEnt = new C_ASW_Gun_Smoke_Emitter;
	if (!pEnt)
	{
		Msg("Error, couldn't create new C_ASW_Gun_Smoke_Emitter for autogun smoke\n");
		return;
	}
	if ( !pEnt->InitializeAsClientEntity( NULL, false ) )
	{
		Msg("Error, couldn't InitializeAsClientEntity for autogun smoke\n");
		UTIL_Remove( pEnt );
		return;
	}
	
	Vector vecMuzzle;
	QAngle angMuzzle;
	GetAttachment( iAttachment, vecMuzzle, angMuzzle );

	Q_snprintf(pEnt->m_szTemplateName, sizeof(pEnt->m_szTemplateName), "autogunsmoke");
	pEnt->m_fScale = 1.0f;
	pEnt->m_bEmit = false;
	pEnt->SetAbsOrigin(vecMuzzle);
	pEnt->CreateEmitter();
	pEnt->SetAbsOrigin(vecMuzzle);
	pEnt->SetAbsAngles(angMuzzle);

	pEnt->ClientAttach(this, "muzzle");

	m_hGunSmoke = pEnt;
}

void CASW_Weapon_Autogun::UpdateOnRemove()
{
	if ( m_hGunSmoke.IsValid() )
	{
		UTIL_Remove( m_hGunSmoke.Get() );
	}

	BaseClass::UpdateOnRemove();
}
#endif