#include "cbase.h"
#include "c_asw_debrief_stats.h"
#include "MissionStatsPanel.h"
#include "iclientmode.h"
#include "c_asw_player.h"
#include <vgui_controls/Frame.h>
#include "asw_medal_store.h"
#include "asw_gamerules.h"
#include "c_asw_game_resource.h"
#include "c_asw_marine_resource.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Debrief_Stats, DT_ASW_Debrief_Stats, CASW_Debrief_Stats)
	RecvPropArray3		( RECVINFO_ARRAY(m_iKills), RecvPropInt( RECVINFO(m_iKills[0])) ),
	RecvPropArray3		( RECVINFO_ARRAY(m_fAccuracy), RecvPropFloat( RECVINFO(m_fAccuracy[0])) ),
	RecvPropArray3		( RECVINFO_ARRAY(m_iFF), RecvPropInt( RECVINFO(m_iFF[0])) ),
	RecvPropArray3		( RECVINFO_ARRAY(m_iDamage), RecvPropInt( RECVINFO(m_iDamage[0])) ),
	RecvPropArray3		( RECVINFO_ARRAY(m_iShotsFired), RecvPropInt( RECVINFO(m_iShotsFired[0])) ),
	RecvPropArray3		( RECVINFO_ARRAY(m_iShotsHit), RecvPropInt( RECVINFO(m_iShotsFired[0])) ),
	RecvPropArray3		( RECVINFO_ARRAY(m_iWounded), RecvPropInt( RECVINFO(m_iWounded[0])) ),
	RecvPropArray3		( RECVINFO_ARRAY(m_iAliensBurned), RecvPropInt( RECVINFO(m_iAliensBurned[0])) ),
	RecvPropArray3		( RECVINFO_ARRAY(m_iHealthHealed), RecvPropInt( RECVINFO(m_iHealthHealed[0])) ),
	RecvPropArray3		( RECVINFO_ARRAY(m_iFastHacks), RecvPropInt( RECVINFO(m_iFastHacks[0])) ),
	RecvPropArray3		( RECVINFO_ARRAY(m_iSkillPointsAwarded), RecvPropInt( RECVINFO(m_iSkillPointsAwarded[0])) ),
	RecvPropArray3		( RECVINFO_ARRAY(m_iStartingEquip0), RecvPropInt( RECVINFO(m_iStartingEquip0[0]))),
	RecvPropArray3		( RECVINFO_ARRAY(m_iStartingEquip1), RecvPropInt( RECVINFO(m_iStartingEquip1[0]))),
	RecvPropArray3		( RECVINFO_ARRAY(m_iStartingEquip2), RecvPropInt( RECVINFO(m_iStartingEquip2[0]))),
	RecvPropArray3		( RECVINFO_ARRAY(m_iAmmoDeployed), RecvPropInt( RECVINFO(m_iAmmoDeployed[0]))),
	RecvPropArray3		( RECVINFO_ARRAY(m_iSentryGunsDeployed), RecvPropInt( RECVINFO(m_iSentryGunsDeployed[0]))),
	RecvPropArray3		( RECVINFO_ARRAY(m_iSentryFlamerDeployed), RecvPropInt( RECVINFO(m_iSentryFlamerDeployed[0]))),
	RecvPropArray3		( RECVINFO_ARRAY(m_iSentryFreezeDeployed), RecvPropInt( RECVINFO(m_iSentryFreezeDeployed[0]))),
	RecvPropArray3		( RECVINFO_ARRAY(m_iSentryCannonDeployed), RecvPropInt( RECVINFO(m_iSentryCannonDeployed[0]))),
	RecvPropArray3		( RECVINFO_ARRAY(m_iMedkitsUsed), RecvPropInt( RECVINFO(m_iMedkitsUsed[0]))),
	RecvPropArray3		( RECVINFO_ARRAY(m_iFlaresUsed), RecvPropInt( RECVINFO(m_iFlaresUsed[0]))),
	RecvPropArray3		( RECVINFO_ARRAY(m_iAdrenalineUsed), RecvPropInt( RECVINFO(m_iAdrenalineUsed[0]))),
	RecvPropArray3		( RECVINFO_ARRAY(m_iTeslaTrapsDeployed), RecvPropInt( RECVINFO(m_iTeslaTrapsDeployed[0]))),
	RecvPropArray3		( RECVINFO_ARRAY(m_iFreezeGrenadesThrown), RecvPropInt( RECVINFO(m_iFreezeGrenadesThrown[0]))),
	RecvPropArray3		( RECVINFO_ARRAY(m_iElectricArmorUsed), RecvPropInt( RECVINFO(m_iElectricArmorUsed[0]))),
	RecvPropArray3		( RECVINFO_ARRAY(m_iHealGunHeals), RecvPropInt( RECVINFO(m_iHealGunHeals[0]))),
	RecvPropArray3		( RECVINFO_ARRAY(m_iHealBeaconHeals), RecvPropInt( RECVINFO(m_iHealBeaconHeals[0]))),
	RecvPropArray3		( RECVINFO_ARRAY(m_iHealGunHeals_Self), RecvPropInt( RECVINFO(m_iHealGunHeals_Self[0]))),
	RecvPropArray3		( RECVINFO_ARRAY(m_iHealBeaconHeals_Self), RecvPropInt( RECVINFO(m_iHealBeaconHeals_Self[0]))),
	RecvPropArray3		( RECVINFO_ARRAY(m_iDamageAmpsUsed), RecvPropInt( RECVINFO(m_iDamageAmpsUsed[0]))),
	RecvPropArray3		( RECVINFO_ARRAY(m_iHealBeaconsDeployed), RecvPropInt( RECVINFO(m_iHealBeaconsDeployed[0]))),

 	RecvPropArray3	( RECVINFO_ARRAY(m_iWeaponClassAndKills0), RecvPropInt( RECVINFO(m_iWeaponClassAndKills0[0]))),
 	RecvPropArray3	( RECVINFO_ARRAY(m_iDamageAndFF0), RecvPropInt( RECVINFO(m_iDamageAndFF0[0]))),
	RecvPropArray3	( RECVINFO_ARRAY(m_iShotsFiredAndHit0), RecvPropInt( RECVINFO(m_iShotsFiredAndHit0[0]))),

	RecvPropArray3	( RECVINFO_ARRAY(m_iWeaponClassAndKills1), RecvPropInt( RECVINFO(m_iWeaponClassAndKills1[0]))),
	RecvPropArray3	( RECVINFO_ARRAY(m_iDamageAndFF1), RecvPropInt( RECVINFO(m_iDamageAndFF1[0]))),
	RecvPropArray3	( RECVINFO_ARRAY(m_iShotsFiredAndHit1), RecvPropInt( RECVINFO(m_iShotsFiredAndHit1[0]))),

	RecvPropArray3	( RECVINFO_ARRAY(m_iWeaponClassAndKills2), RecvPropInt( RECVINFO(m_iWeaponClassAndKills2[0]))),
	RecvPropArray3	( RECVINFO_ARRAY(m_iDamageAndFF2), RecvPropInt( RECVINFO(m_iDamageAndFF2[0]))),
	RecvPropArray3	( RECVINFO_ARRAY(m_iShotsFiredAndHit2), RecvPropInt( RECVINFO(m_iShotsFiredAndHit2[0]))),

	RecvPropArray3	( RECVINFO_ARRAY(m_iWeaponClassAndKills3), RecvPropInt( RECVINFO(m_iWeaponClassAndKills3[0]))),
	RecvPropArray3	( RECVINFO_ARRAY(m_iDamageAndFF3), RecvPropInt( RECVINFO(m_iDamageAndFF3[0]))),
	RecvPropArray3	( RECVINFO_ARRAY(m_iShotsFiredAndHit3), RecvPropInt( RECVINFO(m_iShotsFiredAndHit3[0]))),

	RecvPropArray3	( RECVINFO_ARRAY(m_iWeaponClassAndKills4), RecvPropInt( RECVINFO(m_iWeaponClassAndKills4[0]))),
	RecvPropArray3	( RECVINFO_ARRAY(m_iDamageAndFF4), RecvPropInt( RECVINFO(m_iDamageAndFF4[0]))),
	RecvPropArray3	( RECVINFO_ARRAY(m_iShotsFiredAndHit4), RecvPropInt( RECVINFO(m_iShotsFiredAndHit4[0]))),

	RecvPropArray3	( RECVINFO_ARRAY(m_iWeaponClassAndKills5), RecvPropInt( RECVINFO(m_iWeaponClassAndKills5[0]))),
	RecvPropArray3	( RECVINFO_ARRAY(m_iDamageAndFF5), RecvPropInt( RECVINFO(m_iDamageAndFF5[0]))),
	RecvPropArray3	( RECVINFO_ARRAY(m_iShotsFiredAndHit5), RecvPropInt( RECVINFO(m_iShotsFiredAndHit5[0]))),

	RecvPropArray3	( RECVINFO_ARRAY(m_iWeaponClassAndKills6), RecvPropInt( RECVINFO(m_iWeaponClassAndKills6[0]))),
	RecvPropArray3	( RECVINFO_ARRAY(m_iDamageAndFF6), RecvPropInt( RECVINFO(m_iDamageAndFF6[0]))),
	RecvPropArray3	( RECVINFO_ARRAY(m_iShotsFiredAndHit6), RecvPropInt( RECVINFO(m_iShotsFiredAndHit6[0]))),

	RecvPropArray3	( RECVINFO_ARRAY(m_iWeaponClassAndKills7), RecvPropInt( RECVINFO(m_iWeaponClassAndKills7[0]))),
	RecvPropArray3	( RECVINFO_ARRAY(m_iDamageAndFF7), RecvPropInt( RECVINFO(m_iDamageAndFF7[0]))),
	RecvPropArray3	( RECVINFO_ARRAY(m_iShotsFiredAndHit7), RecvPropInt( RECVINFO(m_iShotsFiredAndHit7[0]))),

	RecvPropFloat(RECVINFO(m_fTimeTaken)),
	RecvPropInt(RECVINFO(m_iTotalKills)),
	RecvPropInt(RECVINFO(m_iEggKills)),
	RecvPropInt(RECVINFO(m_iParasiteKills)),
	RecvPropInt(RECVINFO(m_iDroneKills)),
	RecvPropInt(RECVINFO(m_iShieldbugKills)),

	RecvPropString( RECVINFO( m_DebriefText1 ) ),
	RecvPropString( RECVINFO( m_DebriefText2 ) ),
	RecvPropString( RECVINFO( m_DebriefText3 ) ),

	RecvPropBool( RECVINFO( m_bJustUnlockedCarnage ) ),
	RecvPropBool( RECVINFO( m_bJustUnlockedUber ) ),
	RecvPropBool( RECVINFO( m_bJustUnlockedHardcore ) ),
	RecvPropBool( RECVINFO( m_bBeatSpeedrunTime ) ),

	RecvPropFloat(RECVINFO(m_fBestTimeTaken)),
	RecvPropInt(RECVINFO(m_iBestKills)),
	RecvPropInt(RECVINFO(m_iSpeedrunTime)),
