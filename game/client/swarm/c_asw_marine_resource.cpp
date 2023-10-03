#include "cbase.h"
#include "asw_gamerules.h"
#include "c_asw_marine_resource.h"
#include "asw_marine_profile.h"
#include "c_asw_marine.h"
#include "c_asw_player.h"
#include "c_asw_weapon.h"
#include "asw_weapon_medical_satchel_shared.h"
#include "c_asw_door.h"
#include "c_playerresource.h"
#include <vgui/ILocalize.h>
#include "clientmode_shared.h"
#include "nb_main_panel.h"
#include "c_asw_campaign_save.h"
#include "c_asw_game_resource.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#undef CASW_Marine_Resource
#define CASW_Marine_Resource CASW_Marine_Resource

BEGIN_RECV_TABLE_NOBASE( C_ASW_Marine_Resource, DT_MR_Timelines )
	RecvPropDataTable( RECVINFO_DT( m_TimelineFriendlyFire ), 0, &REFERENCE_RECV_TABLE(DT_Timeline) ),
	RecvPropDataTable( RECVINFO_DT( m_TimelineKillsTotal ), 0, &REFERENCE_RECV_TABLE(DT_Timeline) ),
	RecvPropDataTable( RECVINFO_DT( m_TimelineHealth ), 0, &REFERENCE_RECV_TABLE(DT_Timeline) ),
	RecvPropDataTable( RECVINFO_DT( m_TimelineAmmo ), 0, &REFERENCE_RECV_TABLE(DT_Timeline) ),
	RecvPropDataTable( RECVINFO_DT( m_TimelinePosX ), 0, &REFERENCE_RECV_TABLE(DT_Timeline) ),
	RecvPropDataTable( RECVINFO_DT( m_TimelinePosY ), 0, &REFERENCE_RECV_TABLE(DT_Timeline) ),
END_RECV_TABLE();

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Marine_Resource, DT_ASW_Marine_Resource, CASW_Marine_Resource)
	RecvPropDataTable( "mr_timelines", 0, 0, &REFERENCE_RECV_TABLE(DT_MR_Timelines) ),
	RecvPropInt		(RECVINFO(m_MarineProfileIndex)),
	RecvPropEHandle (RECVINFO(m_MarineEntity)),
	RecvPropEHandle (RECVINFO(m_Commander)),
	RecvPropInt		(RECVINFO(m_iCommanderIndex)),
	RecvPropArray	( RecvPropInt( RECVINFO(m_iWeaponsInSlots[0]), 30 ), m_iWeaponsInSlots ),
	RecvPropBool	(RECVINFO(m_bInfested) ),
	RecvPropBool	(RECVINFO(m_bInhabited) ),
	RecvPropInt		(RECVINFO(m_iServerFiring) ),
	//RecvPropFloat		(RECVINFO(m_fDamageTaken) ),
	RecvPropInt		(RECVINFO(m_iAliensKilled), 16 ),
	RecvPropBool	(RECVINFO(m_bTakenWoundDamage) ),
	RecvPropBool	(RECVINFO(m_bHealthHalved) ),
	RecvPropString	(RECVINFO(m_MedalsAwarded) ),
	RecvPropEHandle	(RECVINFO(m_hWeldingDoor)),
	RecvPropBool	(RECVINFO(m_bUsingEngineeringAura) ),
END_RECV_TABLE()

extern ConVar asw_leadership_radius;
extern ConVar asw_skill_healing_charges_base;
extern ConVar asw_skill_healing_charges_step;

C_ASW_Marine_Resource::C_ASW_Marine_Resource()
{
	m_MarineProfileIndex = -1;
	m_iScannerSoundSkip = 3;
	m_fScannerTime = RandomFloat(1.0f, 2.5f);
	m_bPlayedBlipSound = false;
	m_fFiring = 0;
	m_fLastHealthPercent = 0;
	m_fHurtPulse = 0;
	m_bTakenWoundDamage = 0;
	m_fNextLeadershipTest = 0;
	m_fLeadershipResist = 0;
	m_iServerFiring = 0;
	m_fNextMedsCountTime = 0;	
	m_fCachedMedsPercent = 0;
	m_MedalsAwarded[0] = '\0';
	m_bUsingEngineeringAura = false;
	m_Commander = NULL;
	m_iCommanderIndex = -1;
	memset( m_iWeaponsInSlots, -1, sizeof( m_iWeaponsInSlots ) );
}

C_ASW_Marine_Resource::~C_ASW_Marine_Resource()
{

}

CASW_Marine_Profile* C_ASW_Marine_Resource::GetProfile(void)
{
	if (MarineProfileList() == NULL)
		return NULL;

	if (m_MarineProfileIndex >=0 && m_MarineProfileIndex < MarineProfileList()->m_NumProfiles)
		return MarineProfileList()->m_Profiles[m_MarineProfileIndex];

	return NULL;
}

C_ASW_Marine* C_ASW_Marine_Resource::GetMarineEntity()
{
	C_BaseEntity* base = m_MarineEntity;
	C_ASW_Marine* marine = (C_ASW_Marine*) base;
	return marine;
}

