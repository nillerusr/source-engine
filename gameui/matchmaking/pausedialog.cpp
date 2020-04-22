//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Multiplayer pause menu
//
//=============================================================================//

#include "pausedialog.h"
#include "GameUI_Interface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//--------------------------------
// CPauseDialog
//--------------------------------
CPauseDialog::CPauseDialog( vgui::Panel *pParent ) : BaseClass( pParent, "PauseDialog" )
{
	// do nothing
} 

void CPauseDialog::Activate( void )
{
	BaseClass::Activate();

	SetDeleteSelfOnClose( false );
	m_Menu.SetFocus( 0 );
}

//-------------------------------------------------------
// Keyboard input
//-------------------------------------------------------
void CPauseDialog::OnKeyCodePressed( vgui::KeyCode code )
{
	switch( code )
	{
	case KEY_XBUTTON_B:
		if ( GameUI().IsInLevel() )
		{
			m_pParent->OnCommand( "ResumeGame" );
		}
		break;

	default:
		BaseClass::OnKeyCodePressed( code );
		break;
	}
}