#include "cbase.h"
#include "c_asw_objective_countdown.h"
#include "asw_vgui_countdown.h"
#include "iclientmode.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Objective_Countdown, DT_ASW_Objective_Countdown, CASW_Objective_Countdown)
	RecvPropFloat		( RECVINFO( m_fCountdownFinishTime ) ),
END_RECV_TABLE()


C_ASW_Objective_Countdown::C_ASW_Objective_Countdown()
{
	m_bLaunchedPanel = false;
}

C_ASW_Objective_Countdown::~C_ASW_Objective_Countdown()
{
	
}

void C_ASW_Objective_Countdown::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged(updateType);

	if (m_fCountdownFinishTime != 0 && !m_bLaunchedPanel)
	{
		// create panel to show countdown timer
		m_bLaunchedPanel = true;

		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );

			CASW_VGUI_Countdown_Panel *pCountdownPanel = new CASW_VGUI_Countdown_Panel( GetClientMode()->GetViewport(), "CountdownPanel", this );
			vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
			pCountdownPanel->SetScheme(scheme);			
			pCountdownPanel->SetVisible(true);
		}
	}
}