#include "cbase.h"
#include "asw_marine_profile.h"
#ifdef CLIENT_DLL
	#include "c_asw_marine_resource.h"
	#include "c_asw_player.h"
	#include "c_asw_marine.h"
	#include "c_asw_hack_computer.h"
	#include "c_asw_computer_area.h"
	#define CASW_Marine C_ASW_Marine
	#define CASW_Marine_Resource C_ASW_Marine_Resource
	#define CASW_Player C_ASW_Player
	#define CASW_Hack_Computer C_ASW_Hack_Computer
	#define CASW_Computer_Area C_ASW_Computer_Area
#else
	#include "asw_marine_resource.h"
	#include "asw_player.h"
	#include "asw_marine.h"
	#include "asw_hack_computer.h"
	#include "asw_computer_area.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void CASW_Hack_Computer::ASWPostThink(CASW_Player *pPlayer, CASW_Marine *pMarine,  CUserCmd *ucmd, float fDeltaTime)
{
	if (m_bLastAllCorrect)	// don't do any processing the puzzle became correct
		return;
	// todo: make sure we don't rotate them twice for the same tick?

#ifndef CLIENT_DLL
	// check for auto overriding
	if ( GetComputerArea() && pMarine )
	{
		if ( pMarine->GetMarineProfile()->CanHack() && GetComputerArea()->m_bIsLocked
			&& m_iShowOption.Get() != ASW_HACK_OPTION_OVERRIDE && gpGlobals->curtime > GetComputerArea()->m_fAutoOverrideTime )
		{
			GetComputerArea()->Override( pMarine );
		}
	}
	

	// play a sound if the player has done the hack too slowly?
	if (!m_bPlayedTimeOutSound && m_iNumTumblers.Get() > 3)
	{
		if (m_fFastFinishTime.Get() > 0 && gpGlobals->curtime > m_fFastFinishTime.Get())
		{
			if (GetComputerArea())
				GetComputerArea()->EmitSound("ASWComputer.TimeOut");
			m_bPlayedTimeOutSound = true;
		}
	}
#else
	m_fTumblerDiffTime = gpGlobals->curtime - GetNextMoveTime();
	if (m_fTumblerDiffTime < 0)
		m_fTumblerDiffTime = 0;
#endif

	// need to store the reversal until the number reaches a 'whole' position
	if (pPlayer)	// only check the ucmd if we have a valid player driving this (as opposed to an uninhabited marine ticking the computer)
	{
		if (ucmd->impulse == ASW_IMPULSE_REVERSE_TUMBLER_1)		ReverseTumbler(0, pMarine);
		else if (ucmd->impulse == ASW_IMPULSE_REVERSE_TUMBLER_2)	ReverseTumbler(1, pMarine);
		else if (ucmd->impulse == ASW_IMPULSE_REVERSE_TUMBLER_3)	ReverseTumbler(2, pMarine);
		else if (ucmd->impulse == ASW_IMPULSE_REVERSE_TUMBLER_4)	ReverseTumbler(3, pMarine);
		else if (ucmd->impulse == ASW_IMPULSE_REVERSE_TUMBLER_5)	ReverseTumbler(4, pMarine);
		else if (ucmd->impulse == ASW_IMPULSE_REVERSE_TUMBLER_6)	ReverseTumbler(5, pMarine);
		else if (ucmd->impulse == ASW_IMPULSE_REVERSE_TUMBLER_7)	ReverseTumbler(6, pMarine);
		else if (ucmd->impulse == ASW_IMPULSE_REVERSE_TUMBLER_8)	ReverseTumbler(7, pMarine);
	}
		
	if (gpGlobals->curtime >= m_fNextMoveTime.Get() + 0.5f)	// we've finished our half second animation slide
	{
		//Msg("s=%d:%f: next=%f\n", IsServer(), gpGlobals->curtime, m_fNextMoveTime.Get());
		// move each entry up 1 digit
		for (int i=0;i<ASW_HACK_COMPUTER_MAX_TUMBLERS;i++)
		{
			int direction = (m_iTumblerDirection[i] == 1) ? 1 : -1;
			m_iTumblerPosition.Set(i, m_iTumblerPosition[i] + direction);
			// wrap them around
			while (m_iTumblerPosition[i] < 0)
				m_iTumblerPosition.Set(i, m_iTumblerPosition[i] + m_iEntriesPerTumbler);
			while (m_iTumblerPosition[i] >= m_iEntriesPerTumbler)
				m_iTumblerPosition.Set(i, m_iTumblerPosition[i] - m_iEntriesPerTumbler);
#ifdef CLIENT_DLL
			m_iNewTumblerPosition[i] = m_iTumblerPosition[i];
#endif
			//if (i == 0)
				//Msg("s=%d:%f:Tumbler %d at %d\n", IsServer(), gpGlobals->curtime, i, m_iTumblerPosition[i]);
		}
		// check if we've haxored the computer
		bool bAllCorrect = true;
		int iNumWrong = 0;
		for (int k=0;k<m_iNumTumblers;k++)
		{
			if (!IsTumblerCorrect(k))
			{
				bAllCorrect = false;
				iNumWrong++;
			}
		}
		UpdateCorrectStatus(pPlayer, pMarine, iNumWrong);

		if (bAllCorrect != m_bLastAllCorrect)
		{
			if (bAllCorrect)
			{
#ifndef CLIENT_DLL	
				OnHackUnlocked(pMarine);
				
				// TODO: What if the locked state change arrives after the hack option one? then the main menu will come up incorrectly - should put in checks clientside for this
				//if (gpGlobals->maxClients <= 1)
				//{				
					//CPASAttenuationFilter filter( pMarine );
					//pMarine->EmitSound(filter, pMarine->entindex(), "ASWComputer.HackComplete");
				//}
#else
				//CLocalPlayerFilter filter;
				//C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWComputer.HackComplete" );
#endif				
			}
		}
		m_bLastAllCorrect = bAllCorrect;

		if (bAllCorrect)
			return;
				
		// set a time for us to next start sliding		
		m_fNextMoveTime = gpGlobals->curtime + m_fMoveInterval;		
	}
	else if (gpGlobals->curtime >= m_fNextMoveTime)	// we're in the half animation slide period.
	{
		// can't reverse direction during this period, but it'll be queued up to happen once the numbers stop their half second slide
	}
	else
	{
		// if we have a tumbler reverse to deal with, do it
		for (int i=0;i<ASW_HACK_COMPUTER_MAX_TUMBLERS;i++)
		{
			if (m_iNewTumblerDirection[i] != 0)
			{
				m_iTumblerDirection.Set(i, m_iNewTumblerDirection[i]);
				m_iNewTumblerDirection[i] = 0;
			}
		}
	}

}

