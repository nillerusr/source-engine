#include <vgui/IVGui.h>
#include <vgui/IInput.h>
#include <vgui/ISystem.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/MessageBox.h>
#include <vgui_controls/PanelListPanel.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/MenuBar.h>
#include <vgui_controls/MenuButton.h>
#include <vgui_controls/Menu.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/FileOpenDialog.h>
#include "vgui_controls/AnimationController.h"
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/ScrollBar.h>
#include <vgui_controls/PropertySheet.h>
#include "vgui/ISurface.h"
#include <vgui/ILocalize.h>
#include <KeyValues.h>

#include "TileGenDialog.h"
#include "tilegen_pages.h"
#include "ThemesDialog.h"
#include "RoomTemplateEditDialog.h"
#include "RoomTemplateListPanel.h"
#include "RoomTemplatePanel.h"
#include "PlacedRoomTemplatePanel.h"
#include "MapLayoutPanel.h"
#include "ScrollingWindow.h"
#include "VMFExporter.h"
#include "TileSource/LevelTheme.h"
#include "TileSource/RoomTemplate.h"
#include "TileSource/MapLayout.h"
#include "TileSource/Room.h"
#include "cdll_int.h"
#include "filesystem.h"
#include "kv_editor_frame.h"
#include "asw_mission_chooser.h"
#include "location_editor/location_editor_frame.h"
#include "tile_check.h"
#include "layout_system/tilegen_layout_system.h"
#include "layout_system/tilegen_mission_preprocessor.h"
#include "layout_system_editor/layout_system_kv_editor.h"
#include "asw_npcs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

ConVar tilegen_preprocess_mission( "tilegen_preprocess_mission", "0", FCVAR_CHEAT, "If set to 1, tilegen will dump the pre-processed mission file to preprocess_mission.txt." );

CTileGenDialog *g_pTileGenDialog = NULL;

bool g_bProcessGenerator = false;
bool g_bAddedTileGenLocalization = false;

extern IVEngineClient *engine;
extern char g_layoutsdir[1024];

class CModalPreserveMessageBox : public vgui::MessageBox
{
public:
	CModalPreserveMessageBox(const char *title, const char *text, vgui::Panel *parent)
		: vgui::MessageBox( title, text, parent )
	{
		m_PrevAppFocusPanel = vgui::input()->GetAppModalSurface();
	}

	~CModalPreserveMessageBox()
	{
		vgui::input()->SetAppModalSurface( m_PrevAppFocusPanel );
	}


public:
	vgui::VPANEL	m_PrevAppFocusPanel;
};