END_RECV_TABLE()

C_ASW_Debrief_Stats *g_pDebriefStats = NULL;
C_ASW_Debrief_Stats* GetDebriefStats() { return g_pDebriefStats; }

C_ASW_Debrief_Stats::C_ASW_Debrief_Stats()
{
	m_bCreated = false;
	g_pDebriefStats = this;
}

C_ASW_Debrief_Stats::~C_ASW_Debrief_Stats()
{
	if ( g_pDebriefStats == this )
	{
		g_pDebriefStats = NULL;
	}
}


void C_ASW_Debrief_Stats::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );
	if ( type == DATA_UPDATE_CREATED )
	{
		m_bCreated = true;

		// notify the debrief stats page that all data is here and it should start counting numbers/bars up		
		HACK_GETLOCALPLAYER_GUARD( "MissionCompleteFrame needs to be a child of the main client .dll viewport (now a parent to both client mode viewports)" );
		MissionStatsPanel *pStatsPanel = dynamic_cast<MissionStatsPanel*>(GetClientMode()->GetViewport()->FindChildByName("MissionStatsPanel", true));		

		if (pStatsPanel)
		{
			pStatsPanel->InitFrom(this);
		}

		// update our kill counts
#ifdef USE_MEDAL_STORE
		if (GetMedalStore() && ASWGameRules() && ASWGameResource() && !ASWGameRules()->m_bCheated
			&& !engine->IsPlayingDemo())
		{
			C_ASW_Game_Resource *pGameResource = ASWGameResource();
			C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
			int iMissions = ASWGameRules()->GetMissionSuccess() ? 1 : 0;		// award 1 extra mission if it was a success
			int iKills = 0;
			// go through each marine belonging to the local player and increment kills
			for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
			{
				C_ASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
				if (pMR && pMR->GetCommanderIndex() == pPlayer->entindex())
					iKills += m_iKills[i];
			}
			if (iKills > 0)	// only increment their counts if they actually killed something
				GetMedalStore()->OnIncreaseCounts(iMissions, 0, iKills, (gpGlobals->maxClients <= 1));
		}
#endif
	}
