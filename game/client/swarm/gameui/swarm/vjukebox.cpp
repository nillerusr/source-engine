//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//
#undef fopen

#include <tier0/platform.h>
#ifdef IS_WINDOWS_PC
#include "windows.h"
#endif

#include "vjukebox.h"
#include "VGenericPanelList.h"
#include "KeyValues.h"
#include "VFooterPanel.h"
#include "EngineInterface.h"
#include "FileSystem.h"
#include "fmtstr.h"
#include "vgui/ISurface.h"
#include "vgui/IBorder.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/Divider.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/ProgressBar.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/TextImage.h"
#include "UtlBuffer.h"
#include "tier2/fileutils.h"
#include "nb_header_footer.h"
#include "nb_button.h"
#include "cdll_util.h"
#include "asw_vgui_music_importer.h"
#include "c_asw_jukebox.h"
#include <vgui_controls/ListPanel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

KeyValues *g_pPreloadedJukeboxListItemLayout = NULL;
static int s_nLastSortColumn = 0;

static int ListStringSortFunc( ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2, const char *szElement )
{
	NOTE_UNUSED( pPanel );

	bool dir1 = item1.kv->GetInt("directory") == 1;
	bool dir2 = item2.kv->GetInt("directory") == 1;

	// if they're both not directories of files, return if dir1 is a directory (before files)
	if (dir1 != dir2)
	{
		return dir1 ? -1 : 1;
	}

	const char *string1 = item1.kv->GetString( szElement );
	const char *string2 = item2.kv->GetString( szElement );

	// YWB:  Mimic windows behavior where filenames starting with numbers are sorted based on numeric part
	int num1 = Q_atoi( string1 );
	int num2 = Q_atoi( string2 );

	if ( num1 != 0 && 
		num2 != 0 )
	{
		if ( num1 < num2 )
			return -1;
		else if ( num1 > num2 )
			return 1;
	}

	// Push numbers before everything else
	if ( num1 != 0 )
	{
		return -1;
	}

	// Push numbers before everything else
	if ( num2 != 0 )
	{
		return 1;
	}

	return Q_stricmp( string1, string2 );
}

static int ListFileNameSortFunc( ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 )
{
	return ListStringSortFunc( pPanel, item1, item2, "text" );
}

static int ListFileGenreSortFunc( ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 )
{
	return ListStringSortFunc( pPanel, item1, item2, "genre" );
}

static int ListFileArtistSortFunc( ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 )
{
	return ListStringSortFunc( pPanel, item1, item2, "artist" );
}

static int ListFileAlbumSortFunc( ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 )
{
	return ListStringSortFunc( pPanel, item1, item2, "album" );
}

struct ColumnInfo_t
{
	char const	*columnName;
	char const	*columnText;
	int			startingWidth;
	int			minWidth;
	int			maxWidth;
	int			flags;
	SortFunc	*pfnSort;
	Label::Alignment alignment;
};

static ColumnInfo_t g_ColInfo[] =
{
	{	"text",				"#FileOpenDialog_Col_Name",				500,	50, 10000, ListPanel::COLUMN_UNHIDABLE,		&ListFileNameSortFunc			, Label::a_west },
	{	"genre",			"#ASW_Custom_Music_Col_Genre",			200,	50, 10000, 0,								&ListFileGenreSortFunc			, Label::a_west },
	{	"artist",			"#ASW_Custom_Music_Col_Artist",			200,	50, 10000, 0,								&ListFileArtistSortFunc			, Label::a_west },
	{	"album",			"#ASW_Custom_Music_Col_Album",			200,	50, 10000, 0,								&ListFileAlbumSortFunc			, Label::a_west },
};

//=============================================================================
JukeboxListItem::JukeboxListItem(Panel *parent, const char *panelName):
BaseClass(parent, panelName)
{
	SetProportional( true );

	m_LblName = new Label( this, "LblName", "" );
	m_LblName->DisableMouseInputForThisPanel( true );

	m_BtnEnabled = new CheckButton( this, "AddonEnabledCheckbox", "" );

	m_bCurrentlySelected = false;
}

//=============================================================================
void JukeboxListItem::SetTrackIndex( int nTrackIndex )
{
	m_nTrackIndex = nTrackIndex;
	m_LblName->SetText( g_ASWJukebox.GetTrackName( m_nTrackIndex ) );
}

