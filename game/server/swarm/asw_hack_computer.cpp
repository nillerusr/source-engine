#include "cbase.h"
#include "asw_marine_resource.h"
#include "asw_marine_profile.h"
#include "asw_player.h"
#include "asw_marine.h"
#include "asw_hack_computer.h"
#include "asw_remote_turret_shared.h"
#include "asw_computer_area.h"
#include "asw_gamerules.h"

// memdbgon must be the last include file in a .cpp file
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( asw_hack_computer, CASW_Hack_Computer );

IMPLEMENT_SERVERCLASS_ST(CASW_Hack_Computer, DT_ASW_Hack_Computer)	
	SendPropInt		(SENDINFO(m_iNumTumblers)),
	SendPropInt		(SENDINFO(m_iEntriesPerTumbler)),
	SendPropBool	(SENDINFO(m_bLastAllCorrect)),
	
	SendPropFloat	(SENDINFO(m_fMoveInterval)),
	SendPropFloat	(SENDINFO(m_fNextMoveTime), 0, SPROP_NOSCALE ),	
	SendPropFloat	(SENDINFO(m_fFastFinishTime), 0, SPROP_NOSCALE ),
	SendPropArray3  (SENDINFO_ARRAY3(m_iTumblerPosition), SendPropInt( SENDINFO_ARRAY(m_iTumblerPosition), 12 ) ),	
	SendPropArray3  (SENDINFO_ARRAY3(m_iTumblerCorrectNumber), SendPropInt( SENDINFO_ARRAY(m_iTumblerCorrectNumber), 12 ) ),	
	SendPropArray3  (SENDINFO_ARRAY3(m_iTumblerDirection), SendPropInt( SENDINFO_ARRAY(m_iTumblerDirection), 12 ) ),	
END_SEND_TABLE()

BEGIN_DATADESC( CASW_Hack_Computer )
	DEFINE_FIELD(m_bSetupComputer, FIELD_BOOLEAN),
	DEFINE_FIELD(m_fFastFinishTime, FIELD_TIME),
END_DATADESC()

extern ConVar asw_debug_medals;

CASW_Hack_Computer::CASW_Hack_Computer()
{
	m_iShowOption = 0;		// the main menu
	m_bPlayedTimeOutSound = false;
	
	m_fNextMoveTime = 0;
	m_iLastNumWrong = 0;
	m_bLastAllCorrect = false;
	m_bLastHalfCorrect = false;
}