//-----------------------------------------------------------------------------
// Purpose: Utility function to pop up a VGUI message box
//-----------------------------------------------------------------------------
void VGUIMessageBox( vgui::Panel *pParent, const char *pTitle, const char *pMsg, ... )
{
	char msg[4096];
	va_list marker;
	va_start( marker, pMsg );
	Q_vsnprintf( msg, sizeof( msg ), pMsg, marker );
	va_end( marker );

	vgui::MessageBox *dlg = new CModalPreserveMessageBox( pTitle, msg, pParent );
	dlg->DoModal();
	dlg->Activate();
	dlg->RequestFocus();
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CTileGenDialog::CTileGenDialog( Panel *parent, const char *name ) : BaseClass( parent, name )
{
	Assert( !g_pTileGenDialog );
	g_pTileGenDialog = this;

	m_pMapLayout = new CMapLayout();
	m_pLayoutSystem = NULL;
	m_VMFExporter = new VMFExporter();
	m_pGenerationOptions = new KeyValues( "EmptyOptions" );

	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile( "tilegen/tilegen_scheme.res", "GAME" );
	SetScheme( scheme );

	if ( !g_bAddedTileGenLocalization )
	{
		g_pVGuiLocalize->AddFile( "tilegen/tilegen_english.txt" );
		g_bAddedTileGenLocalization = true;
	}

	m_fTileSize = 30.0f;

	m_bShowExits = true;
	m_bShowTileSquares = true;
	m_iStartDragX = -1;
	m_iStartDragY = -1;
	m_iStartRubberBandX = -1;
	
	SetSize(384, 420);
	SetMinimumSize(384, 420);
	
	m_pCurrentThemeDetails = new CThemeDetails(this, "CurrentThemeDetails", NULL);
	m_pCurrentThemeDetails->m_iDesiredWidth = 356;
	m_pTemplateListContainer = new CScrollingWindow(this, "TemplateListWindow");
	m_pTemplateListPanel = new CRoomTemplateListPanel(m_pTemplateListContainer, "TemplateListPanel");
	m_pTemplateFilter = new TextEntry( this, "TemplateFilter" );
	m_pShowExitsCheck = new vgui::CheckButton(this, "ShowExitsCheck", "");
	m_pShowExitsCheck->SetSelected(m_bShowExits);
	m_pShowTileSquaresCheck = new vgui::CheckButton(this, "ShowTileSquaresCheck", "");
	m_pShowTileSquaresCheck->SetSelected(m_bShowTileSquares);

	m_pPropertySheet = new vgui::PropertySheet( this, "PropertySheet" );
	m_pLayoutPage = new CTileGenLayoutPage( m_pPropertySheet, "TileGenLayoutPage" );
	m_pLayoutSystemPage = new CTilegenLayoutSystemPage( m_pPropertySheet, "TilegenLayoutSystemPage" );
	m_pLayoutSystemPage->UpdateList();
	m_pPropertySheet->AddPage( m_pLayoutPage, "Layout" );
	m_pPropertySheet->AddPage( m_pLayoutSystemPage, "Missions" );

	m_pCursorPanel = new CRoomTemplatePanel(this, "CursorRoomTemplatePanel");
	m_pCursorPanel->SetMouseInputEnabled(false);
	m_pCursorPanel->SetVisible(false);
	m_pCursorTemplate = NULL;

	m_pTemplateListContainer->SetChildPanel( m_pTemplateListPanel );

	CLevelTheme::LoadLevelThemes();
	OnUpdateCurrentTheme(NULL);

	// setup the menu
	m_pMenuBar = new MenuBar(this, "TileGenMenuBar");
	MenuButton *pFileMenuButton = new MenuButton(this, "FileMenuButton", "&File");
	Menu *pFileMenu = new Menu(pFileMenuButton, "TileGenFileMenu");	
	pFileMenu->AddMenuItem("&New Map Layout",  "NewMapLayout", this);
	pFileMenu->AddMenuItem("&Open Map Layout...",  "OpenMapLayout", this);
	pFileMenu->AddMenuItem("&Open Current Map Layout...",  "OpenCurrentMapLayout", this);
	pFileMenu->AddMenuItem("&Save Map Layout",  "SaveMapLayout", this);
	pFileMenu->AddMenuItem("&Save Map Layout As...",  "SaveMapLayoutAs", this);
	pFileMenu->AddSeparator();
	pFileMenu->AddMenuItem("&Export VMF", "ExportVMF", this);
	pFileMenu->AddMenuItem("&Export VMF and Play Map", "ExportAndPlay", this);
	pFileMenu->AddMenuItem("&Export VMF and Play Without Aliens", "ExportAndPlayClean", this);	
	pFileMenuButton->SetMenu(pFileMenu);
	m_pMenuBar->AddButton(pFileMenuButton);

	MenuButton *pToolsMenuButton = new MenuButton(this, "ToolsMenuButton", "&Tools");
	m_pToolsMenu = new Menu(pToolsMenuButton, "TileGenToolsMenu");	
	pToolsMenuButton->SetMenu(m_pToolsMenu);
	m_pMenuBar->AddButton(pToolsMenuButton);
	//m_pToolsMenu->AddMenuItem("Push Encounters Apart", "PushEncountersApart", this);		// used to debug encounter push apart
	m_pToolsMenu->AddMenuItem("Generate &Thumbnails", "GenerateThumbnails", this);
	//m_pToolsMenu->AddMenuItem("&Location Layout Editor", "LocationLayoutEditor", this);
	m_pToolsMenu->AddMenuItem("&Check Room Templates for Errors", "TileCheck", this);
	m_pToolsMenu->AddSeparator();
	m_nShowDefaultParametersMenuItemID = m_pToolsMenu->AddCheckableMenuItem( "&Show Optional Parameters", "ShowOptionalParameters", this );
	m_nShowAddButtonsMenuItemID = m_pToolsMenu->AddCheckableMenuItem( "&Show 'Add' Buttons", "ShowAddButtons", this );
	// Get the checked menu items in-sync with editor settings.
	m_pToolsMenu->SetMenuItemChecked( m_nShowDefaultParametersMenuItemID, true );
	m_pToolsMenu->SetMenuItemChecked( m_nShowAddButtonsMenuItemID, true );
	m_pLayoutSystemPage->GetEditor()->ShowOptionalValues( true );
	m_pLayoutSystemPage->GetEditor()->ShowAddButtons( true );

	// disable keyboard input on the menu, otherwise once you click there, this dialog never gets key input again (is there a better way to do this and maintain keyboard shortcuts for the menu?)
	pFileMenuButton->SetKeyBoardInputEnabled(false);
	pFileMenu->SetKeyBoardInputEnabled(false);
	m_pMenuBar->SetKeyBoardInputEnabled(false);

	m_pZoomInButton = new vgui::Button( this, "ZoomInButton", "+", this, "ZoomIn" );
	m_pZoomOutButton = new vgui::Button( this, "ZoomOutButton", "-", this, "ZoomOut" );
	m_pCoordinateLabel = new vgui::Label( this, "CoordinateLabel", "" );
	
	// make sure the window get a tick all the time (could change this to only tick on necessary states?)
	vgui::ivgui()->AddTickSignal( GetVPanel(), 0 );
}

//-----------------------------------------------------------------------------
// Destructor 
//-----------------------------------------------------------------------------
CTileGenDialog::~CTileGenDialog()
{
	g_pTileGenDialog = NULL;

	delete m_VMFExporter;
	delete m_pMapLayout;
	m_pGenerationOptions->deleteThis();
	delete m_pLayoutSystem;
}

//-----------------------------------------------------------------------------
// Purpose: Kills the whole app on close
//-----------------------------------------------------------------------------
void CTileGenDialog::OnClose( void )
{
	BaseClass::OnClose();

	delete this;
}

//-----------------------------------------------------------------------------
// Purpose: Parse commands coming in from the VGUI dialog
//-----------------------------------------------------------------------------
void CTileGenDialog::OnCommand( const char *command )
{
	if ( Q_stricmp( command, "Themes" ) == 0 )
	{
		// Launch the theme editing window
		CThemesDialog *pDialog = new CThemesDialog( this, "ThemesDialog", true );

		pDialog->AddActionSignalTarget( this );
		pDialog->DoModal();
	}
	else if ( Q_stricmp( command, "NewRoomTemplate" ) == 0 )
	{
		if (!CLevelTheme::s_pCurrentTheme)
		{
			MessageBox *pMessage = new MessageBox ("No theme selected", "You must select a theme before creating a new room template.", this);
			pMessage->DoModal();
			return;
		}
		CRoomTemplate *pRoomTemplate = new CRoomTemplate(CLevelTheme::s_pCurrentTheme);
		if (pRoomTemplate)
		{
			// Launch the room template editing window
			CRoomTemplateEditDialog *pDialog = new CRoomTemplateEditDialog( this, "RoomTemplateEditDialog", pRoomTemplate, true );

			pDialog->AddActionSignalTarget( this );
			pDialog->DoModal();
		}
	}
	else if ( Q_stricmp( command, "ExportVMF" ) == 0 )
	{
		if (!m_VMFExporter->ExportVMF( GetMapLayout(), "output.vmf", true ) )
		{			
			MessageBox *pMessage = new MessageBox ("VMF Export Error", m_VMFExporter->m_szLastExporterError, this);
			pMessage->DoModal();
			return;
		}
		if ( !m_VMFExporter->ShowExportErrors() )
		{
			MessageBox *pMessage = new MessageBox ("VMF Export", "Output.vmf exported okay!", this);
			pMessage->DoModal();
		}
		return;
	}
	else if ( Q_stricmp( command, "ExportAndPlay" ) == 0 || Q_stricmp( command, "ExportAndPlayClean" ) == 0 )
	{
		m_pMapLayout->SaveMapLayout( "maps/output.layout" );
		if ( engine )
		{
			char buffer[512];
			if ( Q_stricmp( command, "ExportAndPlayClean" ) == 0 )
			{
				Q_snprintf(buffer, sizeof(buffer), "asw_spawning_enabled 0;  asw_build_map %s", "output.layout" );
			}
			else
			{
				Q_snprintf(buffer, sizeof(buffer), "asw_spawning_enabled 1;  asw_build_map %s", "output.layout" );
			}
			engine->ClientCmd_Unrestricted( buffer );
		}
		else
		{
			MessageBox *pMessage = new MessageBox ("Map Exported", "To build and play map, enter into the console:\n\nasw_build_map output", this);
			pMessage->DoModal();
		}
	}
	else if ( Q_stricmp( command, "NewMapLayout" ) == 0 )
	{
		// todo: check the current map is saved, print a warning if not?
		delete m_pMapLayout;
		m_pMapLayout = new CMapLayout();
		return;
	}
	else if ( Q_stricmp( command, "OpenMapLayout" ) == 0 )
	{
		// todo: check the current map is saved, print a warning if not?
		DoFileOpen();
		return;
	}
	else if ( Q_stricmp( command, "OpenCurrentMapLayout" ) == 0 )
	{
		// todo: check the current map is saved, print a warning if not?
		
		// clear the current map layout, if any
		delete m_pMapLayout;
		m_pMapLayout = new CMapLayout();
		char mapname[ 255 ];
		Q_snprintf( mapname, sizeof( mapname ), "maps/%s.layout", engine->GetLevelNameShort() );
		// load in the new one
		if ( !m_pMapLayout->LoadMapLayout( mapname ) )
		{
			MessageBox *pMessage = new MessageBox ("Error", "Error loading map layout", this);
			pMessage->DoModal();
			return;
		}

		// make sure each placed CRoom has a UI panel
		GetMapLayoutPanel()->CreateAllUIPanels();

		return;
	}	
	else if ( Q_stricmp( command, "SaveMapLayout" ) == 0 )
	{
		if (Q_strlen(m_pMapLayout->m_szFilename) <= 0)
		{
			DoFileSaveAs();
		}
		else
		{
			m_pMapLayout->SaveMapLayout(m_pMapLayout->m_szFilename);
		}
		return;
	}
	else if ( Q_stricmp( command, "SaveMapLayoutAs" ) == 0 )
	{
		DoFileSaveAs();
		return;
	}
	else if ( Q_stricmp( command, "GenerateThumbnails" ) == 0 )
	{
		if ( p4 )
		{
			MessageBox *pMessage = new MessageBox( "Add to P4?", "Would you like to add the thumbnails to perforce?", this );
			pMessage->SetCancelButtonVisible( true );
			pMessage->SetOKButtonText( "#MessageBox_Yes" );
			pMessage->SetCancelButtonText( "#MessageBox_No" );
			pMessage->SetCommand( new KeyValues( "AddToP4", "add", "1" ) );
			pMessage->SetCancelCommand( new KeyValues( "AddToP4", "add", "0" ) );
			pMessage->DoModal();
			return;
		}

		GenerateRoomThumbnails();
		return;
	}
	else if ( !Q_stricmp( command, "PushEncountersApart" ) )
	{
		CASWMissionChooserNPCs::PushEncountersApart( m_pMapLayout );
	}
	else if ( Q_stricmp( command, "LocationLayoutEditor" ) == 0 )
	{
		vgui::Frame *pFrame = new CLocation_Editor_Frame( this, "LocationEditorFrame" );
		pFrame->MoveToCenterOfScreen();
		pFrame->Activate();
		return;
	}
	else if ( Q_stricmp( command, "ZoomIn" ) == 0 )
	{
		Zoom( 10 );
		return;
	}
	else if ( Q_stricmp( command, "ZoomOut" ) == 0 )
	{
		Zoom( -10 );
		return;
	}
	else if ( !Q_stricmp( command, "TileCheck" ) )
	{
		vgui::Frame *pFrame = new CTile_Check_Frame( this, "TileCheckFrame" );
		pFrame->MoveToCenterOfScreen();
		pFrame->Activate();
		return;
	}
	else if ( !Q_stricmp( command, "ShowOptionalParameters" ) )
	{
		m_pLayoutSystemPage->GetEditor()->ShowOptionalValues( m_pToolsMenu->IsChecked( m_nShowDefaultParametersMenuItemID ) );
		return;
	}
	else if ( !Q_stricmp( command, "ShowAddButtons" ) )
	{
		m_pLayoutSystemPage->GetEditor()->ShowAddButtons( m_pToolsMenu->IsChecked( m_nShowAddButtonsMenuItemID ) );
		return;
	}

	BaseClass::OnCommand( command );
}

void CTileGenDialog::OnUpdateCurrentTheme(KeyValues *params)
{
	m_pCurrentThemeDetails->SetTheme(CLevelTheme::s_pCurrentTheme);
	m_pCurrentThemeDetails->InvalidateLayout();

	m_pTemplateListPanel->UpdateRoomList();
	m_pTemplateListContainer->MoveToTopLeft();
	m_pTemplateListContainer->InvalidateLayout();
	m_pTemplateListContainer->Repaint();
}

void CTileGenDialog::OnAddToP4( KeyValues *pKV )
{
	GenerateRoomThumbnails( pKV->GetBool( "add" ) );
}

void CTileGenDialog::PerformLayout()
{
	BaseClass::PerformLayout();

	if ( m_bFirstPerformLayout )
	{
		int screenWide, screenTall;
		surface()->GetScreenSize(screenWide, screenTall);

		m_bFirstPerformLayout = false;
		int inset = screenWide * 0.02f;
		SetBounds( inset, inset, screenWide - inset * 3, screenTall - inset * 2 );
	}
}

void CTileGenDialog::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	
	LoadControlSettings( "tilegen/TileGenDialog.res", "GAME" );

	SetSizeable(true);
	SetMinimizeButtonVisible( false );
	SetMaximizeButtonVisible( false );
	SetCloseButtonVisible( true );
	SetMenuButtonVisible( false );

	m_bFirstPerformLayout = true;
}

