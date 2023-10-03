#include <vgui/IVGui.h>
#include <vgui/IInput.h>
#include "vgui_controls/Controls.h"
#include <vgui/IScheme.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/Slider.h>
#include <vgui_controls/MessageBox.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/PanelListPanel.h>
#include <vgui_controls/Tooltip.h>
#include <vgui_controls/FileOpenDialog.h>
#include <vgui_controls/perforcefilelistframe.h>
#include <KeyValues.h>
#include "filesystem.h"
#include "TagList.h"
#include "RoomTemplatePanel.h"
#include "RoomTemplateEditDialog.h"
#include "ExitEditDialog.h"
#include "TileSource/LevelTheme.h"
#include "TileSource/RoomTemplate.h"
#include "ToggleExitsPanel.h"
#include "TileGenDialog.h"
#include "MapLayout.h"
#include "Room.h"
#include "cdll_int.h"
#include "p4lib/ip4.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

extern char	g_gamedir[1024];
extern IVEngineClient *engine;

#define MAX_ROOM_SIZE 32

MessageMapItem_t CRoomTemplateEditDialog::m_MessageMap[] =
{
	MAP_MESSAGE_CONSTCHARPTR(CRoomTemplateEditDialog, "FileSelected", OnFileSelected, "fullpath"), 
};

IMPLEMENT_PANELMAP(CRoomTemplateEditDialog, Frame);

