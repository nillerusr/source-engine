#include "cbase.h"
#include "asw_weapon_medical_satchel_shared.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
#include "c_asw_player.h"
#include "c_asw_weapon.h"
#include "c_asw_marine.h"
#include "c_asw_game_resource.h"
#include "c_asw_marine_resource.h"
#include "asw_marine_profile.h"
#include "asw_hud_crosshair.h"
#include "asw_input.h"
#else
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_weapon.h"
#include "asw_marine_resource.h"
#include "npcevent.h"
#include "asw_sentry_base.h"
#include "asw_gamerules.h"
#include "asw_fail_advice.h"
#include "asw_marine_speech.h"
#include "asw_triggers.h"
#include "asw_marine_profile.h"
#endif
#include "asw_marine_skills.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define MEDSATCHEL_HEAL_AMOUNT 30
#define MAX_HEAL_DISTANCE 200
#define MAX_HEAL_DISTANCE_SERVER 300
#define THROW_INTERVAL 1.0				// interval between heal applications

IMPLEMENT_NETWORKCLASS_ALIASED( ASW_Weapon_Medical_Satchel, DT_ASW_Weapon_Medical_Satchel )

BEGIN_NETWORK_TABLE( CASW_Weapon_Medical_Satchel, DT_ASW_Weapon_Medical_Satchel )
#ifdef CLIENT_DLL
	// recvprops
#else
	// sendprops
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CASW_Weapon_Medical_Satchel )
	
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( asw_weapon_medical_satchel, CASW_Weapon_Medical_Satchel );
PRECACHE_WEAPON_REGISTER(asw_weapon_medical_satchel);

#ifndef CLIENT_DLL

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CASW_Weapon_Medical_Satchel )
END_DATADESC()

ConVar asw_marine_special_heal_chatter_chance("asw_marine_special_heal_chatter_chance", "0.05", 0, "Chance of medic saying a special healing line.  Wounded special chance is this times 5.");

#endif /* not client */

CASW_Weapon_Medical_Satchel::CASW_Weapon_Medical_Satchel()
{
	m_fMinRange1	= 0;
	m_fMaxRange1	= 2048;

	m_fMinRange2	= 256;
	m_fMaxRange2	= 1024;
}

Activity CASW_Weapon_Medical_Satchel::GetPrimaryAttackActivity( void )
{
	return ACT_VM_PRIMARYATTACK;
}

void CASW_Weapon_Medical_Satchel::PrimaryAttack( void )
{
	// if mouse over entity is a marine
	GiveHealth();
}

int CASW_Weapon_Medical_Satchel::GetHealAmount()
{
	CASW_Marine *pMarine = GetMarine();
	if ( !pMarine )
	{
		return 0;
	}

	float flHealAmount = MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_HEALING, ASW_MARINE_SUBSKILL_HEALING_HPS);

	//CALL_ATTRIB_HOOK_FLOAT( flHealAmount, mod_heal_bonus );

	return flHealAmount;
}

// checks if we're highlighting a fellow marine, if so we chuck him some health
bool CASW_Weapon_Medical_Satchel::GiveHealth()
{
#ifndef CLIENT_DLL
	CASW_Player *pOwner = GetCommander();
	CASW_Marine *pMarine = GetMarine();
	if ( !pOwner || !pMarine )
		return false;
	
	CASW_Marine* pTargetMarine = dynamic_cast< CASW_Marine* >( pOwner->GetHighlightEntity() );
	if ( !pTargetMarine )
		return false;

	GiveHealthTo( pTargetMarine );
#else
	// Play a cool animation or effect here!
#endif

	return true;
}