void CTileGenDialog::SetCursorRoomTemplate( const CRoomTemplate *pRoomTemplate )
{
	const CRoomTemplate *pOldTemplate = m_pCursorTemplate;
	m_pCursorTemplate = pRoomTemplate;
	if (pRoomTemplate)
	{
		ClearRoomSelection();
		m_pCursorPanel->SetRoomTemplate(pRoomTemplate);
		m_pCursorPanel->SetVisible(true);
		//vgui::ivgui()->AddTickSignal( GetVPanel(), 0 );
	}
	else
	{
		m_pCursorPanel->SetVisible(false);
		//vgui::ivgui()->RemoveTickSignal( GetVPanel() );
	}
	// update highlighting of the currently selected panel in the room template browser list
	if (pOldTemplate)
		m_pTemplateListPanel->UpdatePanelsWithTemplate(pOldTemplate);
	if (pRoomTemplate)
		m_pTemplateListPanel->UpdatePanelsWithTemplate(pRoomTemplate);
}

void CTileGenDialog::OnTick()
{
	vgui::GetAnimationController()->UpdateAnimations( Plat_FloatTime() );

	if (m_pCursorTemplate)
	{
		int mx, my;
		input()->GetCursorPos(mx, my);

		if (GetScrollingWindow() && GetScrollingWindow()->m_pView && GetScrollingWindow()->m_hChildPanel.Get()
			&& GetScrollingWindow()->m_pView->IsCursorOver())
		{
			m_pCursorPanel->SetVisible(true);
			// cursor is over the grid area, snap our position to the grid
			vgui::Panel *pGrid = GetScrollingWindow()->m_hChildPanel;
			pGrid->ScreenToLocal(mx, my);
			int iTileX = mx / RoomTemplatePanelTileSize();
			int iTileY = (my / RoomTemplatePanelTileSize()) + 1;

			mx = iTileX * RoomTemplatePanelTileSize();
			my = iTileY * RoomTemplatePanelTileSize();
			pGrid->LocalToScreen(mx, my);

			ScreenToLocal(mx, my);

			my -= m_pCursorPanel->GetTall();		// relative to lower left

			m_pCursorPanel->SetPos(mx, my);
			m_pCursorPanel->InvalidateLayout();
			Repaint();
		}
		else
		{
			if (m_pCursorPanel->IsVisible())
			{
				m_pCursorPanel->SetVisible(false);
				Repaint();
			}
		}		
	}

	// check for dragging rooms
	if (m_iStartDragX != -1)
	{
		if (!input()->IsMouseDown( MOUSE_LEFT ))
		{
			// stop dragging rooms
			m_iStartDragX = -1;
			m_iStartDragY = -1;
		}
		else
		{
			// if the mouse x/y tile changes, then we need to move rooms
			int iTileX = 0;
			int iTileY = 0;
			GetMapLayoutPanel()->GetCursorTile(iTileX, iTileY);

			if (iTileX != m_iStartDragX || iTileY != m_iStartDragY)
			{
				int xdiff = iTileX - m_iStartDragX;
				int ydiff = iTileY - m_iStartDragY;

				// check the rooms fit at the new location
				// TODO check we're not dragging out of map bounds!
				bool bCollision = false;
				int iRooms = m_SelectedRooms.Count();
				for (int i=0; i<iRooms && !bCollision; i++)
				{
					CRoom *pRoom = m_SelectedRooms[i];
					if (!pRoom || !pRoom->m_pRoomTemplate )
					{
						bCollision = true;
						break;
					}

					for ( int x=0;x<pRoom->m_pRoomTemplate->GetTilesX() && !bCollision;x++ )
					{
						for ( int y=0;y<pRoom->m_pRoomTemplate->GetTilesY();y++ )
						{
							if ( m_pMapLayout->m_pRoomGrid[ pRoom->m_iPosX + xdiff + x ][ pRoom->m_iPosY + ydiff + y ] != NULL &&
								 !IsASelectedRoom( m_pMapLayout->m_pRoomGrid[ pRoom->m_iPosX + xdiff + x ][ pRoom->m_iPosY + ydiff + y ] ) )
							{
								bCollision = true;
								break;
							}
						}
					}
				}

				if ( !bCollision )
				{
					// shift all selected rooms by that amount
					for (int i=0; i<iRooms; i++)
					{
						CRoom *pRoom = m_SelectedRooms[i];
						if (!pRoom)
							continue;

						m_pMapLayout->RemoveRoom( pRoom );

						pRoom->m_iPosX += xdiff;
						pRoom->m_iPosY += ydiff;

						m_pMapLayout->PlaceRoom( pRoom );
						
						if ( pRoom->m_pPlacedRoomPanel )
						{
							CPlacedRoomTemplatePanel *pPanel = dynamic_cast<CPlacedRoomTemplatePanel*>(pRoom->m_pPlacedRoomPanel);
							if ( pPanel )
							{
								pPanel->OnDragged();
							}
						}
					}

					Repaint();

					// restart dragging from the new cursor location
					OnStartDraggingSelectedRooms();
				}
			}
		}
	}

	// Display the coordinate and room template name beneath the mouse cursor
	int nMouseX, nMouseY;
	char coordinateString[200];

	input()->GetCursorPos( nMouseX, nMouseY );
	if ( GetMapLayoutPanel()->IsWithin( nMouseX, nMouseY ) && m_pPropertySheet->GetActivePage() == static_cast< Panel * >( m_pLayoutPage ) )
	{
		int nTileX = 0, nTileY = 0;
		GetMapLayoutPanel()->GetCursorTile( nTileX, nTileY );
		CRoom *pRoom = NULL;
		if ( m_pMapLayout && nTileX >= 0 && nTileY >= 0 && nTileX < MAP_LAYOUT_TILES_WIDE && nTileY < MAP_LAYOUT_TILES_WIDE )
		{
			pRoom = m_pMapLayout->GetRoom( nTileX, nTileY );
		}

		if ( pRoom != NULL )
		{
			char roomName[MAX_PATH];
			pRoom->GetFullRoomName( roomName, MAX_PATH );
			Q_snprintf( coordinateString, 200, "(%d, %d) - %s", nTileX, nTileY, roomName );
		}
		else
		{
			Q_snprintf( coordinateString, 200, "(%d, %d)", nTileX, nTileY );
		}
	}
	else
	{
		coordinateString[0] = '\0';
	}
	m_pCoordinateLabel->SetText( coordinateString );

	// Give the new layout system time to perform 1 iteration.
	if ( m_pLayoutSystem != NULL && m_pLayoutSystem->IsGenerating() )
	{
		m_pLayoutSystem->ExecuteIteration();

		// Make sure each placed CRoom has a UI panel
		GetMapLayoutPanel()->CreateAllUIPanels();
	}
}

