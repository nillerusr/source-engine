#include "cbase.h"
#include "C_ASW_Objective_Kill_Eggs.h"
#include <vgui/ILocalize.h>
#include <vgui_controls/Panel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Objective_Kill_Eggs, DT_ASW_Objective_Kill_Eggs, CASW_Objective_Kill_Eggs)	
	RecvPropInt		(RECVINFO(m_iTargetKills)),
	RecvPropInt		(RECVINFO(m_iCurrentKills)),
END_RECV_TABLE()

C_ASW_Objective_Kill_Eggs::C_ASW_Objective_Kill_Eggs()
{
	m_pKillText = NULL;
	m_pAlienPluralText = NULL;
	m_pAlienSingularText = NULL;
	m_bFoundText = false;
	m_dest_buffer[0] = '\0';
	m_iKillsLeft = -1;
}

void C_ASW_Objective_Kill_Eggs::OnDataChanged(DataUpdateType_t updateType)
{
	if ( updateType == DATA_UPDATE_CREATED )
	{
		FindText();
	}
	BaseClass::OnDataChanged(updateType);
}

bool C_ASW_Objective_Kill_Eggs::NeedsTitleUpdate()
{	
	int iKillsLeft = m_iTargetKills - m_iCurrentKills;
	if (iKillsLeft == 0)
		iKillsLeft = m_iTargetKills;
	return (iKillsLeft != m_iKillsLeft);
}

const wchar_t* C_ASW_Objective_Kill_Eggs::GetObjectiveTitle()
{
	if (!m_bFoundText || !m_pKillText || !m_pAlienPluralText || !m_pAlienSingularText)
	{
		return L"";
	}

	int iKills = MIN( m_iTargetKills.Get(), m_iCurrentKills.Get() );

	if ( iKills != m_iKillsLeft )	// update the string
	{		
		m_iKillsLeft = iKills;
		char number_buffer[12];
		Q_snprintf(number_buffer, sizeof(number_buffer), "%d", iKills);
		wchar_t wnumber_buffer[24];
		g_pVGuiLocalize->ConvertANSIToUnicode(number_buffer, wnumber_buffer, sizeof( wnumber_buffer ));

		Q_snprintf(number_buffer, sizeof(number_buffer), "%d", m_iTargetKills);
		wchar_t wnumber_buffer2[24];
		g_pVGuiLocalize->ConvertANSIToUnicode(number_buffer, wnumber_buffer2, sizeof( wnumber_buffer ));
		
		g_pVGuiLocalize->ConstructString( m_dest_buffer, sizeof(m_dest_buffer),
		g_pVGuiLocalize->Find( "#asw_kill_objective_format"), 4,
			m_pKillText, m_pAlienPluralText, wnumber_buffer, wnumber_buffer2 );	
	}
	return m_dest_buffer;
}

void C_ASW_Objective_Kill_Eggs::FindText()
{
	m_pKillText = g_pVGuiLocalize->Find("#asw_destroy");
	m_pAlienPluralText = GetPluralText();
	m_pAlienSingularText = GetSingularText();
	m_bFoundText = true;
}

wchar_t *C_ASW_Objective_Kill_Eggs::GetPluralText()
{
	return g_pVGuiLocalize->Find("#asw_eggs");
}

wchar_t *C_ASW_Objective_Kill_Eggs::GetSingularText()
{
	return g_pVGuiLocalize->Find("#asw_egg");
}

float C_ASW_Objective_Kill_Eggs::GetObjectiveProgress()
{
	if ( m_iTargetKills <= 0 )
		return BaseClass::GetObjectiveProgress();

	float flProgress = (float) m_iCurrentKills.Get() / (float) m_iTargetKills.Get();
	flProgress = clamp<float>( flProgress, 0.0f, 1.0f );

	return flProgress;
}