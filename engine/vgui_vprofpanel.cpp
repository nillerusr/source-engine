//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: VGUI scoreboard
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//

#include "client_pch.h"
#include "vgui_vprofpanel.h"
#include <KeyValues.h>
#include "vgui_budgetpanel.h"
#include "vprof_engine.h"
#include "vprof_record.h"
#include "ivideomode.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

// positions
#define VPROF_INDENT_X			XRES(20)
#define VPROF_INDENT_Y			YRES(10)

// Scoreboard dimensions
#define VPROF_TITLE_SIZE_Y			YRES(22)

#define X_BORDER					XRES(4)
#define Y_BORDER					YRES(4)

static ConVar vprof_verbose( "vprof_verbose", "1", FCVAR_ARCHIVE, "Set to one to show average and peak times" );
static ConVar vprof_unaccounted_limit( "vprof_unaccounted_limit", "0.3", FCVAR_ARCHIVE, 
									  "number of milliseconds that a node must exceed to turn red in the vprof panel" );
static ConVar vprof_warningmsec( "vprof_warningmsec", "10", FCVAR_ARCHIVE, "Above this many milliseconds render the label red to indicate slow code." );

#ifdef VPROF_ENABLED

//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
static CVProfPanel *g_pVProfPanel = NULL;
CVProfPanel *GetVProfPanel( void )
{
	return g_pVProfPanel;
}


static void ClearNodeClientData( CVProfNode *pNode )
{
	pNode->SetClientData( -1 );
	if( pNode->GetSibling() )
	{
		ClearNodeClientData( pNode->GetSibling() );
	}
	
	if( pNode->GetChild() )
	{
		ClearNodeClientData( pNode->GetChild() );
	}
}

void CVProfPanel::Reset()
{
	m_pHierarchy->RemoveAll();
	m_RootItem = -1;
	ClearNodeClientData( m_pVProfile->GetRoot() );
}


CON_COMMAND( vprof_expand_all, "Expand the whole vprof tree" )
{
	Msg("VProf expand all.\n");
	GetVProfPanel()->ExpandAll();
}

CON_COMMAND( vprof_collapse_all, "Collapse the whole vprof tree" )
{
	Msg("VProf collapse all.\n");
	GetVProfPanel()->CollapseAll();
}

CON_COMMAND( vprof_expand_group, "Expand a budget group in the vprof tree by name" )
{
	Msg("VProf expand group.\n");
	if ( args.ArgC() >= 2 )
	{
		GetVProfPanel()->ExpandGroup( args[ 1 ] );
	}
}

void IN_VProfDown(void)
{
	GetVProfPanel()->UserCmd_ShowVProf();
}

void IN_VProfUp(void)
{
	GetVProfPanel()->UserCmd_HideVProf();
}

static ConCommand startshowvprof("+showvprof", IN_VProfDown);
static ConCommand endshowvprof("-showvprof", IN_VProfUp);

void ChangeVProfScopeCallback( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	ConVarRef var( pConVar );
	Msg( "VProf setting scope to %s\n", var.GetString( ) );

	if ( g_pVProfPanel )
	{
		g_pVProfPanel->Reset();
	}
}

ConVar vprof_scope( 
	"vprof_scope", 
	"", 
	0, 
	"Set a specific scope to start showing vprof tree",
	0, 0, 0, 0,
	ChangeVProfScopeCallback );

#define PROF_FONT "DefaultFixed"

class CProfileTree : public TreeView
{
	DECLARE_CLASS_SIMPLE( CProfileTree, TreeView );
public:
	CProfileTree( CVProfPanel *parent, const char *panelName );
	~CProfileTree();

	virtual void OnCommand( const char *cmd );

	virtual void InvalidateLayout( bool layoutNow = false, bool reloadScheme = false );

	virtual void ApplySchemeSettings( IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );
		SetFont( pScheme->GetFont( PROF_FONT ) );
	}

	virtual void SetBgColor( Color color )
	{
		BaseClass::SetBgColor( color );
	}

private:

	Menu		*m_pEditMenu;
	CVProfPanel	*m_pParent;
};

CProfileTree::CProfileTree( CVProfPanel *parent, const char *panelName ) :
	BaseClass( (Panel *)parent, panelName ),
	m_pEditMenu( 0 ),
	m_pParent( parent )
{
}

CProfileTree::~CProfileTree()
{
	delete m_pEditMenu;
}

void CProfileTree::InvalidateLayout( bool layoutNow, bool reloadScheme )
{
	BaseClass::InvalidateLayout( layoutNow, reloadScheme );
	if ( GetParent() )
	{
		GetParent()->InvalidateLayout( layoutNow, reloadScheme );
	}
}

