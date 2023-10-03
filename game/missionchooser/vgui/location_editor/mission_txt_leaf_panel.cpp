#include "mission_txt_leaf_panel.h"
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/FileOpenDialog.h>
#include "KeyValues.h"
#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

DECLARE_BUILD_FACTORY( CMission_Txt_Panel )

extern char	g_gamedir[1024];

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CMission_Txt_Panel::CMission_Txt_Panel( Panel *parent, const char *name ) : BaseClass( parent, name )
{
	m_pPickButton = new vgui::Button(this, "PickButton", "...", this, "Pick");
}

void CMission_Txt_Panel::OnCommand( const char *command )
{
	if (Q_stricmp( command, "Pick" ) == 0 )
	{
		DoPick();
	}

	BaseClass::OnCommand( command );
}


void CMission_Txt_Panel::DoPick()
{
	// Pop up the dialog
	FileOpenDialog *pFileDialog = new FileOpenDialog(this, "Pick Room Template", true);

	char template_dir[1024];
	Q_snprintf( template_dir, sizeof( template_dir ), "%s\\tilegen\\missions", g_gamedir );
	Msg( "DoPick(): missions dir is %s\n", template_dir );
	Msg( "  g_gamedir is %s\n", g_gamedir );
	pFileDialog->SetStartDirectory( template_dir );
	pFileDialog->AddFilter("*.txt", "Mission txt (*.txt)", true);
	pFileDialog->AddActionSignalTarget(this);

	pFileDialog->DoModal(false);
}

MessageMapItem_t CMission_Txt_Panel::m_MessageMap[] =
{
	MAP_MESSAGE_CONSTCHARPTR(CMission_Txt_Panel, "FileSelected", OnFileSelected, "fullpath"), 
};

IMPLEMENT_PANELMAP(CMission_Txt_Panel, CKV_Leaf_Panel);

void CMission_Txt_Panel::OnFileSelected( const char *fullpath )
{
	char buffer[MAX_PATH];
	bool bConverted = g_pFullFileSystem->FullPathToRelativePath( fullpath, buffer, MAX_PATH );
	if ( !bConverted )
	{
		Warning( "Failed to convert this to a relative path: %s\n", fullpath );
		return;
	}	
	Q_FixSlashes( buffer );
	m_pTextEntry->SetText( buffer );
	TextEntryChanged( m_pTextEntry );
}

void CMission_Txt_Panel::PerformLayout()
{
	BaseClass::PerformLayout();

	int iLabelSpace = MAX( m_pLabel->GetWide() + 5, 100 );
	m_pTextEntry->SetBounds( iLabelSpace, 0, 390 - ( iLabelSpace + 25 ), 20 );
	SetSize( 420, 20 );

	m_pPickButton->SetBounds( 367, 0, 25, 20 );
}