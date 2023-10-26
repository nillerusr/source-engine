#include "cbase.h"
#include "asw_debrief_stats.h"
#include "asw_shareddefs.h"
#include "asw_marine.h"
#include "asw_marine_resource.h"
#include "asw_game_resource.h"
#include "asw_gamerules.h"
#include "asw_debrief_info.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( asw_debrief_stats, CASW_Debrief_Stats );
PRECACHE_REGISTER( asw_debrief_stats );

IMPLEMENT_SERVERCLASS_ST(CASW_Debrief_Stats, DT_ASW_Debrief_Stats)	
	SendPropArray3	( SENDINFO_ARRAY3(m_iKills), SendPropInt( SENDINFO_ARRAY(m_iKills) ) ),
	SendPropArray3	( SENDINFO_ARRAY3(m_fAccuracy), SendPropFloat( SENDINFO_ARRAY(m_fAccuracy) ) ),
	SendPropArray3	( SENDINFO_ARRAY3(m_iFF), SendPropInt( SENDINFO_ARRAY(m_iFF) ) ),
	SendPropArray3	( SENDINFO_ARRAY3(m_iDamage), SendPropInt( SENDINFO_ARRAY(m_iDamage) ) ),
	SendPropArray3	( SENDINFO_ARRAY3(m_iShotsFired), SendPropInt( SENDINFO_ARRAY(m_iShotsFired) ) ),
	SendPropArray3	( SENDINFO_ARRAY3(m_iShotsHit), SendPropInt( SENDINFO_ARRAY(m_iShotsHit) ) ),
	SendPropArray3	( SENDINFO_ARRAY3(m_iWounded), SendPropInt( SENDINFO_ARRAY(m_iWounded) ) ),
	SendPropArray3	( SENDINFO_ARRAY3(m_iAliensBurned), SendPropInt( SENDINFO_ARRAY(m_iAliensBurned) ) ),
	SendPropArray3	( SENDINFO_ARRAY3(m_iHealthHealed), SendPropInt( SENDINFO_ARRAY(m_iHealthHealed) ) ),
	SendPropArray3	( SENDINFO_ARRAY3(m_iFastHacks), SendPropInt( SENDINFO_ARRAY(m_iFastHacks) ) ),	
	SendPropArray3	( SENDINFO_ARRAY3(m_iSkillPointsAwarded), SendPropInt( SENDINFO_ARRAY(m_iSkillPointsAwarded) ) ),	
	SendPropArray3	( SENDINFO_ARRAY3(m_iStartingEquip0), SendPropInt( SENDINFO_ARRAY(m_iStartingEquip0))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iStartingEquip1), SendPropInt( SENDINFO_ARRAY(m_iStartingEquip1))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iStartingEquip2), SendPropInt( SENDINFO_ARRAY(m_iStartingEquip2))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iAmmoDeployed), SendPropInt( SENDINFO_ARRAY(m_iAmmoDeployed))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iSentryGunsDeployed), SendPropInt( SENDINFO_ARRAY(m_iSentryGunsDeployed))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iSentryFlamerDeployed), SendPropInt( SENDINFO_ARRAY(m_iSentryFlamerDeployed))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iSentryFreezeDeployed), SendPropInt( SENDINFO_ARRAY(m_iSentryFreezeDeployed))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iSentryCannonDeployed), SendPropInt( SENDINFO_ARRAY(m_iSentryCannonDeployed))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iMedkitsUsed), SendPropInt( SENDINFO_ARRAY(m_iMedkitsUsed))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iFlaresUsed), SendPropInt( SENDINFO_ARRAY(m_iFlaresUsed))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iAdrenalineUsed), SendPropInt( SENDINFO_ARRAY(m_iAdrenalineUsed))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iTeslaTrapsDeployed), SendPropInt( SENDINFO_ARRAY(m_iTeslaTrapsDeployed))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iFreezeGrenadesThrown), SendPropInt( SENDINFO_ARRAY(m_iFreezeGrenadesThrown))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iElectricArmorUsed), SendPropInt( SENDINFO_ARRAY(m_iElectricArmorUsed))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iHealGunHeals), SendPropInt( SENDINFO_ARRAY(m_iHealGunHeals))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iHealBeaconHeals), SendPropInt( SENDINFO_ARRAY(m_iHealBeaconHeals))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iHealGunHeals_Self), SendPropInt( SENDINFO_ARRAY(m_iHealGunHeals_Self))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iHealBeaconHeals_Self), SendPropInt( SENDINFO_ARRAY(m_iHealBeaconHeals_Self))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iDamageAmpsUsed), SendPropInt( SENDINFO_ARRAY(m_iDamageAmpsUsed))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iHealBeaconsDeployed), SendPropInt( SENDINFO_ARRAY(m_iHealBeaconsDeployed))),


 	SendPropArray3	( SENDINFO_ARRAY3(m_iWeaponClassAndKills0), SendPropInt( SENDINFO_ARRAY(m_iWeaponClassAndKills0))),
 	SendPropArray3	( SENDINFO_ARRAY3(m_iDamageAndFF0), SendPropInt( SENDINFO_ARRAY(m_iDamageAndFF0))),
 	SendPropArray3	( SENDINFO_ARRAY3(m_iShotsFiredAndHit0), SendPropInt( SENDINFO_ARRAY(m_iShotsFiredAndHit0))),

	SendPropArray3	( SENDINFO_ARRAY3(m_iWeaponClassAndKills1), SendPropInt( SENDINFO_ARRAY(m_iWeaponClassAndKills1))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iDamageAndFF1), SendPropInt( SENDINFO_ARRAY(m_iDamageAndFF1))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iShotsFiredAndHit1), SendPropInt( SENDINFO_ARRAY(m_iShotsFiredAndHit1))),

	SendPropArray3	( SENDINFO_ARRAY3(m_iWeaponClassAndKills2), SendPropInt( SENDINFO_ARRAY(m_iWeaponClassAndKills2))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iDamageAndFF2), SendPropInt( SENDINFO_ARRAY(m_iDamageAndFF2))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iShotsFiredAndHit2), SendPropInt( SENDINFO_ARRAY(m_iShotsFiredAndHit2))),

	SendPropArray3	( SENDINFO_ARRAY3(m_iWeaponClassAndKills3), SendPropInt( SENDINFO_ARRAY(m_iWeaponClassAndKills3))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iDamageAndFF3), SendPropInt( SENDINFO_ARRAY(m_iDamageAndFF3))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iShotsFiredAndHit3), SendPropInt( SENDINFO_ARRAY(m_iShotsFiredAndHit3))),

	SendPropArray3	( SENDINFO_ARRAY3(m_iWeaponClassAndKills4), SendPropInt( SENDINFO_ARRAY(m_iWeaponClassAndKills4))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iDamageAndFF4), SendPropInt( SENDINFO_ARRAY(m_iDamageAndFF4))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iShotsFiredAndHit4), SendPropInt( SENDINFO_ARRAY(m_iShotsFiredAndHit4))),

	SendPropArray3	( SENDINFO_ARRAY3(m_iWeaponClassAndKills5), SendPropInt( SENDINFO_ARRAY(m_iWeaponClassAndKills5))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iDamageAndFF5), SendPropInt( SENDINFO_ARRAY(m_iDamageAndFF5))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iShotsFiredAndHit5), SendPropInt( SENDINFO_ARRAY(m_iShotsFiredAndHit5))),

	SendPropArray3	( SENDINFO_ARRAY3(m_iWeaponClassAndKills6), SendPropInt( SENDINFO_ARRAY(m_iWeaponClassAndKills6))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iDamageAndFF6), SendPropInt( SENDINFO_ARRAY(m_iDamageAndFF6))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iShotsFiredAndHit6), SendPropInt( SENDINFO_ARRAY(m_iShotsFiredAndHit6))),

	SendPropArray3	( SENDINFO_ARRAY3(m_iWeaponClassAndKills7), SendPropInt( SENDINFO_ARRAY(m_iWeaponClassAndKills7))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iDamageAndFF7), SendPropInt( SENDINFO_ARRAY(m_iDamageAndFF7))),
	SendPropArray3	( SENDINFO_ARRAY3(m_iShotsFiredAndHit7), SendPropInt( SENDINFO_ARRAY(m_iShotsFiredAndHit7))),

	SendPropFloat(SENDINFO(m_fTimeTaken)),
	SendPropInt(SENDINFO(m_iTotalKills)),
	SendPropInt(SENDINFO(m_iEggKills)),
	SendPropInt(SENDINFO(m_iParasiteKills)),
	SendPropInt(SENDINFO(m_iDroneKills)),
	SendPropInt(SENDINFO(m_iShieldbugKills)),

	SendPropString( SENDINFO( m_DebriefText1 ) ),
	SendPropString( SENDINFO( m_DebriefText2 ) ),
	SendPropString( SENDINFO( m_DebriefText3 ) ),

	SendPropBool( SENDINFO( m_bJustUnlockedCarnage ) ),
	SendPropBool( SENDINFO( m_bJustUnlockedUber ) ),
	SendPropBool( SENDINFO( m_bJustUnlockedHardcore ) ),
	SendPropBool( SENDINFO( m_bBeatSpeedrunTime ) ),

	SendPropFloat(SENDINFO(m_fBestTimeTaken)),
	SendPropInt(SENDINFO(m_iBestKills)),
	SendPropInt(SENDINFO(m_iSpeedrunTime)),
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Debrief_Stats )
END_DATADESC()

