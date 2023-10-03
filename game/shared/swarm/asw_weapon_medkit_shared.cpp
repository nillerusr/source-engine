#include "cbase.h"
#include "asw_weapon_medkit_shared.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "c_asw_player.h"
#include "c_asw_weapon.h"
#include "c_asw_marine.h"
#else
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_weapon.h"
#include "npcevent.h"
#include "asw_gamerules.h"
#include "asw_triggers.h"
#endif
#include "asw_marine_profile.h"
#include "asw_marine_skills.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define MEDKIT_HEAL_AMOUNT 50

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Medkit, DT_ASW_Weapon_Medkit )

BEGIN_NETWORK_TABLE( CASW_Weapon_Medkit, DT_ASW_Weapon_Medkit )
#ifdef CLIENT_DLL
	// recvprops
#else
	// sendprops
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CASW_Weapon_Medkit )
	
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( asw_weapon_medkit, CASW_Weapon_Medkit );
PRECACHE_WEAPON_REGISTER(asw_weapon_medkit);

#ifndef CLIENT_DLL

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Weapon_Medkit )
	
END_DATADESC()

#endif /* not client */

CASW_Weapon_Medkit::CASW_Weapon_Medkit()
{
	m_fMinRange1	= 0;
	m_fMaxRange1	= 2048;

	m_fMinRange2	= 256;
	m_fMaxRange2	= 1024;
}


CASW_Weapon_Medkit::~CASW_Weapon_Medkit()
{

}


bool CASW_Weapon_Medkit::OffhandActivate()
{
	if (!GetMarine() || GetMarine()->GetFlags() & FL_FROZEN)	// don't allow this if the marine is frozen
		return false;
	SelfHeal();

	return true;
}
void CASW_Weapon_Medkit::SelfHeal()
{
	CASW_Marine *pMarine = GetMarine();

	if (pMarine)		// firing from a marine
	{
		if (pMarine->GetHealth() >= pMarine->GetMaxHealth())		// already on full health
			return;

		if (pMarine->GetHealth() <= 0)		// aleady dead!
			return;

		if (pMarine->m_bSlowHeal)			// already healing
			return;

		if (pMarine->GetFlags() & FL_FROZEN)	// don't allow this if the marine is frozen
			return;

		// MUST call sound before removing a round from the clip of a CMachineGun
		WeaponSound(SINGLE);

		// sets the animation on the weapon model iteself
		SendWeaponAnim( GetPrimaryAttackActivity() );

		// sets the animation on the marine holding this weapon
		//pMarine->SetAnimation( PLAYER_ATTACK1 );
#ifndef CLIENT_DLL
		bool bMedic = (pMarine->GetMarineProfile() && pMarine->GetMarineProfile()->CanUseFirstAid());
		// put a slow heal onto the marine, play a particle effect
		if (!pMarine->m_bSlowHeal && pMarine->GetHealth() < pMarine->GetMaxHealth())
		{
			pMarine->AddSlowHeal( GetHealAmount(), 1, pMarine, this );

			// Fire event
			IGameEvent * event = gameeventmanager->CreateEvent( "player_heal" );
			if ( event )
			{
				CASW_Player *pPlayer = GetCommander();
				event->SetInt( "userid", ( pPlayer ? pPlayer->GetUserID() : 0 ) );
				event->SetInt( "entindex", pMarine->entindex() );
				gameeventmanager->FireEvent( event );
			}

			if ( ASWGameRules()->GetInfoHeal() )
			{
				ASWGameRules()->GetInfoHeal()->OnMarineHealed( pMarine, pMarine, this );

			}
			pMarine->OnWeaponFired( this, 1 );
		}
		
		if (pMarine->IsInfested() && bMedic)
		{
			float fCure = GetInfestationCureAmount();
			// cure infestation
			if (fCure < 100)
				pMarine->CureInfestation(pMarine, fCure);
		}
#endif
		// decrement ammo
		m_iClip1 -= 1;

#ifndef CLIENT_DLL
		DestroyIfEmpty( false );
#endif
	}
}

int CASW_Weapon_Medkit::GetHealAmount()
{
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
		return 0;

	// medics adjust heal amount by their skills
	if ( pMarine->GetMarineProfile() && pMarine->GetMarineProfile()->CanUseFirstAid() )
	{
		return MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_HEALING, ASW_MARINE_SUBSKILL_HEALING_MEDKIT_HPS);
	}

	return MEDKIT_HEAL_AMOUNT;
}
