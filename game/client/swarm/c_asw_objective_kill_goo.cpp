#include "cbase.h"
#include "c_asw_objective_kill_goo.h"
#include <vgui/ILocalize.h>
#include <vgui_controls/Panel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Objective_Kill_Goo, DT_ASW_Objective_Kill_Goo, CASW_Objective_Kill_Goo)	
	RecvPropInt		(RECVINFO(m_iTargetKills)),
	RecvPropInt		(RECVINFO(m_iCurrentKills)),
END_RECV_TABLE()

C_ASW_Objective_Kill_Goo::C_ASW_Objective_Kill_Goo()
{
	m_dest_buffer[0] = '\0';
	m_iKillsLeft = -1;
}

void C_ASW_Objective_Kill_Goo::OnDataChanged(DataUpdateType_t updateType)
{
	if ( updateType == DATA_UPDATE_CREATED )
	{
		//FindText();
	}
	BaseClass::OnDataChanged(updateType);
}

bool C_ASW_Objective_Kill_Goo::NeedsTitleUpdate()
{	
	int iKillsLeft = m_iTargetKills - m_iCurrentKills;
	if (iKillsLeft == 0)
		iKillsLeft = m_iTargetKills;
	return (iKillsLeft != m_iKillsLeft);
}

const wchar_t* C_ASW_Objective_Kill_Goo::GetObjectiveTitle()
{
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
			g_pVGuiLocalize->Find("#asw_kill_goo_objective_format"), 2,
			wnumber_buffer, wnumber_buffer2 );
	}
	return m_dest_buffer;
}

float C_ASW_Objective_Kill_Goo::GetObjectiveProgress()
{
	if ( m_iTargetKills <= 0 )
		return BaseClass::GetObjectiveProgress();

	float flProgress = (float) m_iCurrentKills.Get() / (float) m_iTargetKills.Get();
	flProgress = clamp<float>( flProgress, 0.0f, 1.0f );

	return flProgress;
}