// 	Msg( "Debrief stats data changed\n" );
// 	
// 	for ( int i = 0; i < ASW_MAX_MARINE_RESOURCES; i++ )
// 	{
// 		Msg( "health healed[%d] = %d", i, GetHealthHealed( i ) );
// 	}
}



int C_ASW_Debrief_Stats::GetHighestKills()
{
	int best = 0;
	for (int i=0;i<ASW_MAX_MARINE_RESOURCES;i++)
	{
		int k = GetKills(i);
		if (k > best)
		{
			best = k;
		}
	}
	return best;
}

float C_ASW_Debrief_Stats::GetHighestAccuracy()
{
	float best = 0;
	for (int i=0;i<ASW_MAX_MARINE_RESOURCES;i++)
	{
		float k = GetAccuracy(i);
		if (k > best)
		{
			best = k;
		}
	}
	return best;
}

int C_ASW_Debrief_Stats::GetHighestFriendlyFire()
{
	int best = 0;
	for (int i=0;i<ASW_MAX_MARINE_RESOURCES;i++)
	{
		int k = GetFriendlyFire(i);
		if (k != 0 && (k > best || best == 0))
		{
			best = k;
		}
	}
	return best;
}

int C_ASW_Debrief_Stats::GetHighestDamageTaken()
{
	int best = 0;
	for (int i=0;i<ASW_MAX_MARINE_RESOURCES;i++)
	{
		int k = GetDamageTaken(i);
		if (k != 0 && (k > best || best == 0))
		{
			best = k;
		}
	}
	return best;
}