CRoomTemplateEditDialog::CRoomTemplateEditDialog( Panel *parent, const char *name, CRoomTemplate *pRoomTemplate, bool bCreatingNew ) : BaseClass( parent, name )
{
	m_bCreatingNew = bCreatingNew;

	m_pRoomTemplate = pRoomTemplate;
	m_pThemeLabel = new vgui::Label(this, "RoomTemplateTheme", "Unknown");
	m_pPickVMFButton = new vgui::Button(this, "PickVMFButton", "...", this, "PickVMF");
	m_pRoomTemplateNameEdit = new vgui::TextEntry(this, "RoomTemplateNameEdit");
	m_pRoomTemplateDescriptionEdit = new vgui::TextEntry(this, "RoomTemplateDescriptionEdit");
	m_pRoomTemplateSoundscapeEdit = new vgui::TextEntry(this, "RoomTemplateSoundscapeEdit");
	
	m_pToggleExitsPanel = new CToggleExitsPanel( this, "CToggleExitsPanel" );

	m_pRoomTemplatePanel = new CRoomTemplatePanel(this, "RoomTemplatePanel");
	m_pRoomTemplatePanel->SetRoomTemplate(m_pRoomTemplate);
	m_pRoomTemplatePanel->m_bRoomTemplateEditMode = true;
	m_pRoomTemplatePanel->m_bForceShowExits = true;
	m_pRoomTemplatePanel->m_bForceShowTileSquares = true;

	m_pToggleExitsPanel->SetRoomTemplatePanel( m_pRoomTemplatePanel );

	m_pSpawnWeightSlider = new vgui::Slider( this, "SpawnWeightSlider" );
	m_pSpawnWeightSlider->SetRange( MIN_SPAWN_WEIGHT, MAX_SPAWN_WEIGHT );
	m_pSpawnWeightSlider->SetNumTicks( MAX_SPAWN_WEIGHT - MIN_SPAWN_WEIGHT );
	m_pSpawnWeightValue = new Label( this, "SpawnWeightValueLabel", "0" );

	// Combo box for room types
	m_pTileTypeBox = new vgui::ComboBox( this, "TileTypeCombo", ASW_TILETYPE_COUNT, false );
	for ( int iType = 0; iType < ASW_TILETYPE_COUNT; ++iType )
	{
		m_pTileTypeBox->AddItem( g_szASWTileTypeStrings[iType], NULL );
	}
	if ( m_pTileTypeBox->IsItemIDValid( ASW_TILETYPE_COUNT ) )
	{
		m_pTileTypeBox->SetText( g_szASWTileTypeStrings[ASW_TILETYPE_UNKNOWN] );
	}

	char buffer[256];
	Q_snprintf( buffer, _countof( buffer ), "%d", MAX_ROOM_SIZE );
	m_pTilesXSlider = new vgui::Slider(this, "TilesXSlider");
	m_pTilesXSlider->SetRange( 1, MAX_ROOM_SIZE );
	m_pTilesXSlider->SetNumTicks( MAX_ROOM_SIZE - 1 );
	m_pTilesXSlider->SetTickCaptions("1", buffer);
	m_pTilesYSlider = new vgui::Slider(this, "TilesYSlider");
	m_pTilesYSlider->SetRange( 1, MAX_ROOM_SIZE );
	m_pTilesYSlider->SetNumTicks( MAX_ROOM_SIZE - 1 );
	m_pTilesYSlider->SetTickCaptions("1", buffer);
	m_pTilesXValue = new Label(this, "TilesWideValueLabel", "1");
	m_pTilesYValue = new Label(this, "TilesTallValueLabel", "2");
	
	m_pTagListPanel = new PanelListPanel(this, "TagListPanel");
	m_pTagListPanel->SetShowScrollBar( true );
	m_pTagListPanel->SetFirstColumnWidth( 0 );
	m_pTagListPanel->SetVerticalBufferPixels( 0 );
	// add all tags
	if ( TagList() )
	{
		for ( int i=0;i<TagList()->GetNumTags();i++ )
		{
			vgui::CheckButton *pCheckbutton = new vgui::CheckButton( m_pTagListPanel, "TagCheckButton", TagList()->GetTag(i) );
			pCheckbutton->AddActionSignalTarget( this );
			if ( pCheckbutton->GetTooltip() )
			{
				pCheckbutton->GetTooltip()->SetText( TagList()->GetTagDescription( i ) );
			}
			if ( m_pRoomTemplate->HasTag( TagList()->GetTag(i) ) )
			{
				pCheckbutton->SetSelected( true );
			}
			m_pTagListPanel->AddItem( NULL, pCheckbutton );
		}
	}

	m_iSelectedTileX = -1;
	m_iSelectedTileY = -1;

	SetMinimizeButtonVisible( false );
	SetCloseButtonVisible( true );
		
	LoadControlSettings( "tilegen/RoomTemplateEditDialog.res", "GAME" );

	m_pThemeLabel->SetText(pRoomTemplate->m_pLevelTheme->m_szName);
	if (!bCreatingNew)
	{
		m_pRoomTemplateNameEdit->SetText( pRoomTemplate->GetFullName() );
		m_pRoomTemplateNameEdit->SetEditable( false );
		m_pPickVMFButton->SetEnabled( false );
		m_pRoomTemplateDescriptionEdit->SetText( pRoomTemplate->GetDescription() );
		m_pRoomTemplateSoundscapeEdit->SetText( pRoomTemplate->GetSoundscape() );

		m_pSpawnWeightSlider->SetValue( m_pRoomTemplate->GetSpawnWeight() );
		m_pTilesXSlider->SetValue( m_pRoomTemplate->GetTilesX() );
		m_pTilesYSlider->SetValue( m_pRoomTemplate->GetTilesY() );
		char buffer[12];
		Q_snprintf( buffer, _countof( buffer ), "%d", m_pRoomTemplate->GetSpawnWeight() );
		m_pSpawnWeightValue->SetText( buffer );
		Q_snprintf( buffer, _countof( buffer ), "%d", m_pRoomTemplate->GetTilesX() );
		m_pTilesXValue->SetText( buffer );
		Q_snprintf( buffer, _countof( buffer ), "%d", m_pRoomTemplate->GetTilesY() );
		m_pTilesYValue->SetText( buffer );

		// Set value for tile type.
		int nTileType = m_pRoomTemplate->GetTileType();
		if ( m_pTileTypeBox->IsItemIDValid( nTileType ) )
		{
			m_pTileTypeBox->SetText( g_szASWTileTypeStrings[nTileType] );
		}
	}

	SetDeleteSelfOnClose( true );

	SetSizeable( true );
	MoveToCenterOfScreen();
}

CRoomTemplateEditDialog::~CRoomTemplateEditDialog( void )
{
	// @TODO: in some cases, it appears that we leak the room template pointer (and possibly this dialog itself)
}