C_ASW_Player* C_ASW_Marine_Resource::GetCommander()
{
	return dynamic_cast<C_ASW_Player*>(m_Commander.Get());
}

void C_ASW_Marine_Resource::GetDisplayName( char *pchDisplayName, int nMaxBytes )
{
	wchar_t wszDisplayName[ 256 ];
	GetDisplayName( wszDisplayName, sizeof( wszDisplayName ) );

	g_pVGuiLocalize->ConvertUnicodeToANSI( wszDisplayName, pchDisplayName, nMaxBytes );
}

void C_ASW_Marine_Resource::GetDisplayName( wchar_t *pwchDisplayName, int nMaxBytes )
{
	bool bPlayerName = false;
	const char *pchName = NULL;

	if ( gpGlobals->maxClients <= 1 )
	{
		// Always use the character name in singleplayer
		pchName = GetProfile()->GetShortName();
	}
	else
	{
		bool bIsInhabited = IsInhabited();

		if ( bIsInhabited && GetCommander() && g_PR->IsConnected( m_iCommanderIndex ) )
		{
			// Use the player name
			pchName = g_PR->GetPlayerName( m_iCommanderIndex );
			bPlayerName = true;
		}
		else
		{
			// Use the character name
			pchName = GetProfile()->GetShortName();
		}
	}

	const wchar_t *pwchLocalizedMarineName = NULL;

	if ( !bPlayerName )
	{
		// Find the localized character name
		pwchLocalizedMarineName = g_pVGuiLocalize->Find( pchName );
	}

	if ( pwchLocalizedMarineName )
	{
		// Copy the localized name
		Q_wcsncpy( pwchDisplayName, pwchLocalizedMarineName, nMaxBytes );
	}
	else
	{
		// Copy the name
		g_pVGuiLocalize->ConvertANSIToUnicode( pchName, pwchDisplayName, nMaxBytes );
	}
}

// accessors for the HUD portraits
//   NOTE: Currently we're assuming marine entities are always transmitted to clients
//         and this code relies on that.  In the future we may want to instead periodically transmit this info
//         through this entity and not rely on the marine entity itself.

float C_ASW_Marine_Resource::GetHealthPercent()
{	
	C_ASW_Marine *marine = GetMarineEntity();
	if (!marine)
		return 0;

	float max_health = marine->GetMaxHealth();
	if ( max_health <= 0 )
		return 0;

	float health = marine->GetHealth();

	return clamp(health / max_health, 0.0f, 1.0f);
}

float C_ASW_Marine_Resource::GetInfestedPercent()
{
	C_ASW_Marine *marine = GetMarineEntity();
	if (!marine || !ASWGameRules() || !IsInfested())
		return 0;

	float max_health = marine->GetMaxHealth();
	float fInfestedDamage = (ASWGameRules()->TotalInfestDamage() / 20.0f) * marine->m_fInfestedTime;

	fInfestedDamage *= MIN( gpGlobals->curtime - marine->m_fInfestedStartTime, 15.0f ) / 15.0f;
	float fPercent = fInfestedDamage / max_health;

	return fPercent * fPercent;
}

float C_ASW_Marine_Resource::GetAmmoPercent()
{
	C_ASW_Marine *marine = GetMarineEntity();
	if (!marine)
		return 0;

	C_ASW_Weapon *weapon = marine->GetActiveASWWeapon();
	if (!weapon)
		return 0;

	float max_ammo = weapon->GetMaxClip1();
	float ammo = weapon->Clip1();   // ammo inside the gun

	return clamp( ammo / max_ammo, 0.0f, 1.0f );
}

float C_ASW_Marine_Resource::GetClipsPercent()
{
	C_ASW_Marine *marine = GetMarineEntity();
	if (!marine)
		return 0;

	C_ASW_Weapon *weapon = marine->GetActiveASWWeapon();
	if (!weapon)
		return 0;

	float max_clips = 8;
	int clips = marine->GetAmmoCount(weapon->GetPrimaryAmmoType());   // ammo the marine is carrying outside the gun
	clips /= weapon->GetMaxClip1();		// divide it down to get the number of clips

	float fResult = clips / max_clips;
	if (fResult > 1)
		fResult = 1.0f;

	return fResult;
}

float C_ASW_Marine_Resource::GetMedsPercent()
{
	if (gpGlobals->curtime < m_fNextMedsCountTime)
	{
		return m_fCachedMedsPercent;
	}
	C_ASW_Marine *pMarine = GetMarineEntity();
	if ( !pMarine )
		return 0;

	int meds = 0;
	for ( int i = 0; i < ASW_NUM_INVENTORY_SLOTS; i++ )
	{
		C_BaseCombatWeapon *pWeapon = pMarine->GetWeapon( i );
		if ( pWeapon && ( pWeapon->Classify() == CLASS_ASW_MEDICAL_SATCHEL 
				|| pWeapon->Classify() == CLASS_ASW_HEALGRENADE ) )
		{
			meds+= pWeapon->Clip1();
		}
	}

	float max_meds = asw_skill_healing_charges_base.GetInt() + ( asw_skill_healing_charges_step.GetInt() * 5 );
	
	m_fCachedMedsPercent = float( meds ) / float( max_meds );
	if ( m_fCachedMedsPercent > 1 )
		m_fCachedMedsPercent = 1.0f;

	m_fNextMedsCountTime = gpGlobals->curtime + 0.5f;

	return m_fCachedMedsPercent;
}