#ifndef CLIENT_DLL
void CASW_Hack_Computer::OnHackUnlocked( CASW_Marine *pMarine )
{
	GetComputerArea()->UnlockFromHack( pMarine );	// unlock it

	// if the computer has a download option, go straight into that
	int iDownloadMenuItem = -1;
	int iNumMenuItems = GetComputerArea()->GetNumMenuOptions();
	for ( int i=0;i<iNumMenuItems;i++ )
	{
		int iOptionType = GetOptionTypeForEntry(i+1);
		if (iOptionType == ASW_COMPUTER_OPTION_TYPE_DOWNLOAD_DOCS)
		{
			iDownloadMenuItem = i+1;
			break;
		}
	}

	if ( iDownloadMenuItem != -1 )
	{
		SelectHackOption( iDownloadMenuItem );
	}
	else
	{
		SelectHackOption(0);	// drop the player back to the computer's main menu
	}
}
#endif

void CASW_Hack_Computer::ReverseTumbler(int i, CASW_Marine *pMarine)
{
	if (i<0 || i>= ASW_HACK_COMPUTER_MAX_TUMBLERS)
		return;

	// if we already set a tumbler direction swap, then change our minds
	if (m_iNewTumblerDirection[i] != 0)
	{
		m_iNewTumblerDirection[i] = -m_iNewTumblerDirection[i];		
	}
	else
	{

		// if not, then queue up a change to go in the opposite direction to the current
		if (m_iTumblerDirection[i] == 1)
			m_iNewTumblerDirection[i] = -1;		
		else
			m_iNewTumblerDirection[i] = 1;
	}

	// check if we've haxored the computer
	if (!m_bLastAllCorrect)
	{
		bool bAllCorrect = true;
		int iNumWrong = 0;
		for (int k=0;k<m_iNumTumblers;k++)
		{
			if (!IsTumblerCorrect(k))
			{
				bAllCorrect = false;
				iNumWrong++;
			}
		}
	
		CASW_Player *pPlayer = NULL;
		if (pMarine && pMarine->IsInhabited())
			pPlayer = pMarine->GetCommander();
		UpdateCorrectStatus(pPlayer, pMarine, iNumWrong);

		if (bAllCorrect != m_bLastAllCorrect)
		{
			if (bAllCorrect && pMarine)
			{
	#ifndef CLIENT_DLL				
				OnHackUnlocked(pMarine);
				// TODO: What if the locked state change arrives after the hack option one? then the main menu will come up incorrectly - should put in checks clientside for this
				//if (gpGlobals->maxClients <= 1)
				//{				
					//CPASAttenuationFilter filter( pMarine );
					//pMarine->EmitSound(filter, pMarine->entindex(), "ASWComputer.HackComplete");
				//}
	#else
				//CLocalPlayerFilter filter;
				//C_BaseEntity::EmitSound( filter, -1 /*SOUND_FROM_LOCAL_PLAYER*/, "ASWComputer.HackComplete" );
	#endif				
			}
		}
		m_bLastAllCorrect = bAllCorrect;
	}
}