//-----------------------------------------------------------------------------
// Purpose: Handles dialog commands
//-----------------------------------------------------------------------------
void CRoomTemplateEditDialog::OnCommand( const char *command )
{
	if ( Q_stricmp( command, "Okay" ) == 0 )
	{
		const int nBufLength = MAX( CRoomTemplate::m_nMaxDescriptionLength, CRoomTemplate::m_nMaxSoundscapeLength );
		char buf[nBufLength];

		// NOTE: Don't need to copy over tilesx/y and exits as those are changed in the template directly
		m_pRoomTemplateDescriptionEdit->GetText( buf, nBufLength );
		m_pRoomTemplate->SetDescription( buf );
		m_pRoomTemplateSoundscapeEdit->GetText( buf, nBufLength );
		m_pRoomTemplate->SetSoundscape( buf );

		if ( m_bCreatingNew )
		{
			char name[MAX_PATH];
			m_pRoomTemplateNameEdit->GetText( name, MAX_PATH );
			if ( Q_strlen( name ) <= 0 )
			{
				MessageBox *pMessage = new MessageBox( "Bad Room template Name", "Please enter a valid name for this room template", this );
				pMessage->DoModal();
				return;
			}
			m_pRoomTemplate->SetFullName( name );
		}

		char relativePath[MAX_PATH], fullPath[MAX_PATH];
		Q_snprintf( relativePath, _countof( relativePath ), "tilegen/roomtemplates/%s/%s.roomtemplate", m_pRoomTemplate->m_pLevelTheme->m_szName, m_pRoomTemplate->GetFullName() );
		// This does not work if the file doesn't exist
		g_pFullFileSystem->RelativePathToFullPath( relativePath, "GAME", fullPath, MAX_PATH );

		if ( m_bCreatingNew )
		{
			if ( g_pFullFileSystem->FileExists( fullPath ) )
			{
				MessageBox *pMessage = new MessageBox( "File Exists", "Room template already exists!", this );
				pMessage->DoModal();
				return;
			}
		}

		if ( !m_pRoomTemplate->SaveRoomTemplate() )
		{
			if ( !m_bCreatingNew && p4 && p4->IsFileInPerforce( fullPath ) )
			{
				MessageBox *pMessage = new MessageBox( "Check Out?", "File is not writeable. Would you like to check it out from Perforce?", this );
				pMessage->SetCancelButtonVisible( true );
				pMessage->SetOKButtonText( "#MessageBox_Yes" );
				pMessage->SetCancelButtonText( "#MessageBox_No" );
				pMessage->SetCommand( new KeyValues( "CheckOutFromP4", "file", fullPath ) );
				pMessage->DoModal();
				return;
			}
			else
			{
				VGUIMessageBox( this, "Save Error", "Failed to save %s.roomtemplate.  Make sure file is checked out from Perforce.", m_pRoomTemplate->GetFullName() );
				return;
			}
		}
		else
		{
			if ( m_bCreatingNew && p4 )
			{
				// Need to re-resolve the path now that the file has been created.
				g_pFullFileSystem->RelativePathToFullPath( relativePath, "GAME", fullPath, MAX_PATH );
				MessageBox *pMessage = new MessageBox( "Add to P4?", "Would you like to add this template to perforce?", this );
				pMessage->SetCancelButtonVisible( true );
				pMessage->SetOKButtonText( "#MessageBox_Yes" );
				pMessage->SetCancelButtonText( "#MessageBox_No" );
				pMessage->SetCommand( new KeyValues( "AddToP4", "file", fullPath ) );
				pMessage->SetCancelCommand( new KeyValues( "AddToP4", "file", NULL ) );
				pMessage->DoModal();
				return;
			}
		}

		if ( m_bCreatingNew )
		{
			CLevelTheme::s_pCurrentTheme->m_RoomTemplates.Insert( m_pRoomTemplate );
		}

		PostActionSignal(new KeyValues("UpdateCurrentTheme"));	// make the TileGenDialog update the current theme (will update the room template display too)
		OnClose();
	}
	else if ( !Q_stricmp( command, "Close" ) )
	{
		if (m_bCreatingNew)
		{
			delete m_pRoomTemplate;
			m_pRoomTemplate = NULL;
		}
		else
		{
			// reload the room template from disk to undo any changes we may have made
			char szFullFileName[256];
			Q_snprintf(szFullFileName, _countof(szFullFileName), "tilegen/roomtemplates/%s/%s.roomtemplate", m_pRoomTemplate->m_pLevelTheme->m_szName, m_pRoomTemplate->GetFullName());
			KeyValues *pRoomTemplateKeyValues = new KeyValues( m_pRoomTemplate->GetFullName() );
			if (pRoomTemplateKeyValues->LoadFromFile(g_pFullFileSystem, szFullFileName, "GAME"))
			{
				char buffer[MAX_PATH];
				Q_snprintf( buffer, _countof( buffer ), "%s", m_pRoomTemplate->GetFullName() );
				m_pRoomTemplate->LoadFromKeyValues( buffer, pRoomTemplateKeyValues );
			}
			else
			{
				Msg("Error: failed to load room template %s\n", szFullFileName);
			}
			pRoomTemplateKeyValues->deleteThis();
		}
	}
	else if (Q_stricmp( command, "EditLights" ) == 0 )
	{
		OnClose();
		CMapLayout *pMapLayout = new CMapLayout;
		new CRoom( pMapLayout, m_pRoomTemplate, MAP_LAYOUT_TILES_WIDE / 2, MAP_LAYOUT_TILES_WIDE / 2 );
		pMapLayout->SaveMapLayout( "maps/output.layout" );
		char buffer[256];
		Q_snprintf(buffer, _countof(buffer), "asw_random_weapons 0; asw_money 0; asw_build_map %s edit %s", "output.layout", GetVMFFilename() );
		engine->ClientCmd_Unrestricted( buffer );
		delete pMapLayout;
	}
	else if (Q_stricmp( command, "GenerateNav" ) == 0 )
	{
		OnClose();
		CMapLayout *pMapLayout = new CMapLayout;
		new CRoom( pMapLayout, m_pRoomTemplate, MAP_LAYOUT_TILES_WIDE / 2, MAP_LAYOUT_TILES_WIDE / 2 );
		pMapLayout->SaveMapLayout( "maps/output.layout" );
		char buffer[256];
		Q_snprintf(buffer, _countof(buffer), "asw_random_weapons 0; asw_money 0; asw_director_spawn_npcs 0; asw_spawner_spawn_npcs 0; asw_build_map %s; asw_generate_nav 1", "output.layout", GetVMFFilename() );
		engine->ClientCmd_Unrestricted( buffer );
		delete pMapLayout;
	}
	else if (Q_stricmp( command, "ClearAllExits" ) == 0 )
	{
		if (!m_pRoomTemplate)
			return;
		// remove all room exits
		m_pRoomTemplate->m_Exits.PurgeAndDeleteElements();		
		m_pRoomTemplatePanel->SetRoomTemplate(m_pRoomTemplate);	// update our room template display
		m_pToggleExitsPanel->SetRoomTemplatePanel( m_pRoomTemplatePanel, true );
		m_pRoomTemplatePanel->InvalidateLayout(true);
		Repaint();
	}
	else if (Q_stricmp( command, "ClearExitsFromTile" ) == 0 )
	{
		if (!m_pRoomTemplate)
			return;
		// go through all room exits and delete any in the selected tile
		int iExits = m_pRoomTemplate->m_Exits.Count();
		for (int i=iExits-1;i>=0;i--)
		{
			CRoomTemplateExit *pExit = m_pRoomTemplate->m_Exits[i];
			if (pExit->m_iXPos == m_iSelectedTileX && pExit->m_iYPos == m_iSelectedTileY)
			{
				m_pRoomTemplate->m_Exits.Remove(i);
				delete pExit;
			}
		}
		m_pRoomTemplatePanel->SetRoomTemplate(m_pRoomTemplate);	// update our room template display
		m_pToggleExitsPanel->SetRoomTemplatePanel( m_pRoomTemplatePanel, true );
		m_pRoomTemplatePanel->InvalidateLayout(true);
		Repaint();
	}
	else if (Q_stricmp( command, "AddExitNorth" ) == 0 )
	{
		AddExit(m_iSelectedTileX, m_iSelectedTileY, EXITDIR_NORTH);
	}
	else if (Q_stricmp( command, "AddExitEast" ) == 0 )
	{
		AddExit(m_iSelectedTileX, m_iSelectedTileY, EXITDIR_EAST);
	}
	else if (Q_stricmp( command, "AddExitSouth" ) == 0 )
	{
		AddExit(m_iSelectedTileX, m_iSelectedTileY, EXITDIR_SOUTH);
	}
	else if (Q_stricmp( command, "AddExitWest" ) == 0 )
	{
		AddExit(m_iSelectedTileX, m_iSelectedTileY, EXITDIR_WEST);
	}
	else if (Q_stricmp( command, "EditExitNorth" ) == 0 )
	{
		EditExit(m_iSelectedTileX, m_iSelectedTileY, EXITDIR_NORTH);
	}
	else if (Q_stricmp( command, "EditExitEast" ) == 0 )
	{
		EditExit(m_iSelectedTileX, m_iSelectedTileY, EXITDIR_EAST);
	}
	else if (Q_stricmp( command, "EditExitSouth" ) == 0 )
	{
		EditExit(m_iSelectedTileX, m_iSelectedTileY, EXITDIR_SOUTH);
	}
	else if (Q_stricmp( command, "EditExitWest" ) == 0 )
	{
		EditExit(m_iSelectedTileX, m_iSelectedTileY, EXITDIR_WEST);
	}
	else if (Q_stricmp( command, "PickVMF" ) == 0 )
	{
		DoPickVMF();
	}
	
	BaseClass::OnCommand( command );
}