int C_ASW_Debrief_Stats::GetHighestShotsFired()
{
	int best = 0;
	for (int i=0;i<ASW_MAX_MARINE_RESOURCES;i++)
	{
		int k = GetShotsFired(i);
		if (k != 0 && (k > best || best == 0))
		{
			best = k;
		}
	}
	return best;
}

int C_ASW_Debrief_Stats::GetHighestAliensBurned()
{
	int best = 0;
	for (int i=0;i<ASW_MAX_MARINE_RESOURCES;i++)
	{
		int k = GetAliensBurned(i);
		if (k != 0 && (k > best || best == 0))
		{
			best = k;
		}
	}
	return best;
}

int C_ASW_Debrief_Stats::GetHighestHealthHealed()
{
	int best = 0;
	for (int i=0;i<ASW_MAX_MARINE_RESOURCES;i++)
	{
		int k = GetHealthHealed(i);
		if (k != 0 && (k > best || best == 0))
		{
			best = k;
		}
	}
	return best;
}

int C_ASW_Debrief_Stats::GetHighestFastHacks()
{
	int best = 0;
	for (int i=0;i<ASW_MAX_MARINE_RESOURCES;i++)
	{
		int k = GetFastHacks(i);
		if (k != 0 && (k > best || best == 0))
		{
			best = k;
		}
	}
	return best;
}

