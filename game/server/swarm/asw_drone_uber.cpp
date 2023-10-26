// A tougher version of the standard Swarm drone.  It's green, bigger and has more health.
#include "cbase.h"
#include "asw_drone_uber.h"
#include "asw_gamerules.h"
#include "asw_marine.h"
#include "asw_weapon_assault_shotgun_shared.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar asw_drone_uber_health("asw_drone_uber_health", "500", FCVAR_CHEAT, "How much health the uber Swarm drones have");
ConVar asw_uber_speed_scale("asw_uber_speed_scale", "0.5f", FCVAR_CHEAT, "Speed scale of uber drone compared to normal");
ConVar asw_uber_auto_speed_scale("asw_uber_auto_speed_scale", "0.3f", FCVAR_CHEAT, "Speed scale of uber drones when attacking");
extern ConVar asw_alien_hurt_speed;
extern ConVar asw_alien_stunned_speed;

#define	SWARM_DRONE_UBER_MODEL	"models/swarm/drone/UberDrone.mdl"

CASW_Drone_Uber::CASW_Drone_Uber()	
{
	
}

CASW_Drone_Uber::~CASW_Drone_Uber()
{
	
}

LINK_ENTITY_TO_CLASS( asw_drone_uber, CASW_Drone_Uber );

BEGIN_DATADESC( CASW_Drone_Uber )

END_DATADESC()

void CASW_Drone_Uber::Spawn( void )
{	
	BaseClass::Spawn();

	SetModel( SWARM_NEW_DRONE_MODEL );	
	Precache();	

	SetHullType(HULL_LARGE);
	SetHullSizeNormal();

	UTIL_SetSize(this, Vector(-40,-40,0), Vector(40,40,130));

	// make sure uber drones are green
	m_nSkin = 0;
	SetHitboxSet(0);
}

void CASW_Drone_Uber::Precache( void )
{
	PrecacheModel( SWARM_NEW_DRONE_MODEL );

	BaseClass::Precache();
}


void CASW_Drone_Uber::SetHealthByDifficultyLevel()
{
	SetHealth(ASWGameRules()->ModifyAlienHealthBySkillLevel(asw_drone_uber_health.GetInt()));
	SetMaxHealth(GetHealth());
	//if (ASWGameRules()->GetSkillLevel() <= 1)	// on easy we use the bigger hitbox set
		SetHitboxSet(0);
	//else
		//SetHitboxSet(2);
}

float CASW_Drone_Uber::GetIdealSpeed() const
{
	return BaseClass::GetIdealSpeed() * asw_uber_speed_scale.GetFloat();
}

int CASW_Drone_Uber::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	int result = 0;

	CTakeDamageInfo newInfo(info);
	float damage = info.GetDamage();

	// reduce damage from shotguns and mining laser
	if (info.GetDamageType() & DMG_ENERGYBEAM)
	{
		damage *= 0.5f;
	}
	if (info.GetDamageType() & DMG_BUCKSHOT)
	{
		// hack to reduce vindicator damage (not reducing normal shotty as much as it's not too strong)
		if (info.GetAttacker() && info.GetAttacker()->Classify() == CLASS_ASW_MARINE)
		{
			CASW_Marine *pMarine = dynamic_cast<CASW_Marine*>(info.GetAttacker());
			if (pMarine)
			{
				CASW_Weapon_Assault_Shotgun *pVindicator = dynamic_cast<CASW_Weapon_Assault_Shotgun*>(pMarine->GetActiveASWWeapon());
				if (pVindicator)
					damage *= 0.45f;
				else
					damage *= 0.6f;
			}
		}		
	}

	newInfo.SetDamage(damage);
	result = BaseClass::OnTakeDamage_Alive(newInfo);

	return result;
}

bool CASW_Drone_Uber::ModifyAutoMovement( Vector &vecNewPos )
{
	// melee auto movement on the drones seems way too fast
	float fFactor = asw_uber_auto_speed_scale.GetFloat();
	if ( ShouldMoveSlow() )
	{
		if ( m_bElectroStunned.Get() )
		{
			fFactor *= asw_alien_stunned_speed.GetFloat() * 0.1f;
		}
		else
		{
			fFactor *= asw_alien_hurt_speed.GetFloat() * 0.1f;
		}
	}
	Vector vecRelPos = vecNewPos - GetAbsOrigin();
	vecRelPos *= fFactor;
	vecNewPos = GetAbsOrigin() + vecRelPos;
	return true;
}