void CRoomTemplateEditDialog::OnCheckOutFromP4( KeyValues *pKV )
{
	const char *pPath = pKV->GetString( "file", NULL );
	if ( pPath != NULL )
	{
		if ( p4->OpenFileForEdit( pPath ) )
		{
			if ( !m_pRoomTemplate->SaveRoomTemplate() )
			{
				VGUIMessageBox( this, "Save Error!", "Checked out '%s' from Perforce, but failed to save file.", pPath );
			}
		}
		else
		{
			VGUIMessageBox( this, "P4 Error!", "Failed to check out '%s' from Perforce.", pPath );			
		}
	}
	PostActionSignal(new KeyValues("UpdateCurrentTheme"));	// make the TileGenDialog update the current theme (will update the room template display too)
	OnClose();
}

void CRoomTemplateEditDialog::OnAddToP4( KeyValues *pKV )
{
	const char *pPath = pKV->GetString( "file", NULL );
	if ( pPath != NULL )
	{
		if ( p4->OpenFileForAdd( pPath ) )
		{
			char vmfPath[MAX_PATH];
			Q_strncpy( vmfPath, pPath, MAX_PATH );
			Q_SetExtension( vmfPath, "vmf", MAX_PATH );
			if ( g_pFullFileSystem->FileExists( vmfPath ) )
			{
				p4->OpenFileForAdd( vmfPath );
			}
		}
		else
		{
			VGUIMessageBox( this, "P4 Error!", "Failed to add '%s' to Perforce.", pPath );
		}
	}
	CLevelTheme::s_pCurrentTheme->m_RoomTemplates.Insert( m_pRoomTemplate );
	PostActionSignal(new KeyValues("UpdateCurrentTheme"));	// make the TileGenDialog update the current theme (will update the room template display too)
	OnClose();
}