bool CTileGenDialog::IsASelectedRoom( CRoom *pRoom )
{
	if ( !pRoom )
		return false;

	return ( m_SelectedRooms.Find( pRoom ) != m_SelectedRooms.InvalidIndex() );
}

void CTileGenDialog::OnCheckButtonChecked(Panel *panel)
{
	if ( panel == m_pShowExitsCheck )
	{
		m_bShowExits = m_pShowExitsCheck->IsSelected();
		CRoomTemplatePanel::UpdateAllImages();
		Repaint();
	}
	else if ( panel == m_pShowTileSquaresCheck )
	{
		m_bShowTileSquares = m_pShowTileSquaresCheck->IsSelected();
		CRoomTemplatePanel::UpdateAllImages();
		Repaint();
	}
}

void CTileGenDialog::OnTextChanged( Panel *panel )
{
	if ( panel == m_pTemplateFilter )
	{
		char buf[CRoomTemplateListPanel::m_nFilterTextLength];

		m_pTemplateFilter->GetText( buf, CRoomTemplateListPanel::m_nFilterTextLength );
		m_pTemplateListPanel->SetFilterText( buf );
		m_pTemplateListPanel->InvalidateLayout();
	}
}

void CTileGenDialog::DoFileOpen()
{
	m_FileSelectType = FST_LAYOUT_OPEN;
	// Pop up the dialog
	FileOpenDialog *pFileDialog = new FileOpenDialog(NULL, "Open Map Layout", true);

	pFileDialog->SetStartDirectory( g_layoutsdir );
	pFileDialog->AddFilter("*.layout", "Map Layout (*.layout)", true);
	pFileDialog->AddActionSignalTarget(this);

	pFileDialog->DoModal(false);
}

