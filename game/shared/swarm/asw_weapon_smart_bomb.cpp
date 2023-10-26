#include "cbase.h"
#include "asw_weapon_smart_bomb.h"

#ifdef CLIENT_DLL
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "c_asw_alien.h"
#include "asw_input.h"
#include "prediction.h"
#else
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_alien.h"
#include "particle_parse.h"
#include "te_effect_dispatch.h"
#include "asw_rocket.h"
#include "asw_gamerules.h"
#endif
#include "asw_marine_skills.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Smart_Bomb, DT_ASW_Weapon_Smart_Bomb )

BEGIN_NETWORK_TABLE( CASW_Weapon_Smart_Bomb, DT_ASW_Weapon_Smart_Bomb )
#ifdef CLIENT_DLL
#else
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CASW_Weapon_Smart_Bomb )
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( asw_weapon_smart_bomb, CASW_Weapon_Smart_Bomb );
PRECACHE_WEAPON_REGISTER( asw_weapon_smart_bomb );

#ifndef CLIENT_DLL
//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Weapon_Smart_Bomb )

END_DATADESC()

#endif /* not client */

CASW_Weapon_Smart_Bomb::CASW_Weapon_Smart_Bomb()
{

}

const QAngle& CASW_Weapon_Smart_Bomb::GetRocketAngle()
{
	static QAngle angRocket = vec3_angle;
	CASW_Player *pPlayer = GetCommander();
	CASW_Marine *pMarine = GetMarine();
	if ( !pPlayer || !pMarine || pMarine->GetHealth() <= 0 )
	{
		return angRocket;
	}

	//Vector vecDir = pPlayer->GetAutoaimVectorForMarine(pMarine, GetAutoAimAmount(), GetVerticalAdjustOnlyAutoAimAmount());	// 45 degrees = 0.707106781187
	//VectorAngles( vecDir, angRocket );
	//angRocket[ YAW ] += random->RandomFloat( -35, 35 );
	angRocket[ PITCH ] = -60.0f;	// aim up to help avoid FF
	if ( GetRocketsToFire() % 2 )
	{
		angRocket[ YAW ] = m_iRocketsToFire * 15.0f;	// 15 degrees between each rocket
	}
	else
	{
		angRocket[ YAW ] = RandomFloat( 0.0f, 360.0f );
	}
	angRocket[ ROLL ] = 0;
	return angRocket;
}

void CASW_Weapon_Smart_Bomb::SetRocketsToFire()
{
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
		return;

	m_iRocketsToFire = MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_GRENADES, ASW_MARINE_SUBSKILL_GRENADE_SMART_BOMB_COUNT );
}

float CASW_Weapon_Smart_Bomb::GetRocketFireInterval()
{
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
		return 0.5f;

	return MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_GRENADES, ASW_MARINE_SUBSKILL_GRENADE_SMART_BOMB_INTERVAL );
}

const Vector& CASW_Weapon_Smart_Bomb::GetRocketFiringPosition()
{
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
		return vec3_origin;

	static Vector vecSrc;
	QAngle ang;

	int nAttachment = pMarine->LookupAttachment( "backpack" );
	pMarine->GetAttachment( nAttachment, vecSrc, ang );

	return vecSrc;
}