int C_ASW_Debrief_Stats::GetHighestSkillPointsAwarded()
{
	int best = 0;
	for (int i=0;i<ASW_MAX_MARINE_RESOURCES;i++)
	{
		int k = GetSkillPointsAwarded(i);
		if (k != 0 && (k > best || best == 0))
		{
			best = k;
		}
	}
	return best;
}

int C_ASW_Debrief_Stats::GetLowestFriendlyFire()
{
	int best = 0;
	for (int i=0;i<ASW_MAX_MARINE_RESOURCES;i++)
	{
		int k = GetFriendlyFire(i);
		if (k != 0 && (k < best || best == 0))
		{
			best = k;
		}
	}
	return best;
}

int C_ASW_Debrief_Stats::GetLowestDamageTaken()
{
	int best = 0;
	for (int i=0;i<ASW_MAX_MARINE_RESOURCES;i++)
	{
		int k = GetDamageTaken(i);
		if (k != 0 && (k < best || best == 0))
		{
			best = k;
		}
	}
	return best;
}

bool C_ASW_Debrief_Stats::GetWeaponStats( int iMarineIndex, int iEquipIndex, int &iDamage, int &iFFDamage, int &iShotsFired, int &iShotsHit, int &iKills )
{
 	if( (unsigned)iEquipIndex == ( ( m_iWeaponClassAndKills0.Get( iMarineIndex ) >> 16 ) & 0x0000FFFF ) )
 	{
 		iDamage = ( m_iDamageAndFF0.Get( iMarineIndex ) >> 16 ) & 0x0000FFFF;
 		iFFDamage = m_iDamageAndFF0.Get( iMarineIndex ) & 0x0000FFFF;
 		iShotsFired = ( m_iShotsFiredAndHit0.Get( iMarineIndex ) >> 16 ) & 0x0000FFFF;
 		iShotsHit = m_iShotsFiredAndHit0.Get( iMarineIndex ) & 0x0000FFFF;
 		iKills = m_iWeaponClassAndKills0.Get( iMarineIndex ) & 0x0000FFFF;
 		return true;
	}
	else if( (unsigned)iEquipIndex == ( ( m_iWeaponClassAndKills1.Get( iMarineIndex ) >> 16 ) & 0x0000FFFF ) )
	{
		iDamage = ( m_iDamageAndFF1.Get( iMarineIndex ) >> 16 ) & 0x0000FFFF;
		iFFDamage = m_iDamageAndFF1.Get( iMarineIndex ) & 0x0000FFFF;
		iShotsFired = ( m_iShotsFiredAndHit1.Get( iMarineIndex ) >> 16 ) & 0x0000FFFF;
		iShotsHit = m_iShotsFiredAndHit1.Get( iMarineIndex ) & 0x0000FFFF;
		iKills = m_iWeaponClassAndKills1.Get( iMarineIndex ) & 0x0000FFFF;
		return true;
	}
	else if( (unsigned)iEquipIndex == ( ( m_iWeaponClassAndKills2.Get( iMarineIndex ) >> 16 ) & 0x0000FFFF ) )
	{
		iDamage = ( m_iDamageAndFF2.Get( iMarineIndex ) >> 16 ) & 0x0000FFFF;
		iFFDamage = m_iDamageAndFF2.Get( iMarineIndex ) & 0x0000FFFF;
		iShotsFired = ( m_iShotsFiredAndHit2.Get( iMarineIndex ) >> 16 ) & 0x0000FFFF;
		iShotsHit = m_iShotsFiredAndHit2.Get( iMarineIndex ) & 0x0000FFFF;
		iKills = m_iWeaponClassAndKills2.Get( iMarineIndex ) & 0x0000FFFF;
		return true;
	}
	else if( (unsigned)iEquipIndex == ( ( m_iWeaponClassAndKills3.Get( iMarineIndex ) >> 16 ) & 0x0000FFFF ) )
	{
		iDamage = ( m_iDamageAndFF3.Get( iMarineIndex ) >> 16 ) & 0x0000FFFF;
		iFFDamage = m_iDamageAndFF3.Get( iMarineIndex ) & 0x0000FFFF;
		iShotsFired = ( m_iShotsFiredAndHit3.Get( iMarineIndex ) >> 16 ) & 0x0000FFFF;
		iShotsHit = m_iShotsFiredAndHit3.Get( iMarineIndex ) & 0x0000FFFF;
		iKills = m_iWeaponClassAndKills3.Get( iMarineIndex ) & 0x0000FFFF;
		return true;
	}
	else if( (unsigned)iEquipIndex == ( ( m_iWeaponClassAndKills4.Get( iMarineIndex ) >> 16 ) & 0x0000FFFF ) )
	{
		iDamage = ( m_iDamageAndFF4.Get( iMarineIndex ) >> 16 ) & 0x0000FFFF;
		iFFDamage = m_iDamageAndFF4.Get( iMarineIndex ) & 0x0000FFFF;
		iShotsFired = ( m_iShotsFiredAndHit4.Get( iMarineIndex ) >> 16 ) & 0x0000FFFF;
		iShotsHit = m_iShotsFiredAndHit4.Get( iMarineIndex ) & 0x0000FFFF;
		iKills = m_iWeaponClassAndKills4.Get( iMarineIndex ) & 0x0000FFFF;
		return true;
	}
	else if( (unsigned)iEquipIndex == ( ( m_iWeaponClassAndKills5.Get( iMarineIndex ) >> 16 ) & 0x0000FFFF ) )
	{
		iDamage = ( m_iDamageAndFF5.Get( iMarineIndex ) >> 16 ) & 0x0000FFFF;
		iFFDamage = m_iDamageAndFF5.Get( iMarineIndex ) & 0x0000FFFF;
		iShotsFired = ( m_iShotsFiredAndHit5.Get( iMarineIndex ) >> 16 ) & 0x0000FFFF;
		iShotsHit = m_iShotsFiredAndHit5.Get( iMarineIndex ) & 0x0000FFFF;
		iKills = m_iWeaponClassAndKills5.Get( iMarineIndex ) & 0x0000FFFF;
		return true;
	}
	else if( (unsigned)iEquipIndex == ( ( m_iWeaponClassAndKills6.Get( iMarineIndex ) >> 16 ) & 0x0000FFFF ) )
	{
		iDamage = ( m_iDamageAndFF6.Get( iMarineIndex ) >> 16 ) & 0x0000FFFF;
		iFFDamage = m_iDamageAndFF6.Get( iMarineIndex ) & 0x0000FFFF;
		iShotsFired = ( m_iShotsFiredAndHit6.Get( iMarineIndex ) >> 16 ) & 0x0000FFFF;
		iShotsHit = m_iShotsFiredAndHit6.Get( iMarineIndex ) & 0x0000FFFF;
		iKills = m_iWeaponClassAndKills6.Get( iMarineIndex ) & 0x0000FFFF;
		return true;
	}
	else if( (unsigned)iEquipIndex == ( ( m_iWeaponClassAndKills7.Get( iMarineIndex ) >> 16 ) & 0x0000FFFF ) )
	{
		iDamage = ( m_iDamageAndFF7.Get( iMarineIndex ) >> 16 ) & 0x0000FFFF;
		iFFDamage = m_iDamageAndFF7.Get( iMarineIndex ) & 0x0000FFFF;
		iShotsFired = ( m_iShotsFiredAndHit7.Get( iMarineIndex ) >> 16 ) & 0x0000FFFF;
		iShotsHit = m_iShotsFiredAndHit7.Get( iMarineIndex ) & 0x0000FFFF;
		iKills = m_iWeaponClassAndKills7.Get( iMarineIndex ) & 0x0000FFFF;
		return true;
	}
	else
		return false;
}