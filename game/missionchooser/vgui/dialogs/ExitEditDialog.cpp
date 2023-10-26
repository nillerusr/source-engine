#include <vgui/IVGui.h>
#include <vgui/IInput.h>
#include "vgui_controls/Controls.h"
#include <vgui/IScheme.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/RichText.h>
#include <vgui_controls/MessageBox.h>
#include <vgui_controls/CheckButton.h>
#include <KeyValues.h>

#include "ExitEditDialog.h"
#include "TileSource/LevelTheme.h"
#include "TileSource/RoomTemplate.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

CExitEditDialog *g_pExitEditDialog = NULL;

CExitEditDialog::CExitEditDialog( Panel *parent, const char *name, CRoomTemplate *pRoomTemplate, CRoomTemplateExit *pExit ) : BaseClass( parent, name )
{
	Assert( !g_pExitEditDialog );
	g_pExitEditDialog = this;

	m_pExit = pExit;
	m_pRoomTemplate = pRoomTemplate;
	m_pExitTagEdit = new vgui::TextEntry(this, "ExitTagEdit");

	m_pExitTagEdit->SetText( m_pExit->m_szExitTag );

	m_pChokeGrowCheck = new vgui::CheckButton( this, "ChokeGrow", "Chokepoing Grow Source" );
	m_pChokeGrowCheck->SetSelected( m_pExit->m_bChokepointGrowSource );

	//SetSize(384, 320);
	//SetMinimumSize(200, 50);

	SetMinimizeButtonVisible( false );
	SetCloseButtonVisible( false );
		
	LoadControlSettings( "tilegen/ExitEditDialog.res", "GAME" );

	SetDeleteSelfOnClose( true );

	SetSizeable( false );
	MoveToCenterOfScreen();
}

CExitEditDialog::~CExitEditDialog( void )
{
	g_pExitEditDialog = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Handles dialog commands
//-----------------------------------------------------------------------------
void CExitEditDialog::OnCommand( const char *command )
{
	if ( Q_stricmp( command, "Okay" ) == 0 )
	{
		// copy new exit tag name
		m_pExitTagEdit->GetText(m_pExit->m_szExitTag, sizeof(m_pExit->m_szExitTag));
		m_pExit->m_bChokepointGrowSource = m_pChokeGrowCheck->IsSelected();

		// NOTE: no need to save the room template here as it'll get saved when we close the room template edit dialog that launched us
		OnClose();
	}
	else if (Q_stricmp( command, "Close" ) == 0 )
	{
		
	}
	BaseClass::OnCommand( command );
}