CASW_Debrief_Stats::CASW_Debrief_Stats()
{
	//Msg("[S] CASW_Debrief_Stats created\n");
	m_bBeatSpeedrunTime = false;
}

CASW_Debrief_Stats::~CASW_Debrief_Stats()
{
	//Msg("[S] CASW_Debrief_Stats destroyed\n");
}

void CASW_Debrief_Stats::Spawn( void )
{
	BaseClass::Spawn();

	CASW_Debrief_Info* pDebriefInfo = dynamic_cast<CASW_Debrief_Info*>(gEntList.FindEntityByClassname( NULL, "asw_debrief_info" ));
	if (pDebriefInfo)
	{
		Q_strncpy( m_DebriefText1.GetForModify(), STRING( pDebriefInfo->m_DebriefText1 ), 255 );
		Q_strncpy( m_DebriefText2.GetForModify(), STRING( pDebriefInfo->m_DebriefText2 ), 255 );
		Q_strncpy( m_DebriefText3.GetForModify(), STRING( pDebriefInfo->m_DebriefText3 ), 255 );
	}
	else
	{
		Q_snprintf(m_DebriefText1.GetForModify(), 255, "");
		Q_snprintf(m_DebriefText2.GetForModify(), 255, "");
		Q_snprintf(m_DebriefText3.GetForModify(), 255, "");
	}	
}