#ifdef CLIENT_DLL

	// if the player has his mouse over another marine, highlight it, cos he's the one we can give health to
	void CASW_Weapon_Medical_Satchel::MouseOverEntity(C_BaseEntity *pEnt, Vector vecWorldCursor)
	{
		C_ASW_Marine* pOtherMarine = C_ASW_Marine::AsMarine( pEnt );
		CASW_Player *pOwner = GetCommander();
		CASW_Marine *pMarine = GetMarine();
		if (!pOwner || !pMarine)
			return;

		float fOtherHealth = 1.0f;
		if (pOtherMarine && pOtherMarine->GetMarineResource())
			fOtherHealth = pOtherMarine->GetMarineResource()->GetHealthPercent();
		if (!pOtherMarine)
		{
			C_ASW_Game_Resource *pGameResource = ASWGameResource();
			if (pGameResource)
			{
				// find marine closest to world cursor
				const float fMustBeThisClose = 70;
				const C_ASW_Game_Resource::CMarineToCrosshairInfo::tuple_t &info = pGameResource->GetMarineCrosshairCache()->GetClosestMarine();
				if ( info.m_fDistToCursor <= fMustBeThisClose )
				{
					pOtherMarine = info.m_hMarine.Get();
				}
			}
		}

		// if the marine our cursor is over is near enough, highlight him
		if (pOtherMarine)
		{
			float dist = (pMarine->GetAbsOrigin() - pOtherMarine->GetAbsOrigin()).Length2D();
			if (dist < MAX_HEAL_DISTANCE)
			{
				bool bCanGiveHealth = ( fOtherHealth < 1.0f && m_iClip1 > 0 );
				ASWInput()->SetHighlightEntity( pOtherMarine, bCanGiveHealth );
				if ( bCanGiveHealth )		// if he needs healing, show the give health cursor
				{
					CASWHudCrosshair *pCrosshair = GET_HUDELEMENT( CASWHudCrosshair );
					if ( pCrosshair )
					{
						pCrosshair->SetShowGiveHealth(true);
					}
				}
			}
		}		
	}