void CRoomTemplateEditDialog::OnSliderMoved(vgui::Panel* pSlider)
{
	char buffer[12];

	if ( pSlider == m_pSpawnWeightSlider )
	{
		m_pRoomTemplate->SetSpawnWeight( m_pSpawnWeightSlider->GetValue() );
		Q_snprintf( buffer, _countof( buffer ), "%d", m_pRoomTemplate->GetSpawnWeight() );
		m_pSpawnWeightValue->SetText( buffer );
	}
	else
	{
		m_pRoomTemplate->SetTileSize( m_pTilesXSlider->GetValue(), m_pTilesYSlider->GetValue() );
		Q_snprintf( buffer, _countof( buffer ), "%d", m_pRoomTemplate->GetTilesX() );
		m_pTilesXValue->SetText(buffer);
		Q_snprintf( buffer, _countof( buffer ), "%d", m_pRoomTemplate->GetTilesY() );
		m_pTilesYValue->SetText( buffer );

		m_pRoomTemplatePanel->SetRoomTemplate( m_pRoomTemplate );	// update our room template display
		m_pToggleExitsPanel->SetRoomTemplatePanel( m_pRoomTemplatePanel, true );
		m_pRoomTemplatePanel->InvalidateLayout( true );
		m_pToggleExitsPanel->InvalidateLayout();
		Repaint();
	}
}