void CProfileTree::OnCommand( const char *cmd )
{
	// Relay to parent
	GetParent()->OnCommand( cmd );
}

CProfileHierarchyPanel::ColumnPanels_t::ColumnPanels_t() :
	treeViewItem( -1 )
{
}

CProfileHierarchyPanel::ColumnPanels_t::ColumnPanels_t( const ColumnPanels_t& src )
{
	treeViewItem = src.treeViewItem;
	int c = src.m_Columns.Count();
	for ( int i = 0; i < c; ++i )
	{
		PanelEntry_t pe;
		pe.dataname = src.m_Columns[ i ].dataname;
		pe.label = src.m_Columns[ i ].label;

		m_Columns.AddToTail( pe );
	}
}

void CProfileHierarchyPanel::ColumnPanels_t::AddColumn( int index, const char *name, vgui::Label *label )
{
	m_Columns.EnsureCount( index + 1 );

	m_Columns[ index ].label = label;
	m_Columns[ index ].dataname = name;
}

void CProfileHierarchyPanel::ColumnPanels_t::Refresh( KeyValues *kv )
{
	VPROF( "CProfileHierarchyPanel::ColumnPanels_t" );

	int c = m_Columns.Count();
	for ( int i = 0; i < c; ++i )
	{
		vgui::Label *label = m_Columns[ i ].label;
		if ( !label )
			continue;

		const char *name = m_Columns[ i ].dataname.String();
		if ( name && name[ 0 ] )
		{
			const char *value = kv->GetString( name, "" );
			if ( value )
			{
				if ( !value[ 0 ] )
				{
					label->SetText( "" );
					label->SetVisible( false );
				}
				else
				{
					label->SetVisible( true );
					label->SetText( value );
				}
			}
			else
			{
				label->SetVisible( false );
			}
		}
	}
}


CProfileHierarchyPanel::CProfileHierarchyPanel(vgui::Panel *parent, const char *panelName)
 : BaseClass(parent,panelName),
	m_Panels( 0, 0, PanelsLessFunc )
{
}

CProfileHierarchyPanel::~CProfileHierarchyPanel()
{
}

void CProfileHierarchyPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	//SetProportional( true );
	BaseClass::ApplySchemeSettings( pScheme );
	m_itemFont = pScheme->GetFont( PROF_FONT );
	SetTitleBarInfo( m_itemFont, 20 );
	SetBgColor( Color(0, 0, 0, 176) );
	(( CProfileTree *)GetTree())->SetBgColor( Color( 0, 0, 0, 176 ) );
}

void CProfileHierarchyPanel::SetItemColors( int id, const Color& fg, const Color& bg )
{
	VPROF( "CProfileHierarchyPanel::SetItemColors" );
	GetTree()->SetItemFgColor( id, fg );
	GetTree()->SetItemBgColor( id, bg );
	ColumnPanels_t search;
	search.treeViewItem = id;
	int idx = m_Panels.Find( search );
	if ( idx == m_Panels.InvalidIndex() )
	{
		return;
	}
	ColumnPanels_t& info = m_Panels[ idx ];
	int c = info.m_Columns.Count();
	for ( int i = 0; i < c; ++i )
	{
		Label *label = info.m_Columns[ i ].label;
		if ( !label )
			continue;
		label->SetFgColor( fg );
		label->SetBgColor( bg );
	}
}

void CProfileHierarchyPanel::SetItemColumnColors( int id, int col, const Color& fg, const Color& bg )
{
	VPROF( "CProfileHierarchyPanel::SetItemColumnColors" );
	ColumnPanels_t search;
	search.treeViewItem = id;
	int idx = m_Panels.Find( search );
	if ( idx == m_Panels.InvalidIndex() )
	{
		return;
	}
	ColumnPanels_t& info = m_Panels[ idx ];
	int c = info.m_Columns.Count();
	if ( col < 0 || col >= c )
		return;

	Label *label = info.m_Columns[ col ].label;
	if ( !label )
		return;

	label->SetFgColor( fg );
	label->SetBgColor( bg );
}

void CProfileHierarchyPanel::ModifyItem( KeyValues *data, int itemIndex )
{
	GetTree()->SetItemFgColor( itemIndex, GetFgColor() );
	GetTree()->SetItemBgColor( itemIndex, GetBgColor() );

	ColumnPanels_t search;
	search.treeViewItem = itemIndex;
	int idx = m_Panels.Find( search );
	if ( idx == m_Panels.InvalidIndex() )
	{
		Assert( 0 );
		return;
	}

	ColumnPanels_t& info = m_Panels[ idx ];
	info.Refresh( data );
}