int CASW_Debrief_Stats::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	return FL_EDICT_ALWAYS;
}

int CASW_Debrief_Stats::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_FULLCHECK );
}

int CASW_Debrief_Stats::GetSkillPointsAwarded(int iProfileIndex)
{
	if (!ASWGameRules())
		return ASW_SKILL_POINTS_PER_MISSION;
	CASW_Game_Resource *pGameResource = ASWGameResource();
	if (!pGameResource)
		return ASW_SKILL_POINTS_PER_MISSION;

	for (int i=0;i<ASW_MAX_MARINE_RESOURCES;i++)
	{
		CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
		if (pMR && pMR->GetProfileIndex() == iProfileIndex)
			return m_iSkillPointsAwarded[i];
	}
	return ASW_SKILL_POINTS_PER_MISSION;
}

void CASW_Debrief_Stats::SetWeaponStats( int nMarineIndex, int nWeaponIndex, int nWeaponClass, int nDamage, int nFFDamage, int nShotsFired, int nShotsHit, int nKills )
{
	// Pack each attribute into 32 bit ints
	unsigned int nClassAndKills = ( (unsigned short)nKills & 0x0000FFFF ) | ( (unsigned short)nWeaponClass << 16 );
	unsigned int nDamageAndFF = ( (unsigned short)nFFDamage & 0x0000FFFF ) | ( (unsigned short)nDamage << 16 );
	unsigned int nShotsFiredAndHit = ( (unsigned short)nShotsHit & 0x0000FFFF ) | ( (unsigned short)nShotsFired << 16 );
	
	switch( nWeaponIndex )
	{
	case 0:
		m_iWeaponClassAndKills0.Set( nMarineIndex, nClassAndKills );
		m_iDamageAndFF0.Set( nMarineIndex, nDamageAndFF );
		m_iShotsFiredAndHit0.Set( nMarineIndex, nShotsFiredAndHit );
		break;

	case 1:
		m_iWeaponClassAndKills1.Set( nMarineIndex, nClassAndKills );
		m_iDamageAndFF1.Set( nMarineIndex, nDamageAndFF );
		m_iShotsFiredAndHit1.Set( nMarineIndex, nShotsFiredAndHit );
		break;

	case 2:
		m_iWeaponClassAndKills2.Set( nMarineIndex, nClassAndKills );
		m_iDamageAndFF2.Set( nMarineIndex, nDamageAndFF );
		m_iShotsFiredAndHit2.Set( nMarineIndex, nShotsFiredAndHit );
		break;

	case 3:
		m_iWeaponClassAndKills3.Set( nMarineIndex, nClassAndKills );
		m_iDamageAndFF3.Set( nMarineIndex, nDamageAndFF );
		m_iShotsFiredAndHit3.Set( nMarineIndex, nShotsFiredAndHit );
		break;

	case 4:
		m_iWeaponClassAndKills4.Set( nMarineIndex, nClassAndKills );
		m_iDamageAndFF4.Set( nMarineIndex, nDamageAndFF );
		m_iShotsFiredAndHit4.Set( nMarineIndex, nShotsFiredAndHit );
		break;

	case 5:
		m_iWeaponClassAndKills5.Set( nMarineIndex, nClassAndKills );
		m_iDamageAndFF5.Set( nMarineIndex, nDamageAndFF );
		m_iShotsFiredAndHit5.Set( nMarineIndex, nShotsFiredAndHit );
		break;

	case 6:
		m_iWeaponClassAndKills6.Set( nMarineIndex, nClassAndKills );
		m_iDamageAndFF6.Set( nMarineIndex, nDamageAndFF );
		m_iShotsFiredAndHit6.Set( nMarineIndex, nShotsFiredAndHit );
		break;

	case 7:
		m_iWeaponClassAndKills7.Set( nMarineIndex, nClassAndKills );
		m_iDamageAndFF7.Set( nMarineIndex, nDamageAndFF );
		m_iShotsFiredAndHit7.Set( nMarineIndex, nShotsFiredAndHit );
		break;

	default:
		return;
	}
}

CASW_Debrief_Stats* GetDebriefStats()
{
	return ASWGameRules() ? ASWGameRules()->m_hDebriefStats.Get() : NULL;
}