void CTileGenDialog::DoFileSaveAs()
{
	m_FileSelectType = FST_LAYOUT_SAVE_AS;
	// Pop up the dialog
	FileOpenDialog *pFileDialog = new FileOpenDialog (NULL, "Save Map Layout", false);
	pFileDialog->SetStartDirectory( g_layoutsdir );
	pFileDialog->AddFilter("*.layout", "Map Layout (*.layout)", true);
	pFileDialog->AddActionSignalTarget(this);

	pFileDialog->DoModal(true);
}

void CTileGenDialog::OnFileSelected( const char *fullpath )
{
	if ( m_FileSelectType == FST_LAYOUT_SAVE_AS )
	{
		m_pMapLayout->SetCurrentFilename(fullpath);
		m_pMapLayout->SaveMapLayout(fullpath);
	}
	else if ( m_FileSelectType == FST_LAYOUT_OPEN )
	{
		// clear the current map layout, if any
		delete m_pMapLayout;
		m_pMapLayout = new CMapLayout();
		// load in the new one
		if ( !m_pMapLayout->LoadMapLayout(fullpath) )
		{
			MessageBox *pMessage = new MessageBox ("Error", "Error loading map layout", this);
			pMessage->DoModal();
			return;
		}

		// make sure each placed CRoom has a UI panel
		GetMapLayoutPanel()->CreateAllUIPanels();
	}
}