int CProfileHierarchyPanel::AddItem( KeyValues *data, int parentItemIndex, ColumnPanels_t& columnPanels )
{
	int itemIndex = GetTree()->AddItem( data, parentItemIndex );
	columnPanels.treeViewItem = itemIndex;
	
	ColumnPanels_t search;
	search.treeViewItem = itemIndex;

	int idx = m_Panels.Find( search );
	if ( idx == m_Panels.InvalidIndex() )
	{
		m_Panels.Insert( columnPanels );

		int c = columnPanels.m_Columns.Count();
		for ( int i = 0; i < c; ++i )
		{
			if ( columnPanels.m_Columns[ i ].label )
			{
				columnPanels.m_Columns[ i ].label->SetParent( this );
			}
		}
	}

	ModifyItem( data, itemIndex );

	return itemIndex;
}

void CProfileHierarchyPanel::PostChildPaint()
{
}

void CProfileHierarchyPanel::PerformLayout()
{
	VPROF( "CProfileHierarchyPanel::PerformLayout" );
	BaseClass::PerformLayout();

	// Assume all invisible at first
	HideAll();

	int tall = GetTall();

	int rowheight = GetTree()->GetRowHeight();
	int top, visitems;
	bool hbarVisible = false;
	GetTree()->GetVBarInfo( top, visitems, hbarVisible );

	int offset = -top * rowheight;

	int numColumns = GetNumColumns();
	// Now position column panels into the correct spot
	int rows = GetNumRows();
	for ( int row = 0; row < rows; ++row )
	{
		int tvi = GetTreeItemAtRow( row );

		for ( int col = 0; col < numColumns; ++col )
		{
			int left, right, bottom;
			GetGridElementBounds( col, row, left, top, right, bottom );

			ColumnPanels_t search;
			search.treeViewItem = tvi;

			int idx = m_Panels.Find( search );
			if ( idx != m_Panels.InvalidIndex() )
			{
				ColumnPanels_t& info = m_Panels[ idx ];

				if ( col >= info.m_Columns.Count() )
					continue;

				vgui::Label *p = info.m_Columns[ col ].label;
				if ( !p )
				{
					continue;
				}
				
				bool vis = ( top + offset - 20 ) >= 0 && ( bottom + offset ) < tall;

				p->SetParent( vis ? this : NULL );
				p->SetVisible( vis );
				p->SetBounds( left, top + offset, right - left, bottom - top );
				p->InvalidateLayout();
			}
			else
			{
				Assert( 0 );
			}
		}
	}
}

void CProfileHierarchyPanel::HideAll()
{
	for ( int i = m_Panels.FirstInorder(); i != m_Panels.InvalidIndex(); i = m_Panels.NextInorder( i ) )
	{
		ColumnPanels_t& info = m_Panels[ i ];
		int c = info.m_Columns.Count();
		for ( int j = 0 ; j < c; ++j )
		{
			Label *panel = info.m_Columns[ j ].label;
			if ( !panel )
			{
				continue;
			}
			panel->SetVisible( false );
		}
	}
}

void CProfileHierarchyPanel::RemoveAll()
{
	GetTree()->RemoveAll();

	for ( int i = m_Panels.FirstInorder(); i != m_Panels.InvalidIndex(); i = m_Panels.NextInorder( i ) )
	{
		ColumnPanels_t& info = m_Panels[ i ];
		int c = info.m_Columns.Count();
		for ( int j = 0 ; j < c; ++j )
		{
			delete info.m_Columns[ j ].label;
		}
		info.m_Columns.RemoveAll();
	}
	m_Panels.RemoveAll();
	InvalidateLayout();
}

void CProfileHierarchyPanel::ExpandItem(int itemIndex, bool bExpand)
{
	GetTree()->ExpandItem( itemIndex, bExpand );
}

bool CProfileHierarchyPanel::IsItemExpanded( int itemIndex )
{
	return GetTree()->IsItemExpanded( itemIndex );
}

KeyValues *CProfileHierarchyPanel::GetItemData(int itemIndex)
{
	return GetTree()->GetItemData( itemIndex );
}