//=============================================================================
void JukeboxListItem::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	if ( !g_pPreloadedJukeboxListItemLayout )
	{
		const char *pszResource = "Resource/UI/BaseModUI/jukeboxlistitem.res";
		g_pPreloadedJukeboxListItemLayout = new KeyValues( pszResource );
		g_pPreloadedJukeboxListItemLayout->LoadFromFile( g_pFullFileSystem, pszResource );
	}

	LoadControlSettings( "", NULL, g_pPreloadedJukeboxListItemLayout );

	//SetBgColor( pScheme->GetColor( "Button.BgColor", Color( 64, 64, 64, 255 ) ) );

	m_hTextFont = pScheme->GetFont( "Default", true );
}

void JukeboxListItem::OnMousePressed( vgui::MouseCode code )
{
	if ( MOUSE_LEFT == code )
	{
		GenericPanelList *pGenericList = dynamic_cast<GenericPanelList*>( GetParent() );

		unsigned short nindex;
		if ( pGenericList && pGenericList->GetPanelItemIndex( this, nindex ) )
		{
			pGenericList->SelectPanelItem( nindex, GenericPanelList::SD_DOWN );
		}
	}

	BaseClass::OnMousePressed( code );
}

void JukeboxListItem::OnMessage(const KeyValues *params, vgui::VPANEL ifromPanel)
{
	BaseClass::OnMessage( params, ifromPanel );

	if ( !V_strcmp( params->GetName(), "PanelSelected" ) ) 
	{
		m_bCurrentlySelected = true;
	}
	if ( !V_strcmp( params->GetName(), "PanelUnSelected" ) ) 
	{
		m_bCurrentlySelected = false;
	}

}

void JukeboxListItem::Paint( )
{
	BaseClass::Paint();

	// Draw the graded outline for the selected item only
	if ( m_bCurrentlySelected )
	{
		int nPanelWide, nPanelTall;
		GetSize( nPanelWide, nPanelTall );

		surface()->DrawSetColor( Color( 169, 213, 255, 128 ) );
		// Top lines
		surface()->DrawFilledRectFade( 0, 0, 0.5f * nPanelWide, 2, 0, 255, true );
		surface()->DrawFilledRectFade( 0.5f * nPanelWide, 0, nPanelWide, 2, 255, 0, true );

		// Bottom lines
		surface()->DrawFilledRectFade( 0, nPanelTall-2, 0.5f * nPanelWide, nPanelTall, 0, 255, true );
		surface()->DrawFilledRectFade( 0.5f * nPanelWide, nPanelTall-2, nPanelWide, nPanelTall, 255, 0, true );

		// Text Blotch
		int nTextWide, nTextTall, nNameX, nNameY, nNameWide, nNameTall;
		wchar_t wsAddonName[120];

		m_LblName->GetPos( nNameX, nNameY );
		m_LblName->GetSize( nNameWide, nNameTall );
		m_LblName->GetText( wsAddonName, sizeof( wsAddonName ) );
		surface()->GetTextSize( m_hTextFont, wsAddonName, nTextWide, nTextTall );
		int nBlotchWide = nTextWide + vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), 75 );

		surface()->DrawFilledRectFade( 0, 2, 0.50f * nBlotchWide, nPanelTall-2, 0, 50, true );
		surface()->DrawFilledRectFade( 0.50f * nBlotchWide, 2, nBlotchWide, nPanelTall-2, 50, 0, true );
	}
}

//=============================================================================
//
//=============================================================================
VJukebox::VJukebox( Panel *parent, const char *panelName ):
BaseClass( parent, panelName, false, true )
{
	GameUI().PreventEngineHideGameUI();

	SetDeleteSelfOnClose(true);
	SetProportional( true );

	m_pHeaderFooter = new CNB_Header_Footer( this, "HeaderFooter" );
	m_pHeaderFooter->SetTitle( "" );
	m_pHeaderFooter->SetHeaderEnabled( false );
	m_pHeaderFooter->SetGradientBarEnabled( true );
	m_pHeaderFooter->SetGradientBarPos( 75, 350 );

	m_GplTrackList = new vgui::ListPanel( this, "GplTrackList" );

	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmFrameScheme.res", "SwarmFrameScheme");
	m_GplTrackList->SetScheme(scheme);

	// list panel
	for ( int i = 0; i < ARRAYSIZE( g_ColInfo ); ++i )
	{
		const ColumnInfo_t& info = g_ColInfo[ i ];

		m_GplTrackList->AddColumnHeader( i, info.columnName, info.columnText, info.startingWidth, info.minWidth, info.maxWidth, info.flags );
		m_GplTrackList->SetSortFunc( i, info.pfnSort );
		m_GplTrackList->SetColumnTextAlignment( i, info.alignment );
	}

	m_GplTrackList->SetSortColumn( s_nLastSortColumn );
	m_GplTrackList->SetMultiselectEnabled( true );
	m_GplTrackList->SetColumnHeaderHeight( ScreenHeight()*0.04 );
	m_GplTrackList->SetVScrollBarTextures( "scroll_up", "scroll_down", "scroll_line", "scroll_box" );

	m_pAddTrackButton = new CNB_Button( this, "AddTrackButton", "", this, "AddTrackButton" );
	m_pRemoveTrackButton = new CNB_Button( this, "RemoveTrackButton", "", this, "RemoveTrackButton" );

	m_LblNoTracks = new Label( this, "LblNoTracks", "" );

	SetLowerGarnishEnabled( true );
	m_ActiveControl = m_GplTrackList;

	LoadControlSettings("Resource/UI/BaseModUI/jukebox.res");

	UpdateFooter();
}