bool CASW_Hack_Computer::InitHack(CASW_Player* pHackingPlayer, CASW_Marine* pHackingMarine, CBaseEntity* pHackTarget)
{
	CASW_Computer_Area *pComputer = dynamic_cast<CASW_Computer_Area*>(pHackTarget);
	if (!pHackingPlayer || !pHackingMarine || !pComputer || !pHackingMarine->GetMarineResource())
	{
		return false;
	}

	if (!m_bSetupComputer)
	{
		m_bSetupComputer = true;
		m_iNumTumblers = pComputer->m_iHackLevel;	// hack level controls how many tumblers there should be
		if (m_iNumTumblers <= 1)
			m_iNumTumblers = 5;
		if (m_iNumTumblers > ASW_HACK_COMPUTER_MAX_TUMBLERS)
			m_iNumTumblers = ASW_HACK_COMPUTER_MAX_TUMBLERS;
		m_iEntriesPerTumbler = pComputer->m_iNumEntriesPerTumbler;	// temp for now
		if (m_iEntriesPerTumbler <= ASW_MIN_ENTRIES_PER_TUMBLER)
			m_iEntriesPerTumbler = ASW_MIN_ENTRIES_PER_TUMBLER;
		if (m_iEntriesPerTumbler >= ASW_MAX_ENTRIES_PER_TUMBLER)
			m_iEntriesPerTumbler= ASW_MAX_ENTRIES_PER_TUMBLER;
		// check it's an odd number
		int r = m_iEntriesPerTumbler % 2;
		if (r != 1)
			r -=1;
		m_fMoveInterval = pComputer->m_fMoveInterval;	// idle gap between slides (slides are 0.5f seconds long themselves)
		if (m_fMoveInterval <= 0.3)
			m_fMoveInterval = 0.3f;
		if (m_fMoveInterval > 2.0f)
			m_fMoveInterval= 2.0f;

		// adjust counts by marine skill
		///  removed this - goes wrong if a different marine access the computer first and generates the hack entity
		/*
		bool bEasyHack = (m_iNumTumblers < ASW_MIN_TUMBLERS_FAST_HACK);
		int iColumnReduction = MarineSkills()->GetSkillBasedValueByMarine(pHackingMarine, ASW_MARINE_SKILL_HACKING, ASW_MARINE_SUBSKILL_HACKING_TUMBLER_COUNT_REDUCTION);
		int iRowReduction = MarineSkills()->GetSkillBasedValueByMarine(pHackingMarine, ASW_MARINE_SKILL_HACKING, ASW_MARINE_SUBSKILL_HACKING_ENTRIES_PER_TUMBLER_REDUCTION);
		m_iNumTumblers -= iColumnReduction;
		if (!bEasyHack && m_iNumTumblers <= ASW_MIN_TUMBLERS_FAST_HACK)
			m_iNumTumblers = ASW_MIN_TUMBLERS_FAST_HACK;
		m_iEntriesPerTumbler -= iRowReduction;
		if (m_iEntriesPerTumbler <= 5)
			m_iEntriesPerTumbler = 5;
			*/

		if ( m_iEntriesPerTumbler % 2 > 0 )
		{
			m_iEntriesPerTumbler++;
		}
		if (m_iNumTumblers <= 3)
			m_iNumTumblers = 3;

		int k=0;
		bool bIncomplete = false;
		for (int i=0;i<ASW_HACK_COMPUTER_MAX_TUMBLERS;i++)
		{
			if (random->RandomFloat() > 0.5f)
				m_iTumblerDirection.Set(i, -1);
			else
				m_iTumblerDirection.Set(i, 1);
			m_iTumblerPosition.Set(i, random->RandomInt(0, m_iEntriesPerTumbler-1));
			m_iTumblerCorrectNumber.Set(i, random->RandomInt(0, m_iEntriesPerTumbler-1));

			if ( m_iTumblerPosition.Get( i ) % 2 > 0 )
			{
				m_iTumblerPosition.Set( i, ( m_iTumblerPosition.Get( i ) + 1 ) % m_iEntriesPerTumbler.Get() );
			}

			if ( m_iTumblerCorrectNumber.Get( i ) % 2 > 0 )
			{
				m_iTumblerCorrectNumber.Set( i, ( m_iTumblerCorrectNumber.Get( i ) + 1 ) % m_iEntriesPerTumbler.Get() );
			}
			//Msg("Set %d to correct number %d\n", i, m_iTumblerCorrectNumber[i]);
			m_iNewTumblerDirection[i] = 0;
		}
		k++;
		bIncomplete = (GetTumblerProgress() < 1.0f);

		// if the generated puzzle is already solved, then flip every other tumbler
		if ( !bIncomplete )
		{
			for (int i=0;i<ASW_HACK_COMPUTER_MAX_TUMBLERS;i+=2)
			{
				m_iTumblerDirection.Set(i, m_iTumblerDirection[i] );
			}
		}

		// have to set this before checking if it's a PDA
		m_hHackTarget = pHackTarget;

		SetDefaultHackOption();
		Msg("initted computer with show option %d\n", m_iShowOption);
	}
	else
	{
		// make sure we're back on the right page

		// have to set this before checking if it's a PDA
		m_hHackTarget = pHackTarget;

		SetDefaultHackOption();
		Msg("reusing computer with show option %d\n", m_iShowOption);
	}

	return BaseClass::InitHack(pHackingPlayer, pHackingMarine, pHackTarget);
}