//-----------------------------------------------------------------------------
// Purpose: Create the VProf panel
//-----------------------------------------------------------------------------
CVProfPanel::CVProfPanel( vgui::Panel *pParent, const char *pElementName ) 
 :	vgui::Frame( pParent, pElementName )
{
	m_pVProfile = g_pVProfileForDisplay;

	Assert( g_pVProfPanel == NULL );
	g_pVProfPanel = this;

	m_RootItem = -1;
	m_fShowVprofHeld = false;

	int x = VPROF_INDENT_X;
	int y = VPROF_INDENT_Y;
	int wide = videomode->GetModeStereoWidth() - x * 2;
	int tall = videomode->GetModeStereoHeight() - y * 2;
	SetBgColor(Color(0, 0, 0, 175));

	// Initialize the top title.
#ifdef VPROF_ENABLED
	SetTitle("VProf", false);
#else
	SetTitle("** VProf is not enabled **", false);
#endif

	SetZPos( 1002 );

	CProfileTree *profileTree = new CProfileTree( this, "ProfileTree" );

	m_pHierarchy = new CProfileHierarchyPanel( this, "Hierarchy" );

	m_pHierarchy->SetTreeView( profileTree );
	m_pHierarchy->SetNumColumns( 3 );

	int treewide = wide - 780;
	m_pHierarchy->SetColumnInfo( 0, "Tree", treewide );

	m_pHierarchy->SetColumnInfo( 1, "Group", 120 );
	m_pHierarchy->SetColumnInfo( 2, "Data", 180 );

	// Treeview of the hierarchical calls
	m_pHierarchy->SetBounds(X_BORDER, VPROF_TITLE_SIZE_Y, wide - X_BORDER*2, tall - Y_BORDER*2 - VPROF_TITLE_SIZE_Y);
	m_pHierarchy->SetParent(this);
	m_pHierarchy->SetPaintBorderEnabled( false );
	m_pHierarchy->SetPaintBackgroundEnabled( false );

	SETUP_PANEL( m_pHierarchy );
	SETUP_PANEL( profileTree );

	// Mode selection used to populate the tree view + lists
	m_pVProfCategory = new ComboBox(this, "CategoryCombo", 5, false);
	m_pVProfCategory->AddItem( "All Categories", NULL );
	m_pVProfCategory->AddActionSignalTarget( this );
	m_pVProfCategory->ActivateItem( 0 );

	m_pVProfSort = new ComboBox( this, "SortCombo", 5, false );
	m_pVProfSort->AddItem( "By Time", NULL );
	m_pVProfSort->AddItem( "By Name", NULL );
	m_pVProfSort->AddItem( "By Budget Group", NULL );
	m_pVProfSort->AddActionSignalTarget( this );
	m_pVProfSort->ActivateItem( 0 );

	m_nLastBudgetGroupCount = 0;
	m_nCurrentBudgetGroup = -1;

	m_pHierarchicalView = new vgui::CheckButton( this, "HierarchicalViewSelection", "" );	
	m_pHierarchicalView->AddActionSignalTarget( this );
	m_pHierarchicalView->SetSelected( true );
	m_bHierarchicalView = true;

	m_pRedoSort = new vgui::Button( this, "RedoSorting", "", this, "redosort" );

	m_pVerbose = new vgui::CheckButton( this, "VerboseCheckbox", "" );	
	m_pVerbose->AddActionSignalTarget( this );
	m_pVerbose->SetSelected( vprof_verbose.GetBool() );


	// Setup the playback controls.
	m_pPlaybackLabel = new vgui::Label( this, "PlaybackLabel", "" );
	m_pPlaybackLabel->SetBgColor( Color( 0, 0, 0, 200 ) );
	m_pPlaybackLabel->SetPaintBackgroundEnabled( true );
	
	m_pStepForward = new vgui::Button( this, "StepForward", "", this, "StepForward" );
	m_pStepBack = new vgui::Button( this, "StepBack", "", this, "StepBack" );
	m_pGotoButton = new vgui::Button( this, "GotoButton", "", this, "GotoButton" );
	
	m_pPlaybackScroll = new vgui::ScrollBar( this, "PlaybackScroll", false );
	m_pPlaybackScroll->SetRange( 0, 1000 );
	m_pPlaybackScroll->SetRangeWindow( 30 );
	m_pPlaybackScroll->AddActionSignalTarget( this );
	
	m_iLastPlaybackTick = -1;


	LoadControlSettings("Resource\\VProfPanel.res");

	SetBounds( x, y, wide, tall );

	vgui::ivgui()->AddTickSignal( GetVPanel() );
}


CVProfPanel::~CVProfPanel( void )
{
	Assert( g_pVProfPanel == this );
	g_pVProfPanel = NULL;
}