void CTileGenDialog::GenerateMission( const char *szMissionFile )
{
	m_pPropertySheet->SetActivePage( m_pPropertySheet->GetPage( 0 ) );
	m_pGenerationOptions->Clear();
	m_pGenerationOptions->LoadFromFile( g_pFullFileSystem, szMissionFile, "GAME" );
	
	if ( IsGenerating() )
	{
		MessageBox *pMessage = new MessageBox ("Error", "Already generating a layout.  Please wait for this to finish before starting a new one.", this);
		pMessage->DoModal();
		return;
	}
	
	delete m_pLayoutSystem;
	m_pLayoutSystem = NULL;
	// Preprocess the mission file (i.e. apply rules)
	if ( m_pLayoutSystemPage->GetPreprocessor()->SubstituteRules( m_pGenerationOptions ) )
	{
		KeyValues *pMissionSettings = m_pGenerationOptions->FindKey( "mission_settings" ); 

		if ( pMissionSettings == NULL )
		{
			MessageBox *pMessage = new MessageBox( "Error", "Mission is missing a Global Options block.", this );
			pMessage->DoModal();
			return;
		}
		// Copy the filename key over
		pMissionSettings->SetString( "Filename", m_pGenerationOptions->GetString( "Filename", "invalid_filename" ) );

		if ( tilegen_preprocess_mission.GetBool() )
		{
			m_pGenerationOptions->SaveToFile( g_pFullFileSystem, "preprocessed_mission.txt", "GAME" );
		}
		
		m_pLayoutSystem = new CLayoutSystem();
		
		AddListeners( m_pLayoutSystem );
		if ( !m_pLayoutSystem->LoadFromKeyValues( m_pGenerationOptions ) )
		{
			MessageBox *pMessage = new MessageBox( "Error", "Failed to load mission from pre-processed key-values.", this );
			pMessage->DoModal();
			return;
		}

		// Clear the current layout
		// @TODO: check the current map is saved, print a warning if not?
		delete m_pMapLayout;
		m_pMapLayout = new CMapLayout( pMissionSettings->MakeCopy() );

		m_pLayoutSystem->BeginGeneration( m_pMapLayout );
	}
	else
	{
		MessageBox *pMessage = new MessageBox ("Error", "Failed to pre-process layout system definition.", this);
		pMessage->DoModal();
		return;
	}
}