void CASW_Hack_Computer::SelectHackOption(int i)
{
	if (!GetComputerArea())
		return;

	int iOptionType = GetOptionTypeForEntry(i);

	// check we're allowed to do this
	if (GetComputerArea()->m_bIsLocked)
	{
		if (i != ASW_HACK_OPTION_OVERRIDE)	// in a locked computer, the only valid hack menu option is the override puzzle
			return;

		if (!GetHackingMarine() || !GetHackingMarine()->GetMarineProfile()	// only a hacker can access a locked computer
				|| !GetHackingMarine()->GetMarineProfile()->CanHack())
			return;

		if (m_fFastFinishTime == 0)
		{
			float ideal_time = 0;
			float diff_factor = 1.8f;
			int iSkill = ASWGameRules()->GetSkillLevel();
			if (iSkill == 2)
				diff_factor = 1.55f;
			else if (iSkill == 3)
				diff_factor = 1.50f;
			else if (iSkill >= 4)
				diff_factor = 1.45f;
			// estimate the time for a fast hack
			// try, time taken for each column to rotate through twice
			float time_per_column = m_fMoveInterval * m_iEntriesPerTumbler;
			ideal_time = time_per_column * diff_factor * m_iNumTumblers;
			if (asw_debug_medals.GetBool())
				Msg("Fast hack time is %f, starting at %f\n", ideal_time, gpGlobals->curtime);
			m_fFastFinishTime = gpGlobals->curtime + ideal_time;
		}
	}
	else if (i == ASW_HACK_OPTION_OVERRIDE)	// can't do an override if the computer isn't locked
		return;

	Msg("[S] CASW_Hack_Computer::SelectHackOption %d\n", i);
	m_iShowOption = i;
	Msg("  Type is %d\n", GetOptionTypeForEntry(i));

	// make sure the computer area knows if we're viewing mail or not
	bool bViewingMail = (iOptionType == ASW_COMPUTER_OPTION_TYPE_MAIL);
	CASW_Computer_Area *pArea = GetComputerArea();
	if (pArea)
		pArea->m_bViewingMail = bViewingMail;
	Msg("Viewing mail %d\n", bViewingMail);

	// make sure the downloading sound isn't playing if we're not doing downloading
	if (iOptionType != ASW_COMPUTER_OPTION_TYPE_DOWNLOAD_DOCS)
	{
		pArea->StopDownloadingSound();
	}

	if (iOptionType == ASW_COMPUTER_OPTION_TYPE_DOWNLOAD_DOCS)
	{
		Msg("This option is downloading critical data\n");		
		// note: being on this option causes the computer area's MarineUsing function to count up to downloading the files
	}
	else if (iOptionType == ASW_COMPUTER_OPTION_TYPE_TURRET_1)
	{
		Msg("This option is a turret 1\n");
		CASW_Computer_Area *pArea = GetComputerArea();
		if (!pArea || !pArea->m_hTurret1.Get())
		{
			Msg("Area is null or no turret handle\n");
			return;
		}
		
		CASW_Remote_Turret* pTurret = dynamic_cast<CASW_Remote_Turret*>(pArea->m_hTurret1.Get());			
		if (pTurret && GetHackingMarine())
		{
			pTurret->StartedUsingTurret(GetHackingMarine());
			GetHackingMarine()->m_hRemoteTurret = pTurret;
			Msg("Set turret 1\n");
			return;
		}
	}
}

bool CASW_Hack_Computer::IsPDA()
{
	CASW_Computer_Area *pArea = GetComputerArea();
	bool bPDA = (pArea && pArea->m_PDAName.Get()[0] != 0);
	//Msg("bpda = %d pArea = %d pdaname[0] = %d\n", bPDA, pArea, pArea ? pArea->m_PDAName.Get()[0] : -1);
	return bPDA;
}

int CASW_Hack_Computer::GetMailOption()
{
	CASW_Computer_Area *pArea = GetComputerArea();
	if (!pArea)
	{
		Msg("no area, so mail option returning -1\n");
		return -1;
	}
	int m = pArea->GetNumMenuOptions();
	for (int i=0;i<m;i++)
	{
		Msg("mail option %d/%d is type %d\n", i+1, m, GetOptionTypeForEntry(i));
		if (GetOptionTypeForEntry(i+1) == ASW_COMPUTER_OPTION_TYPE_MAIL)
			return i+1;
	}
	Msg("didn't find any options set to ASW_COMPUTER_OPTION_TYPE_MAIL so mail option returning -1\n");
	return -1;
}