void CRoomTemplateEditDialog::AddExit(int iXPos, int iYPos, ExitDirection_t dir)
{
	// check we don't already have an exit in this place
	int iExits = m_pRoomTemplate->m_Exits.Count();
	for (int i=iExits-1;i>=0;i--)
	{
		CRoomTemplateExit *pExit = m_pRoomTemplate->m_Exits[i];
		if (pExit->m_iXPos == iXPos && pExit->m_iYPos == iYPos && dir == pExit->m_ExitDirection)
			return;
	}

	CRoomTemplateExit *pExit = new CRoomTemplateExit();
	pExit->m_ExitDirection = dir;
	pExit->m_iXPos = iXPos;
	pExit->m_iYPos = iYPos;
	pExit->m_iZChange = 0;	// todo: let the user set this somehow?
	m_pRoomTemplate->m_Exits.AddToTail(pExit);

	m_pRoomTemplatePanel->SetRoomTemplate(m_pRoomTemplate);	// update our room template display
	m_pToggleExitsPanel->SetRoomTemplatePanel( m_pRoomTemplatePanel, true );
	m_pRoomTemplatePanel->InvalidateLayout(true);
	Repaint();
}

void CRoomTemplateEditDialog::ToggleExit(int iXPos, int iYPos, ExitDirection_t dir)
{
	int iExits = m_pRoomTemplate->m_Exits.Count();
	for (int i=iExits-1;i>=0;i--)
	{
		CRoomTemplateExit *pExit = m_pRoomTemplate->m_Exits[i];
		if (pExit->m_iXPos == iXPos && pExit->m_iYPos == iYPos && dir == pExit->m_ExitDirection)
		{
			m_pRoomTemplate->m_Exits.Remove( i );
			m_pRoomTemplatePanel->SetRoomTemplate(m_pRoomTemplate);	// update our room template display
			m_pToggleExitsPanel->SetRoomTemplatePanel( m_pRoomTemplatePanel, true );
			m_pRoomTemplatePanel->InvalidateLayout(true);
			Repaint();
			return;
		}
	}

	CRoomTemplateExit *pExit = new CRoomTemplateExit();
	pExit->m_ExitDirection = dir;
	pExit->m_iXPos = iXPos;
	pExit->m_iYPos = iYPos;
	pExit->m_iZChange = 0;	// todo: let the user set this somehow?
	m_pRoomTemplate->m_Exits.AddToTail(pExit);

	m_pRoomTemplatePanel->SetRoomTemplate(m_pRoomTemplate);	// update our room template display
	m_pToggleExitsPanel->SetRoomTemplatePanel( m_pRoomTemplatePanel, true );
	m_pRoomTemplatePanel->InvalidateLayout(true);
	Repaint();
}

void CRoomTemplateEditDialog::EditExit(int iXPos, int iYPos, ExitDirection_t dir)
{
	// find exit in this place
	int iExits = m_pRoomTemplate->m_Exits.Count();
	for (int i=iExits-1;i>=0;i--)
	{
		CRoomTemplateExit *pExit = m_pRoomTemplate->m_Exits[i];
		if (pExit->m_iXPos == iXPos && pExit->m_iYPos == iYPos && dir == pExit->m_ExitDirection)
		{
			// Spawn new dialog for editing pExit
			CExitEditDialog *pDialog = new CExitEditDialog( this, "ExitEditDialog", m_pRoomTemplate, pExit );
			pDialog->DoModal();
		}
	}
}

