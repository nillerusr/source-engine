//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_vguiscreen.h"
#include "clientmode_tfbase.h"
#include <vgui/IVGui.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Label.h>
#include "c_info_act.h"


//-----------------------------------------------------------------------------
// Base class for all vgui screens on objects: 
//-----------------------------------------------------------------------------
class CRespawnWaveVGuiScreen : public CVGuiScreenPanel
{
	DECLARE_CLASS( CRespawnWaveVGuiScreen, CVGuiScreenPanel );

public:
	CRespawnWaveVGuiScreen( vgui::Panel *parent, const char *panelName );

	virtual bool Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData );
	virtual void OnTick();

private:
	vgui::Label *m_pTime1RemainingLabel;
	vgui::Label *m_pTime2RemainingLabel;
};


//-----------------------------------------------------------------------------
// Standard VGUI panel for objects 
//-----------------------------------------------------------------------------
DECLARE_VGUI_SCREEN_FACTORY( CRespawnWaveVGuiScreen, "respawn_wave_screen" );


//-----------------------------------------------------------------------------
// Constructor: 
//-----------------------------------------------------------------------------
CRespawnWaveVGuiScreen::CRespawnWaveVGuiScreen( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName, g_hVGuiObjectScheme ) 
{
}


//-----------------------------------------------------------------------------
// Initialization 
//-----------------------------------------------------------------------------
bool CRespawnWaveVGuiScreen::Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData )
{
	// Load all of the controls in
	if (!BaseClass::Init(pKeyValues, pInitData))
		return false;

	// Make sure we get ticked...
	vgui::ivgui()->AddTickSignal( GetVPanel() );

	// Grab ahold of certain well-known controls
	// NOTE: it is valid for these controls to not exist!
	m_pTime1RemainingLabel = dynamic_cast<vgui::Label*>(FindChildByName( "RespawnTime1Remaining" ));
	m_pTime2RemainingLabel = dynamic_cast<vgui::Label*>(FindChildByName( "RespawnTime2Remaining" ));
	
	return true;
}


//-----------------------------------------------------------------------------
// Frame-based update
//-----------------------------------------------------------------------------
void CRespawnWaveVGuiScreen::OnTick()
{
	BaseClass::OnTick();

	if (!GetEntity())
		return;

	int nTime1Remaining = 0;
	int nTime2Remaining = 0;
	if (g_hCurrentAct.Get())
	{
		nTime1Remaining = g_hCurrentAct->RespawnTimeRemaining( GetEntity()->GetTeamNumber(), 1 ); 
		nTime2Remaining = g_hCurrentAct->RespawnTimeRemaining( GetEntity()->GetTeamNumber(), 2 ); 
	}

	char buf[32];
	if (m_pTime1RemainingLabel)
	{
		Q_snprintf( buf, sizeof( buf ), "%d", nTime1Remaining );
		m_pTime1RemainingLabel->SetText( buf );
	}
	if (m_pTime2RemainingLabel)
	{
		Q_snprintf( buf, sizeof( buf ), "%d", nTime2Remaining );
		m_pTime2RemainingLabel->SetText( buf );
	}
}