void CASW_Hack_Computer::SetDefaultHackOption()
{
	if (IsPDA())
	{
		int iMailOption = GetMailOption();
		Msg("mail option is %d\n", iMailOption);
		if (iMailOption != -1)
			m_iShowOption = iMailOption;
		else
			m_iShowOption = 0;
	}
	else
		m_iShowOption = 0;	// put us on the main menu
}

void CASW_Hack_Computer::MarineStoppedUsing(CASW_Marine* pMarine)
{
	Msg("CASW_Hack_Computer::marine stopped using hack!\n");
	/*
	if (IsPDA())
	{
		int iMailOption = GetMailOption();
		Msg("MarineStoppedUsing mail option is %d\n", iMailOption);
		if (iMailOption != -1)
			m_iShowOption = iMailOption;
		else
			m_iShowOption = 0;
	}
	else*/
		m_iShowOption = 0;	// put us back on the main menu
	Msg("MarineStoppedUsing set showoption to %d\n", m_iShowOption);
	if (pMarine->m_hRemoteTurret)
	{
		pMarine->m_hRemoteTurret->StopUsingTurret();
		pMarine->m_hRemoteTurret = NULL;
	}
	BaseClass::MarineStoppedUsing(pMarine);
}

int CASW_Hack_Computer::GetOptionTypeForEntry(int iOption)
{
	CASW_Computer_Area *pArea = GetComputerArea();
	if (!pArea)
		return ASW_COMPUTER_OPTION_TYPE_NONE;

	int icon = 1;	
	if (pArea->m_DownloadObjectiveName.Get()[0] != NULL && pArea->GetHackProgress() < 1.0f)
	{
		if (iOption == icon)
		{
			return ASW_COMPUTER_OPTION_TYPE_DOWNLOAD_DOCS;
		}
		icon++;
	}
	if (pArea->m_hSecurityCam1.Get())
	{
		if (iOption == icon)
		{
			return ASW_COMPUTER_OPTION_TYPE_SECURITY_CAM_1;
		}
		icon++;
	}
	if (pArea->m_hTurret1.Get())
	{
		if (iOption == icon)
		{			
			return ASW_COMPUTER_OPTION_TYPE_TURRET_1;
		}
		icon++;
	}
	if (pArea->m_MailFile.Get()[0] != NULL)
	{
		if (iOption == icon)
		{
			return ASW_COMPUTER_OPTION_TYPE_MAIL;
		}
		icon++;
	}
	if (pArea->m_NewsFile.Get()[0] != NULL)
	{
		if (iOption == icon)
		{
			return ASW_COMPUTER_OPTION_TYPE_NEWS;
		}
		icon++;
	}
	if (pArea->m_StocksSeed.Get()[0] != NULL)
	{
		if (iOption == icon)
		{
			return ASW_COMPUTER_OPTION_TYPE_STOCKS;
		}
		icon++;
	}
	if (pArea->m_WeatherSeed.Get()[0] != NULL)
	{
		if (iOption == icon)
		{
			return ASW_COMPUTER_OPTION_TYPE_WEATHER;
		}
		icon++;
	}
	if (pArea->m_PlantFile.Get()[0] != NULL)
	{
		if (iOption == icon)
		{
			return ASW_COMPUTER_OPTION_TYPE_PLANT;
		}
		icon++;
	}	

	return ASW_COMPUTER_OPTION_TYPE_NONE;
}

bool CASW_Hack_Computer::IsDownloadingFiles()
{
	if (!GetHackingMarine())
		return false;

	int iOptionType = GetOptionTypeForEntry(m_iShowOption);
	return (iOptionType == ASW_COMPUTER_OPTION_TYPE_DOWNLOAD_DOCS);
}