CASW_Computer_Area* CASW_Hack_Computer::GetComputerArea()
{
	CASW_Computer_Area* pComp = dynamic_cast<CASW_Computer_Area*>(m_hHackTarget.Get());
	return pComp;
}

// checks if a particular column in the hack puzzle is in the 'correct' place
bool CASW_Hack_Computer::IsTumblerCorrect(int i)
{
	if (i<0 || i>= ASW_HACK_COMPUTER_MAX_TUMBLERS)
		return false;

	// find the offset of our lit number
	int my_pos = m_iTumblerPosition[i] + m_iTumblerCorrectNumber[i];
	if (my_pos >= m_iEntriesPerTumbler)
		my_pos -= m_iEntriesPerTumbler;

	int my_dir = m_iTumblerDirection[i];
	if (m_iNewTumblerDirection[i] != 0)
		my_dir = m_iNewTumblerDirection[i];

	//  find the most 'correct' row (i.e. the row with the most aligned numbers)
	int iNumAligned[ASW_MAX_ENTRIES_PER_TUMBLER*2];
	int iHighestAligned = 0;
	int iHighestAlignedPos = m_iTumblerPosition[0];	
	int iHighestAlignedDir = (m_iNewTumblerDirection[0] != 0) ? m_iNewTumblerDirection[0] : m_iTumblerDirection[0];
	for (int k=0;k<ASW_MAX_ENTRIES_PER_TUMBLER*2;k++)
	{
		iNumAligned[k] = 0;
	}
	for (int k=0;k<m_iNumTumblers;k++)
	{
		int iIndex = m_iTumblerPosition[k] + m_iTumblerCorrectNumber[k];
		if (iIndex >= m_iEntriesPerTumbler)
			iIndex -= m_iEntriesPerTumbler;
		int dir = m_iTumblerDirection[k];
		if (m_iNewTumblerDirection[k] != 0)
			dir = m_iNewTumblerDirection[k];
		if (dir > 0)
			iIndex += ASW_MAX_ENTRIES_PER_TUMBLER;

		if (iIndex >= 0 && iIndex < ASW_MAX_ENTRIES_PER_TUMBLER*2)
		{
			iNumAligned[iIndex]++;
			if (iNumAligned[iIndex] > iHighestAligned)
			{
				iHighestAligned = iNumAligned[iIndex];
				iHighestAlignedPos = m_iTumblerPosition[k] + m_iTumblerCorrectNumber[k];
				if (iHighestAlignedPos >= m_iEntriesPerTumbler)
					iHighestAlignedPos -= m_iEntriesPerTumbler;
				iHighestAlignedDir = dir;
			}
		}
	}

	// tumbler is correct if it matches the most correct row
	return ((my_dir == iHighestAlignedDir) && (my_pos == iHighestAlignedPos));
}

float CASW_Hack_Computer::GetTumblerProgress()
{
	int iCorrect = 0;
	for (int k=0;k<m_iNumTumblers;k++)
	{
		if (IsTumblerCorrect(k))
		{
			iCorrect++;
		}
	}
	if (m_iNumTumblers <= 0)
		return 0;

	if (iCorrect >= m_iNumTumblers)
		return 0;	// don't show any bar if we're finished

	if (iCorrect == 1 && m_iNumTumblers > 2)	// bump the number correct up slightly since 1 correct tumbler means two aligned and the progress bar looks better this way
		iCorrect++;

	return (float(iCorrect) / float(m_iNumTumblers));
}

void CASW_Hack_Computer::UpdateCorrectStatus(CASW_Player *pPlayer, CASW_Marine *pMarine, int iNumWrong)
{
	if (m_iShowOption == ASW_HACK_OPTION_OVERRIDE)
	{
		if (gpGlobals->curtime >= m_fNextMoveTime)	// can't update our status if we're in the middle of sliding
			return;

		bool bHalfCorrect = (iNumWrong-1) <= (m_iNumTumblers-1) * 0.5f;
#ifndef CLIENT_DLL
		if (bHalfCorrect != m_bLastHalfCorrect && pMarine)
		{
			GetComputerArea()->HackHalfway(pMarine);
		}
#endif
		m_bLastHalfCorrect = bHalfCorrect;

		if (iNumWrong != m_iLastNumWrong)
		{
#ifndef CLIENT_DLL	// client plays these sounds from his vgui panel
			if (iNumWrong < m_iLastNumWrong)
			{
				GetComputerArea()->PlayPositiveSound(pPlayer);
			}
			else
			{
				GetComputerArea()->PlayNegativeSound(pPlayer);
			}
#endif
			m_iLastNumWrong = iNumWrong;			
		}
	}
}