//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "c_vguiscreen.h"
#include "vgui_controls/Label.h"
#include <vgui/IVGui.h>
#include "weapon_c4.h"
#include "ienginevgui.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Control screen 
//-----------------------------------------------------------------------------
class CViewC4Panel : public CVGuiScreenPanel
{
	DECLARE_CLASS( CViewC4Panel, CVGuiScreenPanel );

public:
	CViewC4Panel( vgui::Panel *parent, const char *panelName );
	~CViewC4Panel();
	virtual bool Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData );
	virtual void OnTick();

	C_BaseCombatWeapon *GetOwningWeapon();

	virtual void ApplySchemeSettings( IScheme *pScheme );

private:
	vgui::Label *m_pTimeLabel;
};


DECLARE_VGUI_SCREEN_FACTORY( CViewC4Panel, "c4_view_panel" );

//-----------------------------------------------------------------------------
// Constructor: 
//-----------------------------------------------------------------------------
CViewC4Panel::CViewC4Panel( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, "CViewC4Panel", vgui::scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/C4Panel.res", "ClientScheme" ) ) 
{
	SetSize( 10, 10 ); // Quiet "parent not sized yet" spew
	m_pTimeLabel = new vgui::Label( this, "TimerLabel", "" );
}

CViewC4Panel::~CViewC4Panel()
{
}

void CViewC4Panel::ApplySchemeSettings( IScheme *pScheme )
{
	if( pScheme )
	{
		m_pTimeLabel->SetFgColor( pScheme->GetColor( "C4Panel_Armed", GetFgColor() ) );
	}
}

//-----------------------------------------------------------------------------
// Initialization 
//-----------------------------------------------------------------------------
bool CViewC4Panel::Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData )
{
	// Make sure we get ticked...
	vgui::ivgui()->AddTickSignal( GetVPanel() );

	if (!BaseClass::Init(pKeyValues, pInitData))
		return false;

	return true;
}

C_BaseCombatWeapon *CViewC4Panel::GetOwningWeapon()
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
// Update the screen with the latest string from the view model
//-----------------------------------------------------------------------------
void CViewC4Panel::OnTick()
{
	BaseClass::OnTick();

	SetVisible( true );

	C_BaseEntity *pEnt = GetOwningWeapon();

	C_C4 *pViewC4 = dynamic_cast<C_C4*>( pEnt );

	if( pViewC4 )
	{
		char *display = pViewC4->GetScreenText();

		if( display )
		{
			m_pTimeLabel->SetText( display );
		}
	}		
}