//=============================================================================
VJukebox::~VJukebox()
{
	delete m_GplTrackList;
	GameUI().AllowEngineHideGameUI();
}

//=============================================================================
void VJukebox::Activate()
{
	BaseClass::Activate();

	// Reload the music playlist
	if( g_ASWJukebox.GetTrackCount() == 0 )
		g_ASWJukebox.Init();

	UpdateTrackList();
}

void VJukebox::UpdateTrackList()
{
	m_GplTrackList->RemoveAll();

	
	// Add the addons to the list panel 
	int nTracks = g_ASWJukebox.GetTrackCount();

	KeyValues *kv = new KeyValues("item");

	for ( int i = 0; i < nTracks; i++ )
	{		
		g_ASWJukebox.PrepareTrackKV( i, kv );
		m_GplTrackList->AddItem(kv, 0, false, false);
	}

	kv->deleteThis();

	// Focus on the first item in the list
	if( m_GplTrackList->GetItemCount() > 0 )
	{
		m_GplTrackList->NavigateTo();
		m_LblNoTracks->SetVisible( false );
	}
	else
	{
		m_LblNoTracks->SetVisible( true );
	}

	// Update the item list
	m_GplTrackList->InvalidateLayout();
}

void VJukebox::UpdateFooter()
{
	CBaseModFooterPanel *footer = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( footer )
	{
		footer->SetButtons( FB_BBUTTON, FF_AB_ONLY, false );
		footer->SetButtonText( FB_BBUTTON, "#L4D360UI_Done" );
	}
}



//=============================================================================
void VJukebox::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetupAsDialogStyle();
}

//=============================================================================
void VJukebox::PaintBackground()
{
}

//=============================================================================
void VJukebox::OnCommand(const char *command)
{
	if( FStrEq( command, "Back" ) )
	{
		// Act as though 360 back button was pressed
		OnKeyCodePressed( KEY_XBUTTON_B );
		g_ASWJukebox.SavePlaylist();
	}
	else if ( FStrEq( command, "AddTrackButton" ) )
	{
		MusicImporterDialog::OpenImportDialog( this );
	}
	else if ( FStrEq( command, "RemoveTrackButton" ) )
	{
		for( int i=0; i<m_GplTrackList->GetSelectedItemsCount(); ++i )
		{
			g_ASWJukebox.MarkTrackForDeletion( m_GplTrackList->GetSelectedItem( i ) );
		}
		g_ASWJukebox.Cleanup();
		UpdateTrackList();
	}
	else
	{
		BaseClass::OnCommand( command );
	}	
}

void VJukebox::OnMusicItemSelected( KeyValues *pInfo )
{
	if( !pInfo )
		return;

	UpdateTrackList();
}

void VJukebox::OnMessage(const KeyValues *params, vgui::VPANEL ifromPanel)
{
	BaseClass::OnMessage( params, ifromPanel );
	
	if ( Q_strcmp( params->GetName(), "OnItemSelected" ) == 0 ) 
	{
		int index = ((KeyValues*)params)->GetInt( "index" );

		UpdateButtonsForTrack( index );
	}
}

void VJukebox::UpdateButtonsForTrack( int nIndex )
{
}

void VJukebox::OnThink()
{
	BaseClass::OnThink();
}

void VJukebox::PerformLayout()
{
	BaseClass::PerformLayout();

	SetBounds( 0, 0, ScreenWidth(), ScreenHeight() );

	if( m_GplTrackList->GetItemCount() > 0 )
		m_LblNoTracks->SetVisible( false );
}

int BaseModUI::JukeboxListItem::GetTrackIndex()
{
	return m_nTrackIndex;
}