bool CTileGenDialog::IsGenerating()
{
	return m_pLayoutSystem != NULL && m_pLayoutSystem->IsGenerating();
}

MessageMapItem_t CTileGenDialog::m_MessageMap[] =
{
	MAP_MESSAGE_CONSTCHARPTR(CTileGenDialog, "FileSelected", OnFileSelected, "fullpath"), 
};

IMPLEMENT_PANELMAP(CTileGenDialog, Frame);

// adds or removes a room from the current selection
void CTileGenDialog::ToggleRoomSelection(CRoom* pRoom)
{
	if (m_SelectedRooms.Find(pRoom) != -1)
	{
		m_SelectedRooms.FindAndRemove(pRoom);
	}
	else
	{
		m_SelectedRooms.AddToTail(pRoom);
	}
	pRoom->m_pPlacedRoomPanel->InvalidateLayout(true, true);
}

// sets the room selection to just the specified room
void CTileGenDialog::SetRoomSelection(CRoom* pRoom)
{
	ClearRoomSelection();
	m_SelectedRooms.AddToTail(pRoom);
	pRoom->m_pPlacedRoomPanel->InvalidateLayout(true, true);
}

void CTileGenDialog::ClearRoomSelection()
{
	// store previously selected rooms
	CUtlVector<CRoom*> m_PreviouslySelectedRooms;
	int iRooms = m_SelectedRooms.Count();
	for (int i=0; i<iRooms; i++)
	{
		m_PreviouslySelectedRooms.AddToTail(m_SelectedRooms[i]);
	}

	// clear all rooms from the current selection
	m_SelectedRooms.RemoveAll();

	// make previously selected rooms update their panels
	for (int i=0; i<iRooms; i++)
	{
		if ( m_PreviouslySelectedRooms[i]->m_pPlacedRoomPanel )
		{
			m_PreviouslySelectedRooms[i]->m_pPlacedRoomPanel->InvalidateLayout(true, true);
		}
	}
}

void CTileGenDialog::OnKeyCodeTyped( vgui::KeyCode code )
{
	if ( code == KEY_DELETE )		// delete all selected rooms if you press DEL
	{
		if (m_SelectedRooms.Count() > 0)
		{
			// note: deleting the CRoom objects causes them to be removed from the CMapLayout and their panels deleted
			// so we're deleting them and clearing out our selected room list at the same time
			m_SelectedRooms.PurgeAndDeleteElements();
			Repaint();
		}
	}
	else if ( code == KEY_SPACE )
	{
		g_bProcessGenerator = true;
	}
	else
	{
		BaseClass::OnKeyCodeTyped( code );
	}
}

void CTileGenDialog::OnStartDraggingSelectedRooms()
{
	GetMapLayoutPanel()->GetCursorTile(m_iStartDragX, m_iStartDragY);
}

#define THUMBNAILS_FILE "resource/roomthumbnails.txt"

void CTileGenDialog::GenerateRoomThumbnails( bool bAddToPerforce )
{
	// save out a keyvalues file with position/size of each room
	KeyValues *pKV = new KeyValues( "RoomThumbnails" );
	char filename[MAX_PATH];
	for ( int i=0;i<m_pMapLayout->m_PlacedRooms.Count();i++ )
	{
		CRoom *pRoom = m_pMapLayout->m_PlacedRooms[i];
		if ( !pRoom )
			continue;
	
		KeyValues *pkvEntry = new KeyValues( "Thumbnail" );
		Q_snprintf( filename, sizeof( filename ), "tilegen/roomtemplates/%s/%s.tga", pRoom->m_pRoomTemplate->m_pLevelTheme->m_szName,
			pRoom->m_pRoomTemplate->GetFullName() );
		pkvEntry->SetString( "Filename", filename );

		int half_map_size = MAP_LAYOUT_TILES_WIDE * 0.5f;		// shift back so the middle of our grid is the origin
		float xPos = (pRoom->m_iPosX - half_map_size) * ASW_TILE_SIZE;
		float yPos = (pRoom->m_iPosY - half_map_size) * ASW_TILE_SIZE;
		pkvEntry->SetFloat( "RoomX", xPos );
		pkvEntry->SetFloat( "RoomY", yPos + pRoom->m_pRoomTemplate->GetTilesY() * ASW_TILE_SIZE );

		pkvEntry->SetFloat( "RoomWide", pRoom->m_pRoomTemplate->GetTilesX() * ASW_TILE_SIZE );
		pkvEntry->SetFloat( "RoomTall", pRoom->m_pRoomTemplate->GetTilesY() * ASW_TILE_SIZE );

		pkvEntry->SetFloat( "OutputWide", pRoom->m_pRoomTemplate->GetTilesX() * RoomTemplatePanelTileSize() );
		pkvEntry->SetFloat( "OutputTall", pRoom->m_pRoomTemplate->GetTilesY() * RoomTemplatePanelTileSize() );

		pKV->AddSubKey( pkvEntry );
	}

	if ( !pKV->SaveToFile( g_pFullFileSystem, THUMBNAILS_FILE, "GAME" ) )
	{
		Msg( "Error: Couldn't save %s\n", THUMBNAILS_FILE );
		pKV->deleteThis();
		return;
	}

	pKV->deleteThis();

	m_pMapLayout->SaveMapLayout( "maps/output.layout" );
	if ( engine )
	{
		char buffer[256];
		Q_snprintf(buffer, sizeof(buffer), "asw_random_weapons 0; asw_building_room_thumbnails 1; asw_add_room_thumbnails_to_perforce %d; asw_build_map %s", bAddToPerforce ? 1 : 0, "output.layout" );
		engine->ClientCmd_Unrestricted( buffer );
	}
}