bool C_ASW_Marine_Resource::IsFiring()
{
	// if we can directly see the marine and his weapon, then check accurately if he's firing
	//if (GetMarineEntity() && GetMarineEntity()->GetActiveASWWeapon() && !GetMarineEntity()->GetActiveASWWeapon()->IsDormant())
	//{
		//return GetMarineEntity()->GetActiveASWWeapon()->IsFiring();
	//}
	// if not, then see what the server says
	return (m_iServerFiring > 0);
}

// countdown timer for our firing
float C_ASW_Marine_Resource::GetFiringTimer()
{
	return m_fFiring;
}

void C_ASW_Marine_Resource::OnDataChanged(DataUpdateType_t updateType)
{
	if ( updateType == DATA_UPDATE_CREATED )
	{
		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );

			C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
			if (pPlayer && pPlayer == GetCommander() && ASWGameRules() && ASWGameRules()->GetGameState() <= ASW_GS_BRIEFING)
			{
				// we're in briefing and a marine has just been selected by us (possibly autoselection)
				// send our last saved loadout
				pPlayer->LoadoutSendStored(this);

				// see if we need to spend skill points
				if ( ASWGameRules()->IsCampaignGame() && ASWGameRules()->GetCampaignSave() && !ASWGameRules()->GetCampaignSave()->UsingFixedSkillPoints() 
					&& ASWGameResource() && ASWGameResource()->GetMarineSkill( this, ASW_SKILL_SLOT_SPARE ) > 0 )
				{
					CNB_Main_Panel::QueueSpendSkillPoints( GetProfileIndex() );
				}
			}
		}
		SetNextClientThink(gpGlobals->curtime);
	}
	BaseClass::OnDataChanged(updateType);
}

void C_ASW_Marine_Resource::ClientThink()
{
	if (GetHealthPercent() <= 0)
	{
		m_fFiring = 0;
	}
	else
	{
		if (m_iServerFiring == 2)	// firing a slow firing shotgun/pistol type weapon
		{
			float fFireScale = 1.0f;

			//C_ASW_Marine *pMarine = GetMarineEntity();
			//if (pMarine)
			//{
				//C_ASW_Weapon *pWeapon = pMarine->GetActiveASWWeapon();
				//if (pWeapon)
				//{
					//float fRate = pWeapon->GetFireRate();

					//// scale our rate of decay according to the firing rate of the gun
					//if (fRate > 0)
						//fFireScale = 0.8f / fRate;
				//}
			//}
			
			if (m_fFiring > 0)
			{
				m_fFiring = MAX(m_fFiring - gpGlobals->frametime * fFireScale, 0);	//0.8 seconds to fade out with firescale of 1
			}

			if (m_fFiring <= 0)
				m_fFiring += 0.8;
		}
		else
		{
			if (m_fFiring > 0)
			{
				m_fFiring = MAX(m_fFiring - gpGlobals->frametime, 0);
			}

			if (m_iServerFiring == 1)	// firing a rapid fire machine gun type weapon
			{
				m_fFiring = random->RandomFloat(0.5, 1.0f);
			}
		}		
	}
	// tick down a value used for flashing the marine red when hurt
	if (m_fHurtPulse > 0)
	{
		m_fHurtPulse -= gpGlobals->frametime * 2;
		if (m_fHurtPulse < 0)
			m_fHurtPulse = 0;
	}

	float flHealth = GetHealthPercent();
	if ( flHealth < m_fLastHealthPercent )
	{
		m_fHurtPulse = 1.0f;
	}
	m_fLastHealthPercent = flHealth;

	if (gpGlobals->curtime > m_fNextLeadershipTest)
	{
		UpdateLeadershipBonus();
	}

	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

void C_ASW_Marine_Resource::UpdateLeadershipBonus()
{
	if (GetMarineEntity() && GetHealthPercent() > 0)
	{
		m_fLeadershipResist = MarineSkills()->GetHighestSkillValueNearby(GetMarineEntity()->GetAbsOrigin(),
			asw_leadership_radius.GetFloat(),
				ASW_MARINE_SKILL_LEADERSHIP, ASW_MARINE_SUBSKILL_LEADERSHIP_DAMAGE_RESIST);		
		m_fNextLeadershipTest = gpGlobals->curtime + 0.5f;
	}
	else
	{
		m_fNextLeadershipTest = gpGlobals->curtime + 2.0f;
	}
}

// note: assumes marine entity is networked to all clients...
bool C_ASW_Marine_Resource::IsReloading()
{
	if (!GetMarineEntity())
		return false;

	C_ASW_Weapon *pWeapon = GetMarineEntity()->GetActiveASWWeapon();
	if (!pWeapon)
		return false;

	return pWeapon->IsReloading();
}