#else

	bool CASW_Weapon_Medical_Satchel::MedSatchelEmpty()
	{
		return ( m_iClip1 <= 0 && m_iClip2 <= 0 );
	}


	void CASW_Weapon_Medical_Satchel::GiveHealthTo( CASW_Marine *pTarget )
	{
		if ( !pTarget || m_iClip1 <= 0 )
			return;

		if ( pTarget->GetHealth() >= pTarget->GetMaxHealth() )		// already on full health
			return;

		if ( pTarget->GetHealth() <= 0 )		// target is dead!
			return;

		if ( pTarget->m_bSlowHeal )			// already healing
			return;

		CASW_Marine *pMarine = GetMarine();
		if ( !pMarine )
			return;

		if ( pMarine->GetFlags() & FL_FROZEN )	// don't allow this if the marine is frozen
			return;

		// check if we're close enough
		Vector diff = pMarine->GetAbsOrigin() - pTarget->GetAbsOrigin();
		if ( diff.Length() > MAX_HEAL_DISTANCE_SERVER )	// add a bit of leeway to account for lag
			return;

		bool bMedic = pMarine && pMarine->GetMarineProfile() && pMarine->GetMarineProfile()->CanUseFirstAid();

		//pMarine->DoAnimationEvent(PLAYERANIMEVENT_HEAL);
		WeaponSound(SINGLE);

		int iHealAmount = GetHealAmount();
		pTarget->AddSlowHeal(iHealAmount, 1, pMarine, this );

		// Fire event
		IGameEvent * event = gameeventmanager->CreateEvent( "player_heal" );
		if ( event )
		{
			CASW_Player *pPlayer = GetCommander();
			event->SetInt( "userid", ( pPlayer ? pPlayer->GetUserID() : 0 ) );
			event->SetInt( "entindex", pTarget->entindex() );
			gameeventmanager->FireEvent( event );
		}

		ASWFailAdvice()->OnMarineHealed();

		if ( ASWGameRules()->GetInfoHeal() )
		{
			ASWGameRules()->GetInfoHeal()->OnMarineHealed( GetMarine(), pTarget, this );
		}

		m_iClip1 -= 1;

		if ( bMedic )
		{
			if (pTarget->IsInfested())
			{
				float fCurePercent = GetInfestationCureAmount();
				// cure infestation
				if ( fCurePercent > 0 && fCurePercent < 100 )
				{
					pTarget->CureInfestation(pMarine, fCurePercent);
				}
			}

			bool bSkipChatter = pTarget->IsInfested();
			int iTotalMeds = GetTotalMeds();
			if ( m_iClip1 <= 0 )
			{
				if ( iTotalMeds <= 0 )
				{
					ASWFailAdvice()->OnMedSatchelEmpty();

					pMarine->GetMarineSpeech()->Chatter( CHATTER_MEDS_NONE );
					bSkipChatter = true;
				}
				
				DestroyIfEmpty( false, true );	// out of medical charges, remove this item
			}
			else if ( iTotalMeds <= 1 )
			{
				if ( pMarine->GetMarineSpeech()->Chatter( CHATTER_MEDS_LOW ) )
				{
					bSkipChatter = true;
				}
			}
			if ( !bSkipChatter )
			{
				// try and do a special chatter?
				bool bSkipChatter = false;
				if (pMarine->GetMarineSpeech()->AllowCalmConversations(CONV_HEALING_CRASH))
				{
					if (!pTarget->m_bDoneWoundedRebuke && pTarget->GetMarineResource() && pTarget->GetMarineResource()->m_bTakenWoundDamage)
					{
						// marine has been wounded and this is our first heal after the fact - good chance of the medic saying something
						if (random->RandomFloat() < asw_marine_special_heal_chatter_chance.GetFloat() * 3)
						{
							if (CASW_MarineSpeech::StartConversation(CONV_SERIOUS_INJURY, pMarine, pTarget))
							{
								bSkipChatter = true;
								pTarget->m_bDoneWoundedRebuke = true;
							}
						}
					}

					// if we didn't complaint about a serious injury, check for doing a different kind of conv
					float fRand = random->RandomFloat();
					if (!bSkipChatter && fRand < asw_marine_special_heal_chatter_chance.GetFloat())
					{
						if (pTarget->GetMarineProfile() && pTarget->GetMarineProfile()->m_VoiceType == ASW_VOICE_CRASH
							&& pMarine->GetMarineProfile() && pMarine->GetMarineProfile()->m_VoiceType == ASW_VOICE_BASTILLE)	// bastille healing crash
						{
							if (random->RandomFloat() < 0.5f)
							{
								if (CASW_MarineSpeech::StartConversation(CONV_HEALING_CRASH, pMarine, pTarget))
									bSkipChatter = true;						
							}
							else
							{
								if (CASW_MarineSpeech::StartConversation(CONV_MEDIC_COMPLAIN, pMarine, pTarget))
									bSkipChatter = true;
							}
						}
						else
						{
							if (CASW_MarineSpeech::StartConversation(CONV_MEDIC_COMPLAIN, pMarine, pTarget))
								bSkipChatter = true;
						}
					}
				}
				if (!bSkipChatter)
					pMarine->GetMarineSpeech()->Chatter(CHATTER_HEALING);
			}
		}	
	}

	float CASW_Weapon_Medical_Satchel::GetInfestationCureAmount()
	{
		CASW_Marine *pMarine = GetMarine();
		if (!pMarine)
			return 0.0f;		

		float flCureAmount = MarineSkills()->GetSkillBasedValueByMarine(pMarine, ASW_MARINE_SKILL_XENOWOUNDS) / 100.0f;

		//CALL_ATTRIB_HOOK_FLOAT( flCureAmount, mod_xenowound_bonus );

		return flCureAmount;
	}

	int CASW_Weapon_Medical_Satchel::GetTotalMeds()
	{
		CASW_Marine *pMarine = GetMarine();
		if (!pMarine)
			return 0;

		int iMeds = 0;
		CASW_Weapon_Medical_Satchel* pSatchel = dynamic_cast<CASW_Weapon_Medical_Satchel*>(pMarine->GetWeapon(0));
		if (pSatchel)
		{
			iMeds += pSatchel->Clip1();
		}

		pSatchel = dynamic_cast<CASW_Weapon_Medical_Satchel*>(pMarine->GetWeapon(1));
		if (pSatchel)
		{
			iMeds += pSatchel->Clip1();
		}
		return iMeds;
	}

	bool CASW_Weapon_Medical_Satchel::Holster( CBaseCombatWeapon *pSwitchingTo )
	{
		bool bResult = BaseClass::Holster( pSwitchingTo );

		if ( bResult )
		{
			DestroyIfEmpty( true, false );
		}

		return bResult;
	}

	void CASW_Weapon_Medical_Satchel::Drop( const Vector &vecVelocity )
	{
		if ( !DestroyIfEmpty( true, false ) )
		{
			BaseClass::Drop( vecVelocity );
		}
	}
#endif