void CTileGenDialog::Zoom( int iChange )
{
	int scroll_min = 0, scroll_max = 0, scroll_value;
	GetScrollingWindow()->m_pHorizScrollbar->GetRange( scroll_min, scroll_max );
	scroll_value = GetScrollingWindow()->m_pHorizScrollbar->GetValue();
	float flHorizFraction = ( float ) (scroll_value - scroll_min ) / (float) ( scroll_max - scroll_min );
	GetScrollingWindow()->m_pVertScrollbar->GetRange( scroll_min, scroll_max );
	scroll_value = GetScrollingWindow()->m_pVertScrollbar->GetValue();
	float flVertFraction = ( float ) (scroll_value - scroll_min ) / (float) ( scroll_max - scroll_min );

	m_fTileSize = clamp( m_fTileSize + iChange, 10, 40 );

	InvalidateLayout( true, true );

	GetScrollingWindow()->m_pHorizScrollbar->GetRange( scroll_min, scroll_max );
	scroll_value = scroll_min + ( flHorizFraction * ( scroll_max - scroll_min ) );
	GetScrollingWindow()->m_pHorizScrollbar->SetValue( scroll_value );

	GetScrollingWindow()->m_pVertScrollbar->GetRange( scroll_min, scroll_max );
	scroll_value = scroll_min + ( flVertFraction * ( scroll_max - scroll_min ) );
	GetScrollingWindow()->m_pVertScrollbar->SetValue( scroll_value );

	InvalidateLayout( true, true );

	Repaint();
}

CScrollingWindow* CTileGenDialog::GetScrollingWindow() { return m_pLayoutPage->m_pScrollingWindow; }
CMapLayoutPanel* CTileGenDialog::GetMapLayoutPanel() { return m_pLayoutPage->m_pMapLayoutPanel; }

void CTileGenDialog::StartRubberBandSelection( int mx, int my )
{
	m_iStartRubberBandX = mx;
	m_iStartRubberBandY = my;
}

void CTileGenDialog::EndRubberBandSelection( int mx, int my )
{
	ClearRoomSelection();

	// now add all rooms between the selection
	int iStartTileX = m_iStartRubberBandX / RoomTemplatePanelTileSize();
	int iStartTileY = (m_iStartRubberBandY / RoomTemplatePanelTileSize());
	iStartTileY = MapLayoutTilesWide() - iStartTileY - 1;		// reverse the Y axis

	int iEndTileX = mx / RoomTemplatePanelTileSize();
	int iEndTileY = (my / RoomTemplatePanelTileSize());
	iEndTileY = MapLayoutTilesWide() - iEndTileY - 1;		// reverse the Y axis

	if ( iStartTileX > iEndTileX )
	{
		int i = iStartTileX;
		iStartTileX = iEndTileX;
		iEndTileX = i;
	}
	if ( iStartTileY > iEndTileY )
	{
		int i = iStartTileY;
		iStartTileY = iEndTileY;
		iEndTileY = i;
	}

	int iRooms = m_pMapLayout->m_PlacedRooms.Count();
	for ( int i = 0; i < iRooms; i++ )
	{
		CRoom *pRoom = m_pMapLayout->m_PlacedRooms[i];
		if ( pRoom->m_iPosX < iEndTileX && ( pRoom->m_iPosX + pRoom->m_pRoomTemplate->GetTilesX() ) > iStartTileX &&
			pRoom->m_iPosY < iEndTileY && ( pRoom->m_iPosY + pRoom->m_pRoomTemplate->GetTilesY() ) > iStartTileY )
		{
			ToggleRoomSelection( pRoom );
		}
	}
	m_iStartRubberBandX = -1;
}

bool CTileGenDialog::GetRubberBandStart( int &sx, int &sy )
{
	if ( m_iStartRubberBandX == -1 )
		return false;

	sx = m_iStartRubberBandX;
	sy = m_iStartRubberBandY;

	return true;
}

void cc_kv_editor( const CCommand &args )
{
	CKV_Editor_Frame *pFrame = new CKV_Editor_Frame( g_pTileGenDialog->GetParent(), "KVEditor" );
	pFrame->MoveToCenterOfScreen();
	pFrame->Activate();
}
static ConCommand kv_editor( "kv_editor", cc_kv_editor, 0 );

char* TileGenCopyString( const char *szString )
{
	if ( !szString )
		return NULL;

	int len = Q_strlen( szString ) + 1;
	if ( len <= 1 )
		return NULL;

	char *text = new char[ len ];
	Q_strncpy( text, szString, len );
	return text;
}
