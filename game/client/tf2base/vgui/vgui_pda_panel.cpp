//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "c_vguiscreen.h"
#include <vgui/IVGui.h>
#include "ienginevgui.h"
#include "vgui_bitmapimage.h"
#include "vgui_bitmappanel.h"
#include "vgui_controls/Label.h"
#include "tf_weapon_pda.h"
#include "vgui/ISurface.h"
#include "c_tf_player.h"
#include <vgui_controls/RadioButton.h>
#include "clientmode.h"
#include <vgui_controls/ProgressBar.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Control screen 
//-----------------------------------------------------------------------------
class CPDAPanel : public CVGuiScreenPanel
{
	DECLARE_CLASS( CPDAPanel, CVGuiScreenPanel );

public:
	CPDAPanel( vgui::Panel *parent, const char *panelName );
	~CPDAPanel();
	virtual bool Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData );
	virtual void OnTick();

protected:
	C_BaseCombatWeapon *GetOwningWeapon();
};

DECLARE_VGUI_SCREEN_FACTORY( CPDAPanel, "pda_panel" );

//-----------------------------------------------------------------------------
// Constructor: 
//-----------------------------------------------------------------------------
CPDAPanel::CPDAPanel( vgui::Panel *parent, const char *panelName )
: BaseClass( parent, "CPDAPanel", vgui::scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/PDAControlPanelScheme.res", "TFBase" ) ) 
{
}

CPDAPanel::~CPDAPanel()
{
}

//-----------------------------------------------------------------------------
// Initialization 
//-----------------------------------------------------------------------------
bool CPDAPanel::Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData )
{
	// Make sure we get ticked...
	vgui::ivgui()->AddTickSignal( GetVPanel() );

	if (!BaseClass::Init(pKeyValues, pInitData))
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Returns the object it's attached to 
//-----------------------------------------------------------------------------
C_BaseCombatWeapon *CPDAPanel::GetOwningWeapon()
{
	C_BaseEntity *pScreenEnt = GetEntity();
	if (!pScreenEnt)
		return NULL;

	C_BaseEntity *pOwner = pScreenEnt->GetOwnerEntity();
	if (!pOwner)
		return NULL;

	C_BaseViewModel *pViewModel = dynamic_cast< C_BaseViewModel * >( pOwner );
	if ( !pViewModel )
		return NULL;

	return pViewModel->GetOwningWeapon();
}

//-----------------------------------------------------------------------------
// Frame-based update
//-----------------------------------------------------------------------------
void CPDAPanel::OnTick()
{
	BaseClass::OnTick();

	SetVisible( true );
}

//-----------------------------------------------------------------------------
// Engineer Destroy PDA
//-----------------------------------------------------------------------------

class CPDAPanel_Engineer_Destroy : public CPDAPanel
{
	DECLARE_CLASS( CPDAPanel_Engineer_Destroy, CPDAPanel );

public:
	CPDAPanel_Engineer_Destroy( vgui::Panel *parent, const char *panelName );
};

DECLARE_VGUI_SCREEN_FACTORY( CPDAPanel_Engineer_Destroy, "pda_panel_engineer_destroy" );

//-----------------------------------------------------------------------------
// Constructor: 
//-----------------------------------------------------------------------------
CPDAPanel_Engineer_Destroy::CPDAPanel_Engineer_Destroy( vgui::Panel *parent, const char *panelName )
: CPDAPanel( parent, "CPDAPanel_Engineer_Destroy" ) 
{
}

//-----------------------------------------------------------------------------
// Engineer Build PDA
//-----------------------------------------------------------------------------

class CPDAPanel_Engineer_Build : public CPDAPanel
{
	DECLARE_CLASS( CPDAPanel_Engineer_Build, CPDAPanel );

public:
	CPDAPanel_Engineer_Build( vgui::Panel *parent, const char *panelName );
};

DECLARE_VGUI_SCREEN_FACTORY( CPDAPanel_Engineer_Build, "pda_panel_engineer_build" );

//-----------------------------------------------------------------------------
// Constructor: 
//-----------------------------------------------------------------------------
CPDAPanel_Engineer_Build::CPDAPanel_Engineer_Build( vgui::Panel *parent, const char *panelName )
: CPDAPanel( parent, "CPDAPanel_Engineer" ) 
{
}

//-----------------------------------------------------------------------------
// Spy PDA
//-----------------------------------------------------------------------------

class CPDAPanel_Spy : public CPDAPanel
{
	DECLARE_CLASS( CPDAPanel_Spy, CPDAPanel );

public:
	CPDAPanel_Spy( vgui::Panel *parent, const char *panelName );
};

DECLARE_VGUI_SCREEN_FACTORY( CPDAPanel_Spy, "pda_panel_spy" );

//-----------------------------------------------------------------------------
// Constructor: 
//-----------------------------------------------------------------------------
CPDAPanel_Spy::CPDAPanel_Spy( vgui::Panel *parent, const char *panelName )
: CPDAPanel( parent, "CPDAPanel_Spy" ) 
{
}

//-----------------------------------------------------------------------------
// Spy Invis PDA
//-----------------------------------------------------------------------------

class CPDAPanel_Spy_Invis : public CPDAPanel
{
	DECLARE_CLASS( CPDAPanel_Spy_Invis, CPDAPanel );

public:
	CPDAPanel_Spy_Invis( vgui::Panel *parent, const char *panelName );

	virtual void OnTick();

private:

	ProgressBar *m_pInvisProgress;
};

DECLARE_VGUI_SCREEN_FACTORY( CPDAPanel_Spy_Invis, "pda_panel_spy_invis" );

//-----------------------------------------------------------------------------
// Constructor: 
//-----------------------------------------------------------------------------
CPDAPanel_Spy_Invis::CPDAPanel_Spy_Invis( vgui::Panel *parent, const char *panelName )
: CPDAPanel( parent, "CPDAPanel_Spy_Invis" ) 
{
	vgui::ivgui()->AddTickSignal( GetVPanel() );

	m_pInvisProgress = new ProgressBar( this, "InvisProgress" );
}

//-----------------------------------------------------------------------------
// Update the progress bar with how much invis we have left
//-----------------------------------------------------------------------------
void CPDAPanel_Spy_Invis::OnTick( void )
{
	C_BaseCombatWeapon *pInvisWeapon = GetOwningWeapon();

	if ( !pInvisWeapon )
		return;

	C_TFPlayer *pPlayer = ToTFPlayer( pInvisWeapon->GetOwner() );

	if ( pPlayer && !pPlayer->IsDormant() )
	{
		if ( m_pInvisProgress )
		{
			m_pInvisProgress->SetProgress( pPlayer->m_Shared.GetSpyCloakMeter() / 100.0f );
		}
	}	
}