void CRoomTemplateEditDialog::OnCheckButtonChecked( vgui::Panel *panel )
{
	vgui::CheckButton *pCheckButton = dynamic_cast<vgui::CheckButton*>(panel);
	if ( !pCheckButton )
		return;

	// NOTE: this assumes the checkbutton is a button in the taglist
	char tag[128];
	pCheckButton->GetText( tag, _countof( tag ) );

	if ( pCheckButton->IsSelected() )
	{
		m_pRoomTemplate->AddTag( tag );
	}
	else
	{
		m_pRoomTemplate->RemoveTag( tag );
	}
}

void CRoomTemplateEditDialog::DoPickVMF()
{
	// Pop up the dialog
	FileOpenDialog *pFileDialog = new FileOpenDialog(this, "Set Room Template", true);

	char template_dir[1024];
	Q_snprintf( template_dir, _countof( template_dir ), "%s\\tilegen\\roomtemplates\\%s", g_gamedir, m_pRoomTemplate->m_pLevelTheme->m_szName );
	pFileDialog->SetStartDirectory( template_dir );
	pFileDialog->AddFilter("*.vmf", "Map file (*.vmf)", true);
	pFileDialog->AddActionSignalTarget(this);

	pFileDialog->DoModal(false);
}

void CRoomTemplateEditDialog::OnFileSelected( const char *fullpath )
{
	const char *pThemeStart = Q_strstr( fullpath, m_pRoomTemplate->m_pLevelTheme->m_szName );
	if ( !pThemeStart )
	{
		Warning( "Failed to pull theme name out of selected file.  Make sure vmf file is in the correct folder under tilegen/roomtemplates.\n" );
		return;
	}
	
	char themeName[MAX_PATH], roomName[MAX_PATH];
	if ( !CLevelTheme::SplitThemeAndRoom( pThemeStart, themeName, MAX_PATH, roomName, MAX_PATH ) )
	{
		Warning( "Failed to SplitThemeAndRoom while selecting file %s\n", pThemeStart );
		return;
	}
	m_pRoomTemplateNameEdit->SetText( roomName );

	// now try to extract room dimensions from the name

	char szBaseRoom[128];
	Q_FileBase( roomName, szBaseRoom, _countof( szBaseRoom ) );	
	char* xPos = Q_stristr( szBaseRoom, "x" );
	char* _Pos = Q_stristr( szBaseRoom, "_" );

	if ( xPos && _Pos )
	{
		char width[12];
		char height[12];
		int width_digits = xPos - szBaseRoom;
		int height_digits = ( _Pos - xPos ) - 1;
		Q_strncpy( width, szBaseRoom, width_digits + 1 );
		width[width_digits] = 0;
		Q_strncpy( height, xPos + 1, height_digits + 1 );
		height[height_digits] = 0;

		m_pRoomTemplate->SetTileSize( atoi( width ), atoi( height ) );
		m_pSpawnWeightSlider->SetValue( m_pRoomTemplate->GetSpawnWeight() );
		m_pTilesXSlider->SetValue( m_pRoomTemplate->GetTilesX() );
		m_pTilesYSlider->SetValue( m_pRoomTemplate->GetTilesY() );
	}
}

const char* CRoomTemplateEditDialog::GetVMFFilename()
{
	static char buffer[MAX_PATH];

	Q_snprintf( buffer, _countof( buffer ), "tilegen/roomtemplates/%s/%s", m_pRoomTemplate->m_pLevelTheme->m_szName, m_pRoomTemplate->GetFullName() );
	
	return buffer;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CRoomTemplateEditDialog::OnTextChanged( vgui::Panel *pPanel )
{
	// Check that this is the combo box we want.
	if ( pPanel == m_pTileTypeBox )
	{
		vgui::ComboBox *pComboBox = dynamic_cast<vgui::ComboBox*>( pPanel );
		if ( !pComboBox )
			return;

		int nItem = pComboBox->GetActiveItem();
		m_pRoomTemplate->SetTileType( nItem );
	}
}