#define DATA_FMT_STR "%-30.30s %-30.30s %-45.45s %-10.10s"
void CVProfPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	int w, h;
	GetSize( w, h );

	int topoffset = 95;
	int inset = 10;

	m_pHierarchy->SetBounds( inset, topoffset, w - 2 * inset, h - inset - topoffset );

	int treewide = w - 900 - 20;
	treewide = max( treewide, 240 );
	m_pHierarchy->SetColumnInfo( 0, "Tree", treewide );

	m_pHierarchy->SetColumnInfo( 1, "Group", 125 );
	char header[ 512 ];
	Q_snprintf( header, sizeof( header ), DATA_FMT_STR,
		"Frame Calls + Time + NoChild",
		"Avg Calls   + Time + NoChild",
		"Sum Calls   + Time + NoChild + Peak",
		"L2Miss" );

	m_pHierarchy->SetColumnInfo( 2, header, 775, CTreeViewListControl::CI_HEADER_LEFTALIGN );
}

//-----------------------------------------------------------------------------
// Scheme settings!
//-----------------------------------------------------------------------------
void CVProfPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{			 
	BaseClass::ApplySchemeSettings( pScheme );

	IBorder *border = pScheme->GetBorder( "ToolTipBorder" );
	SetBorder(border);

	SetBgColor(Color(0, 0, 0, 175));
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *command - 
//-----------------------------------------------------------------------------
void CVProfPanel::Close()
{
	UserCmd_HideVProf();
	BaseClass::Close();
}
			  

//-----------------------------------------------------------------------------
// Is it visible?
//-----------------------------------------------------------------------------
void CVProfPanel::OnTick()
{
	BaseClass::OnTick();
	
	// Did the CVProfile we're using switch behind our back?
	if ( g_pVProfileForDisplay != m_pVProfile )
	{
		Reset();
		m_pVProfile = g_pVProfileForDisplay;

		bool bVisible = false;
#ifndef _XBOX
		if ( VProfRecord_IsPlayingBack() )
		{
			bVisible = true;
			m_iLastPlaybackTick = -1;
		}
#endif
		m_pStepForward->SetVisible( bVisible );
		m_pStepBack->SetVisible( bVisible );
		m_pPlaybackLabel->SetVisible( bVisible );
		m_pPlaybackScroll->SetVisible( bVisible );
		m_pGotoButton->SetVisible( bVisible );
	}
#ifndef _XBOX
	if ( VProfRecord_IsPlayingBack() )
	{
		// Update the playback tick.
		int iCur = VProfPlayback_GetCurrentTick();
		if ( iCur != m_iLastPlaybackTick )
		{
			char str[512];
			Q_snprintf( str, sizeof( str ), "VPROF playback (tick %d, %d%%)", iCur, (int)(VProfPlayback_GetCurrentPercent() * 100) );
			m_pPlaybackLabel->SetText( str );
		}
	}
#endif
	SetVisible( m_fShowVprofHeld != 0 );

	m_pRedoSort->SetVisible( !m_bHierarchicalView );
}

// 0  = By Time
// 1 = By "Text"
// 2 = By "group" then by "text"
static int g_nSortType = -1;
//-----------------------------------------------------------------------------
// Visualization changed 
//-----------------------------------------------------------------------------
void CVProfPanel::OnTextChanged( KeyValues *data )
{
	Panel *pPanel = reinterpret_cast<vgui::Panel *>( data->GetPtr("panel") );
	vgui::ComboBox *pBox = dynamic_cast<vgui::ComboBox *>( pPanel );

	if( pBox == m_pVProfCategory ) 
	{
		// The -1 here is for the 'All Categories' item
		m_nCurrentBudgetGroup = pBox->GetActiveItem() - 1;

		// NOTE: We have to reset the tree view so that it 
		// is populated with only the ones we want (need to eliminate everything
		// that's not appropriate)
		Reset();
		return;
	}
	if ( pBox == m_pVProfSort )
	{
		g_nSortType = pBox->GetActiveItem();
		Reset();
		return;
	}
}



//-----------------------------------------------------------------------------
// Sort function for hierarchical data
//-----------------------------------------------------------------------------
bool ChildCostSortFunc(KeyValues *pNode1, KeyValues *pNode2)
{
	switch ( g_nSortType )
	{
	default:
	case 0:
	case -1:
		{
			float flTime1 = pNode1->GetFloat( "Time", -1.0f );
			float flTime2 = pNode2->GetFloat( "Time", -1.0f );
			return (flTime1 > flTime2);
		}
	case 1:
		{
			const char *t1 = pNode1->GetString( "Text", "" );
			const char *t2 = pNode2->GetString( "Text", "" );
			if( Q_stricmp( t1, t2 ) <= 0 )
				return true;
			return false;
		}
		break;
	case 2:
		{
			const char *g1 = pNode1->GetString( "group", "" );
			const char *g2 = pNode2->GetString( "group", "" );
			int val = Q_stricmp( g1, g2 );
			if( val < 0 )
				return true;
			
			if ( val > 0 )
				return false;

			const char *t1 = pNode1->GetString( "Text", "" );
			const char *t2 = pNode2->GetString( "Text", "" );
			if( Q_stricmp( t1, t2 ) <= 0 )
				return true;
			return false;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Changes the visualization method for vprof data 
//-----------------------------------------------------------------------------
void CVProfPanel::OnCheckButtonChecked(Panel *panel)
{
	if ( panel == m_pHierarchicalView )
	{
		bool bSelected = m_pHierarchicalView->IsSelected() ? 1 : 0;
		if ( bSelected != m_bHierarchicalView )
		{
			m_bHierarchicalView = bSelected;
			m_pHierarchy->GetTree()->SetSortFunc( m_bHierarchicalView ? NULL : ChildCostSortFunc );
			m_pRedoSort->SetVisible( !m_bHierarchicalView );
			Reset();
		}
		return;
	}

	if ( panel == m_pVerbose )
	{
		vprof_verbose.SetValue( m_pVerbose->IsSelected() ? 1 : 0 );
		return;
	}
}


//-----------------------------------------------------------------------------
// Methods related to expand/collapse in hierarchy
//-----------------------------------------------------------------------------
void CVProfPanel::ExpandAll( void )
{
	int count = m_pHierarchy->GetTree()->GetHighestItemID();
	int i;
	for( i = 0; i < count; i++ )
	{
		if( m_pHierarchy->GetTree()->IsItemIDValid( i ) )
		{
			m_pHierarchy->GetTree()->ExpandItem( i, true );
		}
	}
}

void CVProfPanel::CollapseAll( void )
{
	int count = m_pHierarchy->GetTree()->GetHighestItemID();
	int i;
	for( i = 1; i < count; i++ )
	{
		if( m_pHierarchy->GetTree()->IsItemIDValid( i ) )
		{
			m_pHierarchy->GetTree()->ExpandItem( i, false );
		}
	}
}

void CVProfPanel::ExpandGroupRecursive( int nBudgetGroupID, CVProfNode *pNode )
{
	if( !pNode )
	{
		return;
	}
	if( pNode->GetBudgetGroupID() == nBudgetGroupID )
	{
		CVProfNode *pTempNode = pNode;
		while( pTempNode )
		{
			if( pTempNode->GetParent() )
			{
				int id = pTempNode->GetParent()->GetClientData();
				m_pHierarchy->ExpandItem( id, true );
			}
			pTempNode = pTempNode->GetParent();
		}
	}

	ExpandGroupRecursive( nBudgetGroupID, pNode->GetChild() );
	ExpandGroupRecursive( nBudgetGroupID, pNode->GetSibling() );
}

void CVProfPanel::ExpandGroup( const char *pGroupName )
{
	int groupID = m_pVProfile->BudgetGroupNameToBudgetGroupID( pGroupName );
	ExpandGroupRecursive( groupID, m_pVProfile->GetRoot() );
}

class CVProfLabel : public Label
{
	DECLARE_CLASS_SIMPLE( CVProfLabel, Label );
public:

	CVProfLabel( Panel *parent, const char *panelName ) :
		BaseClass( parent, panelName, "" )
	{
		//SetPaintBackgroundEnabled( false );
	}

	virtual void ApplySchemeSettings( IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );
		SetFont( pScheme->GetFont( PROF_FONT ) );
		SetBgColor( Color( 0, 0, 0, 255 ) );
	}
};

Label *AllocateVprofLabel( const char *panelName )
{
	CVProfLabel *l = new CVProfLabel( NULL, panelName );
	l->SetContentAlignment( Label::a_west );
	return l;
}

void CVProfPanel::AddColumns( CProfileHierarchyPanel::ColumnPanels_t& cp )
{
	cp.AddColumn( 1, "group", AllocateVprofLabel( "group" ) );
	cp.AddColumn( 2, "data", AllocateVprofLabel( "data" ) );
}


//-----------------------------------------------------------------------------
// Populate the tree
//-----------------------------------------------------------------------------
int CVProfPanel::UpdateVProfTreeEntry( KeyValues *pKeyValues, CVProfNode *pNode, int parent )
{
	VPROF( "UpdateVProfTreeEntry" );

	CFmtStrN<1024> msg;
	double curTimeLessChildren = pNode->GetCurTimeLessChildren();
	double avgLessChildren = ( pNode->GetTotalCalls() > 0 ) ? pNode->GetTotalTimeLessChildren() / (double)pNode->GetTotalCalls() : 0;

	pKeyValues->SetString( "Text", pNode->GetName() );
	pKeyValues->SetString( "group", m_pVProfile->GetBudgetGroupName( pNode->GetBudgetGroupID() ) );

	CFmtStrN< 256 > frame;
	CFmtStrN< 256 > avgstr;
	CFmtStrN< 256 > sum;
	CFmtStrN< 256 > l2miss;

	frame.sprintf( "[%4d] %7.2f %7.2f", pNode->GetCurCalls(), pNode->GetCurTime(), curTimeLessChildren );
	
	
	pKeyValues->SetString( "frame", msg );

	if( vprof_verbose.GetBool() )
	{											 
		double avgCalls = ( m_pVProfile->NumFramesSampled() > 0 ) ? (double)pNode->GetTotalCalls() / (double)m_pVProfile->NumFramesSampled() : 0;
		double avg = ( m_pVProfile->NumFramesSampled() > 0 ) ? (double)pNode->GetTotalTime() / (double)m_pVProfile->NumFramesSampled() : 0;
		double avgLessChildrenVerbose = ( m_pVProfile->NumFramesSampled() > 0 ) ? (double)pNode->GetTotalTimeLessChildren() / (double)m_pVProfile->NumFramesSampled() : 0;

		avgstr.sprintf( "[%6.2f] %6.3f %6.3f", avgCalls, avg, avgLessChildrenVerbose );
		sum.sprintf( "[%7d] %9.2f %9.2f %8.2fp", pNode->GetTotalCalls(), pNode->GetTotalTime(), pNode->GetTotalTimeLessChildren(), pNode->GetPeakTime() );
	}

	if ( m_pVProfile->UsePME() )
	{
		l2miss.sprintf( "%5d", pNode->GetL2CacheMisses() );
	}

	msg.sprintf( DATA_FMT_STR,
		(const char *)frame,
		(const char *)avgstr,
		(const char *)sum,
		(const char *)l2miss );
	pKeyValues->SetString( "data", msg );

	pKeyValues->SetFloat( "Time", avgLessChildren );

	// Add or modify a line in the hierarchy
	int id = pNode->GetClientData();
	if ( id == -1 )
	{
		CProfileHierarchyPanel::ColumnPanels_t cp;
		AddColumns(  cp );
		id = m_pHierarchy->AddItem( pKeyValues, parent, cp ) ;
		pNode->SetClientData( id );
	}
	else
	{
		VPROF( "UpdateVProfTreeEntry:Modify" );
		m_pHierarchy->ModifyItem( pKeyValues, id );
	}

	// Apply color to the item
	int r,g,b,a;
	m_pVProfile->GetBudgetGroupColor( pNode->GetBudgetGroupID(), r, g, b, a );
	m_pHierarchy->SetItemColors( id, Color( r, g, b, a ), Color( 0, 0, 0, 255 ) );

	if( pNode->GetBudgetGroupID() == VPROF_BUDGET_GROUP_ID_UNACCOUNTED )
	{
		if ( curTimeLessChildren > vprof_unaccounted_limit.GetFloat() )
		{
			m_pHierarchy->SetItemColors( id, Color( 255, 0, 0, 255 ), Color( 0, 0, 0, 255 ) );
		}
	}

	if ( pNode->GetCurTime() > vprof_warningmsec.GetFloat() ||
		 curTimeLessChildren > vprof_warningmsec.GetFloat() )
	{
		m_pHierarchy->SetItemColumnColors( id, 2, Color( 255, 0, 0, 255 ), Color( 63, 0, 0, 255 ) );
	}
	return id;
}

//-----------------------------------------------------------------------------
// Populate the tree
//-----------------------------------------------------------------------------
void CVProfPanel::FillTree( KeyValues *pKeyValues, CVProfNode *pNode, int parent )
{
#ifdef VPROF_ENABLED

	bool fIsRoot = ( pNode == m_pVProfile->GetRoot() );
	if ( fIsRoot )
	{
		if( pNode->GetChild() )
		{
			FillTree( pKeyValues, pNode->GetChild(), parent );
		}
		return;
	}

	int id = parent;
	if (( m_nCurrentBudgetGroup < 0 ) || ( pNode->GetBudgetGroupID() == m_nCurrentBudgetGroup )) 
	{
		id = UpdateVProfTreeEntry( pKeyValues, pNode, parent );
	}

	if( pNode->GetSibling() )
	{
		FillTree( pKeyValues, pNode->GetSibling(), parent );
	}
	
	if( pNode->GetChild() )
	{
		FillTree( pKeyValues, pNode->GetChild(), m_bHierarchicalView ? id : parent );
	}
#endif
}


//-----------------------------------------------------------------------------
// Populates the budget group combo box
//-----------------------------------------------------------------------------
void CVProfPanel::PopulateBudgetGroupComboBox()
{
	int nBudgetGroupCount = m_pVProfile->GetNumBudgetGroups();
	while( m_nLastBudgetGroupCount < nBudgetGroupCount )
	{
		m_pVProfCategory->AddItem( m_pVProfile->GetBudgetGroupName(m_nLastBudgetGroupCount), NULL );
		++m_nLastBudgetGroupCount;
	}
}


//-----------------------------------------------------------------------------
// Populates the tree
//-----------------------------------------------------------------------------
void CVProfPanel::UpdateProfile( float filteredtime )
{
#ifdef VPROF_ENABLED
	//ExecuteDeferredOp();
	if (IsVisible())
	{
		PopulateBudgetGroupComboBox();

		SetTitle( CFmtStr( "VProf (%s) --  %d frames sampled", 
									   m_pVProfile->IsEnabled() ?  "running" : "not running",
										   m_pVProfile->NumFramesSampled() ), false );

		// It's important to cache bEnabled since calling pause can disable.
		bool bEnabled = m_pVProfile->IsEnabled();
		if( bEnabled )
		{
			m_pVProfile->Pause();
		}

		KeyValues * pVal = new KeyValues("");
		
		if ( !m_pHierarchy->GetTree()->GetItemCount() )
		{
			pVal->SetString( "Text", "Call tree" );
			CProfileHierarchyPanel::ColumnPanels_t cp;
			AddColumns( cp );
			m_RootItem = m_pHierarchy->AddItem( pVal, -1, cp );
			m_pHierarchy->SetItemColors( m_RootItem, Color( 255, 255, 255, 255 ), Color( 0, 0, 0, 255 ) );
			m_pHierarchy->ExpandItem( m_RootItem, true );
		}

		m_pHierarchy->ExpandItem( m_RootItem, true );

		const char *pScope = vprof_scope.GetString();
		CVProfNode *pStartNode = ( pScope[0] == 0 ) ? m_pVProfile->GetRoot()  : m_pVProfile->FindNode(m_pVProfile->GetRoot(), pScope );
		
		if ( pStartNode )
		{
			FillTree( pVal, pStartNode, m_RootItem );
		}
		
		pVal->deleteThis();

		if( bEnabled )
		{
			m_pVProfile->Resume();
		}
	}

	if ( m_pVProfile->IsEnabled() )
	{
		Assert( m_pVProfile->AtRoot() );
		
		if ( GetBudgetPanel()->IsBudgetPanelShown() )
			GetBudgetPanel()->SnapshotVProfHistory( filteredtime );

		WriteRemoteVProfData(); // send out the vprof data to remote endpoints
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVProfPanel::UserCmd_ShowVProf( void )
{
	m_fShowVprofHeld = true;

	SetVisible( true );
	// This is hacky . . need to at least remember the previous value to set it back.
	ConVarRef cl_mouseenable( "cl_mouseenable" );
	if ( cl_mouseenable.IsValid() )
	{
		cl_mouseenable.SetValue( 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVProfPanel::UserCmd_HideVProf( void )
{
	m_fShowVprofHeld = false;

	SetVisible( false );

	// This is hacky . . need to at least remember the previous value to set it back.
	ConVarRef cl_mouseenable( "cl_mouseenable" );
	if ( cl_mouseenable.IsValid() )
	{
		cl_mouseenable.SetValue( 1 );
	}
}


//-----------------------------------------------------------------------------

void CVProfPanel::Paint()
{
	m_pVProfile->Pause();
	BaseClass::Paint();
	m_pVProfile->Resume();
}


void CVProfPanel::OnCommand( const char *pCommand )
{
#ifndef _XBOX
	if ( !Q_stricmp( pCommand, "StepForward" ) )
	{
		VProfPlayback_Step();
	}
	else if ( !Q_stricmp( pCommand, "StepBack" ) )
	{
		int shouldReset = VProfPlayback_StepBack();
		if ( shouldReset == 2 )
		{
			Reset();
		}
	}
	else if ( !Q_stricmp( pCommand, "GotoButton" ) )
	{
		int shouldReset = VProfPlayback_SeekToPercent( (float)m_pPlaybackScroll->GetValue() / 1000.0 );
		if ( shouldReset == 2 )
		{
			Reset();
		}
	}
	else if ( !Q_stricmp( pCommand, "redosort" ) )
	{
		//
		Assert( !m_bHierarchicalView );
		Reset();
	}
#endif
}

#endif
